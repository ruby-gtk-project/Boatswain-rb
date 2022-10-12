/* obs-types.h
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

#include <glib-object.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

typedef enum
{
  OBS_CONNECTION_STATE_DISCONNECTED,
  OBS_CONNECTION_STATE_CONNECTING,
  OBS_CONNECTION_STATE_AUTHENTICATING,
  OBS_CONNECTION_STATE_WAITING_FOR_CREDENTIALS,
  OBS_CONNECTION_STATE_CONNECTED,
} ObsConnectionState;

typedef enum
{
  OBS_RECORDING_STATE_STOPPED,
  OBS_RECORDING_STATE_PAUSED,
  OBS_RECORDING_STATE_RECORDING,
} ObsRecordingState;

typedef enum
{
  OBS_STREAMING_STATE_STOPPED,
  OBS_STREAMING_STATE_STREAMING,
} ObsStreamingState;

typedef enum
{
  OBS_VIRTUALCAM_STATE_STOPPED,
  OBS_VIRTUALCAM_STATE_STREAMING,
} ObsVirtualcamState;

typedef enum
{
  OBS_WEBSOCKET_OUTPUT_UNKNOWN = -1,
  OBS_WEBSOCKET_OUTPUT_STARTING,
  OBS_WEBSOCKET_OUTPUT_STARTED,
  OBS_WEBSOCKET_OUTPUT_STOPPING,
  OBS_WEBSOCKET_OUTPUT_STOPPED,
  OBS_WEBSOCKET_OUTPUT_RECONNECTING,
  OBS_WEBSOCKET_OUTPUT_RECONNECTED,
  OBS_WEBSOCKET_OUTPUT_PAUSED,
  OBS_WEBSOCKET_OUTPUT_RESUMED,
} ObsWebsocketOutputState;

typedef enum
{
  OBS_OUTPUT_FLAG_NONE = 0,
  OBS_OUTPUT_VIDEO = 1 << 0,
  OBS_OUTPUT_AUDIO = 1 << 1,
  OBS_OUTPUT_ASYNC = 1 << 2,
  OBS_OUTPUT_CUSTOM_DRAW = 1 << 3,
  OBS_OUTPUT_INTERACTION = 1 << 5,
  OBS_OUTPUT_COMPOSITE = 1 << 6,
  OBS_OUTPUT_DO_NOT_DUPLICATE = 1 << 7,
  OBS_OUTPUT_DEPRECATED = 1 << 8,
  OBS_OUTPUT_DO_NOT_SELF_MONITOR = 1 << 9,
  OBS_OUTPUT_DISABLED = 1 << 10,
  OBS_OUTPUT_MONITOR_BY_DEFAULT = 1 << 11,
  OBS_OUTPUT_SUBMIX = 1 << 12,
  OBS_OUTPUT_CONTROLLABLE_MEDIA = 1 << 13,
  OBS_OUTPUT_CEA_708 = 1 << 14,
  OBS_OUTPUT_SRGB = 1 << 15,
  OBS_OUTPUT_CAP_DONT_SHOW_PROPERTIES = 1 << 16,
  OBS_OUTPUT_REQUIRES_CANVAS = 1 << 17,
} ObsOutputFlags;

typedef enum
{
  OBS_SOURCE_CAP_INVALID = -1,
  OBS_SOURCE_CAP_NONE = 0,
  OBS_SOURCE_CAP_AUDIO = 1 << 0,
  OBS_SOURCE_CAP_VIDEO = 1 << 1,
} ObsSourceCaps;

typedef enum
{
  OBS_SOURCE_TYPE_UNKNOWN = -1,
  OBS_SOURCE_TYPE_AUDIO,
  OBS_SOURCE_TYPE_MICROPHONE,
  OBS_SOURCE_TYPE_VIDEO,
  OBS_SOURCE_TYPE_FILTER,
  OBS_SOURCE_TYPE_TRANSITION,
} ObsSourceType;

typedef struct _ObsAction ObsAction;
typedef struct _ObsConnection ObsConnection;
typedef struct _ObsConnectionManager ObsConnectionManager;
typedef struct _ObsRecordAction ObsRecordAction;
typedef struct _ObsScene ObsScene;
typedef struct _ObsSource ObsSource;

G_END_DECLS
