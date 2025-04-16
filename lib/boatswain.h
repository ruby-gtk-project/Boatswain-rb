/*
 * boatswain.h
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

#include <gtk/gtk.h>
#include <libpeas.h>

#define BOATSWAIN_INSIDE
#include "bs-actionable.h"
#include "bs-action.h"
#include "bs-action-factory.h"
#include "bs-action-info.h"
#include "bs-button.h"
#include "bs-button-grid.h"
#include "bs-context.h"
#include "bs-debug.h"
#include "bs-desktop-controller.h"
#include "bs-device.h"
#include "bs-device-manager.h"
#include "bs-device-region.h"
#include "bs-dial.h"
#include "bs-dial-grid.h"
#include "bs-empty-action.h"
#include "bs-events.h"
#include "bs-icon.h"
#include "bs-init.h"
#include "bs-page.h"
#include "bs-page-item.h"
#include "bs-profile.h"
#include "bs-touchscreen.h"
#include "bs-touchscreen-content.h"
#include "bs-touchscreen-slot.h"
#undef BOATSWAIN_INSIDE
