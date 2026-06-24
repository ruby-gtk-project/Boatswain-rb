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
#include "bs-events.h"

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

    gboolean expecting_second_payload;
    uint32_t expected_second_payload_size;
  } bulk_in;

  struct {
    DexPromise *switch_promise;
    gboolean switched;
  } protocol;

  struct {
    uint8_t next_id;
    GHashTable *table;
    DexLimiter *limiter;
  } host_transactions;

  DexLimiter *send_payloads_limiter;

  char *serial_number;
  char *firmware_version;
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

#define WS_UPGRADE_REQUEST \
"GET /index.html\r\n" \
"HTTP/1.1\r\n" \
"Connection: Upgrade\r\n" \
"Upgrade: websocket\r\n" \
"Sec-WebSocket-Key: 123abc\r\n" \
"\r\n"

#define MAGIC_NUMBER 0x82

enum {
  BUTTON_STATE_CHANGED = 0,
  GET_SERIAL_NUMBER = 0x03,
  GET_FIRMWARE_VERSION = 0x07,
  SET_BRIGHTNESS = 0x09,
  APPLY_FRAMEBUFFER = 0x0f,
  SETUP_FRAMEBUFFER = 0x10,
  CLEAR = 0x1f,
  MAGIC_NUMBER_0X73 = 0x73,
};

struct SendFramebuffer
{
  LoupedeckDevice *self;
  uint16_t id;
  uint16_t x_pos;
  uint16_t y_pos;
  uint16_t width;
  uint16_t height;
  GByteArray *buffer;
};

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

static DexFuture *
dex_usb_submit_transfer (struct libusb_transfer *transfer)
{
  DexPromise *promise;

  g_assert (transfer->user_data == NULL);
  g_assert (transfer->callback == NULL);

  g_return_val_if_fail (transfer->type == LIBUSB_TRANSFER_TYPE_BULK ||
                        transfer->type == LIBUSB_TRANSFER_TYPE_CONTROL, NULL);

  promise = dex_promise_new_cancellable ();
  g_cancellable_connect (dex_promise_get_cancellable (promise),
                         G_CALLBACK (on_dex_usb_transfer_cancelled_cb),
                         transfer,
                         NULL);

  transfer->user_data = dex_ref (promise);
  transfer->callback = dex_usb_transfer_cb;

  libusb_submit_transfer (transfer);

  return DEX_FUTURE (promise);
}

static DexFuture *
send_break (LoupedeckDevice *self, uint16_t value)
{
  LoupedeckDevicePrivate *priv = loupedeck_device_get_instance_private (self);
  struct libusb_transfer *transfer;

  transfer = libusb_alloc_transfer (0);
  transfer->buffer = g_malloc0 (LIBUSB_CONTROL_SETUP_SIZE);
  libusb_fill_control_setup (transfer->buffer,
                             LIBUSB_ENDPOINT_OUT |
                             LIBUSB_REQUEST_TYPE_CLASS |
                             LIBUSB_RECIPIENT_INTERFACE,
                             0x23, // SEND_BREAK
                             value,
                             priv->cdc_acm.iface,
                             0);
  libusb_fill_control_transfer (transfer,
                                priv->usb_device_handle,
                                transfer->buffer,
                                NULL,
                                NULL,
                                0);
  transfer->flags |= LIBUSB_TRANSFER_FREE_BUFFER;
  transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;

  return dex_usb_submit_transfer (transfer);
}

static void
handle_bulk_in_data (LoupedeckDevice *self)
{
  LoupedeckDevicePrivate *priv = loupedeck_device_get_instance_private (self);
  uint8_t second_payload_size;

  BS_ENTRY;

  if (!priv->protocol.switched)
    {
      static char ws_upgrade_response[] = "HTTP/1.1 101 Switching Protocols";

      if (g_ascii_strncasecmp ((char *)priv->bulk_in.buffer,
                               ws_upgrade_response,
                               strlen (ws_upgrade_response)) == 0)
        {
          char **split_response = NULL;

          g_debug ("Protocol switched; response:");
          split_response = g_strsplit ((char *)priv->bulk_in.buffer, "\r\n", -1);
          for (size_t i = 0; split_response[i] != NULL; i++)
            {
              if (split_response[i][0] != '\0')
                g_debug ("\t%s", split_response[i]);
            }
          g_strfreev (split_response);

          priv->protocol.switched = TRUE;
          dex_promise_resolve_boolean (priv->protocol.switch_promise, TRUE);

          BS_RETURN ();
        }

      BS_GOTO (unknown);
    }

  if (!priv->bulk_in.expecting_second_payload &&
      priv->bulk_in.buffer[0] == MAGIC_NUMBER)
    {
      g_debug ("Device message start received");

      priv->bulk_in.expected_second_payload_size = priv->bulk_in.buffer[1];
      priv->bulk_in.expecting_second_payload = TRUE;

      BS_RETURN ();
    }

  second_payload_size = priv->bulk_in.buffer[0];
  if (priv->bulk_in.expecting_second_payload &&
      priv->bulk_in.expected_second_payload_size == second_payload_size)
    {
      uint8_t host_transaction_id = priv->bulk_in.buffer[2];
      g_autoptr (DexPromise) promise = NULL;

      if (host_transaction_id)
        {
          g_autoptr (GError) error = NULL;

          if (!dex_await (dex_limiter_acquire (priv->host_transactions.limiter), &error))
            {
              if (!g_error_matches (error, DEX_ERROR, DEX_ERROR_SEMAPHORE_CLOSED))
                {
                  g_critical ("Failed to acquire host transactions limiter: %s",
                              error->message);
                  dex_limiter_close (priv->host_transactions.limiter);
                }

              BS_RETURN ();
            }

          g_hash_table_steal_extended (priv->host_transactions.table,
                                       GUINT_TO_POINTER (host_transaction_id),
                                       NULL,
                                       (gpointer *)&promise);

          g_assert (promise != NULL && DEX_IS_PROMISE (promise));

          dex_limiter_release (priv->host_transactions.limiter);
        }

      g_debug ("Device message end received");

      switch (priv->bulk_in.buffer[1])
        {
        case GET_SERIAL_NUMBER:
        case GET_FIRMWARE_VERSION:
          {
            GByteArray *content;

            content = g_byte_array_sized_new (second_payload_size);
            g_byte_array_append (content, &priv->bulk_in.buffer[3], second_payload_size - 3);

            dex_promise_resolve_boxed (promise, G_TYPE_BYTE_ARRAY, g_steal_pointer (&content));
            break;
          }

        case SET_BRIGHTNESS:
        case SETUP_FRAMEBUFFER:
        case CLEAR:
          if (priv->bulk_in.buffer[3] == 1)
            dex_promise_resolve_boolean (promise, TRUE);
          else
            dex_promise_reject (promise,
                                g_error_new (G_IO_ERROR,
                                             G_IO_ERROR_FAILED,
                                             "Transaction returned: %u",
                                             priv->bulk_in.buffer[3]));
          break;

        case APPLY_FRAMEBUFFER:
          if (priv->bulk_in.buffer[3] == 0)
            dex_promise_resolve_boolean (promise, TRUE);
          else
            dex_promise_reject (promise,
                                g_error_new (G_IO_ERROR,
                                             G_IO_ERROR_FAILED,
                                             "Transaction returned: %u",
                                             priv->bulk_in.buffer[3]));
          break;

        case BUTTON_STATE_CHANGED:
          {
            LoupedeckDeviceClass *klass = LOUPEDECK_DEVICE_GET_CLASS (self);
            BsEventType event_type = BS_BUTTON_PRESS;

            g_assert (second_payload_size == 5);

            /* 0x00 is pressed and 0x01 is released */
            if (priv->bulk_in.buffer[4])
              event_type = BS_BUTTON_RELEASE;

            if (klass->button_state_changed)
              {
                klass->button_state_changed (self, priv->bulk_in.buffer[3], event_type);
              }
            else
              {
                g_warning ("button_state_changed not implemented");
                g_debug ("Button 0x%02x %s",
                         priv->bulk_in.buffer[3],
                         event_type == BS_BUTTON_PRESS ? "pressed" : "released");
              }
            break;
          }

        case MAGIC_NUMBER_0X73:
          g_debug ("Received post protocol switch gibberish");
          break;

        default:
          g_warning ("Unknown second payload received");
        }

      priv->bulk_in.expected_second_payload_size = 0;
      priv->bulk_in.expecting_second_payload = FALSE;

      BS_RETURN ();
    }

unknown:
  g_warning ("Received unknown data");

  BS_EXIT;
}

static DexFuture *
loupedeck_device_send_payloads (LoupedeckDevice  *self,
                                uint8_t          *second_payload,
                                uint32_t          second_payload_size,
                                uint8_t          *extra_payload,
                                uint32_t          extra_payload_size)
{
  LoupedeckDevicePrivate *priv = loupedeck_device_get_instance_private (self);
  uint64_t total_size = second_payload_size + extra_payload_size;
  uint8_t transaction_id;
  g_autoptr (DexPromise) promise = NULL;
  struct libusb_transfer *transfer = NULL;
  DexFuture *ret = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (second_payload != NULL && second_payload_size >= 3);
  g_assert ((extra_payload == NULL && extra_payload_size == 0) || extra_payload_size != 0);
  g_assert (total_size <= G_MAXUINT32);

  g_assert (second_payload[2] == 0);

  if (!dex_await (dex_limiter_acquire (priv->host_transactions.limiter), &error))
    {
      return dex_future_new_for_error (g_error_new (G_IO_ERROR,
                                                    G_IO_ERROR_FAILED,
                                                    "Failed to acquire host transactions limiter: %s",
                                                    error->message));
    }

  if (priv->host_transactions.next_id == 0)
    priv->host_transactions.next_id = 1;

  transaction_id = priv->host_transactions.next_id++;
  second_payload[2] = transaction_id;

  /* First payload */

  transfer = libusb_alloc_transfer (0);
  transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
  if (total_size <= 0x7E)
    {
      uint8_t first_payload[6] = { MAGIC_NUMBER, 0x80 + (uint8_t)total_size, };

      g_assert (*((uint32_t *)&first_payload[2]) == 0);
      g_assert (second_payload[0] == total_size);

      libusb_fill_bulk_transfer (transfer,
                                 priv->usb_device_handle,
                                 priv->cdc_data.ep_out,
                                 first_payload,
                                 sizeof (first_payload),
                                 NULL,
                                 NULL,
                                 0);
      if (!dex_await (dex_future_first (dex_usb_submit_transfer (transfer),
                                        dex_timeout_new_msec (100),
                                        NULL), &error))
        {
          ret = dex_future_new_for_error (g_error_new (G_IO_ERROR,
                                                       G_IO_ERROR_FAILED,
                                                       "Failed to send first payload: %s",
                                                       error->message));
          goto exit;
        }
    }
  else
    {
      uint8_t first_payload[14] = { MAGIC_NUMBER, 0xFF, };

      *((uint32_t *)&first_payload[6]) = GUINT32_TO_BE (total_size);

      g_assert (*((uint32_t *)&first_payload[2]) == 0);
      g_assert (*((uint32_t *)&first_payload[10]) == 0);
      g_assert (second_payload[0] == 0xFF);

      libusb_fill_bulk_transfer (transfer,
                                 priv->usb_device_handle,
                                 priv->cdc_data.ep_out,
                                 first_payload,
                                 sizeof (first_payload),
                                 NULL,
                                 NULL,
                                 0);
      if (!dex_await (dex_future_first (dex_usb_submit_transfer (transfer),
                                        dex_timeout_new_msec (100),
                                        NULL), &error))
        {
          ret = dex_future_new_for_error (g_error_new (G_IO_ERROR,
                                                       G_IO_ERROR_FAILED,
                                                       "Failed to send first payload: %s",
                                                       error->message));
          dex_limiter_close (priv->send_payloads_limiter);
          goto exit;
        }
    }

  /* Second payload */

  transfer = libusb_alloc_transfer (0);
  transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
  libusb_fill_bulk_transfer (transfer,
                             priv->usb_device_handle,
                             priv->cdc_data.ep_out,
                             second_payload,
                             second_payload_size,
                             NULL,
                             NULL,
                             0);
  if (!dex_await (dex_future_first (dex_usb_submit_transfer (transfer),
                                    dex_timeout_new_msec (100),
                                    NULL), &error))
    {
      ret = dex_future_new_for_error (g_error_new (G_IO_ERROR,
                                                   G_IO_ERROR_FAILED,
                                                   "Failed to send second payload: %s",
                                                   error->message));
      dex_limiter_close (priv->send_payloads_limiter);
      goto exit;
    }

  if (extra_payload)
    {

      /* Extra payload */

      transfer = libusb_alloc_transfer (0);
      transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
      libusb_fill_bulk_transfer (transfer,
                                 priv->usb_device_handle,
                                 priv->cdc_data.ep_out,
                                 extra_payload,
                                 extra_payload_size,
                                 NULL,
                                 NULL,
                                 0);
      if (!dex_await (dex_future_first (dex_usb_submit_transfer (transfer),
                                            dex_timeout_new_msec (500),
                                            NULL), &error))
        {
          ret = dex_future_new_for_error (g_error_new (G_IO_ERROR,
                                                       G_IO_ERROR_FAILED,
                                                       "Failed to send extra payload: %s",
                                                       error->message));
          dex_limiter_close (priv->send_payloads_limiter);
          goto exit;
        }
    }

  ret = DEX_FUTURE (dex_promise_new ());
  g_hash_table_insert (priv->host_transactions.table,
                       GUINT_TO_POINTER (transaction_id),
                       dex_ref (ret));

exit:
  g_assert (ret != NULL);

  dex_limiter_release (priv->host_transactions.limiter);

  return ret;
}

static DexFuture *
loupedeck_device_set_brightness_internal (LoupedeckDevice *self,
                                          uint8_t          brightness)
{
  uint8_t payload[4] = { 4, SET_BRIGHTNESS, 0, brightness };

  return loupedeck_device_send_payloads (self, payload, sizeof (payload), NULL, 0);
}

static struct SendFramebuffer *
send_framebuffer_new (LoupedeckDevice *self,
                      uint16_t         id,
                      uint16_t         x_pos,
                      uint16_t         y_pos,
                      uint16_t         width,
                      uint16_t         height,
                      GByteArray      *buffer)
{
  struct SendFramebuffer *send_fb = g_new0 (struct SendFramebuffer, 1);

  g_assert (LOUPEDECK_IS_DEVICE (self));
  g_assert (buffer != NULL);

  send_fb->self = self;
  send_fb->id = id;
  send_fb->x_pos = x_pos;
  send_fb->y_pos = y_pos;
  send_fb->width = width;
  send_fb->height = height;
  send_fb->buffer = g_byte_array_ref (buffer);

  return send_fb;
}

static void
send_framebuffer_free (struct SendFramebuffer *send_fb)
{
  g_return_if_fail (send_fb != NULL);

  g_clear_pointer (&send_fb->buffer, g_byte_array_unref);

  g_free (send_fb);
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
          handle_bulk_in_data (self);
        }
    }

  return dex_future_new_true ();
}

static DexFuture *
loupedeck_device_set_brightness_fiber (gpointer data)
{
  LoupedeckDevice *self = LOUPEDECK_DEVICE (data);
  uint8_t brightness =
    CLAMP (bs_device_get_brightness (BS_DEVICE (self)) * 10, 0, 10);
  g_autoptr (GError) error = NULL;

  if (!dex_await (dex_future_first (loupedeck_device_set_brightness_internal (self,
                                                                              brightness),
                                    dex_timeout_new_seconds (1),
                                    NULL),
                  &error))
    {
      g_warning ("Failed to send set brighness payload: %s", error->message);
    }

  return NULL;
}

static DexFuture *
loupedeck_device_send_framebuffer_fiber (gpointer data)
{
  struct SendFramebuffer *send_fb = data;
  uint8_t setup_payload[13] = { 0xFF, SETUP_FRAMEBUFFER, };
  uint8_t apply_payload[5] = { 5, APPLY_FRAMEBUFFER, };
  DexFuture *setup_future = NULL;
  DexFuture *apply_future = NULL;
  g_autoptr (GError) error = NULL;

  BS_ENTRY;

  g_assert (send_fb != NULL);
  g_assert (LOUPEDECK_IS_DEVICE (send_fb->self));
  g_assert (send_fb->buffer != NULL);
  g_assert (send_fb->buffer->len <= G_MAXINT32);

  *(uint16_t *)&setup_payload[3] = GUINT16_TO_BE (send_fb->id);
  *(uint16_t *)&setup_payload[5] = GUINT16_TO_BE (send_fb->x_pos);
  *(uint16_t *)&setup_payload[7] = GUINT16_TO_BE (send_fb->y_pos);
  *(uint16_t *)&setup_payload[9] = GUINT16_TO_BE (send_fb->width);
  *(uint16_t *)&setup_payload[11] = GUINT16_TO_BE (send_fb->height);
  setup_future = loupedeck_device_send_payloads (send_fb->self,
                                                 setup_payload,
                                                 sizeof (setup_payload),
                                                 send_fb->buffer->data,
                                                 send_fb->buffer->len);

  *(uint16_t *)&apply_payload[3] = GUINT16_TO_BE (send_fb->id);
  apply_future = loupedeck_device_send_payloads (send_fb->self,
                                                 apply_payload,
                                                 sizeof (apply_payload),
                                                 NULL,
                                                 0);

  if (!dex_await (dex_future_first (dex_future_all (g_steal_pointer (&setup_future),
                                                    g_steal_pointer (&apply_future),
                                                    NULL),
                                    dex_timeout_new_msec (2500),
                                    NULL),
                  &error))
    {
      g_warning ("Failed to send framebuffer: %s", error->message);
    }

  BS_RETURN (NULL);
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
  static unsigned char ws_upgrade_request[] = WS_UPGRADE_REQUEST;
  libusb_device *device = NULL;
  struct libusb_config_descriptor *descriptor;
  const struct libusb_interface_descriptor *cdc_acm_iface;
  const struct libusb_interface_descriptor *cdc_data_iface;
  struct libusb_transfer *transfer = NULL;
  uint8_t line_coding[LIBUSB_CONTROL_SETUP_SIZE + 7] = {};
  uint8_t line_coding_set[LIBUSB_CONTROL_SETUP_SIZE + 7] = {};
  uint8_t get_serial_number_payload[3] = { 3, GET_SERIAL_NUMBER, };
  uint8_t get_firmware_version_payload[3] = { 3, GET_FIRMWARE_VERSION, };
  DexFuture *future;
  const GValue *value = NULL;
  GByteArray *content = NULL;
  int ret;

  BS_ENTRY;

  g_assert (sizeof (line_coding) == sizeof(line_coding_set));

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

  g_debug ("Set control line state");
  transfer = libusb_alloc_transfer (0);
  transfer->buffer = g_malloc0 (LIBUSB_CONTROL_SETUP_SIZE);
  libusb_fill_control_setup (transfer->buffer,
                             LIBUSB_ENDPOINT_OUT |
                             LIBUSB_REQUEST_TYPE_CLASS |
                             LIBUSB_RECIPIENT_INTERFACE,
                             0x22, // SET_CONTROL_LINE_STATE
                             0, // deactivate carrier and DTE not present
                             priv->cdc_acm.iface,
                             0);
  libusb_fill_control_transfer (transfer,
                                priv->usb_device_handle,
                                transfer->buffer,
                                NULL,
                                NULL,
                                0);
  transfer->flags |= LIBUSB_TRANSFER_FREE_BUFFER;
  transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
  if (!dex_await (dex_usb_submit_transfer (g_steal_pointer (&transfer)), error))
    BS_RETURN (FALSE);

  g_debug ("Set line coding");

  *((uint32_t *)&line_coding[LIBUSB_CONTROL_SETUP_SIZE + 0]) = GUINT32_TO_LE (9600); // dwDTERate
  line_coding[LIBUSB_CONTROL_SETUP_SIZE + 4] = 2; // bCharFormat
  line_coding[LIBUSB_CONTROL_SETUP_SIZE + 5] = 0; // bParityType
  line_coding[LIBUSB_CONTROL_SETUP_SIZE + 6] = 8; // bDataBits

  transfer = libusb_alloc_transfer (0);
  libusb_fill_control_setup (line_coding,
                             LIBUSB_ENDPOINT_OUT |
                             LIBUSB_REQUEST_TYPE_CLASS |
                             LIBUSB_RECIPIENT_INTERFACE,
                             0x20, // SET_LINE_CODING
                             0,
                             priv->cdc_acm.iface,
                             sizeof (line_coding) - LIBUSB_CONTROL_SETUP_SIZE);
  libusb_fill_control_transfer (transfer,
                                priv->usb_device_handle,
                                line_coding,
                                NULL,
                                NULL,
                                0);
  transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
  if (!dex_await (dex_usb_submit_transfer (g_steal_pointer (&transfer)), error))
    BS_RETURN (FALSE);

  g_debug ("Get line coding for validation");
  transfer = libusb_alloc_transfer (0);
  libusb_fill_control_setup (line_coding_set,
                             LIBUSB_ENDPOINT_IN |
                             LIBUSB_REQUEST_TYPE_CLASS |
                             LIBUSB_RECIPIENT_INTERFACE,
                             0x21, // GET_LINE_CODING
                             0,
                             priv->cdc_acm.iface,
                             sizeof (line_coding_set) - LIBUSB_CONTROL_SETUP_SIZE);
  libusb_fill_control_transfer (transfer,
                                priv->usb_device_handle,
                                line_coding_set,
                                NULL,
                                NULL,
                                0);
  transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
  if (!dex_await (dex_usb_submit_transfer (g_steal_pointer (&transfer)), error))
    BS_RETURN (FALSE);

  if (memcmp (&line_coding_set[LIBUSB_CONTROL_SETUP_SIZE + 0],
              &line_coding[LIBUSB_CONTROL_SETUP_SIZE + 0],
              sizeof (line_coding_set) - LIBUSB_CONTROL_SETUP_SIZE))
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Failed to set line coding");
      BS_RETURN (FALSE);
    }

  g_debug ("Send breaks");
  if (!dex_await (send_break (self, 0xFFFF), error))
    BS_RETURN (FALSE);

  if (!dex_await (send_break (self, 0x0000), error))
    BS_RETURN (FALSE);

  if (!dex_await (send_break (self, 0xFFFF), error))
    BS_RETURN (FALSE);

  if (!dex_await (send_break (self, 0x0000), error))
    BS_RETURN (FALSE);

  g_debug ("Switching protocol");

  priv->protocol.switch_promise = dex_ref (dex_promise_new ());

  transfer = libusb_alloc_transfer (0);
  libusb_fill_bulk_transfer (transfer,
                             priv->usb_device_handle,
                             priv->cdc_data.ep_out,
                             ws_upgrade_request,
                             // NOTE: This needs to be sent without NULL-termination
                             strlen ((char *)ws_upgrade_request),
                             NULL,
                             NULL,
                             0);
  transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
  if (!dex_await (dex_future_first (dex_usb_submit_transfer (g_steal_pointer(&transfer)),
                                    dex_timeout_new_seconds (1),
                                    NULL),
                  error))
    BS_RETURN (FALSE);

  if (!dex_await (dex_future_first (DEX_FUTURE (priv->protocol.switch_promise),
                                    dex_timeout_new_seconds (1),
                                    NULL),
                  error))
    BS_RETURN (FALSE);

  /* Let the device send post-switch gibberish */
  dex_await (dex_timeout_new_usec (2000), NULL);

  g_debug ("Protocol switched");

  g_debug ("Get device serial number");
  future = loupedeck_device_send_payloads (self,
                                           get_serial_number_payload,
                                           sizeof (get_serial_number_payload),
                                           NULL,
                                           0);


  if (!dex_await (dex_future_first (dex_ref (future),
                                    dex_timeout_new_seconds (1),
                                    NULL), error))
    BS_RETURN (FALSE);

  value = dex_future_get_value (future, error);
  if (!value)
    BS_RETURN (FALSE);

  content = g_value_get_boxed (g_steal_pointer (&value));

  /* NULL-terminate the serial number */
  g_byte_array_append (content, (uint8_t *)"", 1);
  priv->serial_number = (char *)g_byte_array_free (g_steal_pointer (&content), FALSE);
  /* Remove trailing whitespaces if present */
  priv->serial_number = g_strchomp (priv->serial_number);

  g_debug ("Get firmware version");
  future = loupedeck_device_send_payloads (self,
                                           get_firmware_version_payload,
                                           sizeof (get_firmware_version_payload),
                                           NULL,
                                           0);

  if (!dex_await (dex_future_first (dex_ref (future),
                                    dex_timeout_new_seconds (1),
                                    NULL),
                  error))
    BS_RETURN (FALSE);

  value = dex_future_get_value (future, error);
  if (!value)
    BS_RETURN (FALSE);

  content = g_value_get_boxed (g_steal_pointer (&value));
  priv->firmware_version =
    g_strdup_printf ("%u.%u.%u", content->data[0], content->data[1], content->data[2]);

  BS_RETURN (parent_initable_iface->init (initable, cancellable, error));
}

static void
g_initable_iface_init (GInitableIface *iface)
{
  parent_initable_iface = g_type_interface_peek_parent (iface);

  iface->init = loupedeck_device_initable_init;
}


/*
 * BsDevice overrides
 */

static const char *
loupedeck_device_get_serial_number (BsDevice *device)
{
  LoupedeckDevicePrivate *priv;

  g_return_val_if_fail (LOUPEDECK_IS_DEVICE (device), NULL);

  priv = loupedeck_device_get_instance_private (LOUPEDECK_DEVICE (device));

  return priv->serial_number;
}

static const char *
loupedeck_device_get_firmware_version (BsDevice *device)
{
  LoupedeckDevicePrivate *priv;

  g_return_val_if_fail (LOUPEDECK_IS_DEVICE (device), NULL);

  priv = loupedeck_device_get_instance_private (LOUPEDECK_DEVICE (device));

  return priv->firmware_version;
}

static void
loupedeck_device_set_brightness (BsDevice *device,
                                 double    brightness)
{
  LoupedeckDevicePrivate *priv;

  g_return_if_fail (LOUPEDECK_IS_DEVICE (device));

  priv = loupedeck_device_get_instance_private (LOUPEDECK_DEVICE (device));

  BS_DEVICE_CLASS (loupedeck_device_parent_class)->set_brightness (device,
                                                                   brightness);

  dex_future_disown (dex_limiter_run (priv->send_payloads_limiter,
                                      NULL,
                                      0,
                                      loupedeck_device_set_brightness_fiber,
                                      device,
                                      NULL));
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
  LoupedeckDevice *self = LOUPEDECK_DEVICE (object);
  LoupedeckDevicePrivate *priv = loupedeck_device_get_instance_private (self);
  GError *error = NULL;

  BS_ENTRY;

  if (bs_device_is_initialized (BS_DEVICE (self)))
    {
      uint8_t payload[3] = {3, CLEAR, };

      g_debug ("Send clear payload");
      if (!dex_await (dex_future_first (loupedeck_device_send_payloads (self,
                                                                        payload,
                                                                        sizeof (payload),
                                                                        NULL,
                                                                        0),
                                        dex_timeout_new_msec (100),
                                        NULL),
                      &error))
        g_warning ("Failed to clear: %s", error->message);
      g_clear_error (&error);

      g_debug ("Set brightness to 0");
      if (!dex_await (dex_future_first (loupedeck_device_set_brightness_internal (self, 0),
                                        dex_timeout_new_msec (100),
                                        NULL),
                      &error))
        g_warning ("Failed to set brightness: %s", error->message);
      g_clear_error (&error);
    }

  dex_limiter_close (priv->host_transactions.limiter);
  dex_limiter_close (priv->send_payloads_limiter);

  if (priv->bulk_in.fiber)
    {
      GHashTableIter iter;
      gpointer value;

      g_debug ("Cancelling bulk in transfer loop and wait for cancellation");
      dex_cancellable_cancel (priv->bulk_in.cancellable);

      if (!dex_await (priv->bulk_in.fiber, &error) &&
          !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed waiting for bulk in transfer cancellation: %s", error->message);
      g_clear_error (&error);

      dex_clear (&priv->bulk_in.cancellable);

      g_debug ("Bulk in transfer loop cancelled");

      g_debug ("Cleanup remaining transactions if any");
      g_hash_table_iter_init (&iter, priv->host_transactions.table);
      while (g_hash_table_iter_next (&iter, NULL, &value))
        {
          g_autoptr (DexPromise) promise = value;

          if (promise && dex_future_is_pending (DEX_FUTURE (promise)))
            dex_promise_reject (promise, g_error_new_literal (G_IO_ERROR,
                                                              G_IO_ERROR_FAILED,
                                                              "Device is being disposed"));

          g_hash_table_iter_remove (&iter);
        }
    }

  g_debug ("Send breaks");
  if (!dex_await (dex_future_first (send_break (self, 0xFFFF),
                                    dex_timeout_new_msec (100),
                                    NULL), NULL))
    BS_GOTO (exit);

  if (!dex_await (dex_future_first (send_break (self, 0x0000),
                                    dex_timeout_new_msec (100),
                                    NULL), NULL))
    BS_GOTO (exit);

  if (!dex_await (dex_future_first (send_break (self, 0xFFFF),
                                    dex_timeout_new_msec (100),
                                    NULL), NULL))
    BS_GOTO (exit);

  if (!dex_await (dex_future_first (send_break (self, 0x0000),
                                    dex_timeout_new_msec (100),
                                    NULL), NULL))
    BS_GOTO (exit);

exit:
  G_OBJECT_CLASS (loupedeck_device_parent_class)->dispose (object);

  BS_EXIT;
}

static void
loupedeck_device_finalize (GObject *object)
{
  LoupedeckDevicePrivate *priv =
    loupedeck_device_get_instance_private (LOUPEDECK_DEVICE (object));

  BS_ENTRY;

  g_clear_pointer (&priv->serial_number, g_free);
  g_clear_pointer (&priv->firmware_version, g_free);

  g_clear_pointer (&priv->host_transactions.table, g_hash_table_unref);

  dex_clear (&priv->host_transactions.limiter);
  dex_clear (&priv->send_payloads_limiter);

  dex_clear (&priv->protocol.switch_promise);

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
  BsDeviceClass *device_class = BS_DEVICE_CLASS (klass);

  object_class->set_property = loupedeck_device_set_property;
  object_class->dispose = loupedeck_device_dispose;
  object_class->finalize = loupedeck_device_finalize;

  device_class->get_serial_number = loupedeck_device_get_serial_number;
  device_class->get_firmware_version = loupedeck_device_get_firmware_version;
  device_class->set_brightness = loupedeck_device_set_brightness;

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
  LoupedeckDevicePrivate *priv = loupedeck_device_get_instance_private (self);

  priv->send_payloads_limiter = dex_limiter_new (1);
  priv->host_transactions.limiter = dex_limiter_new (1);

  priv->host_transactions.table = g_hash_table_new_full (g_direct_hash,
                                                         g_direct_equal,
                                                         NULL,
                                                         NULL);
}

void
loupedeck_device_send_framebuffer (LoupedeckDevice *self,
                                   uint16_t         id,
                                   uint16_t         x_pos,
                                   uint16_t         y_pos,
                                   uint16_t         width,
                                   uint16_t         height,
                                   GByteArray      *buffer)
{
  LoupedeckDevicePrivate *priv;
  struct SendFramebuffer *send_fb;

  g_return_if_fail (LOUPEDECK_IS_DEVICE (self));
  g_return_if_fail (buffer != NULL);
  g_return_if_fail (buffer->len <= G_MAXINT32);

  priv = loupedeck_device_get_instance_private (self);
  send_fb = send_framebuffer_new (self, id, x_pos, y_pos, width, height, buffer);

  dex_future_disown (dex_limiter_run (priv->send_payloads_limiter,
                                      NULL,
                                      0,
                                      loupedeck_device_send_framebuffer_fiber,
                                      send_fb,
                                      (GDestroyNotify)send_framebuffer_free));
}
