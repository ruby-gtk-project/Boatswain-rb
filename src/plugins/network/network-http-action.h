/* network-http-action.h
 *
 * Copyright 2023 Lorenzo Prosseda
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
#include <libsoup/soup.h>

G_BEGIN_DECLS

// Elements under the method selection drop-down
typedef enum
{
  METHOD_GET,
  METHOD_POST,
} RequestMethod;

// Elements under the payload source selection drop-down
typedef enum
{
  SRC_TEXT,
  SRC_FILE,
} PayloadSource;

#define NETWORK_TYPE_HTTP_ACTION (network_http_action_get_type())
G_DECLARE_FINAL_TYPE (NetworkHttpAction, network_http_action, NETWORK, HTTP_ACTION, BsAction)

BsAction * network_http_action_new (SoupSession *session);

GFile * network_http_action_get_file (NetworkHttpAction *self);
void    network_http_action_set_file (NetworkHttpAction *self,
                                      GFile             *file);

void network_http_action_set_method (NetworkHttpAction *self,
                                     RequestMethod      method);

void network_http_action_set_payload_src (NetworkHttpAction *self,
                                          PayloadSource      method);

void network_http_action_set_uri (NetworkHttpAction *self,
                                  const char        *uri);

void
network_http_action_set_payload_text (NetworkHttpAction *self,
                                      const char        *payload_text);

G_END_DECLS
