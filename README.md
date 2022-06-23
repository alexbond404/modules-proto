# proto

This is a simple engine to send/receive packets.

This module works on top of packet channel.

Proto provides the following features:
- CRC32 protection;
- Acknowledge;
- Repeat on transmit timeout (no ack).


## Using

### Context. Init
All functions required a context `proto_struct*` to store state. Before any use context should be defined and initialized with `proto_init`:

```
proto_struct proto;

proto_init(&proto);
```


### Process incoming packets
When you need to process the next packet just call `proto_proc`:

```
int err = proto_proc(&slip, buf_rx, len_rx);
```
where `buf_rx` is buffer with packet and `len_rx` is packet length.
Function returns PROTO_NO_ERR (0) or error.

In case of success `PROTO_PROC_CMD(cmd, p_in, p_in_len, p_out, p_out_len)` will be called inside `proto_proc`. Here
- `cmd` - command in the packet;
- `p_in`, `p_len` - pointer to payload and its length;
- `p_out`, `p_out` - **pointer** to answer payload and **pointer** to it's length. Lenght of payload should be written back to `p_out`.


### Send
To send packet call `proto_send`. Real sending to channel is done by calling `PROTO_SEND` inside.

```
int res = proto_send(&proto, cmd, payload, len, arg_cb, onCompleteCb);
```
In this example buffer `cmd` this is a command in packet, `payload` is a packet payload with length `len`. After send is complete (successful or not) non-NULL `onCompleteCb` will be called with `arg_cb`, result of sending and answer packet payload and length.

After packet is sent with `PROTO_SEND`, timer with timeout `PROTO_SEND_TIMEOUT` is set with `TIMER_SET` function. In case of successful send `TIMER_STOP` will be called. In case of timeout packet will be tried to send again (but not more than `PROTO_SEND_ATTEMPTS` times).


## proto_defs_example.h
To config module you should use `proto_defs.h` file which must define the following:
- CRC32(buf, len) - function is used every time CRC should be calculated. Note: CRC of data, which contains CRC should be zero;
- PUT_CRC32(buf, offset, val) - function or macros that will put CRC to specified buffer at offset. This is required due to little- and big-endian;
- PROTO_PROC_CMD(cmd, p_in, p_in_len, p_out, p_out_len) - called during `proto_proc` if command `cmd` with payload `p_in` of length `p_in_len` received. If command is correct 0 should be returned and payload copied to `p_out`, payload's length to `p_out_len`;
- PROTO_SEND(buf, len) - this function is used to send formed packet to lower layer. It may be called in `proto_proc` and timer callback;
- TIMER_INIT(timer) - called during `proto_init` to init `timer`;
- TIMER_SET(timer, timeout, arg, cb) - called to set one-shot timer;
- TIMER_STOP(timer) - called when timer is not required.


## proto_types_defs_example.h
Header `proto_types_defs.h` is used to setup type of timer (`TIMER_T`) and max payload length (`PROTO_PKT_PAYLOAD_MAX_LEN`).


## Unit tests
Unit tests are located in tests dir. They may be compiled with help of Google Test framework.
