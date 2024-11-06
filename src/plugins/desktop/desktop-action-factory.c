/*
 * desktop-action-factory.c
 *
 * Copyright 2023 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "bs-action-factory.h"
#include "bs-action-info.h"
#include "bs-application.h"
#include "desktop-action-factory.h"
#include "desktop-keyboard-shortcut-action.h"

#include <glib/gi18n.h>

struct _DesktopActionFactory
{
  BsActionFactory parent_instance;
};

G_DEFINE_FINAL_TYPE (DesktopActionFactory, desktop_action_factory, BS_TYPE_ACTION_FACTORY);

static const BsActionEntry entries[] = {
  {
    .id = "desktop-keyboard-shortcut",
    .icon_name = "preferences-desktop-keyboard-shortcuts-symbolic",
    .name = N_("Keyboard Shortcut"),
    .description = N_("Trigger a keyboard shortcut"),
  },
};

static BsAction *
desktop_action_factory_create_action (BsActionFactory *action_factory,
                                      BsActionInfo    *action_info)
{
  DesktopActionFactory *self = (DesktopActionFactory *)action_factory;
  BsDesktopController *desktop_controller;
  BsApplication *application;

  g_assert (DESKTOP_IS_ACTION_FACTORY (self));

  application = BS_APPLICATION (g_application_get_default ());
  desktop_controller = bs_application_get_desktop_controller (application);

  if (g_strcmp0 (bs_action_info_get_id (action_info), "desktop-keyboard-shortcut") == 0)
    return desktop_keyboard_shortcut_action_new (desktop_controller);


  return NULL;
}

static void
desktop_action_factory_class_init (DesktopActionFactoryClass *klass)
{
  BsActionFactoryClass *action_factory_class = BS_ACTION_FACTORY_CLASS (klass);

  action_factory_class->create_action = desktop_action_factory_create_action;
}

static void
desktop_action_factory_init (DesktopActionFactory *self)
{
  bs_action_factory_add_action_entries (BS_ACTION_FACTORY (self),
                                        entries,
                                        G_N_ELEMENTS (entries));
}
