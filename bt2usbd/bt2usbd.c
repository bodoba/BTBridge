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

/* ----------------------------------------------------------------------------------- *
 * Some globals we can't do without... ;)
 * ----------------------------------------------------------------------------------- */
int    debug              = DEBUG;             // debug level
bool   foreground         = false;             // run in foreground, not as daemon

/* ----------------------------------------------------------------------------------- *
 * Prototypes
 * ----------------------------------------------------------------------------------- */
int  main(int rgc, char *argv[]);

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
                writeLog( LOG_DEBUG, "%s (%s)", libevdev_event_code_get_name( ev.type,ev.code), ev.value ? "Pressed" : "Released");
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
