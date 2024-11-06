/* launcher-open-url-action.c
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
#include "launcher-open-url-action.h"

#include <glib/gi18n.h>

struct _LauncherOpenUrlAction
{
  BsAction parent_instance;

  char *url;
  GCancellable *cancellable;
};

G_DEFINE_FINAL_TYPE (LauncherOpenUrlAction, launcher_open_url_action, BS_TYPE_ACTION)


/*
 * Callbacks
 */

static void
on_uri_launched_cb (GObject      *source_object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  g_autoptr (GError) error = NULL;

  gtk_uri_launcher_launch_finish (GTK_URI_LAUNCHER (source_object), result, &error);

  if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    g_warning ("Error showing URL: %s", error->message);
}

static void
on_url_row_text_changed_cb (GtkEditable           *editable,
                            GParamSpec            *pspec,
                            LauncherOpenUrlAction *self)
{
  g_clear_pointer (&self->url, g_free);
  self->url = g_strdup (gtk_editable_get_text (editable));
  g_strstrip (self->url);
}

/*
 * BsAction overrides
 */

static void
launcher_open_url_action_handle_event (BsAction *action,
                                       BsEvent  *event)
{
  g_autoptr (GtkUriLauncher) uri_launcher = NULL;
  LauncherOpenUrlAction *self;
  GApplication *application;
  GtkWindow *window;

  if (bs_event_get_event_type (event) != BS_BUTTON_PRESS)
    return;

  self = LAUNCHER_OPEN_URL_ACTION (action);

  if (!self->url || self->url[0] == '\0')
    return;

  application = g_application_get_default ();
  window = gtk_application_get_active_window (GTK_APPLICATION (application));

  uri_launcher = gtk_uri_launcher_new (self->url);
  gtk_uri_launcher_launch (uri_launcher,
                           window,
                           self->cancellable,
                           on_uri_launched_cb,
                           self);
}

static GtkWidget *
launcher_open_url_action_get_preferences (BsAction *action)
{
  LauncherOpenUrlAction *self;
  GtkWidget *group;
  GtkWidget *row;

  self = LAUNCHER_OPEN_URL_ACTION (action);

  group = adw_preferences_group_new ();

  row = adw_entry_row_new ();
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), _("URL"));
  gtk_editable_set_text (GTK_EDITABLE (row), self->url ?: "");
  g_signal_connect (row, "notify::text", G_CALLBACK (on_url_row_text_changed_cb), self);
  adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

  return group;
}

static JsonNode *
launcher_open_url_action_serialize_settings (BsAction *action)
{
  g_autoptr (JsonBuilder) builder = NULL;
  LauncherOpenUrlAction *self;

  self = LAUNCHER_OPEN_URL_ACTION (action);
  builder = json_builder_new ();

  json_builder_begin_object (builder);

  json_builder_set_member_name (builder, "url");
  if (self->url)
    json_builder_add_string_value (builder, self->url);
  else
    json_builder_add_null_value (builder);

  json_builder_end_object (builder);

  return json_builder_get_root (builder);
}

static void
launcher_open_url_action_deserialize_settings (BsAction   *action,
                                               JsonObject *settings)
{
  LauncherOpenUrlAction *self = LAUNCHER_OPEN_URL_ACTION (action);

  g_clear_pointer (&self->url, g_free);
  self->url = g_strdup (json_object_get_string_member_with_default (settings, "url", NULL));
  if (self->url)
    self->url = g_strstrip (self->url);
}


/*
 * GObject overrides
 */

static void
launcher_open_url_action_finalize (GObject *object)
{
  LauncherOpenUrlAction *self = (LauncherOpenUrlAction *)object;

  g_cancellable_cancel (self->cancellable);

  g_clear_pointer (&self->url, g_free);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (launcher_open_url_action_parent_class)->finalize (object);
}

static void
launcher_open_url_action_class_init (LauncherOpenUrlActionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  BsActionClass *action_class = BS_ACTION_CLASS (klass);

  object_class->finalize = launcher_open_url_action_finalize;

  action_class->handle_event = launcher_open_url_action_handle_event;
  action_class->get_preferences = launcher_open_url_action_get_preferences;
  action_class->serialize_settings = launcher_open_url_action_serialize_settings;
  action_class->deserialize_settings = launcher_open_url_action_deserialize_settings;
}

static void
launcher_open_url_action_init (LauncherOpenUrlAction *self)
{
  bs_icon_set_icon_name (bs_action_get_icon (BS_ACTION (self)), "web-browser-symbolic");
}

BsAction *
launcher_open_url_action_new (void)
{
  return g_object_new (LAUNCHER_TYPE_OPEN_URL_ACTION, NULL);
}

