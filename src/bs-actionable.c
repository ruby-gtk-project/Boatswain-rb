/*
 * bs-actionable.c
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

#define G_LOG_DOMAIN "Actionable"

#include "bs-actionable-private.h"

#include "bs-action-private.h"
#include "bs-events.h"
#include "bs-icon.h"

G_DEFINE_INTERFACE (BsActionable, bs_actionable, G_TYPE_OBJECT)

enum
{
  ACTION_CHANGED,
  N_SIGNALS,
};

static guint signals[N_SIGNALS];

static void
bs_actionable_default_init (BsActionableInterface *iface)
{
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("action", NULL, NULL,
                                                            BS_TYPE_ACTION,
                                                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  signals[ACTION_CHANGED] = g_signal_new ("action-changed",
                                          BS_TYPE_ACTIONABLE,
                                          G_SIGNAL_RUN_FIRST,
                                          0, NULL, NULL, NULL,
                                          G_TYPE_NONE,
                                          0);
}

/**
 * bs_actionable_get_action:
 * @self: a #BsActionable
 *
 * Gets the current action of @self.
 *
 * Returns: (transfer none)(nullable): the current #BsAction of @self
 */
BsAction *
bs_actionable_get_action (BsActionable *self)
{
  g_assert (BS_IS_ACTIONABLE (self));
  g_assert (BS_ACTIONABLE_GET_IFACE (self)->get_action != NULL);

  return BS_ACTIONABLE_GET_IFACE (self)->get_action (self);
}

void
bs_actionable_set_action (BsActionable *self,
                          BsAction     *action)
{
  g_assert (BS_IS_ACTIONABLE (self));
  g_assert (!action || BS_IS_ACTION (action));
  g_assert (BS_ACTIONABLE_GET_IFACE (self)->set_action != NULL);

  BS_ACTIONABLE_GET_IFACE (self)->set_action (self, action);
}

void
bs_actionable_handle_event (BsActionable *self,
                            BsEvent      *event)
{
  BsAction *action;

  g_assert (BS_IS_ACTIONABLE (self));
  g_assert (BS_IS_EVENT (event));

  g_debug ("Actionable %s (%p) handling event %s",
           G_OBJECT_TYPE_NAME (self),
           self,
           G_OBJECT_TYPE_NAME (event));

  action = bs_actionable_get_action (self);
  if (action)
    bs_action_handle_event (action, event);
}

void
bs_actionable_action_changed (BsActionable *self)
{
  g_assert (BS_IS_ACTIONABLE (self));
  g_assert (BS_IS_ACTION (bs_actionable_get_action (self)));

  g_signal_emit (self, signals[ACTION_CHANGED], 0);
}
