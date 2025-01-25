/*
 * bs-touchscreen.c
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

#define G_LOG_DOMAIN "Touchscreen"

#include "bs-device-region.h"
#include "bs-debug.h"
#include "bs-renderer.h"
#include "bs-stream-deck-private.h"
#include "bs-touchscreen.h"
#include "bs-touchscreen-content.h"
#include "bs-touchscreen-slot-private.h"

#include <gio/gio.h>

struct _BsTouchscreen
{
  BsDeviceRegion parent_instance;

  uint32_t width;
  uint32_t height;
  GListStore *slots;

  BsTouchscreenContent *content;

  BsRenderer *renderer;
};

G_DEFINE_FINAL_TYPE (BsTouchscreen, bs_touchscreen, BS_TYPE_DEVICE_REGION)

enum {
  PROP_0,
  PROP_CONTENT,
  PROP_WIDTH,
  PROP_HEIGHT,
  N_PROPS,
};

static GParamSpec *properties [N_PROPS];


/*
 * BsDeviceRegion overrides
 */

static BsRenderer *
bs_touchscreen_get_renderer (BsDeviceRegion *region)
{
  BsTouchscreen *self = (BsTouchscreen *) region;

  g_assert (BS_IS_TOUCHSCREEN (self));

  return self->renderer;
}


/*
 * GObject overrides
 */

static void
bs_touchscreen_dispose (GObject *object)
{
  BsTouchscreen *self = (BsTouchscreen *) object;

  g_clear_object (&self->renderer);
  g_clear_object (&self->slots);

  G_OBJECT_CLASS (bs_touchscreen_parent_class)->dispose (object);
}

static void
bs_touchscreen_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  BsTouchscreen *self = BS_TOUCHSCREEN (object);

  switch (prop_id)
    {
    case PROP_CONTENT:
      g_value_set_object (value, self->content);
      break;

    case PROP_WIDTH:
      g_value_set_uint (value, self->width);
      break;

    case PROP_HEIGHT:
      g_value_set_uint (value, self->height);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_touchscreen_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
bs_touchscreen_class_init (BsTouchscreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  BsDeviceRegionClass *device_region_class = BS_DEVICE_REGION_CLASS (klass);

  object_class->dispose = bs_touchscreen_dispose;
  object_class->get_property = bs_touchscreen_get_property;
  object_class->set_property = bs_touchscreen_set_property;

  device_region_class->get_renderer = bs_touchscreen_get_renderer;

  properties[PROP_CONTENT] = g_param_spec_object ("content", NULL, NULL,
                                                  BS_TYPE_TOUCHSCREEN_CONTENT,
                                                  G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_WIDTH] = g_param_spec_uint ("width", NULL, NULL,
                                              1, G_MAXUINT, 1,
                                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_HEIGHT] = g_param_spec_uint ("height", NULL, NULL,
                                               1, G_MAXUINT, 1,
                                               G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
bs_touchscreen_init (BsTouchscreen *self)
{
  self->slots = g_list_store_new (BS_TYPE_TOUCHSCREEN_SLOT);
  self->width = 1;
  self->height = 1;
}

static void
on_contents_invalidated_cb (GdkPaintable  *paintable,
                            BsTouchscreen *self)
{
  g_autoptr (GError) error = NULL;
  BsStreamDeck *stream_deck;

  stream_deck = bs_device_region_get_stream_deck (BS_DEVICE_REGION (self));

  if (!bs_stream_deck_is_initialized (stream_deck))
    return;

  bs_stream_deck_upload_touchscreen (stream_deck, self, &error);

  if (error)
    g_warning ("Error updating Stream Deck touchscreen: %s", error->message);
}

BsTouchscreen *
bs_touchscreen_new (const char        *id,
                    BsStreamDeck      *stream_deck,
                    const BsImageInfo *image_info,
                    uint32_t           n_slots,
                    unsigned int       column,
                    unsigned int       row,
                    unsigned int       column_span,
                    unsigned int       row_span)
{
  g_autoptr (BsTouchscreen) self = NULL;

  self = g_object_new (BS_TYPE_TOUCHSCREEN,
                       "id", id,
                       "stream-deck", stream_deck,
                       "column", column,
                       "row", row,
                       "column-span", column_span,
                       "row-span", row_span,
                       NULL);

  self->width = image_info->width;
  self->height = image_info->height;
  self->renderer = bs_renderer_new (image_info);

  for (uint32_t i = 0; i < n_slots; i++)
    {
      g_autoptr (BsTouchscreenSlot) slot = NULL;

      slot = bs_touchscreen_slot_new (self, self->width / n_slots, self->height);

      g_list_store_append (self->slots, slot);
    }

  self->content = bs_touchscreen_content_new (G_LIST_MODEL (self->slots), self->width, self->height);
  g_signal_connect (self->content, "invalidate-contents", G_CALLBACK (on_contents_invalidated_cb), self);

  return g_steal_pointer (&self);
}

uint32_t
bs_touchscreen_get_width (BsTouchscreen *self)
{
  g_assert (BS_IS_TOUCHSCREEN (self));

  return self->width;
}

uint32_t
bs_touchscreen_get_height (BsTouchscreen *self)
{
  g_assert (BS_IS_TOUCHSCREEN (self));

  return self->height;
}

BsTouchscreenContent *
bs_touchscreen_get_content (BsTouchscreen *self)
{
  g_assert (BS_IS_TOUCHSCREEN (self));

  return self->content;
}

GListModel *
bs_touchscreen_get_slots (BsTouchscreen *self)
{
  g_assert (BS_IS_TOUCHSCREEN (self));

  return G_LIST_MODEL (self->slots);
}

BsTouchscreenSlot *
bs_touchscreen_pick_slot (BsTouchscreen          *self,
                          const graphene_point_t *point)
{
  g_autoptr (BsTouchscreenSlot) slot = NULL;
  size_t position;
  size_t n_slots;

  g_assert (BS_IS_TOUCHSCREEN (self));

  n_slots = g_list_model_get_n_items (G_LIST_MODEL (self->slots));
  position = floorf ((point->x / (float) self->width) * n_slots);
  g_assert (position >= 0 && position < n_slots);

  BS_TRACE_MSG ("Picking slot at %.0fx%.0f -> slot %lu", point->x, point->y, position);

  slot = g_list_model_get_item (G_LIST_MODEL (self->slots), position);

  return g_steal_pointer (&slot);
}

uint32_t
bs_touchscreen_get_slot_position (BsTouchscreen     *self,
                                  BsTouchscreenSlot *slot)
{
  uint32_t position;

  g_assert (BS_IS_TOUCHSCREEN (self));
  g_assert (BS_IS_TOUCHSCREEN_SLOT (slot));

  if (!g_list_store_find (self->slots, slot, &position))
    g_assert_not_reached ();

  return position;
}
