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

#include <sys/inotify.h>
#include <linux/input.h>
#include <libevdev/libevdev.h>

#include "bt2usbd.h"
#include "logging.h"
#include "daemon.h"
#include "keyboard.h"
#include "keymap.h"

/* ----------------------------------------------------------------------------------- *
 * Some globals we can't do without... ;)
 * ----------------------------------------------------------------------------------- */
int    debug              = DEBUG;             // debug level
bool   foreground         = false;             // run in foreground, not as daemon

/* ----------------------------------------------------------------------------------- *
 * Prototypes
 * ----------------------------------------------------------------------------------- */
int  main(int rgc, char *argv[]);


/**
 * Write a keyboard HID report to the USB HID device.
 *
 * @param fd HID gadget file descriptor.
 * @param report Keyboard report to transmit.
 *
 * @return true if the report was transmitted successfully,
 *         false otherwise.
 */
bool writeReport(int fd, const void *report, size_t reportSize)
{
    bool result = false;
    size_t written;

    if ((fd >= 0) && (report != NULL)) {
        written = write(fd, report, reportSize);
        if (written == reportSize) {
            result = true;
        } else {
            switch (errno) {
            case EINTR:
                writeLog(LOG_ERR, "HID write interrupted");
                break;

            case EAGAIN:
                writeLog(LOG_ERR, "HID endpoint busy");
                break;

            case EPIPE:
                writeLog(LOG_ERR, "USB host disconnected (EPIPE)");
                break;

            case ESHUTDOWN:
                writeLog(LOG_WARNING, "USB gadget shutdown");
                break;

            case ENODEV:
                writeLog(LOG_WARNING,"HID device removed");
                break;

            default:
                writeLog(LOG_ERR, "HID write failed: errno=%d", errno);
                break;
            }
        }
    }

    return result;
}

/* ----------------------------------------------------------------------------------- *
 * Main
 * ----------------------------------------------------------------------------------- */
int main( int argc, char *argv[] ) {
    
    // Process command line options --------------------------------------------------
    for (int i=0; i<argc; i++) {
        if (!strcmp(argv[i], "-d")) {          // '-d' turns debug mode on
            debug++;
        }
        if (!strcmp(argv[i], "-f")) {          // '-f' forces forground mode
            foreground=true;
        }
    }
    
    // initialize logging channel -----------------------------------------------------
    initLog(!foreground);
    setLogLevel(LOG_NOTICE+debug);
        
    if (!foreground) {
        // run in background
        daemonize(PID_FILE);
    } else {
        writeLog(LOG_NOTICE, "Running in foreground");
    }
        
//    int  devMouse  = -1;
//    char pathMouse[MAX_PATH_LENGTH];

    // Watch input device list
    int fdNotify = inotify_init1(IN_NONBLOCK);
    inotify_add_watch( fdNotify, "/dev/input", IN_CREATE | IN_DELETE);
    
    // fd set for select() call
    fd_set readfds;

    // 1 sec select timeout
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    bool scanforNewDevice = false;
           
    // Keyboard connected?
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
    int fdHidKbd = open(HID_KEYBOARD, O_WRONLY|O_NONBLOCK);
    if (fdHidKbd < 0) writeLog(LOG_ERR, "Failed to open %s", HID_KEYBOARD);

    // report structures
    KeyReport_t    keyReport;
    initKeyReport(&keyReport);

    ConsumerReport_t    consumerReport;
    initConsumerReport(&consumerReport);

    // Main loop
    for (;;) {
        FD_ZERO(&readfds);
        int maxFd = -1;
        // Watch for keyboard events
        if (fdKbd >= 0) {
            FD_SET(fdKbd, &readfds);
            if (fdKbd > maxFd) maxFd = fdKbd;
        }
        // watch for changes if input devices
        if (fdNotify >= 0) {
            FD_SET(fdNotify, &readfds);
            if (fdNotify > maxFd) maxFd = fdNotify;
        }

        // wait until something happends, but no longer than one second
        tv.tv_sec = 1; 
        int rc = select( maxFd + 1, &readfds, NULL, NULL, &tv);
        
        if ( rc < 0 ) {
            //if (errno == EINTR) continue;
            writeLog(LOG_ERR, "Select Error");
        }

        // Process keyboard events
        if (fdKbd >= 0 && FD_ISSET(fdKbd, &readfds)) {
            // Read all pending events
            struct input_event ev;
            int rcEv;
            while ((rcEv = libevdev_next_event( devKbd, LIBEVDEV_READ_FLAG_NORMAL, &ev)) == LIBEVDEV_READ_STATUS_SUCCESS) {
                // ignore non keyboard events
                if (ev.type != EV_KEY) continue;
                // Ignore auto-repeat
                if (ev.value == 2) continue;

                // process event
                KeyType_t eventClass = classifyKeyEvent(&ev);
                switch (eventClass) {
                    case KEY_TYPE_MODIFIER:
                        keyReport.modifier = updateModifierState(keyReport.modifier, &ev);
                        break;

                    case KEY_TYPE_CONSUMER_CONTROL:
                        linuxKeyToConsumer(&consumerReport, &ev);
                        //writeReport(fdHidKbd, &consumerReport, sizeof(consumerReport));
                        writeLog( LOG_DEBUG, "C 0x%02x 0x%02x                               | %s",
                            consumerReport.keys[0], consumerReport.keys[1],
                            libevdev_event_code_get_name( ev.type,ev.code)
                        );
                        break;

                    case KEY_TYPE_REGULAR:
                        if(ev.value != 0) { // Key press
                            arrayAdd(keyReport.keys, 6, linuxKeyToHid(&ev));
                        } else {           // key release
                            arrayRemove(keyReport.keys, 6, linuxKeyToHid(&ev));
                        }
                        writeReport(fdHidKbd, &keyReport, sizeof(keyReport));
                        writeLog( LOG_DEBUG, "K 0x%02x 0x00 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x | %s",
                            keyReport.modifier,
                            keyReport.keys[0], keyReport.keys[1], keyReport.keys[2],
                            keyReport.keys[3], keyReport.keys[4], keyReport.keys[5],
                            libevdev_event_code_get_name( ev.type,ev.code)
                        );                        
                        
                        break;

                    case KEY_TYPE_INVALID:
                    default:
                        break;
                }
            }

            if (rcEv == -ENODEV) {
                writeLog(LOG_INFO, "Keyboard disconnected");
                libevdev_free(devKbd);
                devKbd = NULL;
                close(fdKbd);
                fdKbd = -1;
           }
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
                    }
                }
            }
        }
    }
    return 0;
}
