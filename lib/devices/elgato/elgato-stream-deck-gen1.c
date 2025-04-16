/*
 * elgato-stream-deck-gen1.c
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

#define G_LOG_DOMAIN "Elgato Stream Deck Gen1"

#include "bs-debug.h"
#include "elgato-stream-deck-gen1.h"

G_DEFINE_ABSTRACT_TYPE (ElgatoStreamDeckGen1, elgato_stream_deck_gen1, ELGATO_TYPE_STREAM_DECK)


/*
 * ElgatoStreamDeck overrides
 */

static char *
elgato_stream_deck_gen1_get_firmware_version (ElgatoStreamDeck *stream_deck)
{
  g_autofree char *firmware_version = NULL;
  hid_device *hid_device;
  uint8_t data[17];

  BS_ENTRY;

  data[0] = 0x04;

  hid_device = elgato_stream_deck_get_hid_device (stream_deck);
  hid_get_feature_report (hid_device, data, sizeof (data));

  firmware_version = g_malloc0 (sizeof (char) * 13);
  memcpy (firmware_version, &data[5], 12);

  BS_RETURN (g_steal_pointer (&firmware_version));
}

static char *
elgato_stream_deck_gen1_get_serial_number (ElgatoStreamDeck *stream_deck)
{
  g_autofree char *serial = NULL;
  hid_device *hid_device;
  uint8_t data[17];

  BS_ENTRY;

  data[0] = 0x03;

  hid_device = elgato_stream_deck_get_hid_device (stream_deck);
  hid_get_feature_report (hid_device, data, sizeof (data));

  serial = g_malloc0 (sizeof (char) * 13);
  memcpy (serial, &data[5], 12);

  BS_RETURN (g_steal_pointer (&serial));
}

static void
elgato_stream_deck_gen1_set_brightness (ElgatoStreamDeck *stream_deck,
                                        double            brightness)
{
  const uint8_t b = CLAMP (brightness * 100, 0, 100);
  const uint8_t data[] = {
    0x05,
    0x55, 0xaa, 0xd1, 0x01, b   , 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  hid_device *hid_device;

  BS_ENTRY;

  hid_device = elgato_stream_deck_get_hid_device (stream_deck);
  hid_send_feature_report (hid_device, data, sizeof (data));

  BS_EXIT;
}


/*
 * BsDevice overrides
 */

static void
elgato_stream_deck_gen1_reset (BsDevice *device)
{
  const uint8_t reset_command[] = {
    0x0b,
    0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  hid_device *hid_device;

  BS_ENTRY;

  hid_device = elgato_stream_deck_get_hid_device (ELGATO_STREAM_DECK (device));
  hid_send_feature_report (hid_device, reset_command, sizeof (reset_command));

  BS_EXIT;
}


static void
elgato_stream_deck_gen1_class_init (ElgatoStreamDeckGen1Class *klass)
{
  BsDeviceClass *device_class = BS_DEVICE_CLASS (klass);
  ElgatoStreamDeckClass *stream_deck_class = ELGATO_STREAM_DECK_CLASS (klass);

  device_class->reset = elgato_stream_deck_gen1_reset;

  stream_deck_class->get_firmware_version = elgato_stream_deck_gen1_get_firmware_version;
  stream_deck_class->get_serial_number = elgato_stream_deck_gen1_get_serial_number;
  stream_deck_class->set_brightness = elgato_stream_deck_gen1_set_brightness;
}

static void
elgato_stream_deck_gen1_init (ElgatoStreamDeckGen1 *self)
{
}
