/* bs-stream-deck-private.h
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

#include "bs-device.h"

#include <gusb.h>

G_BEGIN_DECLS

BsDevice * bs_device_new (GUsbDevice  *gusb_device,
                          GError     **error);

BsDevice * bs_device_new_fake (GError **error);

GUsbDevice * bs_device_get_device (BsDevice *self);

BsDeviceRegion * bs_device_get_region (BsDevice   *self,
                                       const char *region_id);

gboolean bs_device_is_initialized (BsDevice *self);

void bs_device_upload_button (BsDevice *self,
                              BsButton *button);

void bs_device_upload_touchscreen (BsDevice              *self,
                                   BsTouchscreen         *touchscreen,
                                   const graphene_rect_t *region);

void bs_device_load (BsDevice *self);

G_END_DECLS
