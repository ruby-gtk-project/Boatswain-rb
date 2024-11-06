/* obs-action-factory.c
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

#include "bs-action-factory.h"
#include "bs-action-info.h"
#include "obs-action-factory.h"
#include "obs-connection-manager.h"
#include "obs-record-action.h"
#include "obs-stream-action.h"
#include "obs-switch-scene-action.h"
#include "obs-toggle-source-action.h"
#include "obs-virtualcam-action.h"

#include <glib/gi18n.h>

struct _ObsActionFactory
{
  BsActionFactory parent_instance;

  ObsConnectionManager *connection_manager;
};

G_DEFINE_FINAL_TYPE (ObsActionFactory, obs_action_factory, BS_TYPE_ACTION_FACTORY);

static const BsActionEntry entries[] = {
  {
    .id = "obs-switch-scene-action",
    .icon_name = "obs-switch-scene-symbolic",
    .name = N_("Switch Scene"),
    .description = NULL,
  },
  {
    .id = "obs-record-action",
    .icon_name = "media-record-symbolic",
    /* Translators: "Record" is a verb here */
    .name = N_("Record"),
    .description = NULL,
  },
  {
    .id = "obs-stream-action",
    .icon_name = "transmit-symbolic",
    /* Translators: "Stream" is a verb here */
    .name = N_("Stream"),
    .description = NULL,
  },
  {
    .id = "obs-virtualcam-action",
    .icon_name = "cameras-symbolic",
    .name = N_("Virtual Camera"),
    .description = NULL,
  },
  {
    .id = "obs-toggle-mute-action",
    .icon_name = "audio-volume-muted-symbolic",
    .name = N_("Toggle Mute"),
    .description = NULL,
  },
  {
    .id = "obs-toggle-source-action",
    .icon_name = "eye-not-looking-symbolic",
    .name = N_("Show / Hide Source"),
    .description = NULL,
  },
};

static BsAction *
obs_action_factory_create_action (BsActionFactory *action_factory,
                                  BsActionInfo    *action_info)
{
  ObsActionFactory *self = OBS_ACTION_FACTORY (action_factory);

  if (g_strcmp0 (bs_action_info_get_id (action_info), "obs-switch-scene-action") == 0)
    return obs_switch_scene_action_new (self->connection_manager);
  else if (g_strcmp0 (bs_action_info_get_id (action_info), "obs-record-action") == 0)
    return obs_record_action_new (self->connection_manager);
  else if (g_strcmp0 (bs_action_info_get_id (action_info), "obs-stream-action") == 0)
    return obs_stream_action_new (self->connection_manager);
  else if (g_strcmp0 (bs_action_info_get_id (action_info), "obs-virtualcam-action") == 0)
    return obs_virtualcam_action_new (self->connection_manager);
  else if (g_strcmp0 (bs_action_info_get_id (action_info), "obs-toggle-mute-action") == 0)
    return obs_toggle_source_action_new (self->connection_manager, OBS_SOURCE_CAP_AUDIO);
  else if (g_strcmp0 (bs_action_info_get_id (action_info), "obs-toggle-source-action") == 0)
    return obs_toggle_source_action_new (self->connection_manager, OBS_SOURCE_CAP_VIDEO);

  return NULL;
}

static void
obs_action_factory_finalize (GObject *object)
{
  ObsActionFactory *self = OBS_ACTION_FACTORY (object);

  g_clear_object (&self->connection_manager);

  G_OBJECT_CLASS (obs_action_factory_parent_class)->finalize (object);
}

static void
obs_action_factory_class_init (ObsActionFactoryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  BsActionFactoryClass *action_factory_class = BS_ACTION_FACTORY_CLASS (klass);

  object_class->finalize = obs_action_factory_finalize;

  action_factory_class->create_action = obs_action_factory_create_action;
}

static void
obs_action_factory_init (ObsActionFactory *self)
{
  self->connection_manager = obs_connection_manager_new ();

  bs_action_factory_add_action_entries (BS_ACTION_FACTORY (self),
                                        entries,
                                        G_N_ELEMENTS (entries));
}
