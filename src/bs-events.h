/*
 * bs-events.h
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

#include "bs-types.h"

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum
{
  BS_BUTTON_PRESS_EVENT,
  BS_BUTTON_RELEASE_EVENT,

  // TODO: implement them
  //BS_TOUCHSCREEN_SHORT_PRESS_EVENT,
  //BS_TOUCHSCREEN_LONG_PRESS_EVENT,
  //BS_TOUCHSCREEN_SWIPE_EVENT,
  //BS_DIAL_ROTATION_EVENT,
  //BS_DIAL_PRESS_EVENT,
  //BS_DIAL_RELEASE_EVENT,
} BsEventType;


/*
 * BsEvent
 */

#define BS_TYPE_EVENT         (bs_event_get_type())
#define BS_EVENT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BS_TYPE_EVENT, BsEvent))
#define BS_EVENT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), BS_TYPE_EVENT, BsEventClass))
#define BS_IS_EVENT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BS_TYPE_EVENT))
#define BS_IS_EVENT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BS_TYPE_EVENT))
#define BS_EVENT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BS_TYPE_EVENT, BsEventClass))

typedef struct _BsEvent BsEvent;
typedef struct _BsEventClass BsEventClass;

GType bs_event_get_type (void) G_GNUC_CONST;

G_DEFINE_AUTOPTR_CLEANUP_FUNC (BsEvent, g_object_unref)


/*
 * BsButtonEvent
 */

#define BS_TYPE_BUTTON_EVENT         (bs_button_event_get_type())
#define BS_BUTTON_EVENT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BS_TYPE_BUTTON_EVENT, BsButtonEvent))
#define BS_BUTTON_EVENT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), BS_TYPE_BUTTON_EVENT, BsButtonEventClass))
#define BS_IS_BUTTON_EVENT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BS_TYPE_BUTTON_EVENT))
#define BS_IS_BUTTON_EVENT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BS_TYPE_BUTTON_EVENT))
#define BS_BUTTON_EVENT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BS_TYPE_BUTTON_EVENT, BsButtonEventClass))

typedef struct _BsButtonEvent BsButtonEvent;
typedef struct _BsButtonEventClass BsButtonEventClass;

GType bs_button_event_get_type (void) G_GNUC_CONST;

BsButton * bs_button_event_get_button (BsButtonEvent *event);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (BsButtonEvent, g_object_unref)

G_END_DECLS
