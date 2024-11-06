/* soundboard-play-action.h
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

#include "bs-action.h"

G_BEGIN_DECLS

typedef enum
{
  SOUNDBOARD_PLAY_BEHAVIOR_PLAY_STOP,
  SOUNDBOARD_PLAY_BEHAVIOR_PLAY_OVERLAP,
  SOUNDBOARD_PLAY_BEHAVIOR_PLAY_RESTART,
  SOUNDBOARD_PLAY_BEHAVIOR_LOOP_STOP,
  SOUNDBOARD_PLAY_BEHAVIOR_PRESS_HOLD,
} SoundboardPlayBehavior;

#define SOUNDBOARD_TYPE_PLAY_ACTION (soundboard_play_action_get_type())
G_DECLARE_FINAL_TYPE (SoundboardPlayAction, soundboard_play_action, SOUNDBOARD, PLAY_ACTION, BsAction)

BsAction * soundboard_play_action_new (void);

void soundboard_play_action_set_file (SoundboardPlayAction *self,
                                      GFile                *file);

void soundboard_play_action_set_behavior (SoundboardPlayAction   *self,
                                          SoundboardPlayBehavior  behavior);

void soundboard_play_action_set_volume (SoundboardPlayAction *self,
                                        double                volume);

G_END_DECLS
