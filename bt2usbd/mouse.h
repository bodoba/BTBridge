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
#include <time.h>

#include "hidinterface.h"

/* *********************************************************************************** *
 * Scroll momentum tuning.
 *
 * Decay closer to 1.0 means slower velocity falloff, i.e. a longer
 * running momentum phase. Approximate duration until velocity falls
 * below SCROLL_MOMENTUM_MIN_VELOCITY, starting from v0:
 *
 *   duration_ms ≈ SCROLL_MOMENTUM_TICK_MS * ln(threshold / v0) / ln(decay)
 *
 * Example: decay=0.985, tick=16ms, v0=12, threshold=0.2
 *          -> about 170 ticks -> ~2.7 seconds of coasting.
 * *********************************************************************************** */ 
#define SCROLL_MOMENTUM_DECAY           0.95f   /**< Decay factor for scroll momentum  */
#define SCROLL_MOMENTUM_MIN_VELOCITY    0.2f    /**< Min. velocity for scroll momentum */
#define SCROLL_MOMENTUM_TICK_MS         16      /**< ms between momentum updates       */

/** HID_WHEEL conversion: Linux hi-res wheel reports 120 units per full "line".        */
#define WHEEL_HIRES_UNITS_PER_LINE      120.0f

/** If no REL_WHEEL event arrives within this time, scrolling is considered finished.  */
#define SCROLL_MOMENTUM_RELEASE_MS       280

/** Multiplies the accumulated flick velocity to make momentum feel punchier.          */
#define SCROLL_MOMENTUM_VELOCITY_SMOOTHING   0.5f

/** Number of recent velocity samples to average for release velocity calculation      */
#define SCROLL_VELOCITY_HISTORY_SIZE   3  

/** ********************************************************************************** *
 *  @struct ScrollMomentum_t 
 *  @brief Tracks scroll wheel velocity to emulate macOS-style scroll inertia.
 ** ********************************************************************************** */
typedef struct ScrollMomentum {
    bool active;
    float velocity;
    float lineAccum;
    float velocityHistory[SCROLL_VELOCITY_HISTORY_SIZE]; /**< Recent raw instant velocities */
    int   velocityHistoryCount;                          /**< Number of valid entries so far */
    struct timespec lastEvent;
    struct timespec lastTick;
} ScrollMomentum_t;

/** ********************************************************************************** *
 * @brief Initializes a scroll momentum tracker.
 * @param[in,out] m Momentum tracker to initialize.
 ** ********************************************************************************** */
static inline void initScrollMomentum(ScrollMomentum_t *m) {
    m->active = false;
    m->velocity = 0.0f;
    m->lineAccum = 0.0f;
    for (int i = 0; i < SCROLL_VELOCITY_HISTORY_SIZE; i++) m->velocityHistory[i] = 0.0f;
    m->velocityHistoryCount = 0;
    m->lastEvent.tv_sec = 0;
    m->lastEvent.tv_nsec = 0;
    m->lastTick.tv_sec = 0;
    m->lastTick.tv_nsec = 0;
}

/** ********************************************************************************** *
 * @struct ScrollSmoother_t
 * @brief Tracks fractional REL_WHEEL_HI_RES movement to produce whole-line wheel 
 * events.
 ** ********************************************************************************** */
typedef struct ScrollSmoother {
    float accum;    /**< Fractional line accumulator from REL_WHEEL_HI_RES */
} ScrollSmoother_t;

static inline void initScrollSmoother(ScrollSmoother_t *s) {
    s->accum = 0.0f;
}

/** ********************************************************************************** *
 * @brief Immediately starts the coasting phase using the accumulated
 *        velocity history. Called when the mouse reports that the
 *        finger has physically lifted off the scroll surface (BTN_TOUCH
 *        release), which is a much more reliable release signal than
 *        inferring it from gaps in the wheel event timing.
 *
 * @param[in,out] m Momentum state.
 ** ********************************************************************************** */
void notifyScrollFingerLifted(ScrollMomentum_t *m);

/** ********************************************************************************** *
 * @brief Converts a REL_WHEEL_HI_RES delta into whole HID wheel lines,
 *        keeping the fractional remainder for the next call.
 *
 * @param[in,out] s Smoother state.
 * @param[in] hiResDelta Raw REL_WHEEL_HI_RES delta (120 units = 1 line).
 *
 * @return Whole number of lines to send now (-N..N), usually -1, 0 or 1.
 ** ********************************************************************************** */
int scrollSmootherFeed(ScrollSmoother_t *s, int hiResDelta);

/** *********************************************************************************** *
 * @brief Checks whether the scroll momentum state machine currently
 *        needs frequent main loop ticks - either because coasting is
 *        active, or because a recent flick is still waiting to be
 *        classified as "released" by checkScrollRelease().
 *
 * @param[in] m Momentum state.
 *
 * @return true if the main loop should use a short select() timeout.
 ** ********************************************************************************** */
static inline bool scrollMomentumNeedsFastTick(const ScrollMomentum_t *m) {
    return m->active || m->lastEvent.tv_sec != 0;
}

/** ******************************************************************************** *
 * @brief Feeds a real REL_WHEEL_HI_RES event into the momentum tracker.
 *
 * @param[in,out] m Momentum state.
 * @param[in] hiResDelta Raw REL_WHEEL_HI_RES delta (120 units = 1 line).
 ** ******************************************************************************** */
void feedScrollMomentum(ScrollMomentum_t *m, int hiResDelta);

/** *********************************************************************************** *
 * @brief Mouse movement sensitivity multiplier.
 *
 * Linux delivers relatively small REL_X/REL_Y deltas per report. This
 * factor scales raw movement before clamping to the HID report range,
 * allowing the perceived cursor speed to be tuned without touching the
 * input device's own acceleration settings.
 ** ********************************************************************************** */
#define MOUSE_SENSITIVITY 0.5f
extern float mouseSensitivity;  /* Global variable for mouse sensitivity multiplier */

/** ********************************************************************************** *
 * @struct MouseReport_t
 * @brief Structure representing a standard USB HID boot mouse report.
 *
 * Layout: buttons (bitmap), x (signed relative), y (signed relative),
 * wheel (signed relative).
 ** ********************************************************************************** */
typedef struct MouseReport {
    uint8_t buttons;  /**< Bit 0 = left, bit 1 = right, bit 2 = middle */
    int8_t  x;        /**< Relative X movement */
    int8_t  y;        /**< Relative Y movement */
    int8_t  wheel;    /**< Relative wheel movement */
} MouseReport_t;

/** ********************************************************************************** *
 * @brief Initializes a mouse report to the neutral (no movement, no buttons) state.
 *
 * @param[out] report Pointer to the MouseReport_t structure to initialize.
 ** ********************************************************************************** */
static inline void initMouseReport(MouseReport_t *report)
{
    report->buttons = 0;
    report->x = 0;
    report->y = 0;
    report->wheel = 0;
}

/** ********************************************************************************** *
 * @brief Advances the momentum simulation and sends a synthetic
 *        wheel report if a decay tick is due.
 *
 * @param[in,out] m Momentum state.
 * @param[in,out] mouseReport Mouse report used to send the synthetic event.
 * @param[in] hidMouse HID device used to send the report.
 ** ********************************************************************************* */
void updateScrollMomentum(ScrollMomentum_t *m, MouseReport_t *mouseReport, HidDevice *hidMouse);

/** ********************************************************************************** *
 * @brief Checks whether finger scrolling has stopped and, if so, starts
 *        the coasting phase using the accumulated flick velocity.
 *
 * @param[in,out] m Momentum state.
 ** ********************************************************************************* */
void checkScrollRelease(ScrollMomentum_t *m);

/** *********************************************************************************** *
 * @brief Checks whether a given /dev/input/eventX device is a mouse.
 *
 * @param[in] event Name of the device file in /dev/input/ (e.g. "event3")
 *
 * @return File descriptor (>= 0) if the device is a mouse, -1 otherwise
 ** ********************************************************************************** */
int checkForMouse(const char *event);

/** *********************************************************************************** *
 * @brief Iterates through /dev/input/ to find a mouse device.
 *
 * @return File descriptor (>= 0) on success, -1 if no mouse was found
 ** *********************************************************************************** */
int findMouse(void);

/** *********************************************************************************** *
 * @brief Updates the button bitmap of a mouse report according to a button event.
 *
 * @param[in] buttons Current HID button bitmap
 * @param[in] ev Linux input event (EV_KEY, BTN_LEFT/BTN_RIGHT/BTN_MIDDLE)
 *
 * @return Updated HID button bitmap
 ** ********************************************************************************** */
uint8_t updateMouseButtonState(uint8_t buttons, const struct input_event *ev);

/** *********************************************************************************** *
 * @brief Clamps a relative movement value to the signed 8 bit range used by
 *        the HID boot mouse report.
 *
 * @param[in] value Raw relative movement value (EV_REL)
 *
 * @return Value clamped to [-127, 127]
 ** *********************************************************************************** */
int8_t clampMouseMovement(int value);

/** *********************************************************************************** *
 * @brief Processes pending input events from a mouse device.
 *
 * Updates the mouse report with button and relative movement events, then
 * sends it through the HID mouse device at the end of each input packet.
 * If the input device is disconnected, frees its libevdev context, closes
 * its file descriptor, and sets the descriptor to -1.
 *
 * @param[in] devMouse libevdev context for the mouse input device.
 * @param[in,out] mouseReport Mouse report updated with button and movement state.
 * @param[in,out] fdMouse Mouse input file descriptor; set to -1 on disconnection.
 * @param[in] hidMouse HID device used to send mouse reports.
 * @param[in,out] momentum Scroll momentum state used to implement coasting.
 * @param[in,out] smoother Scroll smoother state used to convert high-resolution
 ** ********************************************************************************** */
void processMouseEvent( struct libevdev *devMouse, 
                        MouseReport_t *mouseReport, 
                        int *fdMouse, 
                        HidDevice *hidMouse, 
                        ScrollMomentum_t *momentum,
                        ScrollSmoother_t *smoother);

#endif