/* bs-icon.h
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
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

#define BS_TYPE_ICON (bs_icon_get_type())
G_DECLARE_FINAL_TYPE (BsIcon, bs_icon, BS, ICON, GObject)

BsIcon * bs_icon_new_from_json (JsonNode  *node,
                                GError   **error);

BsIcon * bs_icon_new (const GdkRGBA *background_color,
                      GdkPaintable  *paintable);

BsIcon * bs_icon_new_empty (void);

JsonNode * bs_icon_to_json (BsIcon *self);

void bs_icon_snapshot_premultiplied (BsIcon      *self,
                                     GdkSnapshot *snapshot,
                                     double       width,
                                     double       height);

const GdkRGBA * bs_icon_get_background_color (BsIcon *self);
void bs_icon_set_background_color (BsIcon        *self,
                                   const GdkRGBA *background_color);

const GdkRGBA * bs_icon_get_color (BsIcon *self);
void bs_icon_set_color (BsIcon        *self,
                        const GdkRGBA *color);

GFile * bs_icon_get_file (BsIcon *self);
void bs_icon_set_file (BsIcon  *self,
                       GFile   *file,
                       GError **error);

const char * bs_icon_get_icon_name (BsIcon *self);
void bs_icon_set_icon_name (BsIcon     *self,
                            const char *icon_name);

GdkPaintable * bs_icon_get_paintable (BsIcon *self);
void bs_icon_set_paintable (BsIcon       *self,
                            GdkPaintable *paintable);

const char * bs_icon_get_text (BsIcon *self);
void bs_icon_set_text (BsIcon     *self,
                       const char *text);

double bs_icon_get_opacity (BsIcon *self);
void  bs_icon_set_opacity (BsIcon *self,
                           double  opacity);

BsIcon * bs_icon_get_relative (BsIcon *self);
void  bs_icon_set_relative (BsIcon *self,
                            BsIcon *relative);

G_END_DECLS
