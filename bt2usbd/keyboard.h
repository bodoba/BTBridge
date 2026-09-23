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
#ifndef BT2USBD_KBD_H
#define BT2USBD_KBD_H

/* *********************************************************************************** *
 * @brief Check if this is a keyboard device
 * @param event  Name of the divece file in /dev/input/
 * @return       device handle, or -1 on failure
 * *********************************************************************************** */
int checkForKeyboard(const char *event);

/* *********************************************************************************** *
 * @brief Iterate throuogh /dev/input/ to find a keyboard decvice
 * @return        device handle, or -1 on failure
 * *********************************************************************************** */
int findKeyboard(void);

#endif