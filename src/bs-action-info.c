/*
 * bs-action-info.c
 *
 * Copyright 2023 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "bs-action-info.h"

struct _BsActionInfo
{
  GObject parent_instance;

  char *id;
  char *name;
  char *description;
  char *icon_name;
  gboolean hidden;
};

G_DEFINE_FINAL_TYPE (BsActionInfo, bs_action_info, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_ID,
  PROP_NAME,
  PROP_DESCRIPTION,
  PROP_ICON_NAME,
  PROP_HIDDEN,
  N_PROPS,
};

static GParamSpec *properties [N_PROPS];

static void
bs_action_info_finalize (GObject *object)
{
  BsActionInfo *self = (BsActionInfo *)object;

  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->description, g_free);
  g_clear_pointer (&self->icon_name, g_free);

  G_OBJECT_CLASS (bs_action_info_parent_class)->finalize (object);
}

static void
bs_action_info_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  BsActionInfo *self = BS_ACTION_INFO (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, self->id);
      break;

    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case PROP_DESCRIPTION:
      g_value_set_string (value, self->description);
      break;

    case PROP_ICON_NAME:
      g_value_set_string (value, self->icon_name);
      break;

    case PROP_HIDDEN:
      g_value_set_boolean (value, self->hidden);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_action_info_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  BsActionInfo *self = BS_ACTION_INFO (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_assert (self->id == NULL);
      self->id = g_value_dup_string (value);
      g_assert (self->id != NULL);
      break;

    case PROP_NAME:
      g_assert (self->name == NULL);
      self->name = g_value_dup_string (value);
      g_assert (self->name != NULL);
      break;

    case PROP_DESCRIPTION:
      g_assert (self->description == NULL);
      self->description = g_value_dup_string (value);
      break;

    case PROP_ICON_NAME:
      g_assert (self->icon_name == NULL);
      self->icon_name = g_value_dup_string (value);
      g_assert (self->icon_name != NULL);
      break;

    case PROP_HIDDEN:
      self->hidden = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_action_info_class_init (BsActionInfoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = bs_action_info_finalize;
  object_class->get_property = bs_action_info_get_property;
  object_class->set_property = bs_action_info_set_property;

  properties[PROP_ID] =
    g_param_spec_string ("id",
                         NULL, NULL, NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_NAME] =
    g_param_spec_string ("name",
                         NULL, NULL, NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  properties[PROP_DESCRIPTION] =
    g_param_spec_string ("description",
                         NULL, NULL, NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  properties[PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         NULL, NULL, NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  properties[PROP_HIDDEN] =
    g_param_spec_boolean ("hidden",
                          NULL, NULL, FALSE,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);


  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
bs_action_info_init (BsActionInfo *self)
{

}

BsActionInfo *
bs_action_info_new (const char *id,
                    const char *name,
                    const char *description,
                    const char *icon_name,
                    gboolean    hidden)
{
  return g_object_new (BS_TYPE_ACTION_INFO,
                       "id", id,
                       "name", name,
                       "description", description,
                       "icon-name", icon_name,
                       "hidden", hidden,
                       NULL);
}

const char *
bs_action_info_get_id (BsActionInfo *self)
{
  g_return_val_if_fail (BS_IS_ACTION_INFO (self), NULL);

  return self->id;
}

const char *
bs_action_info_get_name (BsActionInfo *self)
{
  g_return_val_if_fail (BS_IS_ACTION_INFO (self), NULL);

  return self->name;
}

const char *
bs_action_info_get_description (BsActionInfo *self)
{
  g_return_val_if_fail (BS_IS_ACTION_INFO (self), NULL);

  return self->description;
}

const char *
bs_action_info_get_icon_name (BsActionInfo *self)
{
  g_return_val_if_fail (BS_IS_ACTION_INFO (self), NULL);

  return self->icon_name;
}

gboolean
bs_action_info_get_hidden (BsActionInfo *self)
{
  g_return_val_if_fail (BS_IS_ACTION_INFO (self), FALSE);

  return self->hidden;
}
