/* obs-scene.c
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

#include "obs-connection.h"
#include "obs-scene.h"
#include "obs-source.h"

struct _ObsScene
{
  GObject parent_instance;

  char *name;
  char *uuid;
};

G_DEFINE_FINAL_TYPE (ObsScene, obs_scene, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_NAME,
  PROP_UUID,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];


/*
 * GObject overrides
 */

static void
obs_scene_finalize (GObject *object)
{
  ObsScene *self = (ObsScene *)object;

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->uuid, g_free);

  G_OBJECT_CLASS (obs_scene_parent_class)->finalize (object);
}

static void
obs_scene_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  ObsScene *self = OBS_SCENE (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case PROP_UUID:
      g_value_set_string (value, self->uuid);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
obs_scene_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  ObsScene *self = OBS_SCENE (object);

  switch (prop_id)
    {
    case PROP_NAME:
      obs_scene_set_name (self, g_value_get_string (value));
      break;

    case PROP_UUID:
      self->uuid = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
obs_scene_class_init (ObsSceneClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = obs_scene_finalize;
  object_class->get_property = obs_scene_get_property;
  object_class->set_property = obs_scene_set_property;

  properties[PROP_NAME] = g_param_spec_string ("name", NULL, NULL,
                                               NULL,
                                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  properties[PROP_UUID] = g_param_spec_string ("uuid", NULL, NULL,
                                               NULL,
                                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
obs_scene_init (ObsScene *self)
{
}

ObsScene *
obs_scene_new_from_json (ObsConnection *connection,
                         JsonObject    *scene_object)
{
  g_autoptr (ObsScene) scene = NULL;

  scene = g_object_new (OBS_TYPE_SCENE,
                        "name", json_object_get_string_member (scene_object, "sceneName"),
                        "uuid", json_object_get_string_member (scene_object, "sceneUuid"),
                        NULL);

  return g_steal_pointer (&scene);
}

const char *
obs_scene_get_uuid (ObsScene *self)
{
  g_return_val_if_fail (OBS_IS_SCENE (self), NULL);

  return self->uuid;
}

const char *
obs_scene_get_name (ObsScene *self)
{
  g_return_val_if_fail (OBS_IS_SCENE (self), NULL);

  return self->name;
}

void
obs_scene_set_name (ObsScene   *self,
                    const char *name)
{
  g_return_if_fail (OBS_IS_SCENE (self));
  g_return_if_fail (name != NULL);

  if (g_strcmp0 (self->name, name) == 0)
    return;

  g_clear_pointer (&self->name, g_free);
  self->name = g_strdup (name);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NAME]);
}
