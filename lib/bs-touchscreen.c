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

#include "bs-device-private.h"
#include "bs-device-region.h"
#include "bs-debug.h"
#include "bs-renderer-private.h"
#include "bs-touchscreen.h"
#include "bs-touchscreen-content-private.h"
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

enum
{
  TOUCHSCREEN_SLOT_CHANGED,
  N_SIGNALS,
};

static guint signals[N_SIGNALS];
static GParamSpec *properties [N_PROPS];


/*
 * Callbacks
 */

static void
on_touchscreen_slot_action_changed_cb (BsTouchscreenSlot *slot,
                                       BsTouchscreen     *self)
{
  g_signal_emit (self, signals[TOUCHSCREEN_SLOT_CHANGED], 0, slot);
}

static void
on_region_invalidated_cb (BsTouchscreenContent  *content,
                          const graphene_rect_t *region,
                          BsTouchscreen         *self)
{
  BsDevice *device;

  device = bs_device_region_get_device (BS_DEVICE_REGION (self));

  if (!bs_device_is_initialized (device))
    return;

  bs_device_upload_touchscreen (device, self, region);
}


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

static JsonNode *
bs_touchscreen_serialize (BsDeviceRegion *region)
{
  BsTouchscreen *self = (BsTouchscreen *) region;
  g_autoptr (JsonBuilder) builder = NULL;

  g_assert (BS_IS_TOUCHSCREEN (self));

  builder = json_builder_new ();
  json_builder_begin_object (builder);

  json_builder_set_member_name (builder, "content");
  json_builder_add_value (builder, bs_touchscreen_content_serialize (self->content));

  json_builder_end_object (builder);

  return json_builder_get_root (builder);
}

static void
bs_touchscreen_deserialize (BsDeviceRegion *region,
                            JsonNode       *node)
{
  BsTouchscreen *self = (BsTouchscreen *) region;
  JsonObject *object;

  g_assert (BS_IS_TOUCHSCREEN (self));
  g_assert (JSON_NODE_HOLDS_OBJECT (node));

  object = json_node_get_object (node);

  if (json_object_has_member (object, "content"))
    bs_touchscreen_content_deserialize (self->content, json_object_get_member (object, "content"));
  else
    bs_touchscreen_content_set_default_background (self->content);
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
  device_region_class->serialize = bs_touchscreen_serialize;
  device_region_class->deserialize = bs_touchscreen_deserialize;

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

  signals[TOUCHSCREEN_SLOT_CHANGED] = g_signal_new ("touchscreen-slot-changed",
                                                    BS_TYPE_TOUCHSCREEN,
                                                    G_SIGNAL_RUN_LAST,
                                                    0, NULL, NULL, NULL,
                                                    G_TYPE_NONE,
                                                    1,
                                                    BS_TYPE_TOUCHSCREEN_SLOT);
}

static void
bs_touchscreen_init (BsTouchscreen *self)
{
  self->slots = g_list_store_new (BS_TYPE_TOUCHSCREEN_SLOT);
  self->width = 1;
  self->height = 1;
}

BsTouchscreen *
bs_touchscreen_new (const char        *id,
                    BsDevice          *device,
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
                       "stream-deck", device,
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

      g_signal_connect_object (slot,
                               "action-changed",
                               G_CALLBACK (on_touchscreen_slot_action_changed_cb),
                               self, 0);

      g_list_store_append (self->slots, slot);
    }

  self->content = bs_touchscreen_content_new (G_LIST_MODEL (self->slots), self->width, self->height);
  g_signal_connect (self->content, "invalidate-region", G_CALLBACK (on_region_invalidated_cb), self);

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
