/*
 * Binary config/telemetry protocol for GUI tools (CDC config port).
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define PROTO_CDC_ITF 3

#define PROTO_MAGIC 0x4C504D53u /* 'S','M','P','L' on the wire */

#define PROTO_VERSION 1

#define PROTO_MAX_PAYLOAD 256

#define PROTO_OP_PING              0x01
#define PROTO_OP_GET_CONFIG        0x10
#define PROTO_OP_SET_CONFIG        0x11
#define PROTO_OP_GET_TELEMETRY     0x20
#define PROTO_OP_STREAM_ON         0x21
#define PROTO_OP_STREAM_OFF        0x22
#define PROTO_OP_SAVE              0x30
#define PROTO_OP_FACTORY_RESET     0x31
#define PROTO_OP_RECALC_TOUCH      0x40
#define PROTO_OP_CAPTURE_IR_BASE   0x41
#define PROTO_OP_TELEMETRY_PUSH    0xA0

#define PROTO_STATUS_OK            0
#define PROTO_STATUS_INVALID       1
#define PROTO_STATUS_VALIDATE      2
#define PROTO_STATUS_BUSY          3

void protocol_run(uint8_t itf);

#endif
