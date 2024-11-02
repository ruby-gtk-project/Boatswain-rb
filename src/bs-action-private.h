/* bs-action-private.h
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

void bs_action_set_id (BsAction   *self,
                       const char *id);

void bs_action_set_name (BsAction   *self,
                         const char *name);

BsActionFactory * bs_action_get_factory (BsAction *self);
void bs_action_set_factory (BsAction        *self,
                            BsActionFactory *factory);

JsonNode * bs_action_serialize_settings (BsAction *self);
void bs_action_deserialize_settings (BsAction   *self,
                                     JsonObject *settings);

void bs_action_handle_event (BsAction *self,
                             BsEvent  *event);

G_END_DECLS
