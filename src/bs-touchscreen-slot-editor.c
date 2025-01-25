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

#define G_LOG_DOMAIN "Touchscreen Editor"

#include "bs-touchscreen-slot-editor.h"

#include "bs-actionable-private.h"
#include "bs-action-selector.h"
#include "bs-action-factory.h"
#include "bs-action-info.h"
#include "bs-application-private.h"
#include "bs-empty-action.h"
#include "bs-icon.h"
#include "bs-page.h"
#include "bs-page-item.h"
#include "bs-stream-deck.h"
#include "bs-touchscreen-background-row.h"
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
  g_autoptr (BsIcon) custom_icon = NULL;
  g_autoptr (GError) error = NULL;
  PeasPluginInfo *plugin_info;
  BsTouchscreen *touchscreen;
  BsStreamDeck *stream_deck;
  BsPageItem *item;
  BsPage *active_page;
  const char *region_id;
  uint32_t position;

  touchscreen = bs_touchscreen_slot_get_touchscreen (self->slot);
  region_id = bs_device_region_get_id (BS_DEVICE_REGION (touchscreen));
  stream_deck = bs_device_region_get_stream_deck (BS_DEVICE_REGION (touchscreen));
  active_page = bs_stream_deck_get_active_page (stream_deck);
  plugin_info = peas_extension_base_get_plugin_info (PEAS_EXTENSION_BASE (factory));
  position = bs_touchscreen_get_slot_position (touchscreen, self->slot);

  item = bs_page_get_item (active_page, region_id, position);
  bs_page_item_set_item_type (item, BS_PAGE_ITEM_ACTION);
  bs_page_item_set_factory (item, peas_plugin_info_get_module_name (plugin_info));
  bs_page_item_set_action (item, bs_action_info_get_id (action_info));

  bs_page_realize (active_page, region_id, position, &custom_icon, &action, &error);

  if (error)
    g_warning ("Error realizing action: %s", error->message);

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
on_remove_row_activated_cb (GtkButton               *button,
                            BsTouchscreenSlotEditor *self)
{
  g_autoptr (BsAction) empty_action = NULL;

  empty_action = bs_empty_action_new ();
  bs_actionable_set_action (BS_ACTIONABLE (self->slot), empty_action);
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
  g_type_ensure (BS_TYPE_TOUCHSCREEN_BACKGROUND_ROW);

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
  gtk_widget_class_bind_template_callback (widget_class, on_remove_row_activated_cb);

  gtk_widget_class_set_css_name (widget_class, "touchscreensloteditor");
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
