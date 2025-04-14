/*
 * bs-device-layout-builder.h
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
#include "bs-device-region.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define BS_TYPE_DEVICE_LAYOUT_BUILDER (bs_device_layout_builder_get_type())
G_DECLARE_FINAL_TYPE (BsDeviceLayoutBuilder, bs_device_layout_builder, BS, DEVICE_LAYOUT_BUILDER, GObject)

BsDeviceLayoutBuilder *bs_device_layout_builder_new (BsDevice *device);

void bs_device_layout_builder_add_button_grid (BsDeviceLayoutBuilder *self,
                                               const char            *id,
                                               const BsImageInfo     *image_info,
                                               unsigned int           n_buttons,
                                               unsigned int           grid_columns,
                                               unsigned int           column,
                                               unsigned int           row,
                                               unsigned int           column_span,
                                               unsigned int           row_span);

void bs_device_layout_builder_add_dial_grid (BsDeviceLayoutBuilder *self,
                                             const char            *id,
                                             unsigned int           n_dials,
                                             unsigned int           grid_columns,
                                             unsigned int           column,
                                             unsigned int           row,
                                             unsigned int           column_span,
                                             unsigned int           row_span);

void bs_device_layout_builder_add_touchscreen (BsDeviceLayoutBuilder *self,
                                               const char            *id,
                                               const BsImageInfo     *image_info,
                                               uint32_t               n_slots,
                                               unsigned int           column,
                                               unsigned int           row,
                                               unsigned int           column_span,
                                               unsigned int           row_span);

GListModel * bs_device_layout_builder_build (BsDeviceLayoutBuilder *self);

G_END_DECLS
