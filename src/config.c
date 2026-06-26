/*
 * Controller Config and Runtime Data
 * WHowe <github.com/whowechina>
 * Modified by Infecta <github.com/Infecta>
 */

#include "config.h"
#include "save.h"
#include "slider.h"
#include "air.h"
#include "aime.h"
#include <stddef.h>

chu_cfg_t *chu_cfg;

static chu_cfg_t default_cfg = {
    .sense = {
        .filter = 0x10, // FFI=0, SFI=1, ESI=0
        .debounce_touch = 1,
        .debounce_release = 0,
    },
    .hid = {
        .io4 = 1,
    },
    .aime = {
        .mode = 0,
        .virtual_aic = 0,
    },
    .ir = {
        .base = { 3800, 3800, 3800, 3800, 3800, 3800 },
        .trigger = { 20, 20, 20, 20, 20, 20 },
    },
    .tweak = {
        .delay_ms = 0,
    },
};

chu_runtime_t chu_runtime = {0};

bool config_validate(const chu_cfg_t *cfg)
{
    if ((cfg->sense.filter >> 6) > 3 ||
        ((cfg->sense.filter >> 4) & 0x03) > 3 ||
        (cfg->sense.filter & 0x07) > 7) {
        return false;
    }
    if (cfg->sense.global > 9 || cfg->sense.global < -9) {
        return false;
    }
    if (cfg->sense.debounce_touch > 7 || cfg->sense.debounce_release > 7) {
        return false;
    }
    for (int i = 0; i < 32; i++) {
        if (cfg->sense.keys[i] > 9 || cfg->sense.keys[i] < -9) {
            return false;
        }
    }
    for (int i = 0; i < 6; i++) {
        if (cfg->ir.trigger[i] < 1 || cfg->ir.trigger[i] > 100) {
            return false;
        }
    }
    if (cfg->tweak.delay_ms > 250) {
        return false;
    }
    if (cfg->aime.mode > 1 || cfg->aime.virtual_aic > 1) {
        return false;
    }
    return true;
}

void config_sanitize_loaded()
{
    if ((chu_cfg->sense.filter >> 6) > 3 ||
        ((chu_cfg->sense.filter >> 4) & 0x03) > 3 ||
        (chu_cfg->sense.filter & 0x07) > 7) {
        chu_cfg->sense.filter = default_cfg.sense.filter;
        config_changed();
    }
    if (chu_cfg->sense.global > 9 || chu_cfg->sense.global < -9) {
        chu_cfg->sense.global = default_cfg.sense.global;
        config_changed();
    }
    for (int i = 0; i < 32; i++) {
        if (chu_cfg->sense.keys[i] > 9 || chu_cfg->sense.keys[i] < -9) {
            chu_cfg->sense.keys[i] = default_cfg.sense.keys[i];
            config_changed();
        }
    }
    if (chu_cfg->tweak.delay_ms > 250) {
        chu_cfg->tweak.delay_ms = default_cfg.tweak.delay_ms;
        config_changed();
    }
    if (chu_cfg->sense.debounce_touch > 7 ||
        chu_cfg->sense.debounce_release > 7) {
        chu_cfg->sense.debounce_touch = default_cfg.sense.debounce_touch;
        chu_cfg->sense.debounce_release = default_cfg.sense.debounce_release;
        config_changed();
    }
    for (int i = 0; i < 6; i++) {
        if (chu_cfg->ir.trigger[i] < 1 || chu_cfg->ir.trigger[i] > 100) {
            chu_cfg->ir.trigger[i] = default_cfg.ir.trigger[i];
            config_changed();
        }
    }
}

static void config_loaded()
{
    config_sanitize_loaded();
    config_apply_sense_side_effects();
    config_apply_aime_side_effects();
}

void config_changed()
{
    save_request(false);
}

void config_factory_reset()
{
    *chu_cfg = default_cfg;
    config_apply_sense_side_effects();
    config_apply_aime_side_effects();
    save_request(true);
}

void config_init()
{
    chu_cfg = (chu_cfg_t *)save_alloc(sizeof(*chu_cfg), &default_cfg, config_loaded);
}

_Static_assert(sizeof(chu_cfg_t) == 64, "chu_cfg_t wire size changed — update docs/protocol.md");
_Static_assert(offsetof(chu_cfg_t, tweak.delay_ms) == 56,
               "chu_cfg_t delay_ms offset changed — update tools/protocol_test.py");

void config_apply_sense_side_effects(void)
{
    slider_update_config();
}

void config_apply_aime_side_effects(void)
{
    aime_sub_mode(chu_cfg->aime.mode);
    aime_virtual_aic(chu_cfg->aime.virtual_aic);
}

void config_save_now(void)
{
    save_request(true);
}

void config_capture_ir_baseline(void)
{
    for (int i = 0; i < 6; i++) {
        chu_cfg->ir.base[i] = air_ir_raw(i);
    }
}

config_status_t config_apply_sections(uint32_t flags, const chu_cfg_t *patch, bool persist)
{
    if (!patch || flags == 0) {
        return CONFIG_ERR_INVALID;
    }

    chu_cfg_t trial = *chu_cfg;

    if (flags & CFG_SECTION_SENSE) {
        trial.sense = patch->sense;
    }
    if (flags & CFG_SECTION_HID) {
        trial.hid = patch->hid;
    }
    if (flags & CFG_SECTION_AIME) {
        trial.aime = patch->aime;
    }
    if (flags & CFG_SECTION_IR) {
        trial.ir = patch->ir;
    }
    if (flags & CFG_SECTION_TWEAK) {
        trial.tweak.delay_ms = patch->tweak.delay_ms;
    }

    if (!config_validate(&trial)) {
        return CONFIG_ERR_VALIDATE;
    }

    *chu_cfg = trial;

    if (flags & (CFG_SECTION_SENSE | CFG_SECTION_HID)) {
        config_apply_sense_side_effects();
    }
    if (flags & CFG_SECTION_AIME) {
        config_apply_aime_side_effects();
    }
    if (persist) {
        config_changed();
    }

    return CONFIG_OK;
}

bool config_set_filter(int ffi, int sfi, int esi)
{
    if (ffi < 0 || ffi > 3 || sfi < 0 || sfi > 3 || esi < 0 || esi > 7) {
        return false;
    }
    chu_cfg->sense.filter = (uint8_t)((ffi << 6) | (sfi << 4) | esi);
    config_apply_sense_side_effects();
    config_changed();
    return true;
}

bool config_set_ir_trigger(int percent)
{
    if (percent < 1 || percent > 100) {
        return false;
    }
    for (int i = 0; i < 6; i++) {
        chu_cfg->ir.trigger[i] = (uint8_t)percent;
    }
    config_changed();
    return true;
}

bool config_set_debounce(int touch, int release)
{
    if (touch < 0 || touch > 7 || release < 0 || release > 7) {
        return false;
    }
    chu_cfg->sense.debounce_touch = (uint8_t)touch;
    chu_cfg->sense.debounce_release = (uint8_t)release;
    config_apply_sense_side_effects();
    config_changed();
    return true;
}

bool config_set_delay_ms(int ms)
{
    if (ms < 0 || ms > 250) {
        return false;
    }
    chu_cfg->tweak.delay_ms = (uint16_t)ms;
    config_changed();
    return true;
}

bool config_set_aime_mode(int mode)
{
    if (mode < 0 || mode > 1) {
        return false;
    }
    chu_cfg->aime.mode = (uint8_t)mode;
    config_apply_aime_side_effects();
    config_changed();
    return true;
}

bool config_set_aime_virtual(bool on)
{
    chu_cfg->aime.virtual_aic = on ? 1 : 0;
    config_apply_aime_side_effects();
    config_changed();
    return true;
}
