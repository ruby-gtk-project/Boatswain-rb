/* bs-action.h
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

#include <adwaita.h>
#include <json-glib/json-glib.h>

#include "bs-types.h"

G_BEGIN_DECLS

#define BS_TYPE_ACTION (bs_action_get_type())
G_DECLARE_DERIVABLE_TYPE (BsAction, bs_action, BS, ACTION, GObject)

struct _BsActionClass
{
  GObjectClass parent_class;

  BsIcon * (*get_icon) (BsAction *self);

  void (*handle_event) (BsAction *self,
                        BsEvent  *event);

  GtkWidget * (*get_preferences) (BsAction *self);

  JsonNode * (*serialize_settings) (BsAction *self);
  void (*deserialize_settings) (BsAction   *self,
                                JsonObject *settings);
};

BsIcon * bs_action_get_icon (BsAction *self);
const char * bs_action_get_id (BsAction *self);
const char * bs_action_get_name (BsAction *self);
GtkWidget * bs_action_get_preferences (BsAction *self);
void bs_action_changed (BsAction *self);

G_END_DECLS
