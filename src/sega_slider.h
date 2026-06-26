/*
 * SEGA IO4 Slider Protocol Handler
 * Ported from skogaslider-firmware by skogaby
 * Adapted for simpl-slidrr-firmware
 */

#ifndef SEGA_SLIDER_H
#define SEGA_SLIDER_H

#include <stdint.h>
#include <stdbool.h>
#include "sega_protocol.h"
#include "sega_serial_reader.h"

/**
 * @brief State structure for the SEGA slider protocol handler.
 * Implements the SEGA slider's request and response protocol.
 */
typedef struct {
    /** Whether to automatically send slider reports */
    bool auto_send_reports;
    /** Response data buffer for slider reports */
    uint8_t slider_response_data[32];
    /** Hardware info response data */
    uint8_t hw_info_response_data[18];
    /** Reusable response packet */
    SliderPacket response;
    /** Timestamps (us) of last touch detection for each of the 32 keys */
    uint64_t last_touch_time[32];
} SegaSlider;

/**
 * @brief Initialize the SegaSlider state.
 */
void sega_slider_init(SegaSlider *slider);

/**
 * @brief Process an incoming serial packet from the host.
 * @param slider The slider state
 * @param request The incoming packet
 */
void sega_slider_process_packet(SegaSlider *slider, SliderPacket *request);

/**
 * @brief Send a slider report to the host (auto-report mode).
 */
void sega_slider_send_report(SegaSlider *slider);

#endif // SEGA_SLIDER_H
