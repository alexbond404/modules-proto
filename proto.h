#ifndef __PROTO_H
#define __PROTO_H

#include "../proto_types_defs.h"

#define PROTO_SERVICE_BYTES_LEN     7

#define PROTO_NO_ERR                0
#define PROTO_ERR_BUSY              -1
#define PROTO_ERR_PARAMS            -2
#define PROTO_ERR_TIMEOUT           -3
#define PROTO_ERR_IO                -4
#define PROTO_ERR_CRC               -5


typedef struct
{
    uint8_t fRecvFirst;
    uint8_t ucRecvTag;
    uint32_t ulLatestRecvCrc;
    uint8_t bufPktResp[PROTO_SERVICE_BYTES_LEN + PROTO_PKT_PAYLOAD_MAX_LEN];
    uint16_t usPktRespLen;

    uint8_t ucSendTag;
    uint8_t fSendAttempt;
    uint8_t bufPktSending[PROTO_SERVICE_BYTES_LEN + PROTO_PKT_PAYLOAD_MAX_LEN];
    uint16_t usPktSendLen;
    TIMER_T timerSend;

    void *onCompleteArg;
    void (*onCompleteCb)(int res, uint8_t *buf, uint16_t len, void *arg);
} proto_struct;


void proto_init(proto_struct *pProto);
int proto_proc(proto_struct *pProto, uint8_t *buf_in, uint16_t usLenIn);
int proto_send(proto_struct *pProto, uint8_t cmd, uint8_t *payload, uint16_t usPayloadLen, void *arg_cb, void (*onCompleteCb)(int res, uint8_t *buf, uint16_t len, void *arg));


#endif //__PROTO_H