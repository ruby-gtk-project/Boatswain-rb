/*
 * bs-touchscreen-content.h
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

#include <gdk/gdk.h>
#include <json-glib/json-glib.h>
#include <stdint.h>

G_BEGIN_DECLS

#define BS_TYPE_TOUCHSCREEN_CONTENT (bs_touchscreen_content_get_type())
G_DECLARE_FINAL_TYPE (BsTouchscreenContent, bs_touchscreen_content, BS, TOUCHSCREEN_CONTENT, GObject)

BsTouchscreenContent * bs_touchscreen_content_new (GListModel *slots,
                                                   uint32_t    width,
                                                   uint32_t    height);

GdkPaintable *
bs_touchscreen_content_get_background_paintable (BsTouchscreenContent *self);

void bs_touchscreen_content_set_default_background (BsTouchscreenContent *self);
void bs_touchscreen_content_set_background_from_file (BsTouchscreenContent *self,
                                                      GFile                *file);

JsonNode * bs_touchscreen_content_serialize (BsTouchscreenContent *self);
void bs_touchscreen_content_deserialize (BsTouchscreenContent *self,
                                         JsonNode             *node);

G_END_DECLS
