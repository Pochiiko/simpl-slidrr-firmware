/*
 * Air Sensor
 * Original code by WHowe <github.com/whowechina>
 * Modified by Infecta <github.com/Infecta>
 */

#include "air.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "pico/time.h"

#include "board_defs.h"
#include "config.h"

static const uint8_t IR_ABC[] = IR_GROUP_ABC_GPIO;
static const uint8_t IR_SIG[] = IR_SIG_ADC_CHANNEL;
static_assert(count_of(IR_ABC) == 3, "IR should have 3 groups");
static_assert(count_of(IR_SIG) == 2, "IR should use 2 analog signals");
static uint16_t ir_raw[6];
static bool ir_blocked[6];
#define IR_DEBOUNCE_PERCENT 90

static void air_init_ir()
{
    for (int i = 0; i < count_of(IR_ABC); i++) {
        gpio_init(IR_ABC[i]);
        gpio_set_dir(IR_ABC[i], GPIO_OUT);
        gpio_put(IR_ABC[i], 0);
        gpio_set_drive_strength(IR_ABC[i], GPIO_DRIVE_STRENGTH_12MA);
    }
    for (int i = 0; i < count_of(IR_SIG); i++) {
        adc_init();
        adc_gpio_init(26 + IR_SIG[i]);
    }
}

void air_init()
{
    air_init_ir();
}

static uint8_t ir_bitmap()
{
    uint8_t bitmap = 0;
    for (int i = 0; i < count_of(ir_blocked); i++) {
        bitmap |= ir_blocked[i] << i;
    }
    return bitmap;
}

uint8_t air_bitmap()
{
    return ir_bitmap();
}

uint16_t air_ir_raw(uint8_t index)
{
    if (index >= count_of(ir_raw)) {
        return 0;
    }
    return ir_raw[index];
}

bool air_ir_blocked(uint8_t index)
{
    if (index >= count_of(ir_blocked)) {
        return false;
    }
    return ir_blocked[index];
}

static void ir_read()
{
    static int phase = 0;

    gpio_put(IR_ABC[phase], 1);
    sleep_us(10); // time for phototransistor to settle down

    for (int i = 0; i < 2; i++) {
        adc_select_input(IR_SIG[i]);
        sleep_us(2);
        ir_raw[phase * 2 + i] = adc_read();
    }

    gpio_put(IR_ABC[phase], 0);

    phase = (phase + 1) % count_of(IR_ABC);
}

static void ir_judge()
{
    for (int i = 0; i < count_of(ir_raw); i++) {
        int offset = chu_cfg->ir.base[i] - ir_raw[i];
        int threshold = chu_cfg->ir.base[i] * chu_cfg->ir.trigger[i] / 100;

        if (ir_blocked[i]) {
            threshold = threshold * IR_DEBOUNCE_PERCENT / 100;
        }

        ir_blocked[i] = (offset >= threshold);
    }
}

static void ir_diagnostic()
{
    if (!chu_runtime.ir_diagnostics) {
        return;
    }

    static uint64_t last_print = 0;
    uint64_t now = time_us_64();
    if (now - last_print > 500000) {
        printf("IR: ");
        for (int i = 0; i < count_of(ir_raw); i++) {
            printf(" %4d", ir_raw[i]);
        }
        printf("\n");
        last_print = now;
    }
}

static void air_update_ir()
{
    ir_read();
    ir_judge();
    ir_diagnostic();
}

void air_update()
{

    air_update_ir();
}
