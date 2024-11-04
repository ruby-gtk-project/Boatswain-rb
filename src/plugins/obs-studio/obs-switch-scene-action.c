/* obs-switch-scene-action.c
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

#include "bs-events.h"
#include "bs-icon.h"
#include "obs-connection-settings.h"
#include "obs-scene.h"
#include "obs-switch-scene-action.h"

#include <glib/gi18n.h>

struct _ObsSwitchSceneAction
{
  ObsAction parent_instance;

  char *scene_name;
  ObsScene *scene;

  AdwComboRow *scenes_row;

  gulong scene_name_changed_id;
  gulong scenes_changed_id;
  gulong state_changed_id;
  guint frozen_id;
};

static void on_scene_name_changed_cb (ObsScene             *scene,
                                      GParamSpec           *pspec,
                                      ObsSwitchSceneAction *self);

G_DEFINE_FINAL_TYPE (ObsSwitchSceneAction, obs_switch_scene_action, OBS_TYPE_ACTION)


/*
 * Auxiliary methods
 */

static void
set_scene (ObsSwitchSceneAction *self,
           ObsScene             *scene)
{
  g_clear_signal_handler (&self->scene_name_changed_id, self->scene);

  g_set_object (&self->scene, scene);

  if (scene)
    {
      self->scene_name_changed_id = g_signal_connect (scene,
                                                      "notify::name",
                                                      G_CALLBACK (on_scene_name_changed_cb),
                                                      self);
    }
}

static void
find_scene_from_model (ObsSwitchSceneAction *self)
{
  ObsConnection *connection;
  GListModel *scenes;
  unsigned int i = 0;

  g_clear_signal_handler (&self->scene_name_changed_id, self->scene);
  g_clear_object (&self->scene);

  connection = obs_action_get_connection (OBS_ACTION (self));
  if (obs_connection_get_state (connection) != OBS_CONNECTION_STATE_CONNECTED)
    goto out;

  scenes = obs_connection_get_scenes (connection);
  for (i = 0; i < g_list_model_get_n_items (scenes); i++)
    {
      g_autoptr (ObsScene) scene = g_list_model_get_item (scenes, i);

      if (g_strcmp0 (obs_scene_get_name (scene), self->scene_name) == 0)
        {
          set_scene (self, scene);
          break;
        }
    }

out:
  if (self->scenes_row)
    adw_combo_row_set_selected (self->scenes_row, self->scene ? i : GTK_INVALID_LIST_POSITION);
}

static void
set_scene_name (ObsSwitchSceneAction *self,
                const char           *scene_name)
{
  if (g_strcmp0 (self->scene_name, scene_name) == 0)
    return;

  g_clear_pointer (&self->scene_name, g_free);
  self->scene_name = g_strdup (scene_name);

  bs_icon_set_text (bs_action_get_icon (BS_ACTION (self)), self->scene_name);
}


/*
 * Callbacks
 */

static void
on_connection_state_changed_cb (ObsConnection        *connection,
                                ObsConnectionState    old_state,
                                ObsConnectionState    new_state,
                                ObsSwitchSceneAction *self)
{
  switch (new_state)
    {
    case OBS_CONNECTION_STATE_CONNECTED:
      find_scene_from_model (self);
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
  ObsSwitchSceneAction *self = OBS_SWITCH_SCENE_ACTION (data);

  find_scene_from_model (self);

  self->frozen_id = 0;
  return G_SOURCE_REMOVE;
}

static void
on_connection_scenes_items_changed_cb (GListModel           *list,
                                       unsigned int          position,
                                       unsigned int          removed,
                                       unsigned int          added,
                                       ObsSwitchSceneAction *self)
{
  if (self->frozen_id == 0)
    self->frozen_id = g_idle_add (unfreeze_cb, self);

  find_scene_from_model (self);
}

static void
on_scene_name_changed_cb (ObsScene             *scene,
                          GParamSpec           *pspec,
                          ObsSwitchSceneAction *self)
{
  set_scene_name (self, obs_scene_get_name (self->scene));
  bs_action_changed (BS_ACTION (self));
}

static void
on_scenes_row_selected_item_changed_cb (AdwComboRow          *scenes_row,
                                        GParamSpec           *pspec,
                                        ObsSwitchSceneAction *self)
{
  ObsConnection *connection;

  if (self->frozen_id > 0)
    return;

  connection = obs_action_get_connection (OBS_ACTION (self));
  if (obs_connection_get_state (connection) != OBS_CONNECTION_STATE_CONNECTED)
    return;

  set_scene (self, adw_combo_row_get_selected_item (scenes_row));

  if (self->scene)
    {
      set_scene_name (self, obs_scene_get_name (self->scene));
      bs_action_changed (BS_ACTION (self));
    }
}


/*
 * ObsAction overrides
 */

static void
obs_switch_scene_action_add_extra_settings (ObsAction   *obs_action,
                                            JsonBuilder *builder)
{
  ObsSwitchSceneAction *self = OBS_SWITCH_SCENE_ACTION (obs_action);

  if (self->scene_name)
    {
      json_builder_set_member_name (builder, "scene-name");
      json_builder_add_string_value (builder, self->scene_name);
    }
}

static void
obs_switch_scene_action_update_connection (ObsAction     *obs_action,
                                           ObsConnection *old_connection,
                                           ObsConnection *new_connection)
{
  ObsSwitchSceneAction *self = OBS_SWITCH_SCENE_ACTION (obs_action);

  OBS_ACTION_CLASS (obs_switch_scene_action_parent_class)->update_connection (obs_action,
                                                                              old_connection,
                                                                              new_connection);

  if (old_connection)
    g_clear_signal_handler (&self->scenes_changed_id, obs_connection_get_scenes (old_connection));
  g_clear_signal_handler (&self->state_changed_id, old_connection);

  self->state_changed_id = g_signal_connect (new_connection,
                                             "state-changed",
                                             G_CALLBACK (on_connection_state_changed_cb),
                                             self);

  self->scenes_changed_id = g_signal_connect (obs_connection_get_scenes (new_connection),
                                              "items-changed",
                                              G_CALLBACK (on_connection_scenes_items_changed_cb),
                                              self);
  find_scene_from_model (self);
}


/*
 * BsAction overrides
 */

static void
obs_switch_scene_action_handle_event (BsAction *action,
                                      BsEvent  *event)
{
  ObsSwitchSceneAction *self = OBS_SWITCH_SCENE_ACTION (action);
  ObsConnection *connection;

  if (bs_event_get_event_type (event) != BS_BUTTON_PRESS)
    return;

  if (!self->scene)
    return;

  connection = obs_action_get_connection (OBS_ACTION (self));
  if (obs_connection_get_state (connection) != OBS_CONNECTION_STATE_CONNECTED)
    return;

  obs_connection_switch_to_scene (connection, self->scene);
}

static GtkWidget *
obs_switch_scene_action_get_preferences (BsAction *action)
{
  ObsSwitchSceneAction *self = OBS_SWITCH_SCENE_ACTION (action);
  ObsConnection *connection;
  GListModel *scenes;
  GtkWidget *connection_settings;
  GtkWidget *group;
  GtkWidget *box;
  unsigned int position;

  connection = obs_action_get_connection (OBS_ACTION (self));
  scenes = obs_connection_get_scenes (connection);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 18);

  group = adw_preferences_group_new ();
  gtk_box_append (GTK_BOX (box), group);

  self->scenes_row = ADW_COMBO_ROW (adw_combo_row_new ());
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->scenes_row), _("Scene"));
  adw_combo_row_set_expression (self->scenes_row,
                                gtk_property_expression_new (OBS_TYPE_SCENE, NULL, "name"));
  adw_combo_row_set_model (self->scenes_row, scenes);
  adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), GTK_WIDGET (self->scenes_row));

  if (self->scene && g_list_store_find (G_LIST_STORE (scenes), self->scene, &position))
    adw_combo_row_set_selected (self->scenes_row, position);

  g_object_add_weak_pointer (G_OBJECT (self->scenes_row), (gpointer *) &self->scenes_row);

  g_signal_connect (self->scenes_row,
                    "notify::selected-item",
                    G_CALLBACK (on_scenes_row_selected_item_changed_cb),
                    self);

  connection_settings = BS_ACTION_CLASS (obs_switch_scene_action_parent_class)->get_preferences (action);
  gtk_box_append (GTK_BOX (box), connection_settings);

  return box;
}

static void
obs_switch_scene_action_deserialize_settings (BsAction   *action,
                                              JsonObject *settings)
{
  ObsSwitchSceneAction *self = OBS_SWITCH_SCENE_ACTION (action);

  BS_ACTION_CLASS (obs_switch_scene_action_parent_class)->deserialize_settings (action, settings);

  set_scene_name (self, json_object_get_string_member_with_default (settings, "scene-name", NULL));
  find_scene_from_model (self);
}


/*
 * GObject overrides
 */

static void
obs_switch_scene_action_finalize (GObject *object)
{
  ObsSwitchSceneAction *self = (ObsSwitchSceneAction *)object;
  ObsConnection *connection;

  g_clear_signal_handler (&self->scene_name_changed_id, self->scene);
  g_clear_object (&self->scene);

  connection = obs_action_get_connection (OBS_ACTION (self));
  g_clear_signal_handler (&self->scenes_changed_id, obs_connection_get_scenes (connection));
  g_clear_signal_handler (&self->state_changed_id, connection);
  g_clear_handle_id (&self->frozen_id, g_source_remove);

  G_OBJECT_CLASS (obs_switch_scene_action_parent_class)->finalize (object);
}

static void
obs_switch_scene_action_class_init (ObsSwitchSceneActionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  BsActionClass *action_class = BS_ACTION_CLASS (klass);
  ObsActionClass *obs_action_class = OBS_ACTION_CLASS (klass);

  object_class->finalize = obs_switch_scene_action_finalize;

  action_class->handle_event = obs_switch_scene_action_handle_event;
  action_class->get_preferences = obs_switch_scene_action_get_preferences;
  action_class->deserialize_settings = obs_switch_scene_action_deserialize_settings;

  obs_action_class->add_extra_settings = obs_switch_scene_action_add_extra_settings;
  obs_action_class->update_connection = obs_switch_scene_action_update_connection;
}

static void
obs_switch_scene_action_init (ObsSwitchSceneAction *self)
{
  BsIcon *icon = bs_action_get_icon (BS_ACTION (self));

  bs_icon_set_icon_name (icon, "obs-scene-symbolic");
}

BsAction *
obs_switch_scene_action_new (BsButton             *button,
                             ObsConnectionManager *connection_manager)
{
  return g_object_new (OBS_TYPE_SWITCH_SCENE_ACTION,
                       "button", button,
                       "connection-manager", connection_manager,
                       NULL);
}
