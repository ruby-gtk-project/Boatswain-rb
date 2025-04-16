/*
 * elgato-stream-deck-pedal.c
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

#define G_LOG_DOMAIN "Elgato Stream Deck Pedal"

#include "bs-device-layout-builder-private.h"
#include "bs-renderer.h"
#include "elgato-stream-deck-pedal.h"

#include <glib/gi18n.h>

struct _ElgatoStreamDeckPedal
{
  ElgatoStreamDeckGen2 parent_instance;
};

G_DEFINE_FINAL_TYPE (ElgatoStreamDeckPedal, elgato_stream_deck_pedal, ELGATO_TYPE_STREAM_DECK_GEN2)


/*
 * ElgatoStreamDeck overrides
 */

static void
elgato_stream_deck_pedal_set_brightness (ElgatoStreamDeck *stream_deck,
                                         double            brightness)
{
}

static gboolean
elgato_stream_deck_pedal_set_button_texture (ElgatoStreamDeck  *stream_deck,
                                             BsButton          *button,
                                             GdkTexture        *texture,
                                             GError           **error)
{
  return TRUE;
}


/*
 * BsDevice overrides
 */

static GListModel *
elgato_stream_deck_plus_create_layout (BsDevice *device)
{
  g_autoptr (BsDeviceLayoutBuilder) layout_builder = NULL;

  g_assert (ELGATO_IS_STREAM_DECK_PEDAL (device));

  layout_builder = bs_device_layout_builder_new (device);

  bs_device_layout_builder_add_button_grid (layout_builder,
                                            "main-button-grid",
                                            &(BsImageInfo) {
                                              .width = 96,
                                              .height = 96,
                                              .format = BS_IMAGE_FORMAT_JPEG,
                                              .flags = BS_RENDERER_FLAG_NONE,
                                            },
                                            3, /* n_buttons */
                                            3, /* columns */
                                            0, 0, 1, 1);

  return bs_device_layout_builder_build (layout_builder);
}

static const char *
elgato_stream_deck_pedal_get_name (BsDevice *device)
{
  /* Translators: this is a product name. In most cases, it is not translated.
   * Please verify if Elgato translates their product names on your locale.
   */
  return _("Elgato Stream Deck Pedal");
}

static void
elgato_stream_deck_pedal_class_init (ElgatoStreamDeckPedalClass *klass)
{
  BsDeviceClass *device_class = BS_DEVICE_CLASS (klass);
  ElgatoStreamDeckClass *stream_deck_class = ELGATO_STREAM_DECK_CLASS (klass);

  device_class->create_layout = elgato_stream_deck_plus_create_layout;
  device_class->get_name = elgato_stream_deck_pedal_get_name;

  stream_deck_class->set_brightness = elgato_stream_deck_pedal_set_brightness;
  stream_deck_class->set_button_texture = elgato_stream_deck_pedal_set_button_texture;
}

static void
elgato_stream_deck_pedal_init (ElgatoStreamDeckPedal *self)
{
}
