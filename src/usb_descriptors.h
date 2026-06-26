/*
 * simpl-slidrr - USB Descriptors
 * HID IO4 Joystick + 4x CDC
 * Original code by WHowe <github.com/whowechina>
 * Modified by Infecta <github.com/Infecta>
 */

#ifndef USB_DESCRIPTORS_H_
#define USB_DESCRIPTORS_H_

#include "common/tusb_common.h"
#include "device/usbd.h"

enum {
    REPORT_ID_JOYSTICK = 1,
    REPORT_ID_OUTPUT = 16,
};

// because they are missing from tusb_hid.h
#define HID_STRING_INDEX(x) HID_REPORT_ITEM(x, 7, RI_TYPE_LOCAL, 1)
#define HID_STRING_INDEX_N(x, n) HID_REPORT_ITEM(x, 7, RI_TYPE_LOCAL, n)
#define HID_STRING_MINIMUM(x) HID_REPORT_ITEM(x, 8, RI_TYPE_LOCAL, 1)
#define HID_STRING_MINIMUM_N(x, n) HID_REPORT_ITEM(x, 8, RI_TYPE_LOCAL, n)
#define HID_STRING_MAXIMUM(x) HID_REPORT_ITEM(x, 9, RI_TYPE_LOCAL, 1)
#define HID_STRING_MAXIMUM_N(x, n) HID_REPORT_ITEM(x, 9, RI_TYPE_LOCAL, n)

// Joystick Descriptor to match a real SEGA IO4 I/O Board
// Infecta:: I'm not gonna lie, I don't know what any of these mean
#define CHUPICO_REPORT_DESC_JOYSTICK                                       \
    HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),                                \
    HID_USAGE(HID_USAGE_DESKTOP_GAMEPAD),                                  \
    HID_COLLECTION(HID_COLLECTION_APPLICATION),                            \
        HID_REPORT_ID(REPORT_ID_JOYSTICK)                                  \
        HID_REPORT_COUNT(1), HID_REPORT_SIZE(16),                          \
        HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),                            \
        HID_USAGE(HID_USAGE_DESKTOP_X),                                    \
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                 \
                                                                           \
        HID_REPORT_COUNT(1), HID_REPORT_SIZE(16),                          \
        HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),                            \
        HID_USAGE(HID_USAGE_DESKTOP_Y),                                    \
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                 \
                                                                           \
        HID_REPORT_COUNT(1), HID_REPORT_SIZE(16),                          \
        HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),                            \
        HID_USAGE(HID_USAGE_DESKTOP_X),                                    \
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                 \
                                                                           \
        HID_REPORT_COUNT(1), HID_REPORT_SIZE(16),                          \
        HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),                            \
        HID_USAGE(HID_USAGE_DESKTOP_Y),                                    \
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                 \
                                                                           \
        HID_REPORT_COUNT(1), HID_REPORT_SIZE(16),                          \
        HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),                            \
        HID_USAGE(HID_USAGE_DESKTOP_X),                                    \
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                 \
                                                                           \
        HID_REPORT_COUNT(1), HID_REPORT_SIZE(16),                          \
        HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),                            \
        HID_USAGE(HID_USAGE_DESKTOP_Y),                                    \
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                 \
                                                                           \
        HID_REPORT_COUNT(1), HID_REPORT_SIZE(16),                          \
        HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),                            \
        HID_USAGE(HID_USAGE_DESKTOP_X),                                    \
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                 \
                                                                           \
        HID_REPORT_COUNT(1), HID_REPORT_SIZE(16),                          \
        HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),                            \
        HID_USAGE(HID_USAGE_DESKTOP_Y),                                    \
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                 \
                                                                           \
        HID_REPORT_COUNT(1), HID_REPORT_SIZE(16),                          \
        HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),                            \
        HID_USAGE(HID_USAGE_DESKTOP_RX),                                   \
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                 \
                                                                           \
        HID_REPORT_COUNT(1), HID_REPORT_SIZE(16),                          \
        HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),                            \
        HID_USAGE(HID_USAGE_DESKTOP_RY),                                   \
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                 \
                                                                           \
        HID_REPORT_COUNT(1), HID_REPORT_SIZE(16),                          \
        HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),                            \
        HID_USAGE(HID_USAGE_DESKTOP_RX),                                   \
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                 \
                                                                           \
        HID_REPORT_COUNT(1), HID_REPORT_SIZE(16),                          \
        HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),                            \
        HID_USAGE(HID_USAGE_DESKTOP_RY),                                   \
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                 \
                                                                           \
        HID_REPORT_COUNT(1), HID_REPORT_SIZE(16),                          \
        HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),                            \
        HID_USAGE(HID_USAGE_DESKTOP_SLIDER),                               \
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                 \
                                                                           \
        HID_REPORT_COUNT(1), HID_REPORT_SIZE(16),                          \
        HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),                            \
        HID_USAGE(HID_USAGE_DESKTOP_SLIDER),                               \
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                 \
                                                                           \
        HID_REPORT_COUNT(48), HID_REPORT_SIZE(1),                          \
        HID_USAGE_MAX_N(48, 2), HID_USAGE_MIN_N(1, 2),                     \
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                 \
                                                                           \
        HID_REPORT_COUNT(1), HID_REPORT_SIZE(232),                         \
        HID_INPUT(HID_CONSTANT | HID_ABSOLUTE),                            \
                                                                           \
        HID_USAGE_PAGE_N(0xffa0, 2),                                       \
        HID_USAGE(0x00),                                                   \
        HID_REPORT_ID(REPORT_ID_OUTPUT)                                    \
        HID_COLLECTION(HID_COLLECTION_APPLICATION),                        \
            HID_USAGE(0x00),                                               \
            HID_LOGICAL_MIN(0), HID_LOGICAL_MAX(255),                      \
            HID_REPORT_COUNT(63), HID_REPORT_SIZE(8),                      \
            HID_OUTPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),            \
        HID_COLLECTION_END,                                                \
    HID_COLLECTION_END

#endif /* USB_DESCRIPTORS_H_ */
