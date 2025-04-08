/*
 * bs-device-region.c
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

#include "bs-device-region.h"

#include "bs-device.h"

typedef struct
{
  GObject parent_instance;

  unsigned int column;
  unsigned int column_span;
  unsigned int row;
  unsigned int row_span;

  BsDevice *device;
  char *id;
} BsDeviceRegionPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (BsDeviceRegion, bs_device_region, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_COLUMN,
  PROP_COLUMN_SPAN,
  PROP_ID,
  PROP_ROW,
  PROP_ROW_SPAN,
  PROP_STREAM_DECK,
  N_PROPS,
};

static GParamSpec *properties [N_PROPS];


/*
 * BsDeviceRegion overrides
 */

static BsRenderer *
bs_device_region_real_get_renderer (BsDeviceRegion *self)
{
  return NULL;
}

static JsonNode *
bs_device_region_real_serialize (BsDeviceRegion *self)
{
  g_autoptr (JsonBuilder) builder = NULL;

  builder = json_builder_new ();
  json_builder_begin_object (builder);
  json_builder_end_object (builder);

  return json_builder_get_root (builder);
}

static void
bs_device_region_real_deserialize (BsDeviceRegion *self,
                                   JsonNode       *node)
{
}


/*
 * GObject overrides
 */

static void
bs_device_region_finalize (GObject *object)
{
  BsDeviceRegion *self = (BsDeviceRegion *) object;
  BsDeviceRegionPrivate *priv = bs_device_region_get_instance_private (self);

  g_clear_pointer (&priv->id, g_free);

  G_OBJECT_CLASS (bs_device_region_parent_class)->finalize (object);
}

static void
bs_device_region_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  BsDeviceRegion *self = BS_DEVICE_REGION (object);
  BsDeviceRegionPrivate *priv = bs_device_region_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_COLUMN:
      g_value_set_uint (value, priv->column);
      break;

    case PROP_COLUMN_SPAN:
      g_value_set_uint (value, priv->column_span);
      break;

    case PROP_ID:
      g_value_set_string (value, priv->id);
      break;

    case PROP_ROW:
      g_value_set_uint (value, priv->row);
      break;

    case PROP_ROW_SPAN:
      g_value_set_uint (value, priv->row_span);
      break;

    case PROP_STREAM_DECK:
      g_value_set_object (value, priv->device);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_device_region_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  BsDeviceRegion *self = BS_DEVICE_REGION (object);
  BsDeviceRegionPrivate *priv = bs_device_region_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_COLUMN:
      priv->column = g_value_get_uint (value);
      break;

    case PROP_COLUMN_SPAN:
      priv->column_span = g_value_get_uint (value);
      break;

    case PROP_ID:
      priv->id = g_value_dup_string (value);
      break;

    case PROP_ROW:
      priv->row = g_value_get_uint (value);
      break;

    case PROP_ROW_SPAN:
      priv->row_span = g_value_get_uint (value);
      break;

    case PROP_STREAM_DECK:
      priv->device = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_device_region_class_init (BsDeviceRegionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = bs_device_region_finalize;
  object_class->get_property = bs_device_region_get_property;
  object_class->set_property = bs_device_region_set_property;

  klass->get_renderer = bs_device_region_real_get_renderer;
  klass->serialize = bs_device_region_real_serialize;
  klass->deserialize = bs_device_region_real_deserialize;

  properties[PROP_COLUMN] = g_param_spec_uint ("column", NULL, NULL,
                                               0, G_MAXUINT, 0,
                                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_COLUMN_SPAN] = g_param_spec_uint ("column-span", NULL, NULL,
                                                    1, G_MAXUINT, 1,
                                                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_ID] = g_param_spec_string ("id", NULL, NULL,
                                             NULL,
                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_ROW] = g_param_spec_uint ("row", NULL, NULL,
                                            0, G_MAXUINT, 0,
                                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_ROW_SPAN] = g_param_spec_uint ("row-span", NULL, NULL,
                                                 1, G_MAXUINT, 1,
                                                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_STREAM_DECK] = g_param_spec_object ("stream-deck", NULL, NULL,
                                                      BS_TYPE_DEVICE,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
bs_device_region_init (BsDeviceRegion *self)
{
  BsDeviceRegionPrivate *priv = bs_device_region_get_instance_private (self);

  priv->column_span = 1;
  priv->row_span = 1;
}

const char *
bs_device_region_get_id (BsDeviceRegion *self)
{
  BsDeviceRegionPrivate *priv;

  g_return_val_if_fail (BS_IS_DEVICE_REGION (self), NULL);

  priv = bs_device_region_get_instance_private (self);
  return priv->id;
}

unsigned int
bs_device_region_get_column (BsDeviceRegion *self)
{
  BsDeviceRegionPrivate *priv;

  g_return_val_if_fail (BS_IS_DEVICE_REGION (self), 0);

  priv = bs_device_region_get_instance_private (self);
  return priv->column;
}

unsigned int bs_device_region_get_column_span (BsDeviceRegion *self)
{
  BsDeviceRegionPrivate *priv;

  g_return_val_if_fail (BS_IS_DEVICE_REGION (self), 0);

  priv = bs_device_region_get_instance_private (self);
  return priv->column_span;
}

unsigned int
bs_device_region_get_row (BsDeviceRegion *self)
{
  BsDeviceRegionPrivate *priv;

  g_return_val_if_fail (BS_IS_DEVICE_REGION (self), 0);

  priv = bs_device_region_get_instance_private (self);
  return priv->row;
}

unsigned int
bs_device_region_get_row_span (BsDeviceRegion *self)
{
  BsDeviceRegionPrivate *priv;

  g_return_val_if_fail (BS_IS_DEVICE_REGION (self), 0);

  priv = bs_device_region_get_instance_private (self);
  return priv->row_span;
}

BsDevice *
bs_device_region_get_device (BsDeviceRegion *self)
{
  BsDeviceRegionPrivate *priv;

  g_return_val_if_fail (BS_IS_DEVICE_REGION (self), NULL);

  priv = bs_device_region_get_instance_private (self);
  return priv->device;
}

BsRenderer *
bs_device_region_get_renderer (BsDeviceRegion *self)
{
  g_return_val_if_fail (BS_IS_DEVICE_REGION (self), NULL);

  return BS_DEVICE_REGION_GET_CLASS (self)->get_renderer (self);
}

JsonNode *
bs_device_region_serialize (BsDeviceRegion *self)
{
  g_return_val_if_fail (BS_IS_DEVICE_REGION (self), NULL);

  return BS_DEVICE_REGION_GET_CLASS (self)->serialize (self);
}

void
bs_device_region_deserialize (BsDeviceRegion *self,
                              JsonNode       *node)
{
  g_return_if_fail (BS_IS_DEVICE_REGION (self));

  return BS_DEVICE_REGION_GET_CLASS (self)->deserialize (self, node);
}
