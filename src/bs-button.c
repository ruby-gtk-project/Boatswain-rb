/* bs-button.c
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

#define G_LOG_DOMAIN "Button"

#include "bs-button-private.h"

#include "bs-actionable-private.h"
#include "bs-action.h"
#include "bs-device-region.h"
#include "bs-icon.h"
#include "bs-page.h"
#include "bs-stream-deck-private.h"

struct _BsButton
{
  GObject parent_instance;

  BsAction *action;
  BsIcon *custom_icon;
  BsStreamDeck *stream_deck;
  BsDeviceRegion *region; /* unowned */
  unsigned int icon_width;
  unsigned int icon_height;
  uint8_t position;
  gulong custom_contents_changed_id;
  gulong custom_size_changed_id;
  gulong custom_icon_changed_id;
  gulong action_contents_changed_id;
  gulong action_size_changed_id;
  gulong action_icon_changed_id;
  gulong action_changed_id;
  int inhibit_page_updates_counter;
  gboolean pressed;
};

static void bs_actionable_interface_init (BsActionableInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (BsButton, bs_button, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (BS_TYPE_ACTIONABLE, bs_actionable_interface_init))

enum
{
  PROP_0,
  PROP_ICON,
  PROP_ICON_HEIGHT,
  PROP_ICON_WIDTH,
  PROP_CUSTOM_ICON,
  PROP_PRESSED,
  N_PROPS,

  /* Interface properties */
  PROP_ACTION,
};

enum
{
  ICON_CHANGED,
  N_SIGNALS,
};

static guint signals[N_SIGNALS];
static GParamSpec *properties[N_PROPS];


/*
 * Auxiliary methods
 */

static void
update_page (BsButton *self)
{
  BsPage *page = bs_stream_deck_get_active_page (self->stream_deck);

  if (page && self->inhibit_page_updates_counter == 0)
    {
      const char *region_id = bs_device_region_get_id (self->region);

      bs_page_update_item (page, region_id, self->position, self->action, self->custom_icon);
      bs_stream_deck_save (self->stream_deck);
    }
}

static void
update_relative_icon (BsButton *self)
{

  if (!self->custom_icon)
    return;

  bs_icon_set_relative (self->custom_icon,
                        self->action ? bs_action_get_icon (self->action) : NULL);
}

static void
upload_icon (BsButton *self)
{
  g_autoptr (GError) error = NULL;

  if (!bs_stream_deck_is_initialized (self->stream_deck))
    return;

  bs_stream_deck_upload_button (self->stream_deck, self, &error);

  if (error)
    g_warning ("Error updating Stream Deck button icon: %s", error->message);
}

static void
remove_custom_icon (BsButton *self)
{
  if (self->custom_icon)
    {
      g_clear_signal_handler (&self->custom_contents_changed_id, self->custom_icon);
      g_clear_signal_handler (&self->custom_size_changed_id, self->custom_icon);
      g_clear_signal_handler (&self->custom_icon_changed_id, self->custom_icon);
      g_clear_object (&self->custom_icon);
    }
}


/*
 * Callbacks
 */

static void
on_action_changed_cb (BsAction *action,
                      BsButton *self)
{
  bs_stream_deck_save (self->stream_deck);
}

static void
on_icon_changed_cb (BsIcon   *icon,
                    BsButton *self)
{
  update_relative_icon (self);
  upload_icon (self);
  g_signal_emit (self, signals[ICON_CHANGED], 0, icon);
}

static void
on_icon_properties_changed_cb (BsIcon     *icon,
                               GParamSpec *pspec,
                               BsButton   *self)
{
  bs_stream_deck_save (self->stream_deck);
}


/*
 * BsActionable interface
 */

static BsAction *
bs_button_actionable_get_action (BsActionable *actionable)
{
  BsButton *self = (BsButton *) actionable;

  g_assert (BS_IS_BUTTON (actionable));

  return self->action;
}

static void
bs_button_actionable_set_action (BsActionable *actionable,
                                 BsAction     *action)
{
  BsButton *self = (BsButton *) actionable;
  BsIcon *action_icon;

  g_assert (BS_IS_BUTTON (actionable));

  if (self->action == action)
    return;

  if (action)
    remove_custom_icon (self);

  if (self->action)
    {
      g_clear_signal_handler (&self->action_contents_changed_id, bs_action_get_icon (self->action));
      g_clear_signal_handler (&self->action_size_changed_id, bs_action_get_icon (self->action));
      g_clear_signal_handler (&self->action_icon_changed_id, bs_action_get_icon (self->action));
      g_clear_signal_handler (&self->action_changed_id, self->action);
    }

  g_set_object (&self->action, action);

  self->action_changed_id =
    g_signal_connect (action, "changed", G_CALLBACK (on_action_changed_cb), self);

  action_icon = bs_action_get_icon (action);
  self->action_contents_changed_id =
    g_signal_connect (action_icon, "invalidate-contents", G_CALLBACK (on_icon_changed_cb), self);
  self->action_size_changed_id =
    g_signal_connect (action_icon, "invalidate-size", G_CALLBACK (on_icon_changed_cb), self);
  self->action_icon_changed_id =
    g_signal_connect (action_icon, "notify", G_CALLBACK (on_icon_properties_changed_cb), self);

  update_relative_icon (self);
  update_page (self);
  upload_icon (self);

  g_object_notify (G_OBJECT (self), "action");
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CUSTOM_ICON]);


  g_signal_emit (self, signals[ICON_CHANGED], 0, bs_button_get_icon (self));
}

static void
bs_actionable_interface_init (BsActionableInterface *iface)
{
  iface->get_action = bs_button_actionable_get_action;
  iface->set_action = bs_button_actionable_set_action;
}


/*
 * GObject overrides
 */

static void
bs_button_finalize (GObject *object)
{
  BsButton *self = (BsButton *)object;

  remove_custom_icon (self);

  if (self->action)
    {
      g_clear_signal_handler (&self->action_contents_changed_id, bs_action_get_icon (self->action));
      g_clear_signal_handler (&self->action_size_changed_id, bs_action_get_icon (self->action));
      g_clear_signal_handler (&self->action_icon_changed_id, bs_action_get_icon (self->action));
      g_clear_signal_handler (&self->action_changed_id, self->action);
      g_clear_object (&self->action);
    }

  G_OBJECT_CLASS (bs_button_parent_class)->finalize (object);
}

static void
bs_button_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  BsButton *self = BS_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ACTION:
      g_value_set_object (value, self->action);
      break;

    case PROP_ICON:
      g_value_set_object (value, bs_button_get_icon (self));
      break;

    case PROP_ICON_HEIGHT:
      g_value_set_uint (value, self->icon_height);
      break;

    case PROP_ICON_WIDTH:
      g_value_set_uint (value, self->icon_width);
      break;

    case PROP_CUSTOM_ICON:
      g_value_set_object (value, self->custom_icon);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_button_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
bs_button_class_init (BsButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = bs_button_finalize;
  object_class->get_property = bs_button_get_property;
  object_class->set_property = bs_button_set_property;

  properties[PROP_ICON] = g_param_spec_object ("icon", NULL, NULL,
                                               BS_TYPE_ICON,
                                               G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_ICON_HEIGHT] = g_param_spec_uint ("icon-height", NULL, NULL,
                                                    1, G_MAXUINT, 1,
                                                    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_ICON_WIDTH] = g_param_spec_uint ("icon-width", NULL, NULL,
                                                   1, G_MAXUINT, 1,
                                                   G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_CUSTOM_ICON] = g_param_spec_object ("custom-icon", NULL, NULL,
                                                      BS_TYPE_ICON,
                                                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_PRESSED] = g_param_spec_boolean ("pressed", NULL, NULL,
                                                   FALSE,
                                                   G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  g_object_class_override_property (object_class, PROP_ACTION, "action");

  signals[ICON_CHANGED] = g_signal_new ("icon-changed",
                                        BS_TYPE_BUTTON,
                                        G_SIGNAL_RUN_LAST,
                                        0, NULL, NULL, NULL,
                                        G_TYPE_NONE,
                                        1,
                                        BS_TYPE_ICON);
}

static void
bs_button_init (BsButton *self)
{
  self->pressed = FALSE;
}

BsButton *
bs_button_new (BsStreamDeck   *stream_deck,
               BsDeviceRegion *region,
               uint8_t         position,
               unsigned int    icon_width,
               unsigned int    icon_height)
{
  g_autoptr (BsIcon) empty_icon = NULL;
  BsButton *self;

  self = g_object_new (BS_TYPE_BUTTON, NULL);
  self->stream_deck = stream_deck;
  self->region = region;
  self->position = position;
  self->icon_width = icon_width;
  self->icon_height = icon_height;

  empty_icon = bs_icon_new_empty ();
  bs_button_set_custom_icon (self, empty_icon);

  return self;
}

BsStreamDeck *
bs_button_get_stream_deck (BsButton *self)
{
  g_return_val_if_fail (BS_IS_BUTTON (self), NULL);

  return self->stream_deck;
}

uint8_t
bs_button_get_position (BsButton *self)
{
  g_return_val_if_fail (BS_IS_BUTTON (self), 0);

  return self->position;
}

gboolean
bs_button_get_pressed (BsButton *self)
{
  g_return_val_if_fail (BS_IS_BUTTON (self), FALSE);

  return self->pressed;
}

void
bs_button_set_pressed (BsButton *self,
                       gboolean  pressed)
{
  g_return_if_fail (BS_IS_BUTTON (self));

  if (self->pressed == pressed)
    return;

  self->pressed = pressed;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PRESSED]);
}

BsIcon *
bs_button_get_icon (BsButton *self)
{
  g_return_val_if_fail (BS_IS_BUTTON (self), NULL);

  if (self->custom_icon)
    return self->custom_icon;

  if (self->action)
    return bs_action_get_icon (self->action);

  return NULL;
}

BsIcon *
bs_button_get_custom_icon (BsButton *self)
{
  g_return_val_if_fail (BS_IS_BUTTON (self), NULL);

  return self->custom_icon;
}

void
bs_button_set_custom_icon (BsButton *self,
                           BsIcon   *icon)
{
  g_return_if_fail (BS_IS_BUTTON (self));

  if (self->custom_icon == icon)
    return;

  remove_custom_icon (self);

  g_set_object (&self->custom_icon, icon);

  if (icon)
    {
      self->custom_contents_changed_id =
        g_signal_connect (icon, "invalidate-contents", G_CALLBACK (on_icon_changed_cb), self);
      self->custom_size_changed_id =
        g_signal_connect (icon, "invalidate-size", G_CALLBACK (on_icon_changed_cb), self);
      self->custom_icon_changed_id =
        g_signal_connect (icon, "notify", G_CALLBACK (on_icon_properties_changed_cb), self);
    }

  update_relative_icon (self);
  update_page (self);
  upload_icon (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CUSTOM_ICON]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON]);
  g_signal_emit (self, signals[ICON_CHANGED], 0, icon);
}

void
bs_button_inhibit_page_updates (BsButton *self)
{
  g_return_if_fail (BS_IS_BUTTON (self));

  self->inhibit_page_updates_counter++;
}

void
bs_button_uninhibit_page_updates (BsButton *self)
{
  g_return_if_fail (BS_IS_BUTTON (self));
  g_return_if_fail (self->inhibit_page_updates_counter > 0);

  self->inhibit_page_updates_counter--;
}

unsigned int
bs_button_get_icon_width (BsButton *self)
{
  g_return_val_if_fail (BS_IS_BUTTON (self), 0);

  return self->icon_width;
}

unsigned int
bs_button_get_icon_height (BsButton *self)
{
  g_return_val_if_fail (BS_IS_BUTTON (self), 0);

  return self->icon_height;
}

BsDeviceRegion *
bs_button_get_region (BsButton *self)
{
  g_assert (BS_IS_BUTTON (self));

  return self->region;
}
