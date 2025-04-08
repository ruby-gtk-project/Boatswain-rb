/* bs-profile.h
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

#define BS_TYPE_PROFILE (bs_profile_get_type())
G_DECLARE_FINAL_TYPE (BsProfile, bs_profile, BS, PROFILE, GObject)

BsProfile * bs_profile_new_empty (BsStreamDeck *stream_deck);

BsProfile * bs_profile_new_from_json (BsStreamDeck *stream_deck,
                                      JsonNode     *node);

JsonNode * bs_profile_to_json (BsProfile *self);

double bs_profile_get_brightness (BsProfile *self);
void bs_profile_set_brightness (BsProfile *self,
                                double     brightness);

const char * bs_profile_get_id (BsProfile *self);

const char * bs_profile_get_name (BsProfile *self);
void bs_profile_set_name (BsProfile  *self,
                          const char *name);

BsStreamDeck * bs_profile_get_stream_deck (BsProfile *self);

BsPage * bs_profile_get_root_page (BsProfile *self);

G_END_DECLS
