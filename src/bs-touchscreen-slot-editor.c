/*
 * bs-touchscreen-slot-editor.c
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

#include "bs-touchscreen-slot-editor.h"

#include "bs-actionable-private.h"
#include "bs-action-selector.h"
#include "bs-action-factory.h"
#include "bs-action-info.h"
#include "bs-application-private.h"
#include "bs-empty-action.h"
#include "bs-touchscreen-content.h"
#include "bs-touchscreen-private.h"
#include "bs-touchscreen-slot.h"

#include <glib/gi18n.h>

struct _BsTouchscreenSlotEditor
{
  AdwBin parent_instance;

  AdwPreferencesGroup *action_preferences_group;
  AdwNavigationView *navigation_view;
  GtkWidget *remove_action_group;

  BsTouchscreenSlot *slot;
  GtkWidget *action_preferences;

  gulong action_changed_id;
};

G_DEFINE_FINAL_TYPE (BsTouchscreenSlotEditor, bs_touchscreen_slot_editor, ADW_TYPE_BIN)

enum
{
  PROP_0,
  PROP_TOUCHSCREEN_SLOT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];


/*
 * Auxiliary methods
 */

static void
update_action_preferences_group (BsTouchscreenSlotEditor *self)
{
  GtkWidget *action_preferences;
  BsAction *action;

  action = bs_actionable_get_action (BS_ACTIONABLE (self->slot));
  action_preferences = action ? bs_action_get_preferences (action) : NULL;

  gtk_widget_set_visible (self->remove_action_group,
                          action != NULL && !BS_IS_EMPTY_ACTION (action));

  if (self->action_preferences != action_preferences)
    {
      if (self->action_preferences)
        adw_preferences_group_remove (self->action_preferences_group, self->action_preferences);

      self->action_preferences = action_preferences;

      if (action_preferences)
        adw_preferences_group_add (self->action_preferences_group, action_preferences);

      gtk_widget_set_visible (GTK_WIDGET (self->action_preferences_group),
                              action_preferences != NULL);
    }
}


/*
 * Callbacks
 */

static void
on_action_selector_action_selected_cb (BsActionSelector        *selector,
                                       BsActionFactory         *factory,
                                       BsActionInfo            *action_info,
                                       BsTouchscreenSlotEditor *self)
{
  g_autoptr (BsAction) action = NULL;

  action = bs_action_factory_create_action (factory, action_info);
  bs_actionable_set_action (BS_ACTIONABLE (self->slot), action);

  adw_navigation_view_pop (self->navigation_view);
}

static void
on_action_changed_cb (BsButton                *button,
                      GParamSpec              *pspec,
                      BsTouchscreenSlotEditor *self)
{
  update_action_preferences_group (self);
}

static void
on_file_dialog_file_opened_cb (GObject      *source,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  g_autoptr (GdkPaintable) paintable = NULL;
  g_autoptr (GFileInfo) file_info = NULL;
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) file = NULL;
  BsTouchscreenSlotEditor *self;
  BsTouchscreenContent *content;
  BsTouchscreen *touchscreen;

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

  self = BS_TOUCHSCREEN_SLOT_EDITOR (user_data);

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

  touchscreen = bs_touchscreen_slot_get_touchscreen (self->slot);
  content = bs_touchscreen_get_content (touchscreen);
  bs_touchscreen_content_set_background (content, paintable);
}

static void
on_background_row_activated_cb (AdwPreferencesRow       *row,
                                BsTouchscreenSlotEditor *self)
{
  g_autoptr (GtkFileDialog) dialog = NULL;
  g_autoptr (GtkFileFilter) filter = NULL;
  g_autoptr (GListStore) filters = NULL;

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
bs_touchscreen_slot_editor_dispose (GObject *object)
{
  BsTouchscreenSlotEditor *self = (BsTouchscreenSlotEditor *)object;

  g_clear_signal_handler (&self->action_changed_id, self->slot);
  g_clear_object (&self->slot);

  G_OBJECT_CLASS (bs_touchscreen_slot_editor_parent_class)->dispose (object);
}

static void
bs_touchscreen_slot_editor_constructed (GObject *object)
{
  BsTouchscreenSlotEditor *self = (BsTouchscreenSlotEditor *)object;

  G_OBJECT_CLASS (bs_touchscreen_slot_editor_parent_class)->constructed (object);

  self->action_changed_id = g_signal_connect (self->slot,
                                              "notify::action",
                                              G_CALLBACK (on_action_changed_cb),
                                              self);

  update_action_preferences_group (self);
}

static void
bs_touchscreen_slot_editor_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  BsTouchscreenSlotEditor *self = BS_TOUCHSCREEN_SLOT_EDITOR (object);

  switch (prop_id)
    {
    case PROP_TOUCHSCREEN_SLOT:
      g_value_set_object (value, self->slot);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_touchscreen_slot_editor_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  BsTouchscreenSlotEditor *self = BS_TOUCHSCREEN_SLOT_EDITOR (object);

  switch (prop_id)
    {
    case PROP_TOUCHSCREEN_SLOT:
      g_assert (self->slot == NULL);
      self->slot = g_value_dup_object (value);
      g_assert (self->slot != NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_touchscreen_slot_editor_class_init (BsTouchscreenSlotEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_ensure (BS_TYPE_ACTION_SELECTOR);

  object_class->dispose = bs_touchscreen_slot_editor_dispose;
  object_class->constructed = bs_touchscreen_slot_editor_constructed;
  object_class->get_property = bs_touchscreen_slot_editor_get_property;
  object_class->set_property = bs_touchscreen_slot_editor_set_property;

  properties[PROP_TOUCHSCREEN_SLOT] =
    g_param_spec_object ("touchscreen-slot", NULL, NULL,
                         BS_TYPE_TOUCHSCREEN_SLOT,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/com/feaneron/Boatswain/bs-touchscreen-slot-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, BsTouchscreenSlotEditor, action_preferences_group);
  gtk_widget_class_bind_template_child (widget_class, BsTouchscreenSlotEditor, navigation_view);
  gtk_widget_class_bind_template_child (widget_class, BsTouchscreenSlotEditor, remove_action_group);

  gtk_widget_class_bind_template_callback (widget_class, on_action_selector_action_selected_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_background_row_activated_cb);
}

static void
bs_touchscreen_slot_editor_init (BsTouchscreenSlotEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
bs_touchscreen_slot_editor_new (BsTouchscreenSlot *slot)
{
  return g_object_new (BS_TYPE_TOUCHSCREEN_SLOT_EDITOR,
                       "touchscreen-slot", slot,
                       NULL);
}
