/*
 * SEGA IO4 Serial Packet Reader
 * Ported from skogaslider-firmware by skogaby
 * Adapted for simpl-slidrr-firmware
 */

#ifndef SEGA_SERIAL_READER_H
#define SEGA_SERIAL_READER_H

#include <stdint.h>
#include <stdbool.h>
#include "sega_protocol.h"

/**
 * @brief CDC interface index for the IO4 slider protocol.
 * In simpl-slidrr-firmware, CDC instances are:
 *   0 = CLI port
 *   1 = IO4 slider port (this one)
 *   2 = AIME port
 */
#define IO4_CDC_ITF 1

/**
 * @brief State structure for the SEGA serial packet reader.
 * Manages state machines for reading in-progress streams of serial data
 * and constructing packets, supporting streams ending mid-packet.
 */
typedef struct {
    /** Buffer to hold in-progress packet data */
    uint8_t serial_buf[256];
    /** Flag for whether the last byte read was an escape byte */
    bool last_byte_escape;
    /** Buffer holding the last sync byte read */
    int sync;
    /** Buffer holding the last data length byte read */
    int data_length;
    /** How many bytes of the current packet have been read */
    uint8_t bytes_read;
    /** The last checksum read */
    int checksum;
    /** The last slider command ID read */
    int slider_command_id;
    /** Flag for whether a packet is still in progress */
    bool packet_in_progress;
} SegaSerialReader;

/**
 * @brief Initialize a SegaSerialReader state.
 */
void sega_serial_reader_init(SegaSerialReader *reader);

/**
 * @brief Reads a single slider packet from serial, if one is available.
 * @param reader The serial reader state
 * @param dst Destination for the parsed packet
 * @return true if a complete packet was read, false otherwise
 */
bool sega_serial_read_slider_packet(SegaSerialReader *reader, SliderPacket *dst);

/**
 * @brief Says whether a slider packet is currently in progress of being read.
 */
bool sega_serial_slider_packet_in_progress(SegaSerialReader *reader);

#endif // SEGA_SERIAL_READER_H
