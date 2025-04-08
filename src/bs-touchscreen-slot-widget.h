/*
 * bs-touchscreen-slot-widget.h
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

#include <boatswain.h>

G_BEGIN_DECLS

#define BS_TYPE_TOUCHSCREEN_SLOT_WIDGET (bs_touchscreen_slot_widget_get_type())
G_DECLARE_FINAL_TYPE (BsTouchscreenSlotWidget, bs_touchscreen_slot_widget, BS, TOUCHSCREEN_SLOT_WIDGET, GtkFlowBoxChild)

GtkWidget * bs_touchscreen_slot_widget_new (BsTouchscreenSlot *slot);

BsTouchscreenSlot * bs_touchscreen_slot_widget_get_slot (BsTouchscreenSlotWidget *self);

G_END_DECLS
