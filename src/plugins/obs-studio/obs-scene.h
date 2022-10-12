/* obs-scene.h
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

#include "obs-types.h"

G_BEGIN_DECLS

#define OBS_TYPE_SCENE (obs_scene_get_type())
G_DECLARE_FINAL_TYPE (ObsScene, obs_scene, OBS, SCENE, GObject)

ObsScene * obs_scene_new_from_json (ObsConnection *connection,
                                    JsonObject    *scene_object);

const char * obs_scene_get_uuid (ObsScene *self);
const char * obs_scene_get_name (ObsScene *self);
void obs_scene_set_name (ObsScene   *self,
                         const char *name);

G_END_DECLS
