#ifndef __PROTO_DEFS_H
#define __PROTO_DEFS_H

#define PROTO_SEND_TIMEOUT          400     // ms
#define PROTO_SEND_ATTEMPTS         2       // 1+


#ifdef GTEST
 #include "utils.h"
 #define PUT_CRC32(buf, offset, val)            SET_UINT32R(buf, offset, val)

 // this section is used for Google Test only
 int proto_proc_cmd(uint8_t cmd, uint8_t *payload_in, uint16_t payload_in_len, uint8_t *payload_out, uint16_t *ppayload_out_len);
 #define PROTO_PROC_CMD(cmd, p_in, p_in_len, p_out, p_out_len)         proto_proc_cmd(cmd, p_in, p_in_len, p_out, p_out_len)

 int proto_send_cb(uint8_t *buf, uint16_t len);
 #define PROTO_SEND(buf, len)                   proto_send_cb(buf, len)

 void timer_init(TIMER_T *ptimer);
 void timer_set(TIMER_T *ptimer, uint32_t timeout, void *arg, void (*cb)(void *arg));
 void timer_stop(TIMER_T *ptimer);
 #define TIMER_INIT(timer)                      timer_init(&timer)
 #define TIMER_SET(timer, timeout, arg, cb)     timer_set(&timer, timeout, arg, cb)
 #define TIMER_STOP(timer)                      timer_stop(&timer)
#else

#endif

#include "crc.h"
#define CRC32(buf, len)             crc32(buf, len)

#endif //__PROTO_DEFS_H