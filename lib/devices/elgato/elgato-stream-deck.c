/*
 * elgato-stream-deck.c
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

#define G_LOG_DOMAIN "Elgato Stream Deck"

#include "bs-actionable.h"
#include "bs-button.h"
#include "bs-button-grid.h"
#include "bs-debug.h"
#include "bs-device-layout-builder-private.h"
#include "bs-device-update-private.h"
#include "bs-dial-private.h"
#include "bs-dial-grid.h"
#include "bs-events-private.h"
#include "bs-renderer-private.h"
#include "bs-touchscreen-private.h"
#include "bs-touchscreen-slot-private.h"
#include "elgato-stream-deck.h"

#include <glib/gi18n.h>
#include <glib-unix.h>
#include <hidapi_libusb.h>

#define POLL_RATE_MS 16

#define ELGATO_SYSTEMS_VENDOR_ID (0x0fd9)

G_STATIC_ASSERT (sizeof (unsigned char) == sizeof (uint8_t));

typedef struct
{
  GSource source;
  ElgatoStreamDeck *stream_deck;
} StreamDeckSource;

typedef struct
{
  int usb_device_fd;
  hid_device *handle;

  GSource *poll_source;

  char *serial_number;
  char *firmware_version;
} ElgatoStreamDeckPrivate;

static void g_initable_iface_init (GInitableIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (ElgatoStreamDeck, elgato_stream_deck, BS_TYPE_DEVICE,
                                  G_ADD_PRIVATE (ElgatoStreamDeck)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, g_initable_iface_init))

G_DEFINE_QUARK (ElgatoStreamDeck, elgato_stream_deck_error);

enum {
  PROP_0,
  PROP_USB_DEVICE_FD,
  N_PROPS,
};

static GParamSpec *properties [N_PROPS];


/*
 * GSource
 */

static void
apply_button_update (ElgatoStreamDeck *self,
                     BsButtonUpdate   *button_update)
{
  g_autoptr (GdkTexture) texture = NULL;
  g_autoptr (GError) error = NULL;
  BsDeviceRegion *region;
  BsRenderer *renderer;
  BsButton *button;
  BsIcon *icon;

  BS_TRACE_MSG ("Applying button update %p", button_update);

  g_assert (ELGATO_IS_STREAM_DECK (self));
  g_assert (ELGATO_STREAM_DECK_GET_CLASS (self)->set_button_texture != NULL);

  button = button_update->button;
  icon = bs_button_get_icon (button);
  region = bs_button_get_region (button);
  renderer = bs_device_region_get_renderer (region);
  texture = bs_renderer_compose_icon (renderer, icon, &error);

  if (error)
    {
      g_warning ("Error compositing button texture: %s", error->message);
      return;
    }

  ELGATO_STREAM_DECK_GET_CLASS (self)->set_button_texture (self, button, texture, &error);
  if (error)
    {
      g_warning ("Error uploading button texture: %s", error->message);
      return;
    }
}

static void
apply_touchscreen_update (ElgatoStreamDeck    *self,
                          BsTouchscreenUpdate *touchscreen_update)
{
  g_autoptr (GdkTexture) texture = NULL;
  BsTouchscreenContent *content;
  g_autoptr (GError) error = NULL;
  BsTouchscreen *touchscreen;
  BsRenderer *renderer;

  g_assert (ELGATO_IS_STREAM_DECK (self));
  g_assert (ELGATO_STREAM_DECK_GET_CLASS (self)->set_button_texture != NULL);

  touchscreen = touchscreen_update->touchscreen;
  content = bs_touchscreen_get_content (touchscreen);
  renderer = bs_device_region_get_renderer (BS_DEVICE_REGION (touchscreen));
  texture = bs_renderer_compose_touchscreen_content (renderer, content, &error);

  if (error)
    {
      g_warning ("Error compositing touchscreen texture: %s", error->message);
      return;
    }

  ELGATO_STREAM_DECK_GET_CLASS (self)->set_touchscreen_texture (self, touchscreen, texture, &error);

  if (error)
    {
      g_warning ("Error uploading touchscreen texture: %s", error->message);
      return;
    }
}

static gboolean
stream_deck_source_dispatch (GSource     *source,
                             GSourceFunc  callback,
                             gpointer     user_data)
{
  g_autoptr (BsDeviceUpdate) update = NULL;
  StreamDeckSource *stream_deck_source;
  ElgatoStreamDeck *self;
  gint64 current_time;
  gint64 expiration;

  stream_deck_source = (StreamDeckSource *)source;
  self = stream_deck_source->stream_deck;

  if ((update = bs_device_steal_update (BS_DEVICE (self))))
    {
      BsTouchscreenUpdate **touchscreen_updates;
      BsButtonUpdate **button_updates;

      bs_device_update_seal (update);

      button_updates = bs_device_update_get_button_updates (update);
      for (size_t i = 0; button_updates && button_updates[i]; i++)
        apply_button_update (self, button_updates[i]);

      touchscreen_updates = bs_device_update_get_touchscreen_updates (update);
      for (size_t i = 0; touchscreen_updates && touchscreen_updates[i]; i++)
        apply_touchscreen_update (self, touchscreen_updates[i]);
    }

  ELGATO_STREAM_DECK_GET_CLASS (self)->read_state (self);

  current_time = g_source_get_time (source);
  expiration = current_time + (guint64) POLL_RATE_MS * 1000;
  g_source_set_ready_time (source, expiration);

  return TRUE;
}

GSourceFuncs stream_deck_source_funcs =
{
  NULL, /* prepare */
  NULL, /* check */
  stream_deck_source_dispatch,
  NULL, NULL, NULL,
};

static GSource *
stream_deck_source_new (ElgatoStreamDeck *self)
{
  StreamDeckSource *stream_deck_source;
  GSource *source;

  source = g_source_new (&stream_deck_source_funcs, sizeof (StreamDeckSource));
  stream_deck_source = (StreamDeckSource *)source;
  stream_deck_source->stream_deck = self;

  g_source_set_ready_time (source, g_get_monotonic_time ());

  return source;
}


/*
 * GInitable interface
 */

static GInitableIface *parent_initable_iface = NULL;

static gboolean
elgato_stream_deck_initable_init (GInitable     *initable,
                                  GCancellable  *cancellable,
                                  GError       **error)
{

  ElgatoStreamDeck *self = ELGATO_STREAM_DECK (initable);
  ElgatoStreamDeckPrivate *priv = elgato_stream_deck_get_instance_private (self);

  BS_ENTRY;

  g_assert (priv->usb_device_fd != -1);

  priv->handle = hid_libusb_wrap_sys_device (priv->usb_device_fd, -1);

  if (!priv->handle)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to open Stream Deck device");
      BS_RETURN (FALSE);
    }

  hid_set_nonblocking (priv->handle, TRUE);

  priv->poll_source = stream_deck_source_new (self);

  priv->serial_number = ELGATO_STREAM_DECK_GET_CLASS (self)->get_serial_number (self);
  priv->firmware_version = ELGATO_STREAM_DECK_GET_CLASS (self)->get_firmware_version (self);

  BS_RETURN (parent_initable_iface->init (initable, cancellable, error));
}

static void
g_initable_iface_init (GInitableIface *iface)
{
  parent_initable_iface = g_type_interface_peek_parent (iface);

  iface->init = elgato_stream_deck_initable_init;
}


/*
 * BsDevice overrides
 */

static const char *
elgato_stream_deck_get_firmware_version (BsDevice *device)
{
  ElgatoStreamDeck *self = (ElgatoStreamDeck *) device;
  ElgatoStreamDeckPrivate *priv = elgato_stream_deck_get_instance_private (self);

  g_assert (ELGATO_IS_STREAM_DECK (self));

  return priv->firmware_version;
}

static const char *
elgato_stream_deck_get_serial_number (BsDevice *device)
{
  ElgatoStreamDeck *self = (ElgatoStreamDeck *) device;
  ElgatoStreamDeckPrivate *priv = elgato_stream_deck_get_instance_private (self);

  g_assert (ELGATO_IS_STREAM_DECK (self));

  return priv->serial_number;
}

static void
elgato_stream_deck_load (BsDevice *device)
{
  ElgatoStreamDeck *self = (ElgatoStreamDeck *) device;
  ElgatoStreamDeckPrivate *priv = elgato_stream_deck_get_instance_private (self);

  g_assert (ELGATO_IS_STREAM_DECK (self));

  g_source_attach (priv->poll_source, NULL);

  BS_DEVICE_CLASS (elgato_stream_deck_parent_class)->load (device);
}

static void
elgato_stream_deck_set_brightness (BsDevice *device,
                                   double    brightness)
{
  ElgatoStreamDeck *self = (ElgatoStreamDeck *) device;

  BS_DEVICE_CLASS (elgato_stream_deck_parent_class)->set_brightness (device, brightness);

  ELGATO_STREAM_DECK_GET_CLASS (self)->set_brightness (self, brightness);
}


/*
 * GObject overrides
 */

static void
elgato_stream_deck_finalize (GObject *object)
{
  ElgatoStreamDeck *self = (ElgatoStreamDeck *)object;
  ElgatoStreamDeckPrivate *priv = elgato_stream_deck_get_instance_private (self);

  if (priv->poll_source)
    g_source_destroy (priv->poll_source);

  g_clear_pointer (&priv->poll_source, g_source_unref);
  g_clear_pointer (&priv->handle, hid_close);
  g_clear_fd (&priv->usb_device_fd, NULL);

  G_OBJECT_CLASS (elgato_stream_deck_parent_class)->finalize (object);
}

static void
elgato_stream_deck_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ElgatoStreamDeck *self = ELGATO_STREAM_DECK (object);
  ElgatoStreamDeckPrivate *priv = elgato_stream_deck_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_USB_DEVICE_FD:
      priv->usb_device_fd = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
elgato_stream_deck_class_init (ElgatoStreamDeckClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  BsDeviceClass *device_class = BS_DEVICE_CLASS (klass);

  object_class->finalize = elgato_stream_deck_finalize;
  object_class->set_property = elgato_stream_deck_set_property;

  device_class->get_firmware_version = elgato_stream_deck_get_firmware_version;
  device_class->get_serial_number = elgato_stream_deck_get_serial_number;
  device_class->load = elgato_stream_deck_load;
  device_class->set_brightness = elgato_stream_deck_set_brightness;

  properties[PROP_USB_DEVICE_FD] = g_param_spec_int ("usb-device-fd", NULL, NULL,
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
elgato_stream_deck_init (ElgatoStreamDeck *self)
{
}

hid_device *
elgato_stream_deck_get_hid_device (ElgatoStreamDeck *self)
{
  ElgatoStreamDeckPrivate *priv;

  g_assert (ELGATO_IS_STREAM_DECK (self));

  priv = elgato_stream_deck_get_instance_private (self);
  return priv->handle;
}
