# lbmct - Internal Design

This page provides details on the CT implementation.

### Recent Events

There is a circular recording of recent "events" stored with the ct_t structure
(lbmct_private.h):
```
  ct->num_recent_events;
  ct->recent_events[LBMCT_MAX_RECENT_EVENTS];
  LBMCT_MAX_RECENT_EVENTS 256
```

There is an API which prints the contents to the UM logging callback:
```
  lbmct_debug_dump()
```

Alternatively, a core dump can be examined and the contents of a ct displayed.

This record of events is only useful to somebody familiar with the
design and coding of CT, and has access to the source code.
The advantage of this record of events is that it is very low impact,
and is therefore left enabled, even in production.
For example, there is no locking or atomic operations.
As a result, it is possible for events to be lost if multiple threads are
recording events simultaneously.
But this is a small price to pay for it being available in core dumps.

Each event stored in recent_events[] as a 32-bit unsigned integer where
the most-significant byte (MSB) indicates the event type and the next
3 bytes are an argument for that event type.

Interpret the values as follows:

MSB:
```
  0: command processed by CT controller thread (cmd type)
  1: src side handle handshake from rcv (msg type)
  2: rcv side handle handshake from src (msg type)
  3: src side send handshake to rcv (msg type)
  4: rcv side send handshake to src (msg type)
```

Command types (for MSB 0):
```
1  LBMCT_CTRLR_CMD_TYPE_TEST = 1,
2  LBMCT_CTRLR_CMD_TYPE_QUIT,
3  LBMCT_CTRLR_CMD_TYPE_SRC_CONN_TICK,
4  LBMCT_CTRLR_CMD_TYPE_CT_SRC_CREATE,
5  LBMCT_CTRLR_CMD_TYPE_SRC_HANDSHAKE,
6  LBMCT_CTRLR_CMD_TYPE_CT_SRC_DELETE,
7  LBMCT_CTRLR_CMD_TYPE_RCV_CONN_TICK,
8  LBMCT_CTRLR_CMD_TYPE_CT_RCV_CREATE,
9  LBMCT_CTRLR_CMD_TYPE_RCV_CONN_CREATE,
a  LBMCT_CTRLR_CMD_TYPE_CT_RCV_DELETE,
b  LBMCT_CTRLR_CMD_TYPE_RCV_CONN_DELETE,
c  LBMCT_CTRLR_CMD_TYPE_RCV_SEND_C_OK,
d  LBMCT_CTRLR_CMD_TYPE_RCV_SEND_D_OK,
```

Message types (for MSB 1-4):
```
  CREQ: 1
  CRSP: 2
  C_OK: 3
  DREQ: 4
  DRSP: 5
  D_OK: 6
```

**Example**

This dump of recent events is from the `CtSimpleMessages1` test in
main.cc.  It is from the `s_ct` object.

```
num_recent_events: 14
recent_events:
[0]  0x00000004  - LBMCT_CTRLR_CMD_TYPE_CT_SRC_CREATE
[1]  0x00000005  - LBMCT_CTRLR_CMD_TYPE_SRC_HANDSHAKE
[2]  0x01000001  - src side handle handshake from rcv (CREQ)
[3]  0x00000005  - LBMCT_CTRLR_CMD_TYPE_SRC_HANDSHAKE
[4]  0x01000001  - src side handle handshake from rcv (CREQ)
    This happens because of the intentional retry forced by the test.
[5]  0x03000002  - src side send handshake to rcv (CRSP)
[6]  0x00000005  - LBMCT_CTRLR_CMD_TYPE_SRC_HANDSHAKE
[7]  0x01000003  - src side handle handshake from rcv (C_OK)
[8]  0x00000006  - LBMCT_CTRLR_CMD_TYPE_CT_SRC_DELETE
[9]  0x03000005  - src side send handshake to rcv (DRSP)
[10] 0x00000003  - LBMCT_CTRLR_CMD_TYPE_SRC_CONN_TICK
[11] 0x03000005  - src side send handshake to rcv (DRSP)
    This happens because of the intentional retry forced by the test.
[12] 0x00000005  - LBMCT_CTRLR_CMD_TYPE_SRC_HANDSHAKE
[13] 0x01000006  - src side handle handshake from rcv (D_OK)
```
