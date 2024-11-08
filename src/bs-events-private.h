/*
 * bs-events-private.h
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

#include "bs-events.h"

G_BEGIN_DECLS

void bs_event_init_types_once (void);

/*
 * BsButtonEvent
 */

BsEvent * bs_button_event_new (BsEventType   event_type,
                               BsStreamDeck *device,
                               BsButton     *button);

/*
 * BsCursorEvent
 */

BsEvent * bs_cursor_event_new (BsEventType   event_type,
                               BsStreamDeck *device);

/*
 * BsDialEvent
 */

BsEvent * bs_dial_event_new (BsEventType   event_type,
                             BsStreamDeck *device,
                             BsDial       *dial,
                             int           rotation);

/*
 * BsTouchscreenEvent
 */

BsEvent * bs_touchscreen_event_new (BsEventType             event_type,
                                    BsStreamDeck           *device,
                                    BsTouchscreenSlot      *slot,
                                    const graphene_point_t *start,
                                    const graphene_point_t *end);

G_END_DECLS
