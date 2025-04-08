/* obs-action.h
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

#include <boatswain.h>

#include "obs-connection-manager.h"

G_BEGIN_DECLS

#define OBS_TYPE_ACTION (obs_action_get_type())
G_DECLARE_DERIVABLE_TYPE (ObsAction, obs_action, OBS, ACTION, BsAction)

struct _ObsActionClass
{
  BsActionClass parent_class;

  void (*add_extra_settings) (ObsAction   *self,
                              JsonBuilder *builder);

  void (*update_connection) (ObsAction     *self,
                             ObsConnection *old_connection,
                             ObsConnection *new_connection);
};

ObsConnection * obs_action_get_connection (ObsAction *self);

G_END_DECLS
