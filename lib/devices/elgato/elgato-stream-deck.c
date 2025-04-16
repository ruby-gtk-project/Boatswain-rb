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
#include "bs-renderer.h"
#include "bs-touchscreen-private.h"
#include "bs-touchscreen-slot-private.h"
#include "elgato-stream-deck.h"

#include <glib/gi18n.h>
#include <hidapi.h>

#define POLL_RATE_MS 16

#define ELGATO_SYSTEMS_VENDOR_ID (0x0fd9)

#define STREAMDECK_ORIGINAL_PRODUCT_ID 0x0060
#define STREAMDECK_ORIGINAL_V2_PRODUCT_ID  0x006d
#define STREAMDECK_MINI_PRODUCT_ID  0x0063
#define STREAMDECK_MINI_V2_PRODUCT_ID  0x0090
#define STREAMDECK_XL_PRODUCT_ID  0x006c
#define STREAMDECK_XL_V2_PRODUCT_ID  0x008f
#define STREAMDECK_MK2_PRODUCT_ID  0x0080
#define STREAMDECK_PEDAL_PRODUCT_ID  0x0086
#define STREAMDECK_PLUS_PRODUCT_ID  0x0084
#define STREAMDECK_NEO_PRODUCT_ID  0x009a

G_STATIC_ASSERT (sizeof (unsigned char) == sizeof (uint8_t));

typedef enum
{
  ELGATO_STREAM_DECK_FEATURE_BUTTONS = 1 << 0,
  ELGATO_STREAM_DECK_FEATURE_TOUCHSCREEN = 1 << 1,
  ELGATO_STREAM_DECK_FEATURE_DIALS = 1 << 2,
} ElgatoStreamDeckFeatureFlags;

typedef struct
{
  uint8_t n_buttons;
  uint8_t columns;
  BsImageInfo image_info;
} BsButtonLayout;

typedef struct
{
  uint8_t n_dials;
  uint8_t columns;
} BsDialLayout;

typedef struct
{
  uint32_t n_slots;
  BsImageInfo image_info;
} BsTouchscreenLayout;

typedef struct
{
  GSource source;
  ElgatoStreamDeck *stream_deck;
} StreamDeckSource;

typedef struct
{
  uint8_t product_id;
  const char *name;
  ElgatoStreamDeckFeatureFlags features;
  BsButtonLayout button_layout;
  BsDialLayout dial_layout;
  BsTouchscreenLayout touchscreen_layout;

  void (*reset) (ElgatoStreamDeck *self);
  void (*set_brightness) (ElgatoStreamDeck *self,
                          double            brightness);

  char * (*get_serial_number) (ElgatoStreamDeck *self);
  char * (*get_firmware_version) (ElgatoStreamDeck *self);
  gboolean (*set_button_texture) (ElgatoStreamDeck  *self,
                                  BsButton          *button,
                                  GdkTexture        *texture,
                                  GError           **error);
  gboolean (*set_touchscreen_texture) (ElgatoStreamDeck  *self,
                                       BsTouchscreen     *touchscreen,
                                       GdkTexture        *texture,
                                       GError           **error);
  gboolean (*read_state) (ElgatoStreamDeck *self);
} DeviceModelInfo;

typedef struct
{
  GUsbDevice *gusb_device;
  const DeviceModelInfo *model_info;
  hid_device *handle;

  GSource *poll_source;

  char *serial_number;
  char *firmware_version;
} ElgatoStreamDeckPrivate;

static void g_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (ElgatoStreamDeck, elgato_stream_deck, BS_TYPE_DEVICE,
                         G_ADD_PRIVATE (ElgatoStreamDeck)
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, g_initable_iface_init))

G_DEFINE_QUARK (ElgatoStreamDeck, elgato_stream_deck_error);

enum {
  PROP_0,
  PROP_GUSB_DEVICE,
  N_PROPS,
};

static GParamSpec *properties [N_PROPS];


/*
 * Auxiliary methods
 */

static BsButton *
find_button_at_region (ElgatoStreamDeck *self,
                       const char       *region_id,
                       size_t            button_index)
{
  g_autoptr (BsButton) button = NULL;
  BsButtonGrid *button_grid = NULL;

  button_grid = BS_BUTTON_GRID (bs_device_get_region (BS_DEVICE (self), region_id));
  button = g_list_model_get_item (bs_button_grid_get_buttons (button_grid), button_index);
  g_assert (BS_IS_BUTTON (button));

  return button;
}



/*
 * Device-specific implementations
 */

/* Mini & Original (gen 1) */

static gboolean
set_button_texture_mini (ElgatoStreamDeck  *self,
                         BsButton          *button,
                         GdkTexture        *texture,
                         GError           **error)
{
  ElgatoStreamDeckPrivate *priv;
  g_autofree uint8_t *payload = NULL;
  g_autofree uint8_t *buffer = NULL;
  BsDeviceRegion *region;
  BsRenderer *renderer;
  const size_t package_size = 1024;
  const size_t header_size = 16;
  uint8_t page;
  size_t bytes_remaining;
  size_t buffer_size;

  BS_ENTRY;

  priv = elgato_stream_deck_get_instance_private (self);
  region = bs_button_get_region (button);
  renderer = bs_device_region_get_renderer (region);

  if (!bs_renderer_convert_texture (renderer, texture, (char **) &buffer, &buffer_size, error))
    BS_RETURN (FALSE);

  payload = g_malloc (sizeof (uint8_t) * package_size);
  payload[0] = 0x02;
  payload[1] = 0x01;
  /* payload[2] set in loop */
  payload[3] = 0;
  /* payload[4] set in loop */
  payload[5] = bs_button_get_position (button) + 1;
  payload[6] = 0;
  payload[7] = 0;
  payload[8] = 0;
  payload[9] = 0;
  payload[10] = 0;
  payload[11] = 0;
  payload[12] = 0;
  payload[13] = 0;
  payload[14] = 0;
  payload[15] = 0;

  page = 0;
  bytes_remaining = buffer_size;
  while (bytes_remaining > 0)
    {
      size_t padding_size;
      size_t chunk_size;
      size_t bytes_sent;

      chunk_size = MIN (bytes_remaining, package_size - header_size);

      payload[2] = page;
      payload[4] = chunk_size == bytes_remaining ? 1 : 0;

      bytes_sent = page * (package_size - header_size);
      memcpy (payload + header_size, buffer + bytes_sent, chunk_size);

      padding_size = package_size - header_size - chunk_size;
      if (padding_size > 0)
        memset (payload + header_size + chunk_size, 0, padding_size);

      hid_write (priv->handle, payload, package_size);

      bytes_remaining -= chunk_size;
      page++;
    }

  BS_RETURN (TRUE);
}

static gboolean
read_state_mini (ElgatoStreamDeck *self)
{
  ElgatoStreamDeckPrivate *priv;
  const BsButtonLayout *layout;
  uint8_t *states;
  size_t states_length;
  int result;

  priv = elgato_stream_deck_get_instance_private (self);

  layout = &priv->model_info->button_layout;
  states_length = layout->n_buttons + 1;

  g_assert (states_length < 8192);
  states = g_alloca (sizeof (uint8_t) * states_length);

  priv = elgato_stream_deck_get_instance_private (self);
  result = hid_read (priv->handle, states, states_length);

  if (result == 0)
    return TRUE;

  for (uint8_t i = 0; i < layout->n_buttons; i++)
    {
      g_autoptr (BsEvent) button_event = NULL;
      BsEventType event_type;
      BsButton *button;

      button = find_button_at_region (self, "main-button-grid", i);

      if (states[i + 1] == bs_button_get_pressed (button))
        continue;

      event_type = states[i + 1] ? BS_BUTTON_PRESS : BS_BUTTON_RELEASE;
      button_event = bs_button_event_new (event_type, BS_DEVICE (self), button);
      bs_actionable_handle_event (BS_ACTIONABLE (button), button_event);
    }

  return TRUE;
}

static void
reset_mini_original (ElgatoStreamDeck *self)
{
  ElgatoStreamDeckPrivate *priv;

  const uint8_t reset_command[] = {
    0x0b,
    0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  BS_ENTRY;

  priv = elgato_stream_deck_get_instance_private (self);
  hid_send_feature_report (priv->handle, reset_command, sizeof (reset_command));

  BS_EXIT;
}

static char *
get_serial_number_mini_original (ElgatoStreamDeck *self)
{
  ElgatoStreamDeckPrivate *priv;
  g_autofree char *serial = NULL;
  uint8_t data[17];

  BS_ENTRY;

  data[0] = 0x03;

  priv = elgato_stream_deck_get_instance_private (self);
  hid_get_feature_report (priv->handle, data, sizeof (data));

  serial = g_malloc0 (sizeof (char) * 13);
  memcpy (serial, &data[5], 12);

  BS_RETURN (g_steal_pointer (&serial));
}

static char *
get_firmware_version_mini_original (ElgatoStreamDeck *self)
{
  g_autofree char *firmware_version = NULL;
  ElgatoStreamDeckPrivate *priv;
  uint8_t data[17];

  BS_ENTRY;

  data[0] = 0x04;

  priv = elgato_stream_deck_get_instance_private (self);
  hid_get_feature_report (priv->handle, data, sizeof (data));

  firmware_version = g_malloc0 (sizeof (char) * 13);
  memcpy (firmware_version, &data[5], 12);

  BS_RETURN (g_steal_pointer (&firmware_version));
}

static void
set_brightness_mini_original (ElgatoStreamDeck *self,
                              double            brightness)
{
  ElgatoStreamDeckPrivate *priv;

  const uint8_t b = CLAMP (brightness * 100, 0, 100);
  const uint8_t data[] = {
    0x05,
    0x55, 0xaa, 0xd1, 0x01, b   , 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  BS_ENTRY;

  priv = elgato_stream_deck_get_instance_private (self);
  hid_send_feature_report (priv->handle, data, sizeof (data));

  BS_EXIT;
}

static inline uint8_t
swap_button_index_original (ElgatoStreamDeck *self,
                            uint8_t           button_index)
{
  ElgatoStreamDeckPrivate *priv = elgato_stream_deck_get_instance_private (self);

  int column = button_index % priv->model_info->button_layout.columns;
  int actual_index = ((int) button_index - column) + ((int) priv->model_info->button_layout.columns - 1 - column);
  return (uint8_t) actual_index;
}

static gboolean
set_button_texture_original (ElgatoStreamDeck  *self,
                             BsButton          *button,
                             GdkTexture        *texture,
                             GError           **error)
{
  ElgatoStreamDeckPrivate *priv;
  g_autofree uint8_t *payload = NULL;
  g_autofree uint8_t *buffer = NULL;
  BsDeviceRegion *region;
  BsRenderer *renderer;
  const size_t package_size = 8191;
  const size_t header_size = 16;
  uint8_t button_index;
  uint8_t page;
  size_t bytes_remaining;
  size_t report_size;
  size_t buffer_size;

  BS_ENTRY;

  priv = elgato_stream_deck_get_instance_private (self);
  region = bs_button_get_region (button);
  renderer = bs_device_region_get_renderer (region);

  if (!bs_renderer_convert_texture (renderer, texture, (char **) &buffer, &buffer_size, error))
    BS_RETURN (FALSE);

  report_size = buffer_size / 2;

  /*
   * BMP images have fixed byte sizes for a given width and height, and
   * in this case, a 72x72 BMP image should have exactly 15606 bytes.
   */
  g_assert (buffer_size == 15606);
  g_assert (package_size - header_size >= report_size);

  button_index = bs_button_get_position (button);

  payload = g_malloc (sizeof (uint8_t) * package_size);
  payload[0] = 0x02;
  payload[1] = 0x01;
  /* payload[2] set in loop */
  payload[3] = 0;
  /* payload[4] set in loop */
  payload[5] = swap_button_index_original (self, button_index) + 1;
  payload[6] = 0;
  payload[7] = 0;
  payload[8] = 0;
  payload[9] = 0;
  payload[10] = 0;
  payload[11] = 0;
  payload[12] = 0;
  payload[13] = 0;
  payload[14] = 0;
  payload[15] = 0;

  page = 0;
  bytes_remaining = buffer_size;
  while (bytes_remaining > 0)
    {
      size_t padding_size;
      size_t chunk_size;
      size_t bytes_sent;

      chunk_size = MIN (bytes_remaining, report_size);

      payload[2] = page + 1;
      payload[4] = chunk_size == bytes_remaining ? 1 : 0;

      bytes_sent = page * report_size;
      memcpy (payload + header_size, buffer + bytes_sent, chunk_size);

      padding_size = package_size - header_size - chunk_size;
      if (padding_size > 0)
        memset (payload + header_size + chunk_size, 0, padding_size);

      hid_write (priv->handle, payload, package_size);

      bytes_remaining -= chunk_size;
      page++;
    }

  BS_RETURN (TRUE);
}

static gboolean
read_state_original (ElgatoStreamDeck *self)
{
  ElgatoStreamDeckPrivate *priv;
  const BsButtonLayout *layout;
  uint8_t *states;
  size_t states_length;
  int result;

  priv = elgato_stream_deck_get_instance_private (self);

  layout = &priv->model_info->button_layout;
  states_length = layout->n_buttons + 1;

  g_assert (states_length < 8192);
  states = g_alloca (sizeof (uint8_t) * states_length);

  priv = elgato_stream_deck_get_instance_private (self);
  result = hid_read (priv->handle, states, states_length);

  if (result == 0)
    return TRUE;

  for (uint8_t i = 0; i < layout->n_buttons; i++)
    {
      g_autoptr (BsEvent) button_event = NULL;
      BsEventType event_type;
      BsButton *button;
      uint8_t position;

      position = swap_button_index_original (self, i);
      button = find_button_at_region (self, "main-button-grid", position);

      if (states[i + 1] == bs_button_get_pressed (button))
        continue;

      event_type = states[i + 1] ? BS_BUTTON_PRESS : BS_BUTTON_RELEASE;
      button_event = bs_button_event_new (event_type, BS_DEVICE (self), button);
      bs_actionable_handle_event (BS_ACTIONABLE (button), button_event);
    }

  return TRUE;
}

/* 2nd generation */

static void
reset_gen2 (ElgatoStreamDeck *self)
{
  ElgatoStreamDeckPrivate *priv;

  const uint8_t reset_command[] = {
      0x03,
      0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  BS_ENTRY;

  priv = elgato_stream_deck_get_instance_private (self);
  hid_send_feature_report (priv->handle, reset_command, sizeof (reset_command));

  BS_EXIT;
}

static char *
get_serial_number_gen2 (ElgatoStreamDeck *self)
{
  ElgatoStreamDeckPrivate *priv;
  uint8_t data[32];
  char *serial;

  BS_ENTRY;

  data[0] = 0x06;

  priv = elgato_stream_deck_get_instance_private (self);
  hid_get_feature_report (priv->handle, data, sizeof (data));

  serial = g_malloc0 (sizeof (char) * 31);
  memcpy (serial, &data[2], 30);

  BS_RETURN (serial);
}

static char *
get_firmware_version_gen2 (ElgatoStreamDeck *self)
{
  ElgatoStreamDeckPrivate *priv;
  uint8_t data[32];
  char *serial;

  BS_ENTRY;

  data[0] = 0x05;

  priv = elgato_stream_deck_get_instance_private (self);
  hid_get_feature_report (priv->handle, data, sizeof (data));

  serial = g_malloc0 (sizeof (char) * 27);
  memcpy (serial, &data[6], 26);

  BS_RETURN (serial);
}

static void
set_brightness_gen2 (ElgatoStreamDeck *self,
                     double            brightness)
{
  ElgatoStreamDeckPrivate *priv;

  const uint8_t b = CLAMP (brightness * 100, 0, 100);
  const uint8_t data[] = {
    0x03,
    0x08, b   , 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  BS_ENTRY;

  priv = elgato_stream_deck_get_instance_private (self);
  hid_send_feature_report (priv->handle, data, sizeof (data));

  BS_EXIT;
}

static gboolean
set_button_texture_gen2 (ElgatoStreamDeck  *self,
                         BsButton          *button,
                         GdkTexture        *texture,
                         GError           **error)
{
  ElgatoStreamDeckPrivate *priv;
  g_autofree uint8_t *payload = NULL;
  g_autofree uint8_t *buffer = NULL;
  BsDeviceRegion *region;
  BsRenderer *renderer;
  const size_t package_size = 1024;
  const size_t header_size = 8;
  uint8_t page;
  size_t bytes_remaining;
  size_t buffer_size;

  BS_ENTRY;

  priv = elgato_stream_deck_get_instance_private (self);
  region = bs_button_get_region (button);
  renderer = bs_device_region_get_renderer (region);

  if (!bs_renderer_convert_texture (renderer, texture, (char **) &buffer, &buffer_size, error))
    BS_RETURN (FALSE);

  payload = g_malloc (package_size * sizeof (uint8_t));
  payload[0] = 0x02;
  payload[1] = 0x07;
  payload[2] = bs_button_get_position (button);

  page = 0;
  bytes_remaining = buffer_size;
  while (bytes_remaining > 0)
    {
      size_t padding_size;
      size_t chunk_size;
      size_t bytes_sent;

      chunk_size = MIN (bytes_remaining, package_size - header_size);

      payload[3] = chunk_size == bytes_remaining ? 1 : 0;
      payload[4] = chunk_size & 0xff;
      payload[5] = chunk_size >> 8;
      payload[6] = page & 0xff;
      payload[7] = page >> 8;

      bytes_sent = page * (package_size - header_size);
      memcpy (payload + header_size, buffer + bytes_sent, chunk_size);

      padding_size = package_size - header_size - chunk_size;
      if (padding_size > 0)
        memset (payload + header_size + chunk_size, 0, padding_size);

      hid_write (priv->handle, payload, package_size * sizeof (uint8_t));

      bytes_remaining -= chunk_size;
      page++;
    }

  BS_RETURN (TRUE);
}

static gboolean
read_state_gen2 (ElgatoStreamDeck *self)
{
  ElgatoStreamDeckPrivate *priv;
  const BsButtonLayout *layout;
  uint8_t *states;
  size_t states_length;
  int result;

  priv = elgato_stream_deck_get_instance_private (self);

  layout = &priv->model_info->button_layout;
  states_length = layout->n_buttons + 4;

  g_assert (states_length < 8192);
  states = g_alloca (sizeof (uint8_t) * states_length);

  priv = elgato_stream_deck_get_instance_private (self);
  result = hid_read (priv->handle, states, states_length);

  if (result == 0)
    return TRUE;

  for (uint8_t i = 0; i < layout->n_buttons; i++)
    {
      g_autoptr (BsEvent) button_event = NULL;
      BsEventType event_type;
      BsButton *button;

      button = find_button_at_region (self, "main-button-grid", i);

      if (states[i + 4] == bs_button_get_pressed (button))
        continue;

      event_type = states[i + 4] ? BS_BUTTON_PRESS : BS_BUTTON_RELEASE;
      button_event = bs_button_event_new (event_type, BS_DEVICE (self), button);
      bs_actionable_handle_event (BS_ACTIONABLE (button), button_event);
    }

  return TRUE;
}

/* noops for devices without visual feedback */

static void
set_brightness_pedal (ElgatoStreamDeck *self,
                      double            brightness)
{
  BS_ENTRY;
  BS_EXIT;
}

static gboolean
set_button_texture_pedal (ElgatoStreamDeck  *self,
                          BsButton          *button,
                          GdkTexture        *texture,
                          GError           **error)
{
  BS_ENTRY;
  BS_RETURN (TRUE);
}

static void
reset_pedal (ElgatoStreamDeck *self)
{
  BS_ENTRY;
  BS_EXIT;
}

/* Stream Deck Plus */

static inline int
convert_dial_value (uint8_t value)
{
  if (value < 0x80)
    return value;
  else
    return -(0x100 - (int) value);
}

static gboolean
read_state_plus (ElgatoStreamDeck *self)
{
  ElgatoStreamDeckPrivate *priv;
  const BsButtonLayout *layout;
  const size_t states_length = 14;
  BsDevice *device;
  uint8_t *states;
  int result;

  priv = elgato_stream_deck_get_instance_private (self);

  layout = &priv->model_info->button_layout;

  g_assert (states_length < 8192);
  states = g_alloca (sizeof (uint8_t) * states_length);

  priv = elgato_stream_deck_get_instance_private (self);
  result = hid_read (priv->handle, states, states_length);

  if (result == 0)
    return TRUE;

  enum {
    BUTTON_EVENT = 0x00,
    TOUCHSCREEN_EVENT = 0x02,
    DIAL_EVENT = 0x03,
  } event_type = states[1];

  device = BS_DEVICE (self);

  switch (event_type)
    {
    case BUTTON_EVENT:
      for (uint8_t i = 0; i < layout->n_buttons; i++)
        {
          g_autoptr (BsEvent) button_event = NULL;
          BsEventType event_type;
          BsButton *button;

          button = find_button_at_region (self, "main-button-grid", i);

          if (states[i + 4] == bs_button_get_pressed (button))
            continue;

          event_type = states[i + 4] ? BS_BUTTON_PRESS : BS_BUTTON_RELEASE;
          button_event = bs_button_event_new (event_type, device, button);
          bs_actionable_handle_event (BS_ACTIONABLE (button), button_event);
        }
      break;

    case TOUCHSCREEN_EVENT:
      {
        g_autoptr (BsTouchscreenSlot) touchscreen_slot = NULL;
        g_autoptr (BsEvent) touchscreen_event = NULL;
        enum {
          SHORT_PRESS = 1,
          LONG_PRESS = 2,
          SWIPE = 3,
        } touch_event_type = states[4];
        graphene_point_t position;
        BsTouchscreen *touchscreen;

        touchscreen = BS_TOUCHSCREEN (bs_device_get_region (device, "touchscreen"));

        graphene_point_init (&position,
                             (states[7] << 8) + states[6],
                             (states[9] << 8) + states[8]);

        g_debug ("Touchscreen event (%.0fx%.0f)", position.x, position.y);

        touchscreen_slot = bs_touchscreen_pick_slot (touchscreen, &position);

        switch (touch_event_type)
          {
          case SHORT_PRESS:
            touchscreen_event = bs_touchscreen_event_new (BS_TOUCHSCREEN_SHORT_PRESS,
                                                          device,
                                                          touchscreen_slot,
                                                          &position,
                                                          &position);
            break;

          case LONG_PRESS:
            touchscreen_event = bs_touchscreen_event_new (BS_TOUCHSCREEN_LONG_PRESS,
                                                          device,
                                                          touchscreen_slot,
                                                          &position,
                                                          &position);
            break;

          case SWIPE:
            {
              graphene_point_t release_position;

              graphene_point_init (&release_position,
                                   (states[11] << 8) + states[10],
                                   (states[13] << 8) + states[12]);

              touchscreen_event = bs_touchscreen_event_new (BS_TOUCHSCREEN_SWIPE,
                                                            device,
                                                            touchscreen_slot,
                                                            &position,
                                                            &release_position);
            }
            break;
          }

        bs_actionable_handle_event (BS_ACTIONABLE (touchscreen_slot), touchscreen_event);
      }
      break;

    case DIAL_EVENT:
      {
        BsTouchscreen *touchscreen;
        GListModel *touchscreen_slots;
        BsDialGrid *dial_grid;
        GListModel *dials;

        dial_grid = BS_DIAL_GRID (bs_device_get_region (device, "dial-grid"));
        dials = bs_dial_grid_get_dials (dial_grid);
        g_assert (g_list_model_get_n_items (dials) == 4);

        touchscreen = BS_TOUCHSCREEN (bs_device_get_region (device, "touchscreen"));
        touchscreen_slots = bs_touchscreen_get_slots (touchscreen);
        g_assert (g_list_model_get_n_items (touchscreen_slots) == 4);

        for (uint8_t i = 0; i < 4; i++)
          {
            g_autoptr (BsTouchscreenSlot) slot = NULL;
            g_autoptr (BsEvent) dial_event = NULL;
            g_autoptr (BsDial) dial = NULL;
            BsEventType event_type;
            int rotation = 0;

            dial = g_list_model_get_item (dials, i);

            if (states[4] == 0x01)
              {
                event_type = BS_DIAL_ROTATE;
                rotation = convert_dial_value (states[i + 5]);

                if (rotation == 0)
                  continue;
              }
            else
              {
                if (bs_dial_get_pressed (dial) == (gboolean) states[i + 5])
                  continue;

                bs_dial_set_pressed (dial, (gboolean) states[i + 5]);

                event_type = states[i + 5] ? BS_DIAL_PRESS : BS_DIAL_RELEASE;
              }

            /* Hardcode routing dial events to the touchscreen slot. In the
             * future this may become configurable, but for now, it is not.
             */
            dial_event = bs_dial_event_new (event_type, device, dial, rotation);

            slot = g_list_model_get_item (touchscreen_slots, i);
            bs_actionable_handle_event (BS_ACTIONABLE (slot), dial_event);
          }
      }
      break;
    }

  return TRUE;
}

static gboolean
set_touchscreen_texture_plus (ElgatoStreamDeck  *self,
                              BsTouchscreen     *touchscreen,
                              GdkTexture        *texture,
                              GError           **error)
{
  ElgatoStreamDeckPrivate *priv;
  g_autofree uint8_t *payload = NULL;
  g_autofree uint8_t *buffer = NULL;
  BsRenderer *renderer;
  const size_t package_size = 1024;
  const size_t header_size = 16;
  uint8_t x, y;
  uint8_t page;
  size_t bytes_remaining;
  size_t buffer_size;

  BS_ENTRY;

  priv = elgato_stream_deck_get_instance_private (self);
  renderer = bs_device_region_get_renderer (BS_DEVICE_REGION (touchscreen));

  if (!bs_renderer_convert_texture (renderer, texture, (char **) &buffer, &buffer_size, error))
    BS_RETURN (FALSE);

  /* FIXME: we upload the whole texture every time */
  x = 0;
  y = 0;

  payload = g_malloc (package_size * sizeof (uint8_t));
  payload[0] = 0x02;
  payload[1] = 0x0c;
  payload[2] = x & 0xff;
  payload[3] = x >> 8;
  payload[4] = y & 0xff;
  payload[5] = y >> 8;
  payload[6] = (bs_touchscreen_get_width (touchscreen) & 0xff);
  payload[7] = (bs_touchscreen_get_width (touchscreen) >> 8) & 0xff;
  payload[8] = (bs_touchscreen_get_height (touchscreen) & 0xff);
  payload[9] = (bs_touchscreen_get_height (touchscreen) >> 8) & 0xff;

  page = 0;
  bytes_remaining = buffer_size;
  while (bytes_remaining > 0)
    {
      size_t padding_size;
      size_t chunk_size;
      size_t bytes_sent;

      chunk_size = MIN (bytes_remaining, package_size - header_size);

      payload[10] = chunk_size == bytes_remaining ? 1 : 0;
      payload[11] = page & 0xff;
      payload[12] = page >> 8;
      payload[13] = chunk_size & 0xff;
      payload[14] = chunk_size >> 8;
      payload[15] = 0;

      bytes_sent = page * (package_size - header_size);
      memcpy (payload + header_size, buffer + bytes_sent, chunk_size);

      padding_size = package_size - header_size - chunk_size;
      if (padding_size > 0)
        memset (payload + header_size + chunk_size, 0, padding_size);

      hid_write (priv->handle, payload, package_size * sizeof (uint8_t));

      bytes_remaining -= chunk_size;
      page++;
    }

  BS_RETURN (TRUE);
}

static const DeviceModelInfo models_vtable[] = {
  {
    .product_id = STREAMDECK_MINI_PRODUCT_ID,
    /* Translators: this is a product name. In most cases, it is not translated.
     * Please verify if Elgato translates their product names on your locale.
     */
    .name = N_("Stream Deck Mini"),
    .features = ELGATO_STREAM_DECK_FEATURE_BUTTONS,
    .button_layout = {
      .n_buttons = 6,
      .columns = 3,
      .image_info = {
        .width = 80,
        .height = 80,
        .format = BS_IMAGE_FORMAT_BMP,
        .flags = BS_RENDERER_FLAG_FLIP_Y | BS_RENDERER_FLAG_ROTATE_90,
      },
    },
    .reset = reset_mini_original,
    .get_serial_number = get_serial_number_mini_original,
    .get_firmware_version = get_firmware_version_mini_original,
    .set_brightness = set_brightness_mini_original,
    .set_button_texture = set_button_texture_mini,
    .read_state = read_state_mini,
  },
  {
    .product_id = STREAMDECK_MINI_V2_PRODUCT_ID,
    /* Translators: this is a product name. In most cases, it is not translated.
     * Please verify if Elgato translates their product names on your locale.
     */
    .name = N_("Stream Deck Mini"),
    .features = ELGATO_STREAM_DECK_FEATURE_BUTTONS,
    .button_layout = {
      .n_buttons = 6,
      .columns = 3,
      .image_info = {
        .width = 80,
        .height = 80,
        .format = BS_IMAGE_FORMAT_BMP,
        .flags = BS_RENDERER_FLAG_FLIP_Y | BS_RENDERER_FLAG_ROTATE_90,
      },
    },
    .reset = reset_mini_original,
    .get_serial_number = get_serial_number_mini_original,
    .get_firmware_version = get_firmware_version_mini_original,
    .set_brightness = set_brightness_mini_original,
    .set_button_texture = set_button_texture_mini,
    .read_state = read_state_mini,
  },
  {
    .product_id = STREAMDECK_ORIGINAL_PRODUCT_ID,
    /* Translators: this is a product name. In most cases, it is not translated.
     * Please verify if Elgato translates their product names on your locale.
     */
    .name = N_("Stream Deck"),
    .features = ELGATO_STREAM_DECK_FEATURE_BUTTONS,
    .button_layout = {
      .n_buttons = 15,
      .columns = 5,
      .image_info = {
        .width = 72,
        .height = 72,
        .format = BS_IMAGE_FORMAT_BMP,
        .flags = BS_RENDERER_FLAG_FLIP_X | BS_RENDERER_FLAG_FLIP_Y,
      },
    },
    .reset = reset_mini_original,
    .get_serial_number = get_serial_number_mini_original,
    .get_firmware_version = get_firmware_version_mini_original,
    .set_brightness = set_brightness_mini_original,
    .set_button_texture = set_button_texture_original,
    .read_state = read_state_original,
  },
  {
    .product_id = STREAMDECK_ORIGINAL_V2_PRODUCT_ID,
    /* Translators: this is a product name. In most cases, it is not translated.
     * Please verify if Elgato translates their product names on your locale.
     */
    .name = N_("Stream Deck"),
    .features = ELGATO_STREAM_DECK_FEATURE_BUTTONS,
    .button_layout = {
      .n_buttons = 15,
      .columns = 5,
      .image_info = {
        .width = 72,
        .height = 72,
        .format = BS_IMAGE_FORMAT_JPEG,
        .flags = BS_RENDERER_FLAG_FLIP_X | BS_RENDERER_FLAG_FLIP_Y,
      },
    },
    .reset = reset_gen2,
    .get_serial_number = get_serial_number_gen2,
    .get_firmware_version = get_firmware_version_gen2,
    .set_brightness = set_brightness_gen2,
    .set_button_texture = set_button_texture_gen2,
    .read_state = read_state_gen2,
  },
  {
    .product_id = STREAMDECK_XL_PRODUCT_ID,
    /* Translators: this is a product name. In most cases, it is not translated.
     * Please verify if Elgato translates their product names on your locale.
     */
    .name = N_("Stream Deck XL"),
    .features = ELGATO_STREAM_DECK_FEATURE_BUTTONS,
    .button_layout = {
      .n_buttons = 32,
      .columns = 8,
      .image_info = {
        .width = 96,
        .height = 96,
        .format = BS_IMAGE_FORMAT_JPEG,
        .flags = BS_RENDERER_FLAG_FLIP_X | BS_RENDERER_FLAG_FLIP_Y,
      },
    },
    .reset = reset_gen2,
    .get_serial_number = get_serial_number_gen2,
    .get_firmware_version = get_firmware_version_gen2,
    .set_brightness = set_brightness_gen2,
    .set_button_texture = set_button_texture_gen2,
    .read_state = read_state_gen2,
  },
  {
    .product_id = STREAMDECK_XL_V2_PRODUCT_ID,
    /* Translators: this is a product name. In most cases, it is not translated.
     * Please verify if Elgato translates their product names on your locale.
     */
    .name = N_("Stream Deck XL"),
    .features = ELGATO_STREAM_DECK_FEATURE_BUTTONS,
    .button_layout = {
      .n_buttons = 32,
      .columns = 8,
      .image_info = {
        .width = 96,
        .height = 96,
        .format = BS_IMAGE_FORMAT_JPEG,
        .flags = BS_RENDERER_FLAG_FLIP_X | BS_RENDERER_FLAG_FLIP_Y,
      },
    },
    .reset = reset_gen2,
    .get_serial_number = get_serial_number_gen2,
    .get_firmware_version = get_firmware_version_gen2,
    .set_brightness = set_brightness_gen2,
    .set_button_texture = set_button_texture_gen2,
    .read_state = read_state_gen2,
  },
  {
    .product_id = STREAMDECK_MK2_PRODUCT_ID,
    /* Translators: this is a product name. In most cases, it is not translated.
     * Please verify if Elgato translates their product names on your locale.
     */
    .name = N_("Stream Deck MK.2"),
    .features = ELGATO_STREAM_DECK_FEATURE_BUTTONS,
    .button_layout = {
      .n_buttons = 15,
      .columns = 5,
      .image_info = {
        .width = 72,
        .height = 72,
        .format = BS_IMAGE_FORMAT_JPEG,
        .flags = BS_RENDERER_FLAG_FLIP_X | BS_RENDERER_FLAG_FLIP_Y,
      },
    },
    .reset = reset_gen2,
    .get_serial_number = get_serial_number_gen2,
    .get_firmware_version = get_firmware_version_gen2,
    .set_brightness = set_brightness_gen2,
    .set_button_texture = set_button_texture_gen2,
    .read_state = read_state_gen2,
  },
  {
    .product_id = STREAMDECK_PEDAL_PRODUCT_ID,
    /* Translators: this is a product name. In most cases, it is not translated.
     * Please verify if Elgato translates their product names on your locale.
     */
    .name = N_("Stream Deck Pedal"),
    .features = ELGATO_STREAM_DECK_FEATURE_BUTTONS,
    .button_layout = {
      .n_buttons = 3,
      .columns = 3,
      .image_info = {
        .width = 96,
        .height = 96,
        .format = BS_IMAGE_FORMAT_JPEG,
        .flags = BS_RENDERER_FLAG_NONE,
      },
    },
    .reset = reset_pedal,
    .get_serial_number = get_serial_number_gen2,
    .get_firmware_version = get_firmware_version_gen2,
    .set_brightness = set_brightness_pedal,
    .set_button_texture = set_button_texture_pedal,
    .read_state = read_state_gen2,
  },
  {
    .product_id = STREAMDECK_PLUS_PRODUCT_ID,
    /* Translators: this is a product name. In most cases, it is not translated.
     * Please verify if Elgato translates their product names on your locale.
     */
    .name = N_("Stream Deck Plus"),
    .features = ELGATO_STREAM_DECK_FEATURE_BUTTONS |
                ELGATO_STREAM_DECK_FEATURE_TOUCHSCREEN |
                ELGATO_STREAM_DECK_FEATURE_DIALS,
    .button_layout = {
      .n_buttons = 8,
      .columns = 4,
      .image_info = {
        .width = 120,
        .height = 120,
        .format = BS_IMAGE_FORMAT_JPEG,
        .flags = BS_RENDERER_FLAG_NONE,
      },
    },
    .dial_layout = {
      .n_dials = 4,
      .columns = 4,
    },
    .touchscreen_layout = {
      .n_slots = 4,
      .image_info = {
        .width = 800,
        .height = 100,
        .format = BS_IMAGE_FORMAT_JPEG,
        .flags = BS_RENDERER_FLAG_NONE,
      },
    },
    .reset = reset_gen2,
    .get_serial_number = get_serial_number_gen2,
    .get_firmware_version = get_firmware_version_gen2,
    .set_brightness = set_brightness_gen2,
    .set_button_texture = set_button_texture_gen2,
    .set_touchscreen_texture = set_touchscreen_texture_plus,
    .read_state = read_state_plus,
  },
  {
    .product_id = STREAMDECK_NEO_PRODUCT_ID,
    /* Translators: this is a product name. In most cases, it is not translated.
     * Please verify if Elgato translates their product names on your locale.
     */
    .name = N_("Stream Deck Neo"),
    .features = ELGATO_STREAM_DECK_FEATURE_BUTTONS,
    .button_layout = {
      .n_buttons = 8,
      .columns = 4,
      .image_info = {
        .width = 96,
        .height = 96,
        .format = BS_IMAGE_FORMAT_JPEG,
        .flags = BS_RENDERER_FLAG_FLIP_X | BS_RENDERER_FLAG_FLIP_Y,
      },
    },
    .reset = reset_gen2,
    .get_serial_number = get_serial_number_gen2,
    .get_firmware_version = get_firmware_version_gen2,
    .set_brightness = set_brightness_gen2,
    .set_button_texture = set_button_texture_gen2,
    .read_state = read_state_gen2,
  },
};


/*
 * GSource
 */

static void
apply_button_update (ElgatoStreamDeck *self,
                     BsButtonUpdate   *button_update)
{
  ElgatoStreamDeckPrivate *priv = elgato_stream_deck_get_instance_private (self);
  g_autoptr (GdkTexture) texture = NULL;
  g_autoptr (GError) error = NULL;
  BsDeviceRegion *region;
  BsRenderer *renderer;
  BsButton *button;
  BsIcon *icon;

  BS_TRACE_MSG ("Applying button update %p", button_update);

  g_return_if_fail (BS_IS_DEVICE (self));
  g_return_if_fail (priv->model_info->set_button_texture != NULL);

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

  priv->model_info->set_button_texture (self, button, texture, &error);
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
  ElgatoStreamDeckPrivate *priv = elgato_stream_deck_get_instance_private (self);
  g_autoptr (GdkTexture) texture = NULL;
  BsTouchscreenContent *content;
  g_autoptr (GError) error = NULL;
  BsTouchscreen *touchscreen;
  BsRenderer *renderer;

  g_return_if_fail (BS_IS_DEVICE (self));
  g_return_if_fail (priv->model_info->set_button_texture != NULL);

  touchscreen = touchscreen_update->touchscreen;
  content = bs_touchscreen_get_content (touchscreen);
  renderer = bs_device_region_get_renderer (BS_DEVICE_REGION (touchscreen));
  texture = bs_renderer_compose_touchscreen_content (renderer, content, &error);

  if (error)
    {
      g_warning ("Error compositing touchscreen texture: %s", error->message);
      return;
    }

  priv->model_info->set_touchscreen_texture (self, touchscreen, texture, &error);

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
  ElgatoStreamDeckPrivate *priv;
  StreamDeckSource *stream_deck_source;
  ElgatoStreamDeck *self;
  gint64 current_time;
  gint64 expiration;

  stream_deck_source = (StreamDeckSource *)source;
  self = stream_deck_source->stream_deck;
  priv = elgato_stream_deck_get_instance_private (self);

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

  priv->model_info->read_state (self);

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

  g_assert (priv->gusb_device != NULL);

  if (g_usb_device_get_vid (priv->gusb_device) != ELGATO_SYSTEMS_VENDOR_ID)
    {
      g_set_error (error,
                   ELGATO_STREAM_DECK_ERROR,
                   ELGATO_STREAM_DECK_ERROR_UNRECOGNIZED,
                   "Not an Elgato device");
      BS_RETURN (FALSE);
    }

  for (size_t i = 0; i < G_N_ELEMENTS (models_vtable); i++)
    {
      if (g_usb_device_get_pid (priv->gusb_device) == models_vtable[i].product_id)
        {
          priv->model_info = &models_vtable[i];
          break;
        }
    }

  if (!priv->model_info)
    {
      g_set_error (error,
                   ELGATO_STREAM_DECK_ERROR,
                   ELGATO_STREAM_DECK_ERROR_UNRECOGNIZED,
                   "Not a recognized Stream Deck device");
      BS_RETURN (FALSE);
    }

  priv->handle = hid_open (g_usb_device_get_vid (priv->gusb_device),
                           g_usb_device_get_pid (priv->gusb_device),
                           NULL);

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

  priv->serial_number = priv->model_info->get_serial_number (self);
  priv->firmware_version = priv->model_info->get_firmware_version (self);

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

static GListModel *
elgato_stream_deck_create_layout (BsDevice *device)
{
  g_autoptr (BsDeviceLayoutBuilder) layout_builder = NULL;
  ElgatoStreamDeckPrivate *priv;
  ElgatoStreamDeck *self;
  unsigned int row = 0;

  self = (ElgatoStreamDeck *) device;
  g_assert (ELGATO_IS_STREAM_DECK (self));

  priv = elgato_stream_deck_get_instance_private (self);

  /* All Elgato Stream Decks have one button grid */
  g_assert (priv->model_info->features & ELGATO_STREAM_DECK_FEATURE_BUTTONS);

  layout_builder = bs_device_layout_builder_new (device);

  if (priv->model_info->features & ELGATO_STREAM_DECK_FEATURE_BUTTONS)
    {
      bs_device_layout_builder_add_button_grid (layout_builder,
                                                "main-button-grid",
                                                &priv->model_info->button_layout.image_info,
                                                priv->model_info->button_layout.n_buttons,
                                                priv->model_info->button_layout.columns,
                                                0, row++, 1, 1);
    }

  if (priv->model_info->features & ELGATO_STREAM_DECK_FEATURE_TOUCHSCREEN)
    {
      bs_device_layout_builder_add_touchscreen (layout_builder,
                                                "touchscreen",
                                                &priv->model_info->touchscreen_layout.image_info,
                                                priv->model_info->touchscreen_layout.n_slots,
                                                0, row++, 1, 1);
    }

  if (priv->model_info->features & ELGATO_STREAM_DECK_FEATURE_DIALS)
    {
      bs_device_layout_builder_add_dial_grid (layout_builder,
                                              "dial-grid",
                                              priv->model_info->dial_layout.n_dials,
                                              priv->model_info->dial_layout.columns,
                                              0, row++, 1, 1);
    }

  return bs_device_layout_builder_build (layout_builder);
}

static const char *
elgato_stream_deck_get_firmware_version (BsDevice *device)
{
  ElgatoStreamDeck *self = (ElgatoStreamDeck *) device;
  ElgatoStreamDeckPrivate *priv = elgato_stream_deck_get_instance_private (self);

  g_assert (ELGATO_IS_STREAM_DECK (self));

  return priv->firmware_version;
}

static const char *
elgato_stream_deck_get_name (BsDevice *device)
{
  ElgatoStreamDeck *self = (ElgatoStreamDeck *) device;
  ElgatoStreamDeckPrivate *priv = elgato_stream_deck_get_instance_private (self);

  g_assert (ELGATO_IS_STREAM_DECK (self));

  return _(priv->model_info->name);
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
elgato_stream_deck_reset (BsDevice *device)
{
  ElgatoStreamDeck *self = (ElgatoStreamDeck *) device;
  ElgatoStreamDeckPrivate *priv = elgato_stream_deck_get_instance_private (self);

  priv->model_info->reset (self);
}

static void
elgato_stream_deck_set_brightness (BsDevice *device,
                                   double    brightness)
{
  ElgatoStreamDeck *self = (ElgatoStreamDeck *) device;
  ElgatoStreamDeckPrivate *priv = elgato_stream_deck_get_instance_private (self);

  BS_DEVICE_CLASS (elgato_stream_deck_parent_class)->set_brightness (device, brightness);

  priv->model_info->set_brightness (self, brightness);
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

  if (priv->gusb_device)
    g_usb_device_close (priv->gusb_device, NULL);

  g_clear_pointer (&priv->poll_source, g_source_unref);
  g_clear_pointer (&priv->handle, hid_close);
  g_clear_object (&priv->gusb_device);

  G_OBJECT_CLASS (elgato_stream_deck_parent_class)->finalize (object);
}

static void
elgato_stream_deck_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ElgatoStreamDeck *self = ELGATO_STREAM_DECK (object);
  ElgatoStreamDeckPrivate *priv = elgato_stream_deck_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_GUSB_DEVICE:
      g_value_set_object (value, priv->gusb_device);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
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
    case PROP_GUSB_DEVICE:
      g_assert (priv->gusb_device == NULL);
      priv->gusb_device = g_value_dup_object (value);
      g_assert (priv->gusb_device != NULL);
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
  object_class->get_property = elgato_stream_deck_get_property;
  object_class->set_property = elgato_stream_deck_set_property;

  device_class->create_layout = elgato_stream_deck_create_layout;
  device_class->get_firmware_version = elgato_stream_deck_get_firmware_version;
  device_class->get_name = elgato_stream_deck_get_name;
  device_class->get_serial_number = elgato_stream_deck_get_serial_number;
  device_class->load = elgato_stream_deck_load;
  device_class->reset = elgato_stream_deck_reset;
  device_class->set_brightness = elgato_stream_deck_set_brightness;

  properties[PROP_GUSB_DEVICE] = g_param_spec_object ("gusb-device", NULL, NULL,
                                                      G_USB_TYPE_DEVICE,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
elgato_stream_deck_init (ElgatoStreamDeck *self)
{
}

BsDevice *
elgato_stream_deck_new (GUsbDevice  *gusb_device,
                        GError     **out_error)
{
  return g_initable_new (ELGATO_TYPE_STREAM_DECK,
                         NULL,
                         out_error,
                         "gusb-device", gusb_device,
                         NULL);
}

GUsbDevice *
elgato_stream_deck_get_gusb_device (ElgatoStreamDeck *self)
{
  ElgatoStreamDeckPrivate *priv;

  g_assert (ELGATO_STREAM_DECK (self));

  priv = elgato_stream_deck_get_instance_private (self);
  return priv->gusb_device;
}
