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
#include "bs-device-update-private.h"

G_BEGIN_DECLS

struct _BsDevice
{
  GObject parent;
};

struct _BsDeviceClass
{
  GObjectClass parent_class;

  /* Required */
  GListModel * (*create_layout) (BsDevice *self);
  const char * (*get_name) (BsDevice *self);
  const char * (*get_serial_number) (BsDevice *self);
  const char * (*get_firmware_version) (BsDevice *self);

  /* Optional */
  double (*get_brightness) (BsDevice *self);
  void   (*set_brightness) (BsDevice *self,
                            double    brightness);

  void (*load) (BsDevice *self);
  void (*reset) (BsDevice *self);
};

BsDevice * bs_device_new_fake (GError **error);

gboolean bs_device_is_initialized (BsDevice *self);

void bs_device_upload_button (BsDevice *self,
                              BsButton *button);

void bs_device_upload_touchscreen (BsDevice              *self,
                                   BsTouchscreen         *touchscreen,
                                   const graphene_rect_t *region);

void bs_device_load (BsDevice *self);

BsDeviceUpdate * bs_device_steal_update (BsDevice *self);

G_END_DECLS
