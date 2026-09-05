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

#include <glib-unix.h>
#include <gudev/gudev.h>
#include <libdex.h>

#define ELGATO_SYSTEMS_VENDOR_ID (0x0fd9)

struct _ElgatoDeviceProvider
{
  PeasExtensionBase parent_instance;

  GUdevClient *gudev_client;
  gulong gudev_client_uevent_handler;

  GListStore *devices;
  GHashTable *syspaths_to_devices;

  DexFuture *fiber;
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

static gboolean
is_gudev_device_suitable (GUdevDevice *device)
{
  const char *devtype = NULL;

  g_assert (G_UDEV_IS_DEVICE (device));
  g_return_val_if_fail (g_str_equal (g_udev_device_get_subsystem (device), "usb"), FALSE);
  g_return_val_if_fail (g_udev_device_get_sysfs_path (device) != NULL, FALSE);

  devtype = g_udev_device_get_property (device, "DEVTYPE");
  if (!devtype || g_strcmp0 (devtype, "usb_device"))
    return FALSE;

  if (!g_udev_device_get_device_file (device))
    return FALSE;

  return TRUE;
}

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

static uint16_t
usb_id_str_to_hex (const char *id)
{
  char *end;
  guint64 ret;
  g_return_val_if_fail (strlen (id) == 4, 0);
  ret = g_ascii_strtoull (id, &end, 16);
  g_return_val_if_fail (end - id == 4, 0);
  g_return_val_if_fail (ret <= UINT16_MAX, 0);
  return (uint16_t) ret;
}

static void
add_device (GUdevDevice          *gudev_device,
            ElgatoDeviceProvider *self)
{
  const char *vendor_id;
  const char *product_id;
  const char *device_file;
  const char *syspath;
  GType device_type;
  g_autoptr (BsDevice) device = NULL;
  g_autofd int fd = -1;
  g_autoptr (GError) error = NULL;

  BS_ENTRY;

  g_assert (BS_IS_MAIN_THREAD ());

  if (!is_gudev_device_suitable (gudev_device))
    BS_RETURN ();

  // NOTE: ID_VENDOR_ID, ID_USB_VENDOR_ID, ID_MODEL_ID, ID_USB_MODEL_ID properties return NULL
  vendor_id = g_udev_device_get_sysfs_attr (gudev_device, "idVendor");
  product_id = g_udev_device_get_sysfs_attr (gudev_device, "idProduct");
  device_file = g_udev_device_get_device_file (gudev_device);
  syspath = g_udev_device_get_sysfs_path (gudev_device);

  g_assert (vendor_id != NULL);
  g_assert (product_id != NULL);

  if (usb_id_str_to_hex (vendor_id) != ELGATO_SYSTEMS_VENDOR_ID)
    BS_RETURN ();

  device_type = find_elgato_device_gtype (usb_id_str_to_hex (product_id));

  if (device_type == G_TYPE_NONE)
    BS_RETURN ();

  fd = open (device_file, O_RDWR | O_CLOEXEC);

  if (fd == -1)
    {
      g_warning ("Failed to open device file, %s", g_strerror (errno));
      BS_RETURN ();
    }

  device = g_initable_new (device_type,
                           NULL,
                           &error,
                           "usb-device-fd", g_steal_fd (&fd),
                           NULL);

  g_debug ("Found %s (%s) at %s",
           bs_device_get_name (device),
           bs_device_get_serial_number (device),
           syspath);

  g_list_store_append (self->devices, device);
  g_hash_table_insert (self->syspaths_to_devices, g_strdup (syspath), device);

  BS_EXIT;
}

static void
enumerate_devices (ElgatoDeviceProvider *self)
{
  g_autoptr (GUdevEnumerator) gudev_enumerator = NULL;
  g_autolist (GUdevDevice) gudev_devices = NULL;

  BS_ENTRY;

  gudev_enumerator = g_udev_enumerator_new (self->gudev_client);
  g_udev_enumerator_add_match_property (gudev_enumerator, "DEVTYPE", "usb_device");
  gudev_devices = g_udev_enumerator_execute (gudev_enumerator);
  g_list_foreach (gudev_devices, (GFunc) add_device, self);

  BS_EXIT;
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
              g_autoptr (GUdevDevice) gudev_device = NULL;
              g_autoptr (GError) error = NULL;

              gudev_device = dex_await_object (g_steal_pointer (&added_device_future), &error);
              if (error)
                return dex_future_new_for_error (g_steal_pointer (&error));

              add_device (gudev_device, self);
            }

          if (dex_future_is_resolved (removed_device_future))
            {
              g_autoptr (GUdevDevice) gudev_device = NULL;
              const char *syspath;
              BsDevice *device = NULL;
              g_autoptr (GError) error = NULL;

              gudev_device = dex_await_object (g_steal_pointer (&removed_device_future), &error);
              if (error)
                return dex_future_new_for_error (g_steal_pointer (&error));

              syspath = g_udev_device_get_sysfs_path (gudev_device);
              device = g_hash_table_lookup (self->syspaths_to_devices, syspath);

              g_assert (device != NULL);

              for (unsigned int i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->devices)); i++)
                {
                  g_autoptr (BsDevice) stream_deck = g_list_model_get_item (G_LIST_MODEL (self->devices), i);

                  if (stream_deck == device)
                    {
                      g_message ("Removing device %s", syspath);
                      g_hash_table_remove (self->syspaths_to_devices, syspath);
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
on_gudev_client_uevent_cb (GUdevClient          *gudev_client,
                           const char           *uevent_action,
                           GUdevDevice          *gudev_device,
                           ElgatoDeviceProvider *self)
{
  static const char *supported_uevent_actions[] = {
    "add",
    "remove",
    NULL
  };

  BS_ENTRY;

  if (!g_strv_contains (supported_uevent_actions, uevent_action))
    BS_RETURN ();

  if (!is_gudev_device_suitable (gudev_device))
    BS_RETURN ();

  if (g_str_equal (uevent_action, "add"))
    {
      const char *vendor_id;
      const char *product_id;
      GType device_type;

      // NOTE: ID_VENDOR_ID, ID_USB_VENDOR_ID, ID_MODEL_ID, ID_USB_MODEL_ID properties return NULL
      vendor_id = g_udev_device_get_sysfs_attr (gudev_device, "idVendor");
      product_id = g_udev_device_get_sysfs_attr (gudev_device, "idProduct");

      g_assert (vendor_id != NULL);
      g_assert (product_id != NULL);

      if (usb_id_str_to_hex (vendor_id) != ELGATO_SYSTEMS_VENDOR_ID)
        BS_RETURN ();

      device_type = find_elgato_device_gtype (usb_id_str_to_hex (product_id));
      if (device_type == G_TYPE_NONE)
        BS_RETURN ();

      dex_future_disown (dex_channel_send (self->added_devices,
                                           dex_future_new_for_object (gudev_device)));
      BS_RETURN ();
    }

  if (g_str_equal (uevent_action, "remove"))
    {
      const char *syspath;

      syspath = g_udev_device_get_sysfs_path (gudev_device);

      if (g_hash_table_contains (self->syspaths_to_devices, syspath))
        dex_future_disown (dex_channel_send (self->removed_devices,
                                             dex_future_new_for_object (gudev_device)));

      BS_RETURN ();
    }

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

  if (self->gudev_client_uevent_handler)
    g_signal_handler_disconnect (self->gudev_client, self->gudev_client_uevent_handler);

  if (self->fiber)
    {
      dex_promise_resolve_boolean (self->quit_fiber, TRUE);
      dex_await (g_steal_pointer (&self->fiber), NULL);
    }

  g_clear_pointer (&self->added_devices, dex_unref);
  g_clear_pointer (&self->removed_devices, dex_unref);
  g_clear_pointer (&self->quit_fiber, dex_unref);

  g_clear_object (&self->devices);
  g_clear_object (&self->gudev_client);

  g_hash_table_unref (self->syspaths_to_devices);

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
  static const char * const gudev_subsystems[] = { "usb", NULL };

  self->devices = g_list_store_new (BS_TYPE_DEVICE);
  self->syspaths_to_devices = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  self->gudev_client = g_udev_client_new (gudev_subsystems);
  if (self->gudev_client)
    {
      self->quit_fiber = dex_promise_new ();
      self->added_devices = dex_channel_new (0);
      self->removed_devices = dex_channel_new (0);

      enumerate_devices (self);
      self->gudev_client_uevent_handler = g_signal_connect (self->gudev_client,
                                                            "uevent",
                                                            G_CALLBACK (on_gudev_client_uevent_cb),
                                                            self);

      self->fiber = dex_scheduler_spawn (NULL, 0, devices_changed_fiber, self, NULL);
    }

  g_signal_connect (self->devices, "items-changed", G_CALLBACK (on_devices_items_changed_cb), self);
}
