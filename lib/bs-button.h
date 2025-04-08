/* bs-button.h
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

#if !defined(BOATSWAIN_COMPILATION) && !defined (BOATSWAIN_INSIDE)
#error "Only <boatswain.h> can be included directly."
#endif

#include <gtk/gtk.h>

#include "bs-types.h"

G_BEGIN_DECLS

#define BS_TYPE_BUTTON (bs_button_get_type())
G_DECLARE_FINAL_TYPE (BsButton, bs_button, BS, BUTTON, GObject)

BsStreamDeck * bs_button_get_stream_deck (BsButton *self);

BsDeviceRegion * bs_button_get_region (BsButton *self);

uint8_t bs_button_get_position (BsButton *self);

gboolean bs_button_get_pressed (BsButton *self);

BsIcon * bs_button_get_icon (BsButton *self);

BsIcon * bs_button_get_custom_icon (BsButton *self);
void bs_button_set_custom_icon (BsButton *self,
                                BsIcon   *icon);

G_END_DECLS
