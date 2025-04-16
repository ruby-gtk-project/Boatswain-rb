/* bs-device.h
 *
 * Copyright 2022 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if !defined(BOATSWAIN_COMPILATION) && !defined (BOATSWAIN_INSIDE)
#error "Only <boatswain.h> can be included directly."
#endif

#include <gtk/gtk.h>
#include <stdint.h>

#include "bs-types.h"

G_BEGIN_DECLS

#define BS_TYPE_DEVICE (bs_device_get_type())
BS_DECLARE_INTERNAL_TYPE (BsDevice, bs_device, BS, DEVICE, GObject)

void bs_device_reset (BsDevice *self);

const char * bs_device_get_name (BsDevice *self);
const char * bs_device_get_serial_number (BsDevice *self);
const char * bs_device_get_firmware_version (BsDevice *self);

double bs_device_get_brightness (BsDevice *self);
void bs_device_set_brightness (BsDevice *self,
                               double    brightness);

GListModel * bs_device_get_profiles (BsDevice *self);

BsProfile * bs_device_get_active_profile (BsDevice *self);

void bs_device_load_profile (BsDevice  *self,
                             BsProfile *profile);

BsPage * bs_device_get_active_page (BsDevice *self);

void bs_device_push_page (BsDevice *self,
                          BsPage   *page);

void bs_device_pop_page (BsDevice *self);

GListModel * bs_device_get_regions (BsDevice *self);
BsDeviceRegion * bs_device_get_region (BsDevice   *self,
                                       const char *region_id);

G_END_DECLS
