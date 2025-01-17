/* default-switch-page-action.c
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

#include "bs-events.h"
#include "bs-icon.h"
#include "bs-page.h"
#include "bs-stream-deck.h"
#include "bs-button.h"
#include "default-switch-page-action.h"

#include <glib/gi18n.h>

struct _DefaultSwitchPageAction
{
  BsAction parent_instance;

  BsPage *page;
};

G_DEFINE_FINAL_TYPE (DefaultSwitchPageAction, default_switch_page_action, BS_TYPE_ACTION)


/*
 * BsAction overrides
 */

static void
default_switch_page_action_handle_event (BsAction *action,
                                         BsEvent  *event)
{
  DefaultSwitchPageAction *self = DEFAULT_SWITCH_PAGE_ACTION (action);
  BsStreamDeck *stream_deck;

  if (bs_event_get_event_type (event) != BS_BUTTON_PRESS &&
      bs_event_get_event_type (event) != BS_CURSOR_DOUBLE_CLICK)
    return;

  stream_deck = bs_event_get_device (event);

  bs_stream_deck_push_page (stream_deck, self->page);
}

static JsonNode *
default_switch_page_action_serialize_settings (BsAction *action)
{
  DefaultSwitchPageAction *self = DEFAULT_SWITCH_PAGE_ACTION (action);
  g_autoptr (JsonBuilder) builder = NULL;

  builder = json_builder_new ();

  json_builder_begin_object (builder);

  if (self->page)
    {
      json_builder_set_member_name (builder, "page");
      json_builder_add_value (builder, bs_page_to_json (self->page));
    }

  json_builder_end_object (builder);

  return json_builder_get_root (builder);
}

static void
default_switch_page_action_deserialize_settings (BsAction   *action,
                                                 JsonObject *object)
{
  DefaultSwitchPageAction *self = DEFAULT_SWITCH_PAGE_ACTION (action);

  if (json_object_has_member (object, "page"))
    {
      g_clear_object (&self->page);
      self->page = bs_page_new_from_json (json_object_get_member (object, "page"));
    }
}


/*
 * GObject overrides
 */

static void
default_switch_page_action_finalize (GObject *object)
{
  DefaultSwitchPageAction *self = (DefaultSwitchPageAction *)object;

  g_clear_object (&self->page);

  G_OBJECT_CLASS (default_switch_page_action_parent_class)->finalize (object);
}

static void
default_switch_page_action_constructed (GObject *object)
{
  DefaultSwitchPageAction *self = (DefaultSwitchPageAction *)object;

  G_OBJECT_CLASS (default_switch_page_action_parent_class)->constructed (object);

  self->page = bs_page_new_empty ();

  bs_icon_set_icon_name (bs_action_get_icon (BS_ACTION (self)), "folder-symbolic");
}

static void
default_switch_page_action_class_init (DefaultSwitchPageActionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  BsActionClass *action_class = BS_ACTION_CLASS (klass);

  object_class->finalize = default_switch_page_action_finalize;
  object_class->constructed = default_switch_page_action_constructed;

  action_class->handle_event = default_switch_page_action_handle_event;
  action_class->serialize_settings = default_switch_page_action_serialize_settings;
  action_class->deserialize_settings = default_switch_page_action_deserialize_settings;
}

static void
default_switch_page_action_init (DefaultSwitchPageAction *self)
{
}

BsAction *
default_switch_page_action_new (void)
{
  return g_object_new (DEFAULT_TYPE_SWITCH_PAGE_ACTION, NULL);
}
