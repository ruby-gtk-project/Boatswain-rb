/*
 * bs-dial-grid.h
 *
 * Copyright 2024 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "bs-types.h"

#include "bs-device-region.h"
#include <gdk/gdk.h>

G_BEGIN_DECLS

#define BS_TYPE_DIAL_GRID (bs_dial_grid_get_type())
G_DECLARE_FINAL_TYPE (BsDialGrid, bs_dial_grid, BS, DIAL_GRID, BsDeviceRegion)

BsDialGrid * bs_dial_grid_new (const char   *id,
                               BsStreamDeck *stream_deck,
                               unsigned int  n_dials,
                               unsigned int  grid_columns,
                               unsigned int  column,
                               unsigned int  row,
                               unsigned int  column_span,
                               unsigned int  row_span);

GListModel * bs_dial_grid_get_dials (BsDialGrid *self);

unsigned int bs_dial_grid_get_grid_columns (BsDialGrid *self);

G_END_DECLS
