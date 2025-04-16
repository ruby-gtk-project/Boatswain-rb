/*
 * bs-button-grid.c
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

#include "bs-button-grid.h"

#include "bs-button-private.h"
#include "bs-device.h"
#include "bs-device-region.h"
#include "bs-renderer-private.h"
#include "bs-device.h"

struct _BsButtonGrid
{
  BsDeviceRegion parent_instance;

  GListStore *buttons;
  BsRenderer *renderer;

  BsImageInfo image_info;
  unsigned int grid_columns;
};

G_DEFINE_FINAL_TYPE (BsButtonGrid, bs_button_grid, BS_TYPE_DEVICE_REGION)

enum {
  PROP_0,
  PROP_BUTTONS,
  PROP_GRID_COLUMNS,
  N_PROPS,
};

enum
{
  BUTTON_CHANGED,
  N_SIGNALS,
};

static guint signals[N_SIGNALS];
static GParamSpec *properties [N_PROPS];


/*
 * Callbacks
 */

static void
on_button_action_changed_cb (BsButton     *button,
                             BsButtonGrid *self)
{
  g_signal_emit (self, signals[BUTTON_CHANGED], 0, button);
}


/*
 * BsDeviceRegion overrides
 */

static BsRenderer *
bs_button_grid_get_renderer (BsDeviceRegion *region)
{
  BsButtonGrid *self = (BsButtonGrid *) region;

  g_assert (BS_IS_BUTTON_GRID (self));

  return self->renderer;
}

/*
 * GObject overrides
 */

static void
bs_button_grid_finalize (GObject *object)
{
  BsButtonGrid *self = (BsButtonGrid *)object;

  g_clear_object (&self->buttons);
  g_clear_object (&self->renderer);

  G_OBJECT_CLASS (bs_button_grid_parent_class)->finalize (object);
}

static void
bs_button_grid_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  BsButtonGrid *self = BS_BUTTON_GRID (object);

  switch (prop_id)
    {
    case PROP_BUTTONS:
      g_value_set_object (value, self->buttons);
      break;

    case PROP_GRID_COLUMNS:
      g_value_set_uint (value, self->grid_columns);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_button_grid_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  BsButtonGrid *self = BS_BUTTON_GRID (object);

  switch (prop_id)
    {
    case PROP_GRID_COLUMNS:
      self->grid_columns = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_button_grid_class_init (BsButtonGridClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  BsDeviceRegionClass *device_class = BS_DEVICE_REGION_CLASS (klass);

  object_class->finalize = bs_button_grid_finalize;
  object_class->get_property = bs_button_grid_get_property;
  object_class->set_property = bs_button_grid_set_property;

  device_class->get_renderer = bs_button_grid_get_renderer;

  properties[PROP_BUTTONS] = g_param_spec_object ("buttons", NULL, NULL,
                                                  G_TYPE_LIST_MODEL,
                                                  G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_GRID_COLUMNS] = g_param_spec_uint ("grid-columns", NULL, NULL,
                                                     1, G_MAXUINT, 1,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[BUTTON_CHANGED] = g_signal_new ("button-changed",
                                          BS_TYPE_BUTTON_GRID,
                                          G_SIGNAL_RUN_LAST,
                                          0, NULL, NULL, NULL,
                                          G_TYPE_NONE,
                                          1,
                                          BS_TYPE_BUTTON);
}

static void
bs_button_grid_init (BsButtonGrid *self)
{
  self->buttons = g_list_store_new (BS_TYPE_BUTTON);
  self->grid_columns = 1;
}

BsButtonGrid *
bs_button_grid_new (const char        *id,
                    BsDevice          *device,
                    const BsImageInfo *image_info,
                    unsigned int       n_buttons,
                    unsigned int       grid_columns,
                    unsigned int       column,
                    unsigned int       row,
                    unsigned int       column_span,
                    unsigned int       row_span)
{
  g_autoptr (BsButtonGrid) self = NULL;

  g_assert (BS_IS_DEVICE (device));
  g_assert (image_info != NULL);

  self = g_object_new (BS_TYPE_BUTTON_GRID,
                       "id", id,
                       "stream-deck", device,
                       "grid-columns", grid_columns,
                       "column", column,
                       "row", row,
                       "column-span", column_span,
                       "row-span", row_span,
                       NULL);

  /* TODO: make it a construct-only property */
  self->image_info = *image_info;
  self->renderer = bs_renderer_new (&self->image_info);

  for (unsigned int i = 0; i < n_buttons; i++)
    {
      g_autoptr (BsButton) button = NULL;

      button = bs_button_new (device,
                              BS_DEVICE_REGION (self),
                              i,
                              image_info->width,
                              image_info->height);

      g_signal_connect_object (button,
                               "action-changed",
                               G_CALLBACK (on_button_action_changed_cb),
                               self, 0);

      g_list_store_append (self->buttons, button);
    }

  return g_steal_pointer (&self);
}

GListModel *
bs_button_grid_get_buttons (BsButtonGrid *self)
{
  g_return_val_if_fail (BS_IS_BUTTON_GRID (self), NULL);

  return G_LIST_MODEL (self->buttons);
}

unsigned int
bs_button_grid_get_grid_columns (BsButtonGrid *self)
{
  g_return_val_if_fail (BS_IS_BUTTON_GRID (self), 0);

  return self->grid_columns;
}
