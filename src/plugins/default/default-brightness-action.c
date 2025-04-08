/* default-brightness-action.c
 *
 * Copyright 2022 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "default-brightness-action.h"

#include <adwaita.h>
#include <glib/gi18n.h>

typedef enum
{
  MODE_SET_VALUE,
  MODE_INCREASE,
  MODE_DECREASE,
} BrightnessMode;

struct _DefaultBrightnessAction
{
  BsAction parent_instance;

  BrightnessMode mode;
  double value;
};

G_DEFINE_FINAL_TYPE (DefaultBrightnessAction, default_brightness_action, BS_TYPE_ACTION)

enum
{
  PROP_0,
  PROP_MODE,
  PROP_VALUE,
  N_PROPS,
};

static GParamSpec *properties[N_PROPS];


/*
 * Auxiliary methods
 */

static void
set_brightness_mode (DefaultBrightnessAction *self,
                     BrightnessMode           mode)
{
  if (self->mode == mode)
    return;

  self->mode = mode;

  switch (mode)
    {
    case MODE_SET_VALUE:
      bs_icon_set_icon_name (bs_action_get_icon (BS_ACTION (self)),
                             "display-brightness-symbolic");
      break;

    case MODE_INCREASE:
      bs_icon_set_icon_name (bs_action_get_icon (BS_ACTION (self)),
                             "daytime-sunrise-symbolic");
      break;

    case MODE_DECREASE:
      bs_icon_set_icon_name (bs_action_get_icon (BS_ACTION (self)),
                             "daytime-sunset-symbolic");
      break;
    }
  bs_action_changed (BS_ACTION (self));
}


/*
 * BsAction overrides
 */

static void
default_brightness_action_handle_event (BsAction *action,
                                        BsEvent  *event)
{
  DefaultBrightnessAction *self = DEFAULT_BRIGHTNESS_ACTION (action);
  BsDevice *device;
  double brightness;

  if (bs_event_get_event_type (event) != BS_BUTTON_PRESS)
    return;

  device = bs_event_get_device (event);
  brightness = bs_device_get_brightness (device);

  switch (self->mode)
    {
    case MODE_SET_VALUE:
      bs_device_set_brightness (device, self->value);
      break;

    case MODE_INCREASE:
      bs_device_set_brightness (device, CLAMP (brightness + self->value, 0.0, 1.0));
      break;

    case MODE_DECREASE:
      bs_device_set_brightness (device, CLAMP (brightness - self->value, 0.0, 1.0));
      break;
    }
}

static GtkWidget *
default_brightness_action_get_preferences (BsAction *action)
{
  g_autoptr (GtkStringList) strings = NULL;
  DefaultBrightnessAction *self;
  GtkAdjustment *adjustment;
  GtkWidget *listbox;
  GtkWidget *mode_row;
  GtkWidget *value_row;
  GtkWidget *spin;

  self = DEFAULT_BRIGHTNESS_ACTION (action);

  listbox = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (listbox), GTK_SELECTION_NONE);
  gtk_widget_add_css_class (listbox, "boxed-list");

  strings = gtk_string_list_new (NULL);
  /* Translators: "Constant" as in "Constant value of brightness" */
  gtk_string_list_append (strings, _("Constant"));
  gtk_string_list_append (strings, _("Increase"));
  gtk_string_list_append (strings, _("Decrease"));

  mode_row = adw_combo_row_new ();
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (mode_row), _("Mode"));
  adw_combo_row_set_model (ADW_COMBO_ROW (mode_row), G_LIST_MODEL (strings));
  adw_combo_row_set_selected (ADW_COMBO_ROW (mode_row), self->mode);
  g_object_bind_property (mode_row, "selected", self, "mode", G_BINDING_DEFAULT);
  gtk_list_box_append (GTK_LIST_BOX (listbox), mode_row);

  value_row = adw_action_row_new ();
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (value_row), _("Value"));
  gtk_list_box_append (GTK_LIST_BOX (listbox), value_row);

  spin = gtk_spin_button_new_with_range (0.01, 1.0, 0.01);
  gtk_widget_set_valign (spin, GTK_ALIGN_CENTER);

  adjustment = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (spin));
  gtk_adjustment_set_value (adjustment, self->value);
  g_object_bind_property (adjustment, "value", self, "value", G_BINDING_DEFAULT);
  adw_action_row_add_suffix (ADW_ACTION_ROW (value_row), spin);

  return listbox;
}

static JsonNode *
default_brightness_action_serialize_settings (BsAction *action)
{
  DefaultBrightnessAction *self = DEFAULT_BRIGHTNESS_ACTION (action);
  g_autoptr (JsonBuilder) builder = NULL;

  builder = json_builder_new ();

  json_builder_begin_object (builder);

  json_builder_set_member_name (builder, "mode");
  json_builder_add_int_value (builder, self->mode);

  json_builder_set_member_name (builder, "value");
  json_builder_add_double_value (builder, self->value);

  json_builder_end_object (builder);

  return json_builder_get_root (builder);
}

static void
default_brightness_action_deserialize_settings (BsAction   *action,
                                                JsonObject *object)
{
  DefaultBrightnessAction *self = DEFAULT_BRIGHTNESS_ACTION (action);
  BrightnessMode mode;

  self->value = json_object_get_double_member_with_default (object, "value", 0.01);
  mode = json_object_get_int_member_with_default (object, "mode", MODE_SET_VALUE);
  set_brightness_mode (self, mode);
}


/*
 * GObject overrides
 */

static void
default_brightness_action_constructed (GObject *object)
{
  DefaultBrightnessAction *self = (DefaultBrightnessAction *)object;

  G_OBJECT_CLASS (default_brightness_action_parent_class)->constructed (object);

  bs_icon_set_icon_name (bs_action_get_icon (BS_ACTION (self)),
                         "display-brightness-symbolic");
  set_brightness_mode (self, MODE_SET_VALUE);
}

static void
default_brightness_action_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  DefaultBrightnessAction *self = DEFAULT_BRIGHTNESS_ACTION (object);

  switch (prop_id)
    {
    case PROP_MODE:
      g_value_set_int (value, self->mode);
      break;

    case PROP_VALUE:
      g_value_set_double (value, self->value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
default_brightness_action_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  DefaultBrightnessAction *self = DEFAULT_BRIGHTNESS_ACTION (object);

  switch (prop_id)
    {
    case PROP_MODE:
      set_brightness_mode (self, g_value_get_int (value));
      break;

    case PROP_VALUE:
      self->value = g_value_get_double (value);
      bs_action_changed (BS_ACTION (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
default_brightness_action_class_init (DefaultBrightnessActionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  BsActionClass *action_class = BS_ACTION_CLASS (klass);

  object_class->constructed = default_brightness_action_constructed;
  object_class->get_property = default_brightness_action_get_property;
  object_class->set_property = default_brightness_action_set_property;

  action_class->handle_event = default_brightness_action_handle_event;
  action_class->get_preferences = default_brightness_action_get_preferences;
  action_class->serialize_settings = default_brightness_action_serialize_settings;
  action_class->deserialize_settings = default_brightness_action_deserialize_settings;

  properties[PROP_MODE] = g_param_spec_int ("mode", NULL, NULL,
                                            MODE_SET_VALUE, MODE_DECREASE, MODE_SET_VALUE,
                                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_VALUE] = g_param_spec_double ("value", NULL, NULL,
                                                0.01, 1.0, 0.01,
                                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
default_brightness_action_init (DefaultBrightnessAction *self)
{
  self->value = 0.01;
}

BsAction *
default_brightness_action_new (void)
{
  return g_object_new (DEFAULT_TYPE_BRIGHTNESS_ACTION, NULL);
}
