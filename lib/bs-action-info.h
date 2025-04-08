/*
 * bs-action-info.h
 *
 * Copyright 2023 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#if !defined(BOATSWAIN_COMPILATION) && !defined (BOATSWAIN_INSIDE)
#error "Only <boatswain.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define BS_TYPE_ACTION_INFO (bs_action_info_get_type())
G_DECLARE_FINAL_TYPE (BsActionInfo, bs_action_info, BS, ACTION_INFO, GObject)

BsActionInfo * bs_action_info_new (const char *id,
                                   const char *name,
                                   const char *description,
                                   const char *icon_name,
                                   gboolean    hidden);

const char * bs_action_info_get_id (BsActionInfo *self);
const char * bs_action_info_get_name (BsActionInfo *self);
const char * bs_action_info_get_description (BsActionInfo *self);
const char * bs_action_info_get_icon_name (BsActionInfo *self);
gboolean bs_action_info_get_hidden (BsActionInfo *self);

G_END_DECLS
