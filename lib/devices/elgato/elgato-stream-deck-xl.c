/*
 * elgato-stream-deck-xl.c
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

#define G_LOG_DOMAIN "Elgato Stream Deck XL"

#include "bs-device-layout-builder-private.h"
#include "bs-renderer-private.h"
#include "elgato-stream-deck-xl.h"

#include <glib/gi18n.h>

struct _ElgatoStreamDeckXL
{
  ElgatoStreamDeckGen2 parent_instance;
};

G_DEFINE_FINAL_TYPE (ElgatoStreamDeckXL, elgato_stream_deck_xl, ELGATO_TYPE_STREAM_DECK_GEN2)


/*
 * BsDevice overrides
 */

static GListModel *
elgato_stream_deck_xl_create_layout (BsDevice *device)
{
  g_autoptr (BsDeviceLayoutBuilder) layout_builder = NULL;

  g_assert (ELGATO_IS_STREAM_DECK_XL (device));

  layout_builder = bs_device_layout_builder_new (device);

  /* Buttons */
  bs_device_layout_builder_add_button_grid (layout_builder,
                                            "main-button-grid",
                                            &(BsImageInfo) {
                                              .width = 96,
                                              .height = 96,
                                              .format = BS_IMAGE_FORMAT_JPEG,
                                              .flags = BS_RENDERER_FLAG_FLIP_X | BS_RENDERER_FLAG_FLIP_Y,
                                            },
                                            32, /* n_buttons */
                                            8, /* columns */
                                            0, 0, 1, 1);

  return bs_device_layout_builder_build (layout_builder);
}

static const char *
elgato_stream_deck_xl_get_name (BsDevice *device)
{
  /* Translators: this is a product name. In most cases, it is not translated.
   * Please verify if Elgato translates their product names on your locale.
   */
  return _("Elgato Stream Deck XL");
}

static void
elgato_stream_deck_xl_class_init (ElgatoStreamDeckXLClass *klass)
{
  BsDeviceClass *device_class = BS_DEVICE_CLASS (klass);

  device_class->create_layout = elgato_stream_deck_xl_create_layout;
  device_class->get_name = elgato_stream_deck_xl_get_name;
}

static void
elgato_stream_deck_xl_init (ElgatoStreamDeckXL *self)
{
}
