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
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>

#include "logging.h"
#include "keyboard.h"
#include "keymap.h"

int findKeyboard(void) {
    char event[16];
    int  fd = -1;
    // check for no more than 64 devices
    for (int i = 0; i < 64; i++) {
        snprintf(event, sizeof(event), "event%d", i);
        fd = checkForKeyboard(event);
        // first kexboard wins
        if ( fd >= 0) break;
    }
    return fd;
}

int checkForKeyboard(const char *event) {
    char devname[64];
    int  fd = -1;
        
    snprintf(devname, sizeof(devname), "/dev/input/%s", event);
    fd = open(devname, O_RDONLY | O_NONBLOCK);

    if (fd >= 0) {
        struct libevdev *dev = NULL;

        if (libevdev_new_from_fd(fd, &dev) >= 0) {
            const char *name = libevdev_get_name(dev);
            
            if (!name) name = "unknown";
            
            // check for must have input events for keys: a, <Enter> and <Space>
            if (    libevdev_has_event_code(dev, EV_KEY, KEY_A)        // A
                &&  libevdev_has_event_code(dev, EV_KEY, KEY_ENTER)    // Enter 
                &&  libevdev_has_event_code(dev, EV_KEY, KEY_SPACE)) { // Space

                writeLog(LOG_DEBUG, "Keyboard connected: %s",devname);
            } else {
                // no keyboard device
                close(fd);
                fd = -1;                               
            }
            libevdev_free(dev);
        } 
    }
    return fd;
}

KeyType_t classifyKeyEvent(const struct input_event *ev) {
    KeyType_t result;

    result = KEY_TYPE_INVALID;

    if (ev != NULL) {
        if (ev->type == EV_KEY) {
            switch (ev->code) {
                /* HID modifiers */
                case KEY_LEFTCTRL:
                case KEY_RIGHTCTRL:
                case KEY_LEFTSHIFT:
                case KEY_RIGHTSHIFT:
                case KEY_LEFTALT:
                case KEY_RIGHTALT:
                case KEY_LEFTMETA:
                case KEY_RIGHTMETA:
                    result = KEY_TYPE_MODIFIER;
                    break;

                /* Consumer Control keys */
                case KEY_MUTE:
                case KEY_VOLUMEUP:
                case KEY_VOLUMEDOWN:
                case KEY_PLAY:
                case KEY_PAUSE:
                case KEY_PLAYPAUSE:
                case KEY_STOPCD:
                case KEY_NEXTSONG:
                case KEY_PREVIOUSSONG:
                case KEY_RECORD:
                case KEY_REWIND:
                case KEY_FASTFORWARD:
                case KEY_HOMEPAGE:
                case KEY_BACK:
                case KEY_FORWARD:
                case KEY_REFRESH:
                case KEY_FAVORITES:
                case KEY_EMAIL:
                case KEY_CALC:
                case KEY_COMPUTER:
                case KEY_BRIGHTNESSUP:
                case KEY_BRIGHTNESSDOWN:
                case KEY_SLEEP:
                case KEY_MICMUTE:
                case KEY_SEARCH:
                case KEY_SCALE:
                case KEY_COFFEE:
                    result = KEY_TYPE_CONSUMER_CONTROL;
                    break;

                default:
                    result = KEY_TYPE_REGULAR;
                    break;
            }
        }
    }
    return result;
}

uint8_t updateModifierState(uint8_t modifier, const struct input_event *ev) {
    uint8_t mask = 0;

    switch (ev->code) {

        case KEY_LEFTCTRL:
            mask = 0x01;
            break;

        case KEY_LEFTSHIFT:
            mask = 0x02;
            break;

        case KEY_LEFTALT:
            mask = 0x04;
            break;

        case KEY_LEFTMETA:
            mask = 0x08;
            break;

        case KEY_RIGHTCTRL:
            mask = 0x10;
            break;

        case KEY_RIGHTSHIFT:
            mask = 0x20;
            break;

        case KEY_RIGHTALT:
            mask = 0x40;
            break;

        case KEY_RIGHTMETA:
            mask = 0x80;
            break;
        
        default:
            break;
    }

    if (mask != 0) {
        if (ev->value != 0) {
            modifier |= mask;
        } else {
            modifier &= (uint8_t)~mask; 
        }
    }
    return modifier;
}

bool arrayAdd(uint8_t *array, size_t max, uint8_t value) {
    bool result = false;
    if ((array != NULL) && (value != 0)) {
        for (int i = 0; i < max; i++) {
            if (array[i] == value) {
                result = true;
                break;
            }
            if (array[i] == 0) {
                array[i] = value;
                result = true;
                break;
            }
        }
    }
    return result;
}

bool arrayRemove(uint8_t *array, size_t max, uint8_t value) {
    bool result = false;
    size_t i;

    if (array != NULL && value != 0) {
        for (i = 0; i < max; i++) {
           if (array[i] == value) {
                array[i] = 0;
                for (int j = i; (j + i) < max; j++)
                    array[j] = array[j+1];
                array[max-1] = 0;
                result = true;
            }
        }
    }
    return result;
}