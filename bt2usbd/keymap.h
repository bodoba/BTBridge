/* *********************************************************************************** */
/*                                                                                     */
/*  Copyright (c) 2026 by Bodo Bauer <bb@bb-zone.com>                                  */
/*                                                                                     */
/*  This program is free software: you can redistribute it and/or modify               */
/*  it under the terms of the GNU General Public License as published by               */
/*  the Free Software Foundation, either version 3 of the License, or                  */
/*  (at your option) any later version.                                                */
/*                                                                                     */
/*  This program is distributed in the hope that it will be useful,                    */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of                     */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                      */
/*  GNU General Public License for more details.                                       */
/*                                                                                     */
/*  You should have received a copy of the GNU General Public License                  */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>.              */
/* *********************************************************************************** */

#ifndef BT2USBD_KEYMAP_H
#define BT2USBD_KEYMAP_H

#include <stdint.h>
#include <linux/input-event-codes.h>

/* *********************************************************************************** *
 * Linux EV_KEY code to USB HID Usage mapping.
 *
 * HID Usage Page:
 * 0x07 - Keyboard/Keypad
 *
 * Modifier keys are intentionally excluded because they
 * are handled by the HID modifier byte.
 * *********************************************************************************** */
static const uint8_t hidKeyMap[KEY_MAX + 1] = {

    /* Letters */
    [KEY_A] = 0x04,
    [KEY_B] = 0x05,
    [KEY_C] = 0x06,
    [KEY_D] = 0x07,
    [KEY_E] = 0x08,
    [KEY_F] = 0x09,
    [KEY_G] = 0x0A,
    [KEY_H] = 0x0B,
    [KEY_I] = 0x0C,
    [KEY_J] = 0x0D,
    [KEY_K] = 0x0E,
    [KEY_L] = 0x0F,
    [KEY_M] = 0x10,
    [KEY_N] = 0x11,
    [KEY_O] = 0x12,
    [KEY_P] = 0x13,
    [KEY_Q] = 0x14,
    [KEY_R] = 0x15,
    [KEY_S] = 0x16,
    [KEY_T] = 0x17,
    [KEY_U] = 0x18,
    [KEY_V] = 0x19,
    [KEY_W] = 0x1A,
    [KEY_X] = 0x1B,
    [KEY_Y] = 0x1C,
    [KEY_Z] = 0x1D,

    /* Number row */
    [KEY_1] = 0x1E,
    [KEY_2] = 0x1F,
    [KEY_3] = 0x20,
    [KEY_4] = 0x21,
    [KEY_5] = 0x22,
    [KEY_6] = 0x23,
    [KEY_7] = 0x24,
    [KEY_8] = 0x25,
    [KEY_9] = 0x26,
    [KEY_0] = 0x27,

    /* Common keys */
    [KEY_ENTER]     = 0x28,
    [KEY_ESC]       = 0x29,
    [KEY_BACKSPACE] = 0x2A,
    [KEY_TAB]       = 0x2B,
    [KEY_SPACE]     = 0x2C,
    [KEY_MINUS]     = 0x2D,
    [KEY_EQUAL]     = 0x2E,
    [KEY_LEFTBRACE] = 0x2F,
    [KEY_RIGHTBRACE]= 0x30,
    [KEY_BACKSLASH] = 0x31,
    [KEY_SEMICOLON] = 0x33,
    [KEY_APOSTROPHE]= 0x34,
    [KEY_GRAVE]     = 0x35,
    [KEY_COMMA]     = 0x36,
    [KEY_DOT]       = 0x37,
    [KEY_SLASH]     = 0x38,
    [KEY_CAPSLOCK]  = 0x39,

    /* Function keys */
    [KEY_F1]  = 0x3A,
    [KEY_F2]  = 0x3B,
    [KEY_F3]  = 0x3C,
    [KEY_F4]  = 0x3D,
    [KEY_F5]  = 0x3E,
    [KEY_F6]  = 0x3F,
    [KEY_F7]  = 0x40,
    [KEY_F8]  = 0x41,
    [KEY_F9]  = 0x42,
    [KEY_F10] = 0x43,
    [KEY_F11] = 0x44,
    [KEY_F12] = 0x45,

    /* Cursor keys */
    [KEY_RIGHT] = 0x4F,
    [KEY_LEFT]  = 0x50,
    [KEY_DOWN]  = 0x51,
    [KEY_UP]    = 0x52,

    /* Navigation */
    [KEY_INSERT]   = 0x49,
    [KEY_HOME]     = 0x4A,
    [KEY_PAGEUP]   = 0x4B,
    [KEY_DELETE]   = 0x4C,
    [KEY_END]      = 0x4D,
    [KEY_PAGEDOWN] = 0x4E,

    /* Keypad */
    [KEY_NUMLOCK]     = 0x53,
    [KEY_KPSLASH]     = 0x54,
    [KEY_KPASTERISK]  = 0x55,
    [KEY_KPMINUS]     = 0x56,
    [KEY_KPPLUS]      = 0x57,
    [KEY_KPENTER]     = 0x58,
    [KEY_KP1]         = 0x59,
    [KEY_KP2]         = 0x5A,
    [KEY_KP3]         = 0x5B,
    [KEY_KP4]         = 0x5C,
    [KEY_KP5]         = 0x5D,
    [KEY_KP6]         = 0x5E,
    [KEY_KP7]         = 0x5F,
    [KEY_KP8]         = 0x60,
    [KEY_KP9]         = 0x61,
    [KEY_KP0]         = 0x62,
    [KEY_KPDOT]       = 0x63,

    /* Apple specials */
    [KEY_MENU]      = 0x65,
    [KEY_SYSRQ]     = 0x46,
    [KEY_PAUSE]     = 0x48,
};

/* *********************************************************************************** *
* Linux EV_KEY to HID Consumer Control Usage mapping.
*
* HID Usage Page 0x0C (Consumer Devices)
*
* Usage values are sent through the Consumer Control
* interface (/dev/hidg2).
* *********************************************************************************** */
static const uint16_t hidConsumerMap[KEY_MAX + 1] = {

    /* Audio */
    [KEY_MUTE]          = 0x00E2,
    [KEY_VOLUMEUP]      = 0x00E9,
    [KEY_VOLUMEDOWN]    = 0x00EA,

    /* Media playback */
    [KEY_PLAY]          = 0x00B0,
    [KEY_PAUSE]         = 0x00B1,
    [KEY_RECORD]        = 0x00B2,
    [KEY_FASTFORWARD]   = 0x00B3,
    [KEY_REWIND]        = 0x00B4,
    [KEY_NEXTSONG]      = 0x00B5,
    [KEY_PREVIOUSSONG]  = 0x00B6,
    [KEY_STOPCD]        = 0x00B7,
    [KEY_EJECTCD]       = 0x00B8,
    [KEY_PLAYPAUSE]     = 0x00CD,

    /* Browser */
    [KEY_HOMEPAGE]      = 0x0223,
    [KEY_BACK]          = 0x0224,
    [KEY_FORWARD]       = 0x0225,
    [KEY_REFRESH]       = 0x0227,
    [KEY_FAVORITES]     = 0x022A,

    /* Application launch */
    [KEY_EMAIL]         = 0x018A,
    [KEY_CALC]          = 0x0192,
    [KEY_COMPUTER]      = 0x0194,

    /* Display brightness */
    [KEY_BRIGHTNESSUP]   = 0x006F,
    [KEY_BRIGHTNESSDOWN] = 0x0070,

    /* Non Standard Keyn on Apple Magic Keyboard */
    [KEY_SLEEP]          = 0x0032,
    [KEY_MICMUTE]        = 0x00F8,
    [KEY_SEARCH]         = 0x0221,   // HID Search
    [KEY_SCALE]          = 0x029F,   // AC Desktop Show All Windows / Mission Control  
    [KEY_COFFEE]         = 0x019E    // AL Terminal Lock / Screensaver
};

/* *********************************************************************************** *
 * @brief Convert a Linux key code into a HID Consumer Control usage code.
 *
 * @param report Pointer to consumer report structure
 * @param code Linux EV_KEY code.
 * @param ev Input even structure
 * *********************************************************************************** */
static inline void linuxKeyToConsumer(ConsumerReport_t *report, const struct input_event *ev) {
    if (ev != NULL && ev->code <= KEY_MAX) {
        if (ev->value) {
            uint16_t usage = hidConsumerMap[ev->code];
            report->keys[0] = usage & 0xff;
            report->keys[1] = usage >> 8;
        } else {
            report->keys[0] = 0;
            report->keys[1] = 0;
        }
    }  
} 

/**
 * Convert a Linux input event key code into a USB HID key code.
 *
 * The function expects a regular key event. Modifier keys
 * and Consumer Control keys must be handled separately.
 *
 * @param ev Linux input event.
 *
 * @return USB HID usage code or 0 if unsupported.
 */
static inline uint8_t linuxKeyToHid(const struct input_event *ev) {
    uint8_t hidCode = 0;
    if (ev != NULL) {
        if (ev->type == EV_KEY) {
            if (ev->code <= KEY_MAX) {
                hidCode = hidKeyMap[ev->code];
            }
        }
    }
    return hidCode;
}

#endif