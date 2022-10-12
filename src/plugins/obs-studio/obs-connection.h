/* obs-connection.h
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

#pragma once

#include <gio/gio.h>

#include "obs-types.h"

G_BEGIN_DECLS


#define OBS_TYPE_CONNECTION (obs_connection_get_type())
G_DECLARE_FINAL_TYPE (ObsConnection, obs_connection, OBS, CONNECTION, GObject)

ObsConnection * obs_connection_new (const char   *host,
                                    unsigned int  port);

const char * obs_connection_get_host (ObsConnection *self);

unsigned int obs_connection_get_port (ObsConnection *self);

ObsConnectionState obs_connection_get_state (ObsConnection *self);
ObsRecordingState obs_connection_get_recording_state (ObsConnection *self);
gboolean obs_connection_get_streaming (ObsConnection *self);
gboolean obs_connection_get_virtualcam_enabled (ObsConnection *self);

void obs_connection_authenticate (ObsConnection *self,
                                  const char    *password);

GListModel * obs_connection_get_scenes (ObsConnection *self);
GListModel * obs_connection_get_sources (ObsConnection *self);

void obs_connection_switch_to_scene (ObsConnection *self,
                                     ObsScene      *scene);

void obs_connection_toggle_recording (ObsConnection *self);
void obs_connection_toggle_streaming (ObsConnection *self);
void obs_connection_toggle_virtualcam (ObsConnection *self);
void obs_connection_toggle_source_mute (ObsConnection *self,
                                        ObsSource     *source);
void obs_connection_set_source_mute (ObsConnection *self,
                                     ObsSource     *source,
                                     gboolean       mute);
void obs_connection_toggle_source_visible (ObsConnection *self,
                                           ObsSource     *source);
void obs_connection_set_source_visible (ObsConnection *self,
                                        ObsSource     *source,
                                        gboolean       visible);

G_END_DECLS
