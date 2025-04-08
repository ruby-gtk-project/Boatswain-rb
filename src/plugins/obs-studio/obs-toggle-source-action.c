/* obs-toggle-source-action.c
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

#include "obs-connection-settings.h"
#include "obs-source.h"
#include "obs-toggle-source-action.h"

#include <glib/gi18n.h>

typedef enum
{
  BEHAVIOR_TOGGLE,
  BEHAVIOR_DISABLE,
  BEHAVIOR_ENABLE,
} Behavior;

struct _ObsToggleSourceAction
{
  ObsAction parent_instance;

  char *source_name;
  ObsSource *source;
  Behavior behavior;

  AdwComboRow *sources_row;

  ObsSourceCaps source_caps;
  GListModel *filtered_sources;

  gulong source_changed_id;
  gulong state_changed_id;
  guint frozen_id;
};

static void on_source_changed_cb (ObsSource             *source,
                                  GParamSpec            *pspec,
                                  ObsToggleSourceAction *self);

G_DEFINE_FINAL_TYPE (ObsToggleSourceAction, obs_toggle_source_action, OBS_TYPE_ACTION)

enum
{
  PROP_0,
  PROP_SOURCE_CAPS,
  N_PROPS,
};

static GParamSpec *properties [N_PROPS];


/*
 * Auxiliary methods
 */

static gboolean
parse_behavior_from_string (const char *string,
                            Behavior     *out_behavior)
{
  if (g_strcmp0 (string, "toggle") == 0)
    {
      *out_behavior = BEHAVIOR_TOGGLE;
      return TRUE;
    }
  else if (g_strcmp0 (string, "disable") == 0)
    {
      *out_behavior = BEHAVIOR_DISABLE;
      return TRUE;
    }
  else if (g_strcmp0 (string, "enable") == 0)
    {
      *out_behavior = BEHAVIOR_ENABLE;
      return TRUE;
    }

  return FALSE;
}

static const char *
behavior_to_string (Behavior action)
{
  switch (action)
    {
    case BEHAVIOR_TOGGLE:
      return "toggle";

    case BEHAVIOR_DISABLE:
      return "disable";

    case BEHAVIOR_ENABLE:
      return "enable";

    default:
      return NULL;
    }
}

static GListModel *
get_action_model_for_caps (ObsSourceCaps caps)
{
  g_autoptr (GtkStringList) string_list = NULL;

  string_list = gtk_string_list_new (NULL);

  if ((caps & OBS_SOURCE_CAP_AUDIO) == OBS_SOURCE_CAP_AUDIO)
    {
      gtk_string_list_append (string_list, _("Toggle mute"));
      gtk_string_list_append (string_list, _("Mute"));
      gtk_string_list_append (string_list, _("Unmute"));
    }
  else
    {
      gtk_string_list_append (string_list, _("Toggle visibility"));
      gtk_string_list_append (string_list, _("Hide"));
      gtk_string_list_append (string_list, _("Show"));
    }

  return G_LIST_MODEL (g_steal_pointer (&string_list));
}

static void
update_icon (ObsToggleSourceAction *self)
{
  BsIcon *icon = bs_action_get_icon (BS_ACTION (self));

  if (!self->source)
    {
      if (self->source_caps & OBS_SOURCE_CAP_AUDIO)
        bs_icon_set_icon_name (icon, "audio-volume-high-symbolic");
      else
        bs_icon_set_icon_name (icon, "eye-open-negative-filled-symbolic");
      return;
    }

  switch (obs_source_get_source_type (self->source))
    {
    case OBS_SOURCE_TYPE_AUDIO:
      if (obs_source_get_muted (self->source))
        bs_icon_set_icon_name (icon, "audio-volume-muted-symbolic");
      else
        bs_icon_set_icon_name (icon, "audio-volume-high-symbolic");
      break;

    case OBS_SOURCE_TYPE_MICROPHONE:
      if (obs_source_get_muted (self->source))
        bs_icon_set_icon_name (icon, "microphone-disabled-symbolic");
      else
        bs_icon_set_icon_name (icon, "audio-input-microphone-symbolic");
      break;

    case OBS_SOURCE_TYPE_VIDEO:
      if (obs_source_get_visible (self->source))
        bs_icon_set_icon_name (icon, "eye-open-negative-filled-symbolic");
      else
        bs_icon_set_icon_name (icon, "eye-not-looking-symbolic");
      break;

    case OBS_SOURCE_TYPE_TRANSITION:
    case OBS_SOURCE_TYPE_FILTER:
    case OBS_SOURCE_TYPE_UNKNOWN:
    default:
      break;
    }
}

static void
set_source (ObsToggleSourceAction *self,
            ObsSource             *source)
{
  g_clear_signal_handler (&self->source_changed_id, self->source);

  g_set_object (&self->source, source);

  if (source)
    {
      self->source_changed_id = g_signal_connect (source,
                                                  "notify",
                                                  G_CALLBACK (on_source_changed_cb),
                                                  self);
    }

  update_icon (self);
}

static gboolean
find_item_in_model (GListModel   *model,
                    gpointer      item,
                    unsigned int *out_position)
{
  for (unsigned int i = 0; i < g_list_model_get_n_items (model); i++)
    {
      g_autoptr (GObject) model_item = g_list_model_get_item (model, i);

      if (model_item == item)
        {
          *out_position = i;
          return TRUE;
        }
    }


  return FALSE;
}

static void
find_source_from_model (ObsToggleSourceAction *self)
{
  ObsConnection *connection;
  unsigned int i = 0;

  g_clear_signal_handler (&self->source_changed_id, self->source);
  g_clear_object (&self->source);

  connection = obs_action_get_connection (OBS_ACTION (self));
  if (obs_connection_get_state (connection) != OBS_CONNECTION_STATE_CONNECTED)
    goto out;

  for (i = 0; i < g_list_model_get_n_items (self->filtered_sources); i++)
    {
      g_autoptr (ObsSource) source = g_list_model_get_item (self->filtered_sources, i);

      if (g_strcmp0 (obs_source_get_name (source), self->source_name) == 0)
        {
          set_source (self, source);
          break;
        }
    }

out:
  if (self->sources_row && self->source)
    adw_combo_row_set_selected (self->sources_row, i);
}

static void
set_source_name (ObsToggleSourceAction *self,
                 const char            *source_name)
{
  if (g_strcmp0 (self->source_name, source_name) == 0)
    return;

  g_clear_pointer (&self->source_name, g_free);
  self->source_name = g_strdup (source_name);

  bs_icon_set_text (bs_action_get_icon (BS_ACTION (self)), self->source_name);
}


/*
 * Callbacks
 */

static gboolean
filter_source_caps_cb (gpointer item,
                       gpointer user_data)
{
  ObsToggleSourceAction *self = user_data;
  ObsSource *source = item;

  return (obs_source_get_caps (source) & self->source_caps) != 0;
}

static void
on_action_row_selected_changed_cb (AdwComboRow           *action_row,
                                   GParamSpec            *pspec,
                                   ObsToggleSourceAction *self)
{
  self->behavior = adw_combo_row_get_selected (action_row);
  bs_action_changed (BS_ACTION (self));
}

static void
on_connection_state_changed_cb (ObsConnection         *connection,
                                ObsConnectionState     old_state,
                                ObsConnectionState     new_state,
                                ObsToggleSourceAction *self)
{
  switch (new_state)
    {
    case OBS_CONNECTION_STATE_CONNECTED:
      find_source_from_model (self);
      break;

    case OBS_CONNECTION_STATE_DISCONNECTED:
    case OBS_CONNECTION_STATE_CONNECTING:
    case OBS_CONNECTION_STATE_AUTHENTICATING:
    case OBS_CONNECTION_STATE_WAITING_FOR_CREDENTIALS:
    default:
      break;
    }
}

static gboolean
unfreeze_cb (gpointer data)
{
  ObsToggleSourceAction *self = OBS_TOGGLE_SOURCE_ACTION (data);

  find_source_from_model (self);

  self->frozen_id = 0;
  return G_SOURCE_REMOVE;
}

static void
on_connection_sources_items_changed_cb (GListModel            *list,
                                        unsigned int           position,
                                        unsigned int           removed,
                                        unsigned int           added,
                                        ObsToggleSourceAction *self)
{
  if (self->frozen_id == 0)
    self->frozen_id = g_idle_add (unfreeze_cb, self);

  find_source_from_model (self);
}

static void
on_source_changed_cb (ObsSource             *source,
                      GParamSpec            *pspec,
                      ObsToggleSourceAction *self)
{
  update_icon (self);

  if (g_strcmp0 (g_param_spec_get_name (pspec), "name") == 0)
    {
      set_source_name (self, obs_source_get_name (self->source));
      bs_action_changed (BS_ACTION (self));
    }
}

static void
on_sources_row_selected_item_changed_cb (AdwComboRow           *sources_row,
                                         GParamSpec            *pspec,
                                         ObsToggleSourceAction *self)
{
  ObsConnection *connection;

  if (self->frozen_id > 0)
    return;

  connection = obs_action_get_connection (OBS_ACTION (self));
  if (obs_connection_get_state (connection) != OBS_CONNECTION_STATE_CONNECTED)
    return;

  set_source (self, adw_combo_row_get_selected_item (sources_row));

  if (self->source)
    {
      set_source_name (self, obs_source_get_name (self->source));
      bs_action_changed (BS_ACTION (self));
    }
}



/*
 * ObsAction overrides
 */

static void
obs_toggle_source_action_add_extra_settings (ObsAction   *obs_action,
                                             JsonBuilder *builder)
{
  ObsToggleSourceAction *self = OBS_TOGGLE_SOURCE_ACTION (obs_action);

  json_builder_set_member_name (builder, "behavior");
  json_builder_add_string_value (builder, behavior_to_string (self->behavior));

  if (self->source_name)
    {
      json_builder_set_member_name (builder, "source-name");
      json_builder_add_string_value (builder, self->source_name);
    }
}

static void
obs_toggle_source_action_update_connection (ObsAction     *obs_action,
                                            ObsConnection *old_connection,
                                            ObsConnection *new_connection)
{
  ObsToggleSourceAction *self = OBS_TOGGLE_SOURCE_ACTION (obs_action);

  OBS_ACTION_CLASS (obs_toggle_source_action_parent_class)->update_connection (obs_action,
                                                                               old_connection,
                                                                               new_connection);

  g_clear_signal_handler (&self->state_changed_id, old_connection);

  self->state_changed_id = g_signal_connect (new_connection,
                                             "state-changed",
                                             G_CALLBACK (on_connection_state_changed_cb),
                                             self);

  gtk_filter_list_model_set_model (GTK_FILTER_LIST_MODEL (self->filtered_sources),
                                   obs_connection_get_sources (new_connection));

  find_source_from_model (self);
}


/*
 * BsAction overrides
 */

static void
obs_toggle_source_action_handle_event (BsAction *action,
                                       BsEvent  *event)
{
  ObsToggleSourceAction *self = OBS_TOGGLE_SOURCE_ACTION (action);
  ObsConnection *connection;

  if (bs_event_get_event_type (event) != BS_BUTTON_PRESS)
    return;

  if (!self->source)
    return;

  connection = obs_action_get_connection (OBS_ACTION (self));
  if (obs_connection_get_state (connection) != OBS_CONNECTION_STATE_CONNECTED)
    return;

  switch (self->behavior)
    {
    case BEHAVIOR_TOGGLE:
      if (obs_source_get_caps (self->source) & OBS_SOURCE_CAP_AUDIO)
        obs_connection_toggle_source_mute (connection, self->source);
      if (obs_source_get_caps (self->source) & OBS_SOURCE_CAP_VIDEO)
        obs_connection_toggle_source_visible (connection, self->source);
      break;

    case BEHAVIOR_DISABLE:
      if (obs_source_get_caps (self->source) & OBS_SOURCE_CAP_AUDIO)
        obs_connection_set_source_mute (connection, self->source, TRUE);
      if (obs_source_get_caps (self->source) & OBS_SOURCE_CAP_VIDEO)
        obs_connection_set_source_visible (connection, self->source, FALSE);
      break;

    case BEHAVIOR_ENABLE:
      if (obs_source_get_caps (self->source) & OBS_SOURCE_CAP_AUDIO)
        obs_connection_set_source_mute (connection, self->source, FALSE);
      if (obs_source_get_caps (self->source) & OBS_SOURCE_CAP_VIDEO)
        obs_connection_set_source_visible (connection, self->source, TRUE);
      break;
    }
}

static GtkWidget *
obs_toggle_source_action_get_preferences (BsAction *action)
{
  ObsToggleSourceAction *self = OBS_TOGGLE_SOURCE_ACTION (action);
  g_autoptr (GListModel) action_model = NULL;
  GtkWidget *connection_settings;
  GtkWidget *group;
  GtkWidget *box;
  GtkWidget *row;
  unsigned int position;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 18);

  group = adw_preferences_group_new ();
  gtk_box_append (GTK_BOX (box), group);

  /* Action */
  action_model = get_action_model_for_caps (self->source_caps);

  row = adw_combo_row_new ();
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), _("Behavior"));
  adw_combo_row_set_model (ADW_COMBO_ROW (row), action_model);
  adw_combo_row_set_selected (ADW_COMBO_ROW (row), self->behavior);
  adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

  g_signal_connect (row, "notify::selected", G_CALLBACK (on_action_row_selected_changed_cb), self);

  /* Source */
  self->sources_row = ADW_COMBO_ROW (adw_combo_row_new ());

  if ((self->source_caps & OBS_SOURCE_CAP_AUDIO) == OBS_SOURCE_CAP_AUDIO)
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->sources_row), _("Audio Source"));
  else
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->sources_row), _("Source"));
  adw_combo_row_set_expression (self->sources_row,
                                gtk_property_expression_new (OBS_TYPE_SOURCE, NULL, "name"));
  adw_combo_row_set_model (self->sources_row, self->filtered_sources);
  adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), GTK_WIDGET (self->sources_row));

  if (self->source && find_item_in_model (self->filtered_sources, self->source, &position))
    adw_combo_row_set_selected (self->sources_row, position);

  g_object_add_weak_pointer (G_OBJECT (self->sources_row), (gpointer *) &self->sources_row);

  g_signal_connect (self->sources_row,
                    "notify::selected-item",
                    G_CALLBACK (on_sources_row_selected_item_changed_cb),
                    self);

  connection_settings = BS_ACTION_CLASS (obs_toggle_source_action_parent_class)->get_preferences (action);
  gtk_box_append (GTK_BOX (box), connection_settings);

  return box;
}

static void
obs_toggle_source_action_deserialize_settings (BsAction   *action,
                                               JsonObject *settings)
{
  ObsToggleSourceAction *self = OBS_TOGGLE_SOURCE_ACTION (action);

  BS_ACTION_CLASS (obs_toggle_source_action_parent_class)->deserialize_settings (action, settings);

  if (!parse_behavior_from_string (json_object_get_string_member_with_default (settings, "behavior", NULL),
                                   &self->behavior))
    self->behavior = BEHAVIOR_TOGGLE;

  set_source_name (self, json_object_get_string_member_with_default (settings, "source-name", NULL));
  find_source_from_model (self);
}


/*
 * GObject overrides
 */

static void
obs_toggle_source_action_finalize (GObject *object)
{
  ObsToggleSourceAction *self = (ObsToggleSourceAction *) object;
  ObsConnection *connection;

  g_clear_pointer (&self->source_name, g_free);
  g_clear_object (&self->filtered_sources);

  g_clear_signal_handler (&self->source_changed_id, self->source);
  g_clear_object (&self->source);

  connection = obs_action_get_connection (OBS_ACTION (self));
  g_clear_signal_handler (&self->state_changed_id, connection);
  g_clear_handle_id (&self->frozen_id, g_source_remove);

  G_OBJECT_CLASS (obs_toggle_source_action_parent_class)->finalize (object);
}

static void
obs_toggle_source_action_constructed (GObject *object)
{
  ObsToggleSourceAction *self = (ObsToggleSourceAction *)object;

  G_OBJECT_CLASS (obs_toggle_source_action_parent_class)->constructed (object);

  update_icon (self);
}

static void
obs_toggle_source_action_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  ObsToggleSourceAction *self = OBS_TOGGLE_SOURCE_ACTION (object);

  switch (prop_id)
    {
    case PROP_SOURCE_CAPS:
      g_value_set_int (value, self->source_caps);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
obs_toggle_source_action_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  ObsToggleSourceAction *self = OBS_TOGGLE_SOURCE_ACTION (object);

  switch (prop_id)
    {
    case PROP_SOURCE_CAPS:
      g_assert (self->source_caps == OBS_SOURCE_CAP_NONE);
      self->source_caps = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
obs_toggle_source_action_class_init (ObsToggleSourceActionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  BsActionClass *action_class = BS_ACTION_CLASS (klass);
  ObsActionClass *obs_action_class = OBS_ACTION_CLASS (klass);

  object_class->finalize = obs_toggle_source_action_finalize;
  object_class->constructed = obs_toggle_source_action_constructed;
  object_class->get_property = obs_toggle_source_action_get_property;
  object_class->set_property = obs_toggle_source_action_set_property;

  action_class->handle_event = obs_toggle_source_action_handle_event;
  action_class->get_preferences = obs_toggle_source_action_get_preferences;
  action_class->deserialize_settings = obs_toggle_source_action_deserialize_settings;

  obs_action_class->add_extra_settings = obs_toggle_source_action_add_extra_settings;
  obs_action_class->update_connection = obs_toggle_source_action_update_connection;

  properties[PROP_SOURCE_CAPS] = g_param_spec_int ("source-caps", NULL, NULL,
                                                   OBS_SOURCE_CAP_NONE, G_MAXINT, OBS_SOURCE_CAP_NONE,
                                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
obs_toggle_source_action_init (ObsToggleSourceAction *self)
{
  GtkFilter *filter;

  filter = GTK_FILTER (gtk_custom_filter_new (filter_source_caps_cb, self, NULL));

  self->behavior = BEHAVIOR_TOGGLE;
  self->source_caps = OBS_SOURCE_CAP_NONE;
  self->filtered_sources = G_LIST_MODEL (gtk_filter_list_model_new (NULL, filter));

  g_signal_connect (self->filtered_sources,
                    "items-changed",
                    G_CALLBACK (on_connection_sources_items_changed_cb),
                    self);
}

BsAction *
obs_toggle_source_action_new (ObsConnectionManager *connection_manager,
                              ObsSourceCaps         source_caps)
{
  return g_object_new (OBS_TYPE_TOGGLE_SOURCE_ACTION,
                       "connection-manager", connection_manager,
                       "source-caps", source_caps,
                       NULL);
}
