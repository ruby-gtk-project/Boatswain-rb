/* launcher-launch-action.c
 *
 * Copyright 2022 Emmanuele Bassi
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
#include "launcher-launch-action.h"
#include "launcher-launch-preferences.h"

struct _LauncherLaunchAction
{
  BsAction parent_instance;

  GAppInfo *app;
  GList *files;

  GtkWidget *prefs;
};

G_DEFINE_FINAL_TYPE (LauncherLaunchAction, launcher_launch_action, BS_TYPE_ACTION)

static void
launcher_launch_action_update_icon (LauncherLaunchAction *self)
{
  gboolean icon_set = FALSE;

  if (self->app != NULL)
    {
      GIcon *app_icon = g_app_info_get_icon (self->app);

      if (G_IS_THEMED_ICON (app_icon))
        {
          const char * const *names = g_themed_icon_get_names (G_THEMED_ICON (app_icon));

          if (names != NULL && names[0] != NULL)
            {
              bs_icon_set_icon_name (bs_action_get_icon (BS_ACTION (self)), names[0]);
              icon_set = TRUE;
            }
        }
    }

  if (!icon_set)
    bs_icon_set_icon_name (bs_action_get_icon (BS_ACTION (self)), "application-x-executable-symbolic");
}

static void
launcher_launch_action_handle_event (BsAction *action,
                                     BsEvent  *event)
{
  g_autoptr (GdkAppLaunchContext) context = NULL;
  g_autoptr (GError) error = NULL;
  LauncherLaunchAction *self;
  GdkDisplay *display;

  if (bs_event_get_event_type (event) != BS_BUTTON_PRESS)
    return;

  self = LAUNCHER_LAUNCH_ACTION (action);
  display = gdk_display_get_default ();
  context = gdk_display_get_app_launch_context (display);

  if (!g_app_info_launch (self->app, self->files, G_APP_LAUNCH_CONTEXT (context), &error))
    g_warning ("Unable to launch application %s: %s", g_app_info_get_name (self->app), error->message);
}

static GtkWidget *
launcher_launch_action_get_preferences (BsAction *action)
{
  LauncherLaunchAction *self = LAUNCHER_LAUNCH_ACTION (action);

  return self->prefs;
}

static JsonNode *
launcher_launch_action_serialize_settings (BsAction *action)
{
  g_autoptr (JsonBuilder) builder = NULL;
  LauncherLaunchAction *self;

  self = LAUNCHER_LAUNCH_ACTION (action);
  builder = json_builder_new ();

  json_builder_begin_object (builder);

  json_builder_set_member_name (builder, "app");
  if (self->app != NULL)
    json_builder_add_string_value (builder, g_app_info_get_id (self->app));
  else
    json_builder_add_null_value (builder);

  json_builder_set_member_name (builder, "files");

  json_builder_begin_array (builder);
  for (GList *iter = self->files; iter != NULL; iter = iter->next)
    {
      g_autofree char *path = g_file_get_path (iter->data);
      json_builder_add_string_value (builder, path);
    }
  json_builder_end_array (builder);

  json_builder_end_object (builder);

  return json_builder_get_root (builder);
}

static void
build_files_list (JsonArray *array,
                  guint      index_,
                  JsonNode  *element,
                  gpointer   data)
{
  GList **files = data;

  *files = g_list_append (*files, g_file_new_for_path (json_node_get_string (element)));
}

static void
launcher_launch_action_deserialize_settings (BsAction   *action,
                                             JsonObject *settings)
{
  LauncherLaunchAction *self = LAUNCHER_LAUNCH_ACTION (action);
  JsonArray *app_files;
  GAppInfo *app_info = NULL;
  GList *files = NULL;
  const char *app_id;

  app_id = json_object_get_string_member (settings, "app");
  if (app_id != NULL)
    {
      GList *apps = g_app_info_get_all ();
      GList *l;

      for (l = apps; l; l = l->next)
        {
          if (!g_app_info_should_show (l->data))
            continue;

          if (g_strcmp0 (app_id, g_app_info_get_id (l->data)) == 0)
            {
              app_info = l->data;
              break;
            }
        }
    }

  app_files = json_object_get_array_member (settings, "files");
  json_array_foreach_element (app_files, build_files_list, &files);

  g_set_object (&self->app, app_info);

  g_list_free_full (self->files, g_object_unref);
  self->files = files;

  launcher_launch_action_update_icon (self);
}

static void
launcher_launch_action_finalize (GObject *gobject)
{
  LauncherLaunchAction *self = LAUNCHER_LAUNCH_ACTION (gobject);

  g_clear_object (&self->app);
  g_clear_object (&self->prefs);

  g_list_free_full (self->files, g_object_unref);

  G_OBJECT_CLASS (launcher_launch_action_parent_class)->finalize (gobject);
}

static void
launcher_launch_action_class_init (LauncherLaunchActionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  BsActionClass *action_class = BS_ACTION_CLASS (klass);

  object_class->finalize = launcher_launch_action_finalize;

  action_class->handle_event = launcher_launch_action_handle_event;
  action_class->get_preferences = launcher_launch_action_get_preferences;
  action_class->serialize_settings = launcher_launch_action_serialize_settings;
  action_class->deserialize_settings = launcher_launch_action_deserialize_settings;
}

static void
launcher_launch_action_init (LauncherLaunchAction *self)
{
  bs_icon_set_icon_name (bs_action_get_icon (BS_ACTION (self)), "application-x-executable-symbolic");

  self->prefs = launcher_launch_preferences_new (self);
  g_object_ref_sink (self->prefs);
}

BsAction *
launcher_launch_action_new (BsButton *button)
{
  return g_object_new (LAUNCHER_TYPE_LAUNCH_ACTION,
                       "button", button,
                       NULL);
}

void
launcher_launch_action_set_app (LauncherLaunchAction *self,
                                GAppInfo             *app_info)
{
  if (g_set_object (&self->app, app_info))
    launcher_launch_action_update_icon (self);
}
