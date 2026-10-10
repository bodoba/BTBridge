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
#include <libevdev/libevdev.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <time.h>

#include "logging.h"
#include "mouse.h"

float mouseSensitivity = MOUSE_SENSITIVITY;  /* Default sensitivity multiplier */

static long elapsedMsSince(const struct timespec *then, const struct timespec *now) {
    return (now->tv_sec - then->tv_sec) * 1000 +
           (now->tv_nsec - then->tv_nsec) / 1000000;
}

int scrollSmootherFeed(ScrollSmoother_t *s, int hiResDelta) {
    int lines;

    s->accum += (float)hiResDelta / WHEEL_HIRES_UNITS_PER_LINE;

    lines = (int)s->accum; /* truncates toward zero, keeps remainder */
    s->accum -= (float)lines;

    return lines;
}

void feedScrollMomentum(ScrollMomentum_t *m, int hiResDelta) {
    struct timespec now;
    long dtMs;
    float instVelocity;
    float ticksElapsed;

    clock_gettime(CLOCK_MONOTONIC, &now);

    m->active = false;

    dtMs = (m->lastEvent.tv_sec == 0) ? SCROLL_MOMENTUM_TICK_MS
                                       : elapsedMsSince(&m->lastEvent, &now);
    if (dtMs <= 0) dtMs = 1;

    ticksElapsed = (float)dtMs / (float)SCROLL_MOMENTUM_TICK_MS;
    instVelocity = ((float)hiResDelta / WHEEL_HIRES_UNITS_PER_LINE) / ticksElapsed;

    /* shift history and store the raw instantaneous velocity, unfiltered;
     * this is used by checkScrollRelease() to reject a single trailing
     * spike (e.g. the Magic Mouse's own release-detection "flick") that
     * would otherwise be picked up by the EMA below */
    for (int i = SCROLL_VELOCITY_HISTORY_SIZE - 1; i > 0; i--) {
        m->velocityHistory[i] = m->velocityHistory[i - 1];
    }
    m->velocityHistory[0] = instVelocity;
    if (m->velocityHistoryCount < SCROLL_VELOCITY_HISTORY_SIZE) {
        m->velocityHistoryCount++;
    }

    /* EMA still used for live display/logging purposes only, no longer
     * used directly as the coasting start velocity */
    m->velocity = (SCROLL_MOMENTUM_VELOCITY_SMOOTHING * instVelocity) +
                  ((1.0f - SCROLL_MOMENTUM_VELOCITY_SMOOTHING) * m->velocity);

    m->lastEvent = now;
}

void updateScrollMomentum(ScrollMomentum_t *m, MouseReport_t *mouseReport, HidDevice *hidMouse) {
    struct timespec now;
    long elapsedMs;
    int lines;

    checkScrollRelease(m);

    if (!m->active) return;

    clock_gettime(CLOCK_MONOTONIC, &now);
    elapsedMs = elapsedMsSince(&m->lastTick, &now);

    if (elapsedMs < SCROLL_MOMENTUM_TICK_MS) return;

    m->velocity *= SCROLL_MOMENTUM_DECAY;

    if (fabsf(m->velocity) < SCROLL_MOMENTUM_MIN_VELOCITY) {
        m->active    = false;
        m->velocity  = 0.0f;
        m->lineAccum = 0.0f;
        return;
    }

    /* accumulate fractional velocity instead of truncating it directly;
     * this lets small velocities (< 1.0) still emit a whole line every
     * few ticks instead of producing wheel=0 as soon as velocity drops
     * below 1.0 - this is what makes the tail-off feel smooth. */
    m->lineAccum += m->velocity;
    lines = (int)m->lineAccum;
    m->lineAccum -= (float)lines;

    if (lines != 0) {
        mouseReport->wheel = clampMouseMovement(lines);
        writeHidReport(hidMouse, mouseReport, sizeof(*mouseReport));
        writeLog(LOG_DEBUG, "M (momentum) wheel=%d v=%.2f accum=%.2f",
                 mouseReport->wheel, m->velocity, m->lineAccum);
        mouseReport->wheel = 0;
    }

    m->lastTick = now;
}

static void startCoastingFromHistory(ScrollMomentum_t *m, struct timespec now) {
    float sum = 0.0f;
    int i;
    float releaseVelocity;

    m->lastEvent.tv_sec = 0;

    if (m->velocityHistoryCount == 0) {
        m->velocity = 0.0f;
        return;
    }

    for (i = 0; i < m->velocityHistoryCount; i++) {
        sum += m->velocityHistory[i];
    }
    releaseVelocity = sum / (float)m->velocityHistoryCount;
    m->velocityHistoryCount = 0;

    if (fabsf(releaseVelocity) < SCROLL_MOMENTUM_MIN_VELOCITY) {
        m->velocity = 0.0f;
        return;
    }

    m->velocity  = releaseVelocity;
    m->lineAccum = 0.0f;
    m->active    = true;
    m->lastTick  = now;

    writeLog(LOG_DEBUG, "Scroll momentum started: v=%.2f (finger lifted)", m->velocity);
}

void notifyScrollFingerLifted(ScrollMomentum_t *m) {
    struct timespec now;

    if (m->active) return; /* already coasting */

    clock_gettime(CLOCK_MONOTONIC, &now);
    startCoastingFromHistory(m, now);
}

void checkScrollRelease(ScrollMomentum_t *m) {
    struct timespec now;

    if (m->active || m->lastEvent.tv_sec == 0) return;

    clock_gettime(CLOCK_MONOTONIC, &now);
    if (elapsedMsSince(&m->lastEvent, &now) < SCROLL_MOMENTUM_RELEASE_MS) return;

    if (m->velocityHistoryCount < SCROLL_VELOCITY_HISTORY_SIZE) {
        m->lastEvent.tv_sec = 0;
        m->velocityHistoryCount = 0;
        m->velocity = 0.0f;
        return;
    }

    /* fallback only - normally notifyScrollFingerLifted() already
     * triggered the release via ABS_MT_TRACKING_ID before this
     * timing-based fallback would ever fire */
    startCoastingFromHistory(m, now);
}

int findMouse(void) {
    char event[16];
    int fd = -1;

    /* check for no more than 64 devices */
    for (int i = 0; i < 64; i++) {
        snprintf(event, sizeof(event), "event%d", i);
        fd = checkForMouse(event);
        if (fd >= 0) break; /* first mouse wins */
    }
    return fd;
}

int checkForMouse(const char *event) {
    char devname[64];
    int  fd = -1;

    snprintf(devname, sizeof(devname), "/dev/input/%s", event);
    fd = open(devname, O_RDONLY | O_NONBLOCK);

    if (fd >= 0) {
        struct libevdev *dev = NULL;

        if (libevdev_new_from_fd(fd, &dev) >= 0) {
            const char *name = libevdev_get_name(dev);

            if (!name) name = "unknown";

            /* a mouse must report relative X/Y movement and at least the left button */
            if (libevdev_has_event_code(dev, EV_REL, REL_X) &&
                libevdev_has_event_code(dev, EV_REL, REL_Y) &&
                libevdev_has_event_code(dev, EV_KEY, BTN_LEFT)) {

                writeLog(LOG_DEBUG, "Mouse connected: %s (%s)", devname, name);
            } else {
                close(fd);
                fd = -1;
            }
            libevdev_free(dev);
        }
    }
    return fd;
}

uint8_t updateMouseButtonState(uint8_t buttons, const struct input_event *ev) {
    uint8_t mask = 0;

    switch (ev->code) {
    case BTN_LEFT:
        mask = 0x01;
        break;

    case BTN_RIGHT:
        mask = 0x02;
        break;

    case BTN_MIDDLE:
        mask = 0x04;
        break;

    default:
        break;
    }

    if (mask != 0) {
        if (ev->value != 0) {
            buttons |= mask;
        } else {
            buttons &= (uint8_t)~mask;
        }
    }
    return buttons;
}

int8_t clampMouseMovement(int value) {
    float scaled = (float)value * mouseSensitivity;  /* Apply sensitivity multiplier */

    /* round to nearest instead of truncating, important for values < 1.0 */
    int rounded = (int)(scaled >= 0 ? scaled + 0.5f : scaled - 0.5f);

    if (rounded > 127) return 127;
    if (rounded < -127) return -127;
    return (int8_t)rounded;
}

void processMouseEvent( struct libevdev *devMouse, 
                        MouseReport_t *mouseReport, 
                        int *fdMouse, 
                        HidDevice *hidMouse, 
                        ScrollMomentum_t *momentum,
                        ScrollSmoother_t *smoother) {
    struct input_event ev;
    int rcEv;
    bool reportDirty = false;

    while ((rcEv = libevdev_next_event(devMouse, LIBEVDEV_READ_FLAG_NORMAL, &ev)) == LIBEVDEV_READ_STATUS_SUCCESS) {

        switch (ev.type) {
        case EV_KEY:
            mouseReport->buttons = updateMouseButtonState(mouseReport->buttons, &ev);
            reportDirty = true;
            break;

        case EV_ABS:
            if (ev.code == ABS_MT_TRACKING_ID && ev.value == -1) {
                /* authoritative release signal straight from the hardware -
                * far more reliable than inferring release from gaps in
                * wheel event timing */
                notifyScrollFingerLifted(momentum);
            }
            break;

        case EV_REL:
            switch (ev.code) {
                case REL_X:
                    mouseReport->x = clampMouseMovement(ev.value);
                    reportDirty = true;
                    break;

                case REL_Y:
                    mouseReport->y = clampMouseMovement(ev.value);
                    reportDirty = true;
                    break;

                case REL_WHEEL_HI_RES: {
                        int lines = scrollSmootherFeed(smoother, ev.value);

                        /* feed the raw hi-res delta for velocity tracking (regular
                        * sampling interval), independent of when whole lines are
                        * actually emitted to the host */
                        feedScrollMomentum(momentum, ev.value);

                        if (lines != 0) {
                            mouseReport->wheel = clampMouseMovement(lines);
                            reportDirty = true;
                        }
                    }
                    break;

                case REL_WHEEL:
                    /* Intentionally ignored: REL_WHEEL and REL_WHEEL_HI_RES are sent
                     * together for the same physical movement. Only HI_RES is used
                     * to avoid double-counting the scroll distance. */
                    break;

                default:
                    break;
                }
            break;

        case EV_SYN:
            /* SYN_REPORT marks the end of one input packet - send it now */
            if (ev.code == SYN_REPORT && reportDirty) {
                writeHidReport(hidMouse, mouseReport, sizeof(*mouseReport));
                writeLog(LOG_DEBUG, "M btn=0x%02x x=%d y=%d wheel=%d",
                        mouseReport->buttons, mouseReport->x, mouseReport->y, mouseReport->wheel);

                /* relative movement must be reset after sending, buttons stay persistent */
                mouseReport->x = 0;
                mouseReport->y = 0;
                mouseReport->wheel = 0;
                reportDirty = false;
            }
            break;

        default:
            break;
        }
    }

    if (rcEv == -ENODEV) {
        writeLog(LOG_INFO, "Mouse disconnected");
        libevdev_free(devMouse);
        devMouse = NULL;
        close(*fdMouse);
        *fdMouse = -1;
    }
}