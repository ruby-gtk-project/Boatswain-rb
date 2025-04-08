/*
 * bs-touchscreen-content.c
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

#include "bs-touchscreen-content.h"

#include "bs-actionable.h"
#include "bs-action.h"
#include "bs-touchscreen-slot-private.h"

#include <graphene-gobject.h>
#include <gtk/gtk.h>

typedef enum
{
  BACKGROUND_TYPE_DEFAULT,
  BACKGROUND_TYPE_FILE,
} BackgroundType;

struct _BsTouchscreenContent
{
  GObject parent_instance;

  struct {
    BackgroundType type;
    GdkPaintable *paintable;
    gulong content_invalidated_id;

    union {
      GFile *file;
    } d;

  } background;

  GListModel *slots;
  uint32_t width;
  uint32_t height;
};

static void on_background_invalidate_contents_cb (GdkPaintable         *paintable,
                                                  BsTouchscreenContent *self);

static void gdk_paintable_interface_init (GdkPaintableInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (BsTouchscreenContent, bs_touchscreen_content, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE, gdk_paintable_interface_init))

enum
{
  PROP_0,
  PROP_BACKGROUND_PAINTABLE,
  PROP_WIDTH,
  PROP_HEIGHT,
  N_PROPS,
};

enum
{
  INVALIDATE_REGION,
  N_SIGNALS,
};

static guint signals[N_SIGNALS];
static GParamSpec *properties [N_PROPS];


/*
 * Auxiliary methods
 */

static void
set_background_paintable (BsTouchscreenContent *self,
                          GdkPaintable         *paintable)
{
  graphene_rect_t rect;

  g_return_if_fail (BS_IS_TOUCHSCREEN_CONTENT (self));
  g_return_if_fail (!paintable || GDK_IS_PAINTABLE (paintable));

  if (self->background.paintable == paintable)
    return;

  g_clear_signal_handler (&self->background.content_invalidated_id, self->background.paintable);

  g_set_object (&self->background.paintable, paintable);

  if (paintable)
    {
      self->background.content_invalidated_id =
        g_signal_connect (paintable,
                          "invalidate-contents",
                          G_CALLBACK (on_background_invalidate_contents_cb),
                          self);
    }

  rect = GRAPHENE_RECT_INIT (0.f, 0.f, self->width, self->height);
  g_signal_emit (self, signals[INVALIDATE_REGION], 0, &rect);

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BACKGROUND_PAINTABLE]);
}



/*
 * Callbacks
 */

static void
on_slot_invalidate_contents_cb (GdkPaintable         *paintable,
                                BsTouchscreenContent *self)
{
  graphene_rect_t rect;
  unsigned int position;
  double slot_width;
  size_t n_slots;

  g_assert (BS_IS_TOUCHSCREEN_CONTENT (self));
  g_assert (BS_IS_TOUCHSCREEN_SLOT (paintable));

  position = GTK_INVALID_LIST_POSITION;
  n_slots = g_list_model_get_n_items (self->slots);
  for (size_t i = 0; i < n_slots; i++)
    {
      g_autoptr (BsTouchscreenSlot) slot = g_list_model_get_item (self->slots, i);

      if (slot == BS_TOUCHSCREEN_SLOT (paintable))
        {
          position = i;
          break;
        }
    }

  g_assert (position != GTK_INVALID_LIST_POSITION);

  slot_width = self->width / n_slots;

  graphene_rect_init (&rect, position * slot_width, 0.f, slot_width, self->height);
  g_signal_emit (self, signals[INVALIDATE_REGION], 0, &rect);

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}

static void
on_background_invalidate_contents_cb (GdkPaintable         *paintable,
                                      BsTouchscreenContent *self)
{
  graphene_rect_t rect = GRAPHENE_RECT_INIT (0.f, 0.f, self->width, self->height);

  g_signal_emit (self, signals[INVALIDATE_REGION], 0, &rect);

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}


/*
 * GdkPaintable interface
 */

static void
bs_touchscreen_content_snapshot (GdkPaintable *paintable,
                                 GdkSnapshot  *snapshot,
                                 double        width,
                                 double        height)
{
  BsTouchscreenContent *self = (BsTouchscreenContent *) paintable;
  double slot_width;
  size_t n_slots;

  g_assert (BS_IS_TOUCHSCREEN_CONTENT (self));

  if (self->background.paintable)
    gdk_paintable_snapshot (self->background.paintable, snapshot, width, height);

  n_slots = g_list_model_get_n_items (self->slots);
  slot_width = width / n_slots;

  for (size_t i = 0; i < n_slots; i++)
    {
      g_autoptr (BsTouchscreenSlot) slot = g_list_model_get_item (self->slots, i);

      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (i * slot_width, 0));

      gdk_paintable_snapshot (GDK_PAINTABLE (slot), snapshot, slot_width, height);

      gtk_snapshot_restore (snapshot);
    }
}

static int
bs_touchscreen_content_get_intrinsic_width (GdkPaintable *paintable)
{
  BsTouchscreenContent *self = (BsTouchscreenContent *) paintable;

  g_assert (BS_IS_TOUCHSCREEN_CONTENT (self));

  return self->width;
}

static int
bs_touchscreen_content_get_intrinsic_height (GdkPaintable *paintable)
{
  BsTouchscreenContent *self = (BsTouchscreenContent *) paintable;

  g_assert (BS_IS_TOUCHSCREEN_CONTENT (self));

  return self->height;
}

static GdkPaintableFlags
bs_touchscreen_content_get_flags (GdkPaintable *paintable)
{
  return GDK_PAINTABLE_STATIC_SIZE;
}

static void
gdk_paintable_interface_init (GdkPaintableInterface *iface)
{
  iface->snapshot = bs_touchscreen_content_snapshot;
  iface->get_intrinsic_width = bs_touchscreen_content_get_intrinsic_width;
  iface->get_intrinsic_height = bs_touchscreen_content_get_intrinsic_height;
  iface->get_flags = bs_touchscreen_content_get_flags;
}

/*
 * GObject overrides
 */

static void
bs_touchscreen_content_finalize (GObject *object)
{
  BsTouchscreenContent *self = (BsTouchscreenContent *)object;

  g_clear_signal_handler (&self->background.content_invalidated_id, self->background.paintable);

  g_clear_object (&self->background.paintable);
  g_clear_object (&self->slots);

  G_OBJECT_CLASS (bs_touchscreen_content_parent_class)->finalize (object);
}

static void
bs_touchscreen_content_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  BsTouchscreenContent *self = BS_TOUCHSCREEN_CONTENT (object);

  switch (prop_id)
    {
    case PROP_BACKGROUND_PAINTABLE:
      g_value_set_object (value, self->background.paintable);
      break;

    case PROP_HEIGHT:
      g_value_set_uint (value, self->height);
      break;

    case PROP_WIDTH:
      g_value_set_uint (value, self->width);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_touchscreen_content_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  BsTouchscreenContent *self = BS_TOUCHSCREEN_CONTENT (object);

  switch (prop_id)
    {
    case PROP_HEIGHT:
      self->height = g_value_get_uint (value);
      g_assert (self->height > 0);
      break;

    case PROP_WIDTH:
      self->width = g_value_get_uint (value);
      g_assert (self->width > 0);
      break;

    case PROP_BACKGROUND_PAINTABLE:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_touchscreen_content_class_init (BsTouchscreenContentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = bs_touchscreen_content_finalize;
  object_class->get_property = bs_touchscreen_content_get_property;
  object_class->set_property = bs_touchscreen_content_set_property;

  properties[PROP_BACKGROUND_PAINTABLE] =
    g_param_spec_object ("background-paintable", NULL, NULL,
                         GDK_TYPE_PAINTABLE,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_WIDTH] = g_param_spec_uint ("width", NULL, NULL,
                                              1, G_MAXUINT, 1,
                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_HEIGHT] = g_param_spec_uint ("height", NULL, NULL,
                                               1, G_MAXUINT, 1,
                                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[INVALIDATE_REGION] = g_signal_new ("invalidate-region",
                                             BS_TYPE_TOUCHSCREEN_CONTENT,
                                             G_SIGNAL_RUN_LAST,
                                             0, NULL, NULL, NULL,
                                             G_TYPE_NONE,
                                             1,
                                             GRAPHENE_TYPE_RECT);
}

static void
bs_touchscreen_content_init (BsTouchscreenContent *self)
{
}

BsTouchscreenContent *
bs_touchscreen_content_new (GListModel *slots,
                            uint32_t    width,
                            uint32_t    height)
{
  g_autoptr (BsTouchscreenContent) self = NULL;
  size_t n_slots;

  g_assert (width > 0);
  g_assert (height > 0);
  g_assert (G_IS_LIST_MODEL (slots));
  g_assert (g_type_is_a (g_list_model_get_item_type (slots), BS_TYPE_TOUCHSCREEN_SLOT));

  self = g_object_new (BS_TYPE_TOUCHSCREEN_CONTENT,
                       "width", width,
                       "height", height,
                       NULL);
  self->slots = g_object_ref (slots);

  n_slots = g_list_model_get_n_items (self->slots);

  for (size_t i = 0; i < n_slots; i++)
    {
      g_autoptr (BsTouchscreenSlot) slot = g_list_model_get_item (self->slots, i);
      g_signal_connect_object (slot, "invalidate-contents", G_CALLBACK (on_slot_invalidate_contents_cb), self, 0);
    }

  return g_steal_pointer (&self);
}

GdkPaintable *
bs_touchscreen_content_get_background_paintable (BsTouchscreenContent *self)
{
  g_return_val_if_fail (BS_IS_TOUCHSCREEN_CONTENT (self), NULL);

  return self->background.paintable;
}

void
bs_touchscreen_content_set_default_background (BsTouchscreenContent *self)
{
  g_assert (BS_IS_TOUCHSCREEN_CONTENT (self));

  switch (self->background.type)
    {
    case BACKGROUND_TYPE_DEFAULT:
      g_assert (self->background.d.file == NULL);
      break;

    case BACKGROUND_TYPE_FILE:
      g_assert (G_IS_FILE (self->background.d.file));
      g_clear_object (&self->background.d.file);
      break;
    }

  self->background.type = BACKGROUND_TYPE_DEFAULT;
  set_background_paintable (self, NULL);
}

void
bs_touchscreen_content_set_background_from_file (BsTouchscreenContent *self,
                                                 GFile                *file)
{
  g_autoptr (GdkPaintable) paintable = NULL;
  g_autoptr (GFileInfo) file_info = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (BS_IS_TOUCHSCREEN_CONTENT (self));
  g_assert (G_IS_FILE (file));

  file_info = g_file_query_info (file,
                                 G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                 G_FILE_QUERY_INFO_NONE,
                                 NULL,
                                 &error);

  if (file_info)
    {
      const char * const media_stream_content_types[] = {
        "image/gif",
        "video/*",
      };

      g_autoptr (GtkMediaStream) media_stream = NULL;
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

          paintable = GDK_PAINTABLE (g_steal_pointer (&media_stream));
          break;
        }
    }
  else
    {
      g_warning ("Error querying file info: %s", error->message);
    }

  if (!paintable)
    {
      g_autoptr (GdkTexture) texture = NULL;

      texture = gdk_texture_new_from_file (file, &error);
      if (!texture)
        return;

      paintable = GDK_PAINTABLE (g_steal_pointer (&texture));
    }

  self->background.type = BACKGROUND_TYPE_FILE;
  g_set_object (&self->background.d.file, file);

  set_background_paintable (self, paintable);
}

JsonNode *
bs_touchscreen_content_serialize (BsTouchscreenContent *self)
{
  g_autoptr (JsonBuilder) builder = NULL;

  g_assert (BS_IS_TOUCHSCREEN_CONTENT (self));

  builder = json_builder_new ();
  json_builder_begin_object (builder);

  switch (self->background.type)
    {
    case BACKGROUND_TYPE_DEFAULT:
      g_assert (self->background.d.file == NULL);

      json_builder_set_member_name (builder, "background-type");
      json_builder_add_string_value (builder, "default");
      break;

    case BACKGROUND_TYPE_FILE:
      g_assert (G_IS_FILE (self->background.d.file));

      json_builder_set_member_name (builder, "background-type");
      json_builder_add_string_value (builder, "file");

      json_builder_set_member_name (builder, "background-uri");
      json_builder_add_string_value (builder, g_file_get_uri (self->background.d.file));
      break;
    }

  json_builder_end_object (builder);

  return json_builder_get_root (builder);
}

void
bs_touchscreen_content_deserialize (BsTouchscreenContent *self,
                                    JsonNode             *node)
{
  const char *background_type;
  JsonObject *object;

  g_assert (JSON_NODE_HOLDS_OBJECT (node));

  object = json_node_get_object (node);

  background_type = json_object_get_string_member_with_default (object, "background-type", "default");
  if (g_strcmp0 (background_type, "file") == 0)
    {
      g_autoptr (GdkPaintable) paintable = NULL;
      g_autoptr (GdkTexture) texture = NULL;
      g_autoptr (GError) error = NULL;
      g_autoptr (GFile) file = NULL;
      const char *uri;

      g_assert (json_object_has_member (object, "background-uri"));
      uri = json_object_get_string_member (object, "background-uri");

      file = g_file_new_for_uri (uri);

      bs_touchscreen_content_set_background_from_file (self, file);
    }
  else
    {
      bs_touchscreen_content_set_default_background (self);
    }
}
