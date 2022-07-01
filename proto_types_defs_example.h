#ifndef __PROTO_TYPES_DEFS_H
#define __PROTO_TYPES_DEFS_H

#define PROTO_PKT_PAYLOAD_MAX_LEN               4096

#ifdef GTEST
 typedef struct
 {
    uint8_t fRun;
    void *arg;
    void (*cb)(void *arg);
 } TIMER_T;
#else

#endif //GTEST

#endif //__PROTO_TYPES_DEFS_H