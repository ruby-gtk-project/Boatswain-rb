/* bs-stream-deck-editor.c
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

#define G_LOG_DOMAIN "Device Editor"

#include "bs-device-editor.h"

#include "bs-action-private.h"
#include "bs-button-grid-region.h"
#include "bs-debug.h"
#include "bs-dial.h"
#include "bs-dial-grid-region.h"
#include "bs-dial-widget.h"
#include "bs-icon.h"
#include "bs-page.h"
#include "bs-stream-deck.h"
#include "bs-button.h"
#include "bs-button-editor.h"
#include "bs-button-grid-widget.h"
#include "bs-button-widget.h"
#include "bs-selection-controller.h"
#include "bs-stream-deck-private.h"
#include "bs-touchscreen-region.h"
#include "bs-touchscreen-slot.h"
#include "bs-touchscreen-widget.h"

#include <glib/gi18n.h>
#include <libpeas.h>

struct _BsDeviceEditor
{
  AdwBin parent_instance;

  AdwBin *editor_bin;
  AdwToolbarView *empty_page;
  GtkGrid *regions_grid;

  BsStreamDeck *stream_deck;

  GHashTable *region_to_widget;

  /* Can be a BsButton, BsDial, or BsTouchscreenSlot */
  BsSelectionController *selection_controller;
};

G_DEFINE_FINAL_TYPE (BsDeviceEditor, bs_device_editor, ADW_TYPE_BIN)

enum
{
  PROP_0,
  PROP_STREAM_DECK,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];


/*
 * Auxiliary methods
 */

static inline void
set_selected_item (BsDeviceEditor *self,
                   gpointer        selected_item)
{
  adw_bin_set_child (self->editor_bin, NULL);

  if (BS_IS_BUTTON (selected_item))
    {
      GtkWidget *button_editor = bs_button_editor_new (selected_item);
      adw_bin_set_child (self->editor_bin, button_editor);
    }
  else if (BS_IS_DIAL (selected_item))
    {
      BS_TODO ("Dial editor");
      adw_bin_set_child (self->editor_bin, GTK_WIDGET (self->empty_page));
    }
  else if (BS_IS_TOUCHSCREEN_SLOT (selected_item))
    {
      BS_TODO ("Touchscreen slot editor");
      adw_bin_set_child (self->editor_bin, GTK_WIDGET (self->empty_page));
    }
  else
    {
      g_assert_not_reached ();
    }
}

static void
add_button_grid (BsDeviceEditor     *self,
                 BsButtonGridRegion *button_grid)
{
  BsDeviceRegion *region;
  GtkWidget *widget;

  widget = bs_button_grid_widget_new (button_grid, self->selection_controller);
  region = BS_DEVICE_REGION (button_grid);
  gtk_grid_attach (self->regions_grid,
                   widget,
                   bs_device_region_get_column (region),
                   bs_device_region_get_row (region),
                   bs_device_region_get_column_span (region),
                   bs_device_region_get_row_span (region));

  g_hash_table_insert (self->region_to_widget, region, widget);
}

static void
add_dial_grid (BsDeviceEditor   *self,
               BsDialGridRegion *dial_grid)
{
  BsDeviceRegion *region;
  GtkFlowBox *dials_flowbox;
  GListModel *dials;
  unsigned int grid_columns;

  dials = bs_dial_grid_region_get_dials (dial_grid);
  grid_columns = bs_dial_grid_region_get_grid_columns (dial_grid);

  dials_flowbox = g_object_new (GTK_TYPE_FLOW_BOX,
                                "valign", GTK_ALIGN_CENTER,
                                "homogeneous", TRUE,
                                "activate-on-single-click", FALSE,
                                "selection-mode", GTK_SELECTION_NONE,
                                "column-spacing", 18,
                                "row-spacing", 18,
                                "min-children-per-line", grid_columns,
                                "max-children-per-line", grid_columns,
                                NULL);

  for (size_t i = 0; i < g_list_model_get_n_items (dials); i++)
    {
      g_autoptr (BsDial) dial = NULL;
      GtkWidget *widget;

      dial = g_list_model_get_item (dials, i);
      widget = bs_dial_widget_new (dial);

      gtk_flow_box_append (dials_flowbox, widget);
    }

  region = BS_DEVICE_REGION (dial_grid);
  gtk_grid_attach (self->regions_grid,
                   GTK_WIDGET (dials_flowbox),
                   bs_device_region_get_column (region),
                   bs_device_region_get_row (region),
                   bs_device_region_get_column_span (region),
                   bs_device_region_get_row_span (region));

  g_hash_table_insert (self->region_to_widget, region, dials_flowbox);
}

static void
add_touchscreen (BsDeviceEditor      *self,
                 BsTouchscreenRegion *touchscreen_region)
{
  BsDeviceRegion *region;
  GtkWidget *widget;

  widget = bs_touchscreen_widget_new (touchscreen_region, self->selection_controller);

  region = BS_DEVICE_REGION (touchscreen_region);
  gtk_grid_attach (self->regions_grid,
                   widget,
                   bs_device_region_get_column (region),
                   bs_device_region_get_row (region),
                   bs_device_region_get_column_span (region),
                   bs_device_region_get_row_span (region));

  g_hash_table_insert (self->region_to_widget, region, widget);
}

static void
build_regions (BsDeviceEditor *self)
{
  GListModel *regions = bs_stream_deck_get_regions (self->stream_deck);

  for (unsigned int i = 0; i < g_list_model_get_n_items (regions); i++)
    {
      g_autoptr (BsDeviceRegion) region = g_list_model_get_item (regions, i);

      if (BS_IS_BUTTON_GRID_REGION (region))
        add_button_grid (self, BS_BUTTON_GRID_REGION (region));
      else if (BS_IS_DIAL_GRID_REGION (region))
        add_dial_grid (self, BS_DIAL_GRID_REGION (region));
      else if (BS_IS_TOUCHSCREEN_REGION (region))
        add_touchscreen (self, BS_TOUCHSCREEN_REGION (region));
      else
        g_assert_not_reached ();
    }
}


/*
 * Callbacks
 */

static void
on_selection_controller_selection_changed_cb (BsSelectionController *selection_controller,
                                              BsDeviceEditor        *self)
{
  gpointer owner, item;

  if (!bs_selection_controller_get_selection (selection_controller, &owner, &item))
    return; // TODO: set empty page

  set_selected_item (self, item);
}


/*
 * GObject overrides
 */

static void
bs_device_editor_finalize (GObject *object)
{
  BsDeviceEditor *self = (BsDeviceEditor *)object;

  g_clear_pointer (&self->region_to_widget, g_hash_table_destroy);
  g_clear_object (&self->selection_controller);
  g_clear_object (&self->stream_deck);

  G_OBJECT_CLASS (bs_device_editor_parent_class)->finalize (object);
}

static void
bs_device_editor_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  BsDeviceEditor *self = BS_DEVICE_EDITOR (object);

  switch (prop_id)
    {
    case PROP_STREAM_DECK:
      g_value_set_object (value, self->stream_deck);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_device_editor_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  BsDeviceEditor *self = BS_DEVICE_EDITOR (object);

  switch (prop_id)
    {
    case PROP_STREAM_DECK:
      g_assert (self->stream_deck == NULL);
      self->stream_deck = g_value_dup_object (value);
      build_regions (self);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_device_editor_class_init (BsDeviceEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = bs_device_editor_finalize;
  object_class->get_property = bs_device_editor_get_property;
  object_class->set_property = bs_device_editor_set_property;

  properties[PROP_STREAM_DECK] = g_param_spec_object ("stream-deck", NULL, NULL,
                                                      BS_TYPE_STREAM_DECK,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/com/feaneron/Boatswain/bs-device-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, BsDeviceEditor, editor_bin);
  gtk_widget_class_bind_template_child (widget_class, BsDeviceEditor, empty_page);
  gtk_widget_class_bind_template_child (widget_class, BsDeviceEditor, regions_grid);

  gtk_widget_class_set_css_name (widget_class, "streamdeckeditor");
}

static void
bs_device_editor_init (BsDeviceEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->region_to_widget = g_hash_table_new (g_direct_hash, g_direct_equal);
  self->selection_controller = bs_selection_controller_new ();
  g_signal_connect (self->selection_controller,
                    "selection-changed",
                    G_CALLBACK (on_selection_controller_selection_changed_cb),
                    self);
}

GtkWidget *
bs_device_editor_new (BsStreamDeck *stream_deck)
{
  return g_object_new (BS_TYPE_DEVICE_EDITOR,
                       "stream-deck", stream_deck,
                       NULL);
}
