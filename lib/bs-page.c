/* bs-page.c
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

#define G_LOG_DOMAIN "BsPage"

#include "bs-page-private.h"

#include "bs-actionable.h"
#include "bs-action-private.h"
#include "bs-empty-action.h"
#include "bs-icon.h"
#include "bs-page-item-private.h"
#include "bs-button.h"

#include <libpeas.h>

struct _BsPage
{
  GObject parent_instance;

  GHashTable *page_regions; /* const char* → PageRegion */

  gboolean loaded;
  gboolean root;
};

G_DEFINE_FINAL_TYPE (BsPage, bs_page, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_ROOT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];


/*
 * ItemData
 */

typedef struct
{
  BsPageItem *item;
  BsAction *action;
  BsIcon *custom_icon;
} ItemData;

static void
item_data_free (gpointer data)
{
  ItemData *item_data = data;

  g_clear_object (&item_data->item);
  g_clear_object (&item_data->action);
  g_clear_object (&item_data->custom_icon);
  g_clear_pointer (&item_data, g_free);
}

static ItemData *
item_data_new (BsPageItem *item)
{
  ItemData *item_data;

  g_assert (BS_IS_PAGE_ITEM (item));

  item_data = g_new0 (ItemData, 1);
  item_data->item = g_steal_pointer (&item);

  return g_steal_pointer (&item_data);
}

/*
 * PageRegion
 */

typedef struct
{
  char *id;
  JsonNode *region_data;
  GPtrArray *items;
} PageRegion;


static void
page_region_free (PageRegion *page_region)
{
  g_clear_pointer (&page_region->id, g_free);
  g_clear_pointer (&page_region->region_data, json_node_unref);
  g_clear_pointer (&page_region->items, g_ptr_array_unref);
  g_clear_pointer (&page_region, g_free);
}


static PageRegion *
page_region_new (const char *id)
{
  PageRegion *page_region;

  g_assert (id != NULL);

  page_region = g_new0 (PageRegion, 1);
  page_region->id = g_strdup (id);
  page_region->items = g_ptr_array_new_with_free_func (item_data_free);
  page_region->region_data = json_node_new (JSON_NODE_OBJECT);
  json_node_take_object (page_region->region_data, json_object_new ());

  return g_steal_pointer (&page_region);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (PageRegion, page_region_free);


/*
 * Auxiliary methods
 */

static inline ItemData *
get_item_data (BsPage       *self,
               const char   *region_id,
               unsigned int  position)
{
  PageRegion *page_region = g_hash_table_lookup (self->page_regions, region_id);
  ItemData *item_data = NULL;

  if (!page_region || position >= page_region->items->len)
    return NULL;

  item_data = g_ptr_array_index (page_region->items, position);
  g_assert (item_data != NULL);
  g_assert (BS_IS_PAGE_ITEM (item_data->item));

  return item_data;
}

static inline BsPageItem *
get_item (BsPage       *self,
          const char   *region_id,
          unsigned int  position)
{
  ItemData *item_data = get_item_data (self, region_id, position);

  return item_data ? item_data->item : NULL;
}

static inline PageRegion *
ensure_page_region (BsPage     *self,
                    const char *region_id)
{
  PageRegion *page_region;

  g_assert (region_id != NULL);

  page_region = g_hash_table_lookup (self->page_regions, region_id);

  if (!page_region)
    {
      page_region = page_region_new (region_id);
      g_hash_table_insert (self->page_regions, g_strdup (region_id), page_region);
    }

  return page_region;
}

static inline void
add_item (BsPage       *self,
          BsPageItem   *item,
          const char   *region_id,
          unsigned int  position)
{
  PageRegion *page_region;

  g_assert (region_id != NULL);

  page_region = ensure_page_region (self, region_id);
  g_assert (page_region != NULL);

  g_ptr_array_insert (page_region->items, position, item_data_new (item));
}

static void
ensure_first_subpage_item_is_move_up (BsPage *self)
{
  BsPageItem *item;

  if (self->root)
    return;

  item = get_item (self, "main-button-grid", 0);

  if (!item ||
      bs_page_item_get_item_type (item) != BS_PAGE_ITEM_ACTION ||
      g_strcmp0 (bs_page_item_get_factory (item), "default") != 0 ||
      g_strcmp0 (bs_page_item_get_action (item), "default-page-up-action") != 0)
    {
      /* Compatibility code: we used to use the swich-page action for this */
      if (item &&
          bs_page_item_get_item_type (item) != BS_PAGE_ITEM_ACTION &&
          g_strcmp0 (bs_page_item_get_factory (item), "default") != 0 &&
          g_strcmp0 (bs_page_item_get_action (item), "default-switch-page-action") != 0)
        {
          bs_page_item_set_item_type (item, BS_PAGE_ITEM_ACTION);
          bs_page_item_set_factory (item, "default");
          bs_page_item_set_action (item, "default-page-up-action");
          bs_page_item_set_settings (item, NULL);
        }
      else
        {
          item = bs_page_item_new ();
          bs_page_item_set_item_type (item, BS_PAGE_ITEM_ACTION);
          bs_page_item_set_factory (item, "default");
          bs_page_item_set_action (item, "default-page-up-action");
          bs_page_item_set_settings (item, NULL);

          add_item (self, item, "main-button-grid", 0);
        }
    }
}

static JsonNode *
convert_page_v0_to_v1 (JsonNode  *node,
                       GError   **error)
{
  g_autoptr (JsonBuilder) builder = NULL;

  /* Converts this:
   *
   * [
   *   { ... },
   *   { ... },
   *   { ... }.
   *   ...
   * ]
   *
   * Into this:
   *
   * {
   *   "version": 1,
   *   "regions": [
   *     {
   *       "id": "foo",
   *       "region-data": {
   *         ...
   *       },
   *       "items": [
   *         { ... },
   *         ...
   *       ]
   *   ]
   * }
   */

  g_assert (JSON_NODE_HOLDS_ARRAY (node));

  builder = json_builder_new ();

  json_builder_begin_object (builder);

  json_builder_set_member_name (builder, "version");
  json_builder_add_int_value (builder, 1);

  json_builder_set_member_name (builder, "regions");
  json_builder_begin_array (builder);
    {
      JsonArray *node_array = json_node_get_array (node);

      json_builder_begin_object (builder);

      json_builder_set_member_name (builder, "id");
      json_builder_add_string_value (builder, "main-button-grid");

      json_builder_set_member_name (builder, "region-data");
      json_builder_begin_object (builder);
      json_builder_end_object (builder);

      json_builder_set_member_name (builder, "items");
      json_builder_begin_array (builder);
      for (unsigned int i = 0; i < json_array_get_length (node_array); i++)
        {
          JsonNode *item_node = json_array_get_element (node_array, i);
          json_builder_add_value (builder, json_node_copy (item_node));
        }
      json_builder_end_array (builder);

      json_builder_end_object (builder);
    }
  json_builder_end_array (builder);

  json_builder_end_object (builder);

  return json_builder_get_root (builder);
}

typedef JsonNode * (*PageConvertFunc) (JsonNode  *node,
                                       GError   **error);
static const PageConvertFunc conversion_vtable[] = {
  convert_page_v0_to_v1,
};

static JsonNode *
convert_page (JsonNode  *node,
              GError   **error)
{
  g_autoptr (JsonNode) result = NULL;
  int64_t version;

  g_assert (node != NULL);

  result = json_node_ref (node);

  if (JSON_NODE_HOLDS_OBJECT (node))
    {
      JsonObject *object = json_node_get_object (node);
      version = json_object_get_int_member_with_default (object, "version", 1);
    }
  else
    {
      version = 0;
    }

  if (version != G_N_ELEMENTS (conversion_vtable))
    g_debug ("Converting from version %ld to %lu", version, G_N_ELEMENTS (conversion_vtable));

  for (size_t i = version; i < G_N_ELEMENTS (conversion_vtable); i++)
    {
      g_autoptr (JsonNode) new_node = NULL;

      new_node = conversion_vtable[i] (result, error);

      if (!node)
        break;

      g_clear_pointer (&result, json_node_unref);
      result = g_steal_pointer (&new_node);
    }

  return g_steal_pointer (&result);
}

static void
load_page_from_json (BsPage   *self,
                     JsonNode *node)
{
  JsonObject *object;
  JsonArray *regions;

  g_assert (JSON_NODE_HOLDS_OBJECT (node));

  /* Parses the following:
   * {
   *   "version": 1,
   *   "regions": [
   *     {
   *       "id": "foo",
   *       "region-data": {
   *         ...
   *       },
   *       "items": [
   *         { ... },
   *         ...
   *       ]
   *   ]
   * }
   */

  object = json_node_get_object (node);
  g_assert (json_object_get_int_member (object, "version") == 1);
  g_assert (json_object_has_member (object, "regions"));

  regions = json_object_get_array_member (object, "regions");
  for (guint i = 0; i < json_array_get_length (regions); i++)
    {
      g_autoptr (PageRegion) page_region = NULL;
      JsonObject *region_object;
      JsonArray *items;
      JsonNode *region;
      const char *id;

      region = json_array_get_element (regions, i);
      g_assert (JSON_NODE_HOLDS_OBJECT (region));

      region_object = json_node_get_object (region);
      g_assert (json_object_has_member (region_object, "id"));
      g_assert (json_object_has_member (region_object, "region-data"));
      g_assert (json_object_has_member (region_object, "items"));

      id = json_object_get_string_member (region_object, "id");

      page_region = page_region_new (id);
      page_region->region_data = json_node_ref (json_object_get_member (region_object, "region-data"));

      items = json_object_get_array_member (region_object, "items");
      for (unsigned int j = 0; j < json_array_get_length (items); j++)
        {
          JsonNode *item_node = json_array_get_element (items, j);
          BsPageItem *item = bs_page_item_new_from_json (item_node);

          g_ptr_array_insert (page_region->items, j, item_data_new (item));
        }

      g_assert (!g_hash_table_contains (self->page_regions, id));

      g_hash_table_insert (self->page_regions, g_strdup (id), g_steal_pointer (&page_region));
    }
}


/*
 * GObject overrides
 */

static void
bs_page_finalize (GObject *object)
{
  BsPage *self = (BsPage *)object;

  g_clear_pointer (&self->page_regions, g_hash_table_destroy);

  G_OBJECT_CLASS (bs_page_parent_class)->finalize (object);
}

static void
bs_page_get_property (GObject    *object,
                      guint       prop_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
  BsPage *self = BS_PAGE (object);

  switch (prop_id)
    {
    case PROP_ROOT:
      g_value_set_boolean (value, self->root);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_page_set_property (GObject      *object,
                      guint         prop_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
bs_page_class_init (BsPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = bs_page_finalize;
  object_class->get_property = bs_page_get_property;
  object_class->set_property = bs_page_set_property;

  properties[PROP_ROOT] = g_param_spec_boolean ("root", NULL, NULL,
                                                FALSE,
                                                G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
bs_page_init (BsPage *self)
{
  self->page_regions = g_hash_table_new_full (g_str_hash,
                                              g_str_equal,
                                              g_free,
                                              (GDestroyNotify) page_region_free);
}

BsPage *
bs_page_new (void)
{
  return g_object_new (BS_TYPE_PAGE, NULL);
}

BsPage *
bs_page_new_empty (void)
{
  g_autoptr (BsPage) page = NULL;

  page = g_object_new (BS_TYPE_PAGE, NULL);
  ensure_first_subpage_item_is_move_up (page);

  return g_steal_pointer (&page);
}

BsPage *
bs_page_new_from_json (JsonNode *node)
{
  g_autoptr (JsonNode) converted = NULL;
  g_autoptr (GError) error = NULL;
  g_autoptr (BsPage) page = NULL;

  page = g_object_new (BS_TYPE_PAGE, NULL);

  converted = convert_page (node, &error);
  if (error)
    {
      g_warning ("Error converting page: %s", error->message);
      goto out;
    }

  load_page_from_json (page, converted);

out:
  ensure_first_subpage_item_is_move_up (page);

  return g_steal_pointer (&page);
}

BsPage *
bs_page_new_root (JsonNode *node)
{
  g_autoptr (BsPage) page = NULL;

  page = g_object_new (BS_TYPE_PAGE, NULL);
  page->root = TRUE;

  if (node)
    {
      g_autoptr (JsonNode) converted = NULL;
      g_autoptr (GError) error = NULL;

      converted = convert_page (node, &error);
      if (error)
        {
          g_warning ("Error converting page: %s", error->message);
          return g_steal_pointer (&page);
        }

      load_page_from_json (page, converted);
    }

  return g_steal_pointer (&page);
}

JsonNode *
bs_page_to_json (BsPage *self)
{
  g_autoptr (JsonBuilder) builder = NULL;
  GHashTableIter iter;
  PageRegion *page_region;
  const char *region_id;

  g_return_val_if_fail (BS_IS_PAGE (self), NULL);

  builder = json_builder_new ();

  json_builder_begin_object (builder);

  json_builder_set_member_name (builder, "version");
  json_builder_add_int_value (builder, 1);

  json_builder_set_member_name (builder, "regions");
  json_builder_begin_array (builder);

  g_hash_table_iter_init (&iter, self->page_regions);
  while (g_hash_table_iter_next (&iter, (gpointer *) &region_id, (gpointer *) &page_region))
    {
      json_builder_begin_object (builder);

      json_builder_set_member_name (builder, "id");
      json_builder_add_string_value (builder, region_id);

      json_builder_set_member_name (builder, "region-data");
      json_builder_add_value (builder, json_node_ref (page_region->region_data));

      json_builder_set_member_name (builder, "items");
      json_builder_begin_array (builder);
      for (unsigned int i = 0; i < page_region->items->len; i++)
        {
          ItemData *item_data = g_ptr_array_index (page_region->items, i);
          json_builder_add_value (builder, bs_page_item_to_json (item_data->item));
        }
      json_builder_end_array (builder);

      json_builder_end_object (builder);
    }
  json_builder_end_array (builder);

  json_builder_end_object (builder);

  return json_builder_get_root (builder);
}

gboolean
bs_page_is_root (BsPage *self)
{
  g_return_val_if_fail (BS_IS_PAGE (self), FALSE);

  return self->root;
}

void
bs_page_update_item (BsPage     *self,
                     const char *region_id,
                     size_t      position,
                     BsAction   *action,
                     BsIcon     *custom_icon)

{
  BsPageItem *item;
  ItemData *item_data;

  g_return_if_fail (BS_IS_PAGE (self));
  g_return_if_fail (!custom_icon || BS_IS_ICON (custom_icon));
  g_return_if_fail (self->loaded);

  item_data = get_item_data (self, region_id, position);

  if (!item_data)
    {
      add_item (self, bs_page_item_new (), region_id, position);
      item_data = get_item_data (self, region_id, position);
    }

  g_assert (item_data != NULL);

  bs_page_item_update (item_data->item, action, custom_icon);

  g_set_object (&item_data->action, action);
  g_set_object (&item_data->custom_icon, custom_icon);
}

gboolean
bs_page_realize (BsPage      *self,
                 const char  *region_id,
                 size_t       position,
                 BsIcon     **out_custom_icon,
                 BsAction   **out_action,
                 GError     **error)
{
  BsPageItem *item;

  g_return_val_if_fail (BS_IS_PAGE (self), FALSE);
  g_return_val_if_fail (out_custom_icon != NULL, FALSE);
  g_return_val_if_fail (out_action != NULL, FALSE);

  item = get_item (self, region_id, position);

  if (!item)
    {
      *out_custom_icon = NULL;
      *out_action = bs_empty_action_new ();
      return FALSE;
    }

  if (self->loaded)
    {
      PageRegion *page_region = g_hash_table_lookup (self->page_regions, region_id);
      ItemData *item_data = NULL;

      if (!page_region || position >= page_region->items->len)
        {
          *out_custom_icon = NULL;
          *out_action = bs_empty_action_new ();
          return FALSE;
        }

      item_data = g_ptr_array_index (page_region->items, position);
      g_assert (item_data != NULL);
      g_assert (BS_IS_PAGE_ITEM (item_data->item));

      *out_action = item_data->action ? g_object_ref (item_data->action) : NULL;
      *out_custom_icon = item_data->custom_icon ? g_object_ref (item_data->custom_icon) : NULL;

      return TRUE;
    }

  return bs_page_item_realize (item,
                               out_custom_icon,
                               out_action,
                               error);
}

JsonNode *
bs_page_get_region_data (BsPage     *self,
                         const char *region_id)
{
  PageRegion *page_region;

  g_assert (BS_IS_PAGE (self));
  g_assert (region_id != NULL);

  page_region = ensure_page_region (self, region_id);
  g_assert (page_region != NULL);
  g_assert (JSON_NODE_HOLDS_OBJECT (page_region->region_data));

  return page_region->region_data;
}

void
bs_page_set_region_data (BsPage     *self,
                         const char *region_id,
                         JsonNode   *region_data)
{
  g_autoptr (JsonNode) old_data = NULL;
  PageRegion *page_region;

  g_assert (BS_IS_PAGE (self));
  g_assert (region_id != NULL);
  g_assert (JSON_NODE_HOLDS_OBJECT (region_data));

  page_region = ensure_page_region (self, region_id);
  g_assert (page_region != NULL);
  g_assert (JSON_NODE_HOLDS_OBJECT (page_region->region_data));

  old_data = json_node_ref (page_region->region_data);

  g_clear_pointer (&page_region->region_data, json_node_unref);
  page_region->region_data = json_node_ref (region_data);
}

void
bs_page_load_items (BsPage *self)
{
  GHashTableIter iter;
  PageRegion *page_region;
  const char *region_id;

  g_assert (BS_IS_PAGE (self));

  if (self->loaded)
    return;

  g_hash_table_iter_init (&iter, self->page_regions);
  while (g_hash_table_iter_next (&iter, (gpointer *) &region_id, (gpointer *) &page_region))
    {
      for (unsigned int i = 0; i < page_region->items->len; i++)
        {
          g_autoptr (GError) error = NULL;
          ItemData *item_data = NULL;

          item_data = g_ptr_array_index (page_region->items, i);

          g_assert (item_data != NULL);
          g_assert (BS_IS_PAGE_ITEM (item_data->item));
          g_assert (item_data->action == NULL);
          g_assert (item_data->custom_icon == NULL);

          if (!bs_page_item_realize (item_data->item,
                                     &item_data->custom_icon,
                                     &item_data->action,
                                     &error))
            {
              g_warning ("Error creating action %u for region '%s': %s",
                         i,
                         region_id,
                         error->message);
            }

        }
    }

  self->loaded = TRUE;
}


void
bs_page_unload_items (BsPage *self)
{
  GHashTableIter iter;
  PageRegion *page_region;
  const char *region_id;

  g_assert (BS_IS_PAGE (self));

  if (!self->loaded)
    return;

  g_hash_table_iter_init (&iter, self->page_regions);
  while (g_hash_table_iter_next (&iter, (gpointer *) &region_id, (gpointer *) &page_region))
    {
      for (unsigned int i = 0; i < page_region->items->len; i++)
        {
          g_autoptr (GError) error = NULL;
          ItemData *item_data = NULL;

          item_data = g_ptr_array_index (page_region->items, i);

          g_assert (item_data != NULL);
          g_assert (BS_IS_PAGE_ITEM (item_data->item));

          bs_page_item_update (item_data->item, item_data->action, item_data->custom_icon);

          g_clear_object (&item_data->action);
          g_clear_object (&item_data->custom_icon);
        }
    }

  self->loaded = FALSE;
}

void
bs_page_update_items (BsPage *self)
{
  GHashTableIter iter;
  PageRegion *page_region;
  const char *region_id;

  g_assert (BS_IS_PAGE (self));
  g_assert (self->loaded);

  g_hash_table_iter_init (&iter, self->page_regions);
  while (g_hash_table_iter_next (&iter, (gpointer *) &region_id, (gpointer *) &page_region))
    {
      for (unsigned int i = 0; i < page_region->items->len; i++)
        {
          g_autoptr (GError) error = NULL;
          ItemData *item_data = NULL;

          item_data = g_ptr_array_index (page_region->items, i);

          g_assert (item_data != NULL);
          g_assert (BS_IS_PAGE_ITEM (item_data->item));

          bs_page_item_update (item_data->item, item_data->action, item_data->custom_icon);
        }
    }
}
