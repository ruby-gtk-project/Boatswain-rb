/* obs-record-action.c
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
#include "obs-record-action.h"

#include <glib/gi18n.h>

struct _ObsRecordAction
{
  ObsAction parent_instance;

  gulong recording_state_changed_id;
};

static void on_connection_recording_state_changed_cb (ObsConnection   *connection,
                                                      GParamSpec      *pspec,
                                                      ObsRecordAction *self);

G_DEFINE_FINAL_TYPE (ObsRecordAction, obs_record_action, OBS_TYPE_ACTION)


/*
 * Auxiliary methods
 */

static void
update_recording_state (ObsRecordAction *self)
{
  ObsRecordingState recording_state;
  ObsConnection *connection;
  GdkRGBA color;
  BsIcon *icon;

  connection = obs_action_get_connection (OBS_ACTION (self));
  icon = bs_action_get_icon (BS_ACTION (self));

  recording_state = obs_connection_get_recording_state (connection);
  switch (recording_state)
    {
    case OBS_RECORDING_STATE_STOPPED:
      gdk_rgba_parse (&color, "#ffffff");
      bs_icon_set_icon_name (icon, "media-record-symbolic");
      break;

    case OBS_RECORDING_STATE_PAUSED:
      gdk_rgba_parse (&color, "#e5a50a");
      bs_icon_set_icon_name (icon, "media-playback-pause-symbolic");
      break;

    case OBS_RECORDING_STATE_RECORDING:
      gdk_rgba_parse (&color, "#e01b24");
      bs_icon_set_icon_name (icon, "media-record-symbolic");
      break;
    }

  if (obs_connection_get_state (connection) == OBS_CONNECTION_STATE_CONNECTED)
    bs_icon_set_color (icon, &color);
  else
    bs_icon_set_color (icon, NULL);
}


/*
 * Callbacks
 */

static void
on_connection_recording_state_changed_cb (ObsConnection   *connection,
                                          GParamSpec      *pspec,
                                          ObsRecordAction *self)
{
  update_recording_state (self);
}


/*
 * ObsAction overrides
 */

static void
obs_record_action_update_connection (ObsAction     *obs_action,
                                     ObsConnection *old_connection,
                                     ObsConnection *new_connection)
{
  ObsRecordAction *self = OBS_RECORD_ACTION (obs_action);

  OBS_ACTION_CLASS (obs_record_action_parent_class)->update_connection (obs_action,
                                                                        old_connection,
                                                                        new_connection);

  g_clear_signal_handler (&self->recording_state_changed_id, old_connection);
  self->recording_state_changed_id = g_signal_connect (new_connection,
                                                       "notify::recording-state",
                                                       G_CALLBACK (on_connection_recording_state_changed_cb),
                                                       self);
  update_recording_state (self);
}


/*
 * BsAction overrides
 */

static void
obs_record_action_handle_event (BsAction *action,
                                BsEvent  *event)
{
  ObsConnection *connection;

  if (bs_event_get_event_type (event) != BS_BUTTON_PRESS)
    return;

  connection = obs_action_get_connection (OBS_ACTION (action));
  if (obs_connection_get_state (connection) != OBS_CONNECTION_STATE_CONNECTED)
    return;

  obs_connection_toggle_recording (connection);
}


/*
 * GObject overrides
 */

static void
obs_record_action_finalize (GObject *object)
{
  ObsRecordAction *self = (ObsRecordAction *)object;

  g_clear_signal_handler (&self->recording_state_changed_id,
                          obs_action_get_connection (OBS_ACTION (self)));

  G_OBJECT_CLASS (obs_record_action_parent_class)->finalize (object);
}

static void
obs_record_action_constructed (GObject *object)
{
  ObsRecordAction *self = (ObsRecordAction *)object;

  G_OBJECT_CLASS (obs_record_action_parent_class)->constructed (object);

  update_recording_state (self);
}

static void
obs_record_action_class_init (ObsRecordActionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  BsActionClass *action_class = BS_ACTION_CLASS (klass);
  ObsActionClass *obs_action_class = OBS_ACTION_CLASS (klass);

  object_class->finalize = obs_record_action_finalize;
  object_class->constructed = obs_record_action_constructed;

  action_class->handle_event = obs_record_action_handle_event;

  obs_action_class->update_connection = obs_record_action_update_connection;
}

static void
obs_record_action_init (ObsRecordAction *self)
{
  BsIcon *icon = bs_action_get_icon (BS_ACTION (self));

  bs_icon_set_icon_name (icon, "media-record-symbolic");
}

BsAction *
obs_record_action_new (ObsConnectionManager *connection_manager)
{
  return g_object_new (OBS_TYPE_RECORD_ACTION,
                       "connection-manager", connection_manager,
                       NULL);
}
