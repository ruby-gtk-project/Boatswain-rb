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

#define G_LOG_DOMAIN "Device"

#include "bs-device-private.h"

#include "bs-actionable-private.h"
#include "bs-action.h"
#include "bs-button-grid.h"
#include "bs-button-private.h"
#include "bs-debug.h"
#include "bs-device-region.h"
#include "bs-device-update-private.h"
#include "bs-dial-private.h"
#include "bs-dial-grid.h"
#include "bs-events-private.h"
#include "bs-icon.h"
#include "bs-macros.h"
#include "bs-page-private.h"
#include "bs-profile.h"
#include "bs-renderer-private.h"
#include "bs-touchscreen-private.h"
#include "bs-touchscreen-slot.h"

#include <glib/gi18n.h>

typedef struct {
  GSource source;
  BsDevice *device;
} BsDeviceSource;

typedef struct
{
  GObject parent_instance;

  GListStore *profiles;
  GListModel *regions;
  BsProfile *active_profile;
  GQueue *active_pages;
  guint save_timeout_id;

  BsDeviceUpdate *update;

  double brightness;
  gboolean initialized;
  gboolean loaded;
  gboolean loading_profile;
} BsDevicePrivate;

static gboolean save_after_timeout_cb (gpointer data);

static void g_initable_iface_init (GInitableIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (BsDevice, bs_device, G_TYPE_OBJECT,
                                  G_ADD_PRIVATE (BsDevice)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, g_initable_iface_init))

enum
{
  PROP_0,
  PROP_ACTIVE_PAGE,
  PROP_ACTIVE_PROFILE,
  PROP_BRIGHTNESS,
  PROP_NAME,
  PROP_SERIAL_NUMBER,
  N_PROPS,
};

static GParamSpec *properties[N_PROPS];


/*
 * Auxiliary methods
 */

static inline void
ensure_device_update (BsDevice *self)
{
  BsDevicePrivate *priv = bs_device_get_instance_private (self);

  if (!priv->update)
    priv->update = bs_device_update_new ();

  g_assert (BS_IS_DEVICE_UPDATE (priv->update));
}

static char *
get_profile_path (BsDevice *self)
{
  g_autofree char *profile_filename = NULL;

  profile_filename = g_strdup_printf ("%s.json", bs_device_get_serial_number (self));

  return g_build_filename (g_get_user_data_dir (),
                           profile_filename,
                           NULL);
}

static void
update_page_region_data (BsDevice *self,
                         BsPage   *page)
{
  BsDevicePrivate *priv = bs_device_get_instance_private (self);
  size_t n_regions = g_list_model_get_n_items (priv->regions);

  for (size_t i = 0; i < n_regions; i++)
    {
      g_autoptr (BsDeviceRegion) region = NULL;
      g_autoptr (JsonNode) region_data = NULL;
      const char *region_id = NULL;

      region = g_list_model_get_item (priv->regions, i);

      if (BS_IS_DIAL_GRID (region))
        {
          /* TODO: implement me */
          continue;
        }

      region_id = bs_device_region_get_id (region);
      region_data = bs_device_region_serialize (region);
      bs_page_set_region_data (page, region_id, region_data);
    }
}

static void
save_profiles (BsDevice *self)
{
  BsDevicePrivate *priv = bs_device_get_instance_private (self);
  g_autoptr (JsonGenerator) generator = NULL;
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) root = NULL;
  g_autoptr (GError) error = NULL;
  g_autofree char *profile_path = NULL;
  g_autofree char *json_str = NULL;

  BS_ENTRY;

  update_page_region_data (self, bs_device_get_active_page (self));
  for (GList *l = g_queue_peek_head_link (priv->active_pages); l; l = l->next)
    bs_page_update_items (l->data);

  /* Update the active profile */
  bs_profile_set_brightness (priv->active_profile, priv->brightness);

  builder = json_builder_new ();

  json_builder_begin_object (builder);

  json_builder_set_member_name (builder, "active-profile");
  json_builder_add_string_value (builder, bs_profile_get_id (priv->active_profile));

  json_builder_set_member_name (builder, "profiles");
  json_builder_begin_array (builder);
  for (size_t i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (priv->profiles)); i++)
    {
      g_autoptr (BsProfile) profile = NULL;

      profile = g_list_model_get_item (G_LIST_MODEL (priv->profiles), i);
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
maybe_create_backup (BsDevice *self,
                     uint32_t  version)
{
  g_autoptr (GError) error = NULL;
  g_autofree char *backup_file_path = NULL;
  g_autofree char *backup_file_name = NULL;
  g_autofree char *backup_folder = NULL;
  g_autofree char *profile_path = NULL;
  g_autofree char *data = NULL;
  size_t length;

  BS_ENTRY;

  g_assert (BS_IS_DEVICE (self));
  g_assert (version > 0);

  backup_folder = g_build_filename (g_get_user_data_dir (),
                                    "backups",
                                    NULL);

  g_mkdir_with_parents (backup_folder, 0755);

  backup_file_name = g_strdup_printf ("%s.bak.v%u", bs_device_get_serial_number (self), version - 1);
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
load_profiles (BsDevice *self)
{
  g_autoptr (JsonParser) parser = NULL;
  g_autoptr (BsProfile) active_profile = NULL;
  g_autoptr (GError) local_error = NULL;
  BsDevicePrivate *priv;
  g_autofree char *profile_path = NULL;
  const char *active_profile_id;
  JsonObject *object;
  JsonArray *profiles_array;
  JsonNode *root;

  BS_ENTRY;

  priv = bs_device_get_instance_private (self);
  profile_path = get_profile_path (self);

  maybe_create_backup (self, 1);

  g_debug ("Loading %s", profile_path);

  parser = json_parser_new ();

  json_parser_load_from_file (parser, profile_path, &local_error);
  if (local_error)
    {
      g_debug ("Error loading profile for device %s: %s",
               bs_device_get_serial_number (self),
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
      g_list_store_append (priv->profiles, profile);

      if (g_strcmp0 (active_profile_id, bs_profile_get_id (profile)) == 0)
        active_profile = g_object_ref (profile);
    }

  if (!active_profile)
    active_profile = g_list_model_get_item (G_LIST_MODEL (priv->profiles), 0);

out:
  if (!active_profile)
    {
      active_profile = bs_profile_new_empty (self);
      bs_profile_set_name (active_profile, _("Default"));
      g_list_store_append (priv->profiles, active_profile);
    }

  bs_device_load_profile (self, active_profile);

  BS_EXIT;
}

static void
load_active_page (BsDevice *self)
{
  BsDevicePrivate *priv = bs_device_get_instance_private (self);
  BsPage *active_page;
  size_t n_regions;

  BS_ENTRY;

  active_page = bs_device_get_active_page (self);
  n_regions = g_list_model_get_n_items (priv->regions);

  for (size_t i = 0; i < n_regions; i++)
    {
      g_autoptr (BsDeviceRegion) region = NULL;
      const char *region_id;
      JsonNode *region_data;

      region = g_list_model_get_item (priv->regions, i);
      region_id = bs_device_region_get_id (region);

      region_data = bs_page_get_region_data (active_page, region_id);
      if (region_data)
        bs_device_region_deserialize (region, region_data);

      if (BS_IS_BUTTON_GRID (region))
        {
          g_autoptr (JsonNode) region_data = NULL;
          BsButtonGrid *button_grid;
          GListModel *buttons;

          button_grid = BS_BUTTON_GRID (region);
          buttons = bs_button_grid_get_buttons (button_grid);

          for (size_t i = 0; i < g_list_model_get_n_items (buttons); i++)
            {
              g_autoptr (BsButton) button = NULL;
              g_autoptr (BsAction) action = NULL;
              g_autoptr (BsIcon) custom_icon = NULL;
              g_autoptr (GError) error = NULL;

              button = g_list_model_get_item (buttons, i);

              bs_page_realize (active_page, region_id, i, &custom_icon, &action, &error);

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

        }
      else if (BS_IS_DIAL_GRID (region))
        {
          /* TODO: implement me */
        }
      else if (BS_IS_TOUCHSCREEN (region))
        {
          BsTouchscreen *touchscreen;
          GListModel *touchscreen_slots;

          touchscreen = BS_TOUCHSCREEN (region);
          touchscreen_slots = bs_touchscreen_get_slots (touchscreen);

          for (size_t i = 0; i < g_list_model_get_n_items (touchscreen_slots); i++)
            {
              g_autoptr (BsTouchscreenSlot) slot = NULL;
              g_autoptr (BsAction) action = NULL;
              g_autoptr (BsIcon) custom_icon = NULL;
              g_autoptr (GError) error = NULL;

              bs_page_realize (active_page, region_id, i, &custom_icon, &action, &error);

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
      else
        {
          g_assert_not_reached ();
        }
    }

  BS_EXIT;
}

static void
schedule_save (BsDevice *self)
{
  BsDevicePrivate *priv = bs_device_get_instance_private (self);

  g_return_if_fail (BS_IS_DEVICE (self));

  BS_ENTRY;

  if (priv->save_timeout_id == 0)
    priv->save_timeout_id = g_timeout_add_seconds (5, save_after_timeout_cb, self);

  BS_EXIT;
}


/*
 * Callbacks
 */

static gboolean
save_after_timeout_cb (gpointer data)
{
  BsDevice *self = BS_DEVICE (data);
  BsDevicePrivate *priv = bs_device_get_instance_private (self);

  BS_ENTRY;

  save_profiles (self);

  priv->save_timeout_id = 0;
  BS_RETURN (G_SOURCE_REMOVE);
}


/*
 * GInitable interface
 */

static void
on_button_grid_button_changed_cb (BsButtonGrid *button_grid,
                                  BsButton     *button,
                                  BsDevice     *self)
{
  BsDevicePrivate *priv = bs_device_get_instance_private (self);
  BsAction *action;
  BsIcon *custom_icon;
  BsPage *active_page;
  const char *region_id;
  size_t position;

  BS_ENTRY;

  if (priv->loading_profile)
    BS_RETURN ();

  g_assert (g_queue_get_length (priv->active_pages) > 0);

  active_page = g_queue_peek_head (priv->active_pages);
  region_id = bs_device_region_get_id (BS_DEVICE_REGION (button_grid));
  custom_icon = bs_button_get_custom_icon (button);
  position = bs_button_get_position (button);
  action = bs_actionable_get_action (BS_ACTIONABLE (button));

  bs_page_update_item (active_page, region_id, position, action, custom_icon);

  schedule_save (self);

  BS_EXIT;
}

static void
on_touchscreen_slot_changed_cb (BsTouchscreen     *touchscreen,
                                BsTouchscreenSlot *slot,
                                BsDevice          *self)
{
  BsDevicePrivate *priv = bs_device_get_instance_private (self);
  BsAction *action;
  BsPage *active_page;
  const char *region_id;
  size_t position;

  BS_ENTRY;

  if (priv->loading_profile)
    BS_RETURN ();

  g_assert (g_queue_get_length (priv->active_pages) > 0);

  active_page = g_queue_peek_head (priv->active_pages);
  region_id = bs_device_region_get_id (BS_DEVICE_REGION (touchscreen));
  position = bs_touchscreen_get_slot_position (touchscreen, slot);
  action = bs_actionable_get_action (BS_ACTIONABLE (slot));

  // TODO: custom icon in touchscreen slots?
  bs_page_update_item (active_page, region_id, position, action, NULL);

  schedule_save (self);

  BS_EXIT;
}

static gboolean
bs_device_initable_init (GInitable     *initable,
                         GCancellable  *cancellable,
                         GError       **error)
{
  BsDevice *self = BS_DEVICE (initable);
  BsDevicePrivate *priv = bs_device_get_instance_private (self);

  BS_ENTRY;

  priv->regions = BS_DEVICE_GET_CLASS (self)->create_layout (self);

  /* TODO: dynamically connect / disconnect to new regions */
  for (size_t i = 0; i < g_list_model_get_n_items (priv->regions); i++)
    {
      g_autoptr (BsDeviceRegion) region = g_list_model_get_item (priv->regions, i);

      if (BS_IS_BUTTON_GRID (region))
        {
          g_signal_connect (region,
                            "button-changed",
                            G_CALLBACK (on_button_grid_button_changed_cb),
                            self);
        }
      else if (BS_IS_DIAL_GRID (region))
        {
          /* TODO: implement me */
        }
      else if (BS_IS_TOUCHSCREEN (region))
        {
          g_signal_connect (region,
                            "touchscreen-slot-changed",
                            G_CALLBACK (on_touchscreen_slot_changed_cb),
                            self);
        }
    }

  priv->initialized = TRUE;

  bs_device_load (self);

  BS_RETURN (TRUE);
}

static void
g_initable_iface_init (GInitableIface *iface)
{
  iface->init = bs_device_initable_init;
}


/*
 * BsDevice overrides
 */

static double
bs_device_real_get_brightness (BsDevice *self)
{
  BsDevicePrivate *priv = bs_device_get_instance_private (self);

  return priv->brightness;
}

static void
bs_device_real_set_brightness (BsDevice *self,
                               double    brightness)
{
  BsDevicePrivate *priv = bs_device_get_instance_private (self);

  priv->brightness = brightness;
}

static void
bs_device_real_load (BsDevice *self)
{
  BsDevicePrivate *priv = bs_device_get_instance_private (self);

  g_assert (BS_IS_DEVICE (self));
  g_assert (!priv->loaded);

  load_profiles (self);

  priv->loaded = TRUE;
}

static void
bs_device_real_update_queued (BsDevice       *self,
                              BsDeviceUpdate *update)
{
}


/*
 * GObject overrides
 */

static void
bs_device_dispose (GObject *object)
{
  BsDevice *self = (BsDevice *) object;
  BsDevicePrivate *priv = bs_device_get_instance_private (self);

  BS_ENTRY;

  g_assert (BS_IS_MAIN_THREAD ());

  if (priv->initialized)
    {
      save_profiles (self);
      bs_device_reset (self);
    }

  g_clear_handle_id (&priv->save_timeout_id, g_source_remove);

  G_OBJECT_CLASS (bs_device_parent_class)->dispose (object);

  BS_EXIT;
}

static void
bs_device_finalize (GObject *object)
{
  BsDevice *self = (BsDevice *) object;
  BsDevicePrivate *priv = bs_device_get_instance_private (self);

  BS_ENTRY;

  g_assert (BS_IS_MAIN_THREAD ());

  g_queue_free_full (priv->active_pages, g_object_unref);
  g_clear_object (&priv->regions);
  g_clear_object (&priv->profiles);
  g_clear_object (&priv->update);

  G_OBJECT_CLASS (bs_device_parent_class)->finalize (object);

  BS_EXIT;
}

static void
bs_device_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  BsDevice *self = BS_DEVICE (object);
  BsDevicePrivate *priv = bs_device_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ACTIVE_PAGE:
      g_value_set_object (value, bs_device_get_active_page (self));
      break;

    case PROP_ACTIVE_PROFILE:
      g_value_set_object (value, priv->active_profile);
      break;

    case PROP_BRIGHTNESS:
      g_value_set_double (value, priv->brightness);
      break;

    case PROP_NAME:
      g_value_set_string (value, bs_device_get_name (self));
      break;

    case PROP_SERIAL_NUMBER:
      g_value_set_string (value, bs_device_get_serial_number (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_device_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  BsDevice *self = BS_DEVICE (object);

  switch (prop_id)
    {
    case PROP_BRIGHTNESS:
      bs_device_set_brightness (self, g_value_get_double (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_device_class_init (BsDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = bs_device_dispose;
  object_class->finalize = bs_device_finalize;
  object_class->get_property = bs_device_get_property;
  object_class->set_property = bs_device_set_property;

  klass->get_brightness = bs_device_real_get_brightness;
  klass->set_brightness = bs_device_real_set_brightness;
  klass->load = bs_device_real_load;
  klass->update_queued = bs_device_real_update_queued;

  properties[PROP_ACTIVE_PAGE] = g_param_spec_object ("active-page", NULL, NULL,
                                                      BS_TYPE_PAGE,
                                                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_ACTIVE_PROFILE] = g_param_spec_object ("active-profile", NULL, NULL,
                                                         BS_TYPE_PROFILE,
                                                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_BRIGHTNESS] = g_param_spec_double ("brightness", NULL, NULL,
                                                     0.0, 1.0, 0.5,
                                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_NAME] = g_param_spec_string ("name", NULL, NULL, NULL,
                                               G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_SERIAL_NUMBER] = g_param_spec_string ("serial-number", NULL, NULL, NULL,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
bs_device_init (BsDevice *self)
{
  BsDevicePrivate *priv = bs_device_get_instance_private (self);

  priv->profiles = g_list_store_new (BS_TYPE_PROFILE);
  priv->active_pages = g_queue_new ();
}

void
bs_device_reset (BsDevice *self)
{
  g_return_if_fail (BS_IS_DEVICE (self));

  if (BS_DEVICE_GET_CLASS (self)->reset)
    BS_DEVICE_GET_CLASS (self)->reset (self);
}

const char *
bs_device_get_name (BsDevice *self)
{
  g_return_val_if_fail (BS_IS_DEVICE (self), NULL);
  g_assert (BS_DEVICE_GET_CLASS (self)->get_name != NULL);

  return BS_DEVICE_GET_CLASS (self)->get_name (self);
}

const char *
bs_device_get_serial_number (BsDevice *self)
{
  g_return_val_if_fail (BS_IS_DEVICE (self), NULL);
  g_assert (BS_DEVICE_GET_CLASS (self)->get_serial_number != NULL);

  return BS_DEVICE_GET_CLASS (self)->get_serial_number (self);
}

const char *
bs_device_get_firmware_version (BsDevice *self)
{
  g_return_val_if_fail (BS_IS_DEVICE (self), NULL);
  g_assert (BS_DEVICE_GET_CLASS (self)->get_firmware_version != NULL);

  return BS_DEVICE_GET_CLASS (self)->get_firmware_version (self);
}

/**
 * bs_device_get_brightness:
 * @self: a #BsDevice
 *
 * Retrieves the current brightness of the device.
 *
 * Returns: device brightness ranging between [0.0, 1.0]
 */
double
bs_device_get_brightness (BsDevice *self)
{
  BsDevicePrivate *priv = bs_device_get_instance_private (self);

  g_return_val_if_fail (BS_IS_DEVICE (self), 0.0);

  return priv->brightness;
}

/**
 * bs_device_set_brightness:
 * @self: a #BsDevice
 * @brightness: a double between and including 0.0 and 1.0
 *
 * Sets the brightness of @self to @brightness.
 */
void
bs_device_set_brightness (BsDevice *self,
                          double    brightness)
{
  BsDevicePrivate *priv = bs_device_get_instance_private (self);

  g_return_if_fail (BS_IS_DEVICE (self));
  g_return_if_fail (brightness >= 0.0 && brightness <= 1.0);
  g_return_if_fail (BS_DEVICE_GET_CLASS (self)->set_brightness != NULL);

  if (G_APPROX_VALUE (priv->brightness, brightness, FLT_EPSILON))
    return;

  priv->brightness = brightness;
  BS_DEVICE_GET_CLASS (self)->set_brightness (self, brightness);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BRIGHTNESS]);
}

GListModel *
bs_device_get_regions (BsDevice *self)
{
  BsDevicePrivate *priv = bs_device_get_instance_private (self);

  g_return_val_if_fail (BS_IS_DEVICE (self), NULL);

  return G_LIST_MODEL (priv->regions);
}

BsDeviceRegion *
bs_device_get_region (BsDevice   *self,
                      const char *region_id)
{
  BsDevicePrivate *priv = bs_device_get_instance_private (self);

  g_return_val_if_fail (BS_IS_DEVICE (self), NULL);
  g_return_val_if_fail (region_id && g_utf8_validate (region_id, -1, NULL), NULL);

  for (unsigned int i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (priv->regions)); i++)
    {
      g_autoptr (BsDeviceRegion) region = g_list_model_get_item (G_LIST_MODEL (priv->regions), i);

      if (g_strcmp0 (bs_device_region_get_id (region), region_id) == 0)
        return region;
    }

  return NULL;
}

gboolean
bs_device_is_initialized (BsDevice *self)
{
  BsDevicePrivate *priv = bs_device_get_instance_private (self);

  g_assert (BS_IS_DEVICE (self));

  return priv->initialized;
}

void
bs_device_upload_button (BsDevice *self,
                         BsButton *button)
{
  BsDeviceClass *klass = BS_DEVICE_GET_CLASS (self);
  BsDevicePrivate *priv = bs_device_get_instance_private (self);

  g_return_if_fail (BS_IS_DEVICE (self));

  ensure_device_update (self);

  bs_device_update_add_button (priv->update, button);

  klass->update_queued (self, priv->update);
}

void
bs_device_upload_touchscreen (BsDevice              *self,
                              BsTouchscreen         *touchscreen,
                              const graphene_rect_t *region)
{
  BsDeviceClass *klass = BS_DEVICE_GET_CLASS (self);
  BsDevicePrivate *priv = bs_device_get_instance_private (self);

  g_return_if_fail (BS_IS_DEVICE (self));

  ensure_device_update (self);

  bs_device_update_add_touchscreen_region (priv->update, touchscreen, region);

  klass->update_queued (self, priv->update);
}

GListModel *
bs_device_get_profiles (BsDevice *self)
{
  BsDevicePrivate *priv = bs_device_get_instance_private (self);

  g_return_val_if_fail (BS_IS_DEVICE (self), NULL);

  return G_LIST_MODEL (priv->profiles);
}

BsProfile *
bs_device_get_active_profile (BsDevice *self)
{
  BsDevicePrivate *priv = bs_device_get_instance_private (self);

  g_return_val_if_fail (BS_IS_DEVICE (self), NULL);

  return priv->active_profile;
}

void
bs_device_load_profile (BsDevice  *self,
                        BsProfile *profile)
{
  BsDevicePrivate *priv = bs_device_get_instance_private (self);

  g_return_if_fail (BS_IS_DEVICE (self));
  g_return_if_fail (g_list_store_find (priv->profiles, profile, NULL));

  BS_ENTRY;

  if (priv->active_profile == profile)
    BS_RETURN ();

  if (!g_queue_is_empty (priv->active_pages))
    update_page_region_data (self, g_queue_peek_head (priv->active_pages));

  while (!g_queue_is_empty (priv->active_pages))
    {
      g_autoptr (BsPage) page = g_queue_pop_head (priv->active_pages);

      bs_page_unload_items (page);
    }

  g_assert (g_queue_is_empty (priv->active_pages));

  g_queue_clear_full (priv->active_pages, g_object_unref);

  priv->active_profile = profile;

  priv->loading_profile = TRUE;

  bs_device_set_brightness (self, bs_profile_get_brightness (profile));
  bs_device_push_page (self, bs_profile_get_root_page (profile));

  priv->loading_profile = FALSE;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVE_PROFILE]);

  BS_EXIT;
}

BsPage *
bs_device_get_active_page (BsDevice *self)
{
  BsDevicePrivate *priv = bs_device_get_instance_private (self);

  g_return_val_if_fail (BS_IS_DEVICE (self), NULL);

  return g_queue_peek_head (priv->active_pages);
}

void
bs_device_push_page (BsDevice *self,
                     BsPage   *page)
{
  BsDevicePrivate *priv = bs_device_get_instance_private (self);

  g_return_if_fail (BS_IS_DEVICE (self));
  g_return_if_fail (BS_IS_PAGE (page));
  g_return_if_fail (g_queue_find (priv->active_pages, page) == NULL);

  BS_ENTRY;

  g_queue_push_head (priv->active_pages, g_object_ref (page));

  bs_page_load_items (page);
  load_active_page (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVE_PAGE]);

  BS_EXIT;
}

void
bs_device_pop_page (BsDevice *self)
{
  BsDevicePrivate *priv = bs_device_get_instance_private (self);
  g_autoptr (BsPage) page = NULL;

  g_return_if_fail (BS_IS_DEVICE (self));
  g_return_if_fail (g_queue_get_length (priv->active_pages) > 1);

  BS_ENTRY;

  page = g_queue_pop_head (priv->active_pages);
  update_page_region_data (self, page);
  bs_page_unload_items (page);

  load_active_page (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVE_PAGE]);

  BS_EXIT;
}

void
bs_device_load (BsDevice *self)
{
  BsDevicePrivate *priv = bs_device_get_instance_private (self);

  g_return_if_fail (BS_IS_DEVICE (self));
  g_return_if_fail (!priv->loaded);

  BS_DEVICE_GET_CLASS (self)->load (self);
}

BsDeviceUpdate *
bs_device_steal_update (BsDevice *self)
{
  BsDevicePrivate *priv = bs_device_get_instance_private (self);

  g_assert (BS_IS_DEVICE (self));

  return g_steal_pointer (&priv->update);
}
