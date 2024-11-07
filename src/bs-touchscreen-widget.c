/*
 * bs-touchscreen-widget.c
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

#include "bs-touchscreen-widget.h"

#include "bs-selection-controller.h"
#include "bs-touchscreen-private.h"
#include "bs-touchscreen-region.h"
#include "bs-touchscreen-slot-widget.h"
#include "bs-touchscreen-slot.h"
#include "bs-debug.h"

struct _BsTouchscreenWidget
{
  GtkWidget parent_instance;

  GtkPicture *picture;
  GtkFlowBox *slots_flowbox;

  BsTouchscreenRegion *touchscreen_region;
  BsSelectionController *selection_controller;
};

G_DEFINE_FINAL_TYPE (BsTouchscreenWidget, bs_touchscreen_widget, GTK_TYPE_WIDGET)

enum {
  PROP_0,
  PROP_TOUCHSCREEN_REGION,
  PROP_SELECTION_CONTROLLER,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];


/*
 * Callbacks
 */

static void
on_slots_flowbox_selected_children_changed_cb (GtkFlowBox          *flowbox,
                                               BsTouchscreenWidget *self)
{
  g_autoptr (GList) selected_children = NULL;
  GtkFlowBoxChild *child;

  selected_children = gtk_flow_box_get_selected_children (flowbox);
  child = selected_children ? selected_children->data : NULL;

  if (child)
    {
      BsTouchscreenSlot *slot;

      g_assert (BS_IS_TOUCHSCREEN_SLOT_WIDGET (child));

      slot = bs_touchscreen_slot_widget_get_slot (BS_TOUCHSCREEN_SLOT_WIDGET (child));

      bs_selection_controller_set_selection (self->selection_controller,
                                             self->touchscreen_region,
                                             slot);
    }
}
static void
on_selection_controller_selection_changed_cb (BsSelectionController *selection_controller,
                                              BsTouchscreenWidget   *self)
{
  gpointer owner, item;

  if (!bs_selection_controller_get_selection (selection_controller, &owner, &item) ||
      owner != self->touchscreen_region)
    {
      gtk_flow_box_unselect_all (self->slots_flowbox);
    }
}


/*
 * GObject overrides
 */

static void
bs_touchscreen_widget_dispose (GObject *object)
{
  BsTouchscreenWidget *self = (BsTouchscreenWidget *)object;

  g_clear_object (&self->touchscreen_region);
  g_clear_object (&self->selection_controller);

  G_OBJECT_CLASS (bs_touchscreen_widget_parent_class)->dispose (object);
}

static void
bs_touchscreen_widget_constructed (GObject *object)
{
  BsTouchscreenWidget *self = (BsTouchscreenWidget *)object;
  BsTouchscreen *touchscreen;
  GListModel *slots;

  G_OBJECT_CLASS (bs_touchscreen_widget_parent_class)->constructed (object);

  touchscreen = bs_touchscreen_region_get_touchscreen (self->touchscreen_region);
  slots = bs_touchscreen_get_slots (touchscreen);

  gtk_flow_box_set_min_children_per_line (self->slots_flowbox, g_list_model_get_n_items (slots));
  gtk_flow_box_set_max_children_per_line (self->slots_flowbox, g_list_model_get_n_items (slots));

  for (size_t i = 0; i < g_list_model_get_n_items (slots); i++)
    {
      g_autoptr (BsTouchscreenSlot) slot = g_list_model_get_item (slots, i);
      GtkWidget *child;

      child = bs_touchscreen_slot_widget_new (slot);

      gtk_flow_box_append (self->slots_flowbox, child);
    }

  g_object_bind_property (bs_touchscreen_get_content (touchscreen),
                          "background-paintable",
                          self->picture,
                          "paintable",
                          G_BINDING_SYNC_CREATE);
}

static void
bs_touchscreen_widget_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  BsTouchscreenWidget *self = BS_TOUCHSCREEN_WIDGET (object);

  switch (prop_id)
    {
    case PROP_TOUCHSCREEN_REGION:
      g_value_set_object (value, self->touchscreen_region);
      break;

    case PROP_SELECTION_CONTROLLER:
      g_value_set_object (value, self->selection_controller);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_touchscreen_widget_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  BsTouchscreenWidget *self = BS_TOUCHSCREEN_WIDGET (object);

  switch (prop_id)
    {
    case PROP_TOUCHSCREEN_REGION:
      g_assert (self->touchscreen_region == NULL);
      self->touchscreen_region = g_value_dup_object (value);
      g_assert (self->touchscreen_region != NULL);
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
bs_touchscreen_widget_class_init (BsTouchscreenWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = bs_touchscreen_widget_dispose;
  object_class->constructed = bs_touchscreen_widget_constructed;
  object_class->get_property = bs_touchscreen_widget_get_property;
  object_class->set_property = bs_touchscreen_widget_set_property;

  properties[PROP_TOUCHSCREEN_REGION] =
    g_param_spec_object ("touchscreen-region", NULL, NULL,
                         BS_TYPE_TOUCHSCREEN_REGION,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_SELECTION_CONTROLLER] =
    g_param_spec_object ("selection-controller", NULL, NULL,
                         BS_TYPE_SELECTION_CONTROLLER,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/com/feaneron/Boatswain/bs-touchscreen-widget.ui");

  gtk_widget_class_bind_template_child (widget_class, BsTouchscreenWidget, picture);
  gtk_widget_class_bind_template_child (widget_class, BsTouchscreenWidget, slots_flowbox);

  gtk_widget_class_bind_template_callback (widget_class, on_slots_flowbox_selected_children_changed_cb);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);

  gtk_widget_class_set_css_name (widget_class, "touchscreenwidget");
}

static void
bs_touchscreen_widget_init (BsTouchscreenWidget *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_set_size_request (GTK_WIDGET (self), -1, 100);
  gtk_widget_add_css_class (GTK_WIDGET (self), "card");
}

GtkWidget *
bs_touchscreen_widget_new (BsTouchscreenRegion   *touchscreen_region,
                           BsSelectionController *selection_controller)
{
  return g_object_new (BS_TYPE_TOUCHSCREEN_WIDGET,
                       "touchscreen-region", touchscreen_region,
                       "selection-controller", selection_controller,
                       NULL);
}

