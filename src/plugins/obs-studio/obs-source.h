/* obs-source.h
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

#pragma once

#include "obs-types.h"

G_BEGIN_DECLS

#define OBS_TYPE_SOURCE (obs_source_get_type())
G_DECLARE_FINAL_TYPE (ObsSource, obs_source, OBS, SOURCE, GObject)

ObsSource * obs_source_new (const char    *uuid,
                            const char    *name,
                            gboolean       muted,
                            gboolean       visible,
                            ObsSourceType  source_type,
                            ObsSourceCaps  source_caps);

const char * obs_source_get_uuid (ObsSource *self);

const char * obs_source_get_name (ObsSource *self);
void obs_source_set_name (ObsSource  *self,
                          const char *name);

gboolean obs_source_get_muted (ObsSource *self);
void obs_source_set_muted (ObsSource *self,
                           gboolean   muted);

gboolean obs_source_get_visible (ObsSource *self);
void obs_source_set_visible (ObsSource *self,
                             gboolean   visible);

ObsSourceCaps obs_source_get_caps (ObsSource *self);
ObsSourceType obs_source_get_source_type (ObsSource *self);

G_END_DECLS
