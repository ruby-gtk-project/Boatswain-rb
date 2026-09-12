/*
 * bs-page-private.h
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

#pragma once

#include "bs-page.h"

G_BEGIN_DECLS

BsPage   *bs_page_new_root        (JsonNode   *node);
JsonNode *bs_page_get_region_data (BsPage     *self,
                                   const char *region_id);
void      bs_page_set_region_data (BsPage     *self,
                                   const char *region_id,
                                   JsonNode   *region_data);
void      bs_page_load_items      (BsPage     *self);
void      bs_page_unload_items    (BsPage     *self);


G_END_DECLS
