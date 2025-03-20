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

#include <gusb.h>

#include "bs-config.h"
#include "bs-debug.h"
#include "bs-device-manager.h"
#include "bs-stream-deck-private.h"

struct _BsDeviceManager
{
  GObject parent_instance;

  GUsbContext *gusb_context;
  GListStore *stream_decks;
  gboolean emulate_devices;
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
 * Auxiliary methods
 */

static void
enumerate_fake_stream_decks (BsDeviceManager *self)
{
  int n_devices = MAX (atoi (g_getenv ("BOATSWAIN_N_DEVICES") ?: "1"), 0);

  for (int i = 0; i < n_devices; i++)
    {
      g_autoptr (BsStreamDeck) stream_deck = NULL;
      g_autoptr (GError) error = NULL;

      stream_deck = bs_stream_deck_new_fake (&error);

      if (error)
        {
          if (!g_error_matches (error, BS_STREAM_DECK_ERROR, BS_STREAM_DECK_ERROR_UNRECOGNIZED))
            g_warning ("Error opening Stream Deck device: %s", error->message);
          continue;
        }

      g_debug ("Created fake device %s (%s)",
               bs_stream_deck_get_name (stream_deck),
               bs_stream_deck_get_serial_number (stream_deck));

      bs_stream_deck_load (stream_deck);

      g_list_store_append (self->stream_decks, stream_deck);
      g_signal_emit (self, signals[DEVICE_ADDED], 0, stream_deck);
    }
}

static void
enumerate_stream_decks (BsDeviceManager *self)
{
  g_autoptr (GPtrArray) devices = NULL;
  unsigned int i;

  g_usb_context_enumerate (self->gusb_context);

  devices = g_usb_context_get_devices (self->gusb_context);
  for (i = 0; devices && i < devices->len; i++)
    {
      g_autoptr (BsStreamDeck) stream_deck = NULL;
      g_autoptr (GError) error = NULL;
      GUsbDevice *usb_device;

      usb_device = g_ptr_array_index (devices, i);
      stream_deck = bs_stream_deck_new (usb_device, &error);

      if (error)
        {
          if (!g_error_matches (error, BS_STREAM_DECK_ERROR, BS_STREAM_DECK_ERROR_UNRECOGNIZED))
            g_warning ("Error opening Stream Deck device: %s", error->message);
          continue;
        }

      g_debug ("Found %s (%s) at bus %hu, port %hu",
               bs_stream_deck_get_name (stream_deck),
               bs_stream_deck_get_serial_number (stream_deck),
               g_usb_device_get_bus (usb_device),
               g_usb_device_get_port_number (usb_device));

      bs_stream_deck_load (stream_deck);

      g_list_store_append (self->stream_decks, stream_deck);
      g_signal_emit (self, signals[DEVICE_ADDED], 0, stream_deck);
    }
}


/*
 * Callbacks
 */

static void
on_gusb_context_device_added_cb (GUsbContext     *gusb_context,
                                 GUsbDevice      *device,
                                 BsDeviceManager *self)
{
  g_autoptr (BsStreamDeck) stream_deck = NULL;
  g_autoptr (GError) error = NULL;

  BS_ENTRY;

  stream_deck = bs_stream_deck_new (device, &error);

  if (error)
    {
      if (!g_error_matches (error, BS_STREAM_DECK_ERROR, BS_STREAM_DECK_ERROR_UNRECOGNIZED))
        g_warning ("Error opening Stream Deck device: %s", error->message);
      BS_RETURN ();
    }

  g_list_store_append (self->stream_decks, g_object_ref (stream_deck));
  g_signal_emit (self, signals[DEVICE_ADDED], 0, stream_deck);

  BS_EXIT;
}

static void
on_gusb_context_device_removed_cb (GUsbContext     *gusb_context,
                                   GUsbDevice      *device,
                                   BsDeviceManager *self)
{
  unsigned int i = 0;

  BS_ENTRY;

  while (i < g_list_model_get_n_items (G_LIST_MODEL (self->stream_decks)))
    {
      g_autoptr (BsStreamDeck) stream_deck = NULL;
      GUsbDevice *d;

      stream_deck = g_list_model_get_item (G_LIST_MODEL (self->stream_decks), i);
      d = bs_stream_deck_get_device (stream_deck);

      if (d == device)
        {
          g_message ("Removing Stream Deck device %p", stream_deck);
          g_signal_emit (self, signals[DEVICE_REMOVED], 0, stream_deck);
          g_list_store_remove (self->stream_decks, i);
          continue;
        }

      i++;
    }

  BS_EXIT;
}

static void
on_stream_decks_items_changed_cb (GListModel      *model,
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
  return BS_TYPE_STREAM_DECK;
}

static gpointer
bs_device_manager_get_item (GListModel *model,
                            guint       i)
{
  BsDeviceManager *self = BS_DEVICE_MANAGER (model);
  return g_list_model_get_item (G_LIST_MODEL (self->stream_decks), i);
}

static guint
bs_device_manager_get_n_items (GListModel *model)
{
  BsDeviceManager *self = BS_DEVICE_MANAGER (model);
  return g_list_model_get_n_items (G_LIST_MODEL (self->stream_decks));
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

  g_clear_object (&self->stream_decks);
  g_clear_object (&self->gusb_context);

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
                                        BS_TYPE_STREAM_DECK);

  signals[DEVICE_REMOVED] = g_signal_new ("device-removed",
                                          BS_TYPE_DEVICE_MANAGER,
                                          G_SIGNAL_RUN_LAST,
                                          0, NULL, NULL, NULL,
                                          G_TYPE_NONE,
                                          1,
                                          BS_TYPE_STREAM_DECK);
}

static void
bs_device_manager_init (BsDeviceManager *self)
{
  const char *emulate_devices = g_getenv ("BOATSWAIN_EMULATE_DEVICES");

  self->emulate_devices = g_strcmp0 (PROFILE, "development") == 0 &&
                          emulate_devices != NULL &&
                          *emulate_devices == '1';

  self->stream_decks = g_list_store_new (BS_TYPE_STREAM_DECK);
  g_signal_connect (self->stream_decks,
                    "items-changed",
                    G_CALLBACK (on_stream_decks_items_changed_cb),
                    self);
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

  if (!self->emulate_devices)
    {
      self->gusb_context = g_usb_context_new (error);
      if (!self->gusb_context)
        goto out;

      enumerate_stream_decks (self);
      g_signal_connect (self->gusb_context, "device-added", G_CALLBACK (on_gusb_context_device_added_cb), self);
      g_signal_connect (self->gusb_context, "device-removed", G_CALLBACK (on_gusb_context_device_removed_cb), self);
    }
  else
    {
      enumerate_fake_stream_decks (self);
    }

out:
  self->loaded = TRUE;

  return self->gusb_context != NULL;
}

