/*
 * Controller Config
 * WHowe <github.com/whowechina>
 * Modified by Infecta <github.com/Infecta>
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#define CFG_VERSION 1

#define CFG_SECTION_SENSE  0x0001u
#define CFG_SECTION_HID    0x0002u
#define CFG_SECTION_AIME   0x0004u
#define CFG_SECTION_IR     0x0008u
#define CFG_SECTION_TWEAK  0x0010u

typedef enum {
    CONFIG_OK = 0,
    CONFIG_ERR_INVALID = 1,
    CONFIG_ERR_VALIDATE = 2,
} config_status_t;

typedef struct __attribute__((packed)) {
    struct {
        uint8_t filter; // FFI[6..7], SFI[4..5], ESI[0..3]
        int8_t global;
        uint8_t debounce_touch;
        uint8_t debounce_release;        
        int8_t keys[32];
    } sense;
    struct {
        uint8_t io4 : 1;
    } hid;
    struct {
        uint8_t mode : 4;
        uint8_t virtual_aic : 4;
    } aime;
    struct {
        uint16_t base[6];
        uint8_t trigger[6];
    } ir;
    struct {
        uint16_t delay_ms; // 0..250ms delay for ground slider reports
        uint8_t reserved[5];
    } tweak;
} chu_cfg_t;

typedef struct {
    bool debug;
    bool ir_diagnostics;
} chu_runtime_t;

extern chu_cfg_t *chu_cfg;
extern chu_runtime_t chu_runtime;

void config_init();
void config_changed();
void config_factory_reset();
void config_sanitize_loaded();

bool config_validate(const chu_cfg_t *cfg);
config_status_t config_apply_sections(uint32_t flags, const chu_cfg_t *patch, bool persist);
void config_capture_ir_baseline(void);
void config_save_now(void);
void config_apply_sense_side_effects(void);
void config_apply_aime_side_effects(void);

bool config_set_filter(int ffi, int sfi, int esi);
bool config_set_ir_trigger(int percent);
bool config_set_debounce(int touch, int release);
bool config_set_delay_ms(int ms);
bool config_set_aime_mode(int mode);
bool config_set_aime_virtual(bool on);

#endif
