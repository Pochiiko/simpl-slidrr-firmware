/*
 * simpl-slidrr - USB Descriptors
 * HID IO4 Joystick + 4x CDC
 * Original code by WHowe <github.com/whowechina>
 * Modified by Infecta <github.com/Infecta>
 */

#include <stdio.h>
#include <string.h>

#include "tusb.h"
#include "usb_descriptors.h"

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

    // Match SEGA's IO4 board VID and PID
    .idVendor = 0x0ca3,
    .idProduct = 0x0021,

    .bcdDevice = 0x0100,

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,

    .bNumConfigurations = 0x01};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const* tud_descriptor_device_cb(void) {
    return (uint8_t const*)&desc_device;
}

//--------------------------------------------------------------------+
// HID Report Descriptor
//--------------------------------------------------------------------+

uint8_t const desc_hid_report_joy[] = {
    CHUPICO_REPORT_DESC_JOYSTICK,
};

// Invoked when received GET HID REPORT DESCRIPTOR
uint8_t const* tud_hid_descriptor_report_cb(uint8_t itf)
{
    (void)itf;
    return desc_hid_report_joy;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

enum {
    ITF_NUM_JOY,
    ITF_NUM_CLI, ITF_NUM_CLI_DATA,
    ITF_NUM_IO4, ITF_NUM_IO4_DATA,
    ITF_NUM_AIME, ITF_NUM_AIME_DATA,
    ITF_NUM_CFG, ITF_NUM_CFG_DATA,
    ITF_NUM_TOTAL };

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + \
                          TUD_HID_INOUT_DESC_LEN + \
                          TUD_CDC_DESC_LEN * 4)

#define EPNUM_JOY_OUT   0x01
#define EPNUM_JOY_IN    0x81

#define EPNUM_CLI_NOTIF 0x85
#define EPNUM_CLI_OUT   0x06
#define EPNUM_CLI_IN    0x86

#define EPNUM_IO4_NOTIF  0x87
#define EPNUM_IO4_OUT    0x08
#define EPNUM_IO4_IN     0x88

#define EPNUM_AIME_NOTIF 0x89
#define EPNUM_AIME_OUT   0x0A
#define EPNUM_AIME_IN    0x8A

#define EPNUM_CFG_NOTIF  0x8B
#define EPNUM_CFG_OUT    0x0C
#define EPNUM_CFG_IN     0x8C

uint8_t const desc_configuration[] = {
    // Config number, interface count, string index, total length, attribute,
    // power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 200),

    // HID IO4 Joystick (IN + OUT for commands)
    TUD_HID_INOUT_DESCRIPTOR(ITF_NUM_JOY, 8, HID_ITF_PROTOCOL_NONE,
                       sizeof(desc_hid_report_joy), EPNUM_JOY_OUT, EPNUM_JOY_IN,
                       CFG_TUD_HID_EP_BUFSIZE, 1),

    TUD_CDC_DESCRIPTOR(ITF_NUM_CLI, 4, EPNUM_CLI_NOTIF,
                       8, EPNUM_CLI_OUT, EPNUM_CLI_IN, 64),

        TUD_CDC_DESCRIPTOR(ITF_NUM_IO4, 5, EPNUM_IO4_NOTIF,
                       8, EPNUM_IO4_OUT, EPNUM_IO4_IN, 64),

    TUD_CDC_DESCRIPTOR(ITF_NUM_AIME, 6, EPNUM_AIME_NOTIF,
                       8, EPNUM_AIME_OUT, EPNUM_AIME_IN, 64),

    TUD_CDC_DESCRIPTOR(ITF_NUM_CFG, 7, EPNUM_CFG_NOTIF,
                       8, EPNUM_CFG_OUT, EPNUM_CFG_IN, 64),
};

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    return desc_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// array of pointer to string descriptors
const char *string_desc_arr[] = {
    (const char[]){0x09, 0x04}, // 0: is supported language is English (0x0409)
    "SEGA",                     // 1: Manufacturer
    "simpl-slidrr",             // 2: Product
    "123456",                   // 3: Serials, should use chip ID
    "simpl-slidrr CLI Port",    // 4: CLI Port
    "simpl-slidrr Slider Port", // 5: Slider Port
    "simpl-slidrr AIME Port",   // 6: AIME Port
    "simpl-slidrr Config port", // 7: Config Port
    "I/O CONTROL BD;15257;01;90;1831;6679A;00;GOUT=14_ADIN=8,E_ROTIN=4_COININ=2_SWIN=2,E_UQ1=41,6;", // 8: SEGA IO4
};

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long
// enough for transfer to complete
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    static uint16_t _desc_str[128];

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 + 2);
        return _desc_str;
    }

    const size_t base_num = sizeof(string_desc_arr) / sizeof(string_desc_arr[0]);
    char str[128];

    if (index < base_num) {
        strcpy(str, string_desc_arr[index]);
    } else {
        sprintf(str, "Unknown %d", index);
    }

    uint8_t chr_count = strlen(str);
    if (chr_count > 127) {
        chr_count = 127;
    }

    // Convert ASCII string into UTF-16
    for (uint8_t i = 0; i < chr_count; i++) {
        _desc_str[1 + i] = str[i];
    }

    // first byte is length (including header), second byte is string type
    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);

    return _desc_str;
}

// IO4 HID report is defined in main.c
extern struct __attribute__((packed)) {
    uint16_t adcs[8];
    uint16_t spinners[4];
    uint16_t chutes[2];
    uint16_t buttons[2];
    uint8_t system_status;
    uint8_t usb_status;
    uint8_t padding[29];
} hid_joy;

// Invoked when received GET_REPORT control request
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen)
{
    (void)itf;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const *buffer,
                           uint16_t bufsize)
{
    (void)itf;
    (void)bufsize;

    if (report_type != HID_REPORT_TYPE_OUTPUT) {
        return;
    }

    // IO4 board commands, idk the game sends it... some smarter people probably know. ::infecta
    // Structure: report_id (1 byte) | cmd (1 byte) | payload (up to 62 bytes)
    uint8_t cmd = buffer[1];

    switch (cmd) {
        case 0x01:
        case 0x02:
            // Start/continue IO4 polling
            hid_joy.system_status = 0x30;
            break;
        case 0x03:
            // Reset
            hid_joy.chutes[0] = 0;
            hid_joy.chutes[1] = 0;
            hid_joy.system_status = 0;
            printf("IO4 HID reset.\n");
            break;
        case 0x04:
        case 0x41:
            // No-op
            break;
        default:
            printf("IO4 unknown cmd: %02x\n", cmd);
            break;
    }
}
