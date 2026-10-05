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
#ifndef BT2USBD_KBD_H
#define BT2USBD_KBD_H
#include "hidinterface.h"


// HID Keypress data
typedef struct HidKeyboardReport {
    uint8_t modifier;
    uint8_t reserved;
    uint8_t keys[6];
} KeyReport_t;

typedef struct HidConsumerReport {
    uint8_t keys[2];
} ConsumerReport_t;

/**
 * Classification of keyboard events for HID forwarding.
 */
typedef enum {
    KEY_TYPE_INVALID = 0,
    KEY_TYPE_MODIFIER,
    KEY_TYPE_CONSUMER_CONTROL,
    KEY_TYPE_REGULAR,
    KEY_TYPE_TOGGLE_KEY,
    KEY_TYPE_CAPS_LOCK
} KeyType_t;

/* *********************************************************************************** *
 * @brief Check if this is a keyboard device
 *
 *  @param event  Name of the divece file in /dev/input/
 * @return       device handle, or -1 on failure
 * *********************************************************************************** */
int checkForKeyboard(const char *event);

/* *********************************************************************************** *
 * @brief Iterate throuogh /dev/input/ to find a keyboard device
 *
 * @return        device handle, or -1 on failure
 * *********************************************************************************** */
int findKeyboard(void);

/* *********************************************************************************** *
 * @brief Classify a Linux input event.
 *
 * @param ev Input event received from libevdev.
 *
 * @return Event classification.
 * *********************************************************************************** */
KeyType_t classifyKeyEvent(const struct input_event *ev);

/* *********************************************************************************** *
 * @brief Update the HID modifier byte according to a modifier key event.
 *
 * The function expects only modifier key events (Ctrl, Shift, Alt, Meta/GUI). 
 *
 * @param modifier Current HID modifier bitmap.
 * @param ev Linux input event.
 *
 * @return Updated HID modifier bitmap.
 * *********************************************************************************** */
uint8_t updateModifierState(uint8_t modifier, const struct input_event *ev);

/* *********************************************************************************** *
 * @brief Add a value to an array.
 *
 * Empty slots are marked with value 0.
 *
 * If the value already exists in the array, the function
 * succeeds without modifying the array.
 *
 * @param array Array to modify.
 * @param max Maximum number of elements in the array.
 * @param value Value to add.
 *
 * @return true  if the value exists or was added successfully
 *         false if no free slot was available.
 * *********************************************************************************** */
bool arrayAdd(uint8_t *array, size_t max, uint8_t value);

/* *********************************************************************************** *
 * @brief Remove a value from an array.
 *
 * Empty slots are marked with value 0.
 *
 * @param array Array to modify.
 * @param max Maximum number of elements in the array.
 * @param value Value to remove.
 *
 * @return true if the value was found and removed,
 *         false otherwise.
 * *********************************************************************************** */
bool arrayRemove(uint8_t *array, size_t max, uint8_t value);

/* *********************************************************************************** *
 * @brief initialize key report
 *
 * @param report pointer to keyReport structure
 * *********************************************************************************** */
static inline void initKeyReport(KeyReport_t *report) {
    report->modifier = 0;
    report->reserved = 0;
    for(int i=0; i<6; i++)
        report->keys[i]=0;
}

/* *********************************************************************************** *
 * @brief initialize consumer report
 *
 * @param report pointer to comsumerReport structure
 * *********************************************************************************** */
static inline void initConsumerReport(ConsumerReport_t *report) {
    for(int i=0; i<2; i++)
        report->keys[i]=0;
}

/* *********************************************************************************** *
 * @brief Processes LED Output Reports sent by the host.
 *
 * Reads from the gadget's hidg device - the kernel's
 * usb_f_hid driver delivers host-originated Output Reports via
 * this single bidirectional character device.
 *
 * @param[in] hidKbd HID device
 * @param[in,out] capsLockActive Current Caps Lock state
 * @param[in,out] numLockActive  Current Num Lock state
 *
 * @return true if the LED state changed
 * *********************************************************************************** */
bool processHidLedReport(HidDevice *hidKbd, bool *capsLockActive, bool *numLockActive);

/* *********************************************************************************** *
 * @brief Reads the actual Caps Lock / Num Lock LED state directly from
 *        the kernel, bypassing any libevdev-internal caching.
 *
 * This uses the EVIOCGLED ioctl, which queries the current LED state
 * of the input device directly from the kernel's input core - this
 * is the authoritative state, independent of event queue timing.
 *
 * @param[in] fd File descriptor of the keyboard input device
 * @param[out] capsLockActive Set to the current Caps Lock LED state
 * @param[out] numLockActive  Set to the current Num Lock LED state
 *
 * @return true on success, false if the ioctl failed
 * *********************************************************************************** */
bool readKeyboardLedState(int fd, bool *capsLockActive, bool *numLockActive);

#endif