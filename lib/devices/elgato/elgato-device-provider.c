/*
 * elgato-device-provider.c
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

#define G_LOG_DOMAIN "Elgato Device Provider"

#include "bs-debug.h"
#include "bs-device-provider-private.h"
#include "elgato-device-provider.h"
#include "elgato-stream-deck.h"

#include <gusb.h>

struct _ElgatoDeviceProvider
{
  PeasExtensionBase parent_instance;

  GListStore *devices;
  GUsbContext *gusb_context;
};

static void g_list_model_interface_init (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (ElgatoDeviceProvider, elgato_device_provider, PEAS_TYPE_EXTENSION_BASE,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_interface_init)
                               G_IMPLEMENT_INTERFACE (BS_TYPE_DEVICE_PROVIDER, NULL))


/*
 * Auxiliary methods
 */

static void
enumerate_devices (ElgatoDeviceProvider *self)
{
  g_autoptr (GPtrArray) devices = NULL;

  g_usb_context_enumerate (self->gusb_context);

  devices = g_usb_context_get_devices (self->gusb_context);
  for (unsigned int i = 0; devices && i < devices->len; i++)
    {
      g_autoptr (BsDevice) device = NULL;
      g_autoptr (GError) error = NULL;
      GUsbDevice *usb_device;

      usb_device = g_ptr_array_index (devices, i);
      device = elgato_stream_deck_new (usb_device, &error);

      if (error)
        {
          if (!g_error_matches (error, BS_DEVICE_ERROR, BS_DEVICE_ERROR_UNRECOGNIZED))
            g_warning ("Error opening device: %s", error->message);
          continue;
        }

      g_debug ("Found %s (%s) at bus %hu, port %hu",
               bs_device_get_name (device),
               bs_device_get_serial_number (device),
               g_usb_device_get_bus (usb_device),
               g_usb_device_get_port_number (usb_device));

      g_list_store_append (self->devices, device);
    }
}


/*
 * Callbacks
 */

static void
on_gusb_context_device_added_cb (GUsbContext          *gusb_context,
                                 GUsbDevice           *usb_device,
                                 ElgatoDeviceProvider *self)
{
  g_autoptr (BsDevice) device = NULL;
  g_autoptr (GError) error = NULL;

  BS_ENTRY;

  device = elgato_stream_deck_new (usb_device, &error);

  if (error)
    {
      if (!g_error_matches (error, BS_DEVICE_ERROR, BS_DEVICE_ERROR_UNRECOGNIZED))
        g_warning ("Error opening device: %s", error->message);
      BS_RETURN ();
    }

  g_list_store_append (self->devices, g_object_ref (device));

  BS_EXIT;
}

static void
on_gusb_context_device_removed_cb (GUsbContext     *gusb_context,
                                   GUsbDevice      *gusb_device,
                                   ElgatoDeviceProvider *self)
{
  unsigned int i = 0;

  BS_ENTRY;

  while (i < g_list_model_get_n_items (G_LIST_MODEL (self->devices)))
    {
      g_autoptr (ElgatoStreamDeck) stream_deck = NULL;
      GUsbDevice *d;

      stream_deck = g_list_model_get_item (G_LIST_MODEL (self->devices), i);
      d = elgato_stream_deck_get_gusb_device (stream_deck);

      if (d == gusb_device)
        {
          g_message ("Removing device %p", stream_deck);
          g_list_store_remove (self->devices, i);
          continue;
        }

      i++;
    }

  BS_EXIT;
}

static void
on_devices_items_changed_cb (GListModel      *model,
                             unsigned int     position,
                             unsigned int     removed,
                             unsigned int     added,
                             ElgatoDeviceProvider *self)
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
  ElgatoDeviceProvider *self = ELGATO_DEVICE_PROVIDER (model);
  return g_list_model_get_item (G_LIST_MODEL (self->devices), i);
}

static guint
bs_device_manager_get_n_items (GListModel *model)
{
  ElgatoDeviceProvider *self = ELGATO_DEVICE_PROVIDER (model);
  return g_list_model_get_n_items (G_LIST_MODEL (self->devices));
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
elgato_device_provider_finalize (GObject *object)
{
  ElgatoDeviceProvider *self = (ElgatoDeviceProvider *)object;

  g_clear_object (&self->devices);
  g_clear_object (&self->gusb_context);

  G_OBJECT_CLASS (elgato_device_provider_parent_class)->finalize (object);
}

static void
elgato_device_provider_class_init (ElgatoDeviceProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = elgato_device_provider_finalize;
}

static void
elgato_device_provider_init (ElgatoDeviceProvider *self)
{
  self->devices = g_list_store_new (BS_TYPE_DEVICE);

  self->gusb_context = g_usb_context_new (NULL);
  if (self->gusb_context)
    {
      enumerate_devices (self);
      g_signal_connect (self->gusb_context, "device-added", G_CALLBACK (on_gusb_context_device_added_cb), self);
      g_signal_connect (self->gusb_context, "device-removed", G_CALLBACK (on_gusb_context_device_removed_cb), self);
    }

  g_signal_connect (self->devices, "items-changed", G_CALLBACK (on_devices_items_changed_cb), self);
}
