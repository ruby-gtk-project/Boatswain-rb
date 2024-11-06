/* bs-action-factory.h
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

#pragma once

#include <libpeas.h>

#include "bs-types.h"

G_BEGIN_DECLS

#define BS_TYPE_ACTION_FACTORY (bs_action_factory_get_type())
G_DECLARE_DERIVABLE_TYPE (BsActionFactory, bs_action_factory, BS, ACTION_FACTORY, PeasExtensionBase)

typedef struct
{
  const char *id;
  const char *icon_name;
  const char *name;
  const char *description;
} BsActionEntry;

struct _BsActionFactoryClass
{
  PeasExtensionBaseClass parent_class;

  BsAction * (*create_action) (BsActionFactory *self,
                               BsActionInfo    *action_info);
};

BsActionInfo * bs_action_factory_get_info (BsActionFactory *self,
                                           const char      *id);

BsAction * bs_action_factory_create_action (BsActionFactory *self,
                                            BsActionInfo    *action_info);


void bs_action_factory_add_action (BsActionFactory *self,
                                   BsActionInfo    *info);

void bs_action_factory_add_action_entries (BsActionFactory     *self,
                                           const BsActionEntry *entries,
                                           size_t               n_entries);

G_END_DECLS
