#include "proto.h"
#include "../proto_defs.h"

#define MSK_RESPONSE                0x80

#define GET_CMD(buf)                (buf[0])
#define GET_FLAGS(buf)              (buf[1])
#define GET_TAG(buf)                (buf[2])
#define GET_PAYLOAD_PTR(buf)        (&buf[3])
#define GET_CRC(buf, len)           ((uint32_t)buf[len-4+0] | ((uint32_t)buf[len-4+1] << 8) | ((uint32_t)buf[len-4+2] << 16) | ((uint32_t)buf[len-4+3] << 24))

#define SET_CMD(buf, cmd)           do { buf[0] = cmd; } while (0)
#define SET_FLAGS(buf, flags)       do { buf[1] = flags; } while (0)
#define SET_TAG(buf, tag)           do { buf[2] = tag; } while (0)

#define PKT_HEADER_SIZE             3

static void proto_send_timeout(void *arg);


/**
 * @brief Inits proto structure
 *
 * @param pProto Pointer to proto_struct
 *
 * @return
 *     - None
 */
void proto_init(proto_struct *pProto)
{
    pProto->fRecvFirst = 1;
    pProto->ucRecvTag = 0;

    pProto->ucSendTag = 0;
    pProto->fSendAttempt = 0;
    pProto->usPktSendLen = 0;

    TIMER_INIT(pProto->timerSend);
}

/**
 * @brief Parses packet in buf_in with len usLenIn
 *
 * @param pProto Pointer to proto_struct
 * @param buf_in buffer which contains packet
 * @param usLenIn length of packet in buffer
 *
 * @return
 *     - PROTO_NO_ERR       Success
 *     - PROTO_ERR_PARAMS   Params error (packet too short)
 *     - PROTO_ERR_CRC      CRC packet error
 *     - PROTO_ERR_IO       IO error (mainly on SEND_PROTO)
 */
int proto_proc(proto_struct *pProto, uint8_t *buf_in, uint16_t usLenIn)
{
    uint8_t cmd = GET_CMD(buf_in);
    uint8_t flags = GET_FLAGS(buf_in);
    uint8_t tag = GET_TAG(buf_in);

    // packet should contain at least cmd and CRC32
    if (usLenIn < PROTO_SERVICE_BYTES_LEN)
        return PROTO_ERR_PARAMS;

    // check CRC32
    if (CRC32(buf_in, usLenIn) != 0)
        return PROTO_ERR_CRC;

    // this is response?
    if (flags & MSK_RESPONSE)
    {
        if (pProto->fSendAttempt)
        {
            if ((GET_TAG(buf_in) == pProto->ucSendTag) && 
                (GET_CMD(buf_in) == pProto->ucSendCmd))
            {
                // send complete
                pProto->fSendAttempt = 0;

                // no need in Timer
                TIMER_STOP(pProto->timerSend);

                // call callback
                if (pProto->onCompleteCb)
                {
                    pProto->onCompleteCb(0, GET_PAYLOAD_PTR(buf_in), usLenIn - PROTO_SERVICE_BYTES_LEN, pProto->onCompleteArg);
                }
            }
        }
    }
    else
    {
        // first receiving or different tag?
        if (pProto->fRecvFirst || pProto->ucRecvTag != tag)
        {
            pProto->fRecvFirst = 0;
            pProto->ucRecvTag = tag;
            pProto->ulLatestRecvCrc = GET_CRC(buf_in, usLenIn);

            int res = PROTO_PROC_CMD(cmd, GET_PAYLOAD_PTR(buf_in), usLenIn-PROTO_SERVICE_BYTES_LEN, GET_PAYLOAD_PTR(pProto->bufPktResp), &pProto->usPktRespLen);
            if (res == 0)
            {
                SET_CMD(pProto->bufPktResp, cmd);
                SET_FLAGS(pProto->bufPktResp, flags | MSK_RESPONSE);
                SET_TAG(pProto->bufPktResp, tag);

                PUT_CRC32(pProto->bufPktResp, PKT_HEADER_SIZE + pProto->usPktRespLen, CRC32(pProto->bufPktResp, PKT_HEADER_SIZE + pProto->usPktRespLen));
                pProto->usPktRespLen += PROTO_SERVICE_BYTES_LEN;
                if (PROTO_SEND(pProto->bufPktResp, pProto->usPktRespLen) != 0)
                {
                    return PROTO_ERR_IO;
                }
            }
        }
        else if (!pProto->fRecvFirst && pProto->ucRecvTag == tag && pProto->ulLatestRecvCrc == GET_CRC(buf_in, usLenIn))
        {
            // in case we got the same request as previous, repeate response
            if (PROTO_SEND(pProto->bufPktResp, pProto->usPktRespLen) != 0)
            {
                return PROTO_ERR_IO;
            }
        }
    }

    return PROTO_NO_ERR;
}

/**
 * @brief Sends command cmd via protocol
 *
 * @param pProto  Pointer to proto_struct
 * @param cmd     command to send
 * @param payload buffer which contains command payload
 * @param usPayloadLen payload length
 * @param arg_cb       param to pass to callback
 * @param onCompleteCb pointer to callback function after send completion. May be NULL if not required
 *
 * @return
 *     - PROTO_NO_ERR       Success
 *     - PROTO_ERR_BUSY     Some sending is already on-going
 *     - PROTO_ERR_PARAMS   Params error (payload too long)
 *     - PROTO_ERR_IO       IO error (mainly on SEND_PROTO)
 */
int proto_send(proto_struct *pProto, uint8_t cmd, uint8_t *payload, uint16_t usPayloadLen, void *arg_cb, void (*onCompleteCb)(int res, uint8_t *buf, uint16_t len, void *arg))
{
    if (pProto->fSendAttempt)
    {
        return PROTO_ERR_BUSY;
    }
    if (usPayloadLen > PROTO_PKT_PAYLOAD_MAX_LEN)
    {
        return PROTO_ERR_PARAMS;
    }

    // increase tag. Don't care about overflow
    pProto->ucSendTag += 1;

    // save command to verify during response processing
    pProto->ucSendCmd = cmd;

    // add header
    SET_CMD(pProto->bufPktSending, cmd);
    SET_FLAGS(pProto->bufPktSending, 0);
    SET_TAG(pProto->bufPktSending, pProto->ucSendTag);

    // copy payload
    memcpy(GET_PAYLOAD_PTR(pProto->bufPktSending), payload, usPayloadLen);

    // calc CRC32
    PUT_CRC32(pProto->bufPktSending, PKT_HEADER_SIZE + usPayloadLen, CRC32(pProto->bufPktSending, PKT_HEADER_SIZE + usPayloadLen));

    pProto->usPktSendLen = PROTO_SERVICE_BYTES_LEN + usPayloadLen;
    if (PROTO_SEND(pProto->bufPktSending, pProto->usPktSendLen) != 0)
    {
        return PROTO_ERR_IO;
    }

    pProto->onCompleteArg = arg_cb;
    pProto->onCompleteCb = onCompleteCb;
    pProto->fSendAttempt = 1;

    // set timer for possible timeout
    TIMER_SET(pProto->timerSend, PROTO_SEND_TIMEOUT, (void*)pProto, proto_send_timeout);

    return PROTO_NO_ERR;
}

/**
 * @brief Internal function to be called on send timeout
 *
 * @param arg_cb       (Pointer to proto_struct)
 *
 * @return
 *     - None
 */
void proto_send_timeout(void *arg)
{
    proto_struct *pProto = (proto_struct*)(arg);
    if (pProto->fSendAttempt)
    {
        if (pProto->fSendAttempt < PROTO_SEND_ATTEMPTS)
        {
            if (PROTO_SEND(pProto->bufPktSending, pProto->usPktSendLen) != 0)
            {
                // send "complete"
                pProto->fSendAttempt = 0;

                // call callback
                if (pProto->onCompleteCb)
                {
                    pProto->onCompleteCb(PROTO_ERR_IO, NULL, 0, pProto->onCompleteArg);
                }
            }
            pProto->fSendAttempt += 1;

            // set timer for possible timeout
            TIMER_SET(pProto->timerSend, PROTO_SEND_TIMEOUT, arg, proto_send_timeout);
        }
        else
        {
            // send "complete"
            pProto->fSendAttempt = 0;

            // call callback
            if (pProto->onCompleteCb)
            {
                pProto->onCompleteCb(PROTO_ERR_TIMEOUT, NULL, 0, pProto->onCompleteArg);
            }
        }
    }
}
