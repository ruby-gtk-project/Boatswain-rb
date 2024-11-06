/* bs-page.h
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

#include <json-glib/json-glib.h>
#include <stdint.h>

#include "bs-types.h"

G_BEGIN_DECLS

#define BS_TYPE_PAGE (bs_page_get_type())
G_DECLARE_FINAL_TYPE (BsPage, bs_page, BS, PAGE, GObject)

BsPage * bs_page_new_empty (void);

BsPage * bs_page_new_from_json (JsonNode *node);

JsonNode * bs_page_to_json (BsPage *self);

gboolean bs_page_is_root (BsPage *self);

BsPageItem * bs_page_get_item (BsPage  *self,
                               uint8_t  position);

void bs_page_update_item (BsPage   *self,
                          size_t    position,
                          BsAction *action,
                          BsIcon   *custom_icon);

void bs_page_update_all_items (BsPage *self);

gboolean bs_page_realize (BsPage    *self,
                          size_t     position,
                          BsIcon   **out_custom_icon,
                          BsAction **out_action,
                          GError   **error);

G_END_DECLS
