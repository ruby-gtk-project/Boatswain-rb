/* bs-renderer.c
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

#include "bs-icon.h"
#include "bs-renderer.h"

struct _BsRenderer
{
  GObject parent_instance;

  BsImageInfo image_info;
  GskRenderer *renderer;
};

G_DEFINE_FINAL_TYPE (BsRenderer, bs_renderer, G_TYPE_OBJECT)

static void
bs_renderer_finalize (GObject *object)
{
  BsRenderer *self = (BsRenderer *)object;

  g_clear_object (&self->renderer);

  G_OBJECT_CLASS (bs_renderer_parent_class)->finalize (object);
}

static void
bs_renderer_class_init (BsRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = bs_renderer_finalize;
}

static void
bs_renderer_init (BsRenderer *self)
{
  self->renderer = gsk_cairo_renderer_new ();
}

BsRenderer *
bs_renderer_new (const BsImageInfo *image_info)
{
  BsRenderer *self;

  self = g_object_new (BS_TYPE_RENDERER, NULL);
  self->image_info = *image_info;

  return self;
}

GdkTexture *
bs_renderer_compose_icon (BsRenderer  *self,
                          BsIcon      *icon,
                          GError     **error)
{
  g_autoptr (GtkSnapshot) snapshot = NULL;
  g_autoptr (GskRenderNode) node = NULL;
  g_autoptr (GdkTexture) texture = NULL;
  gboolean flip_x;
  gboolean flip_y;
  double height;
  double width;

  g_return_val_if_fail (BS_IS_RENDERER (self), NULL);

  if (!gsk_renderer_realize (self->renderer, NULL, error))
    return NULL;

  snapshot = gtk_snapshot_new ();

  flip_x = self->image_info.flags & BS_RENDERER_FLAG_FLIP_X;
  flip_y = self->image_info.flags & BS_RENDERER_FLAG_FLIP_Y;

  width = (double) self->image_info.width;
  height = (double) self->image_info.height;

  if (self->image_info.flags & BS_RENDERER_FLAG_ROTATE_90)
    {
      gtk_snapshot_translate (snapshot,
                              &GRAPHENE_POINT_INIT (width / 2.0, height / 2.0));
      gtk_snapshot_rotate (snapshot, 90.0);
      gtk_snapshot_translate (snapshot,
                              &GRAPHENE_POINT_INIT (-width / 2.0, -height / 2.0));
    }

  gtk_snapshot_translate (snapshot,
                          &GRAPHENE_POINT_INIT (flip_x ? width : 0.0,
                                                flip_y ? height : 0.0));

  gtk_snapshot_scale (snapshot,
                      flip_x ? -1.0 : 1.0,
                      flip_y ? -1.0 : 1.0);

  if (icon)
    {
      bs_icon_snapshot_premultiplied (icon, snapshot, width, height);
    }
  else
    {
      gtk_snapshot_append_color (snapshot,
                                 &(GdkRGBA) { 0.0, 0.0, 0.0, 1.0, },
                                 &GRAPHENE_RECT_INIT (0, 0, width, height));
    }

  node = gtk_snapshot_free_to_node (g_steal_pointer (&snapshot));

  texture = gsk_renderer_render_texture (self->renderer,
                                         node,
                                         &GRAPHENE_RECT_INIT (0, 0, width, height));
  gsk_renderer_unrealize (self->renderer);

  return g_steal_pointer (&texture);
}

GdkTexture *
bs_renderer_compose_touchscreen_content (BsRenderer            *self,
                                         BsTouchscreenContent  *content,
                                         GError               **error)
{
  g_autoptr (GskRenderNode) node = NULL;
  g_autoptr (GtkSnapshot) snapshot = NULL;
  g_autoptr (GdkTexture) texture = NULL;
  gboolean flip_x;
  gboolean flip_y;
  double height;
  double width;

  g_return_val_if_fail (BS_IS_RENDERER (self), NULL);

  if (!gsk_renderer_realize (self->renderer, NULL, error))
    return NULL;

  flip_x = self->image_info.flags & BS_RENDERER_FLAG_FLIP_X;
  flip_y = self->image_info.flags & BS_RENDERER_FLAG_FLIP_Y;

  width = (double) self->image_info.width;
  height = (double) self->image_info.height;

  snapshot = gtk_snapshot_new ();

  if (self->image_info.flags & BS_RENDERER_FLAG_ROTATE_90)
    {
      gtk_snapshot_translate (snapshot,
                              &GRAPHENE_POINT_INIT (width / 2.0, height / 2.0));
      gtk_snapshot_rotate (snapshot, 90.0);
      gtk_snapshot_translate (snapshot,
                              &GRAPHENE_POINT_INIT (-width / 2.0, -height / 2.0));
    }

  gtk_snapshot_translate (snapshot,
                          &GRAPHENE_POINT_INIT (flip_x ? width : 0.0,
                                                flip_y ? height : 0.0));

  gtk_snapshot_scale (snapshot,
                      flip_x ? -1.0 : 1.0,
                      flip_y ? -1.0 : 1.0);

  gdk_paintable_snapshot (GDK_PAINTABLE (content), snapshot, width, height);

  node = gtk_snapshot_free_to_node (g_steal_pointer (&snapshot));

  texture = gsk_renderer_render_texture (self->renderer,
                                         node,
                                         &GRAPHENE_RECT_INIT (0, 0, width, height));
  gsk_renderer_unrealize (self->renderer);

  return g_steal_pointer (&texture);
}

gboolean
bs_renderer_convert_texture (BsRenderer  *self,
                             GdkTexture  *texture,
                             char       **buffer,
                             size_t      *buffer_len,
                             GError     **error)
{
  g_autoptr (GdkPixbuf) pixbuf = NULL;

  g_return_val_if_fail (BS_IS_RENDERER (self), FALSE);
  g_return_val_if_fail (GDK_IS_TEXTURE (texture), FALSE);
  g_return_val_if_fail (buffer != NULL, FALSE);
  g_return_val_if_fail (buffer_len != NULL, FALSE);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  pixbuf = gdk_pixbuf_get_from_texture (texture);
G_GNUC_END_IGNORE_DEPRECATIONS

  switch (self->image_info.format)
    {
    case BS_IMAGE_FORMAT_BMP:
      return gdk_pixbuf_save_to_buffer (pixbuf,
                                        buffer,
                                        buffer_len,
                                        "bmp",
                                        error,
                                        NULL);

    case BS_IMAGE_FORMAT_JPEG:
      return gdk_pixbuf_save_to_buffer (pixbuf,
                                        buffer,
                                        buffer_len,
                                        "jpeg",
                                        error,
                                        "quality", "96",
                                        NULL);

    default:
      g_assert_not_reached ();
    }
}
