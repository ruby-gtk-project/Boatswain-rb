/* obs-source.c
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

#include "obs-source.h"

struct _ObsSource
{
  GObject parent_instance;

  char *uuid;
  char *name;
  gboolean muted;
  gboolean visible;
  ObsSourceCaps source_caps;
  ObsSourceType source_type;
};

G_DEFINE_FINAL_TYPE (ObsSource, obs_source, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_MUTED,
  PROP_NAME,
  PROP_SOURCE_CAPS,
  PROP_SOURCE_TYPE,
  PROP_UUID,
  PROP_VISIBLE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
obs_source_finalize (GObject *object)
{
  ObsSource *self = (ObsSource *)object;

  g_clear_pointer (&self->uuid, g_free);
  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (obs_source_parent_class)->finalize (object);
}

static void
obs_source_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  ObsSource *self = OBS_SOURCE (object);

  switch (prop_id)
    {
    case PROP_MUTED:
      g_value_set_boolean (value, self->muted);
      break;

    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case PROP_SOURCE_CAPS:
      g_value_set_int (value, self->source_caps);
      break;

    case PROP_SOURCE_TYPE:
      g_value_set_int (value, self->source_type);
      break;

    case PROP_UUID:
      g_value_set_string (value, self->uuid);
      break;

    case PROP_VISIBLE:
      g_value_set_boolean (value, self->visible);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
obs_source_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  ObsSource *self = OBS_SOURCE (object);

  switch (prop_id)
    {
    case PROP_MUTED:
      obs_source_set_muted (self, g_value_get_boolean (value));
      break;

    case PROP_NAME:
      self->name = g_value_dup_string (value);
      break;

    case PROP_SOURCE_CAPS:
      g_assert (self->source_caps == OBS_SOURCE_CAP_NONE);
      self->source_caps = g_value_get_int (value);
      break;

    case PROP_SOURCE_TYPE:
      self->source_type = g_value_get_int (value);
      break;

    case PROP_UUID:
      g_assert (self->uuid == NULL);
      self->uuid = g_value_dup_string (value);
      break;

    case PROP_VISIBLE:
      obs_source_set_visible (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
obs_source_class_init (ObsSourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = obs_source_finalize;
  object_class->get_property = obs_source_get_property;
  object_class->set_property = obs_source_set_property;

  properties[PROP_MUTED] = g_param_spec_boolean ("muted", NULL, NULL,
                                                 FALSE,
                                                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  properties[PROP_NAME] = g_param_spec_string ("name", NULL, NULL,
                                               NULL,
                                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  properties[PROP_VISIBLE] = g_param_spec_boolean ("visible", NULL, NULL,
                                                   TRUE,
                                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  properties[PROP_SOURCE_CAPS] = g_param_spec_int ("source-caps", NULL, NULL,
                                                   OBS_SOURCE_CAP_NONE, G_MAXINT, OBS_SOURCE_CAP_NONE,
                                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_SOURCE_TYPE] = g_param_spec_int ("source-type", NULL, NULL,
                                                   OBS_SOURCE_TYPE_UNKNOWN, G_MAXINT, OBS_SOURCE_TYPE_UNKNOWN,
                                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_UUID] = g_param_spec_string ("uuid", NULL, NULL,
                                               NULL,
                                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
obs_source_init (ObsSource *self)
{
  self->source_caps = OBS_SOURCE_CAP_NONE;
  self->source_type = OBS_SOURCE_TYPE_UNKNOWN;
  self->visible = TRUE;
}


ObsSource *
obs_source_new (const char    *uuid,
                const char    *name,
                gboolean       muted,
                gboolean       visible,
                ObsSourceType  source_type,
                ObsSourceCaps  source_caps)
{
  g_autoptr (ObsSource) source = NULL;

  source = g_object_new (OBS_TYPE_SOURCE,
                         "uuid", uuid,
                         "name", name,
                         "muted", muted,
                         "visible", visible,
                         "source-type", source_type,
                         "source-caps", source_caps,
                         NULL);

  return g_steal_pointer (&source);
}

const char *
obs_source_get_uuid (ObsSource *self)
{
  g_return_val_if_fail (OBS_IS_SOURCE (self), NULL);

  return self->uuid;
}

const char *
obs_source_get_name (ObsSource *self)
{
  g_return_val_if_fail (OBS_IS_SOURCE (self), NULL);

  return self->name;
}

void
obs_source_set_name (ObsSource  *self,
                     const char *name)
{
  g_return_if_fail (OBS_IS_SOURCE (self));
  g_return_if_fail (name != NULL);

  if (g_strcmp0 (self->name, name) == 0)
    return;

  g_clear_pointer (&self->name, g_free);
  self->name = g_strdup (name);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NAME]);
}

gboolean
obs_source_get_muted (ObsSource *self)
{
  g_return_val_if_fail (OBS_IS_SOURCE (self), FALSE);

  return self->muted;
}

void
obs_source_set_muted (ObsSource *self,
                      gboolean   muted)
{
  g_return_if_fail (OBS_IS_SOURCE (self));

  if (self->muted == muted)
    return;

  self->muted = muted;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MUTED]);
}

gboolean
obs_source_get_visible (ObsSource *self)
{
  g_return_val_if_fail (OBS_IS_SOURCE (self), FALSE);

  return self->visible;
}

void
obs_source_set_visible (ObsSource *self,
                        gboolean   visible)
{
  g_return_if_fail (OBS_IS_SOURCE (self));

  if (self->visible == visible)
    return;

  self->visible = visible;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VISIBLE]);
}

ObsSourceCaps
obs_source_get_caps (ObsSource *self)
{
  g_return_val_if_fail (OBS_IS_SOURCE (self), OBS_SOURCE_CAP_INVALID);

  return self->source_caps;
}

ObsSourceType
obs_source_get_source_type (ObsSource *self)
{
  g_return_val_if_fail (OBS_IS_SOURCE (self), OBS_SOURCE_TYPE_UNKNOWN);

  return self->source_type;
}
