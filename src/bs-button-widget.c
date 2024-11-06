/*
 * bs-button-widget.c
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

#define G_LOG_DOMAIN "Stream Deck Button Widget"

#include "bs-button-widget.h"

#include "bs-actionable-private.h"
#include "bs-action-private.h"
#include "bs-debug.h"
#include "bs-empty-action.h"
#include "bs-icon.h"
#include "bs-page.h"
#include "bs-stream-deck.h"
#include "bs-button.h"

#include <libpeas.h>

struct _BsButtonWidget
{
  GtkFlowBoxChild parent_instance;

  GtkDragSource *drag_source;
  GtkDropTarget *drop_target;
  GtkPicture *picture;

  BsButton *button;
};

G_DEFINE_FINAL_TYPE (BsButtonWidget, bs_button_widget, GTK_TYPE_FLOW_BOX_CHILD)

enum
{
  PROP_0,
  PROP_BUTTON,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];


/*
 * Auxiliary methods
 */

static inline gboolean
is_move_page_up_action (BsButtonWidget *self)
{
  const PeasPluginInfo *plugin_info;
  BsActionFactory *factory;
  BsStreamDeck *stream_deck;
  BsAction *action;
  BsPage *active_page;

  action = bs_actionable_get_action (BS_ACTIONABLE (self->button));
  if (!action || BS_IS_EMPTY_ACTION (action))
    return FALSE;

  factory = bs_action_get_factory (action);
  plugin_info = peas_extension_base_get_plugin_info (PEAS_EXTENSION_BASE (factory));
  stream_deck = bs_button_get_stream_deck (self->button);
  active_page = bs_stream_deck_get_active_page (stream_deck);

  return bs_button_get_position (self->button) == 0 &&
         !bs_page_is_root (active_page) &&
         g_strcmp0 (peas_plugin_info_get_module_name (plugin_info), "default") == 0 &&
         g_strcmp0 (bs_action_get_id (action), "default-switch-page-action") == 0;
}

static void
select_myself_in_parent_flowbox (BsButtonWidget *self)
{
  GtkWidget *flowbox;

  flowbox = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_FLOW_BOX);
  gtk_flow_box_select_child (GTK_FLOW_BOX (flowbox), GTK_FLOW_BOX_CHILD (self));
}

static void
update_button_texture (BsButtonWidget *self)
{
  BsIcon *icon = bs_button_get_icon (self->button);
  gtk_picture_set_paintable (self->picture, GDK_PAINTABLE (icon));
}


/*
 * Callbacks
 */

static GdkContentProvider *
on_drag_prepare_cb (GtkDragSource            *drag_source,
                    double                    x,
                    double                    y,
                    BsButtonWidget *self)
{
  BS_RETURN (gdk_content_provider_new_typed (BS_TYPE_BUTTON, self->button));
}

static void
on_drag_begin_cb (GtkDragSource            *drag_source,
                  GdkDrag                  *drag,
                  BsButtonWidget *self)
{
  BsIcon *icon;

  BS_ENTRY;

  icon = bs_button_get_icon (self->button);
  gtk_drag_source_set_icon (drag_source, GDK_PAINTABLE (icon), 0, 0);

  BS_EXIT;
}

static gboolean
on_drop_target_drop_cb (GtkDropTarget            *drop_target,
                        const GValue             *value,
                        double                    x,
                        double                    y,
                        BsButtonWidget *self)
{
  g_autoptr (BsAction) dragged_button_action = NULL;
  g_autoptr (BsAction) dropped_button_action = NULL;
  g_autoptr (BsIcon) dragged_button_icon = NULL;
  g_autoptr (BsIcon) dropped_button_icon = NULL;
  BsButton *dragged_button;
  BsButton *dropped_button;

  BS_ENTRY;

  if (!G_VALUE_HOLDS (value, BS_TYPE_BUTTON))
    BS_RETURN (FALSE);

  dropped_button = self->button;
  dragged_button = g_value_get_object (value);

  if (dragged_button == dropped_button)
    BS_RETURN (FALSE);

  /* Swap actions */
  dragged_button_action = bs_actionable_get_action (BS_ACTIONABLE (dragged_button));
  dropped_button_action = bs_actionable_get_action (BS_ACTIONABLE (dropped_button));
  dragged_button_icon = bs_button_get_custom_icon (dragged_button);
  dropped_button_icon = bs_button_get_custom_icon (dropped_button);

  if (dragged_button_action)
    g_object_ref (dragged_button_action);
  if (dropped_button_action)
    g_object_ref (dropped_button_action);
  if (dragged_button_icon)
    g_object_ref (dragged_button_icon);
  if (dropped_button_icon)
    g_object_ref (dropped_button_icon);

  bs_actionable_set_action (BS_ACTIONABLE (dragged_button), dropped_button_action);
  bs_actionable_set_action (BS_ACTIONABLE (dropped_button), dragged_button_action);

  bs_button_set_custom_icon (dragged_button, dropped_button_icon);
  bs_button_set_custom_icon (dropped_button, dragged_button_icon);

  select_myself_in_parent_flowbox (self);

  BS_RETURN (TRUE);
}

static void
on_button_action_changed_cb (BsButton       *button,
                                         GParamSpec               *pspec,
                                         BsButtonWidget *self)
{
  if (is_move_page_up_action (self))
    {
      gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (self->drag_source),
                                                  GTK_PHASE_NONE);
      gtk_drop_target_set_actions (self->drop_target, 0);
    }
  else
    {
      gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (self->drag_source),
                                                  GTK_PHASE_BUBBLE);
      gtk_drop_target_set_actions (self->drop_target, GDK_ACTION_MOVE);
    }
}

static void
on_button_icon_changed_cb (BsButton       *button,
                                       BsIcon         *icon,
                                       BsButtonWidget *self)
{
  update_button_texture (self);
}


/*
 * Overrides
 */

static void
bs_button_widget_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  BsButtonWidget *self = BS_BUTTON_WIDGET (object);

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
bs_button_widget_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  BsButtonWidget *self = BS_BUTTON_WIDGET (object);

  switch (prop_id)
    {
    case PROP_BUTTON:
      g_assert (self->button == NULL);
      self->button = g_value_get_object (value);
      g_signal_connect_object (self->button,
                               "notify::action",
                               G_CALLBACK (on_button_action_changed_cb),
                               self,
                               0);
      g_signal_connect_object (self->button,
                               "icon-changed",
                               G_CALLBACK (on_button_icon_changed_cb),
                               self,
                               0);
      update_button_texture (self);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_button_widget_class_init (BsButtonWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = bs_button_widget_get_property;
  object_class->set_property = bs_button_widget_set_property;

  properties[PROP_BUTTON] = g_param_spec_object ("button", NULL, NULL,
                                                 BS_TYPE_BUTTON,
                                                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/com/feaneron/Boatswain/bs-button-widget.ui");

  gtk_widget_class_bind_template_child (widget_class, BsButtonWidget, drag_source);
  gtk_widget_class_bind_template_child (widget_class, BsButtonWidget, picture);

  gtk_widget_class_bind_template_callback (widget_class, on_drag_prepare_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_drag_begin_cb);

  gtk_widget_class_set_css_name (widget_class, "streamdeckbuttonwidget");
}

static void
bs_button_widget_init (BsButtonWidget *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->drop_target = gtk_drop_target_new (BS_TYPE_BUTTON, GDK_ACTION_MOVE);
  gtk_drop_target_set_preload (self->drop_target, TRUE);
  g_signal_connect (self->drop_target, "drop", G_CALLBACK (on_drop_target_drop_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (self->drop_target));
}

GtkWidget *
bs_button_widget_new (BsButton *button)
{
  return g_object_new (BS_TYPE_BUTTON_WIDGET,
                       "button", button,
                       NULL);
}

BsButton *
bs_button_widget_get_button (BsButtonWidget *self)
{
  g_return_val_if_fail (BS_IS_BUTTON_WIDGET (self), NULL);

  return self->button;
}
