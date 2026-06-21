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

  struct {
    uint8_t iface;
    uint8_t ep_in;
  } cdc_acm;

  struct {
    uint8_t iface;
    uint8_t ep_out;
    uint8_t ep_in;
  } cdc_data;
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
  libusb_device *device = NULL;
  struct libusb_config_descriptor *descriptor;
  const struct libusb_interface_descriptor *cdc_acm_iface;
  const struct libusb_interface_descriptor *cdc_data_iface;
  int ret;

  BS_ENTRY;

  if (priv->usb_device_fd == -1 || priv->usb_device_handle == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           "File descriptor or handle missing");
      BS_RETURN (FALSE);
    }

  device = libusb_get_device (priv->usb_device_handle);
  if (!device)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Failed to get the underlying USB device");
      BS_RETURN (FALSE);
    }

  ret = libusb_get_active_config_descriptor (device, &descriptor);
  if (ret)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to get device config descriptor: %s", libusb_strerror (ret));
      BS_RETURN (FALSE);
    }

  if (descriptor->bNumInterfaces < 2)
    {
      libusb_free_config_descriptor (descriptor);
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Device has less than 2 interfaces");
      BS_RETURN (FALSE);
    }

  for (size_t i = 0; i < descriptor->bNumInterfaces; i++)
    {
      const struct libusb_interface_descriptor *iface =
        &descriptor->interface[i].altsetting[0];

      if (iface->bInterfaceClass == LIBUSB_CLASS_COMM &&
          iface->bInterfaceSubClass == 0x02 && // Abstract Control Model
          iface->bInterfaceProtocol == 0x01) // AT Commands: V.250 etc
        cdc_acm_iface = iface;

      if (iface->bInterfaceClass == LIBUSB_CLASS_DATA)
        cdc_data_iface = iface;
    }

  if (!(cdc_acm_iface && cdc_data_iface))
    {
      libusb_free_config_descriptor (descriptor);
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Failed to find CDC interfaces");
      BS_RETURN (FALSE);
    }

  g_assert (cdc_acm_iface->bNumEndpoints == 1);
  priv->cdc_acm.ep_in = cdc_acm_iface->endpoint[0].bEndpointAddress;
  g_assert ((priv->cdc_acm.ep_in & LIBUSB_ENDPOINT_IN) != FALSE);

  g_assert (cdc_data_iface->bNumEndpoints == 2);
  if (cdc_data_iface->endpoint[0].bEndpointAddress & LIBUSB_ENDPOINT_IN)
    {
      priv->cdc_data.ep_out = cdc_data_iface->endpoint[1].bEndpointAddress;
      priv->cdc_data.ep_in = cdc_data_iface->endpoint[0].bEndpointAddress;
    }
  else
    {
      priv->cdc_data.ep_out = cdc_data_iface->endpoint[0].bEndpointAddress;
      priv->cdc_data.ep_in = cdc_data_iface->endpoint[1].bEndpointAddress;
    }

  g_assert ((priv->cdc_data.ep_out & LIBUSB_ENDPOINT_IN) == FALSE);

  libusb_free_config_descriptor (descriptor);

   g_debug ("Claiming interfaces");

  // NOTE: cdc_acm kernel driver is usually attached
  libusb_set_auto_detach_kernel_driver (priv->usb_device_handle, TRUE);

  ret = libusb_claim_interface (priv->usb_device_handle, priv->cdc_acm.iface);
  if (ret)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to claim CDC ACM interface: %s", libusb_strerror (ret));
      BS_RETURN (FALSE);
    }

  ret = libusb_claim_interface (priv->usb_device_handle, priv->cdc_data.iface);
  if (ret)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to claim CDC Data interface: %s", libusb_strerror (ret));
      BS_RETURN (FALSE);
    }

  g_debug ("Interfaces claimed");

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

  libusb_release_interface (priv->usb_device_handle, priv->cdc_data.iface);
  libusb_release_interface (priv->usb_device_handle, priv->cdc_acm.iface);

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
