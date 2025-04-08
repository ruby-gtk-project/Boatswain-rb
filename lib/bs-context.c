/*
 * bs-context.c
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

#include "bs-action-factory.h"
#include "bs-context.h"
#include "bs-device-manager-private.h"
#include "bs-desktop-controller-private.h"

#include <libpeas.h>

struct _BsContext
{
  GObject parent_instance;

  BsDeviceManager *device_manager;
  BsDesktopController *desktop_controller;
  PeasExtensionSet *action_factories_set;
};

G_DEFINE_FINAL_TYPE (BsContext, bs_context, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_DEVICES,
  N_PROPS,
};

static GParamSpec *properties [N_PROPS];


/*
 * Auxiliary methods
 */

static void
load_plugin (PeasEngine     *engine,
             PeasPluginInfo *plugin_info)
{
  g_autofree char *icons_dir = NULL;
  GtkIconTheme *icon_theme;
  const char *plugin_datadir;

  peas_engine_load_plugin (engine, plugin_info);

  /* Add icons */
  plugin_datadir = peas_plugin_info_get_data_dir (plugin_info);

  if (g_str_has_prefix (plugin_datadir, "resource://"))
    plugin_datadir += strlen ("resource://");
  icons_dir = g_strdup_printf ("%s/icons", plugin_datadir);

  icon_theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
  gtk_icon_theme_add_resource_path (icon_theme, icons_dir);
}


/*
 * GObject overrides
 */

static void
bs_context_finalize (GObject *object)
{
  BsContext *self = (BsContext *)object;

  g_clear_object (&self->desktop_controller);

  G_OBJECT_CLASS (bs_context_parent_class)->finalize (object);
}

static void
bs_context_constructed (GObject *object)
{
  PeasEngine *engine;
  BsContext *self;

  self = (BsContext *) object;

  G_OBJECT_CLASS (bs_context_parent_class)->constructed (object);

  self->desktop_controller = bs_desktop_controller_new ();

  /* All plugins must be loaded before profiles and Stream Decks */
  engine = peas_engine_get_default ();
  peas_engine_enable_loader (engine, "gjs");
  peas_engine_add_search_path (engine,
                               "resource:///com/feaneron/Boatswain/plugins",
                               "resource:///com/feaneron/Boatswain/plugins");

  for (uint32_t i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (engine)); i++)
    {
      g_autoptr (PeasPluginInfo) plugin_info = NULL;

      plugin_info = g_list_model_get_item (G_LIST_MODEL (engine), i);
      load_plugin (engine, plugin_info);
    }

  self->action_factories_set = peas_extension_set_new (peas_engine_get_default (),
                                                       BS_TYPE_ACTION_FACTORY,
                                                       "context", self,
                                                       NULL);

  self->device_manager = bs_device_manager_new ();
}

static void
bs_context_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  BsContext *self = BS_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_DEVICES:
      g_value_set_object (value, self->device_manager);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_context_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
bs_context_class_init (BsContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = bs_context_finalize;
  object_class->constructed = bs_context_constructed;
  object_class->get_property = bs_context_get_property;
  object_class->set_property = bs_context_set_property;

  properties[PROP_DEVICES] = g_param_spec_object ("devices", NULL, NULL,
                                                  G_TYPE_LIST_MODEL,
                                                  G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
bs_context_init (BsContext *self)
{
}

static BsContext *default_context = NULL;
static size_t default_context_initialized = 0;

void
bs_context_init_default (GError **out_error)
{
  if (g_once_init_enter (&default_context_initialized))
    {
      default_context = g_object_new (BS_TYPE_CONTEXT, NULL);
      g_once_init_leave (&default_context_initialized, TRUE);

      bs_device_manager_load (default_context->device_manager, out_error);
    }
}

/**
 * bs_context_get_default:
 *
 * Returns: (transfer none):
 */
BsContext *
bs_context_get_default (void)
{
  g_assert (default_context_initialized);

  return default_context;
}

/**
 * bs_context_get_devices:
 *
 * Returns: (transfer none):
 */
GListModel *
bs_context_get_devices (BsContext *self)
{
  g_return_val_if_fail (BS_IS_CONTEXT (self), NULL);

  return G_LIST_MODEL (self->device_manager);
}

/**
 * bs_context_get_desktop_controller:
 *
 * Returns: (transfer none):
 */
BsDesktopController *
bs_context_get_desktop_controller (BsContext *self)
{
  g_return_val_if_fail (BS_IS_CONTEXT (self), NULL);

  return self->desktop_controller;
}

/**
 * bs_context_get_available_action_factories:
 *
 * Returns: (transfer none):
 */
GListModel *
bs_context_get_available_action_factories (BsContext *self)
{
  g_return_val_if_fail (BS_IS_CONTEXT (self), NULL);

  return G_LIST_MODEL (self->action_factories_set);
}

