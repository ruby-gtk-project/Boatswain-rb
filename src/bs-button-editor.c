/* bs-button-editor.c
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

#include "bs-action-selector.h"
#include "bs-button-editor.h"

#include <glib/gi18n.h>

struct _BsButtonEditor
{
  AdwBin parent_instance;

  AdwPreferencesGroup *action_preferences_group;
  GtkColorDialogButton *background_color_dialog_button;
  AdwPreferencesPage *button_preferences_page;
  GtkMenuButton *custom_icon_menubutton;
  GtkEditable *custom_icon_text_row;
  GtkImage *icon_image;
  GtkFilterListModel *icons_filter_list_model;
  AdwNavigationView *navigation_view;
  GtkWidget *remove_group;
  GtkWidget *remove_custom_icon_button;

  BsButton *button;
  GtkWidget *action_preferences;

  gulong action_changed_id;
  gulong custom_icon_changed_id;
  gulong icon_changed_id;
};

static void on_background_color_dialog_button_rgba_changed_cb (GtkColorDialogButton *button,
                                                               GParamSpec           *pspec,
                                                               BsButtonEditor       *self);

static void on_custom_icon_text_row_text_changed_cb (GtkEditable    *entry,
                                                     GParamSpec     *pspec,
                                                     BsButtonEditor *self);

G_DEFINE_FINAL_TYPE (BsButtonEditor, bs_button_editor, ADW_TYPE_BIN)

enum
{
  PROP_0,
  PROP_BUTTON,
  N_PROPS,
};

static GParamSpec *properties [N_PROPS];


/*
 * Auxiliary methods
 */

static void
setup_button (BsButtonEditor *self)
{
  BsDevice *device;
  BsIcon *custom_icon;

  device = bs_button_get_device (self->button);
  custom_icon = bs_button_get_custom_icon (self->button);

  gtk_widget_set_sensitive (GTK_WIDGET (self),
                            bs_button_get_position (self->button) != 0 ||
                            bs_page_is_root (bs_device_get_active_page (device)));

  gtk_widget_set_visible (self->remove_custom_icon_button, custom_icon != NULL);

  g_signal_handlers_block_by_func (self->custom_icon_text_row,
                                   on_custom_icon_text_row_text_changed_cb,
                                   self);
  g_signal_handlers_block_by_func (self->background_color_dialog_button,
                                   on_background_color_dialog_button_rgba_changed_cb,
                                   self);

  if (custom_icon)
    {
      const char *icon_text = bs_icon_get_text (custom_icon);

      if (g_strcmp0 (gtk_editable_get_text (self->custom_icon_text_row), icon_text) != 0)
        gtk_editable_set_text (self->custom_icon_text_row, icon_text ?: "");

      gtk_color_dialog_button_set_rgba (self->background_color_dialog_button,
                                        bs_icon_get_background_color (custom_icon));
    }
  else
    {
      gtk_color_dialog_button_set_rgba (self->background_color_dialog_button,
                                        &(GdkRGBA) { 0.0, 0.0, 0.0, 0.0 });
      gtk_editable_set_text (self->custom_icon_text_row, "");
    }

  g_signal_handlers_unblock_by_func (self->background_color_dialog_button,
                                     on_background_color_dialog_button_rgba_changed_cb,
                                     self);
  g_signal_handlers_unblock_by_func (self->custom_icon_text_row,
                                     on_custom_icon_text_row_text_changed_cb,
                                     self);
}

static void
update_action_preferences_group (BsButtonEditor *self)
{
  GtkWidget *action_preferences;
  BsAction *action;

  action = bs_actionable_get_action (BS_ACTIONABLE (self->button));
  action_preferences = action ? bs_action_get_preferences (action) : NULL;

  gtk_widget_set_visible (self->remove_group,
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

static void
update_icon (BsButtonEditor *self)
{
  BsIcon *icon = bs_button_get_icon (self->button);

  gtk_image_set_from_paintable (self->icon_image, GDK_PAINTABLE (icon));
}

static void
maybe_remove_custom_icon (BsButtonEditor *self)
{
  BsIcon *custom_icon = bs_button_get_custom_icon (self->button);
  GdkRGBA transparent = { 0.0, 0.0, 0.0, 0.0 };
  const char *text;

  if (!custom_icon)
    return;

  text = bs_icon_get_text (custom_icon);

  if (gdk_rgba_equal (bs_icon_get_background_color (custom_icon), &transparent) &&
      !bs_icon_get_file (custom_icon) &&
      !bs_icon_get_icon_name (custom_icon) &&
      !bs_icon_get_paintable (custom_icon) &&
      (!text || strlen (text) == 0))
    {
      bs_button_set_custom_icon (self->button, NULL);
    }

}


/*
 * Callbacks
 */

static void
on_action_selector_action_selected_cb (BsActionSelector *selector,
                                       BsActionFactory  *factory,
                                       BsActionInfo     *action_info,
                                       BsButtonEditor   *self)
{
  g_autoptr (BsAction) new_action = NULL;
  g_autoptr (BsIcon) new_custom_icon = NULL;
  g_autoptr (GError) error = NULL;
  PeasPluginInfo *plugin_info;
  BsDeviceRegion *region;
  BsPageItem *item;
  BsDevice *device;
  BsIcon *custom_icon;
  BsPage *active_page;
  const char *region_id;
  size_t position;

  device = bs_button_get_device (self->button);
  active_page = bs_device_get_active_page (device);
  plugin_info = peas_extension_base_get_plugin_info (PEAS_EXTENSION_BASE (factory));
  region = bs_button_get_region (self->button);
  region_id = bs_device_region_get_id (region);

  position = bs_button_get_position (self->button);
  item = bs_page_get_item (active_page, region_id, position);
  bs_page_item_set_item_type (item, BS_PAGE_ITEM_ACTION);
  bs_page_item_set_factory (item, peas_plugin_info_get_module_name (plugin_info));
  bs_page_item_set_action (item, bs_action_info_get_id (action_info));

  custom_icon = bs_button_get_custom_icon (self->button);
  if (custom_icon)
    bs_page_item_set_custom_icon (item, bs_icon_to_json (custom_icon));

  bs_page_realize (active_page, region_id, position, &new_custom_icon, &new_action, &error);

  if (error)
    g_warning ("Error realizing action: %s", error->message);

  bs_actionable_set_action (BS_ACTIONABLE (self->button), new_action);
  bs_button_set_custom_icon (self->button, new_custom_icon);
  update_action_preferences_group (self);

  adw_navigation_view_pop (self->navigation_view);
}

static void
on_action_changed_cb (BsButton       *button,
                      GParamSpec     *pspec,
                      BsButtonEditor *self)
{
  update_action_preferences_group (self);
  setup_button (self);
  update_icon (self);
}

static void
on_background_color_dialog_button_rgba_changed_cb (GtkColorDialogButton *button,
                                                   GParamSpec           *pspec,
                                                   BsButtonEditor       *self)
{
  g_autoptr (BsIcon) icon = NULL;
  const GdkRGBA *background_color;

  icon = bs_button_get_custom_icon (self->button);
  if (!icon)
    icon = bs_icon_new_empty ();
  else
    g_object_ref (icon);
  background_color = gtk_color_dialog_button_get_rgba (button);

  bs_icon_set_background_color (icon, background_color);
  bs_button_set_custom_icon (self->button, icon);
}

static void
on_button_custom_icon_changed_cb (BsButton       *button,
                                  GParamSpec     *pspec,
                                  BsButtonEditor *self)
{
  setup_button (self);
}

static void
on_button_icon_changed_cb (BsButton       *button,
                           BsIcon         *icon,
                           BsButtonEditor *self)
{
  setup_button (self);
  update_icon (self);
}


static void
on_file_dialog_file_opened_cb (GObject      *source,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  BsButtonEditor *self;
  g_autoptr (GError) error = NULL;
  g_autoptr (BsIcon) icon = NULL;
  g_autoptr (GFile) file = NULL;

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

  self = BS_BUTTON_EDITOR (user_data);
  icon = bs_button_get_custom_icon (self->button);

  if (!icon)
    icon = bs_icon_new_empty ();
  else
    g_object_ref (icon);

  bs_icon_set_file (icon, file, &error);
  if (error)
    {
      g_warning ("Error setting custom icon: %s", error->message);
      return;
    }

  bs_button_set_custom_icon (self->button, icon);
}

static void
on_custom_icon_button_clicked_cb (AdwPreferencesRow *row,
                                  BsButtonEditor    *self)
{
  g_autoptr (GtkFileDialog) dialog = NULL;
  g_autoptr (GtkFileFilter) filter = NULL;
  g_autoptr (GListStore) filters = NULL;

  gtk_menu_button_popdown (self->custom_icon_menubutton);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("All supported formats"));
  gtk_file_filter_add_mime_type (filter, "image/*");
  gtk_file_filter_add_mime_type (filter, "video/*");

  filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  g_list_store_append (filters, filter);

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_modal (dialog, TRUE);
  gtk_file_dialog_set_title (dialog, _("Select icon"));
  gtk_file_dialog_set_accept_label (dialog, _("Open"));
  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));

  gtk_file_dialog_open (dialog,
                        GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self))),
                        NULL,
                        on_file_dialog_file_opened_cb,
                        self);
}

static void
on_custom_icon_text_row_text_changed_cb (GtkEditable    *row,
                                         GParamSpec     *pspec,
                                         BsButtonEditor *self)
{
  g_autoptr (BsIcon) custom_icon = NULL;
  const char *text;

  text = gtk_editable_get_text (row);
  custom_icon = bs_button_get_custom_icon (self->button);

  if (custom_icon)
    g_object_ref (custom_icon);

  if (strlen (text) > 0)
    {
      if (!custom_icon)
        custom_icon = bs_icon_new_empty ();
      else
        g_object_ref (custom_icon);
      bs_icon_set_text (custom_icon, text);
      bs_button_set_custom_icon (self->button, custom_icon);
    }
  else
    {
      if (custom_icon)
        bs_icon_set_text (custom_icon, text);
      maybe_remove_custom_icon (self);
    }
}

static void
on_icons_gridview_activate_cb (GtkGridView    *grid_view,
                               unsigned int    position,
                               BsButtonEditor *self)
{
  g_autoptr (GtkStringObject) string_object = NULL;
  g_autoptr (BsIcon) custom_icon = NULL;
  GtkSelectionModel *selection_model;

  selection_model = gtk_grid_view_get_model (grid_view);
  string_object = g_list_model_get_item (G_LIST_MODEL (selection_model), position);

  custom_icon = bs_button_get_custom_icon (self->button);
  if (!custom_icon)
    custom_icon = bs_icon_new_empty ();
  else
    g_object_ref (custom_icon);

  bs_icon_set_file (custom_icon, NULL, NULL);
  bs_icon_set_paintable (custom_icon, NULL);
  bs_icon_set_icon_name (custom_icon, gtk_string_object_get_string (string_object));
  bs_button_set_custom_icon (self->button, custom_icon);
}

static void
on_remove_row_activated_cb (GtkButton      *button,
                            BsButtonEditor *self)
{
  g_autoptr (BsAction) empty_action = NULL;

  empty_action = bs_empty_action_new ();
  bs_actionable_set_action (BS_ACTIONABLE (self->button), empty_action);
}

static void
on_remove_custom_icon_button_clicked_cb (GtkButton      *button,
                                         BsButtonEditor *self)
{
  bs_button_set_custom_icon (self->button, NULL);
  gtk_color_dialog_button_set_rgba (self->background_color_dialog_button,
                                    &(GdkRGBA) { 0.0, 0.0, 0.0, 0.0 });
}


/*
 * GObject overrides
 */

static void
bs_button_editor_finalize (GObject *object)
{
  BsButtonEditor *self = (BsButtonEditor *)object;

  g_clear_signal_handler (&self->action_changed_id, self->button);
  g_clear_signal_handler (&self->custom_icon_changed_id, self->button);
  g_clear_signal_handler (&self->icon_changed_id, self->button);
  g_clear_object (&self->button);

  G_OBJECT_CLASS (bs_button_editor_parent_class)->finalize (object);
}

static void
bs_button_editor_constructed (GObject *object)
{
  BsButtonEditor *self = (BsButtonEditor *)object;

  G_OBJECT_CLASS (bs_button_editor_parent_class)->constructed (object);

  update_action_preferences_group (self);
  setup_button (self);
  update_icon (self);

  self->action_changed_id = g_signal_connect (self->button,
                                              "notify::action",
                                              G_CALLBACK (on_action_changed_cb),
                                              self);

  self->custom_icon_changed_id = g_signal_connect (self->button,
                                                   "notify::custom-icon",
                                                   G_CALLBACK (on_button_custom_icon_changed_cb),
                                                   self);

  self->icon_changed_id = g_signal_connect (self->button,
                                            "icon-changed",
                                            G_CALLBACK (on_button_icon_changed_cb),
                                            self);
}

static void
bs_button_editor_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  BsButtonEditor *self = BS_BUTTON_EDITOR (object);

  switch (prop_id)
    {
    case PROP_BUTTON:
      g_value_set_object (value, self->button);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_button_editor_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  BsButtonEditor *self = BS_BUTTON_EDITOR (object);

  switch (prop_id)
    {
    case PROP_BUTTON:
      g_assert (self->button == NULL);
      self->button = g_value_dup_object (value);
      g_assert (self->button != NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_button_editor_class_init (BsButtonEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = bs_button_editor_finalize;
  object_class->constructed = bs_button_editor_constructed;
  object_class->get_property = bs_button_editor_get_property;
  object_class->set_property = bs_button_editor_set_property;

  properties[PROP_BUTTON] = g_param_spec_object ("button", NULL, NULL,
                                                 BS_TYPE_BUTTON,
                                                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/com/feaneron/Boatswain/bs-button-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, BsButtonEditor, action_preferences_group);
  gtk_widget_class_bind_template_child (widget_class, BsButtonEditor, background_color_dialog_button);
  gtk_widget_class_bind_template_child (widget_class, BsButtonEditor, button_preferences_page);
  gtk_widget_class_bind_template_child (widget_class, BsButtonEditor, custom_icon_menubutton);
  gtk_widget_class_bind_template_child (widget_class, BsButtonEditor, custom_icon_text_row);
  gtk_widget_class_bind_template_child (widget_class, BsButtonEditor, icon_image);
  gtk_widget_class_bind_template_child (widget_class, BsButtonEditor, icons_filter_list_model);
  gtk_widget_class_bind_template_child (widget_class, BsButtonEditor, navigation_view);
  gtk_widget_class_bind_template_child (widget_class, BsButtonEditor, remove_group);
  gtk_widget_class_bind_template_child (widget_class, BsButtonEditor, remove_custom_icon_button);

  gtk_widget_class_bind_template_callback (widget_class, on_action_selector_action_selected_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_background_color_dialog_button_rgba_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_custom_icon_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_custom_icon_text_row_text_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_icons_gridview_activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_remove_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_remove_custom_icon_button_clicked_cb);

  gtk_widget_class_set_css_name (widget_class, "streamdeckbuttoneditor");
}

static void
bs_button_editor_init (BsButtonEditor *self)
{
  static GtkStringList *builtin_icons_list = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  if (g_once_init_enter_pointer (&builtin_icons_list))
    {
      g_autoptr (GtkStringList) list = NULL;
      g_auto (GStrv) icon_names = NULL;
      GtkIconTheme *icon_theme;

      list = gtk_string_list_new (NULL);
      icon_theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
      icon_names = gtk_icon_theme_get_icon_names (icon_theme);

      for (size_t i = 0; icon_names && icon_names[i]; i++)
        {
          if (g_str_has_suffix (icon_names[i], "-symbolic"))
            gtk_string_list_append (list, icon_names[i]);
        }

      g_once_init_leave_pointer (&builtin_icons_list, g_steal_pointer (&list));
    }

  gtk_filter_list_model_set_model (self->icons_filter_list_model, G_LIST_MODEL (builtin_icons_list));
}

GtkWidget *
bs_button_editor_new (BsButton *button)
{
  g_assert (BS_IS_BUTTON (button));

  return g_object_new (BS_TYPE_BUTTON_EDITOR,
                       "button", button,
                       NULL);
}
