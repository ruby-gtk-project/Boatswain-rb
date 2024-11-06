/* bs-action-factory.c
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

#include "bs-action-factory.h"
#include "bs-action-info.h"
#include "bs-action-private.h"

#include <glib/gi18n.h>

typedef struct
{
  PeasPluginInfo *plugin_info;
  GListModel *action_infos;
} BsActionFactoryPrivate;

static void g_list_model_iface_init (GListModelInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (BsActionFactory, bs_action_factory, PEAS_TYPE_EXTENSION_BASE,
                                  G_ADD_PRIVATE (BsActionFactory)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_iface_init))


/*
 * GListModel interface
 */

static GType
bs_action_factory_get_item_type (GListModel *model)
{
  return BS_TYPE_ACTION_INFO;
}

static uint32_t
bs_action_factory_get_n_items (GListModel *model)
{
  BsActionFactory *self = (BsActionFactory *) model;
  BsActionFactoryPrivate *priv = bs_action_factory_get_instance_private (self);

  g_assert (BS_IS_ACTION_FACTORY (self));

  return g_list_model_get_n_items (priv->action_infos);
}

static gpointer
bs_action_factory_get_item (GListModel *model,
                            uint32_t    position)
{
  BsActionFactory *self = (BsActionFactory *) model;
  BsActionFactoryPrivate *priv = bs_action_factory_get_instance_private (self);

  g_assert (BS_IS_ACTION_FACTORY (self));

  return g_list_model_get_item (priv->action_infos, position);
}

static void
g_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = bs_action_factory_get_item_type;
  iface->get_n_items = bs_action_factory_get_n_items;
  iface->get_item = bs_action_factory_get_item;
}


/*
 * GObject overrides
 */

static void
bs_action_factory_finalize (GObject *object)
{
  BsActionFactory *self = (BsActionFactory *)object;
  BsActionFactoryPrivate *priv = bs_action_factory_get_instance_private (self);

  g_clear_object (&priv->action_infos);

  G_OBJECT_CLASS (bs_action_factory_parent_class)->finalize (object);
}

static void
bs_action_factory_class_init (BsActionFactoryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = bs_action_factory_finalize;
}

static void
bs_action_factory_init (BsActionFactory *self)
{
  BsActionFactoryPrivate *priv = bs_action_factory_get_instance_private (self);

  priv->action_infos = G_LIST_MODEL (g_list_store_new (BS_TYPE_ACTION_INFO));
}

/**
 * bs_action_factory_get_info:
 * @self: a #BsAction
 * @id: id of the action
 *
 * Retrieves the #BsActionInfo instance with the corresponding @id
 *
 * Returns: (transfer none)(nullable): a #BsActionInfo
 */
BsActionInfo *
bs_action_factory_get_info (BsActionFactory *self,
                            const char      *id)
{
  g_return_val_if_fail (BS_IS_ACTION_FACTORY (self), NULL);
  g_return_val_if_fail (id != NULL, NULL);

  for (uint32_t i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self)); i++)
    {
      g_autoptr (BsActionInfo) info = g_list_model_get_item (G_LIST_MODEL (self), i);

      if (g_strcmp0 (bs_action_info_get_id (info), id) == 0)
        return g_steal_pointer (&info);
    }

  return NULL;
}

/**
 * bs_action_factory_create_action:
 * @self: a #BsAction
 * @action_info: a #BsActionInfo
 *
 * Creates an instance of the action represented by #BsActionInfo.
 *
 * Returns: (transfer full)(nullable): a #BsAction
 */
BsAction *
bs_action_factory_create_action (BsActionFactory *self,
                                 BsActionInfo    *action_info)
{
  BsAction *action;

  g_return_val_if_fail (BS_IS_ACTION_FACTORY (self), NULL);
  g_return_val_if_fail (BS_ACTION_FACTORY_GET_CLASS (self)->create_action, NULL);

  action = BS_ACTION_FACTORY_GET_CLASS (self)->create_action (self,
                                                              action_info);

  if (!action)
    return NULL;

  bs_action_set_id (action, bs_action_info_get_id (action_info));
  bs_action_set_name (action, bs_action_info_get_name (action_info));
  bs_action_set_factory (action, self);

  return action;
}

void
bs_action_factory_add_action (BsActionFactory *self,
                              BsActionInfo    *info)
{
  BsActionFactoryPrivate *priv = bs_action_factory_get_instance_private (self);

  g_return_if_fail (BS_IS_ACTION_FACTORY (self));
  g_return_if_fail (BS_IS_ACTION_INFO (info));

  g_list_store_append (G_LIST_STORE (priv->action_infos), info);
}

void
bs_action_factory_add_action_entries (BsActionFactory     *self,
                                      const BsActionEntry *entries,
                                      size_t               n_entries)
{
  for (size_t i = 0; i < n_entries; i++)
    {
      g_autoptr (BsActionInfo) info = NULL;
      const BsActionEntry *entry;

      entry = &entries[i];

      g_assert (entry->id != NULL);

      info = bs_action_info_new (entry->id,
                                 gettext (entry->name),
                                 gettext (entry->description),
                                 entry->icon_name);

      bs_action_factory_add_action (self, info);
    }
}
