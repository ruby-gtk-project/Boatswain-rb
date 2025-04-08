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

BsTouchscreen * bs_touchscreen_new (const char        *id,
                                    BsDevice          *device,
                                    const BsImageInfo *image_info,
                                    uint32_t           n_slots,
                                    unsigned int       column,
                                    unsigned int       row,
                                    unsigned int       column_span,
                                    unsigned int       row_span);

GListModel * bs_touchscreen_get_slots (BsTouchscreen *self);

BsTouchscreenSlot * bs_touchscreen_pick_slot (BsTouchscreen          *self,
                                              const graphene_point_t *point);

uint32_t bs_touchscreen_get_slot_position (BsTouchscreen     *self,
                                           BsTouchscreenSlot *slot);

G_END_DECLS
