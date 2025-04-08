/*
 * bs-dial.c
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

#include "bs-dial.h"

#include "bs-device.h"

struct _BsDial
{
  GObject parent_instance;

  gboolean pressed;

  BsDevice *device;
  uint8_t position;
};

G_DEFINE_FINAL_TYPE (BsDial, bs_dial, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_PRESSED,
  N_PROPS,
};

static GParamSpec *properties [N_PROPS];


/*
 * GObject overrides
 */

static void
bs_dial_get_property (GObject    *object,
                      guint       prop_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
  BsDial *self = BS_DIAL (object);

  switch (prop_id)
    {
    case PROP_PRESSED:
      g_value_set_boolean (value, self->pressed);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_dial_set_property (GObject      *object,
                      guint         prop_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
bs_dial_class_init (BsDialClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = bs_dial_get_property;
  object_class->set_property = bs_dial_set_property;

  properties[PROP_PRESSED] = g_param_spec_boolean ("pressed", NULL, NULL,
                                                   FALSE,
                                                   G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
bs_dial_init (BsDial *self)
{
}

BsDial *
bs_dial_new (BsDevice *device,
             uint8_t       position)
{
  g_autoptr (BsDial) self = NULL;

  self = g_object_new (BS_TYPE_DIAL, NULL);
  self->device = device;
  self->position = position;

  return g_steal_pointer (&self);
}

gboolean
bs_dial_get_pressed (BsDial *self)
{
  g_return_val_if_fail (BS_IS_DIAL (self), FALSE);

  return self->pressed;
}

void
bs_dial_set_pressed (BsDial   *self,
                     gboolean  pressed)
{
  g_assert (BS_IS_DIAL (self));

  if (self->pressed == pressed)
    return;

  self->pressed = pressed;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PRESSED]);
}
