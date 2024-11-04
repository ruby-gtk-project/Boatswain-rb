/* soundboard-mpris-action.c
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

#include "bs-events.h"
#include "bs-icon.h"
#include "soundboard-mpris-action.h"

#include <glib/gi18n.h>

typedef enum
{
  ACTION_NEXT,
  ACTION_PAUSE,
  ACTION_PLAY,
  ACTION_PREVIOUS,
  ACTION_STOP,
  ACTION_TOGGLE,
} Action;

struct _SoundboardMprisAction
{
  BsAction parent_instance;

  Action action;

  MprisController *mpris_controller;
};

G_DEFINE_FINAL_TYPE (SoundboardMprisAction, soundboard_mpris_action, BS_TYPE_ACTION)


/*
 * Auxiliary methods
 */
static const char * const action_names[] = {
  [ACTION_NEXT] = "next",
  [ACTION_PAUSE] = "pause",
  [ACTION_PLAY] = "play",
  [ACTION_PREVIOUS] = "previous",
  [ACTION_STOP] = "stop",
  [ACTION_TOGGLE] = "toggle",
};

static gboolean
action_from_string (const char *string,
                    Action     *out_action)
{

  for (size_t i = 0; i < G_N_ELEMENTS (action_names); i++)
    {
      if (g_strcmp0 (action_names[i], string) == 0)
        {
          *out_action = i;
          return TRUE;
        }
    }

  return FALSE;
}

static const char *
action_to_string (Action action)
{
  return action_names[action];
}

static void
update_icon (SoundboardMprisAction *self)
{
  BsIcon *icon = bs_action_get_icon (BS_ACTION (self));

  switch (self->action)
    {
    case ACTION_NEXT:
      bs_icon_set_icon_name (icon, "media-skip-forward-symbolic");
      break;

    case ACTION_PAUSE:
      bs_icon_set_icon_name (icon, "media-playback-pause-symbolic");
      break;

    case ACTION_PLAY:
      bs_icon_set_icon_name (icon, "media-playback-start-symbolic");
      break;

    case ACTION_PREVIOUS:
      bs_icon_set_icon_name (icon, "media-skip-backward-symbolic");
      break;

    case ACTION_STOP:
      bs_icon_set_icon_name (icon, "media-playback-stop-symbolic");
      break;

    case ACTION_TOGGLE:
      if (mpris_controller_get_is_playing (self->mpris_controller))
        bs_icon_set_icon_name (icon, "media-playback-pause-symbolic");
      else
        bs_icon_set_icon_name (icon, "media-playback-start-symbolic");
      break;
    }

  if (mpris_controller_get_has_active_player (self->mpris_controller))
    bs_icon_set_opacity (icon, -1.0);
  else
    bs_icon_set_opacity (icon, 0.35);
}


/*
 * Callbacks
 */

static void
on_mpris_controller_changed_cb (MprisController       *mpris_controller,
                                GParamSpec            *pspec,
                                SoundboardMprisAction *self)
{
  update_icon (self);
}

static void
on_combo_row_selected_changed_cb (AdwComboRow           *combo_row,
                                  GParamSpec            *pspec,
                                  SoundboardMprisAction *self)
{
  self->action = adw_combo_row_get_selected (combo_row);
  update_icon (self);
  bs_action_changed (BS_ACTION (self));
}


/*
 * BsAction overrides
 */

static void
soundboard_mpris_action_handle_event (BsAction *action,
                                      BsEvent  *event)
{
  SoundboardMprisAction *self = SOUNDBOARD_MPRIS_ACTION (action);

  if (bs_event_get_event_type (event) != BS_BUTTON_PRESS)
    return;

  if (!mpris_controller_get_has_active_player (self->mpris_controller))
    return;

  switch (self->action)
    {
    case ACTION_NEXT:
      mpris_controller_key (self->mpris_controller, "Next");
      break;

    case ACTION_PAUSE:
      mpris_controller_key (self->mpris_controller, "Pause");
      break;

    case ACTION_PLAY:
      mpris_controller_key (self->mpris_controller, "Play");
      break;

    case ACTION_PREVIOUS:
      mpris_controller_key (self->mpris_controller, "Previous");
      break;

    case ACTION_STOP:
      mpris_controller_key (self->mpris_controller, "Stop");
      break;

    case ACTION_TOGGLE:
      mpris_controller_key (self->mpris_controller, "PlayPause");
      break;
    }
}

static GtkWidget *
soundboard_mpris_action_get_preferences (BsAction *action)
{
  SoundboardMprisAction *self = SOUNDBOARD_MPRIS_ACTION (action);
  g_autoptr (GtkStringList) options = NULL;
  GtkWidget *group;
  GtkWidget *row;

  options = gtk_string_list_new (NULL);
  gtk_string_list_append (options, _("Next"));
  gtk_string_list_append (options, _("Pause"));
  gtk_string_list_append (options, _("Play"));
  gtk_string_list_append (options, _("Previous"));
  gtk_string_list_append (options, _("Stop"));
  gtk_string_list_append (options, _("Play / Pause"));

  row = adw_combo_row_new ();
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), _("Playback Action"));
  adw_combo_row_set_model (ADW_COMBO_ROW (row), G_LIST_MODEL (options));
  adw_combo_row_set_selected (ADW_COMBO_ROW (row), self->action);
  g_signal_connect (row, "notify::selected", G_CALLBACK (on_combo_row_selected_changed_cb), self);

  group = adw_preferences_group_new ();
  adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

  return group;
}

static JsonNode *
soundboard_mpris_action_serialize_settings (BsAction *action)
{
  g_autoptr (JsonBuilder) builder = NULL;
  SoundboardMprisAction *self;

  self = SOUNDBOARD_MPRIS_ACTION (action);
  builder = json_builder_new ();

  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "action");
  json_builder_add_string_value (builder, action_to_string (self->action));
  json_builder_end_object (builder);

  return json_builder_get_root (builder);
}

static void
soundboard_mpris_action_deserialize_settings (BsAction   *action,
                                              JsonObject *settings)
{
  SoundboardMprisAction *self = SOUNDBOARD_MPRIS_ACTION (action);

  if (!action_from_string (json_object_get_string_member_with_default (settings, "action", NULL),
                           &self->action))
    {
      self->action = ACTION_TOGGLE;
    }

  update_icon (self);
}


/*
 * GObject overrides
 */

static void
soundboard_mpris_action_finalize (GObject *object)
{
  SoundboardMprisAction *self = (SoundboardMprisAction *)object;

  g_clear_object (&self->mpris_controller);

  G_OBJECT_CLASS (soundboard_mpris_action_parent_class)->finalize (object);
}

static void
soundboard_mpris_action_class_init (SoundboardMprisActionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  BsActionClass *action_class = BS_ACTION_CLASS (klass);

  object_class->finalize = soundboard_mpris_action_finalize;

  action_class->handle_event = soundboard_mpris_action_handle_event;
  action_class->get_preferences = soundboard_mpris_action_get_preferences;
  action_class->serialize_settings = soundboard_mpris_action_serialize_settings;
  action_class->deserialize_settings = soundboard_mpris_action_deserialize_settings;
}

static void
soundboard_mpris_action_init (SoundboardMprisAction *self)
{
  self->action = ACTION_TOGGLE;
}

BsAction *
soundboard_mpris_action_new (BsButton        *button,
                             MprisController *mpris_controller)
{
  SoundboardMprisAction *self;

  self = g_object_new (SOUNDBOARD_TYPE_MPRIS_ACTION,
                       "button", button,
                       NULL);

  self->mpris_controller = g_object_ref (mpris_controller);
  g_signal_connect_object (self->mpris_controller,
                           "notify",
                           G_CALLBACK (on_mpris_controller_changed_cb),
                           self,
                           0);
  update_icon (self);

  return BS_ACTION (self);
}
