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
#include <libdex.h>
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

  struct {
    DexFuture *fiber;
    DexCancellable *cancellable;
    uint8_t buffer[4096];
  } bulk_in;
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

static void dex_usb_transfer_cb (struct libusb_transfer *transfer);
static void on_dex_usb_transfer_cancelled_cb (GCancellable           *cancellable,
                                              struct libusb_transfer *transfer);


/*
 * Auxiliary methods
 */

static GError *
usb_transfer_status_to_g_error (enum libusb_transfer_type   type,
                                enum libusb_transfer_status status)
{
  g_autoptr (GError) error = NULL;
  const char *message = NULL;
  int code = G_IO_ERROR_FAILED;

  g_assert (status != LIBUSB_TRANSFER_COMPLETED);

  switch (status)
    {
    case LIBUSB_TRANSFER_ERROR:
      message = "USB transfer has failed";
      break;

    case LIBUSB_TRANSFER_TIMED_OUT:
      message = "USB transfer has timed out";
      code = G_IO_ERROR_TIMED_OUT;
      break;

    case LIBUSB_TRANSFER_CANCELLED:
      message = "USB transfer was cancelled";
      code = G_IO_ERROR_CANCELLED;
      break;

    case LIBUSB_TRANSFER_STALL:
      if (type == LIBUSB_TRANSFER_TYPE_CONTROL)
        {
          message = "USB control request not supported";
          code = G_IO_ERROR_NOT_SUPPORTED;
        }
      else
        message = "USB transfer has stalled";
      break;

    case LIBUSB_TRANSFER_NO_DEVICE:
      message = "USB device was disconnected";
      code = G_IO_ERROR_CONNECTION_CLOSED;
      break;

    case LIBUSB_TRANSFER_OVERFLOW:
      message = "USB device sent more data than requested";
      break;

    case LIBUSB_TRANSFER_COMPLETED:
    default:
      g_assert_not_reached ();
    }

  g_assert (message != NULL);
  g_set_error_literal (&error, G_IO_ERROR, code, message);

  return g_steal_pointer (&error);
}

static DexFuture *
bulk_in_transfer_future (LoupedeckDevice *self)
{
  LoupedeckDevicePrivate *priv =
    loupedeck_device_get_instance_private (self);
  DexPromise *promise;
  struct libusb_transfer *transfer;

  promise = dex_promise_new_cancellable ();
  transfer = libusb_alloc_transfer (0);

  memset (priv->bulk_in.buffer, 0, sizeof (priv->bulk_in.buffer));

  libusb_fill_bulk_transfer (transfer,
                             priv->usb_device_handle,
                             priv->cdc_data.ep_in,
                             priv->bulk_in.buffer,
                             sizeof (priv->bulk_in.buffer),
                             dex_usb_transfer_cb,
                             dex_ref (promise),
                             0);
  transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;

  g_cancellable_connect (dex_promise_get_cancellable (promise),
                         G_CALLBACK (on_dex_usb_transfer_cancelled_cb),
                         transfer,
                         NULL);

  libusb_submit_transfer (transfer);

  return DEX_FUTURE (promise);
}


/*
 * Callbacks
 */

static void
dex_usb_transfer_cb (struct libusb_transfer *transfer)
{
  g_autoptr (DexPromise) promise = transfer->user_data;
  GValue value;

  g_assert (DEX_IS_PROMISE (promise));

  if (transfer->status != LIBUSB_TRANSFER_COMPLETED)
    {
      dex_promise_reject (promise, usb_transfer_status_to_g_error (transfer->type,
                                                                   transfer->status));
      return;
    }

  if ((transfer->flags & LIBUSB_TRANSFER_FREE_TRANSFER) &&
      (transfer->flags & LIBUSB_TRANSFER_FREE_BUFFER))
    {
      uint8_t *real_buffer = transfer->buffer;
      GByteArray *buffer = NULL;

      if (transfer->type == LIBUSB_TRANSFER_TYPE_CONTROL)
        real_buffer += LIBUSB_CONTROL_SETUP_SIZE;

      if (transfer->type != LIBUSB_TRANSFER_TYPE_CONTROL ||
          transfer->actual_length != 0)
        {
          buffer = g_byte_array_new ();
          g_byte_array_append (buffer, real_buffer, transfer->actual_length);

          dex_promise_resolve_boxed (promise, G_TYPE_BYTE_ARRAY, buffer);
          return;
        }
    }

  if (transfer->flags & LIBUSB_TRANSFER_FREE_TRANSFER)
    {
      dex_promise_resolve_boolean (promise, TRUE);
      return;
    }

  g_value_init (&value, G_TYPE_POINTER);
  g_value_set_pointer (&value, transfer);

  dex_promise_resolve (promise, &value);
}

static void
on_dex_usb_transfer_cancelled_cb (GCancellable           *cancellable,
                                  struct libusb_transfer *transfer)
{
  libusb_cancel_transfer (transfer);
}

static DexFuture *
bulk_in_loop_fiber (gpointer data)
{
  LoupedeckDevice *self = LOUPEDECK_DEVICE (data);
  LoupedeckDevicePrivate *priv = loupedeck_device_get_instance_private (self);
  DexFuture *bulk_in_transfer = NULL;

  while (TRUE)
    {
      g_autoptr (GError) error = NULL;

      if (!bulk_in_transfer)
        bulk_in_transfer = bulk_in_transfer_future (self);

      dex_await (dex_future_first (dex_ref (priv->bulk_in.cancellable),
                                   dex_ref (bulk_in_transfer),
                                   NULL),
                 NULL);

      if (dex_future_is_rejected (DEX_FUTURE (priv->bulk_in.cancellable)))
        {
          g_debug ("Bulk in transfer cancelled");
          return dex_ref (priv->bulk_in.cancellable);
        }

      g_assert (dex_future_is_pending (DEX_FUTURE (priv->bulk_in.cancellable)));
      g_assert (!dex_future_is_pending (bulk_in_transfer));

      if (!dex_await (g_steal_pointer (&bulk_in_transfer), &error))
        {
          if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED))
            {
              g_message ("Device was disconnected");
              return dex_future_new_for_error (g_steal_pointer (&error));
            }

          g_warning ("Failed bulk in transfer: %s", error->message);
        }
      else
        {
          // TODO: Handle bulk in data
          g_assert_not_reached ();
        }
    }

  return dex_future_new_true ();
}


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

  g_debug ("Starting bulk in transfer loop");

  priv->bulk_in.cancellable = dex_cancellable_new ();
  priv->bulk_in.fiber = dex_scheduler_spawn (NULL,
                                             0,
                                             bulk_in_loop_fiber,
                                             self,
                                             NULL);

  g_debug ("Bulk in transfer loop started");

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
loupedeck_device_dispose (GObject *object)
{
  LoupedeckDevicePrivate *priv =
    loupedeck_device_get_instance_private (LOUPEDECK_DEVICE (object));

  BS_ENTRY;

  if (priv->bulk_in.fiber)
    {
      GError *error = NULL;

      g_debug ("Cancelling bulk in transfer loop and wait for cancellation");
      dex_cancellable_cancel (priv->bulk_in.cancellable);

      if (!dex_await (priv->bulk_in.fiber, &error) &&
          !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed waiting for bulk in transfer cancellation: %s", error->message);
      g_clear_error (&error);

      dex_clear (&priv->bulk_in.cancellable);

      g_debug ("Bulk in transfer loop cancelled");
    }

  G_OBJECT_CLASS (loupedeck_device_parent_class)->dispose (object);

  BS_EXIT;
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
  object_class->dispose = loupedeck_device_dispose;
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
