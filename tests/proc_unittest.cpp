#include "gtest/gtest.h"

#define GTEST

#include "crc.c"
#include "proto.c"

static int iSendResult = 0;
static uint8_t buf_send[PROTO_SERVICE_BYTES_LEN + PROTO_PKT_PAYLOAD_MAX_LEN];
static uint16_t usBufSendLen = 0;
static uint8_t ucProcCmdCnt = 0;


void on_send_complete_cb(int res, uint8_t *buf, uint16_t len, void *arg);
void on_send_payload_complete_cb(int res, uint8_t *buf, uint16_t len, void *arg);
void on_send_timeout_cb(int res, uint8_t *buf, uint16_t len, void *arg);

class ProtoTest : public ::testing::Test {
  protected:
    proto_struct proto;

    void SetUp() override {
        proto_init(&proto);
    }
};


// CRC32 macros test
TEST(Crc32Test, ZeroTest) {
    uint8_t buf[PROTO_SERVICE_BYTES_LEN] = "\x01\x00\xab";
    PUT_CRC32(buf, 3, crc32(buf, 3));

    ASSERT_EQ(crc32(buf, PROTO_SERVICE_BYTES_LEN), 0);
}

// Test proc
TEST_F(ProtoTest, TestProc) {
    uint8_t buf_in[PROTO_SERVICE_BYTES_LEN] = "\x01\x00\xab";
    PUT_CRC32(buf_in, 3, crc32(buf_in, 3));

    usBufSendLen = 0;
    ASSERT_EQ(proto_proc(&proto, buf_in, sizeof(buf_in)), 0);
    ASSERT_EQ(usBufSendLen, PROTO_SERVICE_BYTES_LEN + 3);
    ASSERT_EQ(crc32(buf_send, usBufSendLen), 0);
    ASSERT_EQ(memcmp(buf_send, "\x01\x80\xab""123", usBufSendLen - 4), 0);
}

// Test double proc
TEST_F(ProtoTest, TestDoubleProc) {
    uint8_t buf_in[PROTO_SERVICE_BYTES_LEN] = "\x01\x00\xab";
    PUT_CRC32(buf_in, 3, crc32(buf_in, 3));

    // first send as usual
    usBufSendLen = 0;
    ASSERT_EQ(proto_proc(&proto, buf_in, sizeof(buf_in)), 0);
    ASSERT_EQ(usBufSendLen, PROTO_SERVICE_BYTES_LEN + 3);
    ASSERT_EQ(crc32(buf_send, usBufSendLen), 0);
    ASSERT_EQ(memcmp(buf_send, "\x01\x80\xab""123", usBufSendLen - 4), 0);

    // second send - the same packet should be excluded from processing, but we should get response
    usBufSendLen = 0;
    ucProcCmdCnt = 0;
    ASSERT_EQ(proto_proc(&proto, buf_in, sizeof(buf_in)), 0);
    ASSERT_EQ(ucProcCmdCnt, 0);
    ASSERT_EQ(usBufSendLen, PROTO_SERVICE_BYTES_LEN + 3);
    ASSERT_EQ(crc32(buf_send, usBufSendLen), 0);
    ASSERT_EQ(memcmp(buf_send, "\x01\x80\xab""123", usBufSendLen - 4), 0);
}

// Test send no payload
TEST_F(ProtoTest, TestSend) {
    uint8_t buf_response[PROTO_SERVICE_BYTES_LEN];
    uint8_t ucCbCallCounter = 0;

    usBufSendLen = 0;
    ASSERT_EQ(proto_send(&proto, 0x01, NULL, 0, (void*)&ucCbCallCounter, on_send_complete_cb), 0);
    ASSERT_EQ(proto_send(&proto, 0x01, NULL, 0, (void*)&ucCbCallCounter, on_send_complete_cb), PROTO_ERR_BUSY); // second call should fail
    ASSERT_EQ(usBufSendLen, PROTO_SERVICE_BYTES_LEN);
    memcpy(buf_response, buf_send, PKT_HEADER_SIZE);
    SET_FLAGS(buf_response, 0x80);
    PUT_CRC32(buf_response, PKT_HEADER_SIZE, crc32(buf_response, PKT_HEADER_SIZE));

    usBufSendLen = 0;
    ASSERT_EQ(proto_proc(&proto, buf_response, sizeof(buf_response)), 0);
    ASSERT_EQ(usBufSendLen, 0);  // don't wait for smth
    ASSERT_EQ(ucCbCallCounter, 1);  // 1 call for correct callback
}

// Test send with payload
TEST_F(ProtoTest, TestSendPayload) {
    uint8_t buf_response[PROTO_SERVICE_BYTES_LEN+5];
    uint8_t buf_payload[3] = {0x0a, 0x0b, 0x0c};
    uint8_t buf_out[64];
    uint16_t usBufOutLen = 0;
    uint8_t ucCbCallCounter = 0;

    // send request
    usBufSendLen = 0;
    ASSERT_EQ(proto_send(&proto, 0x01, buf_payload, sizeof(buf_payload), (void*)&ucCbCallCounter, on_send_payload_complete_cb), 0);
    ASSERT_EQ(usBufSendLen, PROTO_SERVICE_BYTES_LEN+sizeof(buf_payload));
    memcpy(buf_response, buf_send, PKT_HEADER_SIZE);
    SET_FLAGS(buf_response, 0x80);
    memcpy(GET_PAYLOAD_PTR(buf_response), "\x01\x02\x03\x04\x05", 5);
    PUT_CRC32(buf_response, PKT_HEADER_SIZE+5, crc32(buf_response, PKT_HEADER_SIZE+5));

    // process response (callback should be called here)
    usBufSendLen = 0;
    ASSERT_EQ(proto_proc(&proto, buf_response, sizeof(buf_response)), 0);
    ASSERT_EQ(usBufSendLen, 0);  // don't wait for smth
    ASSERT_EQ(ucCbCallCounter, 1);  // 1 call for correct callback
}

// Test send timeout
TEST_F(ProtoTest, TestSendTimeout) {
    uint8_t buf_response[PROTO_SERVICE_BYTES_LEN];
    uint8_t ucCbCallCounter = 0;

    usBufSendLen = 0;
    proto.timerSend.fRun = 0;
    ASSERT_EQ(proto_send(&proto, 0x01, NULL, 0, (void*)&ucCbCallCounter, on_send_timeout_cb), 0);
    ASSERT_EQ(usBufSendLen, PROTO_SERVICE_BYTES_LEN);
    ASSERT_EQ(proto.timerSend.fRun, 1);         // make sure timer is running
    ASSERT_EQ((proto.timerSend.cb != NULL), 1); // make sure we have some callback

    // immitate timeout
    usBufSendLen = 0;
    proto.timerSend.fRun = 0;
    proto.timerSend.cb(proto.timerSend.arg);
    ASSERT_EQ(usBufSendLen, PROTO_SERVICE_BYTES_LEN);
    ASSERT_EQ(proto.timerSend.fRun, 1);         // make sure timer is running
    ASSERT_EQ((proto.timerSend.cb != NULL), 1); // make sure we have some callback

    // immitate second timeout
    usBufSendLen = 0;
    proto.timerSend.fRun = 0;
    proto.timerSend.cb(proto.timerSend.arg);
    ASSERT_EQ(usBufSendLen, 0);
    ASSERT_EQ(proto.timerSend.fRun, 0);         // make sure timer is not running
    ASSERT_EQ(ucCbCallCounter, 1);              // 1 call for correct callback
}

// Test sync send/recv
TEST_F(ProtoTest, TestSendRecvSync) {
    uint8_t buf_response[PROTO_SERVICE_BYTES_LEN+5];
    uint8_t buf_payload[3] = {0x0a, 0x0b, 0x0c};
    uint8_t buf_out[64];
    uint16_t usBufOutLen = 0;
    uint8_t ucCbCallCounter = 0;
    uint8_t buf_in[PROTO_SERVICE_BYTES_LEN] = "\x01\x00\xab";
    PUT_CRC32(buf_in, 3, crc32(buf_in, 3));

    // send request
    usBufSendLen = 0;
    ASSERT_EQ(proto_send(&proto, 0x01, buf_payload, sizeof(buf_payload), (void*)&ucCbCallCounter, on_send_payload_complete_cb), 0);
    ASSERT_EQ(usBufSendLen, PROTO_SERVICE_BYTES_LEN+sizeof(buf_payload));
    memcpy(buf_response, buf_send, PKT_HEADER_SIZE);
    SET_FLAGS(buf_response, 0x80);
    memcpy(GET_PAYLOAD_PTR(buf_response), "\x01\x02\x03\x04\x05", 5);
    PUT_CRC32(buf_response, PKT_HEADER_SIZE+5, crc32(buf_response, PKT_HEADER_SIZE+5));

    // proc some different packet
    usBufSendLen = 0;
    ASSERT_EQ(proto_proc(&proto, buf_in, sizeof(buf_in)), 0);
    ASSERT_EQ(usBufSendLen, PROTO_SERVICE_BYTES_LEN + 3);
    ASSERT_EQ(crc32(buf_send, usBufSendLen), 0);
    ASSERT_EQ(memcmp(buf_send, "\x01\x80\xab""123", usBufSendLen - 4), 0);

    // process response (callback should be called here)
    usBufSendLen = 0;
    ASSERT_EQ(proto_proc(&proto, buf_response, sizeof(buf_response)), 0);
    ASSERT_EQ(usBufSendLen, 0);  // don't wait for smth
    ASSERT_EQ(ucCbCallCounter, 1);  // 1 call for correct callback
}


int proto_send_cb(uint8_t *buf, uint16_t len)
{
    // just copy data to buffer
    memcpy(buf_send, buf, len);
    usBufSendLen = len;

    return iSendResult;
}

int proto_proc_cmd(uint8_t cmd, uint8_t *payload_in, uint16_t payload_in_len, uint8_t *payload_out, uint16_t *ppayload_out_len)
{
    ucProcCmdCnt++;

    switch (cmd)
    {
        case 0x01:
            memcpy(payload_out, "123", 3);
            *ppayload_out_len = 3;
            break;
    }

    return 0;
}

void on_send_complete_cb(int res, uint8_t *buf, uint16_t len, void *arg)
{
    uint8_t *pucCbCallCounter = (uint8_t*)arg;
    if (res == 0 && len == 0)
        *pucCbCallCounter += 1;
}

void on_send_payload_complete_cb(int res, uint8_t *buf, uint16_t len, void *arg)
{
    uint8_t *pucCbCallCounter = (uint8_t*)arg;
    if (res == 0 && len == 5 && memcmp(buf, "\x01\x02\x03\x04\x05", 5) == 0)
        *pucCbCallCounter += 1;
}

void on_send_timeout_cb(int res, uint8_t *buf, uint16_t len, void *arg)
{
    uint8_t *pucCbCallCounter = (uint8_t*)arg;
    if (res)
        *pucCbCallCounter += 1;
}

void timer_init(TIMER_T *ptimer)
{
    ptimer->fRun = 0;
    ptimer->cb = NULL;
}

void timer_set(TIMER_T *ptimer, uint32_t timeout, void *arg, void (*cb)(void *arg))
{
    ptimer->fRun = 1;
    ptimer->arg = arg;
    ptimer->cb = cb;
}

void timer_stop(TIMER_T *ptimer)
{
    ptimer->fRun = 0;
}


int main(int argc, char* argv[])
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
