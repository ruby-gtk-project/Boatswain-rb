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


/* Map BsEventType to the appropriate GType */
#define BS_N_EVENTS (BS_BUTTON_RELEASE_EVENT + 1)
static GType bs_event_types[BS_N_EVENTS];
#define BS_EVENT_TYPE_SLOT(ETYPE) { bs_event_types[ETYPE] = g_define_type_id; }

void
bs_event_init_types_once (void)
{
  g_type_ensure (BS_TYPE_EVENT);
  g_type_ensure (BS_TYPE_BUTTON_EVENT);
}


/*
 * BsEvent
 */

struct _BsEvent
{
  GObject parent_instance;

  BsEventType event_type;
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
bs_event_alloc (BsEventType event_type)
{
  g_autoptr (BsEvent) event = NULL;

  g_assert (event_type >= BS_BUTTON_PRESS_EVENT && event_type < BS_N_EVENTS);
  g_assert (bs_event_types[event_type] != G_TYPE_INVALID);

  event = g_object_new (bs_event_types[event_type], NULL);
  event->event_type = event_type;

  return g_steal_pointer (&event);
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
                               BS_EVENT_TYPE_SLOT (BS_BUTTON_PRESS_EVENT)
                               BS_EVENT_TYPE_SLOT (BS_BUTTON_RELEASE_EVENT))

static void
bs_button_event_class_init (BsButtonEventClass *klass)
{
}

static void
bs_button_event_init (BsButtonEvent *self)
{
}

BsEvent *
bs_button_event_new (BsEventType  event_type,
                     BsButton    *button)
{
  g_autoptr (BsButtonEvent) button_event = NULL;

  g_assert (event_type == BS_BUTTON_PRESS_EVENT ||
            event_type == BS_BUTTON_RELEASE_EVENT);

  button_event = bs_event_alloc (event_type);
  g_assert (BS_IS_BUTTON_EVENT (button_event));

  button_event->button = button;

  return (BsEvent *) g_steal_pointer (&button_event);
}

