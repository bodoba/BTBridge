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
#ifndef hidinterface_h
#define hidinterface_h

#include <stdbool.h>

#define HID_RECONNECT_RETRY_COUNT     5
#define HID_RECONNECT_DELAY_MS      500

/* *********************************************************************************** *
 * @struct HidDevice
 * @brief Struktur zur Verwaltung eines virtuellen HID-Geräts
 * 
 * Diese Struktur kapselt alle notwendigen Informationen für die
 * Kommunikation mit einem HID-Gerät über USB-OTG.
 * *********************************************************************************** */
typedef struct {
    int fd;                          /**< File-Deskriptor des HID-Geräts */
    const char *devicePath;          /**< Pfad zum HID-Gerät (z.B. /dev/hidg0) */
    time_t lastErrorTime;            /**< Zeitstempel des letzten Fehlers */
    int consecutiveErrors;           /**< Anzahl aufeinanderfolgender Fehler */
} HidDevice;

/* *********************************************************************************** *
 * @brief Initialisiert ein HID-Gerät
 * 
 * Diese Funktion erstellt eine neue HidDevice-Struktur, speichert den
 * Gerätepfad und öffnet das HID-Gerät. Bei Fehlern wird die Struktur
 * freigegeben und NULL zurückgegeben.
 * 
 * @param[in] devicePath Pfad zum HID-Gerät (z.B. "/dev/hidg0")
 * 
 * @return Zeiger auf die neu erstellte HidDevice-Struktur bei Erfolg,
 *         NULL bei Fehler (Speicher oder Gerät konnte nicht geöffnet werden)
 * 
 * @note Der Aufrufer ist verantwortlich für die Freigabe mit cleanupHidDevice()
 * @note Die Struktur wird mit consecutiveErrors = 0 und lastErrorTime = 0 initialisiert
 * 
 * @see cleanupHidDevice
 * @see openHidDevice
 * *********************************************************************************** */
HidDevice* initHidDevice(const char *devicePath);

/* *********************************************************************************** *
 * @brief Schreibt einen HID-Report mit robuster Fehlerbehandlung, Reconnect und Retry
 * 
 * Diese Funktion versucht, einen HID-Report zu schreiben und behandelt
 * Fehler intelligent:
 * - Temporäre Fehler (EINTR, EAGAIN) werden automatisch erneut versucht
 * - Fatale Fehler (ENODEV, EPIPE, ESHUTDOWN) lösen einen Reconnect-Versuch aus
 * - Bei zu vielen aufeinanderfolgenden Fehlern wird automatisch ein Reconnect eingeleitet
 * 
 * Die Funktion implementiert eine Retry-Strategie mit exponentiellem Backoff
 * für temporäre Fehler und begrenzte Versuche für fatale Fehler nach Reconnect.
 * 
 * @param[in,out] device Zeiger auf die HidDevice-Struktur
 *                       consecutiveErrors wird aktualisiert
 * @param[in] report Zeiger auf den HID-Report-Puffer
 * @param[in] reportSize Größe des HID-Reports in Bytes
 * 
 * @return true wenn der Report erfolgreich geschrieben wurde,
 *         false wenn ein Fehler aufgetreten ist (nach allen Wiederholungsversuchen)
 * 
 * @pre device != NULL
 * @pre device->fd >= 0
 * @pre report != NULL
 * @pre reportSize > 0
 * 
 * @note Bei Erfolg wird consecutiveErrors auf 0 zurückgesetzt
 * @note Nach 3 aufeinanderfolgenden Fehlern wird automatisch ein Reconnect versucht
 * @note Temporäre Fehler werden bis zu 5 Mal mit exponentiellem Backoff wiederholt
 * @note Nach erfolgreicher Wiederverbindung wird der Schreibversuch wiederholt
 * 
 * @see reconnectHidDevice
 * @see isFatalError
 * ************************************************************************************ */    
bool writeHidReport(HidDevice *device, const void *report, size_t reportSize);

/* *********************************************************************************** *
 * @brief Gibt die Ressourcen eines HID-Geräts frei
 * 
 * Diese Funktion schließt das HID-Gerät und gibt den Speicher frei,
 * der für die HidDevice-Struktur und den Gerätepfad allokiert wurde.
 * 
 * @param[in,out] device Zeiger auf die HidDevice-Struktur
 *                       Nach dem Aufruf ist dieser Zeiger ungültig
 * 
 * @note Sicher zu aufrufen mit device == NULL
 * @note Der Aufrufer sollte den Zeiger nach dem Aufruf nicht mehr verwenden
 * 
 * @see initHidDevice
 * @see closeHidDevice
 * *********************************************************************************** */
void cleanupHidDevice(HidDevice *device);

#endif