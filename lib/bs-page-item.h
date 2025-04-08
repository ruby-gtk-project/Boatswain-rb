/* bs-page-item.h
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

#if !defined(BOATSWAIN_COMPILATION) && !defined (BOATSWAIN_INSIDE)
#error "Only <boatswain.h> can be included directly."
#endif

#include <json-glib/json-glib.h>

#include "bs-types.h"

G_BEGIN_DECLS

typedef enum
{
  BS_PAGE_ITEM_EMPTY,
  BS_PAGE_ITEM_ACTION,
} BsPageItemType;

#define BS_TYPE_PAGE_ITEM (bs_page_item_get_type())
G_DECLARE_FINAL_TYPE (BsPageItem, bs_page_item, BS, PAGE_ITEM, GObject)

BsPageItem * bs_page_item_new (BsPage *page);

BsPageItem * bs_page_item_new_from_json (BsPage   *page,
                                         JsonNode *node);

JsonNode * bs_page_item_to_json (BsPageItem *self);

BsPage * bs_page_item_get_page (BsPageItem *self);

const char * bs_page_item_get_action (BsPageItem *self);
void bs_page_item_set_action (BsPageItem *self,
                              const char *action);

JsonNode * bs_page_item_get_custom_icon (BsPageItem *self);
void bs_page_item_set_custom_icon (BsPageItem *self,
                                   JsonNode   *custom_icon);

BsPageItemType bs_page_item_get_item_type (BsPageItem *self);
void bs_page_item_set_item_type (BsPageItem     *self,
                                 BsPageItemType  item_type);

const char * bs_page_item_get_factory (BsPageItem *self);
void bs_page_item_set_factory (BsPageItem *self,
                               const char *factory);

JsonNode * bs_page_item_get_settings (BsPageItem *self);
void bs_page_item_set_settings (BsPageItem *self,
                                JsonNode   *settings);

gboolean bs_page_item_realize (BsPageItem  *self,
                               BsIcon     **out_custom_icon,
                               BsAction   **out_action,
                               GError     **error);

void bs_page_item_update (BsPageItem *self);

G_END_DECLS
