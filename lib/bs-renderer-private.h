/* bs-renderer-private.h
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

enum _BsRendererFlags
{
  BS_RENDERER_FLAG_NONE = 0,
  BS_RENDERER_FLAG_FLIP_X = 1 << 0,
  BS_RENDERER_FLAG_FLIP_Y = 1 << 1,
  BS_RENDERER_FLAG_ROTATE_90 = 1 << 2,
};

enum _BsImageFormat
{
  BS_IMAGE_FORMAT_BMP,
  BS_IMAGE_FORMAT_JPEG,
};

struct _BsImageInfo
{
  BsImageFormat format;
  BsRendererFlags flags;
  uint32_t width;
  uint32_t height;
};

#define BS_TYPE_RENDERER (bs_renderer_get_type())
G_DECLARE_FINAL_TYPE (BsRenderer, bs_renderer, BS, RENDERER, GObject)

BsRenderer * bs_renderer_new (const BsImageInfo *layout);

GdkTexture * bs_renderer_compose_icon (BsRenderer  *self,
                                       BsIcon      *icon,
                                       GError     **error);

GdkTexture * bs_renderer_compose_touchscreen_content (BsRenderer            *self,
                                                      BsTouchscreenContent  *content,
                                                      GError               **error);

GBytes * bs_renderer_convert_texture (BsRenderer  *self,
                                      GdkTexture  *texture,
                                      GError     **error);

G_END_DECLS
