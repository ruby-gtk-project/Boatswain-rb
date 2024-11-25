/*
 * bs-touchscreen-background-row.c
 *
 * Copyright 2024 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "bs-touchscreen-background-row.h"

#include "bs-touchscreen-content.h"
#include "bs-touchscreen-private.h"

#include <glib/gi18n.h>

struct _BsTouchscreenBackgroundRow
{
  AdwPreferencesRow parent_instance;

  GSimpleActionGroup *action_group;
  BsTouchscreen *touchscreen;
};

G_DEFINE_FINAL_TYPE (BsTouchscreenBackgroundRow, bs_touchscreen_background_row, ADW_TYPE_PREFERENCES_ROW)

enum {
  PROP_0,
  PROP_TOUCHSCREEN,
  N_PROPS,
};

static GParamSpec *properties [N_PROPS];


/*
 * Callbacks
 */

static void
on_file_dialog_file_opened_cb (GObject      *source,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  g_autoptr (GdkPaintable) paintable = NULL;
  g_autoptr (GFileInfo) file_info = NULL;
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) file = NULL;
  BsTouchscreenBackgroundRow *self;
  BsTouchscreenContent *content;

  file = gtk_file_dialog_open_finish (GTK_FILE_DIALOG (source), result, &error);

  if (error)
    {
      if (!g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED) &&
          !g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
        {
          g_warning ("Error opening file: %s", error->message);
        }
      return;
    }

  self = BS_TOUCHSCREEN_BACKGROUND_ROW (user_data);

  file_info = g_file_query_info (file,
                                 G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                 G_FILE_QUERY_INFO_NONE,
                                 NULL,
                                 &error);

  if (file_info)
    {
      const char * const media_stream_content_types[] = {
        "image/gif",
        "video/*",
      };

      g_autoptr (GtkMediaStream) media_stream = NULL;
      const char *content_type;

      content_type = g_file_info_get_content_type (file_info);
      for (size_t i = 0; i < G_N_ELEMENTS (media_stream_content_types); i++)
        {
          if (!g_content_type_is_mime_type (content_type, media_stream_content_types[i]))
            continue;

          media_stream = gtk_media_file_new_for_file (file);
          gtk_media_stream_set_volume (media_stream, 0.0);
          gtk_media_stream_set_muted (media_stream, TRUE);
          gtk_media_stream_set_loop (media_stream, TRUE);
          gtk_media_stream_play (media_stream);

          paintable = GDK_PAINTABLE (g_steal_pointer (&media_stream));
          break;
        }
    }
  else
    {
      g_warning ("Error querying file info: %s", error->message);
    }

  if (!paintable)
    {
      g_autoptr (GdkTexture) texture = NULL;

      texture = gdk_texture_new_from_file (file, &error);
      if (!texture)
        return;

      paintable = GDK_PAINTABLE (g_steal_pointer (&texture));
    }

  content = bs_touchscreen_get_content (self->touchscreen);
  bs_touchscreen_content_set_background (content, paintable);
}

static void
on_select_background_action_activated_cb (GSimpleAction *action,
                                          GVariant      *parameter,
                                          gpointer       user_data)
{
  BsTouchscreenBackgroundRow *self = (BsTouchscreenBackgroundRow *) user_data;
  g_autoptr (GtkFileDialog) dialog = NULL;
  g_autoptr (GtkFileFilter) filter = NULL;
  g_autoptr (GListStore) filters = NULL;

  g_assert (BS_IS_TOUCHSCREEN_BACKGROUND_ROW (self));

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("All supported formats"));
  gtk_file_filter_add_mime_type (filter, "image/*");
  gtk_file_filter_add_mime_type (filter, "video/*");

  filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  g_list_store_append (filters, filter);

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_modal (dialog, TRUE);
  gtk_file_dialog_set_title (dialog, _("Select media file"));
  gtk_file_dialog_set_accept_label (dialog, _("Open"));
  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));

  gtk_file_dialog_open (dialog,
                        GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self))),
                        NULL,
                        on_file_dialog_file_opened_cb,
                        self);
}


/*
 * GObject overrides
 */

static void
bs_touchscreen_background_row_dispose (GObject *object)
{
  BsTouchscreenBackgroundRow *self = (BsTouchscreenBackgroundRow *)object;

  g_clear_object (&self->touchscreen);

  gtk_widget_dispose_template (GTK_WIDGET (self), BS_TYPE_TOUCHSCREEN_BACKGROUND_ROW);

  G_OBJECT_CLASS (bs_touchscreen_background_row_parent_class)->dispose (object);
}

static void
bs_touchscreen_background_row_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
  BsTouchscreenBackgroundRow *self = BS_TOUCHSCREEN_BACKGROUND_ROW (object);

  switch (prop_id)
    {
    case PROP_TOUCHSCREEN:
      g_value_set_object (value, self->touchscreen);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_touchscreen_background_row_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
  BsTouchscreenBackgroundRow *self = BS_TOUCHSCREEN_BACKGROUND_ROW (object);

  switch (prop_id)
    {
    case PROP_TOUCHSCREEN:
      if (g_set_object (&self->touchscreen, g_value_get_object (value)))
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TOUCHSCREEN]);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_touchscreen_background_row_class_init (BsTouchscreenBackgroundRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = bs_touchscreen_background_row_dispose;
  object_class->get_property = bs_touchscreen_background_row_get_property;
  object_class->set_property = bs_touchscreen_background_row_set_property;

  properties[PROP_TOUCHSCREEN] =
    g_param_spec_object ("touchscreen", NULL, NULL,
                         BS_TYPE_TOUCHSCREEN,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/com/feaneron/Boatswain/bs-touchscreen-background-row.ui");
}

static void
bs_touchscreen_background_row_init (BsTouchscreenBackgroundRow *self)
{
  const GActionEntry actions[] = {
    { "select-background", on_select_background_action_activated_cb, },
  };

  self->action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (self->action_group),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);

  gtk_widget_insert_action_group (GTK_WIDGET (self), "backgroundrow", G_ACTION_GROUP (self->action_group));

  gtk_widget_init_template (GTK_WIDGET (self));
}
