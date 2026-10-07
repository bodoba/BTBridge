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
#ifndef BT2USBD_MOUSE_H
#define BT2USBD_MOUSE_H

#include <stdint.h>
#include <stdbool.h>
#include <linux/input.h>
#include "hidinterface.h"

/* *********************************************************************************** *
 * @struct MouseReport_t
 * @brief Structure representing a standard USB HID boot mouse report.
 *
 * Layout: buttons (bitmap), x (signed relative), y (signed relative),
 * wheel (signed relative).
 * *********************************************************************************** */
typedef struct MouseReport {
    uint8_t buttons;  /**< Bit 0 = left, bit 1 = right, bit 2 = middle */
    int8_t  x;        /**< Relative X movement */
    int8_t  y;        /**< Relative Y movement */
    int8_t  wheel;    /**< Relative wheel movement */
} MouseReport_t;

/* *********************************************************************************** *
 * @brief Initializes a mouse report to the neutral (no movement, no buttons) state.
 *
 * @param[out] report Pointer to the MouseReport_t structure to initialize.
 * *********************************************************************************** */
static inline void initMouseReport(MouseReport_t *report)
{
    report->buttons = 0;
    report->x = 0;
    report->y = 0;
    report->wheel = 0;
}

/* *********************************************************************************** *
 * @brief Checks whether a given /dev/input/eventX device is a mouse.
 *
 * @param[in] event Name of the device file in /dev/input/ (e.g. "event3")
 *
 * @return File descriptor (>= 0) if the device is a mouse, -1 otherwise
 * *********************************************************************************** */
int checkForMouse(const char *event);

/* *********************************************************************************** *
 * @brief Iterates through /dev/input/ to find a mouse device.
 *
 * @return File descriptor (>= 0) on success, -1 if no mouse was found
 * *********************************************************************************** */
int findMouse(void);

/* *********************************************************************************** *
 * @brief Updates the button bitmap of a mouse report according to a button event.
 *
 * @param[in] buttons Current HID button bitmap
 * @param[in] ev Linux input event (EV_KEY, BTN_LEFT/BTN_RIGHT/BTN_MIDDLE)
 *
 * @return Updated HID button bitmap
 * *********************************************************************************** */
uint8_t updateMouseButtonState(uint8_t buttons, const struct input_event *ev);

/* *********************************************************************************** *
 * @brief Clamps a relative movement value to the signed 8 bit range used by
 *        the HID boot mouse report.
 *
 * @param[in] value Raw relative movement value (EV_REL)
 *
 * @return Value clamped to [-127, 127]
 * *********************************************************************************** */
int8_t clampMouseMovement(int value);

#endif