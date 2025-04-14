/*
 * bs-utils.c
 *
 * Copyright 2025 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "bs-config.h"
#include "bs-utils-private.h"

gboolean
bs_is_emulating_devices (void)
{
  /*
   * 0: uninitialized
   * 1: no
   * 2: yes
   */
  static size_t emulating = 0;

  if (g_once_init_enter (&emulating))
    {
      const char *env_var;
      gboolean result;

      env_var = g_getenv ("BOATSWAIN_EMULATE_DEVICES");
      result = g_strcmp0 (PROFILE, "development") == 0 &&
               env_var != NULL &&
               *env_var == '1';

      g_once_init_leave (&emulating, result ? 2 : 1);
    }

  return emulating == 2;
}
