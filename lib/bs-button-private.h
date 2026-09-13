/* bs-button-private.h
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

#include "bs-button.h"
#include "bs-types.h"

#include <stdint.h>

G_BEGIN_DECLS

BsButton * bs_button_new (BsDevice       *device,
                          BsDeviceRegion *region,
                          uint8_t         position,
                          unsigned int    icon_width,
                          unsigned int    icon_height);

void bs_button_set_pressed (BsButton *self,
                            gboolean  pressed);

unsigned int bs_button_get_icon_width (BsButton *self);
unsigned int bs_button_get_icon_height (BsButton *self);

G_END_DECLS
