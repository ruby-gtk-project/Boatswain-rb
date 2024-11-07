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

#include <gtk/gtk.h>

struct _BsTouchscreenContent
{
  GObject parent_instance;

  struct {
    GdkPaintable *paintable;
    gulong content_invalidated_id;
  } background;

  GListModel *slots;
  uint32_t width;
  uint32_t height;
};

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

static GParamSpec *properties [N_PROPS];


/*
 * Callbacks
 */

static void
on_paintable_invalidate_contents_cb (GdkPaintable         *paintable,
                                     BsTouchscreenContent *self)
{
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
    case PROP_BACKGROUND_PAINTABLE:
      bs_touchscreen_content_set_background (self, g_value_get_object (value));
      break;

    case PROP_HEIGHT:
      self->height = g_value_get_uint (value);
      g_assert (self->height > 0);
      break;

    case PROP_WIDTH:
      self->width = g_value_get_uint (value);
      g_assert (self->width > 0);
      break;

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
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_WIDTH] = g_param_spec_uint ("width", NULL, NULL,
                                              1, G_MAXUINT, 1,
                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_HEIGHT] = g_param_spec_uint ("height", NULL, NULL,
                                               1, G_MAXUINT, 1,
                                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
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
      g_signal_connect_object (slot, "invalidate-contents", G_CALLBACK (on_paintable_invalidate_contents_cb), self, 0);
    }

  return g_steal_pointer (&self);
}

GdkPaintable *
bs_touchscreen_content_get_background (BsTouchscreenContent *self)
{
  g_return_val_if_fail (BS_IS_TOUCHSCREEN_CONTENT (self), NULL);

  return self->background.paintable;
}

void
bs_touchscreen_content_set_background (BsTouchscreenContent *self,
                                       GdkPaintable         *paintable)
{
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
                          G_CALLBACK (on_paintable_invalidate_contents_cb),
                          self);
    }

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BACKGROUND_PAINTABLE]);
}
