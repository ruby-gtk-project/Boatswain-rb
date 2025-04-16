/*
 * elgato-stream-deck-plus.c
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

#define G_LOG_DOMAIN "Elgato Stream Deck Plus"

#include "bs-actionable-private.h"
#include "bs-button.h"
#include "bs-button-grid.h"
#include "bs-debug.h"
#include "bs-device-layout-builder-private.h"
#include "bs-dial-private.h"
#include "bs-dial-grid.h"
#include "bs-events-private.h"
#include "bs-renderer-private.h"
#include "bs-touchscreen-private.h"
#include "bs-touchscreen-slot-private.h"
#include "elgato-stream-deck-plus.h"

#include <glib/gi18n.h>

struct _ElgatoStreamDeckPlus
{
  ElgatoStreamDeckGen2 parent_instance;
};

G_DEFINE_FINAL_TYPE (ElgatoStreamDeckPlus, elgato_stream_deck_plus, ELGATO_TYPE_STREAM_DECK_GEN2)

/*
 * Auxiliary functions
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

static inline int
convert_dial_value (uint8_t value)
{
  if (value < 0x80)
    return value;
  else
    return -(0x100 - (int) value);
}


/*
 * ElgatoStreamDeck overrides
 */

static gboolean
elgato_stream_deck_plus_read_state (ElgatoStreamDeck *stream_deck)
{
  hid_device *hid_device;
  const size_t states_length = 14;
  BsDevice *device;
  uint8_t *states;
  int result;

  g_assert (states_length < 8192);
  states = g_alloca (sizeof (uint8_t) * states_length);

  hid_device = elgato_stream_deck_get_hid_device (stream_deck);
  result = hid_read (hid_device, states, states_length);

  if (result == 0)
    return TRUE;

  enum {
    BUTTON_EVENT = 0x00,
    TOUCHSCREEN_EVENT = 0x02,
    DIAL_EVENT = 0x03,
  } event_type = states[1];

  device = BS_DEVICE (stream_deck);

  switch (event_type)
    {
    case BUTTON_EVENT:
      {
        BsDeviceRegion *button_grid;
        GListModel *buttons;
        size_t n_buttons;

        button_grid = bs_device_get_region (device, "main-button-grid");
        buttons = bs_button_grid_get_buttons (BS_BUTTON_GRID (button_grid));
        n_buttons = g_list_model_get_n_items (buttons);

        for (uint8_t i = 0; i < n_buttons; i++)
          {
            g_autoptr (BsEvent) button_event = NULL;
            BsEventType event_type;
            BsButton *button;

            button = find_button_at_region (stream_deck, "main-button-grid", i);

            if (states[i + 4] == bs_button_get_pressed (button))
              continue;

            event_type = states[i + 4] ? BS_BUTTON_PRESS : BS_BUTTON_RELEASE;
            button_event = bs_button_event_new (event_type, device, button);
            bs_actionable_handle_event (BS_ACTIONABLE (button), button_event);
          }
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
elgato_stream_deck_plus_set_touchscreen_texture (ElgatoStreamDeck  *stream_deck,
                                                 BsTouchscreen     *touchscreen,
                                                 GdkTexture        *texture,
                                                 GError           **error)
{
  g_autofree uint8_t *payload = NULL;
  g_autofree uint8_t *buffer = NULL;
  BsRenderer *renderer;
  hid_device *hid_device;
  const size_t package_size = 1024;
  const size_t header_size = 16;
  uint8_t x, y;
  uint8_t page;
  size_t bytes_remaining;
  size_t buffer_size;

  BS_ENTRY;

  hid_device = elgato_stream_deck_get_hid_device (stream_deck);
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

      hid_write (hid_device, payload, package_size * sizeof (uint8_t));

      bytes_remaining -= chunk_size;
      page++;
    }

  BS_RETURN (TRUE);
}


/*
 * BsDevice overrides
 */

static GListModel *
elgato_stream_deck_plus_create_layout (BsDevice *device)
{
  g_autoptr (BsDeviceLayoutBuilder) layout_builder = NULL;

  g_assert (ELGATO_IS_STREAM_DECK_PLUS (device));

  layout_builder = bs_device_layout_builder_new (device);

  /* Buttons */
  bs_device_layout_builder_add_button_grid (layout_builder,
                                            "main-button-grid",
                                            &(BsImageInfo) {
                                              .width = 120,
                                              .height = 120,
                                              .format = BS_IMAGE_FORMAT_JPEG,
                                              .flags = BS_RENDERER_FLAG_NONE,
                                            },
                                            8, /* n_buttons */
                                            4, /* columns */
                                            0, 0, 1, 1);

  /* Touchscreen */
  bs_device_layout_builder_add_touchscreen (layout_builder,
                                            "touchscreen",
                                            &(BsImageInfo) {
                                              .width = 800,
                                              .height = 100,
                                              .format = BS_IMAGE_FORMAT_JPEG,
                                              .flags = BS_RENDERER_FLAG_NONE,
                                            },
                                            4, /* slots */
                                            0, 1, 1, 1);

  /* Dials */
  bs_device_layout_builder_add_dial_grid (layout_builder,
                                          "dial-grid",
                                          4, /* n_dials */
                                          4, /* columns */
                                          0, 2, 1, 1);

  return bs_device_layout_builder_build (layout_builder);
}

static const char *
elgato_stream_deck_plus_get_name (BsDevice *device)
{
  /* Translators: this is a product name. In most cases, it is not translated.
   * Please verify if Elgato translates their product names on your locale.
   */
  return _("Elgato Stream Deck Plus");
}

static void
elgato_stream_deck_plus_class_init (ElgatoStreamDeckPlusClass *klass)
{
  BsDeviceClass *device_class = BS_DEVICE_CLASS (klass);
  ElgatoStreamDeckClass *stream_deck_class = ELGATO_STREAM_DECK_CLASS (klass);

  device_class->create_layout = elgato_stream_deck_plus_create_layout;
  device_class->get_name = elgato_stream_deck_plus_get_name;

  stream_deck_class->read_state = elgato_stream_deck_plus_read_state;
  stream_deck_class->set_touchscreen_texture = elgato_stream_deck_plus_set_touchscreen_texture;
}

static void
elgato_stream_deck_plus_init (ElgatoStreamDeckPlus *self)
{
}
