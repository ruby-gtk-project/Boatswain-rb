/* soundboard-play-action.c
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
#include "soundboard-play-action.h"
#include "soundboard-play-action-prefs.h"

struct _SoundboardPlayAction
{
  BsAction parent_instance;

  GtkWidget *image;

  GFile *file;
  GQueue *media_streams_queue;
  GtkMediaStream *media_stream;
  double volume;

  SoundboardPlayBehavior behavior;
  GtkWidget *prefs;
  gboolean deserializing;
};

static void on_media_stream_ended_changed_cb (GtkMediaStream       *media_stream,
                                              GParamSpec           *pspec,
                                              SoundboardPlayAction *self);

static void on_overlap_media_stream_ended_changed_cb (GtkMediaStream       *media_stream,
                                                      GParamSpec           *pspec,
                                                      SoundboardPlayAction *self);

G_DEFINE_FINAL_TYPE (SoundboardPlayAction, soundboard_play_action, BS_TYPE_ACTION)


/*
 * Auxiliary methods
 */

static void
update_icon_from_state (SoundboardPlayAction *self)
{
  gboolean is_playing = FALSE;

  switch (self->behavior)
    {
    case SOUNDBOARD_PLAY_BEHAVIOR_PLAY_STOP:
    case SOUNDBOARD_PLAY_BEHAVIOR_PLAY_RESTART:
    case SOUNDBOARD_PLAY_BEHAVIOR_LOOP_STOP:
    case SOUNDBOARD_PLAY_BEHAVIOR_PRESS_HOLD:
      if (self->media_stream)
        is_playing = gtk_media_stream_get_playing (self->media_stream);
      break;

    case SOUNDBOARD_PLAY_BEHAVIOR_PLAY_OVERLAP:
      is_playing = !g_queue_is_empty (self->media_streams_queue);
      break;
    }

  bs_icon_set_icon_name (bs_action_get_icon (BS_ACTION (self)),
                         is_playing ? "media-playback-stop-symbolic" : "media-playback-start-symbolic");
}

static GtkMediaStream *
get_media_stream (SoundboardPlayAction *self)
{
  GtkMediaStream *media_stream;

  switch (self->behavior)
    {
    case SOUNDBOARD_PLAY_BEHAVIOR_PLAY_STOP:
    case SOUNDBOARD_PLAY_BEHAVIOR_PLAY_RESTART:
    case SOUNDBOARD_PLAY_BEHAVIOR_LOOP_STOP:
    case SOUNDBOARD_PLAY_BEHAVIOR_PRESS_HOLD:
      if (!self->media_stream)
        {
          self->media_stream = gtk_media_file_new_for_file (self->file);
          gtk_media_stream_set_volume (self->media_stream, self->volume);
          g_signal_connect (self->media_stream, "notify::ended", G_CALLBACK (on_media_stream_ended_changed_cb), self);
        }
      return self->media_stream;

    case SOUNDBOARD_PLAY_BEHAVIOR_PLAY_OVERLAP:
      media_stream = gtk_media_file_new_for_file (self->file);
      gtk_media_stream_set_volume (media_stream, self->volume);
      g_queue_push_tail (self->media_streams_queue, media_stream);
      g_object_set_data (G_OBJECT (media_stream), "self-destroy", GINT_TO_POINTER (TRUE));
      g_signal_connect (media_stream, "notify::ended", G_CALLBACK (on_overlap_media_stream_ended_changed_cb), self);
      return media_stream;
    }

  return NULL;
}

static void
activate_play_action (SoundboardPlayAction *self)
{
  GtkMediaStream *media_stream = get_media_stream (self);

  switch (self->behavior)
    {
    case SOUNDBOARD_PLAY_BEHAVIOR_PLAY_STOP:
      gtk_media_stream_set_loop (media_stream, FALSE);
      if (gtk_media_stream_get_playing (media_stream))
        {
          gtk_media_stream_pause (media_stream);
          gtk_media_stream_seek (media_stream, 0);
        }
      else
        {
          gtk_media_stream_seek (media_stream, 0);
          gtk_media_stream_play (media_stream);
        }
      break;

    case SOUNDBOARD_PLAY_BEHAVIOR_PLAY_RESTART:
      gtk_media_stream_seek (media_stream, 0);
      gtk_media_stream_play (media_stream);
      break;

    case SOUNDBOARD_PLAY_BEHAVIOR_LOOP_STOP:
      gtk_media_stream_set_loop (media_stream, TRUE);
      if (gtk_media_stream_get_playing (media_stream))
        {
          gtk_media_stream_pause (media_stream);
          gtk_media_stream_seek (media_stream, 0);
        }
      else
        {
          gtk_media_stream_seek (media_stream, 0);
          gtk_media_stream_play (media_stream);
        }
      break;

    case SOUNDBOARD_PLAY_BEHAVIOR_PLAY_OVERLAP:
      gtk_media_stream_play (media_stream);
      break;

    case SOUNDBOARD_PLAY_BEHAVIOR_PRESS_HOLD:
      gtk_media_stream_set_loop (media_stream, TRUE);
      gtk_media_stream_seek (media_stream, 0);
      gtk_media_stream_play (media_stream);
      break;
    }
}

static void
deactivate_play_action (SoundboardPlayAction *self)
{
  GtkMediaStream *media_stream = get_media_stream (self);

  switch (self->behavior)
    {
    case SOUNDBOARD_PLAY_BEHAVIOR_PLAY_STOP:
    case SOUNDBOARD_PLAY_BEHAVIOR_PLAY_RESTART:
    case SOUNDBOARD_PLAY_BEHAVIOR_LOOP_STOP:
    case SOUNDBOARD_PLAY_BEHAVIOR_PLAY_OVERLAP:
      break;

    case SOUNDBOARD_PLAY_BEHAVIOR_PRESS_HOLD:
      gtk_media_stream_pause (media_stream);
      break;
    }

}

static void
maybe_save_action (SoundboardPlayAction *self)
{
  if (!self->deserializing)
    bs_action_changed (BS_ACTION (self));
}


/*
 * Callbacks
 */


static void
on_media_stream_ended_changed_cb (GtkMediaStream       *media_stream,
                                  GParamSpec           *pspec,
                                  SoundboardPlayAction *self)
{
  if (!gtk_media_stream_get_ended (media_stream))
    return;

  update_icon_from_state (self);
}

static void
on_overlap_media_stream_ended_changed_cb (GtkMediaStream       *media_stream,
                                          GParamSpec           *pspec,
                                          SoundboardPlayAction *self)
{
  if (!gtk_media_stream_get_ended (media_stream))
    return;

  g_queue_remove (self->media_streams_queue, media_stream);
  g_object_unref (media_stream);

  update_icon_from_state (self);
}


/*
 * BsAction overrides
 */

static void
soundboard_play_action_handle_event (BsAction *action,
                                     BsEvent  *event)
{
  SoundboardPlayAction *self = SOUNDBOARD_PLAY_ACTION (action);

  switch (bs_event_get_event_type (event))
    {
    case BS_BUTTON_PRESS:
      activate_play_action (self);
      break;

    case BS_BUTTON_RELEASE:
      deactivate_play_action (self);
      break;
    }

  update_icon_from_state (self);
}

static GtkWidget *
soundboard_play_action_get_preferences (BsAction *action)
{
  SoundboardPlayAction *self = SOUNDBOARD_PLAY_ACTION (action);

  return self->prefs;
}

static JsonNode *
soundboard_play_action_serialize_settings (BsAction *action)
{
  g_autoptr (JsonBuilder) builder = NULL;
  SoundboardPlayAction *self;

  self = SOUNDBOARD_PLAY_ACTION (action);
  builder = json_builder_new ();

  json_builder_begin_object (builder);

  json_builder_set_member_name (builder, "file");
  if (self->file)
    {
      g_autofree char *path = g_file_get_path (self->file);
      json_builder_add_string_value (builder, path);
    }
  else
    {
      json_builder_add_null_value (builder);
    }

  json_builder_set_member_name (builder, "behavior");
  json_builder_add_int_value (builder, self->behavior);

  json_builder_set_member_name (builder, "volume");
  json_builder_add_double_value (builder, self->volume);

  json_builder_end_object (builder);

  return json_builder_get_root (builder);
}

static void
soundboard_play_action_deserialize_settings (BsAction   *action,
                                             JsonObject *settings)
{
  SoundboardPlayAction *self = SOUNDBOARD_PLAY_ACTION (action);

  self->deserializing = TRUE;
  soundboard_play_action_prefs_deserialize_settings (SOUNDBOARD_PLAY_ACTION_PREFS (self->prefs),
                                                     settings);
  self->deserializing = FALSE;
}



/*
 * GObject overrides
 */

static void
soundboard_play_action_finalize (GObject *object)
{
  SoundboardPlayAction *self = (SoundboardPlayAction *)object;

  g_queue_free_full (self->media_streams_queue, g_object_unref);

  g_clear_object (&self->media_stream);
  g_clear_object (&self->file);

  G_OBJECT_CLASS (soundboard_play_action_parent_class)->finalize (object);
}

static void
soundboard_play_action_class_init (SoundboardPlayActionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  BsActionClass *action_class = BS_ACTION_CLASS (klass);

  object_class->finalize = soundboard_play_action_finalize;

  action_class->handle_event = soundboard_play_action_handle_event;
  action_class->get_preferences = soundboard_play_action_get_preferences;
  action_class->serialize_settings = soundboard_play_action_serialize_settings;
  action_class->deserialize_settings = soundboard_play_action_deserialize_settings;
}

static void
soundboard_play_action_init (SoundboardPlayAction *self)
{
  bs_icon_set_icon_name (bs_action_get_icon (BS_ACTION (self)), "media-playback-start-symbolic");

  self->behavior = SOUNDBOARD_PLAY_BEHAVIOR_PLAY_STOP;
  self->media_streams_queue = g_queue_new ();
  self->volume = 1.0;

  self->prefs = soundboard_play_action_prefs_new (self);
  g_object_ref_sink (self->prefs);
}

BsAction *
soundboard_play_action_new (BsButton *button)
{
  return g_object_new (SOUNDBOARD_TYPE_PLAY_ACTION,
                       "button", button,
                       NULL);
}

void
soundboard_play_action_set_file (SoundboardPlayAction *self,
                                 GFile                *file)
{
  g_return_if_fail (SOUNDBOARD_IS_PLAY_ACTION (self));
  g_return_if_fail (G_IS_FILE (file));

  g_set_object (&self->file, file);

  g_clear_object (&self->media_stream);
  g_queue_clear_full (self->media_streams_queue, g_object_unref);

  update_icon_from_state (self);
  maybe_save_action (self);
}

void
soundboard_play_action_set_behavior (SoundboardPlayAction   *self,
                                     SoundboardPlayBehavior  behavior)
{
  g_return_if_fail (SOUNDBOARD_IS_PLAY_ACTION (self));

  self->behavior = behavior;

  update_icon_from_state (self);
  maybe_save_action (self);
}

void
soundboard_play_action_set_volume (SoundboardPlayAction *self,
                                   double                volume)
{
  GList *l;

  g_return_if_fail (SOUNDBOARD_IS_PLAY_ACTION (self));
  g_return_if_fail (volume >= 0.0 && volume <= 1.0);

  self->volume = volume;

  if (self->media_stream)
    gtk_media_stream_set_volume (self->media_stream, volume);

  for (l = g_queue_peek_head_link (self->media_streams_queue); l; l = l->next)
    gtk_media_stream_set_volume (l->data, volume);

  maybe_save_action (self);
}
