/*
 * razer-stream-controller-x.c
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

#define G_LOG_DOMAIN "Razer Stream Controller X"

#include "razer-stream-controller-x.h"

#include <glib/gi18n.h>

#include "bs-actionable.h"
#include "bs-button-grid.h"
#include "bs-button-private.h"
#include "bs-events-private.h"
#include "bs-debug.h"
#include "bs-device-layout-builder-private.h"
#include "bs-device-private.h"
#include "bs-renderer-private.h"

struct _RazerStreamControllerX
{
  LoupedeckDevice parent_instance;
};

G_DEFINE_FINAL_TYPE (RazerStreamControllerX, razer_stream_controller_x, LOUPEDECK_TYPE_DEVICE)

#define BUTTON_WIDTH 72
#define BUTTON_HEIGHT BUTTON_WIDTH


/*
 * LoupedeckDevice overrides
 */

static void
razer_stream_controller_x_button_state_changed (LoupedeckDevice *loupedeck_device,
                                                uint8_t          id,
                                                BsEventType      event_type)
{
  BsDevice *device = BS_DEVICE (loupedeck_device);
  BsDeviceRegion *button_grid;
  g_autoptr (BsButton) button = NULL;
  g_autoptr (BsEvent) button_event = NULL;

  button_grid = bs_device_get_region (device, "main-button-grid");
  button = g_list_model_get_item (bs_button_grid_get_buttons (BS_BUTTON_GRID (button_grid)),
                                  id - 0x1b); /* Button 0 has id 0x1b */

  g_assert (BS_IS_BUTTON (button));

  button_event = bs_button_event_new (event_type, device, button);
  bs_actionable_handle_event (BS_ACTIONABLE (button), button_event);
}

static void
razer_stream_controller_x_apply_button_update (LoupedeckDevice *loupedeck_device,
                                               BsButtonUpdate  *update)
{
  int x_offset = 9;
  int y_offset = 2;
  int column;
  BsDeviceRegion *region;
  BsRenderer *renderer;
  g_autoptr (GdkTexture) texture = NULL;
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (GByteArray) buffer = NULL;
  g_autoptr (GError) error = NULL;

  BS_TRACE_MSG ("Applying button update %p", update);

  /* Button indexes in the first row are less than 5, so they skip the loop.
   * Button indexes in the second row are less than 10, so they pass the loop once.
   * Button indexes in the third row are less than 15, so they pass the loop twice.
   * What remains of the used variable can be used as column index. */
  for (column = bs_button_get_position (update->button); column >= 5; column -= 5)
    y_offset += (BUTTON_HEIGHT + 25); /* Previous row button + spacing between rows */

  x_offset += (BUTTON_WIDTH * column); /* Previous columns buttons */

  /* A spacing of 26 is applied between a column with an odd index and
   * a column index with an even index.
   * A spacing of 25 is applied to the opposite. */
  for (; column > 0; column--)
    x_offset += (column % 2) ? 25 : 26;

  region = bs_button_get_region (update->button);
  renderer = bs_device_region_get_renderer (region);

  texture = bs_renderer_compose_icon (renderer,
                                      bs_button_get_icon (update->button),
                                      &error);
  if (error)
    {
      g_warning ("Error compositing button texture: %s", error->message);
      return;
    }

  bytes = bs_renderer_convert_texture (renderer, texture, &error);
  if (error)
    {
      g_warning ("Error converting button texture: %s", error->message);
      return;
    }

  buffer = g_bytes_unref_to_array (g_steal_pointer (&bytes));

  loupedeck_device_send_framebuffer (loupedeck_device,
                                     0x38,
                                     x_offset,
                                     y_offset,
                                     BUTTON_WIDTH,
                                     BUTTON_HEIGHT,
                                     buffer);
}


/*
 * BsDevice overrides
 */

static GListModel *
razer_stream_controller_x_create_layout (BsDevice *device)
{
  g_autoptr (BsDeviceLayoutBuilder) layout_builder = NULL;

  g_assert (RAZER_IS_STREAM_CONTROLLER_X (device));

  layout_builder = bs_device_layout_builder_new (device);

  bs_device_layout_builder_add_button_grid (layout_builder,
                                            "main-button-grid",
                                            &(BsImageInfo) {
                                            .width = BUTTON_WIDTH,
                                            .height = BUTTON_HEIGHT,
                                            .format = BS_IMAGE_FORMAT_R8G8B8,
                                            .flags = BS_RENDERER_FLAG_NONE,
                                            },
                                            15, /* n_buttons */
                                            5, /* columns */
                                            0, 0, 1, 1);

  return bs_device_layout_builder_build (layout_builder);
}

static const char *
razer_stream_controller_x_get_name (BsDevice *device)
{
  /* Translators: this is a product name. In most cases, it is not translated.
   * Please verify if Razer translates their product names on your locale.
   */
  return _("Razer Stream Controller X");
}


/*
 * GObject overrides
 */

static void
razer_stream_controller_x_class_init (RazerStreamControllerXClass *klass)
{
  BsDeviceClass *device_class = BS_DEVICE_CLASS (klass);
  LoupedeckDeviceClass *loupedeck_device_class = LOUPEDECK_DEVICE_CLASS (klass);

  device_class->create_layout = razer_stream_controller_x_create_layout;
  device_class->get_name = razer_stream_controller_x_get_name;

  loupedeck_device_class->button_state_changed =
    razer_stream_controller_x_button_state_changed;
  loupedeck_device_class->apply_button_update =
    razer_stream_controller_x_apply_button_update;
}

static void
razer_stream_controller_x_init (RazerStreamControllerX *self)
{
}
