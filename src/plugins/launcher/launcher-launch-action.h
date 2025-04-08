/* launcher-launch-action.h
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

#pragma once

#include <boatswain.h>

G_BEGIN_DECLS

#define LAUNCHER_TYPE_LAUNCH_ACTION (launcher_launch_action_get_type())
G_DECLARE_FINAL_TYPE (LauncherLaunchAction, launcher_launch_action, LAUNCHER, LAUNCH_ACTION, BsAction)

BsAction * launcher_launch_action_new (void);

void launcher_launch_action_set_app (LauncherLaunchAction *self,
                                     GAppInfo             *app_info);

G_END_DECLS
