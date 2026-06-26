/*
 * Binary config/telemetry protocol for GUI tools (CDC config port).
 */

#include "protocol.h"

#include <string.h>

#include "tusb.h"
#include "pico/time.h"

#include "config.h"
#include "save.h"
#include "slider.h"
#include "air.h"
#include "cli.h"

#define PROTO_HDR_SIZE 10
#define PROTO_CRC_SIZE 2
#define PROTO_MIN_FRAME (PROTO_HDR_SIZE + PROTO_CRC_SIZE)

#define PROTO_TELEMETRY_SIZE 219

enum { RX_BUF_SIZE = 320 };

static uint8_t rx_buf[RX_BUF_SIZE];
static size_t rx_len;

static uint32_t frame_counter;
static uint8_t stream_hz;
static uint64_t next_stream_us;

static uint16_t crc16_ccitt(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

static void put_u16_le(uint8_t *buf, uint16_t val)
{
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)(val >> 8);
}

static void put_u32_le(uint8_t *buf, uint32_t val)
{
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
    buf[2] = (uint8_t)((val >> 16) & 0xFF);
    buf[3] = (uint8_t)((val >> 24) & 0xFF);
}

static uint16_t get_u16_le(const uint8_t *buf)
{
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

static uint32_t get_u32_le(const uint8_t *buf)
{
    return (uint32_t)buf[0]
         | ((uint32_t)buf[1] << 8)
         | ((uint32_t)buf[2] << 16)
         | ((uint32_t)buf[3] << 24);
}

static void send_frame(uint8_t itf, uint8_t opcode, uint16_t seq,
                       const uint8_t *payload, uint16_t payload_len)
{
    uint8_t frame[PROTO_HDR_SIZE + PROTO_MAX_PAYLOAD + PROTO_CRC_SIZE];
    frame[0] = 'S';
    frame[1] = 'M';
    frame[2] = 'P';
    frame[3] = 'L';
    frame[4] = PROTO_VERSION;
    frame[5] = opcode;
    put_u16_le(frame + 6, seq);
    put_u16_le(frame + 8, payload_len);
    if (payload_len > 0 && payload != NULL) {
        memcpy(frame + PROTO_HDR_SIZE, payload, payload_len);
    }

    size_t frame_len = PROTO_HDR_SIZE + payload_len;
    uint16_t crc = crc16_ccitt(frame, frame_len);
    put_u16_le(frame + frame_len, crc);

    size_t total = frame_len + PROTO_CRC_SIZE;
    size_t off = 0;
    while (off < total) {
        uint32_t n = tud_cdc_n_write(itf, frame + off, (uint32_t)(total - off));
        if (n == 0) {
            continue;
        }
        off += n;
    }
    tud_cdc_n_write_flush(itf);
}

static void send_status(uint8_t itf, uint8_t opcode, uint16_t seq, uint8_t status)
{
    send_frame(itf, opcode, seq, &status, 1);
}

static size_t build_telemetry(uint8_t *out)
{
    const uint16_t *raw = slider_raw();
    uint32_t touch_bits = 0;

    put_u32_le(out, frame_counter++);

    for (int i = 0; i < 32; i++) {
        put_u16_le(out + 4 + i * 2, raw[i]);
        if (slider_touched((unsigned)i)) {
            touch_bits |= (1u << i);
        }
    }

    for (int i = 0; i < 32; i++) {
        put_u32_le(out + 68 + i * 4, slider_count((unsigned)i));
    }

    out[196] = (uint8_t)(touch_bits & 0xFF);
    out[197] = (uint8_t)((touch_bits >> 8) & 0xFF);
    out[198] = (uint8_t)((touch_bits >> 16) & 0xFF);
    out[199] = (uint8_t)((touch_bits >> 24) & 0xFF);

    for (int i = 0; i < 6; i++) {
        put_u16_le(out + 200 + i * 2, air_ir_raw((uint8_t)i));
        out[212 + i] = air_ir_blocked((uint8_t)i) ? 1 : 0;
    }
    out[218] = air_bitmap();

    return PROTO_TELEMETRY_SIZE;
}

static void send_telemetry(uint8_t itf, uint16_t seq, uint8_t opcode)
{
    uint8_t payload[PROTO_TELEMETRY_SIZE];
    build_telemetry(payload);
    send_frame(itf, opcode, seq, payload, sizeof(payload));
}

static void handle_ping(uint8_t itf, uint16_t seq)
{
    uint8_t payload[64];
    size_t off = 0;

    payload[off++] = PROTO_VERSION;
    size_t fw_len = strlen(built_time);
    if (fw_len > sizeof(payload) - off - 1) {
        fw_len = sizeof(payload) - off - 1;
    }
    memcpy(payload + off, built_time, fw_len);
    off += fw_len;
    payload[off++] = '\0';

    uint64_t id = board_id_64();
    memcpy(payload + off, &id, sizeof(id));
    off += sizeof(id);

    send_frame(itf, PROTO_OP_PING, seq, payload, (uint16_t)off);
}

static void handle_get_config(uint8_t itf, uint16_t seq)
{
    uint8_t payload[1 + sizeof(chu_cfg_t)];
    payload[0] = CFG_VERSION;
    memcpy(payload + 1, chu_cfg, sizeof(chu_cfg_t));
    send_frame(itf, PROTO_OP_GET_CONFIG, seq, payload, sizeof(payload));
}

static void handle_set_config(uint8_t itf, uint16_t seq, const uint8_t *payload,
                              uint16_t len)
{
    if (len < 4 + sizeof(chu_cfg_t)) {
        send_status(itf, PROTO_OP_SET_CONFIG, seq, PROTO_STATUS_INVALID);
        return;
    }

    uint32_t flags = get_u32_le(payload);
    chu_cfg_t patch;
    memcpy(&patch, payload + 4, sizeof(patch));

    config_status_t st = config_apply_sections(flags, &patch, true);
    if (st == CONFIG_ERR_INVALID) {
        send_status(itf, PROTO_OP_SET_CONFIG, seq, PROTO_STATUS_INVALID);
        return;
    }
    if (st == CONFIG_ERR_VALIDATE) {
        send_status(itf, PROTO_OP_SET_CONFIG, seq, PROTO_STATUS_VALIDATE);
        return;
    }

    uint8_t resp[1 + sizeof(chu_cfg_t)];
    resp[0] = PROTO_STATUS_OK;
    memcpy(resp + 1, chu_cfg, sizeof(chu_cfg_t));
    send_frame(itf, PROTO_OP_SET_CONFIG, seq, resp, sizeof(resp));
}

static void handle_stream_on(uint8_t itf, uint16_t seq, const uint8_t *payload,
                             uint16_t len)
{
    if (len < 1) {
        send_status(itf, PROTO_OP_STREAM_ON, seq, PROTO_STATUS_INVALID);
        return;
    }

    uint8_t hz = payload[0];
    if (hz < 1 || hz > 60) {
        send_status(itf, PROTO_OP_STREAM_ON, seq, PROTO_STATUS_INVALID);
        return;
    }

    stream_hz = hz;
    next_stream_us = time_us_64();
    send_status(itf, PROTO_OP_STREAM_ON, seq, PROTO_STATUS_OK);
}

static void handle_capture_ir_baseline(uint8_t itf, uint16_t seq)
{
    config_capture_ir_baseline();
    config_changed();

    uint8_t resp[1 + 12];
    resp[0] = PROTO_STATUS_OK;
    for (int i = 0; i < 6; i++) {
        put_u16_le(resp + 1 + i * 2, chu_cfg->ir.base[i]);
    }
    send_frame(itf, PROTO_OP_CAPTURE_IR_BASE, seq, resp, sizeof(resp));
}

static void dispatch_frame(uint8_t itf, const uint8_t *frame, size_t frame_len)
{
    uint8_t opcode = frame[5];
    uint16_t seq = get_u16_le(frame + 6);
    uint16_t payload_len = get_u16_le(frame + 8);
    const uint8_t *payload = frame + PROTO_HDR_SIZE;

    switch (opcode) {
    case PROTO_OP_PING:
        handle_ping(itf, seq);
        break;
    case PROTO_OP_GET_CONFIG:
        handle_get_config(itf, seq);
        break;
    case PROTO_OP_SET_CONFIG:
        handle_set_config(itf, seq, payload, payload_len);
        break;
    case PROTO_OP_GET_TELEMETRY:
        send_telemetry(itf, seq, PROTO_OP_GET_TELEMETRY);
        break;
    case PROTO_OP_STREAM_ON:
        handle_stream_on(itf, seq, payload, payload_len);
        break;
    case PROTO_OP_STREAM_OFF:
        stream_hz = 0;
        send_status(itf, PROTO_OP_STREAM_OFF, seq, PROTO_STATUS_OK);
        break;
    case PROTO_OP_SAVE:
        config_save_now();
        send_status(itf, PROTO_OP_SAVE, seq, PROTO_STATUS_OK);
        break;
    case PROTO_OP_FACTORY_RESET:
        config_factory_reset();
        send_status(itf, PROTO_OP_FACTORY_RESET, seq, PROTO_STATUS_OK);
        break;
    case PROTO_OP_RECALC_TOUCH:
        slider_sensor_init();
        send_status(itf, PROTO_OP_RECALC_TOUCH, seq, PROTO_STATUS_OK);
        break;
    case PROTO_OP_CAPTURE_IR_BASE:
        handle_capture_ir_baseline(itf, seq);
        break;
    default:
        break;
    }
}

static void sync_to_magic(void)
{
    while (rx_len >= 4) {
        if (rx_buf[0] == 'S' && rx_buf[1] == 'M' &&
            rx_buf[2] == 'P' && rx_buf[3] == 'L') {
            return;
        }
        memmove(rx_buf, rx_buf + 1, rx_len - 1);
        rx_len--;
    }
}

static void process_rx(uint8_t itf)
{
    while (tud_cdc_n_available(itf)) {
        if (rx_len >= RX_BUF_SIZE) {
            rx_len = 0;
        }
        int c = tud_cdc_n_read_char(itf);
        if (c < 0) {
            break;
        }
        rx_buf[rx_len++] = (uint8_t)c;
    }

    while (rx_len >= PROTO_MIN_FRAME) {
        sync_to_magic();
        if (rx_len < PROTO_MIN_FRAME) {
            return;
        }

        if (rx_buf[4] != PROTO_VERSION) {
            memmove(rx_buf, rx_buf + 1, rx_len - 1);
            rx_len--;
            continue;
        }

        uint16_t payload_len = get_u16_le(rx_buf + 8);
        if (payload_len > PROTO_MAX_PAYLOAD) {
            memmove(rx_buf, rx_buf + 1, rx_len - 1);
            rx_len--;
            continue;
        }

        size_t total = PROTO_HDR_SIZE + payload_len + PROTO_CRC_SIZE;
        if (rx_len < total) {
            return;
        }

        uint16_t expect = crc16_ccitt(rx_buf, PROTO_HDR_SIZE + payload_len);
        uint16_t got = get_u16_le(rx_buf + PROTO_HDR_SIZE + payload_len);
        if (expect != got) {
            memmove(rx_buf, rx_buf + 1, rx_len - 1);
            rx_len--;
            continue;
        }

        dispatch_frame(itf, rx_buf, total);
        memmove(rx_buf, rx_buf + total, rx_len - total);
        rx_len -= total;
    }
}

static void process_stream(uint8_t itf)
{
    if (stream_hz == 0) {
        return;
    }

    uint64_t now = time_us_64();
    uint64_t interval = 1000000u / stream_hz;
    if (now < next_stream_us) {
        return;
    }
    next_stream_us = now + interval;

    if (!tud_cdc_n_connected(itf) || !tud_cdc_n_write_available(itf)) {
        return;
    }

    send_telemetry(itf, 0, PROTO_OP_TELEMETRY_PUSH);
}

void protocol_run(uint8_t itf)
{
    if (!tud_cdc_n_connected(itf)) {
        return;
    }

    process_rx(itf);
    process_stream(itf);
}
