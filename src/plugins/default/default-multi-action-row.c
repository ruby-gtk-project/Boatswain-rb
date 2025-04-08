/* default-multi-action-row.c
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

#include "default-multi-action-row.h"

#include <boatswain.h>
#include <glib/gi18n.h>

struct _DefaultMultiActionRow
{
  AdwActionRow parent_instance;

  GtkAdjustment *delay_adjustment;
  GtkWidget *delay_spinbutton;
  GtkWidget *edit_button;
  GtkImage *icon;

  MultiActionEntry *entry;
};

G_DEFINE_FINAL_TYPE (DefaultMultiActionRow, default_multi_action_row, ADW_TYPE_ACTION_ROW)

enum
{
  CHANGED,
  EDIT,
  REMOVE,
  N_SIGNALS,
};

static guint signals[N_SIGNALS];


/*
 * Callbacks
 */

static void
on_delay_adjustment_value_changed_cb (GtkAdjustment         *delay_adjustment,
                                      GParamSpec            *pspec,
                                      DefaultMultiActionRow *self)
{
  g_assert (self->entry->entry_type == MULTI_ACTION_ENTRY_DELAY);
  self->entry->v.delay_ms = gtk_adjustment_get_value (delay_adjustment);
  g_signal_emit (self, signals[CHANGED], 0);
}

static void
on_edit_button_clicked_cb (GtkButton             *button,
                           DefaultMultiActionRow *self)
{
  g_signal_emit (self, signals[EDIT], 0);
}

static void
on_remove_button_clicked_cb (GtkButton             *button,
                             DefaultMultiActionRow *self)
{
  g_signal_emit (self, signals[REMOVE], 0);
}


/*
 * GObject overrides
 */

static void
default_multi_action_row_class_init (DefaultMultiActionRowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  signals[CHANGED] = g_signal_new ("changed",
                                   DEFAULT_TYPE_MULTI_ACTION_ROW,
                                   G_SIGNAL_RUN_LAST,
                                   0, NULL, NULL, NULL,
                                   G_TYPE_NONE,
                                   0);

  signals[EDIT] = g_signal_new ("edit",
                                DEFAULT_TYPE_MULTI_ACTION_ROW,
                                G_SIGNAL_RUN_LAST,
                                0, NULL, NULL, NULL,
                                G_TYPE_NONE,
                                0);

  signals[REMOVE] = g_signal_new ("remove",
                                  DEFAULT_TYPE_MULTI_ACTION_ROW,
                                  G_SIGNAL_RUN_LAST,
                                  0, NULL, NULL, NULL,
                                  G_TYPE_NONE,
                                  0);

  gtk_widget_class_set_template_from_resource (widget_class, "/com/feaneron/Boatswain/plugins/default/default-multi-action-row.ui");

  gtk_widget_class_bind_template_child (widget_class, DefaultMultiActionRow, delay_adjustment);
  gtk_widget_class_bind_template_child (widget_class, DefaultMultiActionRow, delay_spinbutton);
  gtk_widget_class_bind_template_child (widget_class, DefaultMultiActionRow, edit_button);
  gtk_widget_class_bind_template_child (widget_class, DefaultMultiActionRow, icon);

  gtk_widget_class_bind_template_callback (widget_class, on_delay_adjustment_value_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_edit_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_remove_button_clicked_cb);
}

static void
default_multi_action_row_init (DefaultMultiActionRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
default_multi_action_row_new (MultiActionEntry *entry)

{
  DefaultMultiActionRow *self;

  self = g_object_new (DEFAULT_TYPE_MULTI_ACTION_ROW, NULL);
  self->entry = entry;

  switch (entry->entry_type)
    {
    case MULTI_ACTION_ENTRY_DELAY:
      gtk_image_set_from_icon_name (self->icon, "preferences-system-time-symbolic");
      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self), _("Delay"));
      gtk_widget_set_visible (self->delay_spinbutton, TRUE);
      g_signal_handlers_block_by_func (self->delay_adjustment,
                                       on_delay_adjustment_value_changed_cb,
                                       self);
      gtk_adjustment_set_value (self->delay_adjustment, entry->v.delay_ms);
      g_signal_handlers_unblock_by_func (self->delay_adjustment,
                                         on_delay_adjustment_value_changed_cb,
                                         self);
      break;

    case MULTI_ACTION_ENTRY_ACTION:
      {
        g_autoptr (BsActionInfo) info = NULL;
        BsActionFactory *factory;

        factory = bs_action_get_factory (entry->v.action);
        info = bs_action_factory_get_info (factory, bs_action_get_id (entry->v.action));

        gtk_image_set_from_icon_name (self->icon, bs_action_info_get_icon_name (info));
        adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self),
                                       bs_action_get_name (entry->v.action));

        gtk_widget_set_visible (self->edit_button, TRUE);
      }
      break;
    }

  return GTK_WIDGET (self);
}

MultiActionEntry *
default_multi_action_row_get_entry (DefaultMultiActionRow *self)
{
  g_return_val_if_fail (DEFAULT_IS_MULTI_ACTION_ROW (self), NULL);

  return self->entry;
}
