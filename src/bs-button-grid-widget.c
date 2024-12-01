/*
 * bs-button-grid-widget.c
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

#define G_LOG_DOMAIN "Button Grid Widget"

#include "bs-button-grid-widget.h"

#include "bs-actionable.h"
#include "bs-action-private.h"
#include "bs-button-private.h"
#include "bs-button-grid.h"
#include "bs-button-widget.h"
#include "bs-debug.h"
#include "bs-events-private.h"
#include "bs-selection-controller.h"

#include <libpeas.h>

struct _BsButtonGridWidget
{
  GtkWidget parent_instance;

  GtkWidget *flowbox;

  BsButtonGrid *button_grid;
  BsSelectionController *selection_controller;
};

G_DEFINE_FINAL_TYPE (BsButtonGridWidget, bs_button_grid_widget, GTK_TYPE_WIDGET)

enum
{
  PROP_0,
  PROP_BUTTON_GRID,
  PROP_SELECTION_CONTROLLER,
  N_PROPS,
};

static GParamSpec *properties [N_PROPS];


/*
 * Callbacks
 */

static void
on_flowbox_child_activated_cb (GtkFlowBox         *flowbox,
                               GtkFlowBoxChild    *child,
                               BsButtonGridWidget *self)
{
  BsButton *button;
  BsAction *action;

  button = bs_button_widget_get_button (BS_BUTTON_WIDGET (child));
  action = bs_actionable_get_action (BS_ACTIONABLE (button));

  if (action)
    {
      g_autoptr (BsEvent) cursor_event = NULL;
      BsStreamDeck *device;

      device = bs_device_region_get_stream_deck (BS_DEVICE_REGION (self->button_grid));
      cursor_event = bs_cursor_event_new (BS_CURSOR_DOUBLE_CLICK, device);

      bs_action_handle_event (action, cursor_event);
    }
}

static void
on_flowbox_selected_children_changed_cb (GtkFlowBox         *flowbox,
                                         BsButtonGridWidget *self)
{
  g_autoptr (GList) selected_children = NULL;
  BsButtonWidget *child;

  selected_children = gtk_flow_box_get_selected_children (flowbox);
  child = selected_children ? selected_children->data : NULL;

  if (child)
    {
      g_assert (BS_IS_BUTTON_WIDGET (child));

      bs_selection_controller_set_selection (self->selection_controller,
                                             self->button_grid,
                                             bs_button_widget_get_button (child));
    }
}

static void
on_selection_controller_selection_changed_cb (BsSelectionController *selection_controller,
                                              BsButtonGridWidget    *self)
{
  gpointer owner, item;

  if (!bs_selection_controller_get_selection (selection_controller, &owner, &item) ||
      owner != self->button_grid)
    {
      gtk_flow_box_unselect_all (GTK_FLOW_BOX (self->flowbox));
    }
}


/*
 * GObject overrides
 */

static void
bs_button_grid_widget_dispose (GObject *object)
{
  BsButtonGridWidget *self = (BsButtonGridWidget *)object;

  g_clear_pointer (&self->flowbox, gtk_widget_unparent);

  g_clear_object (&self->button_grid);
  g_clear_object (&self->selection_controller);

  gtk_widget_dispose_template (GTK_WIDGET (self), BS_TYPE_BUTTON_GRID_WIDGET);

  G_OBJECT_CLASS (bs_button_grid_widget_parent_class)->dispose (object);
}

static void
bs_button_grid_widget_constructed (GObject *object)
{
  BsButtonGridWidget *self = (BsButtonGridWidget *)object;
  GListModel *buttons;
  unsigned int grid_columns;

  G_OBJECT_CLASS (bs_button_grid_widget_parent_class)->constructed (object);

  grid_columns = bs_button_grid_get_grid_columns (self->button_grid);
  gtk_flow_box_set_min_children_per_line (GTK_FLOW_BOX (self->flowbox), grid_columns);
  gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (self->flowbox), grid_columns);

  buttons = bs_button_grid_get_buttons (self->button_grid);

  for (unsigned int i = 0; i < g_list_model_get_n_items (buttons); i++)
    {
      g_autoptr (BsButton) button = NULL;
      GtkWidget *widget;

      button = g_list_model_get_item (buttons, i);

      widget = bs_button_widget_new (button);
      gtk_flow_box_append (GTK_FLOW_BOX (self->flowbox), widget);
    }
}

static void
bs_button_grid_widget_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  BsButtonGridWidget *self = BS_BUTTON_GRID_WIDGET (object);

  switch (prop_id)
    {
    case PROP_BUTTON_GRID:
      g_value_set_object (value, self->button_grid);
      break;

    case PROP_SELECTION_CONTROLLER:
      g_value_set_object (value, self->selection_controller);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_button_grid_widget_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  BsButtonGridWidget *self = BS_BUTTON_GRID_WIDGET (object);

  switch (prop_id)
    {
    case PROP_BUTTON_GRID:
      g_assert (self->button_grid == NULL);
      self->button_grid = g_value_dup_object (value);
      g_assert (self->button_grid != NULL);
      break;

    case PROP_SELECTION_CONTROLLER:
      g_assert (self->selection_controller == NULL);
      self->selection_controller = g_value_dup_object (value);
      g_assert (self->selection_controller != NULL);

      g_signal_connect_object (self->selection_controller,
                               "selection-changed",
                               G_CALLBACK (on_selection_controller_selection_changed_cb),
                               self, 0);

      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_button_grid_widget_class_init (BsButtonGridWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = bs_button_grid_widget_dispose;
  object_class->constructed = bs_button_grid_widget_constructed;
  object_class->get_property = bs_button_grid_widget_get_property;
  object_class->set_property = bs_button_grid_widget_set_property;

  properties[PROP_BUTTON_GRID] = g_param_spec_object ("button-grid", NULL, NULL,
                                                      BS_TYPE_BUTTON_GRID,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_SELECTION_CONTROLLER] =
    g_param_spec_object ("selection-controller", NULL, NULL,
                         BS_TYPE_SELECTION_CONTROLLER,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/com/feaneron/Boatswain/bs-button-grid-widget.ui");

  gtk_widget_class_bind_template_child (widget_class, BsButtonGridWidget, flowbox);

  gtk_widget_class_bind_template_callback (widget_class, on_flowbox_child_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_flowbox_selected_children_changed_cb);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);

  gtk_widget_class_set_css_name (widget_class, "buttongridwidget");
}

static void
bs_button_grid_widget_init (BsButtonGridWidget *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
bs_button_grid_widget_new (BsButtonGrid          *button_grid,
                           BsSelectionController *selection_controller)
{
  g_assert (BS_IS_BUTTON_GRID (button_grid));

  return g_object_new (BS_TYPE_BUTTON_GRID_WIDGET,
                       "button-grid", button_grid,
                       "selection-controller", selection_controller,
                       NULL);
}
