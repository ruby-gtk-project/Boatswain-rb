/* boatswain-window.c
 *
 * Copyright 2022 Georges Basile Stavracas Neto
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
 */

#include "bs-application-private.h"
#include "bs-config.h"
#include "bs-device-editor.h"
#include "bs-device-manager.h"
#include "bs-profile.h"
#include "bs-profile-row.h"
#include "bs-stream-deck.h"
#include "bs-window.h"

#include <glib/gi18n.h>

struct _BsWindow
{
  AdwApplicationWindow  parent_instance;

  GtkAdjustment *brightness_adjustment;
  GtkMenuButton *devices_menu_button;
  GtkPopover *devices_popover;
  GtkStack *devices_stack;
  AdwStatusPage *empty_page;
  GtkWidget *empty_page_view;
  GtkLabel *firmware_version_label;
  GtkStack *main_stack;
  GtkListBox *profiles_listbox;
  GtkEditable *new_profile_name_entry;
  AdwNavigationSplitView *split_view;
  GtkListBox *stream_decks_listbox;

  GBinding *brightness_binding;
  BsStreamDeck *current_stream_deck;
};

static GtkWidget * create_profile_row_cb (gpointer item,
                                          gpointer user_data);

G_DEFINE_FINAL_TYPE (BsWindow, bs_window, ADW_TYPE_APPLICATION_WINDOW)

enum
{
  PROP_0,
  PROP_DEVICE,
  N_PROPS,
};

static GParamSpec *properties [N_PROPS] = { NULL, };

/*
 * Auxiliary methods
 */

static void
append_new_profile (BsWindow *self)
{
  g_autoptr (BsProfile) new_profile = NULL;
  g_autofree char *new_profile_name = NULL;
  GListModel *profiles;

  new_profile_name = g_strdup (gtk_editable_get_text (self->new_profile_name_entry));
  g_assert (new_profile_name != NULL);

  g_strstrip (new_profile_name);

  if (g_utf8_strlen (new_profile_name, -1) == 0)
    return;

  new_profile = bs_profile_new_empty (self->current_stream_deck);
  bs_profile_set_name (new_profile, new_profile_name);

  profiles = bs_stream_deck_get_profiles (self->current_stream_deck);
  g_list_store_append (G_LIST_STORE (profiles), new_profile);

  bs_stream_deck_load_profile (self->current_stream_deck, new_profile);

  gtk_editable_set_text (self->new_profile_name_entry, "");
}

static void
select_stream_deck (BsWindow     *self,
                    BsStreamDeck *stream_deck)
{
  g_autofree char *page_name = NULL;

  if (self->current_stream_deck == stream_deck)
    return;

  gtk_stack_set_visible_child_name (self->main_stack, "devices");

  page_name = g_strdup_printf ("%p", stream_deck);
  gtk_stack_set_visible_child_name (self->devices_stack, page_name);

  g_clear_pointer (&self->brightness_binding, g_binding_unbind);

  self->current_stream_deck = stream_deck;

  if (stream_deck)
    {
      gtk_label_set_label (self->firmware_version_label, bs_stream_deck_get_firmware_version (stream_deck));

      self->brightness_binding = g_object_bind_property (stream_deck,
                                                         "brightness",
                                                         self->brightness_adjustment,
                                                         "value",
                                                         G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
    }

  gtk_list_box_bind_model (self->profiles_listbox,
                           stream_deck ? bs_stream_deck_get_profiles (stream_deck) : NULL,
                           create_profile_row_cb,
                           self,
                           NULL);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DEVICE]);
}


/*
 * Callbacks
 */

static void
on_profile_row_move_cb (BsProfileRow *profile_row,
                        unsigned int  new_position,
                        BsWindow     *self)
{
  GListModel *profiles;
  BsProfile *profile;
  unsigned int position;

  profiles = bs_stream_deck_get_profiles (self->current_stream_deck);
  profile = bs_profile_row_get_profile (profile_row);

  g_object_ref (profile);
  g_list_store_find (G_LIST_STORE (profiles), profile, &position);
  g_list_store_remove (G_LIST_STORE (profiles), position);
  g_list_store_insert (G_LIST_STORE (profiles), new_position, profile);
  g_object_unref (profile);
}

static GtkWidget *
create_profile_row_cb (gpointer item,
                       gpointer user_data)
{
  GtkWidget *row;
  BsWindow *self;

  self = BS_WINDOW (user_data);

  row = bs_profile_row_new (self->current_stream_deck, BS_PROFILE (item));
  g_signal_connect (row, "move", G_CALLBACK (on_profile_row_move_cb), self);

  return row;
}

static GtkWidget *
create_stream_deck_row_cb (gpointer item,
                           gpointer user_data)
{
  BsStreamDeck *stream_deck;
  GtkWidget *subtitle;
  GtkWidget *title;
  GtkWidget *box;
  GtkWidget *row;

  stream_deck = BS_STREAM_DECK (item);

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_VERTICAL,
                      "spacing", 0,
                      "margin-top", 3,
                      "margin-bottom", 3,
                      NULL);

  title = g_object_new (GTK_TYPE_LABEL,
                        "hexpand", TRUE,
                        "halign", GTK_ALIGN_START,
                        "label", bs_stream_deck_get_name (stream_deck),
                        "xalign", 0.0,
                        NULL);
  gtk_box_append (GTK_BOX (box), title);

  subtitle = g_object_new (GTK_TYPE_LABEL,
                           "hexpand", TRUE,
                           "halign", GTK_ALIGN_START,
                           "label", bs_stream_deck_get_serial_number (stream_deck),
                           "xalign", 0.0,
                           NULL);
  gtk_widget_add_css_class (subtitle, "caption");
  gtk_widget_add_css_class (subtitle, "dim-label");
  gtk_box_append (GTK_BOX (box), subtitle);

  row = gtk_list_box_row_new ();
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);
  g_object_set_data (G_OBJECT (row), "stream-deck", item);

  return row;
}

static void
on_device_manager_device_added_cb (BsDeviceManager *device_manager,
                                   BsStreamDeck    *stream_deck,
                                   BsWindow        *self)
{
  g_autofree char *page_name = NULL;
  GtkWidget *editor;

  editor = bs_device_editor_new (stream_deck);
  page_name = g_strdup_printf ("%p", stream_deck);
  gtk_stack_add_named (self->devices_stack, editor, page_name);

  if (g_list_model_get_n_items (G_LIST_MODEL (device_manager)) == 1)
    select_stream_deck (self, stream_deck);
}

static void
on_device_manager_device_removed_cb (BsDeviceManager *device_manager,
                                     BsStreamDeck    *stream_deck,
                                     BsWindow        *self)
{
  g_autofree char *page_name = NULL;
  GtkWidget *child;

  page_name = g_strdup_printf ("%p", stream_deck);
  child = gtk_stack_get_child_by_name (self->devices_stack, page_name);

  gtk_stack_remove (self->devices_stack, child);

  if (g_list_model_get_n_items (G_LIST_MODEL (device_manager)) == 0)
    gtk_stack_set_visible_child_name (self->main_stack, "empty");
}

static void
on_show_about_action_activated_cb (GSimpleAction *action,
                                   GVariant      *parameter,
                                   gpointer       user_data)
{
  BsWindow *self = BS_WINDOW (user_data);
  const char *artists[] = {
    "Jakub Steiner",
    "Sam Hewitt",
    NULL,
  };
  const char *developers[] = {
    "Georges Basile Stavracas Neto <georges.stavracas@gmail.com>",
    NULL,
  };

  g_assert (BS_IS_WINDOW (self));

  adw_show_about_dialog (GTK_WIDGET (self),
                         "application-name", "Boatswain",
                         "application-icon", APPLICATION_ID,
                         "version", PACKAGE_VERSION,
                         "copyright", "\xc2\xa9 2022 Georges Basile Stavracas Neto",
                         "developers", developers,
                         "artists", artists,
                         "license-type", GTK_LICENSE_GPL_3_0,
                         "website", "https://gitlab.gnome.org/World/boatswain",
                         "issue-url", "https://gitlab.gnome.org/World/boatswain/issues",
                         NULL);
}

static void
on_new_profile_name_entry_activate_cb (GtkEntry *entry,
                                       BsWindow *self)
{
  append_new_profile (self);
}

static void
on_profiles_listbox_row_activated_cb (GtkListBox    *listbox,
                                      GtkListBoxRow *row,
                                      BsWindow      *self)
{
  g_autoptr (BsProfile) profile = NULL;
  GListModel *profiles;

  profiles = bs_stream_deck_get_profiles (self->current_stream_deck);
  profile = g_list_model_get_item (profiles, gtk_list_box_row_get_index (row));

  bs_stream_deck_load_profile (self->current_stream_deck, profile);
}

static void
on_stream_decks_listbox_row_activated_cb (GtkListBox    *listbox,
                                          GtkListBoxRow *row,
                                          BsWindow      *self)
{
  BsStreamDeck *stream_deck;
  GtkWidget *menu_button;

  stream_deck = g_object_get_data (G_OBJECT (row), "stream-deck");
  select_stream_deck (self, stream_deck);

  menu_button = gtk_widget_get_ancestor (GTK_WIDGET (self->stream_decks_listbox),
                                         GTK_TYPE_MENU_BUTTON);
  gtk_menu_button_popdown (GTK_MENU_BUTTON (menu_button));
}


/*
 * GObject overrides
 */

static void
bs_window_constructed (GObject *object)
{
  BsDeviceManager *device_manager;
  GApplication *application;
  BsWindow *self;
  gboolean first;
  size_t i;

  G_OBJECT_CLASS (bs_window_parent_class)->constructed (object);

  self = BS_WINDOW (object);
  first = TRUE;
  application = g_application_get_default ();
  device_manager = bs_application_get_device_manager (BS_APPLICATION (application));

  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (device_manager)); i++)
    {
      g_autoptr (BsStreamDeck) stream_deck = NULL;
      g_autofree char *page_name = NULL;
      GtkWidget *editor;

      stream_deck = g_list_model_get_item (G_LIST_MODEL (device_manager), i);
      editor = bs_device_editor_new (stream_deck);
      page_name = g_strdup_printf ("%p", stream_deck);
      gtk_stack_add_named (self->devices_stack, editor, page_name);

      if (first)
        {
          select_stream_deck (self, stream_deck);
          first = FALSE;
        }
    }

  gtk_list_box_bind_model (self->stream_decks_listbox,
                           G_LIST_MODEL (device_manager),
                           create_stream_deck_row_cb,
                           self,
                           NULL);

  g_signal_connect_object (device_manager,
                           "device-added",
                           G_CALLBACK (on_device_manager_device_added_cb),
                           self,
                           0);

  g_signal_connect_object (device_manager,
                           "device-removed",
                           G_CALLBACK (on_device_manager_device_removed_cb),
                           self,
                           0);
}

static void
bs_window_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  BsWindow *self = BS_WINDOW (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, self->current_stream_deck);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_window_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  BsWindow *self = BS_WINDOW (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      select_stream_deck (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_window_class_init (BsWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = bs_window_constructed;
  object_class->get_property = bs_window_get_property;
  object_class->set_property = bs_window_set_property;

  properties[PROP_DEVICE] = g_param_spec_object ("device", NULL, NULL,
                                                 BS_TYPE_STREAM_DECK,
                                                 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/com/feaneron/Boatswain/bs-window.ui");

  gtk_widget_class_bind_template_child (widget_class, BsWindow, brightness_adjustment);
  gtk_widget_class_bind_template_child (widget_class, BsWindow, devices_menu_button);
  gtk_widget_class_bind_template_child (widget_class, BsWindow, devices_popover);
  gtk_widget_class_bind_template_child (widget_class, BsWindow, devices_stack);
  gtk_widget_class_bind_template_child (widget_class, BsWindow, empty_page);
  gtk_widget_class_bind_template_child (widget_class, BsWindow, firmware_version_label);
  gtk_widget_class_bind_template_child (widget_class, BsWindow, main_stack);
  gtk_widget_class_bind_template_child (widget_class, BsWindow, new_profile_name_entry);
  gtk_widget_class_bind_template_child (widget_class, BsWindow, profiles_listbox);
  gtk_widget_class_bind_template_child (widget_class, BsWindow, stream_decks_listbox);

  gtk_widget_class_bind_template_callback (widget_class, on_new_profile_name_entry_activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_profiles_listbox_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_stream_decks_listbox_row_activated_cb);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_w, GDK_CONTROL_MASK, "window.close", NULL);
}

static void
bs_window_init (BsWindow *self)
{
  const GActionEntry actions[] = {
    { "about", on_show_about_action_activated_cb, },
  };

  gtk_widget_init_template (GTK_WIDGET (self));

  g_action_map_add_action_entries (G_ACTION_MAP (self), actions, G_N_ELEMENTS (actions), self);

  if (g_strcmp0 (PROFILE, "development") == 0)
    {
      adw_status_page_set_icon_name (self->empty_page, "com.feaneron.Boatswain.Devel");
      gtk_widget_add_css_class (GTK_WIDGET (self), "devel");
    }
}
