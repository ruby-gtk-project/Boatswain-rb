/*
 * launcher-open-file-action.c
 *
 * Copyright 2023 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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
#include "launcher-open-file-action.h"

#include <glib/gi18n.h>

struct _LauncherOpenFileAction
{
  BsAction parent_instance;

  AdwActionRow *file_row;

  GFile *file;
  GCancellable *cancellable;
};

G_DEFINE_FINAL_TYPE (LauncherOpenFileAction, launcher_open_file_action, BS_TYPE_ACTION)


/*
 * Auxiliary functions
 */

static void
update_file_row (LauncherOpenFileAction *self)
{
  GtkWidget *button;
  GtkWidget *icon;

  if (!self->file_row)
    return;

  button = g_object_get_data (G_OBJECT (self->file_row), "button");
  icon = g_object_get_data (G_OBJECT (self->file_row), "folder-icon");

  gtk_widget_set_visible (button, self->file != NULL);
  gtk_widget_set_visible (icon, self->file == NULL);

  if (self->file)
    {
      g_autofree char *basename = g_file_get_basename (self->file);

      adw_action_row_set_subtitle (self->file_row, basename);
      gtk_widget_add_css_class (GTK_WIDGET (self->file_row), "property");
    }
  else
    {
      adw_action_row_set_subtitle (self->file_row, _("No file selected"));
      gtk_widget_remove_css_class (GTK_WIDGET (self->file_row), "property");
    }
}

static void
set_file (LauncherOpenFileAction *self,
          GFile                  *file)
{

  g_set_object (&self->file, file);
  bs_action_changed (BS_ACTION (self));

  if (file)
    {
      g_autofree char *basename = g_file_get_basename (self->file);
      bs_icon_set_text (bs_action_get_icon (BS_ACTION (self)), basename);
    }
  else
    {
      bs_icon_set_text (bs_action_get_icon (BS_ACTION (self)), NULL);
    }

  update_file_row (self);
}


/*
 * Callbacks
 */

static void
on_clear_button_clicked_cb (GtkButton              *button,
                            LauncherOpenFileAction *self)
{
  set_file (self, NULL);
}

static void
on_file_launched_cb (GObject      *source_object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  g_autoptr (GError) error = NULL;

  gtk_file_launcher_launch_finish (GTK_FILE_LAUNCHER (source_object), result, &error);

  if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    g_warning ("Error opening file: %s", error->message);
}

static void
on_file_dialog_opened_cb (GObject      *source,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  LauncherOpenFileAction *self = LAUNCHER_OPEN_FILE_ACTION (user_data);
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) file = NULL;

  file = gtk_file_dialog_open_finish (GTK_FILE_DIALOG (source), result, &error);

  if (error)
    {
      if (g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED) ||
          g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
        {
          /* Do nothing if the dialog was just dismissed */
          return;
        }
    }

  set_file (self, file);
}

static void
on_file_row_activated_cb (GtkEditable            *editable,
                          GParamSpec             *pspec,
                          LauncherOpenFileAction *self)
{
  g_autoptr (GtkFileDialog) file_dialog = NULL;
  GApplication *application;
  GtkWindow *window;

  application = g_application_get_default ();
  window = gtk_application_get_active_window (GTK_APPLICATION (application));

  file_dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (file_dialog, _("Select File"));
  gtk_file_dialog_set_modal (file_dialog, TRUE);

  gtk_file_dialog_open (file_dialog,
                        window,
                        self->cancellable,
                        on_file_dialog_opened_cb,
                        self);
}


/*
 * BsAction overrides
 */

static void
launcher_open_file_action_handle_event (BsAction *action,
                                        BsEvent  *event)
{
  g_autoptr (GtkFileLauncher) file_launcher = NULL;
  LauncherOpenFileAction *self;
  GApplication *application;
  GtkWindow *window;

  if (bs_event_get_event_type (event) != BS_BUTTON_PRESS)
    return;

  self = LAUNCHER_OPEN_FILE_ACTION (action);

  if (!self->file)
    return;

  application = g_application_get_default ();
  window = gtk_application_get_active_window (GTK_APPLICATION (application));

  file_launcher = gtk_file_launcher_new (self->file);
  gtk_file_launcher_launch (file_launcher,
                            window,
                            self->cancellable,
                            on_file_launched_cb,
                            self);
}

static GtkWidget *
launcher_open_file_action_get_preferences (BsAction *action)
{
  LauncherOpenFileAction *self;
  GtkWidget *widget;
  GtkWidget *group;
  GtkWidget *row;

  self = LAUNCHER_OPEN_FILE_ACTION (action);

  group = adw_preferences_group_new ();

  row = adw_action_row_new ();
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), _("File"));
  g_signal_connect (row, "activated", G_CALLBACK (on_file_row_activated_cb), self);
  adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

  widget = gtk_image_new_from_icon_name ("document-open-symbolic");
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), widget);
  g_object_set_data (G_OBJECT (row), "folder-icon", widget);

  widget = gtk_button_new_from_icon_name ("edit-clear-symbolic");
  gtk_widget_add_css_class (widget, "flat");
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), widget);
  g_signal_connect (widget, "clicked", G_CALLBACK (on_clear_button_clicked_cb), self);
  g_object_set_data (G_OBJECT (row), "button", widget);

  self->file_row = ADW_ACTION_ROW (row);
  g_object_add_weak_pointer (G_OBJECT (row), (gpointer *) &self->file_row);

  update_file_row (self);

  return group;
}

static JsonNode *
launcher_open_file_action_serialize_settings (BsAction *action)
{
  g_autoptr (JsonBuilder) builder = NULL;
  LauncherOpenFileAction *self;

  self = LAUNCHER_OPEN_FILE_ACTION (action);
  builder = json_builder_new ();

  json_builder_begin_object (builder);

  json_builder_set_member_name (builder, "file");
  if (self->file)
    {
      g_autofree char *uri = g_file_get_uri (self->file);
      json_builder_add_string_value (builder, uri);
    }
  else
    {
      json_builder_add_null_value (builder);
    }

  json_builder_end_object (builder);

  return json_builder_get_root (builder);
}

static void
launcher_open_file_action_deserialize_settings (BsAction   *action,
                                               JsonObject *settings)
{
  LauncherOpenFileAction *self = LAUNCHER_OPEN_FILE_ACTION (action);
  const char *uri;

  uri = json_object_get_string_member_with_default (settings, "file", NULL);

  g_clear_object (&self->file);
  if (uri)
    {
      g_autoptr (GFile) file = g_file_new_for_uri (uri);
      set_file (self, file);
    }
}


/*
 * GObject overrides
 */

static void
launcher_open_file_action_finalize (GObject *object)
{
  LauncherOpenFileAction *self = (LauncherOpenFileAction *)object;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->file);

  G_OBJECT_CLASS (launcher_open_file_action_parent_class)->finalize (object);
}

static void
launcher_open_file_action_class_init (LauncherOpenFileActionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  BsActionClass *action_class = BS_ACTION_CLASS (klass);

  object_class->finalize = launcher_open_file_action_finalize;

  action_class->handle_event = launcher_open_file_action_handle_event;
  action_class->get_preferences = launcher_open_file_action_get_preferences;
  action_class->serialize_settings = launcher_open_file_action_serialize_settings;
  action_class->deserialize_settings = launcher_open_file_action_deserialize_settings;
}

static void
launcher_open_file_action_init (LauncherOpenFileAction *self)
{
  bs_icon_set_icon_name (bs_action_get_icon (BS_ACTION (self)), "folder-documents-symbolic");
}

BsAction *
launcher_open_file_action_new (BsButton *button)
{
  return g_object_new (LAUNCHER_TYPE_OPEN_FILE_ACTION,
                       "button", button,
                       NULL);
}

