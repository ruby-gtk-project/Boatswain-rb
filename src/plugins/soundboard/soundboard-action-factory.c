/* soundboard-action-factory.c
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
#include "soundboard-action-factory.h"
#include "soundboard-mpris-action.h"
#include "soundboard-play-action.h"

#include <glib/gi18n.h>

struct _SoundboardActionFactory
{
  BsActionFactory parent_instance;

  MprisController *mpris_controller;
};

G_DEFINE_FINAL_TYPE (SoundboardActionFactory, soundboard_action_factory, BS_TYPE_ACTION_FACTORY);

static const BsActionEntry entries[] = {
  {
    .id = "soundboard-mpris-action",
    .icon_name = "audio-x-generic-symbolic",
    .name = N_("Music Player"),
    .description = N_("Control the active music player"),
  },
  {
    .id = "soundboard-play-action",
    .icon_name = "media-playback-start-symbolic",
    .name = N_("Play Audio"),
    .description = NULL,
  },
};


/*
 * BsActionFactory overrides
 */

static BsAction *
soundboard_action_factory_create_action (BsActionFactory *action_factory,
                                         BsActionInfo    *action_info)
{
  SoundboardActionFactory *self = SOUNDBOARD_ACTION_FACTORY (action_factory);

  if (g_strcmp0 (bs_action_info_get_id (action_info), "soundboard-play-action") == 0)
    return soundboard_play_action_new ();
  else if (g_strcmp0 (bs_action_info_get_id (action_info), "soundboard-mpris-action") == 0)
    return soundboard_mpris_action_new (self->mpris_controller);

  return NULL;
}


/*
 * GObject overrides
 */

static void
soundboard_action_factory_finalize (GObject *object)
{
  SoundboardActionFactory *self = SOUNDBOARD_ACTION_FACTORY (object);

  g_clear_object (&self->mpris_controller);

  G_OBJECT_CLASS (soundboard_action_factory_parent_class)->finalize (object);
}

static void
soundboard_action_factory_class_init (SoundboardActionFactoryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  BsActionFactoryClass *action_factory_class = BS_ACTION_FACTORY_CLASS (klass);

  object_class->finalize = soundboard_action_factory_finalize;

  action_factory_class->create_action = soundboard_action_factory_create_action;
}

static void
soundboard_action_factory_init (SoundboardActionFactory *self)
{
  self->mpris_controller = mpris_controller_new ();

  bs_action_factory_add_action_entries (BS_ACTION_FACTORY (self),
                                        entries,
                                        G_N_ELEMENTS (entries));
}
