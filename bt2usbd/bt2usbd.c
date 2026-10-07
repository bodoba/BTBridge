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

#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>

#include <sys/inotify.h>
#include <linux/input.h>
#include <linux/hidraw.h>
#include <libevdev/libevdev.h>

#include "bt2usbd.h"
#include "logging.h"
#include "daemon.h"
#include "hidinterface.h"
#include "keyboard.h"
#include "keymap.h"
#include "mouse.h"

/* ----------------------------------------------------------------------------------- *
 * Some globals we can't do without... ;)
 * ----------------------------------------------------------------------------------- */
int    debug              = DEBUG;             // debug level
bool   foreground         = false;             // run in foreground, not as daemon

/* ----------------------------------------------------------------------------------- *
 * Main
 * ----------------------------------------------------------------------------------- */
int main( int argc, char *argv[] ) {
    
    /* *********************************************************************************** *
     * Process command line arguments
     * *********************************************************************************** */ 
    for (int i=0; i<argc; i++) {
        if (!strcmp(argv[i], "-d")) {          // '-d' turns debug mode on
            debug++;
        }
        if (!strcmp(argv[i], "-f")) {          // '-f' forces forground mode
            foreground=true;
        }
    }
    
    /* *********************************************************************************** *
     * Initialize logging
     * *********************************************************************************** */ 
    initLog(!foreground);
    setLogLevel(LOG_NOTICE+debug);
        
    if (!foreground) {
        // run in background
        daemonize(PID_FILE);
    } else {
        writeLog(LOG_NOTICE, "Running in foreground");
    }

    /* *********************************************************************************** *
     * Setup select()
     * *********************************************************************************** */ 
    int fdNotify = inotify_init1(IN_NONBLOCK);
    inotify_add_watch( fdNotify, "/dev/input", IN_CREATE | IN_DELETE);
    
    // fd set for select() call
    fd_set readfds;

    // 1 sec select timeout
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    bool scanforNewDevice = false;
           
    /* *********************************************************************************** *
     * Initialize Bluetooth Keyboard
     * *********************************************************************************** */ 
    int  fdKbd              = -1;
    struct libevdev *devKbd = NULL;
    fdKbd = findKeyboard();
    if (fdKbd >= 0) {
        if (libevdev_new_from_fd(fdKbd, &devKbd) < 0) {
            writeLog(LOG_ERR, "Failed to create libevdev instance");
            close(fdKbd);
            fdKbd = -1;
        }
    }

    // HID Device for Keyboard events 
    HidDevice *hidKbd = initHidDevice(HID_KEYBOARD);
    if (hidKbd == NULL) {
        writeLog(LOG_ERR, "Failed to initialize HID device for keyboard");
    }

     // HID Device for consumer control events 
    HidDevice *hidCon = initHidDevice(HID_CONSUMER);
    if (hidCon == NULL) {
        writeLog(LOG_ERR, "Failed to initialize HID device for consumer control");
    }

    // report structures
    KeyReport_t    keyReport;
    initKeyReport(&keyReport);

    ConsumerReport_t    consumerReport;
    initConsumerReport(&consumerReport);

    /**< Zusätzlicher State für Toggle-Keys */
    bool capsLockActive = false;
    bool numLockActive  = false;
    
    if (devKbd != NULL) {
        if (readKeyboardLedState(fdKbd, &capsLockActive, &numLockActive)) {
            writeLog(LOG_INFO, "Resynced keyboard LED state: Caps=%s Num=%s",
                    capsLockActive ? "ON" : "OFF",
                    numLockActive  ? "ON" : "OFF");
            syncCapsLockWithHost(hidKbd, capsLockActive);
        }
    }

    /* *********************************************************************************** *
     * Setup Bluetooth Mouse
     * *********************************************************************************** */ 
    int  fdMouse              = -1;
    struct libevdev *devMouse = NULL;
    fdMouse = findMouse();
    if (fdMouse >= 0) {
        if (libevdev_new_from_fd(fdMouse, &devMouse) < 0) {
            writeLog(LOG_ERR, "Failed to create libevdev instance for mouse");
            close(fdMouse);
            fdMouse = -1;
        }
    }

    // HID Device for mouse events
    HidDevice *hidMouse = initHidDevice(HID_MOUSE);
    if (hidMouse == NULL) {
        writeLog(LOG_ERR, "Failed to initialize HID device for mouse");
    }

    MouseReport_t mouseReport;
    initMouseReport(&mouseReport);

    /* *********************************************************************************** *
     * From her to eternity... ;)
     * *********************************************************************************** */ 
    for (;;) {
        FD_ZERO(&readfds);
        int maxFd = -1;

        // Watch for keyboard events
        if (fdKbd >= 0) {
            FD_SET(fdKbd, &readfds);
            if (fdKbd > maxFd) maxFd = fdKbd;
        }
        
        // watch for changes of input devices
        if (fdNotify >= 0) {
            FD_SET(fdNotify, &readfds);
            if (fdNotify > maxFd) maxFd = fdNotify;
        }

        // Check keyboard HID device (for LED-Reports by host)
        if (hidKbd != NULL && hidKbd->fd >= 0) {
            FD_SET(hidKbd->fd, &readfds);
            if (hidKbd->fd > maxFd) maxFd = hidKbd->fd;
        }

        // Check mouse input device
        if (fdMouse >= 0) {
            FD_SET(fdMouse, &readfds);
            if (fdMouse > maxFd) maxFd = fdMouse;
        }

        // wait until something happends, but no longer than one second
        tv.tv_sec  = 1; 
        tv.tv_usec = 0;
        int rc = select( maxFd + 1, &readfds, NULL, NULL, &tv);
        
        if ( rc < 0 ) {
            if (errno != EINTR) {
                writeLog(LOG_ERR, "Select Error: %s (errno=%d)", strerror(errno), errno);
            } 
            continue;
        }

        // Process LED reports from HID device (Host sendet Caps Lock Status)
        if (hidKbd != NULL && hidKbd->fd >= 0 ) {
            processHidLedReport(hidKbd, &capsLockActive, &numLockActive);
        }

        // Process keyboard events
        if (fdKbd >= 0 && FD_ISSET(fdKbd, &readfds)) {
           processKeyboardEvent(devKbd, &keyReport, &consumerReport, &fdKbd, hidKbd, hidCon);
        }

        // Process mouse events
        if (fdMouse >= 0 && FD_ISSET(fdMouse, &readfds)) {
            processMouseEvent(devMouse, &mouseReport, &fdMouse, hidMouse);
        }

        // changes of input device list?
        if (fdNotify >= 0 && FD_ISSET(fdNotify, &readfds)) {
            writeLog(LOG_INFO, "Input device list changed");
            char buf[MAX_PATH_LENGTH];
            size_t len = read(fdNotify, buf, sizeof(buf));
            if (len>0) {
                // iterate throgh events
                char *ptr = buf;
                while (ptr < buf+len) {
                    struct inotify_event *event = (struct inotify_event *)ptr;
                    if (event->mask & IN_CREATE) {
                        writeLog(LOG_DEBUG, "Created: %s", event->name);
                        scanforNewDevice = true;
                    }
                    if (event->mask & IN_DELETE) {
                        writeLog(LOG_DEBUG, "Deleted: %s", event->name);
                    }
                    ptr += sizeof(struct inotify_event)+event->len;
                }
            }
        }

        if ( scanforNewDevice ) {
            // Keyboard added?
            if (fdKbd < 0) {
                fdKbd = findKeyboard();
                if (fdKbd >= 0) {
                    if (libevdev_new_from_fd(fdKbd, &devKbd) < 0) {
                        writeLog(LOG_ERR, "Failed to create libevdev instance");
                        close(fdKbd);
                        fdKbd = -1;
                    } else {
                        scanforNewDevice = false;

                        /* Drain any events already queued by the kernel before the
                        * device was fully set up (e.g. the very key press that
                        * triggered the BT reconnect). This avoids reading a stale
                        * or about-to-change LED state.
                        */
                        struct input_event drainEv;
 
                        while (libevdev_next_event(devKbd, LIBEVDEV_READ_FLAG_NORMAL, &drainEv) == LIBEVDEV_READ_STATUS_SUCCESS) {
                            /* discard - handled on next main loop iteration anyway
                            * is not possible since libevdev already consumed it;
                            * so we re-inject relevant key events manually here if needed.
                            * In practice, simply logging is sufficient for diagnosis.
                            */
                            writeLog(LOG_DEBUG, "Drained pending event during reconnect: type=%d code=%d value=%d",
                                    drainEv.type, drainEv.code, drainEv.value);
                        }

                        if (readKeyboardLedState(fdKbd, &capsLockActive, &numLockActive)) {
                            writeLog(LOG_INFO, "Resynced keyboard LED state: Caps=%s Num=%s",
                                    capsLockActive ? "ON" : "OFF",
                                    numLockActive  ? "ON" : "OFF");
                            syncCapsLockWithHost(hidKbd, capsLockActive);
                        }
                    }
                }
            }
      
            // Mouse added?
            if (fdMouse < 0) {
                fdMouse = findMouse();
                if (fdMouse >= 0) {
                    if (libevdev_new_from_fd(fdMouse, &devMouse) < 0) {
                        writeLog(LOG_ERR, "Failed to create libevdev instance for mouse");
                        close(fdMouse);
                        fdMouse = -1;
                    }
                }
            }
        }
    }
    return 0;
}