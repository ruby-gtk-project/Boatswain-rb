/* bs-device-manager.c
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

#define G_LOG_DOMAIN "Device Manager"

#include <gtk/gtk.h>
#include <libpeas.h>

#include "bs-config.h"
#include "bs-debug.h"
#include "bs-device-manager-private.h"
#include "bs-device-private.h"
#include "bs-device-provider-private.h"

struct _BsDeviceManager
{
  GObject parent_instance;

  PeasEngine *devices_engine;
  PeasExtensionSet *device_providers;

  GListModel *devices;
  gboolean loaded;
};

static void g_list_model_interface_init (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (BsDeviceManager, bs_device_manager, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_interface_init))

enum
{
  DEVICE_ADDED,
  DEVICE_REMOVED,
  N_SIGNALS,
};

static guint signals[N_SIGNALS] = { 0, };


/*
 * Callbacks
 */

static void
on_devices_items_changed_cb (GListModel      *model,
                             unsigned int     position,
                             unsigned int     removed,
                             unsigned int     added,
                             BsDeviceManager *self)
{
  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
}


/*
 * GListModel interface
 */

static GType
bs_device_manager_get_item_type (GListModel *model)
{
  return BS_TYPE_DEVICE;
}

static gpointer
bs_device_manager_get_item (GListModel *model,
                            guint       i)
{
  BsDeviceManager *self = BS_DEVICE_MANAGER (model);
  return g_list_model_get_item (self->devices, i);
}

static guint
bs_device_manager_get_n_items (GListModel *model)
{
  BsDeviceManager *self = BS_DEVICE_MANAGER (model);
  return g_list_model_get_n_items (self->devices);
}

static void
g_list_model_interface_init (GListModelInterface *iface)
{
  iface->get_item_type = bs_device_manager_get_item_type;
  iface->get_item = bs_device_manager_get_item;
  iface->get_n_items = bs_device_manager_get_n_items;
}


/*
 * GObject overrides
 */

static void
bs_device_manager_finalize (GObject *object)
{
  BsDeviceManager *self = (BsDeviceManager *)object;

  BS_ENTRY;

  g_clear_object (&self->devices);
  g_clear_object (&self->device_providers);
  g_clear_object (&self->devices_engine);

  G_OBJECT_CLASS (bs_device_manager_parent_class)->finalize (object);

  BS_EXIT;
}

static void
bs_device_manager_class_init (BsDeviceManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = bs_device_manager_finalize;

  signals[DEVICE_ADDED] = g_signal_new ("device-added",
                                        BS_TYPE_DEVICE_MANAGER,
                                        G_SIGNAL_RUN_LAST,
                                        0, NULL, NULL, NULL,
                                        G_TYPE_NONE,
                                        1,
                                        BS_TYPE_DEVICE);

  signals[DEVICE_REMOVED] = g_signal_new ("device-removed",
                                          BS_TYPE_DEVICE_MANAGER,
                                          G_SIGNAL_RUN_LAST,
                                          0, NULL, NULL, NULL,
                                          G_TYPE_NONE,
                                          1,
                                          BS_TYPE_DEVICE);
}

static void
bs_device_manager_init (BsDeviceManager *self)
{
}

BsDeviceManager *
bs_device_manager_new (void)
{
  return g_object_new (BS_TYPE_DEVICE_MANAGER, NULL);
}

gboolean
bs_device_manager_load (BsDeviceManager  *self,
                        GError          **error)
{
  g_return_val_if_fail (BS_IS_DEVICE_MANAGER (self), FALSE);
  g_return_val_if_fail (!error || !*error, FALSE);
  g_return_val_if_fail (!self->loaded, FALSE);

  self->devices_engine = peas_engine_new_with_nonglobal_loaders ();
  peas_engine_add_search_path (self->devices_engine,
                               "resource:///com/feaneron/Boatswain/devices",
                               "resource:///com/feaneron/Boatswain/devices");

  for (uint32_t i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->devices_engine)); i++)
    {
      g_autoptr (PeasPluginInfo) plugin_info =
        g_list_model_get_item (G_LIST_MODEL (self->devices_engine), i);

      peas_engine_load_plugin (self->devices_engine, plugin_info);
    }

  self->device_providers = peas_extension_set_new (self->devices_engine,
                                                   BS_TYPE_DEVICE_PROVIDER,
                                                   NULL);

  self->devices =
    G_LIST_MODEL (gtk_flatten_list_model_new (G_LIST_MODEL (g_object_ref (self->device_providers))));
  g_signal_connect (self->devices,
                    "items-changed",
                    G_CALLBACK (on_devices_items_changed_cb),
                    self);

  self->loaded = TRUE;
  return TRUE;
}

