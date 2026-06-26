#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>

#include "pico/stdio.h"
#include "pico/stdlib.h"

#include "config.h"
#include "air.h"
#include "slider.h"
#include "save.h"
#include "cli.h"

#include "i2c_hub.h"

#include "button.h"

#include "nfc.h"
#include "aime.h"

#define SENSE_LIMIT_MAX 9
#define SENSE_LIMIT_MIN -9

static void disp_ir()
{
    printf("[IR]\n");
    printf("  Base:");
    for (int i = 0; i < count_of(chu_cfg->ir.base); i++) {
        printf(" %d", chu_cfg->ir.base[i]);
    }
    printf("\n  Trigger:");
    for (int i = 0; i < count_of(chu_cfg->ir.trigger); i++) {
        printf(" %d%%", chu_cfg->ir.trigger[i]);
    }
    printf("\n");
}

static void disp_sense()
{
    printf("[Sense]\n");
    printf("  Filter: %u, %u, %u\n", chu_cfg->sense.filter >> 6,
                                    (chu_cfg->sense.filter >> 4) & 0x03,
                                    chu_cfg->sense.filter & 0x07);
    printf("  Sensitivity (global: %+d):\n", chu_cfg->sense.global);
    printf("    | 1| 2| 3| 4| 5| 6| 7| 8| 9|10|11|12|13|14|15|16|\n");
    printf("  ---------------------------------------------------\n");
    printf("  A |");
    for (int i = 0; i < 16; i++) {
        printf("%+2d|", chu_cfg->sense.keys[i * 2]);
    }
    printf("\n  B |");
    for (int i = 0; i < 16; i++) {
        printf("%+2d|", chu_cfg->sense.keys[i * 2 + 1]);
    }
    printf("\n");
    printf("  Debounce (touch, release): %d, %d\n",
           chu_cfg->sense.debounce_touch, chu_cfg->sense.debounce_release);
}

static void disp_aime()
{
    printf("[AIME]\n");
    printf("   NFC Module: %s\n", nfc_module_name());
    printf("  Virtual AIC: %s\n", chu_cfg->aime.virtual_aic ? "ON" : "OFF");
    printf("         Mode: %d\n", chu_cfg->aime.mode);
}

static void disp_misc()
{
    printf("[Misc]\n");
    printf("  Ground Slider Delay: %u ms\n", chu_cfg->tweak.delay_ms);
}

void handle_display(int argc, char *argv[])
{
    const char *usage = "Usage: display [ir|sense|aime|tweak]\n";
    if (argc > 1) {
        printf(usage);
        return;
    }

    if (argc == 0) {
        disp_ir();
        disp_sense();
        disp_aime();
        disp_misc();
        return;
    }

    const char *choices[] = {"ir", "sense", "aime", "tweak"};
    switch (cli_match_prefix(choices, count_of(choices), argv[0])) {
        case 0:
            disp_ir();
            break;
        case 1:
            disp_sense();
            break;
        case 2:
            disp_aime();
            break;
        case 3:
            disp_misc();
            break;
        default:
            printf(usage);
            break;
    }
}

static void handle_stat(int argc, char *argv[])
{
    if (argc == 0) {
        for (int col = 0; col < 4; col++) {
            printf(" %2dA |", col * 4 + 1);
            for (int i = 0; i < 4; i++) {
                printf("%6u|", slider_count(col * 8 + i * 2));
            }
            printf("\n   B |");
            for (int i = 0; i < 4; i++) {
                printf("%6u|", slider_count(col * 8 + i * 2 + 1));
            }
            printf("\n");
        }
    } else if ((argc == 1) &&
               (strncasecmp(argv[0], "reset", strlen(argv[0])) == 0)) {
        slider_reset_stat();
    } else {
        printf("Usage: stat [reset]\n");
    }
}

static void air_diagnostic()
{
    chu_runtime.ir_diagnostics = !chu_runtime.ir_diagnostics;
    printf("IR Diagnostics: %s\n", chu_runtime.ir_diagnostics ? "ON" : "OFF");
}

static void air_baseline()
{
    config_capture_ir_baseline();
    printf("IR Baseline:");
    for (int i = 0; i < count_of(chu_cfg->ir.base); i++) {
        printf(" %4d", chu_cfg->ir.base[i]);
    }
    printf("\n");
}

static void air_trigger(char *argv[])
{
    const char *usage = "Usage: ir trigger <percent>\n"
                        "  percent: [1..100]\n";

    if (strncasecmp(argv[0], "trigger", strlen(argv[0])) != 0) {
        printf(usage);
        return;
    }

    int percent = cli_extract_non_neg_int(argv[1], 0);
    if ((percent < 1) || (percent > 100)) {
        printf(usage);
        return;
    }

    if (!config_set_ir_trigger(percent)) {
        printf(usage);
        return;
    }
    disp_ir();
}

static void handle_ir(int argc, char *argv[])
{
    const char *usage = "Usage: ir <diagnostic|baseline>\n"
                        "       ir trigger <percent>\n"
                        "  percent: [1..100]\n";
    if (argc == 1) {
        const char *commands[] = { "diagnostic", "baseline" };
        switch (cli_match_prefix(commands, count_of(commands), argv[0])) {
        case 0:
            air_diagnostic();
            return;  // runtime-only, no flash save
        case 1:
            air_baseline();
            config_changed();
            return;
        default:
            printf(usage);
            return;
        }
    } else if (argc == 2) {
        air_trigger(argv);
    } else {
        printf(usage);
    }
}

static void handle_filter(int argc, char *argv[])
{
    const char *usage = "Usage: filter <first> <second> [interval]\n"
                        "Adjusts MPR121 noise filtering parameters (see datasheets).\n"
                        "    first:    First Filter Iterations  (FFI) [0..3]\n"
                        "    second:   Second Filter Iterations (SFI) [0..3]\n"
                        "    interval: Electrode Sample Interval (ESI) [0..7]\n";
    if ((argc < 2) || (argc > 3)) {
        printf(usage);
        return;
    }

    int ffi = cli_extract_non_neg_int(argv[0], 0);
    int sfi = cli_extract_non_neg_int(argv[1], 0);
    int intv = chu_cfg->sense.filter & 0x07;
    if (argc == 3) {
        intv = cli_extract_non_neg_int(argv[2], 0);
    }

    if ((ffi < 0) || (ffi > 3) || (sfi < 0) || (sfi > 3) ||
        (intv < 0) || (intv > 7)) {
        printf(usage);
        return;
    }
    if (!config_set_filter(ffi, sfi, intv)) {
        printf(usage);
        return;
    }
    disp_sense();
}

static int8_t *extract_key(const char *param)
{
    int len = strlen(param);

    int offset;
    if (toupper(param[len - 1]) == 'A') {
        offset = 0;
    } else if (toupper(param[len - 1]) == 'B') {
        offset = 1;
    } else {
        return NULL;
    }

    int id = cli_extract_non_neg_int(param, len - 1) - 1;
    if ((id < 0) || (id > 15)) {
        return NULL;
    }

    return &chu_cfg->sense.keys[id * 2 + offset];
}

static void sense_do_op(int8_t *target, char op)
{
    if (op == '+') {
        if (*target < SENSE_LIMIT_MAX) {
            (*target)++;
        }
    } else if (op == '-') {
        if (*target > SENSE_LIMIT_MIN) {
            (*target)--;
        }
    } else if (op == '0') {
        *target = 0;
    }
}

static void handle_sense(int argc, char *argv[])
{
    const char *usage = "Usage: sense [key|*] <+|-|0>\n"
                        "Example:\n"
                        "  >sense +\n"
                        "  >sense -\n"
                        "  >sense 1A +\n"
                        "  >sense 13B -\n"
                        "  >sense * 0\n";
    if ((argc < 1) || (argc > 2)) {
        printf(usage);
        return;
    }

    const char *op = argv[argc - 1];
    if ((strlen(op) != 1) || !strchr("+-0", op[0])) {
        printf(usage);
        return;
    }

    if (argc == 1) {
        sense_do_op(&chu_cfg->sense.global, op[0]);
    } else {
        if (strcmp(argv[0], "*") == 0) {
            for (int i = 0; i < 32; i++) {
                sense_do_op(&chu_cfg->sense.keys[i], op[0]);
            }
        } else {
            int8_t *key = extract_key(argv[0]);
            if (!key) {
                printf(usage);
                return;
            }
            sense_do_op(key, op[0]);
        }
    }

    slider_update_config();
    config_changed();
    disp_sense();
}

static void handle_debounce(int argc, char *argv[])
{
    const char *usage = "Usage: debounce <touch> [release]\n"
                        "  touch, release: 0..7\n";
    if ((argc < 1) || (argc > 2)) {
        printf(usage);
        return;
    }

    int touch = chu_cfg->sense.debounce_touch;
    int release = chu_cfg->sense.debounce_release;
    if (argc >= 1) {
        touch = cli_extract_non_neg_int(argv[0], 0);
    }
    if (argc == 2) {
        release = cli_extract_non_neg_int(argv[1], 0);
    }

    if ((touch < 0) || (release < 0) ||
        (touch > 7) || (release > 7)) {
        printf(usage);
        return;
    }

    if (!config_set_debounce(touch, release)) {
        printf(usage);
        return;
    }
    disp_sense();
}

static void handle_raw()
{
    printf("%s\n", slider_sensor_status());
    printf("Raw readings:\n");
    const uint16_t *raw = slider_raw();
    printf("|");
    for (int i = 0; i < 16; i++) {
        printf("%3d|", raw[i * 2]);
    }
    printf("\n|");
    for (int i = 0; i < 16; i++) {
        printf("%3d|", raw[i * 2 + 1]);
    }
    printf("\n");
}

static void handle_save()
{
    config_save_now();
}

static void handle_factory_reset()
{
    config_factory_reset();
    printf("Factory reset done.\n");
}

static void handle_nfc()
{
    i2c_select(I2C_PORT, 1 << 5); // PN532 on IR1 (I2C mux chn 5)
    printf("NFC module: %s\n", nfc_module_name());
    nfc_rf_field(true);
    nfc_card_t card = nfc_detect_card();
    nfc_rf_field(false);
    printf("Card: %s", nfc_card_type_str(card.card_type));
    for (int i = 0; i < card.len; i++) {
        printf(" %02x", card.uid[i]);
    }
    printf("\n");
}

static bool handle_aime_mode(const char *mode)
{
    if (strcmp(mode, "0") == 0) {
        return config_set_aime_mode(0);
    }
    if (strcmp(mode, "1") == 0) {
        return config_set_aime_mode(1);
    }
    return false;
}

static bool handle_aime_virtual(const char *onoff)
{
    if (strcasecmp(onoff, "on") == 0) {
        return config_set_aime_virtual(true);
    }
    if (strcasecmp(onoff, "off") == 0) {
        return config_set_aime_virtual(false);
    }
    return false;
}

static void handle_aime(int argc, char *argv[])
{
    const char *usage = "Usage:\n"
                        "    aime mode <0|1>\n"
                        "    aime virtual <on|off>\n";
    if (argc != 2) {
        printf("%s", usage);
        return;
    }

    const char *commands[] = { "mode", "virtual" };
    int match = cli_match_prefix(commands, 2, argv[0]);
    
    bool ok = false;
    if (match == 0) {
        ok = handle_aime_mode(argv[1]);
    } else if (match == 1) {
        ok = handle_aime_virtual(argv[1]);
    }

    if (ok) {
        disp_aime();
    } else {
        printf("%s", usage);
    }
}

static void handle_delay(int argc, char *argv[])
{
    const char *usage = "Usage: delay <0..250>\n"
                        "  Delay in ms for ground slider reports (0 = no delay).\n";
    if (argc > 1) {
        printf(usage);
        return;
    }

    if (argc == 0) {
        printf("Ground slider delay: %u ms\n", chu_cfg->tweak.delay_ms);
        return;
    }

    int ms = cli_extract_non_neg_int(argv[0], 0);
    if ((ms < 0) || (ms > 250)) {
        printf(usage);
        return;
    }

    if (!config_set_delay_ms(ms)) {
        printf(usage);
        return;
    }
    printf("Ground slider delay set to %u ms\n", chu_cfg->tweak.delay_ms);
}

static void handle_recalculate(int argc, char *argv[])
{
    printf("Recalibrating MPR121 touch sensors...\n");
    slider_sensor_init();
    printf("%s\n", slider_sensor_status());
    printf("Done.\n");
}

void commands_init()
{
    cli_register("display", handle_display, "Display all config.");
    cli_register("stat", handle_stat, "Display or reset statistics.");
    cli_register("ir", handle_ir, "Set IR config.");
    cli_register("filter", handle_filter, "Set pre-filter config.");
    cli_register("sense", handle_sense, "Set sensitivity config.");
    cli_register("debounce", handle_debounce, "Set debounce config.");
    cli_register("raw", handle_raw, "Show key raw readings.");
    cli_register("delay", handle_delay, "Set ground slider report delay (0-250ms).");
    cli_register("save", handle_save, "Save config to flash.");
    cli_register("factory", handle_factory_reset, "Reset everything to default.");
    cli_register("nfc", handle_nfc, "NFC debug.");
    cli_register("aime", handle_aime, "AIME settings.");
    cli_register("recalculate", handle_recalculate, "Re-calibrate MPR121 touch baselines.");
}
