/*
 * bs-touchscreen-private.h
 *
 * Copyright 2024 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "bs-touchscreen.h"

#include <gio/gio.h>
#include <graphene-1.0/graphene.h>

G_BEGIN_DECLS

BsTouchscreen * bs_touchscreen_new (BsDeviceRegion *region,
                                    uint32_t        n_slots,
                                    uint32_t        width,
                                    uint32_t        height);

BsDeviceRegion * bs_touchscreen_get_region (BsTouchscreen *self);

uint32_t bs_touchscreen_get_width (BsTouchscreen *self);
uint32_t bs_touchscreen_get_height (BsTouchscreen *self);

BsTouchscreenContent * bs_touchscreen_get_content (BsTouchscreen *self);

GListModel * bs_touchscreen_get_slots (BsTouchscreen *self);

BsTouchscreenSlot * bs_touchscreen_pick_slot (BsTouchscreen          *self,
                                              const graphene_point_t *point);

G_END_DECLS
