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
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <string.h>

#include "logging.h"
#include "hidinterface.h"


/* *********************************************************************************** *
 * @brief Findet das hidraw-Device für ein gegebenes hidg-Device
 * 
 * Der hidg-Device ist nur für Input, das hidraw-Device für Input+Output.
 * Beispiel: /dev/hidg0 -> /dev/hidraw0
 * 
 * @param[in] hidgPath Pfad zum hidg-Device (z.B. "/dev/hidg0")
 * 
 * @return Pfad zum entsprechenden hidraw-Device, oder NULL
 * *********************************************************************************** */
char* findHidrawDevice(const char *hidgPath)
{
    if (hidgPath == NULL) {
        return NULL;
    }

    /**< Extrahiere die Nummer aus hidg-Path */
    int hidgNum = -1;
    if (sscanf(hidgPath, "/dev/hidg%d", &hidgNum) != 1) {
        writeLog(LOG_ERR, "Invalid hidg path: %s", hidgPath);
        return NULL;
    }

    /**< Konstruiere hidraw-Path */
    static char hidrawPath[256];
    snprintf(hidrawPath, sizeof(hidrawPath), "/dev/hidraw%d", hidgNum);

    /**< Überprüfe, ob Device existiert */
    if (access(hidrawPath, F_OK) == 0) {
        writeLog(LOG_DEBUG, "Found hidraw device: %s", hidrawPath);
        return hidrawPath;
    }

    writeLog(LOG_WARNING, "hidraw device not found: %s", hidrawPath);
    return NULL;
}

/* *********************************************************************************** *
 * @brief Opens an HID device
 * 
 * This function opens the HID device in write-only mode with
 * non-blocking access. The device is opened with O_WRONLY and O_NONBLOCK
 * to avoid blocking.
 * 
 * @param[in] devicePath Path to the HID device (e.g. "/dev/hidg0")
 * 
 * @return File descriptor (>= 0) on success,
 *         -1 on error (errno is set)
 * 
 * @note The caller is responsible for closing with closeHidDevice()
 * @note The device is opened in non-blocking mode (O_NONBLOCK)
 * @note On error, errno is set and can be interpreted with strerror()
 * 
 * @see closeHidDevice
 * @see strerror
 * *********************************************************************************** */
int openHidDevice(const char *devicePath)
{
    if (devicePath == NULL) {
        writeLog(LOG_ERR, "Device path is NULL");
        errno = EINVAL;
        return -1;
    }

    int fd = open(devicePath, O_WRONLY | O_NONBLOCK);
    
    if (fd < 0) {
        int err = errno;
        writeLog(LOG_ERR, "Failed to open HID device '%s': %s (errno=%d)", 
                 devicePath, strerror(err), err);
        return -1;
    }

    writeLog(LOG_INFO, "HID device opened successfully: %s (fd=%d)", devicePath, fd);
    return fd;
}

/* *********************************************************************************** *
 * @brief Öffnet das hidraw-Device für Output Reports
 * 
 * @param[in] hidrawPath Pfad zum hidraw-Device (z.B. "/dev/hidraw0")
 * 
 * @return File-Deskriptor (>= 0) bei Erfolg, -1 bei Fehler
 * *********************************************************************************** */
int openHidrawDevice(const char *hidrawPath)
{
    if (hidrawPath == NULL) {
        writeLog(LOG_DEBUG, "No hidraw device specified");
        return -1;
    }

    int fd = open(hidrawPath, O_RDWR | O_NONBLOCK);
    
    if (fd < 0) {
        int err = errno;
        writeLog(LOG_WARNING, "Failed to open hidraw device '%s': %s (errno=%d)", 
                 hidrawPath, strerror(err), err);
        return -1;
    }

    writeLog(LOG_INFO, "Hidraw device opened successfully: %s (fd=%d)", hidrawPath, fd);
    return fd;
}

/* *********************************************************************************** *
 * @brief Closes an HID device
 * 
 * This function closes the file descriptor of the HID device.
 * It is safe to call with invalid descriptors.
 * 
 * @param[in] fd File descriptor of the HID device
 * 
 * @return 0 on success,
 *         -1 on error (errno is set)
 * 
 * @note Safe to call with fd < 0
 * @note After the call, the descriptor is invalid
 * @note On error, errno is set
 * 
 * @see openHidDevice
 * *********************************************************************************** */
int closeHidDevice(int fd)
{
    if (fd < 0) {
        return 0;  /**< Already closed or invalid */
    }

    if (close(fd) < 0) {
        int err = errno;
        writeLog(LOG_ERR, "Failed to close HID device (fd=%d): %s (errno=%d)", 
                 fd, strerror(err), err);
        return -1;
    }

    writeLog(LOG_INFO, "HID device closed successfully (fd=%d)", fd);
    return 0;
}

/* *********************************************************************************** *
 * @brief Attempts to restore the HID device
 * 
 * This function closes the old device handle and then tries to reopen
 * the HID device using an exponential backoff strategy.
 * On a successful reconnection, the error counter is reset.
 * 
 * @param[in,out] device Pointer to the HidDevice structure
 *                       The fd field is updated on success
 * 
 * @return true if the reconnect was successful, false otherwise
 * 
 * @note Uses exponential backoff: 500ms, 1s, 2s, 4s, 8s
 * @note Maximum HID_RECONNECT_RETRY_COUNT attempts
 * 
 * @see HID_RECONNECT_RETRY_COUNT
 * @see HID_RECONNECT_DELAY_MS
 * *********************************************************************************** */
static bool reconnectHidDevice(HidDevice *device)
{
    if (device == NULL || device->devicePath == NULL) {
        return false;
    }

    writeLog(LOG_INFO, "Attempting HID reconnect...");
    
    // Close the old device
    if (device->fd_input >= 0) {
        closeHidDevice(device->fd_input);
        device->fd_input = -1;
    }

    if (device->fd_output >= 0) {
        closeHidDevice(device->fd_output);
        device->fd_output = -1;
    }

    // Retry using an exponential backoff strategy
    for (int attempt = 0; attempt < HID_RECONNECT_RETRY_COUNT; attempt++) {
        usleep(HID_RECONNECT_DELAY_MS * 1000 * (1 << attempt)); // 500ms, 1s, 2s, 4s, 8s
        
        /**< Öffne Input Device (hidg) */
        device->fd_input = openHidDevice(device->devicePath);
        if (device->fd_input < 0) {
            writeLog(LOG_ERR, "Failed to open HID device: %s", device->devicePath);
            free((void *)device->devicePath);
            free(device);
            return NULL;
        } else {
            /**< Finde und öffne Output Device (hidraw) */
            char *hidrawPath = findHidrawDevice(device->devicePath);
            if (hidrawPath != NULL) {
                device->hidrawPath = strdup(hidrawPath);
                device->fd_output = openHidrawDevice(hidrawPath);
                if (device->fd_output < 0) {
                    writeLog(LOG_WARNING, "Could not open hidraw device for LED reports");
                }
            } else {
                device->hidrawPath = NULL;
                device->fd_output = -1;
            }
            writeLog(LOG_INFO, "Reconnect successful after %d attempts", attempt);
            return true;  /**< Reconnect successful */
        }
    }
    writeLog(LOG_ERR, "HID reconnect failed after %d attempts", HID_RECONNECT_RETRY_COUNT);
    return false;
}

/* *********************************************************************************** */
/*                       P U B L I C  I N T E R F A C E                                */
/* *********************************************************************************** */

HidDevice* initHidDevice(const char *devicePath)
{
    if (devicePath == NULL) {
        writeLog(LOG_ERR, "Device path is NULL");
        return NULL;
    }
    
    HidDevice *device = malloc(sizeof(HidDevice));
    
    if (device == NULL) {
        return NULL;
    }
    
    device->devicePath = strdup(devicePath);
    if (device->devicePath == NULL) {
        writeLog(LOG_ERR, "Failed to allocate memory for device path");
        free(device);
        return NULL;
    }

    /**< Öffne Input Device (hidg) */
    device->fd_input = openHidDevice(devicePath);
    if (device->fd_input < 0) {
        writeLog(LOG_ERR, "Failed to open HID device: %s", devicePath);
        free((void *)device->devicePath);
        free(device);
        return NULL;
    }

    /**< Finde und öffne Output Device (hidraw) */
    char *hidrawPath = findHidrawDevice(devicePath);
    if (hidrawPath != NULL) {
        device->hidrawPath = strdup(hidrawPath);
        device->fd_output = openHidrawDevice(hidrawPath);
        if (device->fd_output < 0) {
            writeLog(LOG_WARNING, "Could not open hidraw device for LED reports");
        }
    } else {
        device->hidrawPath = NULL;
        device->fd_output = -1;
    }

    device->lastErrorTime = 0;
    device->consecutiveErrors = 0;

    writeLog(LOG_INFO, "HidDevice initialized successfully");
    return device;
}

bool writeHidReport(HidDevice *device, const void *report, size_t reportSize)
{
    if (device == NULL || device->fd_input < 0 || report == NULL) {
        writeLog(LOG_ERR, "Invalid parameters for HID write");
        return false;
    }

    const int MAX_RETRIES = 5;              /**< Maximum retries for temporary errors */
    const int RETRY_DELAY_US = 10000;       /**< Initial delay: 10ms */
    int retry_count = 0;
    int retry_delay = RETRY_DELAY_US;

    while (retry_count < MAX_RETRIES) {
        ssize_t written = write(device->fd_input, report, reportSize);

        if ((size_t)written == reportSize) {
            /**< Successful write */
            device->consecutiveErrors = 0;
            if (retry_count > 0) {
                writeLog(LOG_DEBUG, "HID write succeeded after %d retry/retries", retry_count);
            }
            return true;
        }

        // Error handling
        int err = errno;
        device->consecutiveErrors++;
        retry_count++;

        switch (err) {
        case EINTR:
            /**< The write operation was interrupted by a signal - retry is appropriate */
            writeLog(LOG_DEBUG, "HID write interrupted, retry %d/%d", retry_count, MAX_RETRIES);
            
            if (retry_count < MAX_RETRIES) {
                usleep(retry_delay);
                retry_delay *= 2;  /**< Exponential backoff: 10ms, 20ms, 40ms, 80ms, 160ms */
                continue;
            }
            
            writeLog(LOG_ERR, "HID write interrupted - max retries exceeded");
            return false;

        case EAGAIN:
            /**< The HID endpoint is temporarily unavailable - retry is appropriate */
            writeLog(LOG_DEBUG, "HID endpoint busy, retry %d/%d", retry_count, MAX_RETRIES);
            
            if (retry_count < MAX_RETRIES) {
                usleep(retry_delay);
                retry_delay *= 2;  /**< Exponential backoff */
                continue;
            }
            
            writeLog(LOG_WARNING, "HID endpoint busy - max retries exceeded");
            return false;

        case EPIPE:
            /**< The USB host disconnected - reconnect required */
            writeLog(LOG_ERR, "USB host disconnected (EPIPE)");
            
            if (reconnectHidDevice(device)) {
                /**< Reconnect successful - one retry attempt after reconnect */
                writeLog(LOG_INFO, "Retrying HID write after successful reconnect");
                ssize_t retry_written = write(device->fd_input, report, reportSize);
                
                if ((size_t)retry_written == reportSize) {
                    device->consecutiveErrors = 0;
                    return true;
                }
                
                writeLog(LOG_ERR, "HID write failed after reconnect");
            }
            return false;

        case ESHUTDOWN:
            /**< The USB gadget was shut down - reconnect required */
            writeLog(LOG_WARNING, "USB gadget shutdown");
            
            if (reconnectHidDevice(device)) {
                writeLog(LOG_INFO, "Retrying HID write after gadget restart");
                ssize_t retry_written = write(device->fd_input, report, reportSize);
                
                if ((size_t)retry_written == reportSize) {
                    device->consecutiveErrors = 0;
                    return true;
                }
            }
            return false;

        case ENODEV:
            /**< The HID device was removed - reconnect required */
            writeLog(LOG_WARNING, "HID device removed");
            
            if (reconnectHidDevice(device)) {
                writeLog(LOG_INFO, "Retrying HID write after device reconnection");
                ssize_t retry_written = write(device->fd_input, report, reportSize);
                
                if ((size_t)retry_written == reportSize) {
                    device->consecutiveErrors = 0;
                    return true;
                }
            }
            return false;

        default:
            /**< Unknown error */
            writeLog(LOG_ERR, "HID write failed: errno=%d (%s), consecutive_errors=%d, retry=%d/%d", 
                     err, strerror(err), device->consecutiveErrors, retry_count, MAX_RETRIES);
            
            /**< Reconnect if there are too many consecutive errors */
            if (device->consecutiveErrors >= 3) {
                writeLog(LOG_WARNING, "Too many consecutive errors, attempting reconnect");
                reconnectHidDevice(device);
            }
            
            return false;
        }
    }

    writeLog(LOG_ERR, "HID write failed - max retries exceeded");
    return false;
}

void cleanupHidDevice(HidDevice *device)
{
    if (device != NULL) {
        if (device->fd_input >= 0) {
            closeHidDevice(device->fd_input);
        }
        if (device->fd_output >= 0) {
            closeHidDevice(device->fd_output);
        }
        free((void *)device->devicePath);
        free((void *)device->hidrawPath);
        free(device);
    }
}
