/* bs-page-item.c
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

#include "bs-action-private.h"
#include "bs-action-factory.h"
#include "bs-application-private.h"
#include "bs-enum-types.h"
#include "bs-empty-action.h"
#include "bs-icon.h"
#include "bs-page.h"
#include "bs-page-item.h"

#include <libpeas.h>

struct _BsPageItem
{
  GObject parent_instance;

  BsPage *page;

  char *action;
  BsPageItemType item_type;
  char *factory;
  JsonNode *settings;
  JsonNode *custom_icon;
};

G_DEFINE_FINAL_TYPE (BsPageItem, bs_page_item, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_ACTION,
  PROP_CUSTOM_ICON,
  PROP_FACTORY,
  PROP_PAGE,
  PROP_SETTINGS,
  PROP_TYPE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

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


/*
 * GObject overrides
 */

static void
bs_page_item_finalize (GObject *object)
{
  BsPageItem *self = (BsPageItem *)object;

  g_clear_pointer (&self->action, g_free);
  g_clear_pointer (&self->factory, g_free);
  g_clear_pointer (&self->settings, json_node_unref);
  g_clear_pointer (&self->custom_icon, json_node_unref);

  G_OBJECT_CLASS (bs_page_item_parent_class)->finalize (object);
}

static void
bs_page_item_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  BsPageItem *self = BS_PAGE_ITEM (object);

  switch (prop_id)
    {
    case PROP_ACTION:
      g_value_set_string (value, self->action);
      break;

    case PROP_CUSTOM_ICON:
      g_value_set_boxed (value, self->custom_icon);
      break;

    case PROP_FACTORY:
      g_value_set_string (value, self->factory);
      break;

    case PROP_PAGE:
      g_value_set_object (value, self->page);
      break;

    case PROP_SETTINGS:
      g_value_set_boxed (value, self->settings);
      break;

    case PROP_TYPE:
      g_value_set_enum (value, self->item_type);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_page_item_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  BsPageItem *self = BS_PAGE_ITEM (object);

  switch (prop_id)
    {
    case PROP_ACTION:
      bs_page_item_set_action (self, g_value_get_string (value));
      break;

    case PROP_CUSTOM_ICON:
      bs_page_item_set_custom_icon (self, g_value_get_boxed (value));
      break;

    case PROP_FACTORY:
      bs_page_item_set_factory (self, g_value_get_string (value));
      break;

    case PROP_PAGE:
      g_assert (self->page == NULL);
      self->page = g_value_get_object (value);
      break;

    case PROP_SETTINGS:
      bs_page_item_set_settings (self, g_value_get_boxed (value));
      break;

    case PROP_TYPE:
      bs_page_item_set_item_type (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_page_item_class_init (BsPageItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = bs_page_item_finalize;
  object_class->get_property = bs_page_item_get_property;
  object_class->set_property = bs_page_item_set_property;

  properties[PROP_ACTION] = g_param_spec_string ("action", NULL, NULL,
                                                 "",
                                                 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_CUSTOM_ICON] = g_param_spec_boxed ("custom-icon", NULL, NULL,
                                                     JSON_TYPE_NODE,
                                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_FACTORY] = g_param_spec_string ("factory", NULL, NULL,
                                                  "",
                                                  G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_PAGE] = g_param_spec_object ("page", NULL, NULL,
                                               BS_TYPE_PAGE,
                                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_SETTINGS] = g_param_spec_boxed ("settings", NULL, NULL,
                                                  JSON_TYPE_NODE,
                                                  G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_TYPE] = g_param_spec_enum ("type", NULL, NULL,
                                             BS_TYPE_PAGE_ITEM_TYPE,
                                             BS_PAGE_ITEM_EMPTY,
                                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
bs_page_item_init (BsPageItem *self)
{
}

BsPageItem *
bs_page_item_new (BsPage *page)
{
  return g_object_new (BS_TYPE_PAGE_ITEM,
                       "page", page,
                       NULL);
}

BsPageItem *
bs_page_item_new_from_json (BsPage   *page,
                            JsonNode *node)
{
  g_autoptr (GEnumClass) enum_class = NULL;
  g_autoptr (BsPageItem) page_item = NULL;
  GEnumValue *enum_value;
  JsonObject *object;

  if (!JSON_NODE_HOLDS_OBJECT (node))
    {
      g_warning ("JSON node is not an object");
      return bs_page_item_new (page);
    }

  object = json_node_get_object (node);

  enum_class = g_type_class_ref (BS_TYPE_PAGE_ITEM_TYPE);
  enum_value = g_enum_get_value_by_nick (enum_class, json_object_get_string_member_with_default (object, "type", ""));
  if (!enum_value)
    enum_value = g_enum_get_value_by_name (enum_class, json_object_get_string_member_with_default (object, "type", ""));

  page_item = g_object_new (BS_TYPE_PAGE_ITEM,
                            "page", page,
                            "type", enum_value ? enum_value->value : BS_PAGE_ITEM_EMPTY,
                            "factory", json_object_get_string_member_with_default (object, "factory", NULL),
                            "action", json_object_get_string_member_with_default (object, "action", NULL),
                            "settings", json_object_get_member (object, "settings"),
                            "custom-icon", json_object_get_member (object, "custom-icon"),
                            NULL);

  return g_steal_pointer (&page_item);
}

JsonNode *
bs_page_item_to_json (BsPageItem *self)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (GEnumClass) enum_class = NULL;
  GEnumValue *enum_value;

  g_return_val_if_fail (BS_IS_PAGE_ITEM (self), NULL);

  builder = json_builder_new ();

  json_builder_begin_object (builder);

  enum_class = g_type_class_ref (BS_TYPE_PAGE_ITEM_TYPE);
  enum_value = g_enum_get_value (enum_class, self->item_type);

  switch (self->item_type)
    {
    case BS_PAGE_ITEM_EMPTY:
      if (self->custom_icon)
        {
          json_builder_set_member_name (builder, "custom-icon");
          json_builder_add_value (builder, json_node_copy (self->custom_icon));
        }
      break;

    case BS_PAGE_ITEM_ACTION:
      json_builder_set_member_name (builder, "type");
      json_builder_add_string_value (builder, enum_value->value_nick);

      json_builder_set_member_name (builder, "factory");
      json_builder_add_string_value (builder, self->factory);

      json_builder_set_member_name (builder, "action");
      json_builder_add_string_value (builder, self->action);

      if (self->custom_icon)
        {
          json_builder_set_member_name (builder, "custom-icon");
          json_builder_add_value (builder, json_node_copy (self->custom_icon));
        }

      if (self->settings)
        {
          json_builder_set_member_name (builder, "settings");
          json_builder_add_value (builder, json_node_copy (self->settings));
        }
      break;
    }

  json_builder_end_object (builder);

  return json_builder_get_root (builder);
}

BsPage *
bs_page_item_get_page (BsPageItem *self)
{
  g_return_val_if_fail (BS_IS_PAGE_ITEM (self), NULL);

  return self->page;
}

JsonNode *
bs_page_item_get_custom_icon (BsPageItem *self)
{
  g_return_val_if_fail (BS_IS_PAGE_ITEM (self), NULL);

  return self->custom_icon;
}

void
bs_page_item_set_custom_icon (BsPageItem *self,
                              JsonNode   *custom_icon)
{
  g_return_if_fail (BS_IS_PAGE_ITEM (self));
  g_return_if_fail (custom_icon == NULL || JSON_NODE_HOLDS_OBJECT (custom_icon));

  if (self->custom_icon == custom_icon)
    return;

  if (custom_icon)
    {
      g_autoptr (JsonGenerator) generator = json_generator_new ();
      json_generator_set_pretty (generator, TRUE);
      json_generator_set_root (generator, json_node_copy (custom_icon));
    }

  g_clear_pointer (&self->custom_icon, json_node_unref);
  self->custom_icon = custom_icon ? json_node_ref (custom_icon) : NULL;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CUSTOM_ICON]);
}

const char *
bs_page_item_get_action (BsPageItem *self)
{
  g_return_val_if_fail (BS_IS_PAGE_ITEM (self), NULL);

  return self->action;
}

void
bs_page_item_set_action (BsPageItem *self,
                         const char   *action)
{
  g_return_if_fail (BS_IS_PAGE_ITEM (self));

  if (g_strcmp0 (self->action, action) == 0)
    return;

  g_clear_pointer (&self->action, g_free);
  self->action = g_strdup (action);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTION]);
}

BsPageItemType
bs_page_item_get_item_type (BsPageItem *self)
{
  g_return_val_if_fail (BS_IS_PAGE_ITEM (self), 0);

  return self->item_type;
}

void
bs_page_item_set_item_type (BsPageItem     *self,
                            BsPageItemType  item_type)
{
  g_return_if_fail (BS_IS_PAGE_ITEM (self));

  if (self->item_type == item_type)
    return;

  self->item_type = item_type;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TYPE]);
}

const char *
bs_page_item_get_factory (BsPageItem *self)
{
  g_return_val_if_fail (BS_IS_PAGE_ITEM (self), NULL);

  return self->factory;
}

void
bs_page_item_set_factory (BsPageItem *self,
                          const char   *factory)
{
  g_return_if_fail (BS_IS_PAGE_ITEM (self));

  if (g_strcmp0 (self->factory, factory) == 0)
    return;

  g_clear_pointer (&self->factory, g_free);
  self->factory = g_strdup (factory);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FACTORY]);
}

JsonNode *
bs_page_item_get_settings (BsPageItem *self)
{
  g_return_val_if_fail (BS_IS_PAGE_ITEM (self), NULL);

  return self->settings;
}

void
bs_page_item_set_settings (BsPageItem *self,
                           JsonNode     *settings)
{
  g_return_if_fail (BS_IS_PAGE_ITEM (self));
  g_return_if_fail (settings == NULL || JSON_NODE_HOLDS_OBJECT (settings));

  if (self->settings == settings)
    return;

  g_clear_pointer (&self->settings, json_node_unref);
  self->settings = settings ? json_node_ref (settings) : NULL;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SETTINGS]);
}

gboolean
bs_page_item_realize (BsPageItem  *self,
                      BsIcon     **out_custom_icon,
                      BsAction   **out_action,
                      GError     **error)
{
  g_return_val_if_fail (BS_IS_PAGE_ITEM (self), FALSE);
  g_return_val_if_fail (out_custom_icon != NULL, FALSE);
  g_return_val_if_fail (out_action != NULL, FALSE);

  if (out_action)
    {
      g_autoptr (BsAction) action = NULL;
      BsActionFactory *action_factory;
      BsActionInfo *action_info;

      switch (self->item_type)
        {
        case BS_PAGE_ITEM_EMPTY:
          action = bs_empty_action_new ();
          break;

        case BS_PAGE_ITEM_ACTION:
          action_factory = get_action_factory (self->factory);

          if (!action_factory)
            {
              g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           "No action factory named \"%s\"", self->factory);
              *out_custom_icon = NULL;
              *out_action = NULL;
              return FALSE;
            }

          action_info = bs_action_factory_get_info (action_factory, self->action);
          action = bs_action_factory_create_action (action_factory, action_info);
          if (self->settings)
            bs_action_deserialize_settings (action, json_node_get_object (self->settings));
          break;
        }

      *out_action = g_steal_pointer (&action);
    }

  if (out_custom_icon)
    {
      g_autoptr (BsIcon) custom_icon = NULL;

      if (self->custom_icon)
        custom_icon = bs_icon_new_from_json (self->custom_icon, NULL);

      *out_custom_icon = g_steal_pointer (&custom_icon);
    }

  return TRUE;
}
