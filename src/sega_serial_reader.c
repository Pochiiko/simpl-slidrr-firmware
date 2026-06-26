/*
 * SEGA IO4 Serial Packet Reader
 * Ported from skogaslider-firmware by skogaby
 * Adapted for simpl-slidrr-firmware
 */

#include "sega_serial_reader.h"
#include "tusb.h"

void sega_serial_reader_init(SegaSerialReader *reader)
{
    reader->sync = -1;
    reader->slider_command_id = -1;
    reader->data_length = -1;
    reader->bytes_read = 0;
    reader->checksum = -1;
    reader->last_byte_escape = false;
    reader->packet_in_progress = false;
}

/**
 * @brief Reads a single byte from serial for the given interface, or -1
 * if no bytes are available.
 */
static int read_serial_byte(uint8_t itf)
{
    if (tud_cdc_n_available(itf)) {
        return tud_cdc_n_read_char(itf);
    }
    return -1;
}

/**
 * @brief Reads a single byte from serial for the given interface, or -1
 * if no bytes are available. While reading bytes, if the given escape
 * byte is encountered, it is skipped and the next byte + 1 is returned instead.
 */
static int read_unescaped_serial_byte(uint8_t itf, uint8_t escape_byte,
                                      bool *last_byte_escape)
{
    if (!tud_cdc_n_available(itf)) {
        return -1;
    }

    uint8_t val = tud_cdc_n_read_char(itf);

    if (val != escape_byte) {
        if (*last_byte_escape) {
            *last_byte_escape = false;
            return val + 1;
        } else {
            return val;
        }
    } else {
        /* If we read the escape byte, just set the flag and then call
         * recursively so the next call will unescape the next byte */
        *last_byte_escape = true;
        return read_unescaped_serial_byte(itf, escape_byte, last_byte_escape);
    }
}

bool sega_serial_read_slider_packet(SegaSerialReader *reader, SliderPacket *dst)
{
    bool packet_available = false;
    uint8_t itf = IO4_CDC_ITF;
    int next_byte;

    /* If we're at the beginning of a packet, read bytes without unescaping */
    if (reader->sync == -1) {
        next_byte = read_serial_byte(itf);
    } else {
        next_byte = read_unescaped_serial_byte(itf, SLIDER_PACKET_ESCAPE,
                                               &reader->last_byte_escape);
    }

    /* Process available bytes */
    while (next_byte != -1) {
        if (reader->sync == -1) {
            /* Waiting for packet begin sync byte */
            if (next_byte == SLIDER_PACKET_BEGIN) {
                reader->packet_in_progress = true;
                reader->sync = next_byte;
                next_byte = read_unescaped_serial_byte(itf, SLIDER_PACKET_ESCAPE,
                                                       &reader->last_byte_escape);
            } else {
                next_byte = read_serial_byte(itf);
            }
        } else if (reader->slider_command_id == -1) {
            /* Read command ID */
            reader->slider_command_id = next_byte;
            next_byte = read_unescaped_serial_byte(itf, SLIDER_PACKET_ESCAPE,
                                                   &reader->last_byte_escape);
        } else if (reader->data_length == -1) {
            /* Read data length */
            reader->data_length = next_byte;
            next_byte = read_unescaped_serial_byte(itf, SLIDER_PACKET_ESCAPE,
                                                   &reader->last_byte_escape);
        } else if (reader->bytes_read != reader->data_length) {
            /* Read body bytes */
            reader->serial_buf[reader->bytes_read] = next_byte;
            reader->bytes_read++;
            next_byte = read_unescaped_serial_byte(itf, SLIDER_PACKET_ESCAPE,
                                                   &reader->last_byte_escape);
        } else if (reader->checksum == -1) {
            /* Read checksum and return the packet */
            reader->checksum = next_byte;

            dst->command_id = reader->slider_command_id;
            dst->data = &reader->serial_buf[0];
            dst->length = reader->data_length;
            dst->checksum = reader->checksum;
            packet_available = true;

            /* Reset state for next packet */
            reader->sync = -1;
            reader->slider_command_id = -1;
            reader->data_length = -1;
            reader->bytes_read = 0;
            reader->checksum = -1;
            reader->packet_in_progress = false;
            next_byte = -1;
        }
    }

    return packet_available;
}

bool sega_serial_slider_packet_in_progress(SegaSerialReader *reader)
{
    return reader->packet_in_progress;
}
