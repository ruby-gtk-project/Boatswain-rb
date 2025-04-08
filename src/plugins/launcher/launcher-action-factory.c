/* launcher-action-factory.c
 *
 * Copyright 2022 Emmanuele Bassi
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

#include "launcher-action-factory.h"
#include "launcher-launch-action.h"
#include "launcher-open-file-action.h"
#include "launcher-open-url-action.h"

#include <glib/gi18n.h>

struct _LauncherActionFactory
{
  BsActionFactory parent_instance;
};

G_DEFINE_FINAL_TYPE (LauncherActionFactory, launcher_action_factory, BS_TYPE_ACTION_FACTORY);

static const BsActionEntry entries[] = {
  {
    .id = "launch-action",
    .icon_name = "app-launch-symbolic",
    .name = N_("Launch Application"),
    .description = NULL,
  },
  {
    .id = "launcher-open-file-action",
    .icon_name = "folder-documents-symbolic",
    .name = N_("Open File"),
    .description = NULL,
  },
  {
    .id = "launcher-open-url-action",
    .icon_name = "open-link-symbolic",
    .name = N_("Open URL"),
    .description = NULL,
  }
};

static BsAction *
launcher_action_factory_create_action (BsActionFactory *action_factory,
                                       BsActionInfo    *action_info)
{
  if (g_strcmp0 (bs_action_info_get_id (action_info), "launch-action") == 0)
    return launcher_launch_action_new ();
  else if (g_strcmp0 (bs_action_info_get_id (action_info), "launcher-open-file-action") == 0)
    return launcher_open_file_action_new ();
  else if (g_strcmp0 (bs_action_info_get_id (action_info), "launcher-open-url-action") == 0)
    return launcher_open_url_action_new ();

  return NULL;
}

static void
launcher_action_factory_class_init (LauncherActionFactoryClass *klass)
{
  BsActionFactoryClass *action_factory_class = BS_ACTION_FACTORY_CLASS (klass);

  action_factory_class->create_action = launcher_action_factory_create_action;
}

static void
launcher_action_factory_init (LauncherActionFactory *self)
{
  bs_action_factory_add_action_entries (BS_ACTION_FACTORY (self),
                                        entries,
                                        G_N_ELEMENTS (entries));
}
