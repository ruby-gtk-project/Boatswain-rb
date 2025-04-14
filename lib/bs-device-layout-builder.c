/*
 * bs-device-layout-builder.c
 *
 * Copyright 2025 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "bs-button-grid.h"
#include "bs-device.h"
#include "bs-device-layout-builder-private.h"
#include "bs-dial-grid.h"
#include "bs-touchscreen-private.h"

struct _BsDeviceLayoutBuilder
{
  GObject parent_instance;

  BsDevice *device;

  GListStore *regions;
};

G_DEFINE_FINAL_TYPE (BsDeviceLayoutBuilder, bs_device_layout_builder, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPS,
};

static GParamSpec *properties [N_PROPS];

static void
bs_device_layout_builder_finalize (GObject *object)
{
  BsDeviceLayoutBuilder *self = (BsDeviceLayoutBuilder *)object;

  g_clear_object (&self->device);

  G_OBJECT_CLASS (bs_device_layout_builder_parent_class)->finalize (object);
}

static void
bs_device_layout_builder_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  BsDeviceLayoutBuilder *self = BS_DEVICE_LAYOUT_BUILDER (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, self->device);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_device_layout_builder_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  BsDeviceLayoutBuilder *self = BS_DEVICE_LAYOUT_BUILDER (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_assert (self->device == NULL);
      self->device = g_value_dup_object (value);
      g_assert (self->device != NULL && BS_IS_DEVICE (self->device));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_device_layout_builder_class_init (BsDeviceLayoutBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = bs_device_layout_builder_finalize;
  object_class->get_property = bs_device_layout_builder_get_property;
  object_class->set_property = bs_device_layout_builder_set_property;

  properties[PROP_DEVICE] = g_param_spec_object ("device", NULL, NULL,
                                                 BS_TYPE_DEVICE,
                                                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
bs_device_layout_builder_init (BsDeviceLayoutBuilder *self)
{
  self->regions = g_list_store_new (BS_TYPE_DEVICE_REGION);
}

BsDeviceLayoutBuilder *
bs_device_layout_builder_new (BsDevice *device)
{
  return g_object_new (BS_TYPE_DEVICE_LAYOUT_BUILDER,
                       "device", device,
                       NULL);
}

/**
 * bs_device_layout_builder_add_button_grid:
 */
void
bs_device_layout_builder_add_button_grid (BsDeviceLayoutBuilder *self,
                                          const char            *id,
                                          const BsImageInfo     *image_info,
                                          unsigned int           n_buttons,
                                          unsigned int           grid_columns,
                                          unsigned int           column,
                                          unsigned int           row,
                                          unsigned int           column_span,
                                          unsigned int           row_span)
{
  g_autoptr (BsButtonGrid) button_grid = NULL;

  g_return_if_fail (BS_IS_DEVICE_LAYOUT_BUILDER (self));
  g_return_if_fail (id != NULL);

  button_grid = bs_button_grid_new (id,
                                    self->device,
                                    image_info,
                                    n_buttons,
                                    grid_columns,
                                    column, row,
                                    column_span, row_span);

  g_list_store_append (self->regions, button_grid);
}

void
bs_device_layout_builder_add_dial_grid (BsDeviceLayoutBuilder *self,
                                        const char            *id,
                                        unsigned int           n_dials,
                                        unsigned int           grid_columns,
                                        unsigned int           column,
                                        unsigned int           row,
                                        unsigned int           column_span,
                                        unsigned int           row_span)
{
  g_autoptr (BsDialGrid) dial_grid = NULL;

  g_return_if_fail (BS_IS_DEVICE_LAYOUT_BUILDER (self));
  g_return_if_fail (id != NULL);

  dial_grid = bs_dial_grid_new (id,
                                self->device,
                                n_dials,
                                grid_columns,
                                column, row,
                                column_span, row_span);

  g_list_store_append (self->regions, dial_grid);
}

void
bs_device_layout_builder_add_touchscreen (BsDeviceLayoutBuilder *self,
                                          const char            *id,
                                          const BsImageInfo     *image_info,
                                          uint32_t               n_slots,
                                          unsigned int           column,
                                          unsigned int           row,
                                          unsigned int           column_span,
                                          unsigned int           row_span)
{
  g_autoptr (BsTouchscreen) touchscreen = NULL;

  g_return_if_fail (BS_IS_DEVICE_LAYOUT_BUILDER (self));
  g_return_if_fail (id != NULL);

  touchscreen = bs_touchscreen_new ("touchscreen",
                                    self->device,
                                    image_info,
                                    n_slots,
                                    column, row,
                                    column_span, row_span);

  g_list_store_append (self->regions, touchscreen);
}


/**
 * bs_device_layout_builder_build:
 *
 * Return: (transfer full):
 */
GListModel *
bs_device_layout_builder_build (BsDeviceLayoutBuilder *self)
{
  g_return_val_if_fail (BS_IS_DEVICE_LAYOUT_BUILDER (self), NULL);
  g_return_val_if_fail (self->regions, NULL);

  return G_LIST_MODEL (g_steal_pointer (&self->regions));
}
