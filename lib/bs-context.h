/*
 * bs-context.h
 *
 * Copyright 2025 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include <glib-object.h>
#include <libportal/portal.h>

#include "bs-types.h"

G_BEGIN_DECLS

#define BS_TYPE_CONTEXT (bs_context_get_type())
G_DECLARE_FINAL_TYPE (BsContext, bs_context, BS, CONTEXT, GObject)

BsContext * bs_context_get_default (void);
GListModel * bs_context_get_devices (BsContext *self);
BsDesktopController * bs_context_get_desktop_controller (BsContext *self);
GListModel * bs_context_get_available_action_factories (BsContext *self);

G_END_DECLS
