/* bs-icon.c
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

#define G_LOG_DOMAIN "Icon"

#include "bs-icon.h"

#define ICON_SIZE 32
#define INTENSITY(c)  ((c.red) * 0.30 + (c.green) * 0.59 + (c.blue) * 0.11)

struct _BsIcon
{
  GObject parent_instance;

  GdkRGBA background_color;
  GdkRGBA color;
  GdkPaintable *paintable;
  PangoLayout *layout;
  GFile *file;
  char *icon_name;
  double opacity;
  BsIcon *relative;

  GtkIconPaintable *icon_paintable;
  GdkTexture *file_texture;

  GtkMediaStream *file_media_stream;
  gulong file_media_stream_content_changed_id;

  gulong relative_size_changed_id;
  gulong relative_content_changed_id;

  gulong size_changed_id;
  gulong content_changed_id;

  gboolean foreground_color_set;
};

static void gdk_paintable_iface_init (GdkPaintableInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (BsIcon, bs_icon, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE, gdk_paintable_iface_init))

enum
{
  PROP_0,
  PROP_BACKGROUND_COLOR,
  PROP_COLOR,
  PROP_FILE,
  PROP_ICON_NAME,
  PROP_OPACITY,
  PROP_PAINTABLE,
  PROP_RELATIVE,
  PROP_TEXT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];


static GdkRGBA opaque_white = { 1.0, 1.0, 1.0, 1.0 };
static GdkRGBA transparent_black = { 0.0, 0.0, 0.0, 0.0 };

/*
 * Callbacks
 */

static void
on_paintable_contents_changed_cb (GdkPaintable *paintable,
                                  BsIcon       *self)
{
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}

static void
on_paintable_size_changed_cb (GdkPaintable *paintable,
                              BsIcon       *self)
{
  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
}

static void
premultiply_rgba (const GdkRGBA *rgba,
                  GdkRGBA       *premultiplied_rgba)
{
  premultiplied_rgba->red = rgba->red * rgba->alpha;
  premultiplied_rgba->green = rgba->green * rgba->alpha;
  premultiplied_rgba->blue = rgba->blue * rgba->alpha;
  premultiplied_rgba->alpha = 1.0;
}

static GdkRGBA
generate_foreground_color (BsIcon *self)
{
  GdkRGBA background_color;

  premultiply_rgba (&self->background_color, &background_color);

  if (INTENSITY (background_color) > 0.5)
    return (GdkRGBA) { 0.0, 0.0, 0.0, 1.0 };
  else
    return (GdkRGBA) { 1.0, 1.0, 1.0, 1.0 };
}

static gboolean
snapshot_any_paintable (GdkSnapshot *snapshot,
                        BsIcon      *icon,
                        double       width,
                        double       height)
{
  GdkPaintable *paintable = NULL;

  if (icon->paintable)
    paintable = icon->paintable;
  else if (icon->file_media_stream)
    paintable = GDK_PAINTABLE (icon->file_media_stream);
  else if (icon->file_texture)
    paintable = GDK_PAINTABLE (icon->file_texture);
  else if (icon->icon_paintable)
    paintable = GDK_PAINTABLE (icon->icon_paintable);

  if (!paintable)
    return FALSE;

  if (GTK_IS_SYMBOLIC_PAINTABLE (paintable))
    {
      float hpadding = (width - ICON_SIZE) / 2.0;
      float vpadding = (height - ICON_SIZE) / 2.0;
      GdkRGBA color;

      if (icon->foreground_color_set)
        color = icon->color;
      else
        color = generate_foreground_color (icon);

      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (hpadding, vpadding));
      gtk_symbolic_paintable_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (icon->icon_paintable),
                                                snapshot,
                                                ICON_SIZE,
                                                ICON_SIZE,
                                                &color,
                                                1);
      gtk_snapshot_restore (snapshot);
    }
  else
    {
      double paintable_width = MIN (width, gdk_paintable_get_intrinsic_width (paintable));
      double paintable_height = MIN (height, gdk_paintable_get_intrinsic_height (paintable));
      float hpadding = (width - paintable_width) / 2.0;
      float vpadding = (height - paintable_height) / 2.0;

      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (hpadding, vpadding));
      gdk_paintable_snapshot (paintable, snapshot, paintable_width, paintable_height);
      gtk_snapshot_restore (snapshot);
    }

  return paintable != NULL;
}

static gboolean
snapshot_any_layout (GdkSnapshot *snapshot,
                     BsIcon      *icon,
                     double       width,
                     double       height)
{
  if (icon->layout)
    {
      GdkRGBA color;
      int text_width, text_height;
      int x, y;

      color = generate_foreground_color (icon);

      pango_layout_get_pixel_size (icon->layout, &text_width, &text_height);
      x = (width - text_width) / 2.0;
      y = height - text_height - 3.0;

      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, y));
      gtk_snapshot_append_layout (snapshot, icon->layout, &color);
      gtk_snapshot_restore (snapshot);
    }

  return icon->layout != NULL;
}

static inline double
get_real_opacity (BsIcon *self)
{
  if (self->opacity == -1.0 && self->relative)
    return self->relative->opacity;

  return self->opacity;
}

static void
snapshot_icon (BsIcon      *self,
               GdkSnapshot *snapshot,
               double       width,
               double       height,
               gboolean     premultiply)
{
  GdkRGBA background_color;
  double opacity;

  opacity = get_real_opacity (self);

  background_color = self->background_color;
  if (premultiply)
    premultiply_rgba (&background_color, &background_color);

  gtk_snapshot_append_color (snapshot,
                             &background_color,
                             &GRAPHENE_RECT_INIT (0, 0, width, height));

  if (opacity != -1.0)
    gtk_snapshot_push_opacity (snapshot, opacity);

  if (!snapshot_any_paintable (snapshot, self, width, height) && self->relative)
    snapshot_any_paintable (snapshot, self->relative, width, height);

  if (!snapshot_any_layout (snapshot, self, width, height) && self->relative)
    snapshot_any_layout (snapshot, self->relative, width, height);

  if (opacity != -1.0)
    gtk_snapshot_pop (snapshot);
}


/*
 * GdkPaintable interface
 */

static double
bs_icon_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  BsIcon *self = BS_ICON (paintable);

  return self->paintable ? gdk_paintable_get_intrinsic_aspect_ratio (self->paintable) : 1.0;
}

static int
bs_icon_get_intrinsic_width (GdkPaintable *paintable)
{
  BsIcon *self = BS_ICON (paintable);

  return self->paintable ? gdk_paintable_get_intrinsic_width (self->paintable) : 0;
}


static int
bs_icon_get_intrinsic_height (GdkPaintable *paintable)
{
  BsIcon *self = BS_ICON (paintable);

  return self->paintable ? gdk_paintable_get_intrinsic_height (self->paintable) : 0;
}

static void
bs_icon_snapshot (GdkPaintable *paintable,
                  GdkSnapshot  *snapshot,
                  double        width,
                  double        height)
{
  BsIcon *self = BS_ICON (paintable);

  snapshot_icon (self, snapshot, width, height, FALSE);
}

static void
gdk_paintable_iface_init (GdkPaintableInterface *iface)
{
  iface->get_intrinsic_aspect_ratio = bs_icon_get_intrinsic_aspect_ratio;
  iface->get_intrinsic_height = bs_icon_get_intrinsic_height;
  iface->get_intrinsic_width = bs_icon_get_intrinsic_width;
  iface->snapshot = bs_icon_snapshot;
}


/*
 * GObject overrides
 */

static void
bs_icon_finalize (GObject *object)
{
  BsIcon *self = (BsIcon *)object;

  if (self->relative)
    {
      g_clear_signal_handler (&self->relative_content_changed_id, self->relative);
      g_clear_signal_handler (&self->relative_size_changed_id, self->relative);
      g_object_remove_weak_pointer (G_OBJECT (self->relative), (gpointer) &self->relative);
      self->relative = NULL;
    }

  g_clear_signal_handler (&self->file_media_stream_content_changed_id, self->file_media_stream);
  g_clear_signal_handler (&self->content_changed_id, self->paintable);
  g_clear_signal_handler (&self->size_changed_id, self->paintable);
  g_clear_pointer (&self->icon_name, g_free);
  g_clear_object (&self->paintable);
  g_clear_object (&self->file_media_stream);
  g_clear_object (&self->file_texture);
  g_clear_object (&self->file);
  g_clear_object (&self->layout);

  G_OBJECT_CLASS (bs_icon_parent_class)->finalize (object);
}

static void
bs_icon_get_property (GObject    *object,
                      guint       prop_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
  BsIcon *self = BS_ICON (object);

  switch (prop_id)
    {
    case PROP_BACKGROUND_COLOR:
      g_value_set_boxed (value, &self->background_color);
      break;

    case PROP_COLOR:
      g_value_set_boxed (value, &self->color);
      break;

    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    case PROP_ICON_NAME:
      g_value_set_string (value, self->icon_name);
      break;

    case PROP_OPACITY:
      g_value_set_double (value, self->opacity);
      break;

    case PROP_PAINTABLE:
      g_value_set_object (value, self->paintable);
      break;

    case PROP_RELATIVE:
      g_value_set_object (value, self->relative);
      break;

    case PROP_TEXT:
      g_value_set_string (value, self->layout ? pango_layout_get_text (self->layout) : NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_icon_set_property (GObject      *object,
                      guint         prop_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  BsIcon *self = BS_ICON (object);

  switch (prop_id)
    {
    case PROP_BACKGROUND_COLOR:
      bs_icon_set_background_color (self, g_value_get_boxed (value));
      break;

    case PROP_COLOR:
      bs_icon_set_color (self, g_value_get_boxed (value));
      break;

    case PROP_FILE:
      bs_icon_set_file (self, g_value_get_object (value), NULL);
      break;

    case PROP_ICON_NAME:
      bs_icon_set_icon_name (self, g_value_get_string (value));
      break;

    case PROP_OPACITY:
      bs_icon_set_opacity (self, g_value_get_double (value));
      break;

    case PROP_PAINTABLE:
      bs_icon_set_paintable (self, g_value_get_object (value));
      break;

    case PROP_RELATIVE:
      bs_icon_set_relative (self, g_value_get_object (value));
      break;

    case PROP_TEXT:
      bs_icon_set_text (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_icon_class_init (BsIconClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = bs_icon_finalize;
  object_class->get_property = bs_icon_get_property;
  object_class->set_property = bs_icon_set_property;

  properties[PROP_BACKGROUND_COLOR] = g_param_spec_boxed ("background-color", NULL, NULL,
                                                          GDK_TYPE_RGBA,
                                                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_COLOR] = g_param_spec_boxed ("color", NULL, NULL,
                                               GDK_TYPE_RGBA,
                                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_FILE] = g_param_spec_object ("file", NULL, NULL,
                                               G_TYPE_FILE,
                                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_ICON_NAME] = g_param_spec_string ("icon-name", NULL, NULL,
                                                    NULL,
                                                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_OPACITY] = g_param_spec_double ("opacity", NULL, NULL,
                                                  -1.0, 1.0, -1.0,
                                                  G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_PAINTABLE] = g_param_spec_object ("paintable", NULL, NULL,
                                                    GDK_TYPE_PAINTABLE,
                                                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_RELATIVE] = g_param_spec_object ("relative", NULL, NULL,
                                                    BS_TYPE_ICON,
                                                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_TEXT] = g_param_spec_string ("text", NULL, NULL,
                                               NULL,
                                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
bs_icon_init (BsIcon *self)
{
  self->foreground_color_set = FALSE;
  self->color = opaque_white;
  self->opacity = -1.0;
}


BsIcon *
bs_icon_new_empty (void)
{
  return bs_icon_new (&transparent_black, NULL);
}

BsIcon *
bs_icon_new (const GdkRGBA *background_color,
             GdkPaintable  *paintable)
{
  return g_object_new (BS_TYPE_ICON,
                       "background-color", background_color,
                       "paintable", paintable,
                       NULL);
}

BsIcon *
bs_icon_new_from_json (JsonNode  *node,
                       GError   **error)
{
  g_autoptr (GFile) file = NULL;
  JsonObject *object;
  GdkRGBA background_color;

  if (!JSON_NODE_HOLDS_OBJECT (node))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "JSON node is not an object");
      return NULL;
    }

  object = json_node_get_object (node);
  gdk_rgba_parse (&background_color,
                  json_object_get_string_member_with_default (object,
                                                              "background-color",
                                                              "rgba(0,0,0,0)"));

  if (json_object_has_member (object, "file"))
    file = g_file_new_for_uri (json_object_get_string_member (object, "file"));

  return g_object_new (BS_TYPE_ICON,
                       "background-color", &background_color,
                       "file", file,
                       "icon-name", json_object_get_string_member_with_default (object, "icon-name", NULL),
                       "text", json_object_get_string_member_with_default (object, "text", NULL),
                       NULL);
}

JsonNode *
bs_icon_to_json (BsIcon *self)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autofree char *background_color = NULL;

  g_return_val_if_fail (BS_IS_ICON (self), NULL);

  builder = json_builder_new ();

  json_builder_begin_object (builder);

  background_color = gdk_rgba_to_string (&self->background_color);

  json_builder_set_member_name (builder, "background-color");
  json_builder_add_string_value (builder, background_color);

  json_builder_set_member_name (builder, "text");
  if (self->layout)
    json_builder_add_string_value (builder, pango_layout_get_text (self->layout));
  else
    json_builder_add_null_value (builder);

  if (self->file)
    {
      g_autofree char *uri = g_file_get_uri (self->file);

      json_builder_set_member_name (builder, "file");
      json_builder_add_string_value (builder, uri);
    }

  if (self->icon_name)
    {
      json_builder_set_member_name (builder, "icon-name");
      json_builder_add_string_value (builder, self->icon_name);
    }

  json_builder_end_object (builder);

  return json_builder_get_root (builder);
}

void
bs_icon_snapshot_premultiplied (BsIcon      *self,
                                GdkSnapshot *snapshot,
                                double       width,
                                double       height)

{
  g_return_if_fail (BS_IS_ICON (self));
  g_return_if_fail (GDK_IS_SNAPSHOT (snapshot));

  snapshot_icon (self, snapshot, width, height, TRUE);
}

const GdkRGBA *
bs_icon_get_background_color (BsIcon *self)
{
  g_return_val_if_fail (BS_IS_ICON (self), NULL);

  return &self->background_color;
}

void
bs_icon_set_background_color (BsIcon        *self,
                              const GdkRGBA *background_color)
{
  g_return_if_fail (BS_IS_ICON (self));

  if (background_color)
    self->background_color = *background_color;
  else
    self->background_color = transparent_black;

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BACKGROUND_COLOR]);
}

const GdkRGBA *
bs_icon_get_color (BsIcon *self)
{
  g_return_val_if_fail (BS_IS_ICON (self), NULL);

  return &self->color;
}

void
bs_icon_set_color (BsIcon        *self,
                   const GdkRGBA *color)
{
  g_return_if_fail (BS_IS_ICON (self));

  if (color)
    self->color = *color;
  else
    self->color = opaque_white;

  self->foreground_color_set = color != NULL;

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BACKGROUND_COLOR]);
}

GFile *
bs_icon_get_file (BsIcon *self)
{
  g_return_val_if_fail (BS_IS_ICON (self), NULL);

  return self->file;
}

void
bs_icon_set_file (BsIcon  *self,
                  GFile   *file,
                  GError **error)
{
  g_autoptr (GtkMediaStream) media_stream = NULL;
  g_autoptr (GdkTexture) texture = NULL;

  g_return_if_fail (BS_IS_ICON (self));

  if (self->file == file)
    return;

  if (file)
    {
      g_autoptr (GFileInfo) file_info = NULL;
      g_autoptr (GError) local_error = NULL;

      file_info = g_file_query_info (file,
                                     G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                     G_FILE_QUERY_INFO_NONE,
                                     NULL,
                                     &local_error);

      if (file_info)
        {
          const char * const media_stream_content_types[] = {
            "image/gif",
            "video/*",
          };
          const char *content_type;

          content_type = g_file_info_get_content_type (file_info);
          for (size_t i = 0; i < G_N_ELEMENTS (media_stream_content_types); i++)
            {
              if (!g_content_type_is_mime_type (content_type, media_stream_content_types[i]))
                continue;

              media_stream = gtk_media_file_new_for_file (file);
              gtk_media_stream_set_volume (media_stream, 0.0);
              gtk_media_stream_set_muted (media_stream, TRUE);
              gtk_media_stream_set_loop (media_stream, TRUE);
              gtk_media_stream_play (media_stream);
              break;
            }
        }
      else
        {
          g_warning ("Error querying file info: %s", local_error->message);
        }

      if (!media_stream)
        {
          texture = gdk_texture_new_from_file (file, error);
          if (!texture)
            return;
        }
    }

  g_clear_signal_handler (&self->file_media_stream_content_changed_id, self->file_media_stream);
  g_set_object (&self->file_media_stream, media_stream);
  g_set_object (&self->file_texture, texture);
  g_set_object (&self->file, file);

  if (media_stream)
    self->file_media_stream_content_changed_id = g_signal_connect (media_stream,
                                                                   "invalidate-contents",
                                                                   G_CALLBACK (on_paintable_contents_changed_cb),
                                                                   self);

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FILE]);
}

const char *
bs_icon_get_icon_name (BsIcon *self)
{
  g_return_val_if_fail (BS_IS_ICON (self), NULL);

  return self->icon_name;
}

void
bs_icon_set_icon_name (BsIcon     *self,
                       const char *icon_name)
{
  GtkIconTheme *icon_theme;

  g_return_if_fail (BS_IS_ICON (self));

  if (g_strcmp0 (self->icon_name, icon_name) == 0)
    return;

  g_clear_pointer (&self->icon_name, g_free);
  self->icon_name = g_strdup (icon_name);

  icon_theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
  self->icon_paintable = gtk_icon_theme_lookup_icon (icon_theme,
                                                     icon_name,
                                                     NULL,
                                                     ICON_SIZE,
                                                     1,
                                                     gtk_get_locale_direction (),
                                                     0);

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON_NAME]);
}

GdkPaintable *
bs_icon_get_paintable (BsIcon *self)
{
  g_return_val_if_fail (BS_IS_ICON (self), NULL);

  return self->paintable;
}

void
bs_icon_set_paintable (BsIcon       *self,
                       GdkPaintable *paintable)
{
  g_return_if_fail (BS_IS_ICON (self));
  g_return_if_fail (paintable != GDK_PAINTABLE (self));

  if (self->paintable == paintable)
    return;

  g_clear_signal_handler (&self->content_changed_id, self->paintable);
  g_clear_signal_handler (&self->size_changed_id, self->paintable);

  g_set_object (&self->paintable, paintable);

  self->content_changed_id = g_signal_connect (paintable,
                                               "invalidate-contents",
                                               G_CALLBACK (on_paintable_contents_changed_cb),
                                               self);

  self->size_changed_id = g_signal_connect (paintable,
                                            "invalidate-contents",
                                            G_CALLBACK (on_paintable_size_changed_cb),
                                            self);

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PAINTABLE]);
}

const char *
bs_icon_get_text (BsIcon *self)
{
  g_return_val_if_fail (BS_IS_ICON (self), NULL);

  return self->layout ? pango_layout_get_text (self->layout) : NULL;
}

void
bs_icon_set_text (BsIcon     *self,
                  const char *text)
{
  g_return_if_fail (BS_IS_ICON (self));

  if (self->layout && g_strcmp0 (pango_layout_get_text (self->layout), text) == 0)
    return;

  if (text)
    {
      if (!self->layout)
        {
          g_autoptr (PangoFontDescription) font_description = NULL;
          PangoContext *pango_context;

          font_description = pango_font_description_from_string ("Cantarell Bold 8");

          pango_context = pango_font_map_create_context (pango_cairo_font_map_get_default ());
          pango_context_set_language (pango_context, gtk_get_default_language ());
          pango_context_set_font_description (pango_context, font_description);

          self->layout = pango_layout_new (pango_context);

          g_clear_object (&pango_context);
        }

      pango_layout_set_text (self->layout, text, -1);
    }
  else
    {
      g_clear_object (&self->layout);
    }

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TEXT]);
}

double
bs_icon_get_opacity (BsIcon *self)
{
  g_return_val_if_fail (BS_IS_ICON (self), -1.0);

  return self->opacity;
}

void
bs_icon_set_opacity (BsIcon *self,
                     double  opacity)
{
  g_return_if_fail (BS_IS_ICON (self));
  g_return_if_fail (opacity == -1.0 || (opacity >= 0.0 && opacity <= 1.0));

  if (G_APPROX_VALUE (self->opacity, opacity, DBL_EPSILON))
    return;

  self->opacity = opacity;

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_OPACITY]);
}

BsIcon *
bs_icon_get_relative (BsIcon *self)
{
  g_return_val_if_fail (BS_IS_ICON (self), NULL);

  return self->relative;
}

void
bs_icon_set_relative (BsIcon *self,
                      BsIcon *relative)
{
  g_return_if_fail (BS_IS_ICON (self));
  g_return_if_fail (relative == NULL || BS_IS_ICON (relative));
  g_return_if_fail (relative != self);

  if (self->relative == relative)
    return;

  if (self->relative)
    {
      g_clear_signal_handler (&self->relative_content_changed_id, self->relative);
      g_clear_signal_handler (&self->relative_size_changed_id, self->relative);
      g_object_remove_weak_pointer (G_OBJECT (self->relative), (gpointer) &self->relative);
    }

  self->relative = relative;

  if (relative)
    {
      self->relative_content_changed_id = g_signal_connect (relative,
                                                            "invalidate-contents",
                                                            G_CALLBACK (on_paintable_contents_changed_cb),
                                                            self);

      self->relative_size_changed_id = g_signal_connect (relative,
                                                         "invalidate-contents",
                                                         G_CALLBACK (on_paintable_size_changed_cb),
                                                         self);

      g_object_add_weak_pointer (G_OBJECT (self->relative), (gpointer) &self->relative);
    }

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RELATIVE]);
}
