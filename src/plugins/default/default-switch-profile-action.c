/* default-switch-profile-action.c
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

#include "default-switch-profile-action.h"

#include <adwaita.h>
#include <glib/gi18n.h>

struct _DefaultSwitchProfileAction
{
  BsAction parent_instance;

  char *serial_number;
  char *profile_id;

  AdwComboRow *stream_decks_row;
  AdwComboRow *profiles_row;

  GBinding *binding;

  gulong items_changed_id;
};

G_DEFINE_FINAL_TYPE (DefaultSwitchProfileAction, default_switch_profile_action, BS_TYPE_ACTION)


/*
 * Auxiliary methods
 */

static BsStreamDeck *
find_stream_deck (const char   *serial_number,
                  unsigned int *out_position)
{
  GListModel *devices;
  BsContext *context;

  context = bs_context_get_default ();
  devices = bs_context_get_devices (context);

  for (unsigned int i = 0; i < g_list_model_get_n_items (devices); i++)
    {
      g_autoptr (BsStreamDeck) stream_deck = NULL;

      stream_deck = g_list_model_get_item (devices, i);

      if (g_strcmp0 (bs_stream_deck_get_serial_number (stream_deck), serial_number) == 0)
        {
          if (out_position)
            *out_position = i;
          return stream_deck;
        }
    }

  if (out_position)
    *out_position = GTK_INVALID_LIST_POSITION;

  return NULL;
}

static BsProfile *
get_profile_from_stream_deck (BsStreamDeck *stream_deck,
                              const char   *profile_id,
                              unsigned int *out_position)
{
  GListModel *profiles;
  unsigned int i;

  profiles = bs_stream_deck_get_profiles (stream_deck);

  for (i = 0; i < g_list_model_get_n_items (profiles); i++)
    {
      g_autoptr (BsProfile) profile = g_list_model_get_item (profiles, i);

      if (g_strcmp0 (bs_profile_get_id (profile), profile_id) == 0)
        {
          if (out_position)
            *out_position = i;
          return profile;
        }
    }

  if (out_position)
    *out_position = GTK_INVALID_LIST_POSITION;

  return bs_stream_deck_get_active_profile (stream_deck);
}

static gboolean
find_stream_deck_and_profile (DefaultSwitchProfileAction  *self,
                              BsStreamDeck               **out_stream_deck,
                              BsProfile                  **out_profile)
{
  BsStreamDeck *stream_deck;
  BsProfile *profile;

  stream_deck = find_stream_deck (self->serial_number, NULL);

  if (!stream_deck)
    return FALSE;

  profile = get_profile_from_stream_deck (stream_deck, self->profile_id, NULL);

  if (!profile)
    return FALSE;

  *out_stream_deck = stream_deck;
  *out_profile = profile;
  return TRUE;
}

static void
set_active_profile (DefaultSwitchProfileAction *self,
                    BsProfile                  *profile)
{
  BsIcon *icon;

  g_clear_pointer (&self->binding, g_binding_unbind);

  if (!profile)
    return;

  icon = bs_action_get_icon (BS_ACTION (self));

  self->binding = g_object_bind_property (profile, "name",
                                          icon, "text",
                                          G_BINDING_SYNC_CREATE);
  g_object_add_weak_pointer (G_OBJECT (self->binding), (gpointer *) &self->binding);

  bs_icon_set_text (icon, bs_profile_get_name (profile));
  bs_action_changed (BS_ACTION (self));
}

static void
update_active_profile (DefaultSwitchProfileAction *self)
{
  BsStreamDeck *stream_deck;
  BsProfile *profile;
  BsIcon *icon;

  icon = bs_action_get_icon (BS_ACTION (self));

  if (find_stream_deck_and_profile (self, &stream_deck, &profile))
    {
      bs_icon_set_opacity (icon, -1.0);
      set_active_profile (self, profile);
    }
  else
    {
      bs_icon_set_opacity (icon, 0.35);
    }
}


/*
 * Callbacks
 */

static void
on_stream_decks_combo_row_selected_item_changed_cb (AdwComboRow                *combo_row,
                                                    GParamSpec                 *pspec,
                                                    DefaultSwitchProfileAction *self)
{

  BsStreamDeck *stream_deck = adw_combo_row_get_selected_item (combo_row);

  g_clear_pointer (&self->serial_number, g_free);
  self->serial_number = stream_deck ? g_strdup (bs_stream_deck_get_serial_number (stream_deck)) : NULL;

  g_assert (self->profiles_row != NULL);

  if (stream_deck)
    adw_combo_row_set_model (self->profiles_row, bs_stream_deck_get_profiles (stream_deck));
  else
    adw_combo_row_set_model (self->profiles_row, NULL);

  update_active_profile (self);
}

static void
on_combo_row_selected_item_changed_cb (AdwComboRow                *combo_row,
                                       GParamSpec                 *pspec,
                                       DefaultSwitchProfileAction *self)
{
  BsProfile *profile = adw_combo_row_get_selected_item (combo_row);

  g_clear_pointer (&self->profile_id, g_free);
  self->profile_id = profile ? g_strdup (bs_profile_get_id (profile)) : NULL;

  set_active_profile (self, profile);
}

static void
on_context_devices_items_changed_cb (GListModel                 *model,
                                     unsigned int                position,
                                     unsigned int                removed,
                                     unsigned int                added,
                                     DefaultSwitchProfileAction *self)
{
  update_active_profile (self);
}


/*
 * BsAction overrides
 */

static void
default_switch_profile_action_handle_event (BsAction *action,
                                            BsEvent  *event)
{
  DefaultSwitchProfileAction *self;
  BsStreamDeck *stream_deck;
  BsProfile *profile;

  if (bs_event_get_event_type (event) != BS_BUTTON_PRESS)
    return;

  self = DEFAULT_SWITCH_PROFILE_ACTION (action);

  if (find_stream_deck_and_profile (self, &stream_deck, &profile))
    bs_stream_deck_load_profile (stream_deck, profile);
}

static GtkWidget *
default_switch_profile_action_get_preferences (BsAction *action)
{
  DefaultSwitchProfileAction *self = DEFAULT_SWITCH_PROFILE_ACTION (action);
  BsStreamDeck *stream_deck;
  GListModel *profiles;
  BsContext *context;
  GtkWidget *group;
  GtkWidget *row;
  unsigned int position;

  if (self->stream_decks_row)
    g_object_remove_weak_pointer (G_OBJECT (self->stream_decks_row), (gpointer *) &self->stream_decks_row);
  if (self->profiles_row)
    g_object_remove_weak_pointer (G_OBJECT (self->profiles_row), (gpointer *) &self->profiles_row);

  context = bs_context_get_default ();

  group = adw_preferences_group_new ();

  /* Stream Decks */
  row = adw_combo_row_new ();
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), _("Stream Deck"));
  adw_combo_row_set_expression (ADW_COMBO_ROW (row),
                                gtk_property_expression_new (BS_TYPE_PROFILE, NULL, "name"));
  adw_combo_row_set_model (ADW_COMBO_ROW (row), bs_context_get_devices (context));

  if ((stream_deck = find_stream_deck (self->serial_number, &position)) != NULL)
    adw_combo_row_set_selected (ADW_COMBO_ROW (row), position);
  else
    adw_combo_row_set_selected (ADW_COMBO_ROW (row), GTK_INVALID_LIST_POSITION);

  self->stream_decks_row = ADW_COMBO_ROW (row);
  g_object_add_weak_pointer (G_OBJECT (self->stream_decks_row), (gpointer *) &self->stream_decks_row);
  adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

  g_signal_connect (row,
                    "notify::selected-item",
                    G_CALLBACK (on_stream_decks_combo_row_selected_item_changed_cb),
                    self);

  /* Profiles */
  profiles = stream_deck ? bs_stream_deck_get_profiles (stream_deck) : NULL;

  row = adw_combo_row_new ();
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), _("Profile"));
  adw_combo_row_set_expression (ADW_COMBO_ROW (row),
                                gtk_property_expression_new (BS_TYPE_PROFILE, NULL, "name"));
  adw_combo_row_set_model (ADW_COMBO_ROW (row), profiles);

  if (profiles && get_profile_from_stream_deck (stream_deck, self->profile_id, &position))
    adw_combo_row_set_selected (ADW_COMBO_ROW (row), position);
  else
    adw_combo_row_set_selected (ADW_COMBO_ROW (row), GTK_INVALID_LIST_POSITION);

  self->profiles_row = ADW_COMBO_ROW (row);
  g_object_add_weak_pointer (G_OBJECT (self->profiles_row), (gpointer *) &self->profiles_row);
  adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

  g_signal_connect (row,
                    "notify::selected-item",
                    G_CALLBACK (on_combo_row_selected_item_changed_cb),
                    self);

  return group;
}

static JsonNode *
default_switch_profile_action_serialize_settings (BsAction *action)
{
  DefaultSwitchProfileAction *self = DEFAULT_SWITCH_PROFILE_ACTION (action);
  g_autoptr (JsonBuilder) builder = NULL;

  builder = json_builder_new ();

  json_builder_begin_object (builder);

  if (self->serial_number)
    {
      json_builder_set_member_name (builder, "serial-number");
      json_builder_add_string_value (builder, self->serial_number);
    }

  if (self->profile_id)
    {
      json_builder_set_member_name (builder, "profile-id");
      json_builder_add_string_value (builder, self->profile_id);
    }

  json_builder_end_object (builder);

  return json_builder_get_root (builder);
}

static void
default_switch_profile_action_deserialize_settings (BsAction   *action,
                                                    JsonObject *object)
{
  DefaultSwitchProfileAction *self = DEFAULT_SWITCH_PROFILE_ACTION (action);
  const char *serial_number;
  const char *profile_id;

  serial_number = json_object_get_string_member_with_default (object, "serial-number", NULL);
  if (serial_number)
    {
      g_clear_pointer (&self->serial_number, g_free);
      self->serial_number = g_strdup (serial_number);
    }

  profile_id = json_object_get_string_member_with_default (object, "profile-id", NULL);
  if (profile_id)
    {
      g_clear_pointer (&self->profile_id, g_free);
      self->profile_id = g_strdup (profile_id);
    }

  update_active_profile (self);
}


/*
 * GObject overrides
 */

static void
default_switch_profile_action_finalize (GObject *object)
{
  DefaultSwitchProfileAction *self = (DefaultSwitchProfileAction *)object;

  g_clear_pointer (&self->serial_number, g_free);
  g_clear_pointer (&self->profile_id, g_free);
  g_clear_pointer (&self->binding, g_binding_unbind);

  G_OBJECT_CLASS (default_switch_profile_action_parent_class)->finalize (object);
}

static void
default_switch_profile_action_constructed (GObject *object)
{
  DefaultSwitchProfileAction *self = (DefaultSwitchProfileAction *)object;
  BsContext *context;
  BsIcon *icon;

  G_OBJECT_CLASS (default_switch_profile_action_parent_class)->constructed (object);

  icon = bs_action_get_icon (BS_ACTION (self));
  bs_icon_set_icon_name (icon, "preferences-desktop-apps-symbolic");

  context = bs_context_get_default ();
  g_signal_connect_object (bs_context_get_devices (context),
                           "items-changed",
                           G_CALLBACK (on_context_devices_items_changed_cb),
                           self,
                           0);

  update_active_profile (self);
}


static void
default_switch_profile_action_class_init (DefaultSwitchProfileActionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  BsActionClass *action_class = BS_ACTION_CLASS (klass);

  object_class->finalize = default_switch_profile_action_finalize;
  object_class->constructed = default_switch_profile_action_constructed;

  action_class->handle_event = default_switch_profile_action_handle_event;
  action_class->get_preferences = default_switch_profile_action_get_preferences;
  action_class->serialize_settings = default_switch_profile_action_serialize_settings;
  action_class->deserialize_settings = default_switch_profile_action_deserialize_settings;
}

static void
default_switch_profile_action_init (DefaultSwitchProfileAction *self)
{
}

BsAction *
default_switch_profile_action_new (void)
{
  return g_object_new (DEFAULT_TYPE_SWITCH_PROFILE_ACTION, NULL);
}
