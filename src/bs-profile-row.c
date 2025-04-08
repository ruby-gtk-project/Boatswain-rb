/* bs-profile-row.c
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

#include "bs-profile-row.h"

struct _BsProfileRow
{
  AdwPreferencesRow parent_instance;

  GtkMenuButton *menu_button;
  GtkWidget *rename_button;
  GtkEditable *rename_entry;
  GtkWidget *selected_icon;

  GSimpleActionGroup *action_group;

  BsProfile *profile;
  BsDevice *device;
};

G_DEFINE_FINAL_TYPE (BsProfileRow, bs_profile_row, ADW_TYPE_PREFERENCES_ROW)

enum
{
  PROP_0,
  PROP_PROFILE,
  PROP_STREAM_DECK,
  N_PROPS
};

enum
{
  MOVE,
  N_SIGNALS,
};

static guint signals [N_SIGNALS];
static GParamSpec *properties [N_PROPS];


/*
 * Auxiliary methods
 */

static void
update_actions (BsProfileRow *self)
{
  GListModel *profiles;
  GAction *action;
  unsigned int position;

  profiles = bs_device_get_profiles (self->device);
  action = g_action_map_lookup_action (G_ACTION_MAP (self->action_group), "delete");

  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), g_list_model_get_n_items (profiles) > 1);

  g_list_store_find (G_LIST_STORE (profiles), self->profile, &position);

  action = g_action_map_lookup_action (G_ACTION_MAP (self->action_group), "move-up");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), position > 0);

  action = g_action_map_lookup_action (G_ACTION_MAP (self->action_group), "move-down");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), position + 1 < g_list_model_get_n_items (profiles));
}

static void
rename_profile (BsProfileRow *self)
{
  g_autofree char *new_name = NULL;

  if (!gtk_widget_get_sensitive (self->rename_button))
    return;

  new_name = g_strdup (gtk_editable_get_text (self->rename_entry));
  bs_profile_set_name (self->profile, g_strstrip (new_name));

  gtk_menu_button_popdown (self->menu_button);
}

static void
update_selected_icon_visibility (BsProfileRow *self)
{
  gboolean visible;

  visible = bs_device_get_active_profile (self->device) == self->profile;
  gtk_widget_set_visible (self->selected_icon, visible);
}

static void
validate_rename_entry (BsProfileRow *self)
{
  g_autofree char *new_name = NULL;
  gboolean valid;

  new_name = g_strdup (gtk_editable_get_text (self->rename_entry));
  valid = new_name != NULL &&
          g_utf8_strlen (g_strstrip (new_name), -1) > 0 &&
          g_strcmp0 (new_name, bs_profile_get_name (self->profile)) != 0;

  gtk_widget_set_sensitive (self->rename_button, valid);
}


/*
 * Callbacks
 */

static void
on_profiles_items_changed_cb (GListModel   *list,
                              unsigned int  position,
                              unsigned int  removed,
                              unsigned int  added,
                              BsProfileRow *self)
{
  update_actions (self);
}

static void
on_delete_action_activated_cb (GSimpleAction *simple,
                               GVariant      *parameter,
                               gpointer       user_data)
{
  g_autoptr (BsProfile) new_active_profile = NULL;
  BsProfileRow *self;
  GListModel *profiles;
  unsigned int position;

  self = BS_PROFILE_ROW (user_data);
  profiles = bs_device_get_profiles (self->device);

  if (!g_list_store_find (G_LIST_STORE (profiles), self->profile, &position))
    return;

  if (bs_device_get_active_profile (self->device) == self->profile)
    {
      new_active_profile = g_list_model_get_item (profiles, position + 1);

      if (!new_active_profile)
        new_active_profile = g_list_model_get_item (profiles, position - 1);

      if (new_active_profile)
        bs_device_load_profile (self->device, new_active_profile);
    }

  g_signal_handlers_block_by_func (profiles, on_profiles_items_changed_cb, self);
  g_list_store_remove (G_LIST_STORE (profiles), position);
  g_signal_handlers_unblock_by_func (profiles, on_profiles_items_changed_cb, self);
}

static void
on_move_down_action_activated_cb (GSimpleAction *simple,
                                  GVariant      *parameter,
                                  gpointer       user_data)
{
  BsProfileRow *self = BS_PROFILE_ROW (user_data);
  unsigned int position;

  position = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (self));

  g_signal_emit (self, signals[MOVE], 0, position + 1);
}

static void
on_move_up_action_activated_cb (GSimpleAction *simple,
                                GVariant      *parameter,
                                gpointer       user_data)
{
  BsProfileRow *self = BS_PROFILE_ROW (user_data);
  unsigned int position;

  position = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (self));

  g_signal_emit (self, signals[MOVE], 0, position - 1);
}

static void
on_rename_button_clicked_cb (GtkButton    *button,
                             BsProfileRow *self)
{
  rename_profile (self);
}

static void
on_rename_entry_activate_cb (GtkEntry     *entry,
                             BsProfileRow *self)
{
  rename_profile (self);
}

static void
on_rename_entry_text_changed_cb (GtkEntry     *entry,
                                 GParamSpec   *pspec,
                                 BsProfileRow *self)
{
  validate_rename_entry (self);
}

static void
on_device_active_profile_changed_cb (BsDevice *device,
                                          GParamSpec   *pspec,
                                          BsProfileRow *self)
{
  update_selected_icon_visibility (self);
}



/*
 * GObject overrides
 */

static void
bs_profile_row_constructed (GObject *object)
{
  BsProfileRow *self = (BsProfileRow *)object;
  GListModel *profiles;

  G_OBJECT_CLASS (bs_profile_row_parent_class)->constructed (object);

  g_object_bind_property (self->profile, "name", self, "title", G_BINDING_SYNC_CREATE);
  g_object_bind_property (self->profile, "name", self->rename_entry, "text", G_BINDING_SYNC_CREATE);

  profiles = bs_device_get_profiles (self->device);
  g_signal_connect_object (profiles, "items-changed", G_CALLBACK (on_profiles_items_changed_cb), self, 0);

  g_signal_connect_object (self->device,
                           "notify::active-profile",
                           G_CALLBACK (on_device_active_profile_changed_cb),
                           self,
                           0);

  update_actions (self);
  update_selected_icon_visibility (self);
}

static void
bs_profile_row_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  BsProfileRow *self = BS_PROFILE_ROW (object);

  switch (prop_id)
    {
    case PROP_PROFILE:
      g_value_set_object (value, self->profile);
      break;

    case PROP_STREAM_DECK:
      g_value_set_object (value, self->device);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_profile_row_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  BsProfileRow *self = BS_PROFILE_ROW (object);

  switch (prop_id)
    {
    case PROP_PROFILE:
      g_assert (self->profile == NULL);
      self->profile = g_value_get_object (value);
      break;

    case PROP_STREAM_DECK:
      g_assert (self->device == NULL);
      self->device = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_profile_row_class_init (BsProfileRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = bs_profile_row_constructed;
  object_class->get_property = bs_profile_row_get_property;
  object_class->set_property = bs_profile_row_set_property;

  properties[PROP_PROFILE] = g_param_spec_object ("profile", NULL, NULL,
                                                  BS_TYPE_PROFILE,
                                                  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_STREAM_DECK] = g_param_spec_object ("stream-deck", NULL, NULL,
                                                      BS_TYPE_DEVICE,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[MOVE] = g_signal_new ("move",
                                BS_TYPE_PROFILE_ROW,
                                G_SIGNAL_RUN_LAST,
                                0, NULL, NULL, NULL,
                                G_TYPE_NONE,
                                1,
                                G_TYPE_UINT);

  gtk_widget_class_set_template_from_resource (widget_class, "/com/feaneron/Boatswain/bs-profile-row.ui");

  gtk_widget_class_bind_template_child (widget_class, BsProfileRow, menu_button);
  gtk_widget_class_bind_template_child (widget_class, BsProfileRow, rename_button);
  gtk_widget_class_bind_template_child (widget_class, BsProfileRow, rename_entry);
  gtk_widget_class_bind_template_child (widget_class, BsProfileRow, selected_icon);

  gtk_widget_class_bind_template_callback (widget_class, on_rename_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_rename_entry_activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_rename_entry_text_changed_cb);
}

static void
bs_profile_row_init (BsProfileRow *self)
{
  const GActionEntry actions[] = {
    { "delete", on_delete_action_activated_cb, },
    { "move-down", on_move_down_action_activated_cb, },
    { "move-up", on_move_up_action_activated_cb, },
  };

  gtk_widget_init_template (GTK_WIDGET (self));

  self->action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (self->action_group), actions, G_N_ELEMENTS (actions), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "profile-row", G_ACTION_GROUP (self->action_group));
}

GtkWidget *
bs_profile_row_new (BsDevice  *device,
                    BsProfile *profile)
{
  return g_object_new (BS_TYPE_PROFILE_ROW,
                       "stream-deck", device,
                       "profile", profile,
                       NULL);
}

BsProfile *
bs_profile_row_get_profile (BsProfileRow *self)
{
  g_return_val_if_fail (BS_IS_PROFILE_ROW (self), NULL);

  return self->profile;
}
