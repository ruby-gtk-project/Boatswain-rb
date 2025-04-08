/*
 * bs-touchscreen-background-row.c
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

#include "bs-touchscreen-background-row.h"

#include "bs-touchscreen-background-dialog.h"

#include <boatswain.h>
#include <glib/gi18n.h>

struct _BsTouchscreenBackgroundRow
{
  AdwPreferencesRow parent_instance;

  GSimpleActionGroup *action_group;
  BsTouchscreen *touchscreen;
};

G_DEFINE_FINAL_TYPE (BsTouchscreenBackgroundRow, bs_touchscreen_background_row, ADW_TYPE_PREFERENCES_ROW)

enum {
  PROP_0,
  PROP_TOUCHSCREEN,
  N_PROPS,
};

static GParamSpec *properties [N_PROPS];


/*
 * Callbacks
 */

static void
on_select_background_action_activated_cb (GSimpleAction *action,
                                          GVariant      *parameter,
                                          gpointer       user_data)
{
  BsTouchscreenBackgroundRow *self = (BsTouchscreenBackgroundRow *) user_data;
  GtkWidget *dialog;

  dialog = bs_touchscreen_background_dialog_new (self->touchscreen);
  adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (self));
}


/*
 * GObject overrides
 */

static void
bs_touchscreen_background_row_dispose (GObject *object)
{
  BsTouchscreenBackgroundRow *self = (BsTouchscreenBackgroundRow *)object;

  g_clear_object (&self->touchscreen);

  gtk_widget_dispose_template (GTK_WIDGET (self), BS_TYPE_TOUCHSCREEN_BACKGROUND_ROW);

  G_OBJECT_CLASS (bs_touchscreen_background_row_parent_class)->dispose (object);
}

static void
bs_touchscreen_background_row_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
  BsTouchscreenBackgroundRow *self = BS_TOUCHSCREEN_BACKGROUND_ROW (object);

  switch (prop_id)
    {
    case PROP_TOUCHSCREEN:
      g_value_set_object (value, self->touchscreen);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_touchscreen_background_row_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
  BsTouchscreenBackgroundRow *self = BS_TOUCHSCREEN_BACKGROUND_ROW (object);

  switch (prop_id)
    {
    case PROP_TOUCHSCREEN:
      if (g_set_object (&self->touchscreen, g_value_get_object (value)))
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TOUCHSCREEN]);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_touchscreen_background_row_class_init (BsTouchscreenBackgroundRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = bs_touchscreen_background_row_dispose;
  object_class->get_property = bs_touchscreen_background_row_get_property;
  object_class->set_property = bs_touchscreen_background_row_set_property;

  properties[PROP_TOUCHSCREEN] =
    g_param_spec_object ("touchscreen", NULL, NULL,
                         BS_TYPE_TOUCHSCREEN,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/com/feaneron/Boatswain/bs-touchscreen-background-row.ui");
}

static void
bs_touchscreen_background_row_init (BsTouchscreenBackgroundRow *self)
{
  const GActionEntry actions[] = {
    { "select-background", on_select_background_action_activated_cb, },
  };

  self->action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (self->action_group),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);

  gtk_widget_insert_action_group (GTK_WIDGET (self), "backgroundrow", G_ACTION_GROUP (self->action_group));

  gtk_widget_init_template (GTK_WIDGET (self));
}
