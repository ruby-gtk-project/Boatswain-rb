/*
 * default-page-up-action.c
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

#include "default-page-up-action.h"

#include "bs-events.h"
#include "bs-icon.h"
#include "bs-stream-deck.h"
#include "bs-button.h"

#include <glib/gi18n.h>

struct _DefaultPageUpAction
{
  BsAction parent_instance;
};

G_DEFINE_FINAL_TYPE (DefaultPageUpAction, default_page_up_action, BS_TYPE_ACTION)


/*
 * BsAction overrides
 */

static void
default_page_up_action_handle_event (BsAction *action,
                                     BsEvent  *event)
{
  BsStreamDeck *device;

  if (bs_event_get_event_type (event) != BS_BUTTON_PRESS &&
      bs_event_get_event_type (event) != BS_CURSOR_DOUBLE_CLICK)
    return;

  device = bs_event_get_device (event);
  bs_stream_deck_pop_page (device);
}


/*
 * GObject overrides
 */

static void
default_page_up_action_constructed (GObject *object)
{
  DefaultPageUpAction *self = (DefaultPageUpAction *)object;

  G_OBJECT_CLASS (default_page_up_action_parent_class)->constructed (object);

  bs_icon_set_icon_name (bs_action_get_icon (BS_ACTION (self)), "go-up-symbolic");
}

static void
default_page_up_action_class_init (DefaultPageUpActionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  BsActionClass *action_class = BS_ACTION_CLASS (klass);

  object_class->constructed = default_page_up_action_constructed;

  action_class->handle_event = default_page_up_action_handle_event;
}

static void
default_page_up_action_init (DefaultPageUpAction *self)
{
}

BsAction *
default_page_up_action_new (void)
{
  return g_object_new (DEFAULT_TYPE_PAGE_UP_ACTION, NULL);
}
