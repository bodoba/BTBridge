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
#include <fcntl.h>

#include "logging.h"
#include "keyboard.h"

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