/* default-action-factory.c
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

#include "default-action-factory.h"
#include "default-brightness-action.h"
#include "default-multi-action.h"
#include "default-page-up-action.h"
#include "default-switch-page-action.h"
#include "default-switch-profile-action.h"

#include <glib/gi18n.h>

struct _DefaultActionFactory
{
  BsActionFactory parent_instance;
};

G_DEFINE_FINAL_TYPE (DefaultActionFactory, default_action_factory, BS_TYPE_ACTION_FACTORY);

static const BsActionEntry entries[] = {
  {
    .id = "default-switch-page-action",
    .icon_name = "folder-symbolic",
    .name = N_("Folder"),
    .description = NULL,
  },
  {
    .id = "default-switch-profile-action",
    .icon_name = "view-list-bullet-symbolic",
    .name = N_("Switch Profile"),
    .description = NULL,
  },
  {
    .id = "default-brightness-action",
    .icon_name = "display-brightness-symbolic",
    .name = N_("Brightness"),
    .description = NULL,
  },
  {
    .id = "default-multi-action",
    .icon_name = "stacked-plates-symbolic",
    .name = N_("Multiple Actions"),
    .description = NULL,
  },
  {
    .id = "default-page-up-action",
    .icon_name = "go-up-symbolic",
    .name = N_("Go Up"),
    .description = NULL,
    .hidden = TRUE,
  },
};

static BsAction *
default_action_factory_create_action (BsActionFactory *action_factory,
                                      BsActionInfo    *action_info)
{
  if (g_strcmp0 (bs_action_info_get_id (action_info), "default-switch-profile-action") == 0)
    return default_switch_profile_action_new ();
  else if (g_strcmp0 (bs_action_info_get_id (action_info), "default-brightness-action") == 0)
    return default_brightness_action_new ();
  else if (g_strcmp0 (bs_action_info_get_id (action_info), "default-switch-page-action") == 0)
    return default_switch_page_action_new ();
  else if (g_strcmp0 (bs_action_info_get_id (action_info), "default-multi-action") == 0)
    return default_multi_action_new ();
  else if (g_strcmp0 (bs_action_info_get_id (action_info), "default-page-up-action") == 0)
    return default_page_up_action_new ();

  return NULL;
}

static void
default_action_factory_class_init (DefaultActionFactoryClass *klass)
{
  BsActionFactoryClass *action_factory_class = BS_ACTION_FACTORY_CLASS (klass);

  action_factory_class->create_action = default_action_factory_create_action;
}

static void
default_action_factory_init (DefaultActionFactory *self)
{
  bs_action_factory_add_action_entries (BS_ACTION_FACTORY (self),
                                        entries,
                                        G_N_ELEMENTS (entries));
}
