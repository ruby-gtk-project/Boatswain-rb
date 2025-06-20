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
#include "bs-renderer-private.h"

#include <jpeglib.h>
#include <jerror.h>
#include <setjmp.h>

struct _BsRenderer
{
  GObject parent_instance;

  BsImageInfo image_info;
  GskRenderer *renderer;
};

G_DEFINE_FINAL_TYPE (BsRenderer, bs_renderer, G_TYPE_OBJECT)

/*
 * JPEG conversion
 *
 * Copied from gdkjpeg.c
 */

#define JPEG_MEM_DEST_USES_SIZE_T                                 \
  (!(LIBJPEG_TURBO_VERSION_NUMBER) &&                             \
   (((JPEG_LIB_VERSION) > 90) ||                                  \
    ((JPEG_LIB_VERSION) == 90 && (JPEG_LIB_VERSION_MAJOR) > 9) || \
    ((JPEG_LIB_VERSION) == 90 && (JPEG_LIB_VERSION_MAJOR) == 9 && (JPEG_LIB_VERSION_MINOR) > 3)))

struct error_handler_data
{
  struct jpeg_error_mgr pub;
  sigjmp_buf setjmp_buffer;
  GError **error;
};

G_GNUC_NORETURN static void
fatal_error_handler (j_common_ptr cinfo)
{
  struct error_handler_data *errmgr;
  char buffer[JMSG_LENGTH_MAX];

  errmgr = (struct error_handler_data *) cinfo->err;

  cinfo->err->format_message (cinfo, buffer);

  if (errmgr->error && *errmgr->error == NULL)
    g_set_error (errmgr->error,
                 GDK_TEXTURE_ERROR,
                 GDK_TEXTURE_ERROR_CORRUPT_IMAGE,
                 "Error interpreting JPEG image file (%s)", buffer);

  siglongjmp (errmgr->setjmp_buffer, 1);

  g_assert_not_reached ();
}

static void
output_message_handler (j_common_ptr cinfo)
{
  /* do nothing */
}

static GBytes *
convert_to_jpeg (GdkTexture *texture)
{
  g_autoptr (GdkTextureDownloader) downloader = NULL;
  struct jpeg_compress_struct info;
  struct error_handler_data jerr;
  struct jpeg_error_mgr err;
  guchar *data = NULL;
#if JPEG_MEM_DEST_USES_SIZE_T
  gsize size = 0;
#else
  gulong size = 0;
#endif
  guchar *input = NULL;
  GBytes *texbytes = NULL;
  const guchar *texdata;
  gsize texstride;
  guchar *row;
  int width, height;

  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_height (texture);

  info.err = jpeg_std_error (&jerr.pub);
  jerr.pub.error_exit = fatal_error_handler;
  jerr.pub.output_message = output_message_handler;
  jerr.error = NULL;

  if (sigsetjmp (jerr.setjmp_buffer, 1))
    {
      free (data);
      g_free (input);
      jpeg_destroy_compress (&info);
      g_clear_pointer (&texbytes, g_bytes_unref);
      return NULL;
    }

  info.err = jpeg_std_error (&err);
  jpeg_create_compress (&info);

  info.image_width = width;
  info.image_height = height;
  info.input_components = 3;
  info.in_color_space = JCS_RGB;

  jpeg_set_defaults (&info);
  jpeg_set_quality (&info, 96, TRUE);

  info.mem->max_memory_to_use = 300 * 1024 * 1024;

  jpeg_mem_dest (&info, &data, &size);

  downloader = gdk_texture_downloader_new (texture);
  gdk_texture_downloader_set_format (downloader, GDK_MEMORY_R8G8B8);
  gdk_texture_downloader_set_color_state (downloader, gdk_color_state_get_srgb ());
  texbytes = gdk_texture_downloader_download_bytes (downloader, &texstride);
  texdata = g_bytes_get_data (texbytes, NULL);

  jpeg_start_compress (&info, TRUE);

  while (info.next_scanline < info.image_height)
    {
      row = (guchar *) texdata + info.next_scanline * texstride;
      jpeg_write_scanlines (&info, &row, 1);
    }

  jpeg_finish_compress (&info);

  g_bytes_unref (texbytes);
  g_free (input);
  jpeg_destroy_compress (&info);

  return g_bytes_new_with_free_func (data, size, (GDestroyNotify) free, NULL);
}

/*
 * GObject overrides
 */

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

GBytes *
bs_renderer_convert_texture (BsRenderer  *self,
                             GdkTexture  *texture,
                             GError     **error)
{
  g_return_val_if_fail (BS_IS_RENDERER (self), FALSE);
  g_return_val_if_fail (GDK_IS_TEXTURE (texture), FALSE);

  switch (self->image_info.format)
    {
    case BS_IMAGE_FORMAT_BMP:
      {
        g_autoptr (GdkPixbuf) pixbuf = NULL;
        g_autofree char *buffer = NULL;
        size_t buffer_len;

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        pixbuf = gdk_pixbuf_get_from_texture (texture);
        G_GNUC_END_IGNORE_DEPRECATIONS

        if (!gdk_pixbuf_save_to_buffer (pixbuf,
                                        &buffer,
                                        &buffer_len,
                                        "bmp",
                                        error,
                                        NULL))
          return NULL;

        return g_bytes_new_take (g_steal_pointer (&buffer), buffer_len);
      }

    case BS_IMAGE_FORMAT_JPEG:
      return convert_to_jpeg (texture);

    default:
      g_assert_not_reached ();
    }
}
