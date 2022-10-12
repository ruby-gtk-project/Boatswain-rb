/* obs-connection-settings.c
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

struct _ObsConnectionSettings
{
  AdwPreferencesGroup parent_instance;

  GtkEditable *host_row;
  GtkEditable *password_row;
  GtkAdjustment *port_adjustment;

  guint update_connection_timeout_id;

  ObsConnectionManager *connection_manager;
  ObsConnection *connection;
  GSignalGroup *signal_group;
};

G_DEFINE_FINAL_TYPE (ObsConnectionSettings, obs_connection_settings, ADW_TYPE_PREFERENCES_GROUP)

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
update_password_row (ObsConnectionSettings *self)
{
  switch (obs_connection_get_state (self->connection))
    {
    case OBS_CONNECTION_STATE_CONNECTED:
      gtk_widget_remove_css_class (GTK_WIDGET (self->password_row), "error");
      G_GNUC_FALLTHROUGH;

    case OBS_CONNECTION_STATE_DISCONNECTED:
    case OBS_CONNECTION_STATE_CONNECTING:
      gtk_widget_set_visible (GTK_WIDGET (self->password_row), FALSE);
      break;

    case OBS_CONNECTION_STATE_AUTHENTICATING:
      break;

    case OBS_CONNECTION_STATE_WAITING_FOR_CREDENTIALS:
      gtk_widget_set_visible (GTK_WIDGET (self->password_row), TRUE);
      break;
    }
}

static void
set_connection (ObsConnectionSettings *self,
                ObsConnection         *connection)
{
  if (self->connection == connection)
    return;

  g_set_object (&self->connection, connection);
  g_signal_group_set_target (self->signal_group, connection);

  update_password_row (self);
}

static void
update_connection (ObsConnectionSettings *self)
{
  g_autoptr (ObsConnection) connection = NULL;
  g_autoptr (GError) error = NULL;

  connection = obs_connection_manager_get (self->connection_manager,
                                           gtk_editable_get_text (self->host_row),
                                           gtk_adjustment_get_value (self->port_adjustment),
                                           &error);

  if (error)
    {
      g_warning ("Error creating new connection: %s", error->message);
      return;
    }

  set_connection (self, connection);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CONNECTION]);
}


/*
 * Callbacks
 */

static void
on_connection_authentication_failed_cb (ObsConnection         *connection,
                                        ObsConnectionSettings *self)
{
  gtk_widget_add_css_class (GTK_WIDGET (self->password_row), "error");
}

static gboolean
update_connection_after_timeout_cb (gpointer data)
{
  ObsConnectionSettings *self = OBS_CONNECTION_SETTINGS (data);

  update_connection (self);

  self->update_connection_timeout_id = 0;
  return G_SOURCE_REMOVE;
}

static void
on_connection_state_changed_cb (ObsConnection         *connection,
                                ObsConnectionState     old_state,
                                ObsConnectionState     new_state,
                                ObsConnectionSettings *self)
{
  update_password_row (self);
}

static void
on_host_row_applied_cb (GtkEditable           *host_row,
                        ObsConnectionSettings *self)
{
  g_clear_handle_id (&self->update_connection_timeout_id, g_source_remove);
  update_connection (self);
}

static void
on_password_row_applied_cb (GtkEditable           *row,
                            ObsConnectionSettings *self)
{
  obs_connection_authenticate (self->connection,
                               gtk_editable_get_text (self->password_row));
}

static void
on_port_adjustment_value_changed_cb (GtkAdjustment         *adjustment,
                                     GParamSpec            *pspec,
                                     ObsConnectionSettings *self)
{
  g_clear_handle_id (&self->update_connection_timeout_id, g_source_remove);
  self->update_connection_timeout_id = g_timeout_add_seconds (1,
                                                              update_connection_after_timeout_cb,
                                                              self);
}


/*
 * GObject overrides
 */

static void
obs_connection_settings_finalize (GObject *object)
{
  ObsConnectionSettings *self = (ObsConnectionSettings *)object;

  if (self->update_connection_timeout_id > 0)
    update_connection (self);

  g_clear_handle_id (&self->update_connection_timeout_id, g_source_remove);
  g_clear_object (&self->connection_manager);
  g_clear_object (&self->connection);
  g_clear_object (&self->signal_group);

  G_OBJECT_CLASS (obs_connection_settings_parent_class)->finalize (object);
}

static void
obs_connection_settings_constructed (GObject *object)
{
  ObsConnectionSettings *self = (ObsConnectionSettings *)object;

  G_OBJECT_CLASS (obs_connection_settings_parent_class)->constructed (object);

  gtk_editable_set_text (self->host_row, obs_connection_get_host (self->connection));

  g_signal_handlers_block_by_func (self->port_adjustment, on_port_adjustment_value_changed_cb, self);
  gtk_adjustment_set_value (self->port_adjustment, obs_connection_get_port (self->connection));
  g_signal_handlers_unblock_by_func (self->port_adjustment, on_port_adjustment_value_changed_cb, self);
}

static void
obs_connection_settings_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ObsConnectionSettings *self = OBS_CONNECTION_SETTINGS (object);

  switch (prop_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, self->connection);
      break;

    case PROP_CONNECTION_MANAGER:
      g_value_set_object (value, self->connection_manager);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
obs_connection_settings_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ObsConnectionSettings *self = OBS_CONNECTION_SETTINGS (object);

  switch (prop_id)
    {
    case PROP_CONNECTION:
      set_connection (self, g_value_get_object (value));
      break;

    case PROP_CONNECTION_MANAGER:
      g_assert (self->connection_manager == NULL);
      self->connection_manager = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
obs_connection_settings_class_init (ObsConnectionSettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = obs_connection_settings_finalize;
  object_class->constructed = obs_connection_settings_constructed;
  object_class->get_property = obs_connection_settings_get_property;
  object_class->set_property = obs_connection_settings_set_property;

  properties[PROP_CONNECTION] = g_param_spec_object ("connection", NULL, NULL,
                                                     OBS_TYPE_CONNECTION,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  properties[PROP_CONNECTION_MANAGER] = g_param_spec_object ("connection-manager", NULL, NULL,
                                                             OBS_TYPE_CONNECTION_MANAGER,
                                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/com/feaneron/Boatswain/plugins/obs-studio/obs-connection-settings.ui");

  gtk_widget_class_bind_template_child (widget_class, ObsConnectionSettings, host_row);
  gtk_widget_class_bind_template_child (widget_class, ObsConnectionSettings, password_row);
  gtk_widget_class_bind_template_child (widget_class, ObsConnectionSettings, port_adjustment);

  gtk_widget_class_bind_template_callback (widget_class, on_host_row_applied_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_password_row_applied_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_port_adjustment_value_changed_cb);
}

static void
obs_connection_settings_init (ObsConnectionSettings *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->signal_group = g_signal_group_new (OBS_TYPE_CONNECTION);
  g_signal_group_connect (self->signal_group, "state-changed", G_CALLBACK (on_connection_state_changed_cb), self);
  g_signal_group_connect (self->signal_group, "authentication-failed", G_CALLBACK (on_connection_authentication_failed_cb), self);
}

GtkWidget *
obs_connection_settings_new (ObsConnectionManager *connection_manager,
                             ObsConnection        *connection)
{
  return g_object_new (OBS_TYPE_CONNECTION_SETTINGS,
                       "connection-manager", connection_manager,
                       "connection", connection,
                       NULL);
}

ObsConnection *
obs_connection_settings_get_connection (ObsConnectionSettings *self)
{
  g_return_val_if_fail (OBS_IS_CONNECTION_SETTINGS (self), NULL);

  return self->connection;
}
