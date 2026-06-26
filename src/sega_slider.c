/*
 * SEGA IO4 Slider Protocol Handler
 * Ported from skogaslider-firmware by skogaby
 * Adapted for simpl-slidrr-firmware
 */

#include "sega_slider.h"
#include "slider.h"
#include "config.h"
#include "tusb.h"
#include "pico/time.h"
#include <string.h>

/* IO4_CDC_ITF is defined in sega_serial_reader.h */

void sega_slider_init(SegaSlider *slider)
{
    slider->auto_send_reports = false;
    memset(slider->slider_response_data, 0, sizeof(slider->slider_response_data));

    /* Hardware info: "15330   " + some version data */
    uint8_t hw_info[18] = {
        0x31, 0x35, 0x33, 0x33, 0x30, 0x20, 0x20, 0x20,
        0xA0, 0x30, 0x36, 0x37, 0x31, 0x32, 0xFF, 0x90,
        0x00, 0x64
    };
    memcpy(slider->hw_info_response_data, hw_info, 18);

    memset(slider->last_touch_time, 0, sizeof(slider->last_touch_time));

    slider->response.data = NULL;
    slider->response.length = 0;
    slider->response.command_id = 0;
    slider->response.checksum = 0;
}

/**
 * @brief Generates a slider report packet to send to the host.
 */
static void generate_slider_report(SegaSlider *slider, SliderPacket *response)
{
    /* Re-order the touch states into the right format. Internally, we store
     * them with sensor 0 as the top-left position, but SEGA has it as
     * top-right, so we reverse the order. Also map 10-bit to 8-bit values. */

    uint8_t response_index = 0;
    uint64_t now = time_us_64();
    uint64_t hold_us = (uint64_t)chu_cfg->tweak.delay_ms * 1000;

    for (int key = 15; key >= 0; key--) {
        for (int j = 0; j < 2; j++) {
            int sensor = (key * 2) + j;
            bool touched = slider_touched(sensor);

            if (touched) {
                /* Sensor is actively touched, update timestamp */
                slider->last_touch_time[sensor] = now;
                slider->slider_response_data[response_index++] = 0xFC;
            } else {
                /* Not touched: check if we're still within the release hold window */
                if ((hold_us > 0) &&
                    (now - slider->last_touch_time[sensor] < hold_us)) {
                    slider->slider_response_data[response_index++] = 0xFC;
                } else {
                    slider->slider_response_data[response_index++] = 0x00;
                }
            }
        }
    }

    response->command_id = SLIDER_REPORT;
    response->data = &slider->slider_response_data[0];
    response->length = 32;
}

/**
 * @brief Sends a byte to the host, escaping it if necessary.
 */
static void send_escaped_byte(uint8_t byte)
{
    if (byte == SLIDER_PACKET_BEGIN || byte == SLIDER_PACKET_ESCAPE) {
        tud_cdc_n_write_char(IO4_CDC_ITF, SLIDER_PACKET_ESCAPE);
        byte -= 1;
    }
    tud_cdc_n_write_char(IO4_CDC_ITF, byte);
}

/**
 * @brief Sends a packet to the host, escaping bytes and checksumming.
 */
static void send_packet(SliderPacket *packet)
{
    uint8_t checksum = 0;

    tud_cdc_n_write_char(IO4_CDC_ITF, SLIDER_PACKET_BEGIN);
    checksum -= SLIDER_PACKET_BEGIN;

    send_escaped_byte(packet->command_id);
    checksum -= packet->command_id;

    send_escaped_byte(packet->length);
    checksum -= packet->length;

    for (int i = 0; i < packet->length; i++) {
        send_escaped_byte(packet->data[i]);
        checksum -= packet->data[i];
    }

    send_escaped_byte(checksum);

    tud_cdc_n_write_flush(IO4_CDC_ITF);
}

/**
 * @brief Handles a request for a one-off slider report.
 */
static void handle_slider_report(SegaSlider *slider)
{
    generate_slider_report(slider, &slider->response);
    send_packet(&slider->response);
}

/**
 * @brief Handles a packet from the host to update the LEDs on the slider.
 *
 * Currently stubbed, no plans to add LEDs in the near future for now. ::infecta
 */
static void handle_led_report(SliderPacket *request)
{
    (void)request;
}

/**
 * @brief Handles a request to enable automatic slider reports.
 */
static void handle_enable_slider_report(SegaSlider *slider)
{
    slider->auto_send_reports = true;
}

/**
 * @brief Handles a request to disable automatic slider reports.
 */
static void handle_disable_slider_report(SegaSlider *slider)
{
    slider->auto_send_reports = false;

    slider->response.command_id = DISABLE_SLIDER_REPORT;
    slider->response.length = 0;
    slider->response.data = NULL;
    send_packet(&slider->response);
}

/**
 * @brief Handles a request to reset the board.
 */
static void handle_reset(SegaSlider *slider)
{
    slider->auto_send_reports = false;

    slider->response.command_id = SLIDER_RESET;
    slider->response.length = 0;
    slider->response.data = NULL;
    send_packet(&slider->response);
}

/**
 * @brief Handles a request to get the hardware info.
 */
static void handle_get_hw_info(SegaSlider *slider)
{
    slider->response.command_id = GET_HW_INFO;
    slider->response.data = &slider->hw_info_response_data[0];
    slider->response.length = 18;
    send_packet(&slider->response);
}

/**
 * @brief Handles SET_SHORT_RAW_COUNT_OFFSET - just ACK.
 */
static void handle_set_short_raw_count_offset(SegaSlider *slider)
{
    slider->response.command_id = SET_SHORT_RAW_COUNT_OFFSET;
    slider->response.length = 0;
    slider->response.data = NULL;
    send_packet(&slider->response);
}

/**
 * @brief Handles SET_SHORT_RAW_COUNT_SHIFT - just ACK.
 */
static void handle_set_short_raw_count_shift(SegaSlider *slider)
{
    slider->response.command_id = SET_SHORT_RAW_COUNT_SHIFT;
    slider->response.length = 0;
    slider->response.data = NULL;
    send_packet(&slider->response);
}

void sega_slider_process_packet(SegaSlider *slider, SliderPacket *request)
{
    switch (request->command_id) {
        case SLIDER_REPORT:
            handle_slider_report(slider);
            break;
        case LED_REPORT:
            handle_led_report(request);
            break;
        case ENABLE_SLIDER_REPORT:
            handle_enable_slider_report(slider);
            break;
        case DISABLE_SLIDER_REPORT:
            handle_disable_slider_report(slider);
            break;
        case SLIDER_RESET:
            handle_reset(slider);
            break;
        case GET_HW_INFO:
            handle_get_hw_info(slider);
            break;
        case SET_SHORT_RAW_COUNT_OFFSET:
            handle_set_short_raw_count_offset(slider);
            break;
        case SET_SHORT_RAW_COUNT_SHIFT:
            handle_set_short_raw_count_shift(slider);
            break;
        default:
            break;
    }
}

void sega_slider_send_report(SegaSlider *slider)
{
    generate_slider_report(slider, &slider->response);
    send_packet(&slider->response);
}
