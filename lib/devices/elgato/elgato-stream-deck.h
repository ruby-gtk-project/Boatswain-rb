/*
 * elgato-stream-deck.h
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

#pragma once

#include <gusb.h>

#include "bs-device-private.h"

G_BEGIN_DECLS

/**
 * ElgatoStreamDeckError:
 * @ELGATO_STREAM_DECK_ERROR_UNRECOGNIZED: not a recognized Stream Deck device
 *
 * Errors that #ElgatoStreamDeckError can generate.
 */
typedef enum
{
  ELGATO_STREAM_DECK_ERROR_UNRECOGNIZED,
} ElgatoStreamDeckError;

#define ELGATO_STREAM_DECK_ERROR (elgato_stream_deck_error_quark ())
GQuark  elgato_stream_deck_error_quark (void);

#define ELGATO_TYPE_STREAM_DECK (elgato_stream_deck_get_type())
G_DECLARE_DERIVABLE_TYPE (ElgatoStreamDeck, elgato_stream_deck, ELGATO, STREAM_DECK, BsDevice)

struct _ElgatoStreamDeckClass
{
  BsDeviceClass parent_class;

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
};

BsDevice * elgato_stream_deck_new (GUsbDevice  *gusb_device,
                                   GError     **out_error);

GUsbDevice * elgato_stream_deck_get_gusb_device (ElgatoStreamDeck *self);

G_END_DECLS
