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
#include "bs-macros.h"
#include "elgato-device-provider.h"
#include "elgato-stream-deck.h"
#include "elgato-stream-deck-mini.h"
#include "elgato-stream-deck-mk2.h"
#include "elgato-stream-deck-neo.h"
#include "elgato-stream-deck-original.h"
#include "elgato-stream-deck-original-v2.h"
#include "elgato-stream-deck-pedal.h"
#include "elgato-stream-deck-plus.h"
#include "elgato-stream-deck-xl.h"

#include <gusb.h>

#define ELGATO_SYSTEMS_VENDOR_ID (0x0fd9)

struct _ElgatoDeviceProvider
{
  PeasExtensionBase parent_instance;

  guint device_added_idle_id;
  guint device_removed_idle_id;

  GListStore *devices;
  GUsbContext *gusb_context;

  struct {
    GMutex mutex;
    guint idle_id;
    GType device_type;
    GUsbDevice *usb_device;
  } added;

  struct {
    GMutex mutex;
    guint idle_id;
    guint position;
  } removed;
};

static void g_list_model_interface_init (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (ElgatoDeviceProvider, elgato_device_provider, PEAS_TYPE_EXTENSION_BASE,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_interface_init)
                               G_IMPLEMENT_INTERFACE (BS_TYPE_DEVICE_PROVIDER, NULL))


/*
 * Auxiliary methods
 */

static GType
find_elgato_device_gtype (uint16_t product_id)
{
  const struct {
    uint16_t product_id;
    GType gtype;
  } device_vtable[] = {
    { 0x0060, ELGATO_TYPE_STREAM_DECK_ORIGINAL },
    { 0x0063, ELGATO_TYPE_STREAM_DECK_MINI },
    { 0x006c, ELGATO_TYPE_STREAM_DECK_XL },
    { 0x006d, ELGATO_TYPE_STREAM_DECK_ORIGINAL_V2 },
    { 0x0080, ELGATO_TYPE_STREAM_DECK_MK2 },
    { 0x0084, ELGATO_TYPE_STREAM_DECK_PLUS },
    { 0x0086, ELGATO_TYPE_STREAM_DECK_PEDAL },
    { 0x008f, ELGATO_TYPE_STREAM_DECK_XL },
    { 0x0090, ELGATO_TYPE_STREAM_DECK_MINI },
    { 0x009a, ELGATO_TYPE_STREAM_DECK_NEO },
  };

  for (size_t i = 0; i < G_N_ELEMENTS (device_vtable); i++)
    {
      if (device_vtable[i].product_id == product_id)
        return device_vtable[i].gtype;
    }

  return G_TYPE_NONE;
}

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
      GType device_type;

      usb_device = g_ptr_array_index (devices, i);

      if (g_usb_device_get_vid (usb_device) != ELGATO_SYSTEMS_VENDOR_ID)
        continue;

      device_type = find_elgato_device_gtype (g_usb_device_get_pid (usb_device));
      if (device_type == G_TYPE_NONE)
        continue;

      device = g_initable_new (device_type,
                               NULL,
                               &error,
                               "gusb-device", usb_device,
                               NULL);

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

static gboolean
device_added_in_idle_cb (gpointer user_data)
{
  ElgatoDeviceProvider *self = user_data;
  g_autoptr (BsDevice) device = NULL;
  g_autoptr (GError) error = NULL;

  BS_ENTRY;

  g_assert(BS_IS_MAIN_THREAD ());

  G_MUTEX_AUTO_LOCK (&self->added.mutex, locker);

  device = g_initable_new (self->added.device_type,
                           NULL,
                           &error,
                           "gusb-device", self->added.usb_device,
                           NULL);

  g_list_store_append (self->devices, device);

  self->added.idle_id = 0;
  self->added.device_type = G_TYPE_NONE;
  g_clear_object (&self->added.usb_device);

  BS_RETURN (G_SOURCE_REMOVE);
}

static void
on_gusb_context_device_added_cb (GUsbContext          *gusb_context,
                                 GUsbDevice           *usb_device,
                                 ElgatoDeviceProvider *self)
{
  GType device_type;

  BS_ENTRY;

  if (g_usb_device_get_vid (usb_device) != ELGATO_SYSTEMS_VENDOR_ID)
    BS_RETURN ();

  device_type = find_elgato_device_gtype (g_usb_device_get_pid (usb_device));
  if (device_type == G_TYPE_NONE)
    BS_RETURN ();

  g_mutex_lock (&self->added.mutex);
  self->added.device_type = device_type;
  self->added.usb_device = g_object_ref (usb_device);

  self->added.idle_id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                                         device_added_in_idle_cb,
                                         g_object_ref (self),
                                         g_object_unref);
  g_mutex_unlock (&self->added.mutex);

  BS_EXIT;
}

static gboolean
device_removed_in_idle_cb (gpointer user_data)
{
  ElgatoDeviceProvider *self = user_data;
  g_autoptr (BsDevice) device = NULL;
  g_autoptr (GError) error = NULL;

  BS_ENTRY;

  g_assert(BS_IS_MAIN_THREAD ());

  G_MUTEX_AUTO_LOCK (&self->removed.mutex, locker);

  g_list_store_remove (self->devices, self->removed.position);

  self->removed.idle_id = 0;
  self->removed.position = 0;

  BS_RETURN (G_SOURCE_REMOVE);
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
          g_mutex_lock (&self->removed.mutex);
          self->removed.position = i;

          self->removed.idle_id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                                                   device_removed_in_idle_cb,
                                                   g_object_ref (self),
                                                   g_object_unref);
          g_mutex_unlock (&self->removed.mutex);
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
  g_assert (BS_IS_MAIN_THREAD ());

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

  g_mutex_lock (&self->added.mutex);
  g_clear_handle_id (&self->added.idle_id, g_source_remove);
  g_mutex_unlock (&self->added.mutex);

  g_mutex_lock (&self->removed.mutex);
  g_clear_handle_id (&self->removed.idle_id, g_source_remove);
  g_mutex_unlock (&self->removed.mutex);

  g_clear_object (&self->devices);
  g_clear_object (&self->gusb_context);

  g_mutex_clear (&self->added.mutex);
  g_mutex_clear (&self->removed.mutex);

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

  g_mutex_init (&self->added.mutex);
  g_mutex_init (&self->removed.mutex);

  self->gusb_context = g_usb_context_new (NULL);
  if (self->gusb_context)
    {
      enumerate_devices (self);
      g_signal_connect (self->gusb_context, "device-added", G_CALLBACK (on_gusb_context_device_added_cb), self);
      g_signal_connect (self->gusb_context, "device-removed", G_CALLBACK (on_gusb_context_device_removed_cb), self);
    }

  g_signal_connect (self->devices, "items-changed", G_CALLBACK (on_devices_items_changed_cb), self);
}
