/*
 * Controller Main
 * WHowe <github.com/whowechina>
 * Modified by Infecta <github.com/Infecta>
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "bsp/board.h"
#include "pico/bootrom.h"

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"

#include "tusb.h"
#include "usb_descriptors.h"

#include "aime.h"
#include "nfc.h"

#include "board_defs.h"
#include "save.h"
#include "config.h"
#include "cli.h"
#include "commands.h"

#include "i2c_hub.h"
#include "slider.h"
#include "air.h"
#include "button.h"
#include "sega_serial_reader.h"
#include "sega_slider.h"
#include "protocol.h"

/* SEGA IO4 protocol state */
static SegaSerialReader sega_reader;
static SegaSlider sega_slider_state;
static SliderPacket sega_request;
static uint64_t last_io4_serial_time = 0;
static uint64_t next_io4_report_time = 0;

/* IO4 HID Joystick Report - 64-byte struct emulating SEGA IO4 */
struct __attribute__((packed)) {
    uint16_t adcs[8];
    uint16_t spinners[4];
    uint16_t chutes[2];
    uint16_t buttons[2];
    uint8_t system_status;
    uint8_t usb_status;
    uint8_t padding[29];
} hid_joy;

/* Air sensor → IO4 button bit mapping (same as chu_arcade) */
static const uint16_t air_button_map[6][2] = {
    { 0, 1 << 11 },		// Air 0: buttons[1] bit 11
    { 1 << 11, 0 },		// Air 1: buttons[0] bit 11
    { 0, 1 << 12 },		// Air 2: buttons[1] bit 12
    { 1 << 12, 0 },		// Air 3: buttons[0] bit 12
    { 0, 1 << 13 },		// Air 4: buttons[1] bit 13
    { 1 << 13, 0 },		// Air 5: buttons[0] bit 13
};

/* IO4 button mapping for physical buttons */
#define IO4_BTN_TEST   (1 << 9)
#define IO4_BTN_SERVICE (1 << 6)

static void gen_hid_report()
{
    /* Clear buttons */
    hid_joy.buttons[0] = 0;
    hid_joy.buttons[1] = 0;

    /* Physical buttons */
    uint16_t btns = button_read();
    if (btns & (1 << 0)) {  // TEST button
        hid_joy.buttons[0] |= IO4_BTN_TEST;
    }
    if (btns & (1 << 1)) {  // SERVICE button
        hid_joy.buttons[0] |= IO4_BTN_SERVICE;
    }

    /* Coin counter: rising edge on button 2 */
    static bool last_coin = false;
    bool coin = (btns & (1 << 2)) != 0;
    if (coin && !last_coin) {
        hid_joy.chutes[0] += 0x100;
    }
    last_coin = coin;

    /* Air sensors: IO4 convention = bit set when hand ABSENT */
    uint8_t airkey = air_bitmap();
    for (int i = 0; i < 6; i++) {
        if (~airkey & (1 << i)) {
            hid_joy.buttons[0] |= air_button_map[i][0];
            hid_joy.buttons[1] |= air_button_map[i][1];
        }
    }

        /* NOTE: system_status and usb_status are NOT reset here.
     * They are set by tud_hid_set_report_cb() when the game sends
     * IO4 commands (0x01/0x02 -> 0x30, 0x03 -> 0).
     * Resetting them here would overwrite the game's commands.
     */
}

static void report_usb_hid()
{
    if (tud_hid_ready()) {
        tud_hid_n_report(0, REPORT_ID_JOYSTICK,
                         &hid_joy, sizeof(hid_joy));
    }
}

/* Timeout for auto-report mode (5 seconds without serial packets) */
#define IO4_SERIAL_TIMEOUT_MS 5000

/* Auto-report interval (4ms, matching original skogaslider firmware) */
/* Changed to 12ms to cap report to around ~80hz */
#define IO4_REPORT_INTERVAL_US 8000

const int aime_intf = 2;
static void cdc_aime_putc(uint8_t byte)
{
    tud_cdc_n_write(aime_intf, &byte, 1);
    tud_cdc_n_write_flush(aime_intf);
}

static void aime_run()
{
    if (tud_cdc_n_available(aime_intf)) {
        uint8_t buf[32];
        uint32_t count = tud_cdc_n_read(aime_intf, buf, sizeof(buf));

        i2c_select(I2C_PORT, 1 << 5); // PN532 on IR1 (I2C mux chn 5)
        for (int i = 0; i < count; i++) {
            aime_feed(buf[i]);
        }
    }
}

static void runtime_ctrl()
{
    /* Just use long-press SERVICE to reset touch in runtime */
    static bool applied = false;
    static uint64_t press_time = 0;
    static bool last_svc_button = false;
    bool svc_button = button_read() & 0x02;

    if (svc_button) {
        if (!last_svc_button) {
            press_time = time_us_64();
            applied = false;
        }
        if (!applied && (time_us_64() - press_time > 2000000)) {
            slider_sensor_init();
            applied = true;
        }
    }

    last_svc_button = svc_button;
}

static mutex_t save_lock;

static void core0_loop()
{
    uint64_t next_frame = time_us_64();
    while(1) {
        tud_task();

        cli_run();
        aime_run();
        protocol_run(PROTO_CDC_ITF);
    
        save_loop();
        cli_fps_count();

        slider_update();
        air_update();
        button_update();
        gen_hid_report();
        report_usb_hid();

                /* SEGA IO4 protocol handling */
        {
            uint64_t now = time_us_64();

            /* Read and process incoming slider packets */
            if (sega_serial_read_slider_packet(&sega_reader, &sega_request)) {
                last_io4_serial_time = now;
                sega_slider_process_packet(&sega_slider_state, &sega_request);
            }

            /* Disable auto-report after timeout */
            if (now - last_io4_serial_time > IO4_SERIAL_TIMEOUT_MS * 1000) {
                sega_slider_state.auto_send_reports = false;
            }

            /* Send auto-reports if enabled */
            if (now >= next_io4_report_time
                    && sega_slider_state.auto_send_reports
                    && !sega_serial_slider_packet_in_progress(&sega_reader)) {
                sega_slider_send_report(&sega_slider_state);
                next_io4_report_time = now + IO4_REPORT_INTERVAL_US;
            }
        }

        runtime_ctrl();

        sleep_until(next_frame);
        next_frame += 1000;
    }
}

/* if certain key pressed when booting, enter update mode */
static void update_check()
{
    const uint8_t pins[] = BUTTON_DEF;
    int pressed = 0;
    for (int i = 0; i < count_of(pins); i++) {
        uint8_t gpio = pins[i];
        gpio_init(gpio);
        gpio_set_function(gpio, GPIO_FUNC_SIO);
        gpio_set_dir(gpio, GPIO_IN);
        gpio_pull_up(gpio);
        sleep_ms(1);
        if (!gpio_get(gpio)) {
            pressed++;
        }
    }

    if (pressed >= 2) {
        sleep_ms(100);
        reset_usb_boot(0, 2);
        return;
    }
}

void init()
{
    sleep_ms(50);
    set_sys_clock_khz(150000, true);
    board_init();

    update_check();

    tusb_init();
    stdio_init_all();

    config_init();
    mutex_init(&save_lock);
    save_init(0xca34cafe, &save_lock);

    button_init();
    slider_init();
    air_init();

    nfc_attach_i2c(I2C_PORT);
    i2c_select(I2C_PORT, 1 << 5); // PN532 on IR1 (I2C mux chn 5)
    nfc_init();
    aime_init(cdc_aime_putc);
    aime_virtual_aic(chu_cfg->aime.virtual_aic);
    aime_sub_mode(chu_cfg->aime.mode);

                        cli_init("chu_pico>", "\n   << Chu Pico Native I/O Controller by Infecta >>\n"
                     " Original Project by: https://github.com/whowechina\n\n");
    
    commands_init();

    /* Initialize slider protocol state */
    sega_serial_reader_init(&sega_reader);
    sega_slider_init(&sega_slider_state);
}

int main(void)
{
    init();
    core0_loop();
    return 0;
}
