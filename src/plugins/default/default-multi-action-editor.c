/* default-multi-action-editor.c
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

#include "bs-action-factory.h"
#include "bs-action-info.h"
#include "bs-action-private.h"
#include "bs-application-private.h"
#include "bs-stream-deck.h"
#include "default-multi-action-editor.h"
#include "default-multi-action-private.h"
#include "default-multi-action-row.h"

#include <glib/gi18n.h>

#define DEFAULT_DELAY_MS 250

struct _DefaultMultiActionEditor
{
  GtkBox parent_instance;

  GtkWindow *actions_dialog;
  AdwPreferencesGroup *actions_group;
  GtkWidget *entries_group;
  GtkListBox *entries_listbox;
  AdwPreferencesGroup *others_group;
  AdwBin *preferences_bin;
  GtkWindow *preferences_dialog;

  DefaultMultiAction *multi_action;
};

static void on_entry_row_changed_cb (DefaultMultiActionRow    *entry_row,
                                     DefaultMultiActionEditor *self);

static void on_entry_row_edit_cb (DefaultMultiActionRow    *entry_row,
                                  DefaultMultiActionEditor *self);

static void on_entry_row_remove_cb (DefaultMultiActionRow    *entry_row,
                                    DefaultMultiActionEditor *self);

G_DEFINE_FINAL_TYPE (DefaultMultiActionEditor, default_multi_action_editor, GTK_TYPE_BOX)

enum
{
  PROP_0,
  PROP_MULTI_ACTION,
  N_PROPS,
};

static GParamSpec *properties [N_PROPS];


/*
 * Auxiliary methods
 */

static gboolean
can_add_action (const PeasPluginInfo *plugin_info,
                BsActionInfo         *info)
{
  const struct {
    const char *factory;
    const char *id;
  } denylist[] = {
    { "default", "default-multi-action" },
    { "default", "default-switch-page-action" },
  };
  size_t i;

  for (i = 0; i < G_N_ELEMENTS (denylist); i++)
    {
      const char *module_name = peas_plugin_info_get_module_name (plugin_info);
      const char *id = bs_action_info_get_id (info);

      if (g_strcmp0 (module_name, denylist[i].factory) == 0 && g_strcmp0 (id, denylist[i].id) == 0)
        return FALSE;
    }

  return TRUE;
}

static void
recreate_entry_rows (DefaultMultiActionEditor *self)
{
  GPtrArray *entries;
  GtkWidget *child;
  unsigned int i;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->entries_listbox))) != NULL)
    gtk_list_box_remove (self->entries_listbox, child);

  entries = default_multi_action_get_entries (self->multi_action);

  for (i = 0; i < entries->len; i++)
    {
      MultiActionEntry *entry;
      GtkWidget *row;

      entry = g_ptr_array_index (entries, i);
      row = default_multi_action_row_new (entry);
      g_signal_connect (row, "changed", G_CALLBACK (on_entry_row_changed_cb), self);
      g_signal_connect (row, "edit", G_CALLBACK (on_entry_row_edit_cb), self);
      g_signal_connect (row, "remove", G_CALLBACK (on_entry_row_remove_cb), self);

      gtk_list_box_append (self->entries_listbox, row);
    }

  gtk_widget_set_visible (self->entries_group, entries->len > 0);
}


/*
 * Callbacks
 */

static void
on_action_row_activated_cb (AdwActionRow             *row,
                            DefaultMultiActionEditor *self)
{
  g_autoptr (BsAction) new_action = NULL;
  BsActionFactory *factory;
  BsActionInfo *action_info;

  factory = g_object_get_data (G_OBJECT (row), "factory");
  action_info = g_object_get_data (G_OBJECT (row), "action-info");

  new_action = bs_action_factory_create_action (factory, action_info);

  default_multi_action_add_action (self->multi_action, new_action);
  recreate_entry_rows (self);

  gtk_window_close (self->actions_dialog);
}

static void
on_action_factory_added_cb (PeasExtensionSet *extension_set,
                            PeasPluginInfo   *plugin_info,
                            GObject          *extension,
                            gpointer          user_data)
{
  DefaultMultiActionEditor *self;
  BsActionFactory *action_factory;
  GtkWidget *expander_row;
  GtkWidget *image;

  self = DEFAULT_MULTI_ACTION_EDITOR (user_data);
  action_factory = BS_ACTION_FACTORY (extension);

  plugin_info = peas_extension_base_get_plugin_info (PEAS_EXTENSION_BASE (extension));

  expander_row = adw_expander_row_new ();
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (expander_row),
                                 peas_plugin_info_get_name (plugin_info));

  image = gtk_image_new_from_icon_name (peas_plugin_info_get_icon_name (plugin_info));
  adw_expander_row_add_prefix (ADW_EXPANDER_ROW (expander_row), image);

  adw_preferences_group_add (self->actions_group, expander_row);

  for (uint32_t i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (action_factory)); i++)
    {
      g_autoptr (BsActionInfo) info = NULL;
      GtkWidget *image;
      GtkWidget *row;

      info = g_list_model_get_item (G_LIST_MODEL (action_factory), i);

      if (!can_add_action (plugin_info, info))
        continue;

      row = adw_action_row_new ();
      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), bs_action_info_get_name (info));
      adw_action_row_set_subtitle (ADW_ACTION_ROW (row), bs_action_info_get_description (info));
      gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE);
      g_object_set_data (G_OBJECT (row), "factory", action_factory);
      g_object_set_data (G_OBJECT (row), "action-info", (gpointer) info);
      g_object_set_data (G_OBJECT (row), "plugin-info", (gpointer) plugin_info);
      g_signal_connect (row, "activated", G_CALLBACK (on_action_row_activated_cb), self);

      image = gtk_image_new_from_icon_name (bs_action_info_get_icon_name (info));
      adw_action_row_add_prefix (ADW_ACTION_ROW (row), image);

      adw_expander_row_add_row (ADW_EXPANDER_ROW (expander_row), row);
    }
}

static void
on_add_action_row_activated_cb (AdwActionRow             *add_action_row,
                                DefaultMultiActionEditor *self)
{
  GtkWidget *window;

  window = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_WINDOW);

  gtk_window_set_transient_for (self->actions_dialog, GTK_WINDOW (window));
  gtk_window_present (self->actions_dialog);
}

static void
on_delay_row_activated_cb (AdwActionRow             *delay_row,
                           DefaultMultiActionEditor *self)
{
  default_multi_action_add_delay (self->multi_action, DEFAULT_DELAY_MS);
  recreate_entry_rows (self);
  gtk_window_close (self->actions_dialog);
}

static void
on_entry_row_changed_cb (DefaultMultiActionRow    *entry_row,
                         DefaultMultiActionEditor *self)
{
  bs_action_changed (BS_ACTION (self->multi_action));
}

static void
on_entry_row_edit_cb (DefaultMultiActionRow    *entry_row,
                      DefaultMultiActionEditor *self)
{
  MultiActionEntry *entry;
  GtkWidget *preferences;
  GtkWidget *window;

  entry = default_multi_action_row_get_entry (entry_row);
  g_assert (entry->entry_type == MULTI_ACTION_ENTRY_ACTION);

  preferences = bs_action_get_preferences (entry->v.action);
  adw_bin_set_child (self->preferences_bin, preferences);

  window = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_WINDOW);
  gtk_window_set_transient_for (self->preferences_dialog, GTK_WINDOW (window));

  gtk_window_present (self->preferences_dialog);
}

static void
on_entry_row_remove_cb (DefaultMultiActionRow    *entry_row,
                        DefaultMultiActionEditor *self)
{
  GPtrArray *entries;
  unsigned int position;

  position = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (entry_row));
  entries = default_multi_action_get_entries (self->multi_action);
  g_ptr_array_remove_index (entries, position);

  gtk_list_box_remove (self->entries_listbox, GTK_WIDGET (entry_row));

  bs_action_changed (BS_ACTION (self->multi_action));
}

static gboolean
on_preferences_dialog_close_request_cb (GtkDialog                *preferences_dialog,
                                        DefaultMultiActionEditor *self)
{
  adw_bin_set_child (self->preferences_bin, NULL);
  return FALSE;
}


/*
 * GObject overrides
 */

static void
default_multi_action_editor_finalize (GObject *object)
{
  G_OBJECT_CLASS (default_multi_action_editor_parent_class)->finalize (object);
}

static void
default_multi_action_editor_constructed (GObject *object)
{
  DefaultMultiActionEditor *self = (DefaultMultiActionEditor *)object;
  PeasExtensionSet *extension_set;
  GApplication *application;
  GtkWidget *row;

  G_OBJECT_CLASS (default_multi_action_editor_parent_class)->constructed (object);

  /* Actions */
  application = g_application_get_default ();
  extension_set = bs_application_get_action_factory_set (BS_APPLICATION (application));

  peas_extension_set_foreach (extension_set,
                              (PeasExtensionSetForeachFunc) on_action_factory_added_cb,
                              self);

  /* Delay */
  row = adw_action_row_new ();
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), _("Delay"));
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE);
  g_signal_connect (row, "activated", G_CALLBACK (on_delay_row_activated_cb), self);

  adw_action_row_add_prefix (ADW_ACTION_ROW (row),
                             gtk_image_new_from_icon_name ("preferences-system-time-symbolic"));

  adw_preferences_group_add (self->others_group, row);

  recreate_entry_rows (self);
}

static void
default_multi_action_editor_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  DefaultMultiActionEditor *self = DEFAULT_MULTI_ACTION_EDITOR (object);

  switch (prop_id)
    {
    case PROP_MULTI_ACTION:
      g_value_set_object (value, self->multi_action);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
default_multi_action_editor_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  DefaultMultiActionEditor *self = DEFAULT_MULTI_ACTION_EDITOR (object);

  switch (prop_id)
    {
    case PROP_MULTI_ACTION:
      g_assert (self->multi_action == NULL);
      self->multi_action = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
default_multi_action_editor_class_init (DefaultMultiActionEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = default_multi_action_editor_finalize;
  object_class->constructed = default_multi_action_editor_constructed;
  object_class->get_property = default_multi_action_editor_get_property;
  object_class->set_property = default_multi_action_editor_set_property;

  properties[PROP_MULTI_ACTION] = g_param_spec_object ("multi-action", NULL, NULL,
                                                       DEFAULT_TYPE_MULTI_ACTION,
                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/com/feaneron/Boatswain/plugins/default/default-multi-action-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, DefaultMultiActionEditor, actions_dialog);
  gtk_widget_class_bind_template_child (widget_class, DefaultMultiActionEditor, actions_group);
  gtk_widget_class_bind_template_child (widget_class, DefaultMultiActionEditor, entries_group);
  gtk_widget_class_bind_template_child (widget_class, DefaultMultiActionEditor, entries_listbox);
  gtk_widget_class_bind_template_child (widget_class, DefaultMultiActionEditor, others_group);
  gtk_widget_class_bind_template_child (widget_class, DefaultMultiActionEditor, preferences_bin);
  gtk_widget_class_bind_template_child (widget_class, DefaultMultiActionEditor, preferences_dialog);

  gtk_widget_class_bind_template_callback (widget_class, on_add_action_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_preferences_dialog_close_request_cb);
}

static void
default_multi_action_editor_init (DefaultMultiActionEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
default_multi_action_editor_new (DefaultMultiAction *multi_action)
{
  return g_object_new (DEFAULT_TYPE_MULTI_ACTION_EDITOR,
                       "multi-action", multi_action,
                       NULL);
}

void
default_multi_action_editor_update_entries (DefaultMultiActionEditor *self)
{
  g_return_if_fail (DEFAULT_IS_MULTI_ACTION_EDITOR (self));

  recreate_entry_rows (self);
}
