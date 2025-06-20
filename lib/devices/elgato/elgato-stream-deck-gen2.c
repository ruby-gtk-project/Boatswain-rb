/*
 * elgato-stream-deck-gen2.c
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

#define G_LOG_DOMAIN "Elgato Stream Deck Gen2"

#include "bs-actionable-private.h"
#include "bs-button.h"
#include "bs-button-grid.h"
#include "bs-debug.h"
#include "bs-renderer-private.h"
#include "bs-events-private.h"
#include "elgato-stream-deck-gen2.h"

G_DEFINE_ABSTRACT_TYPE (ElgatoStreamDeckGen2, elgato_stream_deck_gen2, ELGATO_TYPE_STREAM_DECK)


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
 * ElgatoStreamDeck overrides
 */

static char *
elgato_stream_deck_gen2_get_firmware_version (ElgatoStreamDeck *stream_deck)
{
  hid_device *hid_device;
  uint8_t data[32];
  char *serial;

  BS_ENTRY;

  data[0] = 0x05;

  hid_device = elgato_stream_deck_get_hid_device (stream_deck);
  hid_get_feature_report (hid_device, data, sizeof (data));

  serial = g_malloc0 (sizeof (char) * 27);
  memcpy (serial, &data[6], 26);

  BS_RETURN (serial);
}

static char *
elgato_stream_deck_gen2_get_serial_number (ElgatoStreamDeck *stream_deck)
{
  hid_device *hid_device;
  uint8_t data[32];
  char *serial;

  BS_ENTRY;

  data[0] = 0x06;

  hid_device = elgato_stream_deck_get_hid_device (stream_deck);
  hid_get_feature_report (hid_device, data, sizeof (data));

  serial = g_malloc0 (sizeof (char) * 31);
  memcpy (serial, &data[2], 30);

  BS_RETURN (serial);
}

static gboolean
elgato_stream_deck_gen2_read_state (ElgatoStreamDeck *stream_deck)
{
  BsDeviceRegion *button_grid;
  GListModel *buttons;
  hid_device *hid_device;
  uint8_t *states;
  size_t states_length;
  size_t n_buttons;
  int result;

  button_grid = bs_device_get_region (BS_DEVICE (stream_deck), "main-button-grid");
  buttons = bs_button_grid_get_buttons (BS_BUTTON_GRID (button_grid));
  n_buttons = g_list_model_get_n_items (buttons);
  states_length = n_buttons + 4;

  g_assert (states_length < 8192);
  states = g_alloca (sizeof (uint8_t) * states_length);

  hid_device = elgato_stream_deck_get_hid_device (stream_deck);
  result = hid_read (hid_device, states, states_length);

  if (result == 0)
    return TRUE;

  for (uint8_t i = 0; i < n_buttons; i++)
    {
      g_autoptr (BsEvent) button_event = NULL;
      BsEventType event_type;
      BsButton *button;

      button = find_button_at_region (stream_deck, "main-button-grid", i);

      if (states[i + 4] == bs_button_get_pressed (button))
        continue;

      event_type = states[i + 4] ? BS_BUTTON_PRESS : BS_BUTTON_RELEASE;
      button_event = bs_button_event_new (event_type, BS_DEVICE (stream_deck), button);
      bs_actionable_handle_event (BS_ACTIONABLE (button), button_event);
    }

  return TRUE;
}

static void
elgato_stream_deck_gen2_set_brightness (ElgatoStreamDeck *stream_deck,
                                        double            brightness)
{
  const uint8_t b = CLAMP (brightness * 100, 0, 100);
  const uint8_t data[] = {
    0x03,
    0x08, b   , 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  hid_device *hid_device;

  BS_ENTRY;

  hid_device = elgato_stream_deck_get_hid_device (stream_deck);
  hid_send_feature_report (hid_device, data, sizeof (data));

  BS_EXIT;
}

static gboolean
elgato_stream_deck_gen2_set_button_texture (ElgatoStreamDeck  *stream_deck,
                                            BsButton          *button,
                                            GdkTexture        *texture,
                                            GError           **error)
{
  g_autofree uint8_t *payload = NULL;
  g_autoptr (GBytes) bytes = NULL;
  BsDeviceRegion *region;
  hid_device *hid_device;
  BsRenderer *renderer;
  gconstpointer buffer;
  const size_t package_size = 1024;
  const size_t header_size = 8;
  uint8_t page;
  size_t bytes_remaining;

  BS_ENTRY;

  hid_device = elgato_stream_deck_get_hid_device (stream_deck);
  region = bs_button_get_region (button);
  renderer = bs_device_region_get_renderer (region);

  bytes = bs_renderer_convert_texture (renderer, texture, error);
  if (!bytes)
    BS_RETURN (FALSE);

  payload = g_malloc (package_size * sizeof (uint8_t));
  payload[0] = 0x02;
  payload[1] = 0x07;
  payload[2] = bs_button_get_position (button);

  page = 0;
  buffer = g_bytes_get_data (bytes, &bytes_remaining);
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

      hid_write (hid_device, payload, package_size * sizeof (uint8_t));

      bytes_remaining -= chunk_size;
      page++;
    }

  BS_RETURN (TRUE);
}


/*
 * BsDevice overrides
 */

static void
elgato_stream_deck_gen2_reset (BsDevice *device)
{
  const uint8_t reset_command[] = {
      0x03,
      0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  hid_device *hid_device;

  BS_ENTRY;

  hid_device = elgato_stream_deck_get_hid_device (ELGATO_STREAM_DECK (device));
  hid_send_feature_report (hid_device, reset_command, sizeof (reset_command));

  BS_EXIT;
}


static void
elgato_stream_deck_gen2_class_init (ElgatoStreamDeckGen2Class *klass)
{
  BsDeviceClass *device_class = BS_DEVICE_CLASS (klass);
  ElgatoStreamDeckClass *stream_deck_class = ELGATO_STREAM_DECK_CLASS (klass);

  device_class->reset = elgato_stream_deck_gen2_reset;

  stream_deck_class->get_firmware_version = elgato_stream_deck_gen2_get_firmware_version;
  stream_deck_class->get_serial_number = elgato_stream_deck_gen2_get_serial_number;
  stream_deck_class->read_state = elgato_stream_deck_gen2_read_state;
  stream_deck_class->set_brightness = elgato_stream_deck_gen2_set_brightness;
  stream_deck_class->set_button_texture = elgato_stream_deck_gen2_set_button_texture;
}

static void
elgato_stream_deck_gen2_init (ElgatoStreamDeckGen2 *self)
{
}
