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
#include <libdex.h>

#define ELGATO_SYSTEMS_VENDOR_ID (0x0fd9)

struct _ElgatoDeviceProvider
{
  PeasExtensionBase parent_instance;

  guint device_added_idle_id;
  guint device_removed_idle_id;

  GListStore *devices;
  GUsbContext *gusb_context;

  DexChannel *added_devices;
  DexChannel *removed_devices;
  DexPromise *quit_fiber;
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


static DexFuture *
devices_changed_fiber (gpointer data)
{
  ElgatoDeviceProvider *self = (ElgatoDeviceProvider *) data;
  DexFuture *removed_device_future = NULL;
  DexFuture *added_device_future = NULL;

  g_assert (ELGATO_IS_DEVICE_PROVIDER (self));
  g_assert (BS_IS_MAIN_THREAD ());

  while (TRUE)
    {
      if (!added_device_future)
        added_device_future = dex_channel_receive (self->added_devices);

      if (!removed_device_future)
        removed_device_future = dex_channel_receive (self->removed_devices);

      if (dex_await (dex_future_any (dex_ref (self->quit_fiber),
                                     dex_ref (added_device_future),
                                     dex_ref (removed_device_future),
                                     NULL),
                     NULL))
        {
          if (dex_future_is_resolved (added_device_future))
            {
              g_autoptr (ElgatoStreamDeck) stream_deck = NULL;
              g_autoptr (GUsbDevice) gusb_device = NULL;
              g_autoptr (GError) error = NULL;
              GType device_type;

              gusb_device = dex_await_object (g_steal_pointer (&added_device_future), &error);
              if (error)
                return dex_future_new_for_error (g_steal_pointer (&error));

              g_assert (g_usb_device_get_vid (gusb_device) == ELGATO_SYSTEMS_VENDOR_ID);

              device_type = find_elgato_device_gtype (g_usb_device_get_pid (gusb_device));
              g_assert (device_type != G_TYPE_NONE);

              stream_deck = g_initable_new (device_type,
                                       NULL,
                                       &error,
                                       "gusb-device", gusb_device,
                                       NULL);

              g_list_store_append (self->devices, stream_deck);
            }

          if (dex_future_is_resolved (removed_device_future))
            {
              g_autoptr (GUsbDevice) gusb_device = NULL;
              g_autoptr (GError) error = NULL;

              gusb_device = dex_await_object (g_steal_pointer (&removed_device_future), &error);
              if (error)
                return dex_future_new_for_error (g_steal_pointer (&error));

              g_assert (g_usb_device_get_vid (gusb_device) == ELGATO_SYSTEMS_VENDOR_ID);

              for (unsigned int i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->devices)); i++)
                {
                  g_autoptr (ElgatoStreamDeck) stream_deck = g_list_model_get_item (G_LIST_MODEL (self->devices), i);

                  if (elgato_stream_deck_get_gusb_device (stream_deck) == gusb_device)
                    {
                      g_debug ("Removing device %p", stream_deck);
                      g_list_store_remove (self->devices, i);
                      break;
                    }
                }
            }

          if (dex_future_is_resolved (DEX_FUTURE (self->quit_fiber)))
            return NULL;
        }
    }

  return NULL;
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

  if (!dex_await (dex_channel_send (self->added_devices,
                                    dex_future_new_for_object (usb_device)),
                  NULL))
    g_assert_not_reached ();

  BS_EXIT;
}

static void
on_gusb_context_device_removed_cb (GUsbContext     *gusb_context,
                                   GUsbDevice      *gusb_device,
                                   ElgatoDeviceProvider *self)
{
  BS_ENTRY;

  if (g_usb_device_get_vid (gusb_device) != ELGATO_SYSTEMS_VENDOR_ID)
    BS_RETURN ();

  if (!dex_await (dex_channel_send (self->removed_devices,
                                    dex_future_new_for_object (gusb_device)),
                  NULL))
    g_assert_not_reached ();

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

  if (self->quit_fiber)
    {
      dex_promise_resolve_boolean (self->quit_fiber, TRUE);
      g_clear_pointer (&self->quit_fiber, dex_unref);
    }

  g_clear_pointer (&self->added_devices, dex_unref);
  g_clear_pointer (&self->removed_devices, dex_unref);

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
      self->quit_fiber = dex_promise_new ();
      self->added_devices = dex_channel_new (0);
      self->removed_devices = dex_channel_new (0);

      enumerate_devices (self);
      g_signal_connect (self->gusb_context, "device-added", G_CALLBACK (on_gusb_context_device_added_cb), self);
      g_signal_connect (self->gusb_context, "device-removed", G_CALLBACK (on_gusb_context_device_removed_cb), self);

      dex_future_disown (dex_scheduler_spawn (NULL, 0, devices_changed_fiber, self, NULL));
    }

  g_signal_connect (self->devices, "items-changed", G_CALLBACK (on_devices_items_changed_cb), self);
}
