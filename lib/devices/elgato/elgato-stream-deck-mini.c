/*
 * elgato-stream-deck-mini.c
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

#define G_LOG_DOMAIN "Elgato Stream Deck Mini"

#include "bs-actionable-private.h"
#include "bs-button.h"
#include "bs-button-grid.h"
#include "bs-debug.h"
#include "bs-device-layout-builder-private.h"
#include "bs-events-private.h"
#include "bs-renderer-private.h"
#include "elgato-stream-deck-mini.h"

#include <glib/gi18n.h>

struct _ElgatoStreamDeckMini
{
  ElgatoStreamDeckGen1 parent_instance;
};

G_DEFINE_FINAL_TYPE (ElgatoStreamDeckMini, elgato_stream_deck_mini, ELGATO_TYPE_STREAM_DECK_GEN1)


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

static gboolean
elgato_stream_deck_mini_read_state (ElgatoStreamDeck *stream_deck)
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
  states_length = n_buttons + 1;

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

      if (states[i + 1] == bs_button_get_pressed (button))
        continue;

      event_type = states[i + 1] ? BS_BUTTON_PRESS : BS_BUTTON_RELEASE;
      button_event = bs_button_event_new (event_type, BS_DEVICE (stream_deck), button);
      bs_actionable_handle_event (BS_ACTIONABLE (button), button_event);
    }

  return TRUE;
}

static gboolean
elgato_stream_deck_mini_set_button_texture (ElgatoStreamDeck  *stream_deck,
                                            BsButton          *button,
                                            GdkTexture        *texture,
                                            GError           **error)
{
  g_autofree uint8_t *payload = NULL;
  g_autoptr (GBytes) bytes = NULL;
  BsDeviceRegion *region;
  BsRenderer *renderer;
  hid_device *hid_device;
  gconstpointer buffer;
  const size_t package_size = 1024;
  const size_t header_size = 16;
  uint8_t page;
  size_t bytes_remaining;

  BS_ENTRY;

  region = bs_button_get_region (button);
  renderer = bs_device_region_get_renderer (region);

  bytes = bs_renderer_convert_texture (renderer, texture, error);
  if (!bytes)
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

  hid_device = elgato_stream_deck_get_hid_device (stream_deck);

  page = 0;
  buffer = g_bytes_get_data (bytes, &bytes_remaining);
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

      hid_write (hid_device, payload, package_size);

      bytes_remaining -= chunk_size;
      page++;
    }

  BS_RETURN (TRUE);
}



/*
 * BsDevice overrides
 */

static GListModel *
elgato_stream_deck_mini_create_layout (BsDevice *device)
{
  g_autoptr (BsDeviceLayoutBuilder) layout_builder = NULL;

  g_assert (ELGATO_IS_STREAM_DECK_MINI (device));

  layout_builder = bs_device_layout_builder_new (device);

  /* Buttons */
  bs_device_layout_builder_add_button_grid (layout_builder,
                                            "main-button-grid",
                                            &(BsImageInfo) {
                                              .width = 80,
                                              .height = 80,
                                              .format = BS_IMAGE_FORMAT_BMP,
                                              .flags = BS_RENDERER_FLAG_FLIP_Y | BS_RENDERER_FLAG_ROTATE_90,
                                            },
                                            6, /* n_buttons */
                                            3, /* columns */
                                            0, 0, 1, 1);

  return bs_device_layout_builder_build (layout_builder);
}

static const char *
elgato_stream_deck_mini_get_name (BsDevice *device)
{
  /* Translators: this is a product name. In most cases, it is not translated.
   * Please verify if Elgato translates their product names on your locale.
   */
  return _("Elgato Stream Deck Mini");
}

static void
elgato_stream_deck_mini_class_init (ElgatoStreamDeckMiniClass *klass)
{
  BsDeviceClass *device_class = BS_DEVICE_CLASS (klass);
  ElgatoStreamDeckClass *stream_deck_class = ELGATO_STREAM_DECK_CLASS (klass);

  device_class->create_layout = elgato_stream_deck_mini_create_layout;
  device_class->get_name = elgato_stream_deck_mini_get_name;

  stream_deck_class->read_state = elgato_stream_deck_mini_read_state;
  stream_deck_class->set_button_texture = elgato_stream_deck_mini_set_button_texture;
}

static void
elgato_stream_deck_mini_init (ElgatoStreamDeckMini *self)
{
}
