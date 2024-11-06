/*
 * bs-events.c
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

#include "bs-events-private.h"

#include "bs-button.h"
#include "bs-stream-deck.h"
#include "bs-touchscreen-slot.h"

/* Map BsEventType to the appropriate GType */
#define BS_N_EVENTS (BS_CURSOR_DOUBLE_CLICK + 1)
static GType bs_event_types[BS_N_EVENTS];
#define BS_EVENT_TYPE_SLOT(ETYPE) { bs_event_types[ETYPE] = g_define_type_id; }

void
bs_event_init_types_once (void)
{
  g_type_ensure (BS_TYPE_EVENT);
  g_type_ensure (BS_TYPE_BUTTON_EVENT);
  g_type_ensure (BS_TYPE_CURSOR_EVENT);
  g_type_ensure (BS_TYPE_TOUCHSCREEN_EVENT);
}


/*
 * BsEvent
 */

struct _BsEvent
{
  GObject parent_instance;

  BsEventType event_type;
  BsStreamDeck *device;
};

struct _BsEventClass
{
  GObjectClass parent_class;
};

G_DEFINE_ABSTRACT_TYPE (BsEvent, bs_event, G_TYPE_OBJECT)

static void
bs_event_class_init (BsEventClass *klass)
{
}

static void
bs_event_init (BsEvent *self)
{
}

static gpointer
bs_event_alloc (BsEventType   event_type,
                BsStreamDeck *device)
{
  g_autoptr (BsEvent) event = NULL;

  g_assert (event_type >= BS_BUTTON_PRESS && event_type < BS_N_EVENTS);
  g_assert (bs_event_types[event_type] != G_TYPE_INVALID);

  event = g_object_new (bs_event_types[event_type], NULL);
  event->event_type = event_type;
  event->device = device;

  return g_steal_pointer (&event);
}

BsEventType
bs_event_get_event_type (BsEvent *self)
{
  g_return_val_if_fail (BS_IS_EVENT (self), 0);

  return self->event_type;
}

BsStreamDeck *
bs_event_get_device (BsEvent *self)
{
  g_return_val_if_fail (BS_IS_EVENT (self), 0);

  return self->device;
}


/*
 * BsButtonEvent
 */

struct _BsButtonEvent
{
  BsEvent parent_instance;

  BsButton *button;
};

struct _BsButtonEventClass
{
  BsEventClass parent_class;
};

G_DEFINE_FINAL_TYPE_WITH_CODE (BsButtonEvent, bs_button_event, BS_TYPE_EVENT,
                               BS_EVENT_TYPE_SLOT (BS_BUTTON_PRESS)
                               BS_EVENT_TYPE_SLOT (BS_BUTTON_RELEASE))

static void
bs_button_event_class_init (BsButtonEventClass *klass)
{
}

static void
bs_button_event_init (BsButtonEvent *self)
{
}

BsEvent *
bs_button_event_new (BsEventType   event_type,
                     BsStreamDeck *device,
                     BsButton     *button)
{
  g_autoptr (BsButtonEvent) button_event = NULL;

  g_assert (event_type == BS_BUTTON_PRESS ||
            event_type == BS_BUTTON_RELEASE);
  g_assert (BS_IS_STREAM_DECK (device));
  g_assert (BS_IS_BUTTON (button));

  button_event = bs_event_alloc (event_type, device);
  g_assert (BS_IS_BUTTON_EVENT (button_event));

  button_event->button = button;

  return (BsEvent *) g_steal_pointer (&button_event);
}

/**
 * bs_button_event_get_button:
 *
 * Retrieves the #BsButton that generated this event.
 *
 * Returns: (transfer none): a #BsButton
 */
BsButton *
bs_button_event_get_button (BsButtonEvent *self)
{
  g_return_val_if_fail (BS_IS_BUTTON_EVENT (self), NULL);

  return self->button;
}


/*
 * BsCursorEvent
 */

struct _BsCursorEvent
{
  BsEvent parent_instance;
};

struct _BsCursorEventClass
{
  BsEventClass parent_class;
};

G_DEFINE_FINAL_TYPE_WITH_CODE (BsCursorEvent, bs_cursor_event, BS_TYPE_EVENT,
                               BS_EVENT_TYPE_SLOT (BS_CURSOR_DOUBLE_CLICK))

static void
bs_cursor_event_class_init (BsCursorEventClass *klass)
{
}

static void
bs_cursor_event_init (BsCursorEvent *self)
{
}

BsEvent *
bs_cursor_event_new (BsEventType   event_type,
                     BsStreamDeck *device)
{
  g_autoptr (BsCursorEvent) cursor_event = NULL;

  g_assert (event_type == BS_CURSOR_DOUBLE_CLICK);
  g_assert (BS_IS_STREAM_DECK (device));

  cursor_event = bs_event_alloc (event_type, device);
  g_assert (BS_IS_CURSOR_EVENT (cursor_event));

  return (BsEvent *) g_steal_pointer (&cursor_event);
}


/*
 * BsTouchscreenEvent
 */

struct _BsTouchscreenEvent
{
  BsEvent parent_instance;

  BsTouchscreenSlot *slot;
  graphene_point_t start;
  graphene_point_t end;
};

struct _BsTouchscreenEventClass
{
  BsEventClass parent_class;
};

G_DEFINE_FINAL_TYPE_WITH_CODE (BsTouchscreenEvent, bs_touchscreen_event, BS_TYPE_EVENT,
                               BS_EVENT_TYPE_SLOT (BS_TOUCHSCREEN_SHORT_PRESS)
                               BS_EVENT_TYPE_SLOT (BS_TOUCHSCREEN_LONG_PRESS)
                               BS_EVENT_TYPE_SLOT (BS_TOUCHSCREEN_SWIPE))

static void
bs_touchscreen_event_class_init (BsTouchscreenEventClass *klass)
{
}

static void
bs_touchscreen_event_init (BsTouchscreenEvent *self)
{
}

BsEvent *
bs_touchscreen_event_new (BsEventType             event_type,
                          BsStreamDeck           *device,
                          BsTouchscreenSlot      *slot,
                          const graphene_point_t *start,
                          const graphene_point_t *end)
{
  g_autoptr (BsTouchscreenEvent) touchscreen_event = NULL;

  g_assert (event_type == BS_TOUCHSCREEN_SHORT_PRESS ||
            event_type == BS_TOUCHSCREEN_LONG_PRESS ||
            event_type == BS_TOUCHSCREEN_SWIPE);
  g_assert (BS_IS_STREAM_DECK (device));
  g_assert (BS_IS_TOUCHSCREEN_SLOT (slot));
  g_assert (start != NULL);
  g_assert (end != NULL);

  touchscreen_event = bs_event_alloc (event_type, device);
  g_assert (BS_IS_TOUCHSCREEN_EVENT (touchscreen_event));

  touchscreen_event->slot = slot;
  graphene_point_init_from_point (&touchscreen_event->start, start);
  graphene_point_init_from_point (&touchscreen_event->end, end);

  return (BsEvent *) g_steal_pointer (&touchscreen_event);
}

/**
 * bs_touchscreen_event_get_slot:
 *
 * Retrieves the #BsTouchscreenSlot that generated this event.
 *
 * Returns: (transfer none): a #BsTouchscreenSlot
 */
BsTouchscreenSlot *
bs_touchscreen_event_get_slot (BsTouchscreenEvent *self)
{
  g_return_val_if_fail (BS_IS_TOUCHSCREEN_EVENT (self), NULL);

  return self->slot;
}

/**
 * bs_touchscreen_event_get_points:
 * @out_start: (nullable)(out): return location for a #graphene_point_t
 * @out_end: (nullable)(out): return location for a #graphene_point_t
 *
 * Retrieves the start and end points of this event. These points are
 * in touchscreen coordinate space, with (0, 0) being the top left of
 * the touchscreen.
 */
void
bs_touchscreen_event_get_points (BsTouchscreenEvent *self,
                                 graphene_point_t   *out_start,
                                 graphene_point_t   *out_end)
{
  g_return_if_fail (BS_IS_TOUCHSCREEN_EVENT (self));

  if (out_start)
    *out_start = self->start;

  if (out_end)
    *out_end = self->end;
}
