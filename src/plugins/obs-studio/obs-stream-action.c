/* obs-stream-action.c
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
#include "obs-stream-action.h"

#include <glib/gi18n.h>

struct _ObsStreamAction
{
  ObsAction parent_instance;

  gulong streaming_changed_id;
};

G_DEFINE_FINAL_TYPE (ObsStreamAction, obs_stream_action, OBS_TYPE_ACTION)

/*
 * Auxiliary methods
 */

static void
update_streaming (ObsStreamAction *self)
{
  ObsConnection *connection;
  GdkRGBA color;
  BsIcon *icon;

  connection = obs_action_get_connection (OBS_ACTION (self));
  if (!connection)
    return;

  icon = bs_action_get_icon (BS_ACTION (self));

  gdk_rgba_parse (&color, "#1c71d8");

  if (obs_connection_get_streaming (connection))
    bs_icon_set_color (icon, &color);
  else
    bs_icon_set_color (icon, NULL);
}


/*
 * Callbacks
 */

static void
on_connection_streaming_changed_cb (ObsConnection   *connection,
                                    GParamSpec      *pspec,
                                    ObsStreamAction *self)
{
  update_streaming (self);
}


/*
 * ObsAction overrides
 */

static void
obs_stream_action_update_connection (ObsAction     *obs_action,
                                     ObsConnection *old_connection,
                                     ObsConnection *new_connection)
{
  ObsStreamAction *self = OBS_STREAM_ACTION (obs_action);

  OBS_ACTION_CLASS (obs_stream_action_parent_class)->update_connection (obs_action,
                                                                        old_connection,
                                                                        new_connection);

  g_clear_signal_handler (&self->streaming_changed_id, old_connection);
  self->streaming_changed_id = g_signal_connect (new_connection,
                                                 "notify::streaming",
                                                 G_CALLBACK (on_connection_streaming_changed_cb),
                                                 self);
  update_streaming (self);
}


/*
 * BsAction overrides
 */

static void
obs_stream_action_handle_event (BsAction *action,
                                BsEvent  *event)
{
  ObsConnection *connection;

  if (bs_event_get_event_type (event) != BS_BUTTON_PRESS)
    return;

  connection = obs_action_get_connection (OBS_ACTION (action));
  if (obs_connection_get_state (connection) != OBS_CONNECTION_STATE_CONNECTED)
    return;

  obs_connection_toggle_streaming (connection);
}


/*
 * GObject overrides
 */

static void
obs_stream_action_finalize (GObject *object)
{
  ObsStreamAction *self = (ObsStreamAction *)object;

  g_clear_signal_handler (&self->streaming_changed_id, obs_action_get_connection (OBS_ACTION (self)));

  G_OBJECT_CLASS (obs_stream_action_parent_class)->finalize (object);
}

static void
obs_stream_action_constructed (GObject *object)
{
  ObsStreamAction *self = (ObsStreamAction *)object;

  G_OBJECT_CLASS (obs_stream_action_parent_class)->constructed (object);

  update_streaming (self);
}

static void
obs_stream_action_class_init (ObsStreamActionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  BsActionClass *action_class = BS_ACTION_CLASS (klass);
  ObsActionClass *obs_action_class = OBS_ACTION_CLASS (klass);

  object_class->finalize = obs_stream_action_finalize;
  object_class->constructed = obs_stream_action_constructed;

  action_class->handle_event = obs_stream_action_handle_event;

  obs_action_class->update_connection = obs_stream_action_update_connection;
}

static void
obs_stream_action_init (ObsStreamAction *self)
{
  BsIcon *icon = bs_action_get_icon (BS_ACTION (self));

  bs_icon_set_icon_name (icon, "transmit-symbolic");
}

BsAction *
obs_stream_action_new (ObsConnectionManager *connection_manager)
{
  return g_object_new (OBS_TYPE_STREAM_ACTION,
                       "connection-manager", connection_manager,
                       NULL);
}
