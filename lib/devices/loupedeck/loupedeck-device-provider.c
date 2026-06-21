/*
 * loupedeck-provider.c
 *
 * Copyright 2026 tytan652 <tytan652@tytanium.xyz>
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

#define G_LOG_DOMAIN "Loupedeck Device Provider"

#include "loupedeck-device-provider.h"

#include <glib-object.h>
#include <gudev/gudev.h>
#include <libdex.h>

#include "bs-debug.h"
#include "bs-device.h"
#include "bs-device-provider-private.h"
#include "bs-macros.h"

struct _LoupedeckDeviceProvider
{
  PeasExtensionBase parent_instance;

  GUdevClient *gudev_client;
  gulong gudev_client_uevent_handler;

  GListStore *devices;
  GHashTable *syspaths_to_devices;

  DexFuture *fiber;
  DexCancellable *cancellable;
  DexChannel *added_devices;
  DexChannel *removed_devices;
};

static void g_list_model_interface_init (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (LoupedeckDeviceProvider, loupedeck_device_provider, PEAS_TYPE_EXTENSION_BASE,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_interface_init)
                               G_IMPLEMENT_INTERFACE (BS_TYPE_DEVICE_PROVIDER, NULL))

enum {
  LOUPEDECK_LTD_VENDOR_ID = 0x2ec2,
  RAZER_INC_VENDOR_ID = 0x1532,
};


/*
 * Auxiliary methods
 */

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

static void
enumerate_devices (LoupedeckDeviceProvider *self)
{
  g_autoptr (GUdevEnumerator) gudev_enumerator = NULL;
  g_autolist (GUdevDevice) gudev_devices = NULL;

  BS_ENTRY;

  gudev_enumerator = g_udev_enumerator_new (self->gudev_client);
  g_udev_enumerator_add_match_property (gudev_enumerator, "DEVTYPE", "usb_device");
  gudev_devices = g_udev_enumerator_execute (gudev_enumerator);

  for (GList *list = gudev_devices; list; list = list->next)
    dex_future_disown (dex_channel_send (self->added_devices,
                                         dex_future_new_for_object (list->data)));

  BS_EXIT;
}

static GType
find_loupedeck_device_gtype (uint16_t product_id)
{
  const struct {
    uint16_t product_id;
    GType gtype;
  } device_vtable[] = {};

  for (size_t i = 0; i < G_N_ELEMENTS (device_vtable); i++)
    {
      if (device_vtable[i].product_id == product_id)
        return device_vtable[i].gtype;
    }

  return G_TYPE_NONE;
}

static GType
find_razer_device_gtype (uint16_t product_id)
{
  const struct {
    uint16_t product_id;
    GType gtype;
  } device_vtable[] = {};

  for (size_t i = 0; i < G_N_ELEMENTS (device_vtable); i++)
    {
      if (device_vtable[i].product_id == product_id)
        return device_vtable[i].gtype;
    }

  return G_TYPE_NONE;
}

static void
add_device (LoupedeckDeviceProvider *self,
            GUdevDevice             *gudev_device)
{
  const char *vendor_id;
  const char *product_id;
  GType device_type;
  g_autoptr (BsDevice) device = NULL;
  g_autoptr (GError) error = NULL;

  BS_ENTRY;

  g_assert (BS_IS_MAIN_THREAD ());

  if (!is_gudev_device_suitable (gudev_device))
    BS_RETURN ();

  // NOTE: ID_VENDOR_ID, ID_USB_VENDOR_ID, ID_MODEL_ID, ID_USB_MODEL_ID properties return NULL
  vendor_id = g_udev_device_get_sysfs_attr (gudev_device, "idVendor");
  product_id = g_udev_device_get_sysfs_attr (gudev_device, "idProduct");

  g_assert (vendor_id != NULL);
  g_assert (product_id != NULL);

  switch (usb_id_str_to_hex (vendor_id))
    {
    case LOUPEDECK_LTD_VENDOR_ID:
      device_type = find_loupedeck_device_gtype (usb_id_str_to_hex (product_id));
      break;

    case RAZER_INC_VENDOR_ID:
      device_type = find_razer_device_gtype (usb_id_str_to_hex (product_id));
      break;

    default:
      BS_RETURN ();
    }

  if (device_type == G_TYPE_NONE)
    BS_RETURN ();

  // TODO: Implement LoupedeckDevice abstract class
  g_assert_not_reached ();

  BS_EXIT;
}


/*
 * Callbacks
 */

static DexFuture *
devices_changed_fiber (gpointer data)
{
  LoupedeckDeviceProvider *self = data;
  DexFuture *removed_device = NULL;
  DexFuture *added_device = NULL;

  g_assert(LOUPEDECK_IS_DEVICE_PROVIDER (self));
  g_assert (BS_IS_MAIN_THREAD ());

  while (TRUE)
    {
      if (!added_device)
        added_device = dex_channel_receive (self->added_devices);

      if (!removed_device)
        removed_device = dex_channel_receive (self->removed_devices);

      dex_await (dex_future_first (dex_ref (self->cancellable),
                                   dex_ref (added_device),
                                   dex_ref (removed_device),
                                   NULL),
                 NULL);

      if (dex_future_is_resolved (added_device))
        {
          g_autoptr (GUdevDevice) device = NULL;
          g_autoptr (GError) error = NULL;

          device = dex_await_object (g_steal_pointer (&added_device),
                                     &error);
          if (error)
            return dex_future_new_for_error (g_steal_pointer (&error));

          add_device (self, device);
        }

      if (dex_future_is_resolved (removed_device))
        {
          g_autoptr (GUdevDevice) gudev_device = NULL;
          const char *syspath;
          BsDevice *device = NULL;
          size_t i = 0;
          g_autoptr (GError) error = NULL;

          gudev_device = dex_await_object (g_steal_pointer (&removed_device),
                                           &error);
          if (error)
            return dex_future_new_for_error (g_steal_pointer (&error));

          syspath = g_udev_device_get_sysfs_path (gudev_device);
          device = g_hash_table_lookup (self->syspaths_to_devices, syspath);

          if (device)
            {
              while (i < g_list_model_get_n_items (G_LIST_MODEL (self->devices)))
                {
                  g_autoptr (BsDevice) d = NULL;

                  d = g_list_model_get_item (G_LIST_MODEL (self->devices), i);
                  if (device == d)
                    {
                      g_message ("Removing device %s", syspath);
                      g_hash_table_remove (self->syspaths_to_devices, syspath);
                      g_list_store_remove (self->devices, i);
                      break;
                    }
                  i++;
                }
            }
        }

      if (dex_future_is_rejected (DEX_FUTURE (self->cancellable)))
        return NULL;
    }

  return NULL;
}

static void
on_gudev_client_uevent_cb (GUdevClient             *gudev_client,
                           const char              *uevent_action,
                           GUdevDevice             *gudev_device,
                           LoupedeckDeviceProvider *self)
{
  static const char *supported_uevent_actions[] = {
    "add",
    "remove",
    NULL
  };
  DexChannel *channel = NULL;

  BS_ENTRY;

  if (!g_strv_contains (supported_uevent_actions, uevent_action))
    BS_RETURN ();

  if (!is_gudev_device_suitable (gudev_device))
    BS_RETURN ();

  if (g_str_equal (uevent_action, "add"))
    channel = self->added_devices;

  if (g_str_equal (uevent_action, "remove"))
    channel = self->removed_devices;

  g_assert (channel != NULL);

  dex_future_disown (dex_channel_send (channel,
                                       dex_future_new_for_object (gudev_device)));

  BS_EXIT;
}

static void
on_devices_items_changed_cb (GListModel      *model,
                             unsigned int     position,
                             unsigned int     removed,
                             unsigned int     added,
                             LoupedeckDeviceProvider *self)
{
  g_assert (BS_IS_MAIN_THREAD ());

  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
}


/*
 * GListModel interface
 */

static GType
loupedeck_device_provider_get_item_type (GListModel *model)
{
  return BS_TYPE_DEVICE;
}

static gpointer
loupedeck_device_provider_get_item (GListModel *model,
                                    guint       i)
{
  LoupedeckDeviceProvider *self = LOUPEDECK_DEVICE_PROVIDER (model);

  return g_list_model_get_item (G_LIST_MODEL (self->devices), i);
}

static guint
loupedeck_device_provider_get_n_items (GListModel *model)
{
  LoupedeckDeviceProvider *self = LOUPEDECK_DEVICE_PROVIDER (model);

  return g_list_model_get_n_items (G_LIST_MODEL (self->devices));
}

static void
g_list_model_interface_init (GListModelInterface *iface)
{
  iface->get_item_type = loupedeck_device_provider_get_item_type;
  iface->get_item = loupedeck_device_provider_get_item;
  iface->get_n_items = loupedeck_device_provider_get_n_items;
}


/*
 * GObject overrides
 */

static void
loupedeck_device_provider_finalize (GObject *object)
{
  LoupedeckDeviceProvider *self = LOUPEDECK_DEVICE_PROVIDER (object);

  if (self->gudev_client_uevent_handler)
    g_signal_handler_disconnect (self->gudev_client,
                                 self->gudev_client_uevent_handler);

  if (self->fiber)
    {
      dex_cancellable_cancel (self->cancellable);
      dex_await (g_steal_pointer (&self->fiber), NULL);
    }

  dex_clear (&self->added_devices);
  dex_clear (&self->removed_devices);
  dex_clear (&self->cancellable);

  g_clear_object (&self->devices);
  g_clear_object (&self->gudev_client);

  g_clear_pointer (&self->syspaths_to_devices, g_hash_table_unref);

  G_OBJECT_CLASS (loupedeck_device_provider_parent_class)->finalize (object);
}

static void
loupedeck_device_provider_class_init (LoupedeckDeviceProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = loupedeck_device_provider_finalize;
}

static void
loupedeck_device_provider_init (LoupedeckDeviceProvider *self)
{
  static const char * const gudev_subsystems[] = { "usb", NULL };

  self->devices = g_list_store_new (BS_TYPE_DEVICE);
  g_signal_connect (self->devices, "items-changed", G_CALLBACK (on_devices_items_changed_cb), self);

  self->syspaths_to_devices = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  self->gudev_client = g_udev_client_new (gudev_subsystems);
  if (self->gudev_client)
    {
      self->cancellable = dex_cancellable_new ();
      self->added_devices = dex_channel_new (0);
      self->removed_devices = dex_channel_new (0);

      enumerate_devices (self);
      self->gudev_client_uevent_handler = g_signal_connect (self->gudev_client,
                                                            "uevent",
                                                            G_CALLBACK (on_gudev_client_uevent_cb),
                                                            self);

      self->fiber =
        dex_scheduler_spawn (NULL, 0, devices_changed_fiber, self, NULL);
    }
}
