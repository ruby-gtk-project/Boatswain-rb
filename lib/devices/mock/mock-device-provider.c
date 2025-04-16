/*
 * mock-device-provider.c
 *
 * Copyright 2025 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "bs-debug.h"
#include "bs-device-provider-private.h"
#include "mock-device.h"
#include "mock-device-provider.h"

struct _MockDeviceProvider
{
  PeasExtensionBase parent_instance;

  GListStore *devices;
};

static void g_list_model_interface_init (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (MockDeviceProvider, mock_device_provider, PEAS_TYPE_EXTENSION_BASE,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_interface_init)
                               G_IMPLEMENT_INTERFACE (BS_TYPE_DEVICE_PROVIDER, NULL))


/*
 * Callbacks
 */

static void
on_devices_items_changed_cb (GListModel         *model,
                             unsigned int        position,
                             unsigned int        removed,
                             unsigned int        added,
                             MockDeviceProvider *self)
{
  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
}


/*
 * GListModel interface
 */

static GType
bs_mock_device_manager_get_item_type (GListModel *model)
{
  return BS_TYPE_DEVICE;
}

static gpointer
bs_mock_device_manager_get_item (GListModel *model,
                                 guint       i)
{
  MockDeviceProvider *self = MOCK_DEVICE_PROVIDER (model);
  return g_list_model_get_item (G_LIST_MODEL (self->devices), i);
}

static guint
bs_mock_device_manager_get_n_items (GListModel *model)
{
  MockDeviceProvider *self = MOCK_DEVICE_PROVIDER (model);
  return g_list_model_get_n_items (G_LIST_MODEL (self->devices));
}

static void
g_list_model_interface_init (GListModelInterface *iface)
{
  iface->get_item_type = bs_mock_device_manager_get_item_type;
  iface->get_item = bs_mock_device_manager_get_item;
  iface->get_n_items = bs_mock_device_manager_get_n_items;
}


/*
 * GObject overrides
 */

static void
mock_device_provider_finalize (GObject *object)
{
  MockDeviceProvider *self = (MockDeviceProvider *)object;

  g_clear_object (&self->devices);

  G_OBJECT_CLASS (mock_device_provider_parent_class)->finalize (object);
}

static void
mock_device_provider_class_init (MockDeviceProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = mock_device_provider_finalize;
}

static void
mock_device_provider_init (MockDeviceProvider *self)
{
  static const MockDeviceModel models[] = {
    MOCK_DEVICE_MODEL_HANGAR_MILD,
    MOCK_DEVICE_MODEL_HANGAR_XL,
  };
  int n_devices;

  n_devices = MAX (atoi (g_getenv ("BOATSWAIN_N_DEVICES") ?: "1"), 0);

  self->devices = g_list_store_new (BS_TYPE_DEVICE);

  for (int i = 0; i < n_devices; i++)
    {
      g_autoptr (BsDevice) device = mock_device_new (models[i % G_N_ELEMENTS (models)]);

      g_debug ("Created fake device %s (%s)",
               bs_device_get_name (device),
               bs_device_get_serial_number (device));

      g_list_store_append (self->devices, device);
    }

  g_signal_connect (self->devices, "items-changed", G_CALLBACK (on_devices_items_changed_cb), self);
}
