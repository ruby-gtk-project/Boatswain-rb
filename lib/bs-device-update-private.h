/*
 * bs-device-update.h
 *
 * Copyright 2025 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "bs-types.h"

#include <glib-object.h>
#include <graphene.h>

G_BEGIN_DECLS

typedef struct
{
  BsButton *button;
} BsButtonUpdate;

typedef struct
{
  BsTouchscreen *touchscreen;
  graphene_rect_t region;
} BsTouchscreenUpdate;

#define BS_TYPE_DEVICE_UPDATE (bs_device_update_get_type())
G_DECLARE_FINAL_TYPE (BsDeviceUpdate, bs_device_update, BS, DEVICE_UPDATE, GObject)

BsDeviceUpdate * bs_device_update_new (void);

void bs_device_update_add_button (BsDeviceUpdate *self,
                                  BsButton       *button);

void bs_device_update_add_touchscreen_region (BsDeviceUpdate        *self,
                                              BsTouchscreen         *touchscreen,
                                              const graphene_rect_t *region);

void bs_device_update_seal (BsDeviceUpdate *self);

BsButtonUpdate ** bs_device_update_get_button_updates (BsDeviceUpdate *self);
BsTouchscreenUpdate ** bs_device_update_get_touchscreen_updates (BsDeviceUpdate *self);

G_END_DECLS
