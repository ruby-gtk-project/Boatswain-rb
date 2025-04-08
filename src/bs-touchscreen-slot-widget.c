/*
 * bs-touchscreen-slot-widget.c
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

#include "bs-touchscreen-slot-widget.h"

struct _BsTouchscreenSlotWidget
{
  GtkFlowBoxChild parent_instance;

  GtkPicture *picture;

  BsTouchscreenSlot *slot;
};

G_DEFINE_FINAL_TYPE (BsTouchscreenSlotWidget, bs_touchscreen_slot_widget, GTK_TYPE_FLOW_BOX_CHILD)

enum {
  PROP_0,
  PROP_TOUCHSCREEN_SLOT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
bs_touchscreen_slot_widget_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  BsTouchscreenSlotWidget *self = BS_TOUCHSCREEN_SLOT_WIDGET (object);

  switch (prop_id)
    {
    case PROP_TOUCHSCREEN_SLOT:
      g_value_set_object (value, self->slot);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_touchscreen_slot_widget_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  BsTouchscreenSlotWidget *self = BS_TOUCHSCREEN_SLOT_WIDGET (object);

  switch (prop_id)
    {
    case PROP_TOUCHSCREEN_SLOT:
      g_assert (self->slot == NULL);
      self->slot = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_touchscreen_slot_widget_class_init (BsTouchscreenSlotWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = bs_touchscreen_slot_widget_get_property;
  object_class->set_property = bs_touchscreen_slot_widget_set_property;

  properties[PROP_TOUCHSCREEN_SLOT] =
    g_param_spec_object ("touchscreen-slot", NULL, NULL,
                         BS_TYPE_TOUCHSCREEN_SLOT,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/com/feaneron/Boatswain/bs-touchscreen-slot-widget.ui");

  gtk_widget_class_bind_template_child (widget_class, BsTouchscreenSlotWidget, picture);

  gtk_widget_class_set_css_name (widget_class, "touchscreenslotwidget");
}

static void
bs_touchscreen_slot_widget_init (BsTouchscreenSlotWidget *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
bs_touchscreen_slot_widget_new (BsTouchscreenSlot *slot)
{
  return g_object_new (BS_TYPE_TOUCHSCREEN_SLOT_WIDGET,
                       "touchscreen-slot", slot,
                       NULL);
}

BsTouchscreenSlot *
bs_touchscreen_slot_widget_get_slot (BsTouchscreenSlotWidget *self)
{
  g_assert (BS_IS_TOUCHSCREEN_SLOT_WIDGET (self));

  return self->slot;
}

