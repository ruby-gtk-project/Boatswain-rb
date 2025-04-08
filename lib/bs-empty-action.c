/* bs-empty-action.c
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

#include "bs-empty-action.h"

struct _BsEmptyAction
{
  BsAction parent_instance;
};

G_DEFINE_FINAL_TYPE (BsEmptyAction, bs_empty_action, BS_TYPE_ACTION)

static void
bs_empty_action_class_init (BsEmptyActionClass *klass)
{
}

static void
bs_empty_action_init (BsEmptyAction *self)
{
}

BsAction *
bs_empty_action_new (void)
{
  return g_object_new (BS_TYPE_EMPTY_ACTION, NULL);
}
