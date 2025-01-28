/* bs-stream-deck.c
 *
 * Copyright 2022 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#define G_LOG_DOMAIN "Stream Deck"

#include "bs-stream-deck-private.h"

#include "bs-actionable-private.h"
#include "bs-action.h"
#include "bs-button-grid.h"
#include "bs-button-private.h"
#include "bs-debug.h"
#include "bs-device-region.h"
#include "bs-dial-private.h"
#include "bs-dial-grid.h"
#include "bs-events-private.h"
#include "bs-icon.h"
#include "bs-page-private.h"
#include "bs-profile.h"
#include "bs-renderer.h"
#include "bs-touchscreen-private.h"
#include "bs-touchscreen-slot.h"

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
  BS_STREAM_DECK_FEATURE_BUTTONS = 1 << 0,
  BS_STREAM_DECK_FEATURE_TOUCHSCREEN = 1 << 1,
  BS_STREAM_DECK_FEATURE_DIALS = 1 << 2,
} BsStreamDeckFeatureFlags;

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
  uint8_t product_id;
  const char *name;
  const char *icon_name;
  BsStreamDeckFeatureFlags features;
  BsButtonLayout button_layout;
  BsDialLayout dial_layout;
  BsTouchscreenLayout touchscreen_layout;

  void (*reset) (BsStreamDeck *self);
  void (*set_brightness) (BsStreamDeck *self,
                          double        brightness);

  char * (*get_serial_number) (BsStreamDeck *self);
  char * (*get_firmware_version) (BsStreamDeck *self);
  gboolean (*set_button_texture) (BsStreamDeck  *self,
                                  BsButton      *button,
                                  GdkTexture    *texture,
                                  GError       **error);
  gboolean (*set_touchscreen_texture) (BsStreamDeck   *self,
                                       BsTouchscreen  *touchscreen,
                                       GdkTexture     *texture,
                                       GError        **error);
  gboolean (*read_state) (BsStreamDeck *self);
} StreamDeckModelInfo;

typedef struct
{
  GSource source;
  BsStreamDeck *stream_deck;
} StreamDeckSource;

struct _BsStreamDeck
{
  GObject parent_instance;

  GListStore *profiles;
  GListStore *regions;
  BsProfile *active_profile;
  GQueue *active_pages;
  guint save_timeout_id;

  const StreamDeckModelInfo *model_info;
  GUsbDevice *device;
  hid_device *handle;

  double brightness;
  char *serial_number;
  char *firmware_version;
  GIcon *icon;
  GSource *poll_source;
  gboolean initialized;
  gboolean loaded;
  gboolean fake;
};

static void g_initable_iface_init (GInitableIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (BsStreamDeck, bs_stream_deck, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, g_initable_iface_init))

G_DEFINE_QUARK (BsStreamDeck, bs_stream_deck_error);

enum
{
  PROP_0,
  PROP_ACTIVE_PAGE,
  PROP_ACTIVE_PROFILE,
  PROP_BRIGHTNESS,
  PROP_DEVICE,
  PROP_FAKE,
  PROP_ICON,
  PROP_NAME,
  PROP_SERIAL_NUMBER,
  N_PROPS,
};

static GParamSpec *properties[N_PROPS];


/*
 * Auxiliary methods
 */

static char *
get_profile_path (BsStreamDeck *self)
{
  g_autofree char *profile_filename = NULL;

  profile_filename = g_strdup_printf ("%s.json", self->serial_number);

  return g_build_filename (g_get_user_data_dir (),
                           profile_filename,
                           NULL);
}

static BsButton *
find_button_at_region (BsStreamDeck *self,
                       const char   *region_id,
                       size_t        button_index)
{
  g_autoptr (BsButton) button = NULL;
  BsButtonGrid *button_grid = NULL;

  button_grid = BS_BUTTON_GRID (bs_stream_deck_get_region (self, region_id));
  button = g_list_model_get_item (bs_button_grid_get_buttons (button_grid), button_index);
  g_assert (BS_IS_BUTTON (button));

  return button;
}

static void
update_page_items (BsStreamDeck *self,
                   BsPage       *page)
{
  for (size_t i = 0; i < self->model_info->button_layout.n_buttons; i++)
    {
      BsButton *button = find_button_at_region (self, "main-button-grid", i);

      bs_page_update_item (page,
                           "main-button-grid",
                           i,
                           bs_actionable_get_action (BS_ACTIONABLE (button)),
                           bs_button_get_custom_icon (button));
    }

  if (self->model_info->features & BS_STREAM_DECK_FEATURE_TOUCHSCREEN)
    {
      g_autoptr (JsonNode) region_data = NULL;
      BsTouchscreen *touchscreen;
      GListModel *touchscreen_slots;

      touchscreen = BS_TOUCHSCREEN (bs_stream_deck_get_region (self, "touchscreen"));
      touchscreen_slots = bs_touchscreen_get_slots (touchscreen);

      region_data = bs_device_region_serialize (BS_DEVICE_REGION (touchscreen));
      bs_page_set_region_data (page, "touchscreen", region_data);

      for (size_t i = 0; i < g_list_model_get_n_items (touchscreen_slots); i++)
        {
          g_autoptr (BsTouchscreenSlot) slot = g_list_model_get_item (touchscreen_slots, i);

          bs_page_update_item (page,
                               "touchscreen",
                               i,
                               bs_actionable_get_action (BS_ACTIONABLE (slot)),
                               NULL);
        }
    }
}

static void
update_pages (BsStreamDeck *self)
{
  BsPage *active_page;

  BS_ENTRY;

  active_page = bs_stream_deck_get_active_page (self);
  update_page_items (self, active_page);

  for (GList *l = g_queue_peek_head_link (self->active_pages); l; l = l->next)
    bs_page_update_all_items (l->data);

  BS_EXIT;
}

static void
save_profiles (BsStreamDeck *self)
{
  g_autoptr (JsonGenerator) generator = NULL;
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) root = NULL;
  g_autoptr (GError) error = NULL;
  g_autofree char *profile_path = NULL;
  g_autofree char *json_str = NULL;

  BS_ENTRY;

  if (self->fake)
    BS_RETURN ();

  /* Update the active profile */
  bs_profile_set_brightness (self->active_profile, self->brightness);
  update_pages (self);

  builder = json_builder_new ();

  json_builder_begin_object (builder);

  json_builder_set_member_name (builder, "active-profile");
  json_builder_add_string_value (builder, bs_profile_get_id (self->active_profile));

  json_builder_set_member_name (builder, "profiles");
  json_builder_begin_array (builder);
  for (size_t i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->profiles)); i++)
    {
      g_autoptr (BsProfile) profile = NULL;

      profile = g_list_model_get_item (G_LIST_MODEL (self->profiles), i);
      json_builder_add_value (builder, bs_profile_to_json (profile));
    }
  json_builder_end_array (builder);

  json_builder_end_object (builder);

  root = json_builder_get_root (builder);

  generator = json_generator_new ();
  json_generator_set_pretty (generator, TRUE);
  json_generator_set_root (generator, root);
  json_str = json_generator_to_data (generator, NULL);

  profile_path = get_profile_path (self);
  g_file_set_contents (profile_path, json_str, -1, &error);

  if (error)
    g_warning ("Error saving profiles: %s", error->message);

  BS_EXIT;
}

static void
maybe_create_backup (BsStreamDeck *self,
                     uint32_t      version)
{
  g_autoptr (GError) error = NULL;
  g_autofree char *backup_file_path = NULL;
  g_autofree char *backup_file_name = NULL;
  g_autofree char *backup_folder = NULL;
  g_autofree char *profile_path = NULL;
  g_autofree char *data = NULL;
  size_t length;

  BS_ENTRY;

  g_assert (BS_IS_STREAM_DECK (self));
  g_assert (version > 0);

  backup_folder = g_build_filename (g_get_user_data_dir (),
                                    "backups",
                                    NULL);

  g_mkdir_with_parents (backup_folder, 0755);

  backup_file_name = g_strdup_printf ("%s.bak.v%u", self->serial_number, version - 1);
  backup_file_path = g_build_filename (backup_folder,
                                       backup_file_name,
                                       NULL);

  if (g_file_test (backup_file_path, G_FILE_TEST_EXISTS))
    {
      g_debug ("Backup file already exists, skipping");
      return;
    }

  profile_path = get_profile_path (self);
  if (!g_file_get_contents (profile_path, &data, &length, &error))
    {
      g_debug ("Cannot read %s: %s", profile_path, error->message);
      return;
    }

  if (!g_file_set_contents (backup_file_path, data, length, &error))
    {
      g_error ("Cannot write to %s", backup_file_path);
      exit (EXIT_FAILURE);
    }

  BS_EXIT;
}

static void
load_profiles (BsStreamDeck  *self)
{
  g_autoptr (JsonParser) parser = NULL;
  g_autoptr (BsProfile) active_profile = NULL;
  g_autoptr (GError) local_error = NULL;
  g_autofree char *profile_path = NULL;
  const char *active_profile_id;
  JsonObject *object;
  JsonArray *profiles_array;
  JsonNode *root;

  BS_ENTRY;

  profile_path = get_profile_path (self);

  maybe_create_backup (self, 1);

  g_debug ("Loading %s", profile_path);

  parser = json_parser_new ();

  json_parser_load_from_file (parser, profile_path, &local_error);
  if (local_error)
    {
      g_debug ("Error loading profile for device %s: %s",
               self->serial_number,
               local_error->message);
      BS_GOTO (out);
    }

  root = json_parser_get_root (parser);
  object = json_node_get_object (root);

  active_profile = NULL;
  active_profile_id = json_object_get_string_member (object, "active-profile");

  profiles_array = json_object_get_array_member (object, "profiles");
  for (size_t i = 0; i < json_array_get_length (profiles_array); i++)
    {
      g_autoptr (BsProfile) profile = NULL;
      JsonNode *profile_node;

      profile_node = json_array_get_element (profiles_array, i);

      if (!profile_node)
        continue;

      profile = bs_profile_new_from_json (self, profile_node);
      g_list_store_append (self->profiles, profile);

      if (g_strcmp0 (active_profile_id, bs_profile_get_id (profile)) == 0)
        active_profile = g_object_ref (profile);
    }

  if (!active_profile)
    active_profile = g_list_model_get_item (G_LIST_MODEL (self->profiles), 0);

out:
  if (!active_profile)
    {
      active_profile = bs_profile_new_empty (self);
      bs_profile_set_name (active_profile, _("Default"));
      g_list_store_append (self->profiles, active_profile);
    }

  bs_stream_deck_load_profile (self, active_profile);

  BS_EXIT;
}

static void
load_active_page (BsStreamDeck *self)
{
  BsPage *active_page;

  BS_ENTRY;

  active_page = bs_stream_deck_get_active_page (self);

  g_assert (self->model_info->features & BS_STREAM_DECK_FEATURE_BUTTONS);

  for (uint8_t i = 0; i < self->model_info->button_layout.n_buttons; i++)
    {
      g_autoptr (BsAction) action = NULL;
      g_autoptr (BsIcon) custom_icon = NULL;
      g_autoptr (GError) error = NULL;
      BsButton *button;

      button = find_button_at_region (self, "main-button-grid", i);

      bs_page_realize (active_page, "main-button-grid", i, &custom_icon, &action, &error);

      if (error)
        {
          g_warning ("Failed to construct action and icon from page: %s", error->message);
          continue;
        }

      bs_button_inhibit_page_updates (button);

      bs_actionable_set_action (BS_ACTIONABLE (button), action);
      bs_button_set_custom_icon (button, custom_icon);

      bs_button_uninhibit_page_updates (button);
    }

  if (self->model_info->features & BS_STREAM_DECK_FEATURE_TOUCHSCREEN)
    {
      BsTouchscreen *touchscreen;
      GListModel *touchscreen_slots;
      JsonNode *region_data;

      touchscreen = BS_TOUCHSCREEN (bs_stream_deck_get_region (self, "touchscreen"));
      touchscreen_slots = bs_touchscreen_get_slots (touchscreen);

      region_data = bs_page_get_region_data (active_page, "touchscreen");
      bs_device_region_deserialize (BS_DEVICE_REGION (touchscreen), region_data);

      for (size_t i = 0; i < g_list_model_get_n_items (touchscreen_slots); i++)
        {
          g_autoptr (BsTouchscreenSlot) slot = NULL;
          g_autoptr (BsAction) action = NULL;
          g_autoptr (BsIcon) custom_icon = NULL;
          g_autoptr (GError) error = NULL;

          bs_page_realize (active_page, "touchscreen", i, &custom_icon, &action, &error);

          if (error)
            {
              g_warning ("Failed to construct action and icon from page: %s", error->message);
              continue;
            }

          slot = g_list_model_get_item (touchscreen_slots, i);
          g_assert (BS_IS_TOUCHSCREEN_SLOT (slot));

          bs_actionable_set_action (BS_ACTIONABLE (slot), action);
          //TODO: bs_button_set_custom_icon (slot, custom_icon);
        }
    }

  BS_EXIT;
}

static inline uint8_t
swap_button_index_original (BsStreamDeck *self,
                            uint8_t       button_index)
{
  int column = button_index % self->model_info->button_layout.columns;
  int actual_index = ((int) button_index - column) + ((int) self->model_info->button_layout.columns - 1 - column);
  return (uint8_t) actual_index;
}


/*
 * Callbacks
 */

static gboolean
save_after_timeout_cb (gpointer data)
{
  BsStreamDeck *self = BS_STREAM_DECK (data);

  BS_ENTRY;

  save_profiles (self);

  self->save_timeout_id = 0;
  BS_RETURN (G_SOURCE_REMOVE);
}


/*
 * Device-specific implementations
 */

/* Mini & Original (gen 1) */

static gboolean
set_button_texture_mini (BsStreamDeck  *self,
                         BsButton      *button,
                         GdkTexture    *texture,
                         GError       **error)
{
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

      hid_write (self->handle, payload, package_size);

      bytes_remaining -= chunk_size;
      page++;
    }

  BS_RETURN (TRUE);
}

static gboolean
read_state_mini (BsStreamDeck *self)
{
  const BsButtonLayout *layout;
  uint8_t *states;
  size_t states_length;
  int result;

  layout = &self->model_info->button_layout;
  states_length = layout->n_buttons + 1;

  g_assert (states_length < 8192);
  states = g_alloca (sizeof (uint8_t) * states_length);

  result = hid_read (self->handle, states, states_length);

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

      bs_button_set_pressed (button, (gboolean) states[i + 1]);

      event_type = states[i + 1] ? BS_BUTTON_PRESS : BS_BUTTON_RELEASE;
      button_event = bs_button_event_new (event_type, self, button);
      bs_actionable_handle_event (BS_ACTIONABLE (button), button_event);
    }

  return TRUE;
}

static void
reset_mini_original (BsStreamDeck *self)
{
  const uint8_t reset_command[] = {
    0x0b,
    0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  BS_ENTRY;

  hid_send_feature_report (self->handle, reset_command, sizeof (reset_command));

  BS_EXIT;
}

static char *
get_serial_number_mini_original (BsStreamDeck *self)
{
  g_autofree char *serial = NULL;
  uint8_t data[17];

  BS_ENTRY;

  data[0] = 0x03;

  hid_get_feature_report (self->handle, data, sizeof (data));

  serial = g_malloc0 (sizeof (char) * 13);
  memcpy (serial, &data[5], 12);

  BS_RETURN (g_steal_pointer (&serial));
}

static char *
get_firmware_version_mini_original (BsStreamDeck *self)
{
  g_autofree char *firmware_version = NULL;
  uint8_t data[17];

  BS_ENTRY;

  data[0] = 0x04;

  hid_get_feature_report (self->handle, data, sizeof (data));

  firmware_version = g_malloc0 (sizeof (char) * 13);
  memcpy (firmware_version, &data[5], 12);

  BS_RETURN (g_steal_pointer (&firmware_version));
}

static void
set_brightness_mini_original (BsStreamDeck *self,
                              gdouble       brightness)
{
  const uint8_t b = CLAMP (brightness * 100, 0, 100);
  const uint8_t data[] = {
    0x05,
    0x55, 0xaa, 0xd1, 0x01, b   , 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  BS_ENTRY;

  hid_send_feature_report (self->handle, data, sizeof (data));

  BS_EXIT;
}

static gboolean
set_button_texture_original (BsStreamDeck  *self,
                             BsButton      *button,
                             GdkTexture    *texture,
                             GError       **error)
{
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

      hid_write (self->handle, payload, package_size);

      bytes_remaining -= chunk_size;
      page++;
    }

  BS_RETURN (TRUE);
}

static gboolean
read_state_original (BsStreamDeck *self)
{
  const BsButtonLayout *layout;
  uint8_t *states;
  size_t states_length;
  int result;

  layout = &self->model_info->button_layout;
  states_length = layout->n_buttons + 1;

  g_assert (states_length < 8192);
  states = g_alloca (sizeof (uint8_t) * states_length);

  result = hid_read (self->handle, states, states_length);

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

      bs_button_set_pressed (button, (gboolean) states[i + 1]);

      event_type = states[i + 1] ? BS_BUTTON_PRESS : BS_BUTTON_RELEASE;
      button_event = bs_button_event_new (event_type, self, button);
      bs_actionable_handle_event (BS_ACTIONABLE (button), button_event);
    }

  return TRUE;
}

/* 2nd generation */

static void
reset_gen2 (BsStreamDeck *self)
{
  const uint8_t reset_command[] = {
      0x03,
      0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  BS_ENTRY;

  hid_send_feature_report (self->handle, reset_command, sizeof (reset_command));

  BS_EXIT;
}

static char *
get_serial_number_gen2 (BsStreamDeck *self)
{
  uint8_t data[32];
  char *serial;

  BS_ENTRY;

  data[0] = 0x06;

  hid_get_feature_report (self->handle, data, sizeof (data));

  serial = g_malloc0 (sizeof (char) * 31);
  memcpy (serial, &data[2], 30);

  BS_RETURN (serial);
}

static char *
get_firmware_version_gen2 (BsStreamDeck *self)
{
  uint8_t data[32];
  char *serial;

  BS_ENTRY;

  data[0] = 0x05;

  hid_get_feature_report (self->handle, data, sizeof (data));

  serial = g_malloc0 (sizeof (char) * 27);
  memcpy (serial, &data[6], 26);

  BS_RETURN (serial);
}

static void
set_brightness_gen2 (BsStreamDeck *self,
                     gdouble       brightness)
{
  const uint8_t b = CLAMP (brightness * 100, 0, 100);
  const uint8_t data[] = {
    0x03,
    0x08, b   , 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  BS_ENTRY;

  hid_send_feature_report (self->handle, data, sizeof (data));

  BS_EXIT;
}

static gboolean
set_button_texture_gen2 (BsStreamDeck  *self,
                         BsButton      *button,
                         GdkTexture    *texture,
                         GError       **error)
{
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

      hid_write (self->handle, payload, package_size * sizeof (uint8_t));

      bytes_remaining -= chunk_size;
      page++;
    }

  BS_RETURN (TRUE);
}

static gboolean
read_state_gen2 (BsStreamDeck *self)
{
  const BsButtonLayout *layout;
  uint8_t *states;
  size_t states_length;
  int result;

  layout = &self->model_info->button_layout;
  states_length = layout->n_buttons + 4;

  g_assert (states_length < 8192);
  states = g_alloca (sizeof (uint8_t) * states_length);

  result = hid_read (self->handle, states, states_length);

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

      bs_button_set_pressed (button, (gboolean) states[i + 4]);

      event_type = states[i + 4] ? BS_BUTTON_PRESS : BS_BUTTON_RELEASE;
      button_event = bs_button_event_new (event_type, self, button);
      bs_actionable_handle_event (BS_ACTIONABLE (button), button_event);
    }

  return TRUE;
}

/* noops for devices without visual feedback */

static void
set_brightness_pedal (BsStreamDeck *self,
                      double        brightness)
{
  BS_ENTRY;
  BS_EXIT;
}

static gboolean
set_button_texture_pedal (BsStreamDeck  *self,
                          BsButton      *button,
                          GdkTexture    *texture,
                          GError       **error)
{
  BS_ENTRY;
  BS_RETURN (TRUE);
}

static void
reset_pedal (BsStreamDeck *self)
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
read_state_plus (BsStreamDeck *self)
{
  const BsButtonLayout *layout;
  const size_t states_length = 14;
  uint8_t *states;
  int result;

  layout = &self->model_info->button_layout;

  g_assert (states_length < 8192);
  states = g_alloca (sizeof (uint8_t) * states_length);

  result = hid_read (self->handle, states, states_length);

  if (result == 0)
    return TRUE;

  enum {
    BUTTON_EVENT = 0x00,
    TOUCHSCREEN_EVENT = 0x02,
    DIAL_EVENT = 0x03,
  } event_type = states[1];

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

          bs_button_set_pressed (button, (gboolean) states[i + 4]);

          event_type = states[i + 4] ? BS_BUTTON_PRESS : BS_BUTTON_RELEASE;
          button_event = bs_button_event_new (event_type, self, button);
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

        touchscreen = BS_TOUCHSCREEN (bs_stream_deck_get_region (self, "touchscreen"));

        graphene_point_init (&position,
                             (states[7] << 8) + states[6],
                             (states[9] << 8) + states[8]);

        g_debug ("Touchscreen event (%.0fx%.0f)", position.x, position.y);

        touchscreen_slot = bs_touchscreen_pick_slot (touchscreen, &position);

        switch (touch_event_type)
          {
          case SHORT_PRESS:
            touchscreen_event = bs_touchscreen_event_new (BS_TOUCHSCREEN_SHORT_PRESS,
                                                          self,
                                                          touchscreen_slot,
                                                          &position,
                                                          &position);
            break;

          case LONG_PRESS:
            touchscreen_event = bs_touchscreen_event_new (BS_TOUCHSCREEN_LONG_PRESS,
                                                          self,
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
                                                            self,
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

        dial_grid = BS_DIAL_GRID (bs_stream_deck_get_region (self, "dial-grid"));
        dials = bs_dial_grid_get_dials (dial_grid);
        g_assert (g_list_model_get_n_items (dials) == 4);

        touchscreen = BS_TOUCHSCREEN (bs_stream_deck_get_region (self, "touchscreen"));
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
            dial_event = bs_dial_event_new (event_type, self, dial, rotation);

            slot = g_list_model_get_item (touchscreen_slots, i);
            bs_actionable_handle_event (BS_ACTIONABLE (slot), dial_event);
          }
      }
      break;
    }

  return TRUE;
}

static gboolean
set_touchscreen_texture_plus (BsStreamDeck   *self,
                              BsTouchscreen  *touchscreen,
                              GdkTexture     *texture,
                              GError        **error)
{
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

      hid_write (self->handle, payload, package_size * sizeof (uint8_t));

      bytes_remaining -= chunk_size;
      page++;
    }

  BS_RETURN (TRUE);
}

static const StreamDeckModelInfo models_vtable[] = {
  {
    .product_id = STREAMDECK_MINI_PRODUCT_ID,
    /* Translators: this is a product name. In most cases, it is not translated.
     * Please verify if Elgato translates their product names on your locale.
     */
    .name = N_("Stream Deck Mini"),
    .icon_name = "input-dialpad-symbolic",
    .features = BS_STREAM_DECK_FEATURE_BUTTONS,
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
    .icon_name = "input-dialpad-symbolic",
    .features = BS_STREAM_DECK_FEATURE_BUTTONS,
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
    .icon_name = "input-dialpad-symbolic",
    .features = BS_STREAM_DECK_FEATURE_BUTTONS,
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
    .icon_name = "input-dialpad-symbolic",
    .features = BS_STREAM_DECK_FEATURE_BUTTONS,
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
    .icon_name = "input-dialpad-symbolic",
    .features = BS_STREAM_DECK_FEATURE_BUTTONS,
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
    .icon_name = "input-dialpad-symbolic",
    .features = BS_STREAM_DECK_FEATURE_BUTTONS,
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
    .icon_name = "input-dialpad-symbolic",
    .features = BS_STREAM_DECK_FEATURE_BUTTONS,
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
    .icon_name = "input-dialpad-symbolic",
    .features = BS_STREAM_DECK_FEATURE_BUTTONS,
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
    .name = N_("Stream Deck +"),
    .icon_name = "input-dialpad-symbolic",
    .features = BS_STREAM_DECK_FEATURE_BUTTONS |
                BS_STREAM_DECK_FEATURE_TOUCHSCREEN |
                BS_STREAM_DECK_FEATURE_DIALS,
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
    .icon_name = "input-dialpad-symbolic",
    .features = BS_STREAM_DECK_FEATURE_BUTTONS,
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
 * Fake device for testing
 */

static void
reset_fake (BsStreamDeck *self)
{
}

static char *
get_serial_number_fake (BsStreamDeck *self)
{
  static unsigned int counter = 0;
  return g_strdup_printf ("feaneron-hangar-xl-serial-%u", counter++);
}

static char *
get_firmware_version_fake (BsStreamDeck *self)
{
  return g_strdup ("feaneron-hangar-xl-firmware-version");
}

static void
set_brightness_fake (BsStreamDeck *self,
                     double        brightness)
{
}

static gboolean
set_button_texture_fake (BsStreamDeck  *self,
                         BsButton      *button,
                         GdkTexture    *texture,
                         GError       **error)
{
  return TRUE;
}

static gboolean
read_state_fake (BsStreamDeck *self)
{
  return TRUE;
}

static const StreamDeckModelInfo fake_models_vtable[] = {
  {
    .product_id = 0x0001,
    .name = N_("Feaneron Hangar Original"),
    .icon_name = "input-dialpad-symbolic",
    .features = BS_STREAM_DECK_FEATURE_BUTTONS,
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
    .reset = reset_fake,
    .get_serial_number = get_serial_number_fake,
    .get_firmware_version = get_firmware_version_fake,
    .set_brightness = set_brightness_fake,
    .set_button_texture = set_button_texture_fake,
    .read_state = read_state_fake,
  },
  {
    .product_id = 0x0001,
    .name = N_("Feaneron Hangar XL"),
    .icon_name = "input-dialpad-symbolic",
    .features = BS_STREAM_DECK_FEATURE_BUTTONS,
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
    .reset = reset_fake,
    .get_serial_number = get_serial_number_fake,
    .get_firmware_version = get_firmware_version_fake,
    .set_brightness = set_brightness_fake,
    .set_button_texture = set_button_texture_fake,
    .read_state = read_state_fake,
  },
};

/*
 * GSource
 */

static gboolean
stream_deck_source_dispatch (GSource     *source,
                             GSourceFunc  callback,
                             gpointer     user_data)
{
  StreamDeckSource *stream_deck_source = (StreamDeckSource *)source;
  BsStreamDeck *self = stream_deck_source->stream_deck;
  gint64 current_time;
  gint64 expiration;

  self->model_info->read_state (self);

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
stream_deck_source_new (BsStreamDeck *self)
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

static gboolean
bs_stream_deck_initable_init (GInitable     *initable,
                              GCancellable  *cancellable,
                              GError       **error)
{
  BsStreamDeck *self = BS_STREAM_DECK (initable);
  unsigned int row = 0;

  BS_ENTRY;

  /* Short-circuit fake devices here */
  if (self->fake)
    {
      static int fake_index = 0;
      self->model_info = &fake_models_vtable[fake_index++ % G_N_ELEMENTS (fake_models_vtable)];
      BS_GOTO (out);
    }

  if (!self->device)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "No device");
      BS_RETURN (FALSE);
    }

  if (g_usb_device_get_vid (self->device) != ELGATO_SYSTEMS_VENDOR_ID)
    {
      g_set_error (error,
                   BS_STREAM_DECK_ERROR,
                   BS_STREAM_DECK_ERROR_UNRECOGNIZED,
                   "Not an Elgato device");
      BS_RETURN (FALSE);
    }

  for (size_t i = 0; i < G_N_ELEMENTS (models_vtable); i++)
    {
      if (g_usb_device_get_pid (self->device) == models_vtable[i].product_id)
        {
          self->model_info = &models_vtable[i];
          break;
        }
    }

  if (!self->model_info)
    {
      g_set_error (error,
                   BS_STREAM_DECK_ERROR,
                   BS_STREAM_DECK_ERROR_UNRECOGNIZED,
                   "Not a recognized Stream Deck device");
      BS_RETURN (FALSE);
    }

  self->handle = hid_open (g_usb_device_get_vid (self->device),
                           g_usb_device_get_pid (self->device),
                           NULL);

  if (!self->handle)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to open Stream Deck device");
      BS_RETURN (FALSE);
    }

  hid_set_nonblocking (self->handle, TRUE);

  self->poll_source = stream_deck_source_new (self);

out:
  self->serial_number = self->model_info->get_serial_number (self);
  self->firmware_version = self->model_info->get_firmware_version (self);
  self->icon = g_themed_icon_new (self->model_info->icon_name);

  /* All Elgato Stream Decks have one button grid */
  g_assert (self->model_info->features & BS_STREAM_DECK_FEATURE_BUTTONS);

  if (self->model_info->features & BS_STREAM_DECK_FEATURE_BUTTONS)
    {
      g_autoptr (BsButtonGrid) button_grid = NULL;

      button_grid = bs_button_grid_new ("main-button-grid",
                                        self,
                                        &self->model_info->button_layout.image_info,
                                        self->model_info->button_layout.n_buttons,
                                        self->model_info->button_layout.columns,
                                        0, row++, 1, 1);

      g_list_store_append (self->regions, button_grid);
    }

  if (self->model_info->features & BS_STREAM_DECK_FEATURE_TOUCHSCREEN)
    {
      g_autoptr (BsTouchscreen) touchscreen = NULL;

      touchscreen = bs_touchscreen_new ("touchscreen",
                                        self,
                                        &self->model_info->touchscreen_layout.image_info,
                                        self->model_info->touchscreen_layout.n_slots,
                                        0, row++, 1, 1);

      g_list_store_append (self->regions, touchscreen);
    }

  if (self->model_info->features & BS_STREAM_DECK_FEATURE_DIALS)
    {
      g_autoptr (BsDialGrid) dial_grid = NULL;

      dial_grid = bs_dial_grid_new ("dial-grid",
                                    self,
                                    self->model_info->dial_layout.n_dials,
                                    self->model_info->dial_layout.columns,
                                    0, row++, 1, 1);

      g_list_store_append (self->regions, dial_grid);
    }

  self->initialized = TRUE;

  BS_RETURN (TRUE);
}

static void
g_initable_iface_init (GInitableIface *iface)
{
  iface->init = bs_stream_deck_initable_init;
}

/*
 * GObject overrides
 */

static void
bs_stream_deck_finalize (GObject *object)
{
  BsStreamDeck *self = (BsStreamDeck *)object;

  BS_ENTRY;

  if (self->initialized)
    {
      save_profiles (self);
      bs_stream_deck_reset (self);
    }

  if (self->device)
    g_usb_device_close (self->device, NULL);

  if (self->poll_source)
    g_source_destroy (self->poll_source);
  g_clear_pointer (&self->poll_source, g_source_unref);

  g_clear_handle_id (&self->save_timeout_id, g_source_remove);
  g_clear_pointer (&self->serial_number, g_free);
  g_clear_pointer (&self->handle, hid_close);
  g_queue_free_full (self->active_pages, g_object_unref);
  g_clear_object (&self->regions);
  g_clear_object (&self->device);
  g_clear_object (&self->profiles);

  G_OBJECT_CLASS (bs_stream_deck_parent_class)->finalize (object);

  BS_EXIT;
}

static void
bs_stream_deck_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  BsStreamDeck *self = BS_STREAM_DECK (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_PAGE:
      g_value_set_object (value, bs_stream_deck_get_active_page (self));
      break;

    case PROP_ACTIVE_PROFILE:
      g_value_set_object (value, self->active_profile);
      break;

    case PROP_BRIGHTNESS:
      g_value_set_double (value, self->brightness);
      break;

    case PROP_DEVICE:
      g_value_set_object (value, self->device);
      break;

    case PROP_ICON:
      g_value_set_object (value, self->icon);
      break;

    case PROP_NAME:
      g_value_set_string (value, bs_stream_deck_get_name (self));
      break;

    case PROP_SERIAL_NUMBER:
      g_value_set_string (value, bs_stream_deck_get_serial_number (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_stream_deck_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  BsStreamDeck *self = BS_STREAM_DECK (object);

  switch (prop_id)
    {
    case PROP_BRIGHTNESS:
      bs_stream_deck_set_brightness (self, g_value_get_double (value));
      break;

    case PROP_DEVICE:
      g_assert (self->device == NULL);
      self->device = g_value_dup_object (value);
      break;

    case PROP_FAKE:
      self->fake = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_stream_deck_class_init (BsStreamDeckClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = bs_stream_deck_finalize;
  object_class->get_property = bs_stream_deck_get_property;
  object_class->set_property = bs_stream_deck_set_property;

  properties[PROP_ACTIVE_PAGE] = g_param_spec_object ("active-page", NULL, NULL,
                                                      BS_TYPE_PAGE,
                                                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_ACTIVE_PROFILE] = g_param_spec_object ("active-profile", NULL, NULL,
                                                         BS_TYPE_PROFILE,
                                                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_BRIGHTNESS] = g_param_spec_double ("brightness", NULL, NULL,
                                                     0.0, 1.0, 0.5,
                                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_DEVICE] = g_param_spec_object ("device", NULL, NULL,
                                                 G_USB_TYPE_DEVICE,
                                                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_FAKE] = g_param_spec_boolean ("fake", NULL, NULL,
                                                FALSE,
                                                G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_ICON] = g_param_spec_object ("icon", NULL, NULL,
                                               G_TYPE_ICON,
                                               G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_NAME] = g_param_spec_string ("name", NULL, NULL, NULL,
                                               G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_SERIAL_NUMBER] = g_param_spec_string ("serial-number", NULL, NULL, NULL,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
bs_stream_deck_init (BsStreamDeck *self)
{
  self->profiles = g_list_store_new (BS_TYPE_PROFILE);
  self->regions = g_list_store_new (BS_TYPE_DEVICE_REGION);
  self->active_pages = g_queue_new ();
}

BsStreamDeck *
bs_stream_deck_new (GUsbDevice  *gusb_device,
                    GError     **error)
{
  return g_initable_new (BS_TYPE_STREAM_DECK,
                         NULL,
                         error,
                         "device", gusb_device,
                         NULL);
}

BsStreamDeck *
bs_stream_deck_new_fake (GError **error)
{
  return g_initable_new (BS_TYPE_STREAM_DECK,
                         NULL,
                         error,
                         "fake", TRUE,
                         NULL);
}

void
bs_stream_deck_reset (BsStreamDeck *self)
{
  g_return_if_fail (BS_IS_STREAM_DECK (self));
  g_return_if_fail (self->model_info->reset != NULL);

  self->model_info->reset (self);
}

GUsbDevice *
bs_stream_deck_get_device (BsStreamDeck *self)
{
  g_return_val_if_fail (BS_IS_STREAM_DECK (self), NULL);

  return self->device;
}

const char *
bs_stream_deck_get_name (BsStreamDeck *self)
{
  g_return_val_if_fail (BS_IS_STREAM_DECK (self), NULL);

  return _(self->model_info->name);
}

const char *
bs_stream_deck_get_serial_number (BsStreamDeck *self)
{
  g_return_val_if_fail (BS_IS_STREAM_DECK (self), NULL);

  return self->serial_number;
}

const char *
bs_stream_deck_get_firmware_version (BsStreamDeck *self)
{
  g_return_val_if_fail (BS_IS_STREAM_DECK (self), NULL);

  return self->firmware_version;
}

GIcon *
bs_stream_deck_get_icon (BsStreamDeck *self)
{
  g_return_val_if_fail (BS_IS_STREAM_DECK (self), NULL);

  return self->icon;
}

/**
 * bs_stream_deck_get_brightness:
 * @self: a #BsStreamDeck
 *
 * Retrieves the current brightness of the device.
 *
 * Returns: device brightness ranging between [0.0, 1.0]
 */
double
bs_stream_deck_get_brightness (BsStreamDeck *self)
{
  g_return_val_if_fail (BS_IS_STREAM_DECK (self), 0.0);

  return self->brightness;
}

/**
 * bs_stream_deck_set_brightness:
 * @self: a #BsStreamDeck
 * @brightness: a double between and including 0.0 and 1.0
 *
 * Sets the brightness of @self to @brightness.
 */
void
bs_stream_deck_set_brightness (BsStreamDeck *self,
                               double        brightness)
{
  g_return_if_fail (BS_IS_STREAM_DECK (self));
  g_return_if_fail (brightness >= 0.0 && brightness <= 1.0);
  g_return_if_fail (self->model_info->set_brightness != NULL);

  if (G_APPROX_VALUE (self->brightness, brightness, FLT_EPSILON))
    return;

  self->brightness = brightness;
  self->model_info->set_brightness (self, brightness);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BRIGHTNESS]);
}

GListModel *
bs_stream_deck_get_regions (BsStreamDeck *self)
{
  g_return_val_if_fail (BS_IS_STREAM_DECK (self), NULL);

  return G_LIST_MODEL (self->regions);
}

BsDeviceRegion *
bs_stream_deck_get_region (BsStreamDeck *self,
                           const char   *region_id)
{
  g_return_val_if_fail (BS_IS_STREAM_DECK (self), NULL);
  g_return_val_if_fail (region_id && g_utf8_validate (region_id, -1, NULL), NULL);

  for (unsigned int i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->regions)); i++)
    {
      g_autoptr (BsDeviceRegion) region = g_list_model_get_item (G_LIST_MODEL (self->regions), i);

      if (g_strcmp0 (bs_device_region_get_id (region), region_id) == 0)
        return region;
    }

  return NULL;
}

gboolean
bs_stream_deck_is_initialized (BsStreamDeck *self)
{
  g_assert (BS_IS_STREAM_DECK (self));

  return self->initialized;
}

gboolean
bs_stream_deck_upload_button (BsStreamDeck  *self,
                              BsButton      *button,
                              GError       **error)
{
  g_autoptr (GdkTexture) texture = NULL;
  BsDeviceRegion *region;
  BsRenderer *renderer;
  BsIcon *icon;

  g_return_val_if_fail (BS_IS_STREAM_DECK (self), FALSE);
  g_return_val_if_fail (self->model_info->set_button_texture != NULL, FALSE);

  icon = bs_button_get_icon (button);
  region = bs_button_get_region (button);
  renderer = bs_device_region_get_renderer (region);
  texture = bs_renderer_compose_icon (renderer, icon, error);

  if (!texture)
    return FALSE;

  return self->model_info->set_button_texture (self, button, texture, error);
}

gboolean
bs_stream_deck_upload_touchscreen (BsStreamDeck   *self,
                                   BsTouchscreen  *touchscreen,
                                   GError        **error)
{
  g_autoptr (GdkTexture) texture = NULL;
  BsTouchscreenContent *content;
  BsRenderer *renderer;

  g_return_val_if_fail (BS_IS_STREAM_DECK (self), FALSE);
  g_return_val_if_fail (self->model_info->set_button_texture != NULL, FALSE);

  content = bs_touchscreen_get_content (touchscreen);
  renderer = bs_device_region_get_renderer (BS_DEVICE_REGION (touchscreen));
  texture = bs_renderer_compose_touchscreen_content (renderer, content, error);

  if (!texture)
    return FALSE;

  return self->model_info->set_touchscreen_texture (self, touchscreen, texture, error);
}

GListModel *
bs_stream_deck_get_profiles (BsStreamDeck *self)
{
  g_return_val_if_fail (BS_IS_STREAM_DECK (self), NULL);

  return G_LIST_MODEL (self->profiles);
}

BsProfile *
bs_stream_deck_get_active_profile (BsStreamDeck *self)
{
  g_return_val_if_fail (BS_IS_STREAM_DECK (self), NULL);

  return self->active_profile;
}

void
bs_stream_deck_load_profile (BsStreamDeck *self,
                             BsProfile    *profile)
{
  g_return_if_fail (BS_IS_STREAM_DECK (self));
  g_return_if_fail (g_list_store_find (self->profiles, profile, NULL));

  BS_ENTRY;

  if (self->active_profile == profile)
    BS_RETURN ();

  if (g_queue_get_length (self->active_pages) > 0)
    update_page_items (self, g_queue_peek_head (self->active_pages));

  g_queue_clear_full (self->active_pages, g_object_unref);

  self->active_profile = profile;

  bs_stream_deck_set_brightness (self, bs_profile_get_brightness (profile));
  bs_stream_deck_push_page (self, bs_profile_get_root_page (profile));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVE_PROFILE]);

  BS_EXIT;
}

BsPage *
bs_stream_deck_get_active_page (BsStreamDeck *self)
{
  g_return_val_if_fail (BS_IS_STREAM_DECK (self), NULL);

  return g_queue_peek_head (self->active_pages);
}

void
bs_stream_deck_push_page (BsStreamDeck  *self,
                          BsPage        *page)
{
  g_return_if_fail (BS_IS_STREAM_DECK (self));
  g_return_if_fail (BS_IS_PAGE (page));
  g_return_if_fail (g_queue_find (self->active_pages, page) == NULL);

  BS_ENTRY;

  if (g_queue_get_length (self->active_pages) > 0)
    bs_page_update_all_items (g_queue_peek_head (self->active_pages));

  g_queue_push_head (self->active_pages, g_object_ref (page));

  load_active_page (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVE_PAGE]);

  BS_EXIT;
}

void
bs_stream_deck_pop_page (BsStreamDeck *self)
{
  g_autoptr (BsPage) page = NULL;

  g_return_if_fail (BS_IS_STREAM_DECK (self));
  g_return_if_fail (g_queue_get_length (self->active_pages) > 1);

  BS_ENTRY;

  page = g_queue_pop_head (self->active_pages);
  update_page_items (self, page);

  bs_page_update_all_items (g_queue_peek_head (self->active_pages));

  load_active_page (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVE_PAGE]);

  BS_EXIT;
}

void
bs_stream_deck_load (BsStreamDeck *self)
{
  g_return_if_fail (BS_IS_STREAM_DECK (self));
  g_return_if_fail (!self->loaded);

  if (!self->fake)
    g_source_attach (self->poll_source, NULL);

  load_profiles (self);

  self->loaded = TRUE;
}

void
bs_stream_deck_save (BsStreamDeck *self)
{
  g_return_if_fail (BS_IS_STREAM_DECK (self));

  BS_ENTRY;

  if (self->save_timeout_id == 0)
    self->save_timeout_id = g_timeout_add_seconds (5, save_after_timeout_cb, self);

  BS_EXIT;
}
