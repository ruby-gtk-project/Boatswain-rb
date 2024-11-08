/*
 * bs-action-selector.c
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

#define G_LOG_DOMAIN "Action Selector"

#include "bs-action-selector.h"

#include "bs-action.h"
#include "bs-action-factory.h"
#include "bs-action-info.h"
#include "bs-application-private.h"
#include "bs-debug.h"

struct _BsActionSelector
{
  AdwBin parent_instance;

  GtkListBox *actions_listbox;
};

static void on_action_row_activated_cb (GtkListBoxRow    *row,
                                        BsActionSelector *self);

G_DEFINE_FINAL_TYPE (BsActionSelector, bs_action_selector, ADW_TYPE_BIN)

enum
{
  ACTION_SELECTED,
  N_SIGNALS,
};

static guint signals[N_SIGNALS];

/*
 * Auxiliary methods
 */

static void
add_action_factory (BsActionSelector *self,
                    BsActionFactory  *action_factory)
{
  PeasPluginInfo *plugin_info;
  GtkWidget *expander_row;
  GtkWidget *image;

  plugin_info = peas_extension_base_get_plugin_info (PEAS_EXTENSION_BASE (action_factory));

  expander_row = adw_expander_row_new ();
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (expander_row),
                                 peas_plugin_info_get_name (plugin_info));

  image = gtk_image_new_from_icon_name (peas_plugin_info_get_icon_name (plugin_info));
  adw_expander_row_add_prefix (ADW_EXPANDER_ROW (expander_row), image);

  gtk_list_box_append (self->actions_listbox, expander_row);

  for (uint32_t i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (action_factory)); i++)
    {
      g_autoptr (BsActionInfo) info = NULL;
      GtkWidget *image;
      GtkWidget *row;

      info = g_list_model_get_item (G_LIST_MODEL (action_factory), i);

      if (bs_action_info_get_hidden (info))
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


/*
 * Callbacks
 */

static void
on_action_row_activated_cb (GtkListBoxRow    *row,
                            BsActionSelector *self)
{
  BsActionFactory *factory;
  BsActionInfo *action_info;

  factory = g_object_get_data (G_OBJECT (row), "factory");
  action_info = g_object_get_data (G_OBJECT (row), "action-info");

  /* Collapse all expander rows */
  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->actions_listbox));
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      adw_expander_row_set_expanded (ADW_EXPANDER_ROW (child), FALSE);
    }

  g_signal_emit (self, signals[ACTION_SELECTED], 0, factory, action_info);
}

static void
on_action_factory_added_cb (PeasExtensionSet *extension_set,
                            PeasPluginInfo   *plugin_info,
                            GObject          *extension,
                            gpointer          user_data)
{
  BsActionSelector *self = BS_ACTION_SELECTOR (user_data);

  add_action_factory (self, BS_ACTION_FACTORY (extension));
}

static void
on_action_factory_removed_cb (PeasExtensionSet *extension_set,
                              PeasPluginInfo   *plugin_info,
                              GObject          *extension,
                              gpointer          user_data)
{
}


/*
 * GObject overrides
 */

static void
bs_action_selector_dispose (GObject *object)
{
  BsActionSelector *self = (BsActionSelector *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), BS_TYPE_ACTION_SELECTOR);

  G_OBJECT_CLASS (bs_action_selector_parent_class)->dispose (object);
}

static void
bs_action_selector_class_init (BsActionSelectorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = bs_action_selector_dispose;

  signals[ACTION_SELECTED] = g_signal_new ("action-selected",
                                           BS_TYPE_ACTION_SELECTOR,
                                           G_SIGNAL_RUN_LAST,
                                           0, NULL, NULL, NULL,
                                           G_TYPE_NONE,
                                           2,
                                           BS_TYPE_ACTION_FACTORY,
                                           BS_TYPE_ACTION_INFO);

  gtk_widget_class_set_template_from_resource (widget_class, "/com/feaneron/Boatswain/bs-action-selector.ui");

  gtk_widget_class_bind_template_child (widget_class, BsActionSelector, actions_listbox);
}

static void
bs_action_selector_init (BsActionSelector *self)
{
  PeasExtensionSet *extension_set;
  GApplication *application;

  gtk_widget_init_template (GTK_WIDGET (self));

  application = g_application_get_default ();
  extension_set = bs_application_get_action_factory_set (BS_APPLICATION (application));

  peas_extension_set_foreach (extension_set,
                              (PeasExtensionSetForeachFunc) on_action_factory_added_cb,
                              self);

  g_signal_connect (extension_set, "extension-added", G_CALLBACK (on_action_factory_added_cb), self);
  g_signal_connect (extension_set, "extension-removed", G_CALLBACK (on_action_factory_removed_cb), self);
}
