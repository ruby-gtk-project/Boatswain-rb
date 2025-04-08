/* network-action-factory.c
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

#include "network-action-factory.h"
#include "network-http-action.h"

#include <glib/gi18n.h>

struct _NetworkActionFactory
{
  BsActionFactory parent_instance;

  SoupSession *session;
};

G_DEFINE_FINAL_TYPE (NetworkActionFactory, network_action_factory, BS_TYPE_ACTION_FACTORY);

static const BsActionEntry entries[] = {
  {
    .id = "network-http-action",
    .icon_name = "network-transmit-symbolic",
    .name = N_("HTTP Request"),
    .description = N_("Send GET and POST requests to a webserver"),
  },
};

static BsAction *
network_action_factory_create_action (BsActionFactory *action_factory,
                                      BsActionInfo    *action_info)
{
  NetworkActionFactory *self = (NetworkActionFactory *)action_factory;

  g_assert (NETWORK_IS_ACTION_FACTORY (self));

  if (g_strcmp0 (bs_action_info_get_id (action_info), "network-http-action") == 0)
    return network_http_action_new (self->session);

  return NULL;
}

static void
network_action_factory_finalize (GObject *object)
{
  NetworkActionFactory *self = (NetworkActionFactory *)object;

  g_clear_object (&self->session);

  G_OBJECT_CLASS (network_action_factory_parent_class)->finalize (object);
}

static void
network_action_factory_class_init (NetworkActionFactoryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  BsActionFactoryClass *action_factory_class = BS_ACTION_FACTORY_CLASS (klass);

  object_class->finalize = network_action_factory_finalize;

  action_factory_class->create_action = network_action_factory_create_action;
}

static void
network_action_factory_init (NetworkActionFactory *self)
{
  self->session = soup_session_new ();

  bs_action_factory_add_action_entries (BS_ACTION_FACTORY (self),
                                        entries,
                                        G_N_ELEMENTS (entries));
}
