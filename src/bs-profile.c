/* bs-profile.c
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
 * SPDX-License-Identifier: GPL-3.0-or-laterfinalize
 */

#include "bs-profile.h"

#include "bs-page-private.h"
#include "bs-stream-deck.h"

#include <glib/gi18n.h>

struct _BsProfile
{
  GObject parent_instance;

  char *id;
  char *name;
  double brightness;
  BsPage *root_page;
  BsStreamDeck *stream_deck;
};

G_DEFINE_FINAL_TYPE (BsProfile, bs_profile, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_ID,
  PROP_NAME,
  PROP_BRIGHTNESS,
  PROP_PAGE,
  PROP_STREAM_DECK,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];


/*
 * GObject overrides
 */

static void
bs_profile_finalize (GObject *object)
{
  BsProfile *self = (BsProfile *)object;

  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->name, g_free);
  g_clear_object (&self->root_page);

  G_OBJECT_CLASS (bs_profile_parent_class)->finalize (object);
}

static void
bs_profile_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  BsProfile *self = BS_PROFILE (object);

  switch (prop_id)
    {
    case PROP_BRIGHTNESS:
      g_value_set_double (value, self->brightness);
      break;

    case PROP_ID:
      g_value_set_string (value, self->id);
      break;

    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case PROP_PAGE:
      g_value_set_object (value, self->root_page);
      break;

    case PROP_STREAM_DECK:
      g_value_set_object (value, self->stream_deck);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_profile_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  BsProfile *self = BS_PROFILE (object);

  switch (prop_id)
    {
    case PROP_BRIGHTNESS:
      bs_profile_set_brightness (self, g_value_get_double (value));
      break;

    case PROP_ID:
      g_assert (self->id == NULL);
      self->id = g_value_dup_string (value);
      break;

    case PROP_NAME:
      bs_profile_set_name (self, g_value_get_string (value));
      break;

    case PROP_STREAM_DECK:
      g_assert (self->stream_deck == NULL);
      self->stream_deck = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_profile_class_init (BsProfileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = bs_profile_finalize;
  object_class->get_property = bs_profile_get_property;
  object_class->set_property = bs_profile_set_property;

  properties[PROP_BRIGHTNESS] = g_param_spec_double ("brightness", NULL, NULL,
                                                     0.0, 1.0, 0.5,
                                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_ID] = g_param_spec_string ("id", NULL, NULL,
                                             NULL,
                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_NAME] = g_param_spec_string ("name", NULL, NULL,
                                               NULL,
                                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_PAGE] = g_param_spec_object ("page", NULL, NULL,
                                               BS_TYPE_PAGE,
                                               G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_STREAM_DECK] = g_param_spec_object ("stream-deck", NULL, NULL,
                                                      BS_TYPE_STREAM_DECK,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
bs_profile_init (BsProfile *self)
{
  self->brightness = 0.5;
}

BsProfile *
bs_profile_new_empty (BsStreamDeck *stream_deck)
{
  g_autoptr (BsProfile) profile = NULL;
  g_autofree char *id = g_uuid_string_random ();

  profile = g_object_new (BS_TYPE_PROFILE,
                          "id", id,
                          "name", _("Unnamed profile"),
                          "stream-deck", stream_deck,
                          NULL);

  profile->root_page = bs_page_new_root (NULL);

  return g_steal_pointer (&profile);
}

BsProfile *
bs_profile_new_from_json (BsStreamDeck *stream_deck,
                          JsonNode     *node)
{
  g_autoptr (BsProfile) profile = NULL;
  JsonObject *object;

  if (!JSON_NODE_HOLDS_OBJECT (node))
    {
      g_warning ("JSON node is not an object");
      return bs_profile_new_empty (stream_deck);
    }

  object = json_node_get_object (node);

  profile = g_object_new (BS_TYPE_PROFILE,
                          "id", json_object_get_string_member (object, "id"),
                          "name", json_object_get_string_member (object, "name"),
                          "brightness", json_object_get_double_member (object, "brightness"),
                          "stream-deck", stream_deck,
                          NULL);

  profile->root_page = bs_page_new_root (json_object_get_member (object, "page"));

  return g_steal_pointer (&profile);
}

JsonNode *
bs_profile_to_json (BsProfile *self)
{
  g_autoptr (JsonBuilder) builder = NULL;

  g_return_val_if_fail (BS_IS_PROFILE (self), NULL);

  builder = json_builder_new ();

  json_builder_begin_object (builder);

  json_builder_set_member_name (builder, "id");
  json_builder_add_string_value (builder, self->id);

  json_builder_set_member_name (builder, "name");
  json_builder_add_string_value (builder, self->name);

  json_builder_set_member_name (builder, "brightness");
  json_builder_add_double_value (builder, self->brightness);

  json_builder_set_member_name (builder, "page");
  json_builder_add_value (builder, bs_page_to_json (self->root_page));

  json_builder_end_object (builder);

  return json_builder_get_root (builder);
}

double
bs_profile_get_brightness (BsProfile *self)
{
  g_return_val_if_fail (BS_IS_PROFILE (self), 0.0);

  return self->brightness;
}

void
bs_profile_set_brightness (BsProfile *self,
                           double     brightness)
{
  g_return_if_fail (BS_IS_PROFILE (self));
  g_return_if_fail (brightness >= 0.0 && brightness <= 1.0);

  if (G_APPROX_VALUE (self->brightness, brightness, DBL_EPSILON))
    return;

  self->brightness = brightness;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BRIGHTNESS]);
}

const char *
bs_profile_get_name (BsProfile *self)
{
  g_return_val_if_fail (BS_IS_PROFILE (self), NULL);

  return self->name;
}

const char *
bs_profile_get_id (BsProfile *self)
{
  g_return_val_if_fail (BS_IS_PROFILE (self), NULL);

  return self->id;
}

void
bs_profile_set_name (BsProfile  *self,
                     const char *name)
{
  g_return_if_fail (BS_IS_PROFILE (self));
  g_return_if_fail (name != NULL && *name != '\0');

  if (g_strcmp0 (self->name, name) == 0)
    return;

  g_clear_pointer (&self->name, g_free);
  self->name = g_strdup (name);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NAME]);
}

BsStreamDeck *
bs_profile_get_stream_deck (BsProfile *self)
{
  g_return_val_if_fail (BS_IS_PROFILE (self), NULL);

  return self->stream_deck;
}

BsPage *
bs_profile_get_root_page (BsProfile *self)
{
  g_return_val_if_fail (BS_IS_PROFILE (self), NULL);

  return self->root_page;
}
