/*
 * loupedeck-device.c
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

#define G_LOG_DOMAIN "Loupedeck Device"

#include "loupedeck-device.h"

#include <gio/gio.h>
#include <glib-object.h>
#include <glib-unix.h>
#include <libusb.h>

#include "bs-debug.h"

typedef struct
{
  int usb_device_fd;
  libusb_device_handle *usb_device_handle;
} LoupedeckDevicePrivate;

static void g_initable_iface_init (GInitableIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (LoupedeckDevice, loupedeck_device, BS_TYPE_DEVICE,
                                  G_ADD_PRIVATE (LoupedeckDevice)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, g_initable_iface_init))

enum {
  PROP_0,
  PROP_USB_DEVICE_FD,
  PROP_USB_DEVICE_HANDLE,
  N_PROPS,
};

static GParamSpec *properties [N_PROPS];


/*
 * GInitable interface
 */

static GInitableIface *parent_initable_iface = NULL;

static gboolean
loupedeck_device_initable_init (GInitable     *initable,
                               GCancellable  *cancellable,
                               GError       **error)
{
  LoupedeckDevice *self = LOUPEDECK_DEVICE (initable);
  LoupedeckDevicePrivate *priv = loupedeck_device_get_instance_private (self);

  BS_ENTRY;

  if (priv->usb_device_fd == -1 || priv->usb_device_handle == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           "File descriptor or handle missing");
      BS_RETURN (FALSE);
    }

  // TODO: Implement abstract class
  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                           "Not implemented");
  BS_RETURN (FALSE);
}

static void
g_initable_iface_init (GInitableIface *iface)
{
  parent_initable_iface = g_type_interface_peek_parent (iface);

  iface->init = loupedeck_device_initable_init;
}


/*
 * GObject overrides
 */

static void
loupedeck_device_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  LoupedeckDevicePrivate *priv =
    loupedeck_device_get_instance_private (LOUPEDECK_DEVICE (object));

  switch (prop_id)
    {
    case PROP_USB_DEVICE_FD:
      priv->usb_device_fd = g_value_get_int (value);
      break;

    case PROP_USB_DEVICE_HANDLE:
      priv->usb_device_handle = g_value_get_pointer (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
loupedeck_device_finalize (GObject *object)
{
  LoupedeckDevicePrivate *priv =
    loupedeck_device_get_instance_private (LOUPEDECK_DEVICE (object));

  BS_ENTRY;

  g_clear_pointer (&priv->usb_device_handle, libusb_close);
  g_clear_fd (&priv->usb_device_fd, NULL);

  G_OBJECT_CLASS (loupedeck_device_parent_class)->finalize (object);

  BS_EXIT;
}

static void
loupedeck_device_class_init (LoupedeckDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = loupedeck_device_set_property;
  object_class->finalize = loupedeck_device_finalize;

  properties[PROP_USB_DEVICE_FD] = g_param_spec_int ("usb-device-fd", NULL, NULL,
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_USB_DEVICE_HANDLE] = g_param_spec_pointer ("usb-device-handle", NULL, NULL,
                                                             G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
loupedeck_device_init (LoupedeckDevice *self)
{
}
