/*
 * bs-touchscreen-background-dialog.c
 *
 * Copyright 2025 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#define G_LOG_DOMAIN "Touchscreen Background Dialog"

#include "bs-touchscreen-background-dialog.h"

#include "bs-touchscreen-content.h"
#include "bs-touchscreen-private.h"

#include <glib/gi18n.h>

struct _BsTouchscreenBackgroundDialog
{
  AdwDialog parent_instance;

  GSimpleActionGroup *action_group;
  BsTouchscreen *touchscreen;

  AdwPreferencesGroup *adwaita_group;
  AdwPreferencesGroup *colors_group;
};

G_DEFINE_FINAL_TYPE (BsTouchscreenBackgroundDialog, bs_touchscreen_background_dialog, ADW_TYPE_DIALOG)

enum {
  PROP_0,
  PROP_TOUCHSCREEN,
  N_PROPS,
};

static GParamSpec *properties [N_PROPS];


/*
 * Auxiliary functions
 */

static void
populate_builtins (BsTouchscreenBackgroundDialog *self)
{
  struct {
    AdwPreferencesGroup *group;
    const char *title;
    const char *resource_path;
  } builtin_groups[] = {
    /* Adwaita */
    { self->adwaita_group, N_("Adwaita"), "/com/feaneron/Boatswain/assets/backgrounds/adwaita.png" },
    { self->adwaita_group, N_("Glass"), "/com/feaneron/Boatswain/assets/backgrounds/glass2.png" },
    { self->adwaita_group, N_("Oh Sheet!"), "/com/feaneron/Boatswain/assets/backgrounds/oh-sheet.png" },
    { self->adwaita_group, N_("Pills"), "/com/feaneron/Boatswain/assets/backgrounds/pills.png" },
    { self->adwaita_group, N_("Swoosh"), "/com/feaneron/Boatswain/assets/backgrounds/swoosh.png" },
  };

  for (size_t i = 0; i < G_N_ELEMENTS (builtin_groups); i++)
    {
      g_autofree char *resource_uri = NULL;
      GtkWidget *picture;
      GtkWidget *row;

      picture = gtk_picture_new_for_resource (builtin_groups[i].resource_path);
      gtk_picture_set_content_fit (GTK_PICTURE (picture), GTK_CONTENT_FIT_COVER);

      row = adw_preferences_row_new ();
      gtk_widget_set_size_request (row, -1, 80);
      gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE);
      gtk_widget_set_overflow (row, GTK_OVERFLOW_HIDDEN);
      gtk_widget_add_css_class (row, "touchscreen-background");
      gtk_widget_set_tooltip_text (row, _(builtin_groups[i].title));
      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), _(builtin_groups[i].title));
      gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), picture);

      resource_uri = g_strdup_printf ("resource://%s", builtin_groups[i].resource_path);
      gtk_actionable_set_action_name (GTK_ACTIONABLE (row), "backgrounddialog.set-background");
      gtk_actionable_set_action_target (GTK_ACTIONABLE (row), "s", resource_uri);

      adw_preferences_group_add (builtin_groups[i].group, row);
    }
}


/*
 * Callbacks
 */

static void
on_set_background_action_activated_cb (GSimpleAction *action,
                                       GVariant      *parameter,
                                       gpointer       user_data)
{
  BsTouchscreenBackgroundDialog *self = (BsTouchscreenBackgroundDialog *) user_data;
  g_autoptr (GdkPaintable) paintable = NULL;
  g_autoptr (GdkTexture) texture = NULL;
  BsTouchscreenContent *content;
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) file = NULL;
  g_autofree char *uri = NULL;

  g_assert (BS_IS_TOUCHSCREEN_BACKGROUND_DIALOG (self));
  g_assert (g_variant_is_of_type (parameter, G_VARIANT_TYPE_STRING));

  uri = g_variant_dup_string (parameter, NULL);
  g_debug ("Setting touchscreen background to %s", uri);

  file = g_file_new_for_uri (uri);

  content = bs_touchscreen_get_content (self->touchscreen);
  bs_touchscreen_content_set_background_from_file (content, file);
}

static void
on_set_default_background_action_activated_cb (GSimpleAction *action,
                                               GVariant      *parameter,
                                               gpointer       user_data)
{
  BsTouchscreenBackgroundDialog *self = (BsTouchscreenBackgroundDialog *) user_data;
  BsTouchscreenContent *content;

  g_assert (BS_IS_TOUCHSCREEN_BACKGROUND_DIALOG (self));

  g_debug ("Setting default touchscreen background");

  content = bs_touchscreen_get_content (self->touchscreen);
  bs_touchscreen_content_set_default_background (content);
}


/*
 * GObject overrides
 */

static void
bs_touchscreen_background_dialog_dispose (GObject *object)
{
  BsTouchscreenBackgroundDialog *self = (BsTouchscreenBackgroundDialog *)object;

  g_clear_object (&self->touchscreen);

  gtk_widget_dispose_template (GTK_WIDGET (self), BS_TYPE_TOUCHSCREEN_BACKGROUND_DIALOG);

  G_OBJECT_CLASS (bs_touchscreen_background_dialog_parent_class)->dispose (object);
}

static void
bs_touchscreen_background_dialog_get_property (GObject    *object,
                                               guint       prop_id,
                                               GValue     *value,
                                               GParamSpec *pspec)
{
  BsTouchscreenBackgroundDialog *self = BS_TOUCHSCREEN_BACKGROUND_DIALOG (object);

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
bs_touchscreen_background_dialog_set_property (GObject      *object,
                                               guint         prop_id,
                                               const GValue *value,
                                               GParamSpec   *pspec)
{
  BsTouchscreenBackgroundDialog *self = BS_TOUCHSCREEN_BACKGROUND_DIALOG (object);

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
bs_touchscreen_background_dialog_class_init (BsTouchscreenBackgroundDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = bs_touchscreen_background_dialog_dispose;
  object_class->get_property = bs_touchscreen_background_dialog_get_property;
  object_class->set_property = bs_touchscreen_background_dialog_set_property;

  properties[PROP_TOUCHSCREEN] =
    g_param_spec_object ("touchscreen", NULL, NULL,
                         BS_TYPE_TOUCHSCREEN,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/com/feaneron/Boatswain/bs-touchscreen-background-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, BsTouchscreenBackgroundDialog, adwaita_group);
}

static void
bs_touchscreen_background_dialog_init (BsTouchscreenBackgroundDialog *self)
{
  const GActionEntry actions[] = {
    { "set-background", on_set_background_action_activated_cb, "s", },
    { "set-default-background", on_set_default_background_action_activated_cb },
  };

  self->action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (self->action_group),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);

  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "backgrounddialog",
                                  G_ACTION_GROUP (self->action_group));

  gtk_widget_init_template (GTK_WIDGET (self));

  populate_builtins (self);
}

GtkWidget *
bs_touchscreen_background_dialog_new (BsTouchscreen *touchscreen)
{
  return g_object_new (BS_TYPE_TOUCHSCREEN_BACKGROUND_DIALOG,
                       "touchscreen", touchscreen,
                       NULL);
}
