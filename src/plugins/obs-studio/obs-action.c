/* obs-action.c
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

#include "obs-action.h"
#include "obs-connection-settings.h"

typedef struct
{
  char *host;
  unsigned int port;

  ObsConnectionManager *connection_manager;
  ObsConnection *connection;
  gulong state_changed_id;
} ObsActionPrivate;

static void on_connection_state_changed_cb (ObsConnection      *connection,
                                            ObsConnectionState  old_state,
                                            ObsConnectionState  new_state,
                                            ObsAction          *self);

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ObsAction, obs_action, BS_TYPE_ACTION)

enum
{
  PROP_0,
  PROP_CONNECTION,
  PROP_CONNECTION_MANAGER,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];


/*
 * Auxiliary methods
 */

static void
update_icon_opacity_for_state (ObsAction          *self,
                               ObsConnectionState  state)
{
  BsIcon *icon = bs_action_get_icon (BS_ACTION (self));

  if (state != OBS_CONNECTION_STATE_CONNECTED)
    bs_icon_set_opacity (icon, 0.35);
  else
    bs_icon_set_opacity (icon, -1.0);
}

static void
update_connection (ObsAction *self)
{
  g_autoptr (ObsConnection) old_connection = NULL;
  g_autoptr (ObsConnection) new_connection = NULL;
  g_autoptr (GError) error = NULL;
  ObsActionPrivate *priv;

  priv = obs_action_get_instance_private (self);

  new_connection = obs_connection_manager_get (priv->connection_manager,
                                               priv->host,
                                               priv->port,
                                               &error);

  if (error)
    {
      g_warning ("Error updating connection: %s", error->message);
      return;
    }

  old_connection = g_steal_pointer (&priv->connection);

  OBS_ACTION_GET_CLASS (self)->update_connection (self, old_connection, new_connection);
}


/*
 * Callbacks
 */

static void
on_connection_state_changed_cb (ObsConnection      *connection,
                                ObsConnectionState  old_state,
                                ObsConnectionState  new_state,
                                ObsAction          *self)
{
  update_icon_opacity_for_state (self, new_state);
}

static void
on_connection_settings_connection_changed_cb (ObsConnectionSettings *connection_settings,
                                              GParamSpec            *pspec,
                                              ObsAction             *self)
{
  g_autoptr (ObsConnection) old_connection = NULL;
  ObsActionPrivate *priv;
  ObsConnection *new_connection;

  priv = obs_action_get_instance_private (self);
  old_connection = g_steal_pointer (&priv->connection);
  new_connection = obs_connection_settings_get_connection (connection_settings);

  OBS_ACTION_GET_CLASS (self)->update_connection (self, old_connection, new_connection);

  g_clear_pointer (&priv->host, g_free);
  priv->host = g_strdup (obs_connection_get_host (new_connection));
  priv->port = obs_connection_get_port (new_connection);

  bs_action_changed (BS_ACTION (self));
}


/*
 * ObsAction overrides
 */

static void
obs_action_real_add_extra_settings (ObsAction   *self,
                                    JsonBuilder *builder)
{
}

static void
obs_action_real_update_connection (ObsAction     *self,
                                   ObsConnection *old_connection,
                                   ObsConnection *new_connection)
{
  ObsActionPrivate *priv;

  priv = obs_action_get_instance_private (self);

  g_clear_signal_handler (&priv->state_changed_id, old_connection);
  g_set_object (&priv->connection, new_connection);

  priv->state_changed_id = g_signal_connect (new_connection,
                                             "state-changed",
                                             G_CALLBACK (on_connection_state_changed_cb),
                                             self);
  update_icon_opacity_for_state (self, obs_connection_get_state (new_connection));
}


/*
 * BsAction overrides
 */

static GtkWidget *
obs_action_get_preferences (BsAction *action)
{
  ObsAction *self = OBS_ACTION (action);
  ObsActionPrivate *priv = obs_action_get_instance_private (self);
  GtkWidget *connection_settings;

  connection_settings = obs_connection_settings_new (priv->connection_manager,
                                                     priv->connection);
  g_signal_connect (connection_settings,
                    "notify::connection",
                    G_CALLBACK (on_connection_settings_connection_changed_cb),
                    self);

  return connection_settings;
}

static JsonNode *
obs_action_serialize_settings (BsAction *action)
{
  g_autoptr (JsonBuilder) builder = NULL;
  ObsActionPrivate *priv;
  ObsAction *self;

  self = OBS_ACTION (action);
  priv = obs_action_get_instance_private (self);
  builder = json_builder_new ();

  json_builder_begin_object (builder);

  json_builder_set_member_name (builder, "host");
  json_builder_add_string_value (builder, priv->host);

  json_builder_set_member_name (builder, "port");
  json_builder_add_int_value (builder, priv->port);

  OBS_ACTION_GET_CLASS (self)->add_extra_settings (self, builder);

  json_builder_end_object (builder);

  return json_builder_get_root (builder);
}

static void
obs_action_deserialize_settings (BsAction   *action,
                                 JsonObject *settings)
{
  ObsAction *self = OBS_ACTION (action);
  ObsActionPrivate *priv = obs_action_get_instance_private (self);

  g_clear_pointer (&priv->host, g_free);

  priv->host = g_strdup (json_object_get_string_member_with_default (settings, "host", OBS_DEFAULT_URL));
  priv->port = json_object_get_int_member_with_default (settings, "port", OBS_DEFAULT_PORT);

  update_connection (self);
}


/*
 * GObject overrides
 */

static void
obs_action_finalize (GObject *object)
{
  ObsAction *self = (ObsAction *)object;
  ObsActionPrivate *priv = obs_action_get_instance_private (self);

  g_clear_signal_handler (&priv->state_changed_id, priv->connection);
  g_clear_object (&priv->connection_manager);
  g_clear_object (&priv->connection);

  G_OBJECT_CLASS (obs_action_parent_class)->finalize (object);
}

static void
obs_action_constructed (GObject *object)
{
  ObsAction *self = (ObsAction *)object;

  G_OBJECT_CLASS (obs_action_parent_class)->constructed (object);

  update_connection (self);
}

static void
obs_action_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  ObsAction *self = OBS_ACTION (object);
  ObsActionPrivate *priv = obs_action_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, priv->connection);
      break;

    case PROP_CONNECTION_MANAGER:
      g_value_set_object (value, priv->connection_manager);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
obs_action_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  ObsAction *self = OBS_ACTION (object);
  ObsActionPrivate *priv = obs_action_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_CONNECTION_MANAGER:
      g_assert (priv->connection_manager == NULL);
      priv->connection_manager = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
obs_action_class_init (ObsActionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  BsActionClass *action_class = BS_ACTION_CLASS (klass);

  object_class->finalize = obs_action_finalize;
  object_class->constructed = obs_action_constructed;
  object_class->get_property = obs_action_get_property;
  object_class->set_property = obs_action_set_property;

  action_class->get_preferences = obs_action_get_preferences;
  action_class->serialize_settings = obs_action_serialize_settings;
  action_class->deserialize_settings = obs_action_deserialize_settings;

  klass->add_extra_settings = obs_action_real_add_extra_settings;
  klass->update_connection = obs_action_real_update_connection;

  properties[PROP_CONNECTION] = g_param_spec_object ("connection", NULL, NULL,
                                                     OBS_TYPE_CONNECTION,
                                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_CONNECTION_MANAGER] = g_param_spec_object ("connection-manager", NULL, NULL,
                                                             OBS_TYPE_CONNECTION_MANAGER,
                                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
obs_action_init (ObsAction *self)
{
  ObsActionPrivate *priv = obs_action_get_instance_private (self);

  priv->host = g_strdup (OBS_DEFAULT_URL);
  priv->port = OBS_DEFAULT_PORT;
}

ObsConnection *
obs_action_get_connection (ObsAction *self)
{
  ObsActionPrivate *priv;

  g_return_val_if_fail (OBS_IS_ACTION (self), NULL);

  priv = obs_action_get_instance_private (self);
  return priv->connection;
}
