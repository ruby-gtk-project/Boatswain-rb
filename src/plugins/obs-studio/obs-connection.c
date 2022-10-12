/* obs-connection.c
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

#define G_LOG_DOMAIN "OBS Studio"

#define TRACE_WEBSOCKET_MESSAGES 0

#include "obs-connection.h"
#include "obs-enum-types.h"
#include "obs-scene.h"
#include "obs-source.h"
#include "obs-utils.h"

#include <libsecret/secret.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>
#include <stdint.h>
#include <libdex.h>

typedef struct
{
  ObsSourceCaps caps;
  ObsSourceType type;
} SourceInfo;

struct _ObsConnection
{
  GObject parent_instance;

  DexChannel *channel;

  char *host;
  unsigned int port;
  ObsConnectionState state;

  GListStore *scenes;
  GListStore *sources;
  char *current_scene_uuid;

  GHashTable *source_to_scene_items; /* char* (uuid) → GtkBitset */

  gboolean streaming;
  gboolean virtualcam_enabled;
  ObsRecordingState recording_state;
};

G_DEFINE_FINAL_TYPE (ObsConnection, obs_connection, G_TYPE_OBJECT)

typedef enum
{
  OP_HELLO,
  OP_IDENTIFY,
  OP_IDENTIFIED,
  OP_REIDENTIFY,
  /* OpCode 4 doesn't exist! */
  OP_EVENT = 5,
  OP_REQUEST,
  OP_REQUEST_RESPONSE,
  OP_REQUEST_BATCH,
  OP_REQUEST_BATCH_RESPONSE,
} WebSocketOpCode;

enum
{
  PROP_0,
  PROP_HOST,
  PROP_PORT,
  PROP_RECORDING_STATE,
  PROP_STREAMING,
  PROP_VIRTUALCAM_ENABLED,
  N_PROPS,
};

enum
{
  AUTHENTICATION_FAILED,
  STATE_CHANGED,
  N_SIGNALS,
};

static guint signals[N_SIGNALS];
static GParamSpec *properties[N_PROPS];

/*
 * Auxiliary methods
 */

static void
set_recording_state (ObsConnection     *self,
                     ObsRecordingState  recording_state)
{
  if (self->recording_state == recording_state)
    return;

  self->recording_state = recording_state;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RECORDING_STATE]);
}

static void
set_streaming (ObsConnection *self,
               gboolean       streaming)
{
  if (self->streaming == streaming)
    return;

  self->streaming = streaming;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STREAMING]);
}

static void
set_virtualcam_enabled (ObsConnection *self,
                        gboolean       virtualcam_enabled)
{
  if (self->virtualcam_enabled == virtualcam_enabled)
    return;

  self->virtualcam_enabled = virtualcam_enabled;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VIRTUALCAM_ENABLED]);
}

static char *
generate_auth_string (const char *password,
                      const char *challenge,
                      const char *salt)
{
  g_autoptr (GChecksum) secret_checksum = NULL;
  g_autoptr (GChecksum) auth_checksum = NULL;
  g_autoptr (GString) secret_string = NULL;
  g_autoptr (GString) auth_string = NULL;
  g_autofree uint8_t *secret_hash = NULL;
  g_autofree uint8_t *auth_hash = NULL;
  g_autofree char *secret = NULL;
  g_autofree char *auth = NULL;
  size_t secret_length;
  size_t auth_length;

  secret_string = g_string_new (password);
  g_string_append (secret_string, salt);

  secret_checksum = g_checksum_new (G_CHECKSUM_SHA256);
  g_checksum_update (secret_checksum, (guchar *)secret_string->str, secret_string->len);

  secret_length = 200;
  secret_hash = g_malloc0 (sizeof (uint8_t) * secret_length);
  g_checksum_get_digest (secret_checksum, secret_hash, &secret_length);
  secret = g_base64_encode (secret_hash, secret_length);

  auth_string = g_string_new (secret);
  g_string_append (auth_string, challenge);

  auth_checksum = g_checksum_new (G_CHECKSUM_SHA256);
  g_checksum_update (auth_checksum, (guchar *)auth_string->str, auth_string->len);

  auth_length = 200;
  auth_hash = g_malloc0 (sizeof (uint8_t) * auth_length);
  g_checksum_get_digest (auth_checksum, auth_hash, &auth_length);
  auth = g_base64_encode (auth_hash, auth_length);

  return g_steal_pointer (&auth);
}

static ObsWebsocketOutputState
output_state_from_string (const char *string)
{
  struct {
    const char *id;
    ObsWebsocketOutputState state;
  } state_mapping[] = {
    { "OBS_WEBSOCKET_OUTPUT_UNKNOWN",      OBS_WEBSOCKET_OUTPUT_UNKNOWN },
    { "OBS_WEBSOCKET_OUTPUT_STARTING",     OBS_WEBSOCKET_OUTPUT_STARTING },
    { "OBS_WEBSOCKET_OUTPUT_STARTED",      OBS_WEBSOCKET_OUTPUT_STARTED },
    { "OBS_WEBSOCKET_OUTPUT_STOPPING",     OBS_WEBSOCKET_OUTPUT_STOPPING },
    { "OBS_WEBSOCKET_OUTPUT_STOPPED",      OBS_WEBSOCKET_OUTPUT_STOPPED },
    { "OBS_WEBSOCKET_OUTPUT_RECONNECTING", OBS_WEBSOCKET_OUTPUT_RECONNECTING },
    { "OBS_WEBSOCKET_OUTPUT_RECONNECTED",  OBS_WEBSOCKET_OUTPUT_RECONNECTED },
    { "OBS_WEBSOCKET_OUTPUT_PAUSED",       OBS_WEBSOCKET_OUTPUT_PAUSED },
    { "OBS_WEBSOCKET_OUTPUT_RESUMED",      OBS_WEBSOCKET_OUTPUT_RESUMED },
  };

  for (size_t i = 0; i < G_N_ELEMENTS (state_mapping); i++)
    {
      if (g_strcmp0 (string, state_mapping[i].id) == 0)
        return state_mapping[i].state;
    }

  return OBS_WEBSOCKET_OUTPUT_UNKNOWN;
 }


/*
 * Main Fiber
 */

typedef struct
{
  GWeakRef self_wr;

  char *host;
  unsigned int port;

  SoupSession *session;

  DexChannel *requests_channel;
  DexChannel *messages_channel;

  SoupWebsocketConnection *websocket;
  DexCancellable *websocket_cancellable;
  DexTaskGroup *group;

  GHashTable *uuid_to_promise;

  DexStateMachine *state_machine;
  DexFuture *connection_fiber;

  struct {
    char *challenge;
    char *salt;
    gboolean required;
  } authentication;
} WorkerFiberState;

static void
worker_fiber_state_free (gpointer data)
{
  WorkerFiberState *state = data;

  if (state->websocket)
    soup_websocket_connection_close (state->websocket, SOUP_WEBSOCKET_CLOSE_GOING_AWAY, NULL);

  g_weak_ref_clear (&state->self_wr);
  g_clear_pointer (&state->host, g_free);
  g_clear_object (&state->session);
  dex_clear (&state->requests_channel);
  dex_clear (&state->messages_channel);
  dex_clear (&state->state_machine);
  g_clear_object (&state->websocket);
  g_clear_pointer (&state->authentication.challenge, g_free);
  g_clear_pointer (&state->authentication.salt, g_free);
  g_free (state);
}


/*
 * Secrets
 */

static SecretSchema secrets_schema = {
  "com.feaneron.Boatswain.plugin.obs-studio",
  SECRET_SCHEMA_NONE,
  {
    { "host", SECRET_SCHEMA_ATTRIBUTE_STRING },
    { "port", SECRET_SCHEMA_ATTRIBUTE_INTEGER },
    { "NULL", 0 },
  },
};

static void
password_lookup_finished_cb (GObject      *source_object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  g_autoptr (DexPromise) promise = DEX_PROMISE (user_data);
  g_autoptr (GError) error = NULL;
  g_autofree char *password = NULL;

  password = secret_password_lookup_finish (result, &error);

  if (error)
    g_warning ("Error fetching password: %s", error->message);

  dex_promise_resolve_string (promise, g_steal_pointer (&password));
}

static DexFuture *
load_password (WorkerFiberState *state)
{
  g_autoptr (DexPromise) promise = NULL;
  g_autoptr (GError) error = NULL;

  promise = dex_promise_new ();

  secret_password_lookup (&secrets_schema,
                          NULL,
                          password_lookup_finished_cb,
                          dex_ref (promise),
                          "host", state->host,
                          "port", state->port,
                          NULL);

  return DEX_FUTURE (g_steal_pointer (&promise));
}

static void
store_password (WorkerFiberState *state,
                const char       *password)
{
  secret_password_store_sync (&secrets_schema,
                              SECRET_COLLECTION_SESSION,
                              "obs-websocket connection password",
                              password,
                              NULL, NULL,
                              "host", state->host,
                              "port", state->port,
                              NULL);
}


/*
 * Requests
 */

typedef struct
{
  enum {
    WEBSOCKET_REQUEST_AUTHENTICATE,
    WEBSOCKET_REQUEST_GENERIC,
  } type;

  union {
    struct {
      char *password;
    } authenticate;

    struct {
      char *method;
      JsonNode *data;
    } generic;
  } d;
} WebSocketRequest;

static void
websocket_request_free (gpointer data)
{
  WebSocketRequest *request = data;

  g_assert (request != NULL);

  switch (request->type)
    {
    case WEBSOCKET_REQUEST_AUTHENTICATE:
      g_clear_pointer (&request->d.authenticate.password, g_free);
      break;

    case WEBSOCKET_REQUEST_GENERIC:
      g_clear_pointer (&request->d.generic.data, json_node_unref);
      break;
    }

  g_free (request);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebSocketRequest, websocket_request_free)

static WebSocketRequest *
websocket_request_new_authenticate (const char *password)
{
  g_autoptr (WebSocketRequest) request = NULL;

  request = g_new0 (WebSocketRequest, 1);
  request->type = WEBSOCKET_REQUEST_AUTHENTICATE;
  request->d.authenticate.password = g_strdup (password);

  return g_steal_pointer (&request);
}

static WebSocketRequest *
websocket_request_new_generic (const char *method,
                               JsonNode   *data)
{
  g_autoptr (WebSocketRequest) request = NULL;

  g_assert (method != NULL);

  request = g_new0 (WebSocketRequest, 1);
  request->type = WEBSOCKET_REQUEST_GENERIC;
  request->d.generic.method = g_strdup (method);
  request->d.generic.data = data ? g_steal_pointer (&data) : NULL;

  return g_steal_pointer (&request);
}

static DexFuture *
send_request (WorkerFiberState *state,
              const char       *request_type,
              JsonNode         *request_data)
{
  g_autoptr (JsonGenerator) generator = NULL;
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (DexPromise) promise = NULL;
  g_autoptr (GTask) task = NULL;
  g_autofree char *message = NULL;
  g_autofree char *uuid = NULL;

  uuid = g_uuid_string_random ();

  builder = json_builder_new ();
  json_builder_begin_object (builder);
    {
      json_builder_set_member_name (builder, "op");
      json_builder_add_int_value (builder, OP_REQUEST);

      json_builder_set_member_name (builder, "d");
      json_builder_begin_object (builder);
        {
          json_builder_set_member_name (builder, "requestId");
          json_builder_add_string_value (builder, uuid);

          json_builder_set_member_name (builder, "requestType");
          json_builder_add_string_value (builder, request_type);

          if (request_data)
            {
              g_assert (JSON_NODE_HOLDS_OBJECT (request_data));

              json_builder_set_member_name (builder, "requestData");
              json_builder_add_value (builder, request_data);
            }
        }
      json_builder_end_object (builder);
    }
  json_builder_end_object (builder);

  promise = dex_promise_new ();
  g_hash_table_insert (state->uuid_to_promise, g_steal_pointer (&uuid), dex_ref (promise));

  generator = json_generator_new ();
  json_generator_set_root (generator, json_builder_get_root (builder));

  message = json_generator_to_data (generator, NULL);
  soup_websocket_connection_send_text (state->websocket, message);

#if TRACE_WEBSOCKET_MESSAGES
    {
      json_generator_set_pretty (generator, TRUE);

      g_autofree char *json_output = json_generator_to_data (generator, NULL);
      g_debug (">>>>>\n%s", json_output);
    }
#endif

  return (DexFuture *) g_steal_pointer (&promise);
}

static DexFuture *
process_request (WorkerFiberState *state,
                 WebSocketRequest *request)
{
  /* Authenticate requests must only happen before the dispatcher fiber
   * can call this function.
   */
  g_assert (request->type != WEBSOCKET_REQUEST_AUTHENTICATE);

  dex_future_disown (send_request (state, request->d.generic.method, request->d.generic.data));

  return dex_future_new_true ();
}


/*
 * Incoming messages
 */

static gboolean
update_scene_list_items (WorkerFiberState  *state,
                         GError           **error)
{
  g_autoptr (ObsConnection) self = NULL;
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonObject) object = NULL;
  g_autoptr (GHashTable) sources_by_uuid = NULL;
  JsonArray *scene_items;

  if (!(self = g_weak_ref_get (&state->self_wr)))
    return TRUE;

  g_assert (self->current_scene_uuid != NULL);

  builder = json_builder_new ();
  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "sceneUuid");
  json_builder_add_string_value (builder, self->current_scene_uuid);
  json_builder_end_object (builder);

  if (!(object = dex_await_boxed (send_request (state, "GetSceneItemList", json_builder_get_root (builder)), error)))
    return FALSE;

  sources_by_uuid = g_hash_table_new (g_str_hash, g_str_equal);
  for (unsigned int i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->sources)); i++)
    {
      g_autoptr (ObsSource) source = NULL;
      GtkBitset *bitset = NULL;

      source = g_list_model_get_item (G_LIST_MODEL (self->sources), i);
      bitset = g_hash_table_lookup (self->source_to_scene_items, obs_source_get_uuid (source));

      gtk_bitset_remove_all (bitset);

      g_hash_table_insert (sources_by_uuid, (gpointer) obs_source_get_uuid (source), source);
    }

  scene_items = json_object_get_array_member (object, "sceneItems");
  for (unsigned int i = 0; i < json_array_get_length (scene_items); i++)
    {
      JsonObject *scene_item;
      JsonNode *is_group;

      scene_item = json_array_get_object_element (scene_items, i);
      g_assert (scene_item != NULL);

      is_group = json_object_get_member (scene_item, "isGroup");
      if (JSON_NODE_HOLDS_NULL (is_group))
        {
          GtkBitset *bitset = NULL;
          ObsSource *source;
          const char *uuid;

          uuid = json_object_get_string_member (scene_item, "sourceUuid");
          source = g_hash_table_lookup (sources_by_uuid, uuid);
          g_assert (OBS_IS_SOURCE (source));

          if (json_object_get_boolean_member (scene_item, "sceneItemEnabled"))
            obs_source_set_visible (source, TRUE);

          bitset = g_hash_table_lookup (self->source_to_scene_items, uuid);
          g_assert (bitset != NULL);

          gtk_bitset_add (bitset, json_object_get_int_member (scene_item, "sceneItemId"));
        }
      else
        {
          // TODO: fetch group children
        }
    }

  return TRUE;
}


static DexFuture *
current_program_scene_changed_cb (WorkerFiberState *state,
                                  JsonObject       *object)
{
  g_autoptr (ObsConnection) self = NULL;
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonObject) scene_items = NULL;
  g_autoptr (GError) error = NULL;
  const char *scene_uuid;

  if (!(self = g_weak_ref_get (&state->self_wr)))
    return dex_future_new_true ();

  scene_uuid = json_object_get_string_member (object, "sceneUuid");

  if (g_set_str (&self->current_scene_uuid, scene_uuid) &&
      !update_scene_list_items (state, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_true ();
}

static DexFuture *
input_created_cb (WorkerFiberState *state,
                  JsonObject       *object)
{
  g_autoptr (ObsConnection) self = NULL;
  g_autoptr (ObsSource) source = NULL;
  ObsOutputFlags flags = OBS_OUTPUT_FLAG_NONE;
  ObsSourceCaps caps = 0;
  ObsSourceType type = OBS_SOURCE_TYPE_UNKNOWN;
  const char *kind;
  const char *name;
  const char *uuid;

  if (!(self = g_weak_ref_get (&state->self_wr)))
    return dex_future_new_true ();

  uuid = json_object_get_string_member (object, "inputUuid");
  name = json_object_get_string_member (object, "inputName");
  kind = json_object_get_string_member (object, "unversionedInputKind");
  flags = json_object_get_int_member (object, "inputKindCaps");
  if (flags & OBS_OUTPUT_VIDEO)
    caps |= OBS_SOURCE_CAP_VIDEO;
  if (flags & OBS_OUTPUT_AUDIO)
    caps |= OBS_SOURCE_CAP_AUDIO;

  type = obs_parse_source_type (kind, "input", caps);
  source = obs_source_new (uuid, name, TRUE, TRUE, type, caps);
  g_list_store_append (self->sources, source);

  return dex_future_new_true ();
}

static DexFuture *
input_mute_state_changed_cb (WorkerFiberState *state,
                             JsonObject       *object)
{
  g_autoptr (ObsConnection) self = NULL;
  const char *input_name;
  gboolean muted;

  if (!(self = g_weak_ref_get (&state->self_wr)))
    return dex_future_new_true ();

  input_name = json_object_get_string_member (object, "inputName");
  muted = json_object_get_boolean_member (object, "inputMuted");

  for (unsigned int i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->sources)); i++)
    {
      g_autoptr (ObsSource) source = g_list_model_get_item (G_LIST_MODEL (self->sources), i);

      if (g_strcmp0 (obs_source_get_name (source), input_name) == 0)
        {
          obs_source_set_muted (source, muted);
          break;
        }
    }

  return dex_future_new_true ();
}

static DexFuture *
input_removed_cb (WorkerFiberState *state,
                  JsonObject       *object)
{
  g_autoptr (ObsConnection) self = NULL;
  const char *input_name;

  if (!(self = g_weak_ref_get (&state->self_wr)))
    return dex_future_new_true ();

  input_name = json_object_get_string_member (object, "inputName");
  for (unsigned int i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->sources)); i++)
    {
      g_autoptr (ObsSource) source = g_list_model_get_item (G_LIST_MODEL (self->sources), i);

      if (g_strcmp0 (obs_source_get_name (source), input_name) == 0)
        {
          g_hash_table_remove (self->source_to_scene_items, obs_source_get_uuid (source));
          g_list_store_remove (self->sources, i);
          break;
        }
    }

  return dex_future_new_true ();
}

static DexFuture *
recording_state_changed_cb (WorkerFiberState *state,
                            JsonObject       *object)
{
  g_autoptr (ObsConnection) self = NULL;

  if (!(self = g_weak_ref_get (&state->self_wr)))
    return dex_future_new_true ();

  switch (output_state_from_string (json_object_get_string_member (object, "outputState")))
    {
    case OBS_WEBSOCKET_OUTPUT_STARTED:
      set_recording_state (self, OBS_RECORDING_STATE_RECORDING);
      break;

    case OBS_WEBSOCKET_OUTPUT_STOPPED:
      set_recording_state (self, OBS_RECORDING_STATE_STOPPED);
      break;

    case OBS_WEBSOCKET_OUTPUT_PAUSED:
      set_recording_state (self, OBS_RECORDING_STATE_PAUSED);
      break;

    case OBS_WEBSOCKET_OUTPUT_RESUMED:
      set_recording_state (self, OBS_RECORDING_STATE_RECORDING);
      break;

    case OBS_WEBSOCKET_OUTPUT_UNKNOWN:
    case OBS_WEBSOCKET_OUTPUT_STARTING:
    case OBS_WEBSOCKET_OUTPUT_STOPPING:
    case OBS_WEBSOCKET_OUTPUT_RECONNECTING:
    case OBS_WEBSOCKET_OUTPUT_RECONNECTED:
      break;

    default:
      g_assert_not_reached ();
    }

  return dex_future_new_true ();
}

static DexFuture *
scene_item_visibility_changed_cb (WorkerFiberState *state,
                                  JsonObject       *object)
{
  g_autoptr (ObsConnection) self = NULL;
  const char *source_name;
  gboolean visible;

  if (!(self = g_weak_ref_get (&state->self_wr)))
    return dex_future_new_true ();

  source_name = json_object_get_string_member (object, "item-name");
  visible = json_object_get_boolean_member (object, "item-visible");

  for (unsigned int i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->sources)); i++)
    {
      g_autoptr (ObsSource) source = g_list_model_get_item (G_LIST_MODEL (self->sources), i);

      if (g_strcmp0 (obs_source_get_name (source), source_name) == 0)
        {
          obs_source_set_visible (source, visible);
          break;
        }
    }

  return dex_future_new_true ();
}

static DexFuture *
scene_list_changed_cb (WorkerFiberState *state,
                       JsonObject       *object)
{
  g_autoptr (ObsConnection) self = NULL;
  g_autoptr (GPtrArray) new_scenes = NULL;
  JsonArray *scenes_array;
  JsonNode *scenes_node;
  unsigned int old_size;
  unsigned int i;

  if (!(self = g_weak_ref_get (&state->self_wr)))
    return dex_future_new_true ();

  scenes_node = json_object_get_member (object, "scenes");

  if (!JSON_NODE_HOLDS_ARRAY (scenes_node))
    return dex_future_new_true ();

  scenes_array = json_node_get_array (scenes_node);
  new_scenes = g_ptr_array_new_full (json_array_get_length (scenes_array), g_object_unref);
  for (i = 0; i < json_array_get_length (scenes_array); i++)
    {
      g_autoptr (ObsScene) new_scene = NULL;
      JsonObject *scene_object;

      scene_object = json_array_get_object_element (scenes_array, i);
      new_scene = obs_scene_new_from_json (self, scene_object);
      g_ptr_array_add (new_scenes, g_steal_pointer (&new_scene));
    }

  old_size = g_list_model_get_n_items (G_LIST_MODEL (self->scenes));

  g_list_store_splice (self->scenes,
                       0,
                       old_size,
                       new_scenes->pdata,
                       new_scenes->len);

  return dex_future_new_true ();
}

static DexFuture *
input_name_changed_cb (WorkerFiberState *state,
                       JsonObject       *object)
{
  g_autoptr (ObsConnection) self = NULL;
  const char *previous_name;

  if (!(self = g_weak_ref_get (&state->self_wr)))
    return dex_future_new_true ();

  previous_name = json_object_get_string_member (object, "oldInputName");

  for (unsigned int i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->sources)); i++)
    {
      g_autoptr (ObsSource) source = g_list_model_get_item (G_LIST_MODEL (self->sources), i);

      if (g_strcmp0 (obs_source_get_name (source), previous_name) == 0)
        {
          obs_source_set_name (source, json_object_get_string_member (object, "inputName"));
          break;
        }
    }

  return dex_future_new_true ();
}

static DexFuture *
stream_statate_changed_cb (WorkerFiberState *state,
                           JsonObject       *object)
{
  g_autoptr (ObsConnection) self = NULL;

  if (!(self = g_weak_ref_get (&state->self_wr)))
    return dex_future_new_true ();

  switch (output_state_from_string (json_object_get_string_member (object, "outputState")))
    {
    case OBS_WEBSOCKET_OUTPUT_STARTED:
      set_streaming (self, TRUE);
      break;

    case OBS_WEBSOCKET_OUTPUT_UNKNOWN:
    case OBS_WEBSOCKET_OUTPUT_STARTING:
    case OBS_WEBSOCKET_OUTPUT_PAUSED:
    case OBS_WEBSOCKET_OUTPUT_RESUMED:
    case OBS_WEBSOCKET_OUTPUT_STOPPING:
    case OBS_WEBSOCKET_OUTPUT_STOPPED:
    case OBS_WEBSOCKET_OUTPUT_RECONNECTING:
    case OBS_WEBSOCKET_OUTPUT_RECONNECTED:
      set_streaming (self, FALSE);
      break;

    default:
      g_assert_not_reached ();
    }

  return dex_future_new_true ();
}

static DexFuture *
virtualcam_state_changed_cb (WorkerFiberState *state,
                             JsonObject       *object)
{
  g_autoptr (ObsConnection) self = NULL;

  if (!(self = g_weak_ref_get (&state->self_wr)))
    return dex_future_new_true ();

  switch (output_state_from_string (json_object_get_string_member (object, "outputState")))
    {
    case OBS_WEBSOCKET_OUTPUT_STARTED:
      set_virtualcam_enabled (self, TRUE);
      break;

    case OBS_WEBSOCKET_OUTPUT_UNKNOWN:
    case OBS_WEBSOCKET_OUTPUT_STARTING:
    case OBS_WEBSOCKET_OUTPUT_PAUSED:
    case OBS_WEBSOCKET_OUTPUT_RESUMED:
    case OBS_WEBSOCKET_OUTPUT_STOPPING:
    case OBS_WEBSOCKET_OUTPUT_STOPPED:
    case OBS_WEBSOCKET_OUTPUT_RECONNECTING:
    case OBS_WEBSOCKET_OUTPUT_RECONNECTED:
      set_virtualcam_enabled (self, FALSE);
      break;

    default:
      g_assert_not_reached ();
    }

  return dex_future_new_true ();
}

struct {
  const char *event_name;
  DexFuture * (*trigger) (WorkerFiberState *state,
                          JsonObject       *object);
} events_vtable2[] = {
  { "CurrentProgramSceneChanged", current_program_scene_changed_cb },
  { "InputCreated", input_created_cb },
  { "InputNameChanged", input_name_changed_cb },
  { "InputMuteStateChanged", input_mute_state_changed_cb },
  { "InputRemoved", input_removed_cb },
  { "RecordStateChanged", recording_state_changed_cb },
  { "SceneItemVisibilityChanged", scene_item_visibility_changed_cb },
  { "SceneListChanged", scene_list_changed_cb },
  { "StreamStateChanged", stream_statate_changed_cb },
  { "VirtualCamStateChanged", virtualcam_state_changed_cb },
};

static DexFuture *
ingest_event_fiber (gpointer user_data)
{
  g_autoptr (JsonObject) object = NULL;
  WorkerFiberState *state;
  JsonObject *event_data = NULL;
  gpointer *pair = user_data;
  const char *event_type;
  size_t i;

  state = pair[0];
  object = g_steal_pointer (&pair[1]);

  event_type = json_object_get_string_member_with_default (object, "eventType", NULL);
  if (json_object_has_member (object, "eventData"))
    event_data = json_object_get_object_member (object, "eventData");

  for (i = 0; i < G_N_ELEMENTS (events_vtable2); i++)
    {
      if (g_strcmp0 (event_type, events_vtable2[i].event_name) == 0)
        {
          g_autoptr (GError) error = NULL;

          if (!dex_await (events_vtable2[i].trigger (state, event_data), &error))
            return dex_future_new_for_error (g_steal_pointer (&error));

          break;
        }
    }

  return dex_future_new_true ();
}


typedef DexFuture * (WebSockerOpCallback) (WorkerFiberState *state,
                                           JsonObject       *object);

static DexFuture *
TODO_stub_cb (WorkerFiberState *state,
               JsonObject       *object)
{
  return dex_future_new_true ();
}

static DexFuture *
event_cb (WorkerFiberState *state,
          JsonObject       *object)
{
  gpointer *pair = g_new0 (gpointer, 2);

  pair[0] = state;
  pair[1] = json_object_ref (object);

  dex_task_group_add (state->group,
                      dex_scheduler_spawn (NULL, 0, ingest_event_fiber, pair, g_free));

  return dex_future_new_true ();
}

static DexFuture *
request_response_cb (WorkerFiberState *state,
                     JsonObject       *object)
{
  JsonObject *response_data = NULL;
  JsonObject *request_status;
  const char *request_id;
  DexPromise *promise = NULL;

  g_assert (json_object_has_member (object, "requestId"));
  g_assert (json_object_has_member (object, "requestStatus"));

  request_id = json_object_get_string_member (object, "requestId");
  request_status = json_object_get_object_member (object, "requestStatus");

  if (json_object_has_member (object, "responseData"))
    response_data = json_object_get_object_member (object, "responseData");

  promise = g_hash_table_lookup (state->uuid_to_promise, request_id);
  g_assert (promise != NULL);

  if (json_object_get_boolean_member (request_status, "result"))
    {
      dex_promise_resolve_boxed (promise,
                                 JSON_TYPE_OBJECT,
                                 response_data ? json_object_ref (response_data) : NULL);
    }
  else
    {
      /* TODO: parse error */
      dex_promise_reject (promise, g_error_new (G_IO_ERROR,
                                                G_IO_ERROR_NOT_SUPPORTED,
                                                "Request failed with message: %s",
                                                json_object_get_string_member_with_default (request_status, "comment", "(no message)")));
    }

  g_hash_table_remove (state->uuid_to_promise, request_id);

  return dex_future_new_true ();
}

static WebSockerOpCallback *op_callbacks[] = {
  [OP_HELLO]                  = NULL,
  [OP_IDENTIFIED]             = NULL,
  [OP_EVENT]                  = event_cb,
  [OP_REQUEST]                = NULL,
  [OP_REQUEST_RESPONSE]       = request_response_cb,
  [OP_REQUEST_BATCH]          = NULL,
  [OP_REQUEST_BATCH_RESPONSE] = TODO_stub_cb,
};

static DexFuture *
process_message (WorkerFiberState *state,
                 JsonObject       *message)
{
  g_autoptr (JsonNode) node = NULL;
  WebSocketOpCode op_code;

  op_code = json_object_get_int_member_with_default (message, "op", -1);
  g_assert (op_code != -1);
  g_assert (op_callbacks[op_code] != NULL);

  return op_callbacks[op_code] (state, json_object_get_object_member (message, "d"));
}

static JsonObject *
receive_one_message (WorkerFiberState  *state,
                     GError           **error)
{
  g_autoptr (JsonObject) object = NULL;

  g_assert (state->messages_channel != NULL);

  if (!(object = dex_await_boxed (dex_future_first (dex_channel_receive (state->messages_channel),
                                                    dex_ref (state->websocket_cancellable),
                                                    NULL),
                                error)))
    return NULL;

  return g_steal_pointer (&object);
}


/*
 * Dispatcher fiber
 *
 * This is the fiber that receives the messages from obs-websockets and
 * processes these messages; this is also the fiber that receives requests
 * from Boatswain and sends them to obs-websockets.
 *
 */

static DexFuture *
connection_dispatcher_fiber (gpointer user_data)
{
  g_autoptr (DexFuture) next_request = NULL;
  g_autoptr (DexFuture) next_message = NULL;
  WorkerFiberState *state = user_data;

  g_assert (dex_state_machine_get_state (state->state_machine) == OBS_CONNECTION_STATE_CONNECTED);

  while (TRUE)
    {
      g_autoptr (ObsConnection) self = NULL;
      g_autoptr (GError) error = NULL;

      if (!state->websocket_cancellable)
        return dex_future_new_true ();

      if (!next_request)
        next_request = dex_channel_receive (state->requests_channel);

      if (!next_message)
        next_message = dex_channel_receive (state->messages_channel);

      if (dex_await (dex_future_first (dex_ref (state->websocket_cancellable),
                                       dex_ref (next_request),
                                       dex_ref (next_message),
                                       NULL),
                     NULL))
        {
          if (dex_future_is_resolved (next_request))
            {
              g_autoptr (WebSocketRequest) request = NULL;

              if (!(request = dex_await_pointer (g_steal_pointer (&next_request), &error)))
                return dex_future_new_for_error (g_steal_pointer (&error));

              if (!dex_await (process_request (state, request), &error))
                return dex_future_new_for_error (g_steal_pointer (&error));
            }

          if (dex_future_is_resolved (next_message))
            {
              g_autoptr (JsonObject) message = NULL;

              if (!(message = dex_await_boxed (g_steal_pointer (&next_message), &error)))
                return dex_future_new_for_error (g_steal_pointer (&error));

              if (!dex_await (process_message (state, message), &error))
                return dex_future_new_for_error (g_steal_pointer (&error));
            }

          if (!dex_future_is_pending (DEX_FUTURE (state->websocket_cancellable)))
            return dex_future_new_true ();
        }

      if (!(self = g_weak_ref_get (&state->self_wr)))
        break;
    }

  return dex_future_new_true ();
}


/*
 * Initial fetch fiber & helpers
 *
 * This fiber retrieves the list of sources (aka inputs) and scenes from
 * obs-websockets, their capabilities (audio, video, etc), and their states
 * (e.g. muted).
 */

static DexFuture *
fetch_inputs_fiber (gpointer user_data)
{
  g_autoptr (GHashTable) special_sources_names = NULL;
  g_autoptr (JsonObject) object = NULL;
  g_autoptr (GError) error = NULL;
  WorkerFiberState *state = user_data;

  const struct {
    const char *name;
    ObsSourceType source_type;
  } special_sources[] = {
    { "desktop1", OBS_SOURCE_TYPE_AUDIO },
    { "desktop2", OBS_SOURCE_TYPE_AUDIO },
    { "mic1", OBS_SOURCE_TYPE_MICROPHONE },
    { "mic2", OBS_SOURCE_TYPE_MICROPHONE },
    { "mic3", OBS_SOURCE_TYPE_MICROPHONE },
    { "mic4", OBS_SOURCE_TYPE_MICROPHONE },
  };

  special_sources_names = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  /* Special inputs */
  if ((object = dex_await_boxed (send_request (state, "GetSpecialInputs", NULL), &error)))
    {
      for (unsigned int i = 0; i < G_N_ELEMENTS (special_sources); i++)
        {
          const char *name;

          if (json_object_has_member (object, special_sources[i].name) &&
              (name = json_object_get_string_member (object, special_sources[i].name)))
            g_hash_table_insert (special_sources_names, g_strdup (name), GUINT_TO_POINTER (i));
        }
    }
  else
    {
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  g_clear_pointer (&object, json_object_unref);

  /* All inputs */
  if ((object = dex_await_boxed (send_request (state, "GetInputList", NULL), &error)))
    {
      JsonArray *sources = json_object_get_array_member (object, "inputs");

      for (unsigned int i = 0; i < json_array_get_length (sources); i++)
        {
          g_autoptr (ObsConnection) self = NULL;
          g_autoptr (ObsSource) source = NULL;
          JsonObject *source_object;
          ObsOutputFlags flags = OBS_OUTPUT_FLAG_NONE;
          ObsSourceCaps caps = 0;
          ObsSourceType type = OBS_SOURCE_TYPE_UNKNOWN;
          unsigned int special_source_index;
          const char *kind;
          const char *name;
          const char *uuid;
          gboolean muted = FALSE;

          source_object = json_array_get_object_element (sources, i);
          name = json_object_get_string_member (source_object, "inputName");
          uuid = json_object_get_string_member (source_object, "inputUuid");
          kind = json_object_get_string_member (source_object, "unversionedInputKind");

          /* Ignore special sources, they're already added */
          if (g_hash_table_lookup_extended (special_sources_names,
                                            name,
                                            NULL,
                                            (gpointer *) &special_source_index))
            {
              type = special_sources[special_source_index].source_type;
              caps = OBS_SOURCE_CAP_AUDIO;
            }
          else
            {
              flags = json_object_get_int_member (source_object, "inputKindCaps");
              if (flags & OBS_OUTPUT_VIDEO)
                caps |= OBS_SOURCE_CAP_VIDEO;
              if (flags & OBS_OUTPUT_AUDIO)
                caps |= OBS_SOURCE_CAP_AUDIO;

              type = obs_parse_source_type (kind, "input", caps);
            }

          /* Get mute state */
          if (caps & OBS_SOURCE_CAP_AUDIO)
            {
              g_autoptr (JsonBuilder) builder = NULL;
              g_autoptr (JsonObject) result = NULL;

              builder = json_builder_new ();
              json_builder_begin_object (builder);
              json_builder_set_member_name (builder, "inputName");
              json_builder_add_string_value (builder, name);
              json_builder_end_object (builder);

              /* TODO: batch request these */
              if (!(result = dex_await_boxed (send_request (state, "GetInputMute", json_builder_get_root (builder)), &error)))
                return dex_future_new_for_error (g_steal_pointer (&error));

              muted = json_object_get_boolean_member (result, "inputMuted");
            }

          if (!(self = g_weak_ref_get (&state->self_wr)))
            return dex_future_new_true ();

          source = obs_source_new (uuid, name, muted, FALSE, type, caps);
          g_list_store_append (self->sources, source);

          g_hash_table_insert (self->source_to_scene_items, g_strdup (uuid), gtk_bitset_new_empty ());
        }
    }
  else
    {
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  g_clear_pointer (&object, json_object_unref);

  /* Scenes */
  if ((object = dex_await_boxed (send_request (state, "GetSceneList", NULL), &error)))
    {
      g_autoptr (ObsConnection) self = NULL;
      g_autoptr (GPtrArray) new_scenes = NULL;
      JsonArray *scenes;
      const char *current_scene_uuid;

      if (!(self = g_weak_ref_get (&state->self_wr)))
        return dex_future_new_true ();

      current_scene_uuid = json_object_get_string_member (object, "currentProgramSceneUuid");
      g_assert (current_scene_uuid != NULL);
      g_set_str (&self->current_scene_uuid, current_scene_uuid);

      scenes = json_object_get_array_member (object, "scenes");

      new_scenes = g_ptr_array_new_full (json_array_get_length (scenes), g_object_unref);
      for (unsigned int i = 0; i < json_array_get_length (scenes); i++)
        {
          g_autoptr (ObsScene) scene = NULL;
          JsonObject *scene_object;

          scene_object = json_array_get_object_element (scenes, i);
          scene = obs_scene_new_from_json (self, scene_object);
          g_ptr_array_add (new_scenes, g_object_ref (scene));
        }

      g_list_store_splice (self->scenes,
                           0,
                           0,
                           new_scenes->pdata,
                           new_scenes->len);

      if (!update_scene_list_items (state, &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }
  else
    {
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  return dex_future_new_true ();
}

static DexFuture *
fetch_inputs (WorkerFiberState *state)
{
  return dex_scheduler_spawn (NULL, 0, fetch_inputs_fiber, state, NULL);
}

static DexFuture *
fetch_stream_status_fiber (gpointer user_data)
{
  g_autoptr (ObsConnection) self = NULL;
  g_autoptr (JsonObject) object = NULL;
  g_autoptr (GError) error = NULL;
  WorkerFiberState *state = user_data;

  if (!(object = dex_await_boxed (send_request (state, "GetStreamStatus", NULL), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if ((self = g_weak_ref_get (&state->self_wr)))
    set_streaming (self, json_object_get_boolean_member_with_default (object, "outputActive", FALSE));

  return dex_future_new_true ();
}

static DexFuture *
fetch_stream_status (WorkerFiberState *state)
{
  return dex_scheduler_spawn (NULL, 0, fetch_stream_status_fiber, state, NULL);
}

static DexFuture *
fetch_record_status_fiber (gpointer user_data)
{
  g_autoptr (ObsConnection) self = NULL;
  g_autoptr (JsonObject) object = NULL;
  g_autoptr (GError) error = NULL;
  WorkerFiberState *state = user_data;
  gboolean recording_paused;
  gboolean recording;

  if (!(object = dex_await_boxed (send_request (state, "GetRecordStatus", NULL), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(self = g_weak_ref_get (&state->self_wr)))
    return dex_future_new_true ();

  recording = json_object_get_boolean_member_with_default (object, "outputActive", FALSE);
  recording_paused = json_object_get_boolean_member_with_default (object, "outputPaused", FALSE);

  if (!recording)
    set_recording_state (self, OBS_RECORDING_STATE_STOPPED);
  else if (recording_paused)
    set_recording_state (self, OBS_RECORDING_STATE_PAUSED);
  else
    set_recording_state (self, OBS_RECORDING_STATE_RECORDING);

  return dex_future_new_true ();
}

static DexFuture *
fetch_record_status (WorkerFiberState *state)
{
  return dex_scheduler_spawn (NULL, 0, fetch_record_status_fiber, state, NULL);
}

static DexFuture *
fetch_virtualcam_status_fiber (gpointer user_data)
{
  g_autoptr (ObsConnection) self = NULL;
  g_autoptr (JsonObject) object = NULL;
  g_autoptr (GError) error = NULL;
  WorkerFiberState *state = user_data;
  gboolean virtualcam_enabled = FALSE;

  if (!(object = dex_await_boxed (send_request (state, "GetVirtualCamStatus", NULL), &error)))
    {
      /*
       * VirtualCam requests can fail if the host system doesn't support it. Streaming
       * and recording don't have this issue.
       */
      if (!g_str_has_suffix (error->message, "VirtualCam is not available."))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  if (object)
    virtualcam_enabled = json_object_get_boolean_member_with_default (object, "outputActive", FALSE);

  if ((self = g_weak_ref_get (&state->self_wr)))
    set_virtualcam_enabled (self, virtualcam_enabled);

  return dex_future_new_true ();
}

static DexFuture *
fetch_virtualcam_status (WorkerFiberState *state)
{
  return dex_scheduler_spawn (NULL, 0, fetch_virtualcam_status_fiber, state, NULL);
}

static DexFuture *
fetch_initial_data_fiber (gpointer user_data)
{
  g_autoptr (GError) error = NULL;
  WorkerFiberState *state = user_data;

  if (!dex_await (dex_future_all_race (fetch_inputs (state),
                                       fetch_stream_status (state),
                                       fetch_record_status (state),
                                       fetch_virtualcam_status (state),
                                       NULL),
                  &error))
     return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_true ();
}


/*
 * Connection state transitions
 */

static void
emit_state_changed_for_transition_context (ObsConnection             *self,
                                           DexStateTransitionContext *context)
{
  self->state = dex_state_transition_context_get_to (context);

  g_signal_emit (self,
                 signals[STATE_CHANGED],
                 0,
                 dex_state_transition_context_get_from (context),
                 dex_state_transition_context_get_to (context));

}

static gboolean
state_changed_cb (DexStateTransitionContext  *context,
                  gpointer                    user_data,
                  GError                    **error)
{
  g_autoptr (ObsConnection) self = NULL;
  WorkerFiberState *state = user_data;

  g_assert (state != NULL);

  if ((self = g_weak_ref_get (&state->self_wr)))
    emit_state_changed_for_transition_context (self, context);

  return TRUE;
}

static gboolean
enter_connected_cb (DexStateTransitionContext  *context,
                    gpointer                    user_data,
                    GError                    **error)
{
  g_autoptr (ObsConnection) self = NULL;
  WorkerFiberState *state = user_data;

  g_assert (state != NULL);

  if (!(self = g_weak_ref_get (&state->self_wr)))
    return TRUE;

  g_assert (state->group == NULL);

  state->group = dex_task_group_new (DEX_TASK_GROUP_FLAGS_CANCEL_ON_ERROR);

  emit_state_changed_for_transition_context (self, context);
  dex_state_transition_context_set_state (context, OBS_CONNECTION_STATE_CONNECTED);

  dex_task_group_add (state->group,
                      dex_scheduler_spawn (NULL, 0,
                                           connection_dispatcher_fiber,
                                           state,
                                           NULL));

  dex_task_group_add (state->group,
                      dex_scheduler_spawn (NULL, 0,
                                           fetch_initial_data_fiber,
                                           state,
                                           NULL));

  return TRUE;
}

static gboolean
enter_disconnected_cb (DexStateTransitionContext  *context,
                       gpointer                    user_data,
                       GError                    **error)
{
  g_autoptr (ObsConnection) self = NULL;
  WorkerFiberState *state = user_data;

  g_assert (state != NULL);

  if (!(self = g_weak_ref_get (&state->self_wr)))
    return TRUE;

  if (state->group)
    dex_task_group_cancel (state->group);

  dex_clear (&state->group);

  g_hash_table_remove_all (self->source_to_scene_items);
  g_list_store_remove_all (self->sources);
  g_list_store_remove_all (self->scenes);
  set_virtualcam_enabled (self, FALSE);
  set_recording_state (self, OBS_RECORDING_STATE_STOPPED);
  set_streaming (self, FALSE);

  if (state->messages_channel)
    {
      dex_channel_close_receive (state->messages_channel);
      dex_channel_close_send (state->messages_channel);
      dex_clear (&state->messages_channel);
    }

  g_clear_object (&state->websocket);
  g_clear_pointer (&state->authentication.salt, g_free);
  g_clear_pointer (&state->authentication.challenge, g_free);

  emit_state_changed_for_transition_context (self, context);

  return TRUE;
}

static const DexStateTransition connection_state_transitions[] =
{
  { OBS_CONNECTION_STATE_DISCONNECTED, OBS_CONNECTION_STATE_CONNECTING, state_changed_cb },
  { OBS_CONNECTION_STATE_CONNECTING, OBS_CONNECTION_STATE_AUTHENTICATING, state_changed_cb },
  { OBS_CONNECTION_STATE_AUTHENTICATING, OBS_CONNECTION_STATE_WAITING_FOR_CREDENTIALS, state_changed_cb },
  { OBS_CONNECTION_STATE_WAITING_FOR_CREDENTIALS, OBS_CONNECTION_STATE_CONNECTED, enter_connected_cb },

  /* When a saved password succeeds, or no authentication is required */
  { OBS_CONNECTION_STATE_AUTHENTICATING, OBS_CONNECTION_STATE_CONNECTED, enter_connected_cb },

  /* OBS can vanish at any point, all states can transition to disconnected */
  { OBS_CONNECTION_STATE_CONNECTING, OBS_CONNECTION_STATE_DISCONNECTED, enter_disconnected_cb },
  { OBS_CONNECTION_STATE_AUTHENTICATING, OBS_CONNECTION_STATE_DISCONNECTED, enter_disconnected_cb },
  { OBS_CONNECTION_STATE_WAITING_FOR_CREDENTIALS, OBS_CONNECTION_STATE_DISCONNECTED, enter_disconnected_cb },
  { OBS_CONNECTION_STATE_CONNECTED, OBS_CONNECTION_STATE_DISCONNECTED, enter_disconnected_cb },
};

static void
on_websocket_closed_cb (SoupWebsocketConnection *websocket,
                        WorkerFiberState        *state)
{
  g_assert (state->websocket_cancellable != NULL);

  dex_future_disown (dex_state_machine_transition (state->state_machine, OBS_CONNECTION_STATE_DISCONNECTED));

  dex_cancellable_cancel (state->websocket_cancellable);
  dex_clear (&state->websocket_cancellable);
}

static void
on_websocket_error_cb (SoupWebsocketConnection *websocket,
                       WorkerFiberState        *state)
{
  g_warning ("Websocket error");
}

static void
on_websocket_message_cb (SoupWebsocketConnection *websocket,
                         int                      type,
                         GBytes                  *message,
                         WorkerFiberState        *state)
{
  g_autoptr (JsonParser) parser = NULL;
  g_autoptr (JsonNode) root = NULL;
  g_autoptr (GError) error = NULL;
  const char *data;
  size_t length;

  data = g_bytes_get_data (message, &length);

  parser = json_parser_new_immutable ();
  json_parser_load_from_data (parser, data, length, &error);

  if (error)
    {
      g_warning ("Error parsing message: %s", error->message);
      return;
    }

  root = json_parser_steal_root (parser);

#if TRACE_WEBSOCKET_MESSAGES
    {
      g_autoptr (JsonGenerator) generator = json_generator_new ();
      json_generator_set_root (generator, root);
      json_generator_set_pretty (generator, TRUE);

      g_autofree char *json_output = json_generator_to_data (generator, NULL);
      g_debug ("<<<<<\n%s", json_output);
    }
#endif

  dex_future_disown (dex_channel_send (state->messages_channel,
                                       dex_future_new_take_boxed (JSON_TYPE_OBJECT,
                                                                  json_node_dup_object (root))));
}

static void
websocket_connected_cb (GObject      *source_object,
                         GAsyncResult *result,
                         gpointer      user_data)
{

  g_autoptr (SoupWebsocketConnection) websocket = NULL;
  g_autoptr (DexPromise) promise = user_data;
  g_autoptr (GError) error = NULL;

  if ((websocket = soup_session_websocket_connect_finish (SOUP_SESSION (source_object), result, &error)))
    dex_promise_resolve_object (promise, g_steal_pointer (&websocket));
  else
    dex_promise_reject (promise, g_steal_pointer (&error));
}

static DexFuture *
create_websocket (WorkerFiberState *state)
{
  g_autoptr (SoupMessage) message = NULL;
  g_autoptr (DexPromise) promise = NULL;
  g_autofree char *address = NULL;

  promise = dex_promise_new ();
  address = g_strdup_printf ("%s:%u", state->host, state->port);
  message = soup_message_new (SOUP_METHOD_GET, address);

  soup_session_websocket_connect_async (state->session,
                                        message,
                                        NULL,
                                        NULL,
                                        G_PRIORITY_DEFAULT,
                                        NULL,
                                        websocket_connected_cb,
                                        dex_ref (promise));

  return DEX_FUTURE (g_steal_pointer (&promise));
}

static SoupWebsocketConnection *
connect_to_websocket (WorkerFiberState  *state,
                      GError           **error)
{
  g_autoptr (SoupWebsocketConnection) websocket = NULL;

  do
    {
      if (!dex_await (dex_state_machine_transition (state->state_machine, OBS_CONNECTION_STATE_CONNECTING), error))
        return NULL;

      if (!(websocket = dex_await_object (create_websocket (state), error)))
        {
          if (!dex_await (dex_state_machine_transition (state->state_machine, OBS_CONNECTION_STATE_DISCONNECTED), error))
            return NULL;

          dex_await (dex_timeout_new_seconds (1), NULL);
        }
    }
  while (websocket == NULL);

  state->messages_channel = dex_channel_new (0);
  state->websocket_cancellable = dex_cancellable_new ();
  dex_future_set_static_name (DEX_FUTURE (state->websocket_cancellable), "websocket cancellable");

  return g_steal_pointer (&websocket);
}

static gboolean
receive_hello (WorkerFiberState  *state,
               GError           **error)
{
  g_autoptr (JsonObject) message = NULL;
  JsonObject *d;
  WebSocketOpCode op_code;
  const char *version;
  int rpc_version;

  if (!(message = receive_one_message (state, error)))
    return FALSE;

  op_code = json_object_get_int_member_with_default (message, "op", -1);
  g_assert (op_code == OP_HELLO);

  d = json_object_get_object_member (message, "d");

  version = json_object_get_string_member_with_default (d, "obsWebSocketVersion", "unknown");
  rpc_version = json_object_get_int_member_with_default (d, "rpcVersion", 0);

  g_debug ("obs-websocket version: %s (server RPC: %d)", version, rpc_version);

  state->authentication.required = json_object_has_member (d, "authentication");
  if (state->authentication.required)
    {
      g_autofree char *password = NULL;
      JsonObject *auth;

      g_debug ("Connection needs authentication");

      auth = json_object_get_object_member (d, "authentication");

      g_assert (auth != NULL);
      g_assert (json_object_has_member (auth, "challenge"));
      g_assert (json_object_has_member (auth, "salt"));

      state->authentication.challenge = g_strdup (json_object_get_string_member (auth, "challenge"));
      state->authentication.salt = g_strdup (json_object_get_string_member (auth, "salt"));
    }

  return TRUE;
}

static char *
wait_for_authentication_password (WorkerFiberState  *state,
                                  GError           **error)
{
  g_autoptr (WebSocketRequest) request = NULL;
  g_autofree char *password = NULL;

  if (!(request = dex_await_pointer (dex_future_first (dex_channel_receive (state->requests_channel),
                                                       dex_ref (state->websocket_cancellable),
                                                       NULL),
                                     error)))
    return NULL;

  g_assert (request->type == WEBSOCKET_REQUEST_AUTHENTICATE);

  return g_steal_pointer (&request->d.authenticate.password);
}

static gboolean
identify (WorkerFiberState  *state,
          const char        *password,
          GError           **error)
{
  g_autoptr (JsonGenerator) generator = NULL;
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (DexPromise) promise = NULL;
  g_autoptr (JsonObject) message = NULL;
  g_autoptr (GError) local_error = NULL;
  g_autofree char *request = NULL;
  WebSocketOpCode op_code;

  /* The authentication request is completely different from any other
   * request, so we have to build it manually here.
   */
  builder = json_builder_new ();
  json_builder_begin_object (builder);
    {
      json_builder_set_member_name (builder, "op");
      json_builder_add_int_value (builder, OP_IDENTIFY);

      json_builder_set_member_name (builder, "d");
      json_builder_begin_object (builder);
        {
          json_builder_set_member_name (builder, "rpcVersion");
          json_builder_add_int_value (builder, 1);

          if (state->authentication.required)
            {
              g_autofree char *auth = NULL;

              g_assert (password != NULL);

              auth = generate_auth_string (password,
                                           state->authentication.challenge,
                                           state->authentication.salt);

              json_builder_set_member_name (builder, "authentication");
              json_builder_add_string_value (builder, auth);
            }
          else
            {
              g_assert (password == NULL);
            }
        }
      json_builder_end_object (builder);
    }
  json_builder_end_object (builder);

  generator = json_generator_new ();
  json_generator_set_root (generator, json_builder_get_root (builder));

  request = json_generator_to_data (generator, NULL);
  soup_websocket_connection_send_text (state->websocket, request);

#if TRACE_WEBSOCKET_MESSAGES
    {
      json_generator_set_pretty (generator, TRUE);

      g_autofree char *json_output = json_generator_to_data (generator, NULL);
      g_debug (">>>>>\n%s", json_output);
    }
#endif

  if (!(message = receive_one_message (state, &local_error)))
    {
      /* We have to watch out for the cancellation here because obs-websocket
       * kicks us out when authentication fails, and at this point we're not
       * yet in the main event loop of the main fiber.
       */
      if (!g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_propagate_error (error, g_steal_pointer (&local_error));
      return FALSE;
    }

  op_code = json_object_get_int_member_with_default (message, "op", -1);
  g_assert (op_code == OP_IDENTIFIED);

  return TRUE;
}

static gboolean
initialize_websocket_connection (WorkerFiberState  *state,
                                 GError           **error)
{
  gboolean authenticate_from_secret = TRUE;

  g_assert (error != NULL && *error == NULL);

  if (dex_state_machine_get_state (state->state_machine) == OBS_CONNECTION_STATE_CONNECTED)
    return TRUE;

retry:
  /* 1. Create a SoupWebsocketConnection */
  if (!(state->websocket = connect_to_websocket (state, error)))
     return FALSE;

  g_assert (SOUP_IS_WEBSOCKET_CONNECTION (state->websocket));

  g_signal_connect (state->websocket, "closed", G_CALLBACK (on_websocket_closed_cb), state);
  g_signal_connect (state->websocket, "error", G_CALLBACK (on_websocket_error_cb), state);
  g_signal_connect (state->websocket, "message", G_CALLBACK (on_websocket_message_cb), state);

  g_debug ("Connected to websocket, waiting for HELLO");

  /* 2. OP_HELLO */
  if (!receive_hello (state, error))
     return FALSE;

  if (!dex_await (dex_state_machine_transition (state->state_machine, OBS_CONNECTION_STATE_AUTHENTICATING), error))
     return FALSE;

  /* 3. OP_IDENTIFY */
  if (state->authentication.required)
    {
      g_autofree char *password = NULL;

      /* Attempt 1: authenticate from a stored secret, if any */
      if (authenticate_from_secret)
        {
          if (!(password = dex_await_string (load_password (state), error)) && *error)
            return FALSE;
        }

      if (!password)
        {
          authenticate_from_secret = FALSE;

          if (!dex_await (dex_state_machine_transition (state->state_machine, OBS_CONNECTION_STATE_WAITING_FOR_CREDENTIALS), error))
            return FALSE;

          if (!(password = wait_for_authentication_password (state, error)))
            return FALSE;
        }

      g_assert (password != NULL);

      /*
       * When the password is wrong, obs-websocket kicks us out. We have to
       * recreate the SoupWebsocketConnection and start from scratch.
       */

      if (!identify (state, password, error))
        {
          g_clear_pointer (&password, g_free);

          if (*error)
            return FALSE;

          /*
           * Only emit the "authentication-failed" when a user-typed password failed.
           * Authentication from the stored secret should fail quietly.
           */
          if (!authenticate_from_secret)
            {
              g_autoptr (ObsConnection) self = NULL;

              if ((self = g_weak_ref_get (&state->self_wr)))
                g_signal_emit (self, signals[AUTHENTICATION_FAILED], 0);
            }

          authenticate_from_secret = FALSE;
          goto retry;
        }

      if (!authenticate_from_secret)
        store_password (state, password);
    }
  else
    {
      if (!identify (state, NULL, error) && *error)
        return FALSE;
    }

  /* 4. Transition to OBS_CONNECTION_STATE_CONNECTED triggers the initial fetch */
  if (!dex_await (dex_state_machine_transition (state->state_machine, OBS_CONNECTION_STATE_CONNECTED), error))
    return FALSE;

  return TRUE;
}

static DexFuture *
main_connection_fiber (gpointer user_data)
{
  g_autoptr (DexFuture) next_request = NULL;
  g_autoptr (DexFuture) next_message = NULL;
  WorkerFiberState *state = user_data;

  g_assert (state != NULL);
  g_assert (state->requests_channel != NULL);
  g_assert (state->websocket == NULL);
  g_assert (dex_state_machine_get_state (state->state_machine) == OBS_CONNECTION_STATE_DISCONNECTED);
  g_assert (dex_state_machine_get_requested_state (state->state_machine) == OBS_CONNECTION_STATE_DISCONNECTED);

  while (TRUE)
    {
      g_autoptr (ObsConnection) self = NULL;
      g_autoptr (GError) error = NULL;

      if (!initialize_websocket_connection (state, &error))
        return dex_future_new_for_error (g_steal_pointer (&error));

      g_assert (state->group != NULL);

      dex_await (dex_ref (state->group), &error);

      self = g_weak_ref_get (&state->self_wr);
      if (!self)
        break;
    }

  dex_channel_close_receive (state->requests_channel);

  return dex_future_new_true ();
}


/*
 * GObject overrides
 */

static void
obs_connection_finalize (GObject *object)
{
  ObsConnection *self = (ObsConnection *)object;

  dex_channel_close_send (self->channel);

  dex_clear (&self->channel);
  g_clear_pointer (&self->source_to_scene_items, g_hash_table_destroy);
  g_clear_pointer (&self->current_scene_uuid, g_free);
  g_clear_pointer (&self->host, g_free);
  g_clear_object (&self->sources);
  g_clear_object (&self->scenes);

  G_OBJECT_CLASS (obs_connection_parent_class)->finalize (object);
}

static void
obs_connection_constructed (GObject *object)
{
  ObsConnection *self = (ObsConnection *)object;
  WorkerFiberState *state = NULL;
  DexFuture *future;

  G_OBJECT_CLASS (obs_connection_parent_class)->constructed (object);

  state = g_new0 (WorkerFiberState, 1);
  g_weak_ref_init (&state->self_wr, self);
  state->host = g_strdup (self->host);
  state->port = self->port;
  state->session = soup_session_new ();
  state->requests_channel = dex_ref (self->channel);
  state->uuid_to_promise = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, dex_unref);
  state->state_machine = dex_state_machine_new (OBS_TYPE_CONNECTION_STATE,
                                                OBS_CONNECTION_STATE_DISCONNECTED,
                                                connection_state_transitions,
                                                G_N_ELEMENTS (connection_state_transitions),
                                                NULL, 0,
                                                state,
                                                NULL);

  future = dex_scheduler_spawn (NULL, 0,
                                main_connection_fiber,
                                state,
                                worker_fiber_state_free);
  dex_future_set_static_name (future, "main_connection_fiber");
  dex_future_disown (future);
}

static void
obs_connection_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  ObsConnection *self = OBS_CONNECTION (object);

  switch (prop_id)
    {
    case PROP_HOST:
      g_value_set_string (value, self->host);
      break;

    case PROP_PORT:
      g_value_set_uint (value, self->port);
      break;

    case PROP_RECORDING_STATE:
      g_value_set_int (value, self->recording_state);
      break;

    case PROP_STREAMING:
      g_value_set_boolean (value, self->streaming);
      break;

    case PROP_VIRTUALCAM_ENABLED:
      g_value_set_boolean (value, self->virtualcam_enabled);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
obs_connection_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  ObsConnection *self = OBS_CONNECTION (object);

  switch (prop_id)
    {
    case PROP_PORT:
      self->port = g_value_get_uint (value);
      break;

    case PROP_HOST:
      g_assert (self->host == NULL);
      self->host = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
obs_connection_class_init (ObsConnectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = obs_connection_finalize;
  object_class->constructed = obs_connection_constructed;
  object_class->get_property = obs_connection_get_property;
  object_class->set_property = obs_connection_set_property;

  properties[PROP_HOST] = g_param_spec_string ("host", NULL, NULL,
                                               NULL,
                                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_PORT] = g_param_spec_uint ("port", NULL, NULL,
                                             0, G_MAXUINT, 0,
                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_RECORDING_STATE] = g_param_spec_int ("recording-state", NULL, NULL,
                                                       0, G_MAXINT, 0,
                                                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_STREAMING] = g_param_spec_boolean ("streaming", NULL, NULL,
                                                     FALSE,
                                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_VIRTUALCAM_ENABLED] = g_param_spec_boolean ("virtualcam-enabled", NULL, NULL,
                                                              FALSE,
                                                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[STATE_CHANGED] = g_signal_new ("state-changed",
                                         OBS_TYPE_CONNECTION,
                                         G_SIGNAL_RUN_LAST,
                                         0, NULL, NULL, NULL,
                                         G_TYPE_NONE,
                                         2,
                                         G_TYPE_INT,
                                         G_TYPE_INT);

  signals[AUTHENTICATION_FAILED] = g_signal_new ("authentication-failed",
                                                 OBS_TYPE_CONNECTION,
                                                 G_SIGNAL_RUN_LAST,
                                                 0, NULL, NULL, NULL,
                                                 G_TYPE_NONE,
                                                 0);
}

static void
obs_connection_init (ObsConnection *self)
{
  self->channel = dex_channel_new (0);
  self->scenes = g_list_store_new (OBS_TYPE_SCENE);
  self->sources = g_list_store_new (OBS_TYPE_SOURCE);
  self->source_to_scene_items = g_hash_table_new_full (g_str_hash,
                                                       g_str_equal,
                                                       g_free,
                                                       (GDestroyNotify) gtk_bitset_unref);
  self->recording_state = OBS_RECORDING_STATE_STOPPED;
}

ObsConnection *
obs_connection_new (const char   *host,
                    unsigned int  port)
{
  return g_object_new (OBS_TYPE_CONNECTION,
                       "host", host,
                       "port", port,
                       NULL);
}

const char *
obs_connection_get_host (ObsConnection *self)
{
  g_return_val_if_fail (OBS_IS_CONNECTION (self), NULL);

  return self->host;
}

unsigned int
obs_connection_get_port (ObsConnection *self)
{
  g_return_val_if_fail (OBS_IS_CONNECTION (self), 0);

  return self->port;
}

ObsConnectionState
obs_connection_get_state (ObsConnection *self)
{
  g_return_val_if_fail (OBS_IS_CONNECTION (self), OBS_CONNECTION_STATE_DISCONNECTED);

  return self->state;
}

ObsRecordingState
obs_connection_get_recording_state (ObsConnection *self)
{
  g_return_val_if_fail (OBS_IS_CONNECTION (self), OBS_RECORDING_STATE_STOPPED);

  return self->recording_state;
}

gboolean
obs_connection_get_streaming (ObsConnection *self)
{
  g_return_val_if_fail (OBS_IS_CONNECTION (self), FALSE);

  return self->streaming;
}

gboolean
obs_connection_get_virtualcam_enabled (ObsConnection *self)
{
  g_return_val_if_fail (OBS_IS_CONNECTION (self), FALSE);

  return self->virtualcam_enabled;
}

void
obs_connection_authenticate (ObsConnection *self,
                             const char    *password)
{
  WebSocketRequest *request = NULL;
  DexFuture *future = NULL;

  g_return_if_fail (OBS_IS_CONNECTION (self));
  g_return_if_fail (password != NULL && g_utf8_validate (password, -1, NULL));

  if (self->state != OBS_CONNECTION_STATE_WAITING_FOR_CREDENTIALS)
    {
      g_warning ("Cannot authenticate when not waiting for credentials");
      return;
    }

  request = websocket_request_new_authenticate (password);
  future = dex_future_new_for_pointer (request);

  dex_future_disown (dex_channel_send (self->channel, future));
}

GListModel *
obs_connection_get_scenes (ObsConnection *self)
{
  g_return_val_if_fail (OBS_IS_CONNECTION (self), NULL);

  return G_LIST_MODEL (self->scenes);
}

GListModel *
obs_connection_get_sources (ObsConnection *self)
{
  g_return_val_if_fail (OBS_IS_CONNECTION (self), NULL);

  return G_LIST_MODEL (self->sources);
}

void
obs_connection_switch_to_scene (ObsConnection *self,
                                ObsScene      *scene)
{
  g_autoptr (JsonBuilder) builder = NULL;
  WebSocketRequest *request = NULL;
  DexFuture *future = NULL;

  g_return_if_fail (OBS_IS_CONNECTION (self));
  g_return_if_fail (OBS_IS_SCENE (scene));
  g_return_if_fail (self->state == OBS_CONNECTION_STATE_CONNECTED);

  builder = json_builder_new ();
  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "sceneName");
  json_builder_add_string_value (builder, obs_scene_get_name (scene));
  json_builder_end_object (builder);

  request = websocket_request_new_generic ("SetCurrentProgramScene",
                                           json_builder_get_root (builder));
  future = dex_future_new_for_pointer (request);

  dex_future_disown (dex_channel_send (self->channel, future));
}

void
obs_connection_toggle_recording (ObsConnection *self)
{
  WebSocketRequest *request = NULL;
  DexFuture *future = NULL;

  g_return_if_fail (OBS_IS_CONNECTION (self));
  g_return_if_fail (self->state == OBS_CONNECTION_STATE_CONNECTED);

  request = websocket_request_new_generic ("ToggleRecord", NULL);
  future = dex_future_new_for_pointer (request);

  dex_future_disown (dex_channel_send (self->channel, future));
}

void
obs_connection_toggle_streaming (ObsConnection *self)
{
  WebSocketRequest *request = NULL;
  DexFuture *future = NULL;

  g_return_if_fail (OBS_IS_CONNECTION (self));
  g_return_if_fail (self->state == OBS_CONNECTION_STATE_CONNECTED);

  request = websocket_request_new_generic ("ToggleStream", NULL);
  future = dex_future_new_for_pointer (request);

  dex_future_disown (dex_channel_send (self->channel, future));
}

void
obs_connection_toggle_virtualcam (ObsConnection *self)
{
  WebSocketRequest *request = NULL;
  DexFuture *future = NULL;

  g_return_if_fail (OBS_IS_CONNECTION (self));
  g_return_if_fail (self->state == OBS_CONNECTION_STATE_CONNECTED);

  request = websocket_request_new_generic ("ToggleVirtualCam", NULL);
  future = dex_future_new_for_pointer (request);

  dex_future_disown (dex_channel_send (self->channel, future));
}

void
obs_connection_toggle_source_mute (ObsConnection *self,
                                   ObsSource     *source)
{
  g_autoptr (JsonBuilder) builder = NULL;
  WebSocketRequest *request = NULL;
  DexFuture *future = NULL;

  g_return_if_fail (OBS_IS_CONNECTION (self));
  g_return_if_fail (OBS_IS_SOURCE (source));
  g_return_if_fail (self->state == OBS_CONNECTION_STATE_CONNECTED);
  g_return_if_fail (obs_source_get_caps (source) & OBS_SOURCE_CAP_AUDIO);

  builder = json_builder_new ();
  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "inputName");
  json_builder_add_string_value (builder, obs_source_get_name (source));
  json_builder_end_object (builder);

  request = websocket_request_new_generic ("ToggleInputMute",
                                           json_builder_get_root (builder));
  future = dex_future_new_for_pointer (request);

  dex_future_disown (dex_channel_send (self->channel, future));
}

void
obs_connection_set_source_mute (ObsConnection *self,
                                ObsSource     *source,
                                gboolean       mute)
{
  g_autoptr (JsonBuilder) builder = NULL;
  WebSocketRequest *request = NULL;
  DexFuture *future = NULL;

  g_return_if_fail (OBS_IS_CONNECTION (self));
  g_return_if_fail (OBS_IS_SOURCE (source));
  g_return_if_fail (self->state == OBS_CONNECTION_STATE_CONNECTED);
  g_return_if_fail (obs_source_get_caps (source) & OBS_SOURCE_CAP_AUDIO);

  builder = json_builder_new ();
  json_builder_begin_object (builder);
    {
      json_builder_set_member_name (builder, "inputName");
      json_builder_add_string_value (builder, obs_source_get_name (source));

      json_builder_set_member_name (builder, "inputMuted");
      json_builder_add_boolean_value (builder, mute);
    }
  json_builder_end_object (builder);

  request = websocket_request_new_generic ("SetInputMute",
                                           json_builder_get_root (builder));
  future = dex_future_new_for_pointer (request);

  dex_future_disown (dex_channel_send (self->channel, future));
}

void
obs_connection_toggle_source_visible (ObsConnection *self,
                                      ObsSource     *source)
{
  g_return_if_fail (OBS_IS_CONNECTION (self));
  g_return_if_fail (OBS_IS_SOURCE (source));
  g_return_if_fail (self->state == OBS_CONNECTION_STATE_CONNECTED);
  g_return_if_fail (obs_source_get_caps (source) & OBS_SOURCE_CAP_VIDEO);

  obs_connection_set_source_visible (self, source, !obs_source_get_visible (source));
}

void
obs_connection_set_source_visible (ObsConnection *self,
                                   ObsSource     *source,
                                   gboolean       visible)
{
  g_autoptr (JsonBuilder) builder = NULL;
  WebSocketRequest *request = NULL;
  GtkBitset *bitset;
  DexFuture *future = NULL;

  g_return_if_fail (OBS_IS_CONNECTION (self));
  g_return_if_fail (OBS_IS_SOURCE (source));
  g_return_if_fail (self->state == OBS_CONNECTION_STATE_CONNECTED);
  g_return_if_fail (obs_source_get_caps (source) & OBS_SOURCE_CAP_VIDEO);

  bitset = g_hash_table_lookup (self->source_to_scene_items, obs_source_get_uuid (source));
  g_assert (bitset != NULL);

  if (visible == obs_source_get_visible (source))
    return;

  if (gtk_bitset_is_empty (bitset))
    return;

  /* TODO: batch send all scene items */

  builder = json_builder_new ();
  json_builder_begin_object (builder);
    {
      json_builder_set_member_name (builder, "sceneUuid");
      json_builder_add_string_value (builder, self->current_scene_uuid);

      json_builder_set_member_name (builder, "sceneItemId");
      json_builder_add_int_value (builder, gtk_bitset_get_minimum (bitset));

      json_builder_set_member_name (builder, "sceneItemEnabled");
      json_builder_add_boolean_value (builder, visible);

    }
  json_builder_end_object (builder);

  request = websocket_request_new_generic ("SetSceneItemEnabled",
                                           json_builder_get_root (builder));
  future = dex_future_new_for_pointer (request);

  dex_future_disown (dex_channel_send (self->channel, future));

  obs_source_set_visible (source, visible);
}
