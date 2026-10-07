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

#include "logging.h"
#include "mouse.h"

int findMouse(void)
{
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

int checkForMouse(const char *event)
{
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

uint8_t updateMouseButtonState(uint8_t buttons, const struct input_event *ev)
{
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

int8_t clampMouseMovement(int value)
{
    if (value > 127) return 127;
    if (value < -127) return -127;
    return (int8_t)value;
}

void processMouseEvent(struct libevdev *devMouse, MouseReport_t *mouseReport, int *fdMouse, HidDevice *hidMouse) {
    struct input_event ev;
    int rcEv;
    bool reportDirty = false;

    while ((rcEv = libevdev_next_event(devMouse, LIBEVDEV_READ_FLAG_NORMAL, &ev)) == LIBEVDEV_READ_STATUS_SUCCESS) {

        switch (ev.type) {
        case EV_KEY:
            mouseReport->buttons = updateMouseButtonState(mouseReport->buttons, &ev);
            reportDirty = true;
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

            case REL_WHEEL:
                mouseReport->wheel = clampMouseMovement(ev.value);
                reportDirty = true;
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