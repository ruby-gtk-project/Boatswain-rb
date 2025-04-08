/*
 * bs-dial-widget.c
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

#include "bs-dial-widget.h"

struct _BsDialWidget
{
  GtkWidget parent_instance;

  BsDial *dial;
};

G_DEFINE_FINAL_TYPE (BsDialWidget, bs_dial_widget, GTK_TYPE_WIDGET)

enum {
  PROP_0,
  PROP_DIAL,
  N_PROPS,
};

static GParamSpec *properties [N_PROPS];

static void
bs_dial_widget_finalize (GObject *object)
{
  BsDialWidget *self = (BsDialWidget *)object;

  g_clear_object (&self->dial);

  G_OBJECT_CLASS (bs_dial_widget_parent_class)->finalize (object);
}

static void
bs_dial_widget_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  BsDialWidget *self = BS_DIAL_WIDGET (object);

  switch (prop_id)
    {
    case PROP_DIAL:
      g_value_set_object (value, self->dial);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_dial_widget_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  BsDialWidget *self = BS_DIAL_WIDGET (object);

  switch (prop_id)
    {
    case PROP_DIAL:
      self->dial = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_dial_widget_class_init (BsDialWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = bs_dial_widget_finalize;
  object_class->get_property = bs_dial_widget_get_property;
  object_class->set_property = bs_dial_widget_set_property;

  properties[PROP_DIAL] = g_param_spec_object ("dial", NULL, NULL,
                                               BS_TYPE_DIAL,
                                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/com/feaneron/Boatswain/bs-dial-widget.ui");

  gtk_widget_class_set_css_name (widget_class, "streamdeckdialwidget");
}

static void
bs_dial_widget_init (BsDialWidget *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
bs_dial_widget_new (BsDial *dial)
{
  return g_object_new (BS_TYPE_DIAL_WIDGET,
                       "dial", dial,
                       NULL);
}

BsDial *
bs_dial_widget_get_dial (BsDialWidget *self)
{
  g_return_val_if_fail (BS_IS_DIAL_WIDGET (self), NULL);

  return self->dial;
}
