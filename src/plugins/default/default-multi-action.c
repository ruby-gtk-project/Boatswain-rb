/* default-multi-action.c
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

#define G_LOG_DOMAIN "Multiaction"

#include "bs-action-factory.h"
#include "bs-action-private.h"
#include "bs-application-private.h"
#include "bs-events.h"
#include "bs-icon.h"
#include "default-multi-action-editor.h"
#include "default-multi-action-private.h"

#include <libpeas.h>

struct _DefaultMultiAction
{
  BsAction parent_instance;

  GCancellable *cancellable;
  BsEvent *event;
  guint run_source_id;
  guint current_entry;

  GPtrArray *entries;

  GtkWidget *editor;
};

G_DEFINE_FINAL_TYPE (DefaultMultiAction, default_multi_action, BS_TYPE_ACTION)


/*
 * Auxiliary methods
 */

typedef struct
{
  const char *factory_id;
  BsActionFactory *factory;
} FindFactoryData;

static void
find_action_factory_cb (PeasExtensionSet *set,
                        PeasPluginInfo   *info,
                        GObject          *extension,
                        gpointer          data)
{
  FindFactoryData *find_data = data;

  if (g_strcmp0 (peas_plugin_info_get_module_name (info), find_data->factory_id) == 0)
    find_data->factory = BS_ACTION_FACTORY (extension);
}

static BsActionFactory *
get_action_factory (const char *factory_id)
{
  FindFactoryData find_data;
  GApplication *application;

  find_data.factory_id = factory_id;
  find_data.factory = NULL;

  application = g_application_get_default ();
  peas_extension_set_foreach (bs_application_get_action_factory_set (BS_APPLICATION (application)),
                              find_action_factory_cb,
                              &find_data);

  return find_data.factory;
}

static const char *
get_factory_from_action (BsAction *action)
{
  const PeasPluginInfo *plugin_info;
  BsActionFactory *factory;

  factory = bs_action_get_factory (action);
  plugin_info = peas_extension_base_get_plugin_info (PEAS_EXTENSION_BASE (factory));

  return peas_plugin_info_get_module_name (plugin_info);
}

static void
multi_action_entry_free (gpointer data)
{
  MultiActionEntry *entry;

  if (!data)
    return;

  entry = data;

  switch (entry->entry_type)
    {
    case MULTI_ACTION_ENTRY_DELAY:
      break;

    case MULTI_ACTION_ENTRY_ACTION:
      g_clear_object (&entry->v.action);
      break;
    }

  g_free (entry);
}

static void
cancel_ongoing_run (DefaultMultiAction *self)
{
  gboolean needs_unref = self->run_source_id > 0;

  g_cancellable_cancel (self->cancellable);
  g_cancellable_reset (self->cancellable);
  g_assert (self->run_source_id == 0);

  g_clear_object (&self->event);
  self->current_entry = 0;

  if (needs_unref)
    g_object_unref (self);
}


/*
 * Callbacks
 */

static gboolean
run_actions_in_idle_cb (gpointer data)
{
  DefaultMultiAction *self = DEFAULT_MULTI_ACTION (data);
  MultiActionEntry *entry;

  if (self->current_entry < self->entries->len)
    entry = g_ptr_array_index (self->entries, self->current_entry);
  else
    entry = NULL;

  if (!entry)
    {
      g_clear_object (&self->event);
      g_object_unref (self);
      self->run_source_id = 0;
      return G_SOURCE_REMOVE;
    }

  self->current_entry++;

  switch (entry->entry_type)
    {
    case MULTI_ACTION_ENTRY_DELAY:
      g_debug ("Running %ums delay", entry->v.delay_ms);
      self->run_source_id = g_timeout_add (entry->v.delay_ms, run_actions_in_idle_cb, self);
      return G_SOURCE_REMOVE;

    case MULTI_ACTION_ENTRY_ACTION:
      g_debug ("Running action '%s'", bs_action_get_id (entry->v.action));
      bs_action_handle_event (entry->v.action, self->event);
      return G_SOURCE_CONTINUE;

    default:
      g_assert_not_reached ();
    }
}

static void
on_action_changed_cb (BsAction           *action,
                      DefaultMultiAction *self)
{
  bs_action_changed (BS_ACTION (self));
}

static void
on_cancellable_cancelled_cb (GCancellable       *cancellable,
                             DefaultMultiAction *self)
{
  g_clear_handle_id (&self->run_source_id, g_source_remove);
}


/*
 * BsAction overrides
 */

static void
default_multi_action_handle_event (BsAction *action,
                                   BsEvent  *event)
{
  DefaultMultiAction *self = DEFAULT_MULTI_ACTION (action);

  if (bs_event_get_event_type (event) != BS_BUTTON_PRESS)
    return;

  g_object_ref (self);

  cancel_ongoing_run (self);

  g_assert (self->event == NULL);
  self->event = g_object_ref (event);

  self->run_source_id = g_idle_add (run_actions_in_idle_cb, self);
}

static GtkWidget *
default_multi_action_get_preferences (BsAction *action)
{
  DefaultMultiAction *self = DEFAULT_MULTI_ACTION (action);

  return self->editor;
}

static JsonNode *
default_multi_action_serialize_settings (BsAction *action)
{
  DefaultMultiAction *self = DEFAULT_MULTI_ACTION (action);
  g_autoptr (JsonBuilder) builder = NULL;
  JsonNode *settings;
  unsigned int i;

  builder = json_builder_new ();

  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "entries");
  json_builder_begin_array (builder);
  for (i = 0; i < self->entries->len; i++)
    {
      MultiActionEntry *entry = g_ptr_array_index (self->entries, i);

      json_builder_begin_object (builder);

      switch (entry->entry_type)
        {
        case MULTI_ACTION_ENTRY_DELAY:
          json_builder_set_member_name (builder, "type");
          json_builder_add_string_value (builder, "delay");

          json_builder_set_member_name (builder, "delay");
          json_builder_add_int_value (builder, entry->v.delay_ms);
          break;

        case MULTI_ACTION_ENTRY_ACTION:
          json_builder_set_member_name (builder, "type");
          json_builder_add_string_value (builder, "action");

          json_builder_set_member_name (builder, "factory");
          json_builder_add_string_value (builder, get_factory_from_action (entry->v.action));

          json_builder_set_member_name (builder, "action");
          json_builder_add_string_value (builder, bs_action_get_id (entry->v.action));

          settings = bs_action_serialize_settings (entry->v.action);
          if (settings)
            {
              json_builder_set_member_name (builder, "settings");
              json_builder_add_value (builder, settings);
            }
          break;
        }

      json_builder_end_object (builder);
    }
  json_builder_end_array (builder);
  json_builder_end_object (builder);

  return json_builder_get_root (builder);
}

static void
default_multi_action_deserialize_settings (BsAction   *action,
                                           JsonObject *settings)
{
  DefaultMultiAction *self = DEFAULT_MULTI_ACTION (action);
  JsonArray *entries;
  unsigned int i;

  entries = json_object_get_array_member (settings, "entries");

  for (i = 0; i < json_array_get_length (entries); i++)
    {
      MultiActionEntry *entry;
      JsonObject *json_entry;
      const char *type;

      json_entry = json_array_get_object_element (entries, i);
      type = json_object_get_string_member (json_entry, "type");

      if (g_strcmp0 (type, "delay") == 0)
        {
          entry = g_new0 (MultiActionEntry, 1);
          entry->entry_type = MULTI_ACTION_ENTRY_DELAY;
          entry->v.delay_ms = json_object_get_int_member (json_entry, "delay");
        }
      else if (g_strcmp0 (type, "action") == 0)
        {
          BsActionFactory *factory;
          BsActionInfo *action_info;
          JsonNode *settings;

          factory = get_action_factory (json_object_get_string_member (json_entry, "factory"));
          action_info = bs_action_factory_get_info (factory, json_object_get_string_member (json_entry, "action"));

          entry = g_new0 (MultiActionEntry, 1);
          entry->entry_type = MULTI_ACTION_ENTRY_ACTION;
          entry->v.action = bs_action_factory_create_action (factory, action_info);

          settings = json_object_get_member (json_entry, "settings");
          if (settings)
            bs_action_deserialize_settings (entry->v.action, json_node_get_object (settings));
        }
      else
        {
          g_warning ("Invalid entry type");
          continue;
        }

      g_ptr_array_add (self->entries, entry);
    }

  default_multi_action_editor_update_entries (DEFAULT_MULTI_ACTION_EDITOR (self->editor));
}


/*
 * GObject overrides
 */

static void
default_multi_action_finalize (GObject *object)
{
  DefaultMultiAction *self = (DefaultMultiAction *)object;

  cancel_ongoing_run (self);

  g_clear_pointer (&self->entries, g_ptr_array_unref);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (default_multi_action_parent_class)->finalize (object);
}

static void
default_multi_action_class_init (DefaultMultiActionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  BsActionClass *action_class = BS_ACTION_CLASS (klass);

  object_class->finalize = default_multi_action_finalize;

  action_class->handle_event = default_multi_action_handle_event;
  action_class->serialize_settings = default_multi_action_serialize_settings;
  action_class->deserialize_settings = default_multi_action_deserialize_settings;
  action_class->get_preferences = default_multi_action_get_preferences;
}

static void
default_multi_action_init (DefaultMultiAction *self)
{
  self->entries = g_ptr_array_new_with_free_func (multi_action_entry_free);

  self->cancellable = g_cancellable_new ();
  g_cancellable_connect (self->cancellable, G_CALLBACK (on_cancellable_cancelled_cb), self, NULL);

  bs_icon_set_icon_name (bs_action_get_icon (BS_ACTION (self)),
                         "stacked-plates-symbolic");

  self->editor = default_multi_action_editor_new (self);
  g_object_ref_sink (self->editor);
}

BsAction *
default_multi_action_new (void)
{
  return g_object_new (DEFAULT_TYPE_MULTI_ACTION, NULL);
}

GPtrArray *
default_multi_action_get_entries (DefaultMultiAction *self)
{
  g_return_val_if_fail (DEFAULT_IS_MULTI_ACTION (self), NULL);

  return self->entries;
}

void
default_multi_action_add_action (DefaultMultiAction *self,
                                 BsAction           *action)
{
  MultiActionEntry *entry;

  cancel_ongoing_run (self);

  entry = g_new0 (MultiActionEntry, 1);
  entry->entry_type = MULTI_ACTION_ENTRY_ACTION;
  entry->v.action = g_object_ref (action);
  g_signal_connect (action, "changed", G_CALLBACK (on_action_changed_cb), self);

  g_ptr_array_add (self->entries, entry);

  bs_action_changed (BS_ACTION (self));
}

void
default_multi_action_add_delay (DefaultMultiAction *self,
                                unsigned int        delay_ms)
{
  MultiActionEntry *entry;

  cancel_ongoing_run (self);

  entry = g_new0 (MultiActionEntry, 1);
  entry->entry_type = MULTI_ACTION_ENTRY_DELAY;
  entry->v.delay_ms = delay_ms;

  g_ptr_array_add (self->entries, entry);

  bs_action_changed (BS_ACTION (self));
}
