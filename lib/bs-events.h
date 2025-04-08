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

#if !defined(BOATSWAIN_COMPILATION) && !defined (BOATSWAIN_INSIDE)
#error "Only <boatswain.h> can be included directly."
#endif

#include "bs-types.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum
{
  BS_BUTTON_PRESS,
  BS_BUTTON_RELEASE,
  BS_DIAL_ROTATE,
  BS_DIAL_PRESS,
  BS_DIAL_RELEASE,
  BS_TOUCHSCREEN_SHORT_PRESS,
  BS_TOUCHSCREEN_LONG_PRESS,
  BS_TOUCHSCREEN_SWIPE,

  /* Synthetic (non-hardware) events */
  BS_CURSOR_DOUBLE_CLICK,
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

BsEventType bs_event_get_event_type (BsEvent *self);
BsDevice * bs_event_get_device (BsEvent *self);

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

BsButton * bs_button_event_get_button (BsButtonEvent *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (BsButtonEvent, g_object_unref)


/*
 * BsCursorEvent
 */

#define BS_TYPE_CURSOR_EVENT         (bs_cursor_event_get_type())
#define BS_CURSOR_EVENT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BS_TYPE_CURSOR_EVENT, BsCursorEvent))
#define BS_CURSOR_EVENT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), BS_TYPE_CURSOR_EVENT, BsCursorEventClass))
#define BS_IS_CURSOR_EVENT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BS_TYPE_CURSOR_EVENT))
#define BS_IS_CURSOR_EVENT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BS_TYPE_CURSOR_EVENT))
#define BS_CURSOR_EVENT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BS_TYPE_CURSOR_EVENT, BsCursorEventClass))

typedef struct _BsCursorEvent BsCursorEvent;
typedef struct _BsCursorEventClass BsCursorEventClass;

GType bs_cursor_event_get_type (void) G_GNUC_CONST;

/*
 * BsCursorEvent
 *
 * FIXME: make it private
 */

BsEvent * bs_cursor_event_new (BsEventType  event_type,
                               BsDevice    *device);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (BsCursorEvent, g_object_unref)


/*
 * BsDialEvent
 */

#define BS_TYPE_DIAL_EVENT         (bs_dial_event_get_type())
#define BS_DIAL_EVENT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BS_TYPE_DIAL_EVENT, BsDialEvent))
#define BS_DIAL_EVENT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), BS_TYPE_DIAL_EVENT, BsDialEventClass))
#define BS_IS_DIAL_EVENT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BS_TYPE_DIAL_EVENT))
#define BS_IS_DIAL_EVENT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BS_TYPE_DIAL_EVENT))
#define BS_DIAL_EVENT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BS_TYPE_DIAL_EVENT, BsDialEventClass))

typedef struct _BsDialEvent BsDialEvent;
typedef struct _BsDialEventClass BsDialEventClass;

GType bs_dial_event_get_type (void) G_GNUC_CONST;

BsDial * bs_dial_event_get_dial (BsDialEvent *self);
int bs_dial_event_get_rotation (BsDialEvent *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (BsDialEvent, g_object_unref)


/*
 * BsTouchscreenEvent
 */

#define BS_TYPE_TOUCHSCREEN_EVENT         (bs_touchscreen_event_get_type())
#define BS_TOUCHSCREEN_EVENT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BS_TYPE_TOUCHSCREEN_EVENT, BsTouchscreenEvent))
#define BS_TOUCHSCREEN_EVENT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), BS_TYPE_TOUCHSCREEN_EVENT, BsTouchscreenEventClass))
#define BS_IS_TOUCHSCREEN_EVENT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BS_TYPE_TOUCHSCREEN_EVENT))
#define BS_IS_TOUCHSCREEN_EVENT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BS_TYPE_TOUCHSCREEN_EVENT))
#define BS_TOUCHSCREEN_EVENT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BS_TYPE_TOUCHSCREEN_EVENT, BsTouchscreenEventClass))

typedef struct _BsTouchscreenEvent BsTouchscreenEvent;
typedef struct _BsTouchscreenEventClass BsTouchscreenEventClass;

GType bs_touchscreen_event_get_type (void) G_GNUC_CONST;

BsTouchscreenSlot * bs_touchscreen_event_get_slot (BsTouchscreenEvent *self);

void bs_touchscreen_event_get_points (BsTouchscreenEvent *self,
                                      graphene_point_t   *out_start,
                                      graphene_point_t   *out_end);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (BsTouchscreenEvent, g_object_unref)

G_END_DECLS
