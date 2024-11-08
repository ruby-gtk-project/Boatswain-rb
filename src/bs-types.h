/* bs-types.h
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

#include <glib.h>

G_BEGIN_DECLS

typedef enum _BsImageFormat BsImageFormat;
typedef enum _BsRendererFlags BsRendererFlags;

typedef struct _BsActionable BsActionable;
typedef struct _BsAction BsAction;
typedef struct _BsActionFactory BsActionFactory;
typedef struct _BsActionInfo BsActionInfo;
typedef struct _BsApplication BsApplication;
typedef struct _BsButton BsButton;
typedef struct _BsButtonEvent BsButtonEvent;
typedef struct _BsButtonGridRegion BsButtonGridRegion;
typedef struct _BsCursorEvent BsCursorEvent;
typedef struct _BsDesktopController BsDesktopController;
typedef struct _BsDeviceEditor BsDeviceEditor;
typedef struct _BsDeviceManager BsDeviceManager;
typedef struct _BsDeviceRegion BsDeviceRegion;
typedef struct _BsDial BsDial;
typedef struct _BsDialEvent BsDialEvent;
typedef struct _BsEmptyAction BsEmptyAction;
typedef struct _BsEvent BsEvent;
typedef struct _BsIcon BsIcon;
typedef struct _BsImageInfo BsImageInfo;
typedef struct _BsPage BsPage;
typedef struct _BsPageItem BsPageItem;
typedef struct _BsProfile BsProfile;
typedef struct _BsRenderer BsRenderer;
typedef struct _BsSelectionController BsSelectionController;
typedef struct _BsStreamDeck BsStreamDeck;
typedef struct _BsTouchscreen BsTouchscreen;
typedef struct _BsTouchscreenEvent BsTouchscreenEvent;
typedef struct _BsTouchscreenContent BsTouchscreenContent;
typedef struct _BsTouchscreenRegion BsTouchscreenRegion;
typedef struct _BsTouchscreenSlot BsTouchscreenSlot;
typedef struct _BsWindow BsWindow;

G_END_DECLS
