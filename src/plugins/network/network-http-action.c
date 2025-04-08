/* network-http-action.c
 *
 * Copyright 2023 Lorenzo Prosseda
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

#define G_LOG_DOMAIN "HTTP Request"

#include "network-http-action.h"
#include "network-http-action-prefs.h"

#include <glib/gi18n.h>
#include <libsoup/soup.h>

struct _NetworkHttpAction
{
  BsAction parent_instance;

  SoupSession *session;
  char *uri;
  RequestMethod method;
  PayloadSource source;

  char *text;
  GFile *file;

  GtkWidget *prefs;
  gboolean deserializing;
  GCancellable *cancellable;
};

G_DEFINE_FINAL_TYPE (NetworkHttpAction, network_http_action, BS_TYPE_ACTION)

enum
{
  PROP_0,
  PROP_PAYLOAD_TEXT,
  PROP_SESSION,
  N_PROPS,
};

static GParamSpec *properties[N_PROPS];

/*
 * Auxiliary methods
 */

static void
maybe_save_action (NetworkHttpAction *self)
{
  if (!self->deserializing)
    bs_action_changed (BS_ACTION (self));
}

static char *
get_payload_file_contents (NetworkHttpAction *self)
{
  g_autoptr (GError) error = NULL;
  g_autofree char *payload = NULL;

  /* If a file is not selected, return immediately */
  if (!self->file)
    return NULL;

  g_file_get_contents (g_file_peek_path (self->file),
                       &payload,
                       NULL,
                       &error);

  if (error)
    {
      g_warning ("Error reading file contents: %s", error->message);
      return NULL;
    }

  return g_steal_pointer (&payload);
}


/*
 * Callbacks
 */

static void
network_connected_cb (GObject      *source,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  g_autoptr (GInputStream) response_body = NULL;
  g_autoptr (GError) error = NULL;

  response_body = soup_session_send_finish (SOUP_SESSION (source), result, &error);

  if (error)
    g_warning ("The request failed: %s", error->message);
  else
    g_debug ("Request was successful");
}


/*
 * BsAction overrides
 */

static void
network_http_action_handle_event (BsAction *action,
                                      BsEvent  *event)
{
  NetworkHttpAction *self = (NetworkHttpAction *) action;
  g_autoptr (SoupMessage) message = NULL;
  SoupMessageHeaders *headers;
  char *method;

  g_assert (NETWORK_IS_HTTP_ACTION (self));

  if (bs_event_get_event_type (event) != BS_BUTTON_PRESS)
    return;

  if (!self->uri || self->uri[0] == '\0')
    return;

  switch (self->method)
    {
      case METHOD_GET:
        method = "GET";
        break;
      case METHOD_POST:
        method = "POST";
        break;
    }

  message = soup_message_new (method, self->uri);

  if (!message)
    {
      g_warning ("Error initializing message");
      return;
    }

  headers = soup_message_get_request_headers (message);
  soup_message_headers_append (headers, "Connection", "close");

  /* Only on POST method, try to attach payload */
  if (self->method == METHOD_POST)
    {
      g_autofree char *payload = NULL;

      /* Read payload from selected source */
      switch (self->source)
        {
        case SRC_FILE:
          payload = get_payload_file_contents (self);
          break;

        case SRC_TEXT:
          payload = g_strdup (self->text);
          break;
        }

      /* If payload exists attach it to request, otherwise set length header to 0 */
      if (payload)
        {
          g_autoptr (GBytes) bytes = NULL;
          size_t payload_len;

          g_debug ("Read payload from %s:\n'%s'",
                   self->source == SRC_FILE ? "TEXT" : "FILE",
                   payload);

          payload_len = strlen (payload);
          bytes = g_bytes_new_take (g_steal_pointer (&payload), payload_len);

          soup_message_set_request_body_from_bytes (message,
                                                    "application/json",
                                                    bytes);
        }
      else
        {
          soup_message_headers_append (headers, "Content-Length", "0");
        }
    }

  g_assert (self->session != NULL);

  soup_session_send_async (self->session,
                           message,
                           G_PRIORITY_DEFAULT,
                           self->cancellable,
                           network_connected_cb,
                           self);

  g_info ("HTTP %s request to URI: '%s'", method, self->uri);
}

static GtkWidget *
network_http_action_get_preferences (BsAction *action)
{
  NetworkHttpAction *self = NETWORK_HTTP_ACTION (action);
  return self->prefs;
}

static JsonNode *
network_http_action_serialize_settings (BsAction *action)
{
  NetworkHttpAction *self = NETWORK_HTTP_ACTION (action);
  g_autoptr (JsonBuilder) builder = NULL;

  builder = json_builder_new ();
  json_builder_begin_object (builder);

  /* URI text field */
  json_builder_set_member_name (builder, "uri");
  if (self->uri)
    json_builder_add_string_value (builder, self->uri);
  else
    json_builder_add_null_value (builder);

  /* Method drop-down */
  json_builder_set_member_name (builder, "method");
  json_builder_add_int_value (builder, self->method);

  /* Payload source drop-down */
  json_builder_set_member_name (builder, "source");
  json_builder_add_int_value (builder, self->source);

  /* Payload file chooser */
  json_builder_set_member_name (builder, "payload_file");
  if (self->file)
    json_builder_add_string_value (builder, g_file_peek_path (self->file));
  else
    json_builder_add_null_value (builder);

  /* Payload text contents */
  json_builder_set_member_name (builder, "payload_text");
  if (self->uri)
    json_builder_add_string_value (builder, self->text);
  else
    json_builder_add_null_value (builder);

  json_builder_end_object (builder);
  return json_builder_get_root (builder);
}

static void
network_http_action_deserialize_settings (BsAction   *action,
                                          JsonObject *settings)
{
  NetworkHttpAction *self = NETWORK_HTTP_ACTION (action);
  self->deserializing = TRUE;

  network_http_action_prefs_deserialize_settings (NETWORK_HTTP_ACTION_PREFS (self->prefs),
                                                  settings);

  self->deserializing = FALSE;
}


/*
 * GObject overrides
 */

static void
network_http_action_finalize (GObject *object)
{
  NetworkHttpAction *self = (NetworkHttpAction *)object;

  g_cancellable_cancel (self->cancellable);

  g_clear_pointer (&self->text, g_free);
  g_clear_pointer (&self->uri, g_free);

  g_clear_object (&self->file);
  g_clear_object (&self->prefs);
  g_clear_object (&self->session);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (network_http_action_parent_class)->finalize (object);
}

static void
network_http_action_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  NetworkHttpAction *self = (NetworkHttpAction *)object;

  switch (prop_id)
    {
    case PROP_PAYLOAD_TEXT:
      g_value_set_string (value, self->text);
      break;

    case PROP_SESSION:
      g_value_set_object (value, self->session);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
network_http_action_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  NetworkHttpAction *self = (NetworkHttpAction *)object;

  switch (prop_id)
    {
    case PROP_PAYLOAD_TEXT:
      network_http_action_set_payload_text (self, g_value_get_string (value));
      break;

    case PROP_SESSION:
      g_assert (self->session == NULL);
      self->session = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
network_http_action_class_init (NetworkHttpActionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  BsActionClass *action_class = BS_ACTION_CLASS (klass);

  object_class->finalize = network_http_action_finalize;
  object_class->get_property = network_http_action_get_property;
  object_class->set_property = network_http_action_set_property;

  action_class->handle_event = network_http_action_handle_event;
  action_class->get_preferences = network_http_action_get_preferences;
  action_class->serialize_settings = network_http_action_serialize_settings;
  action_class->deserialize_settings = network_http_action_deserialize_settings;

  properties[PROP_PAYLOAD_TEXT] =
    g_param_spec_string ("payload-text", NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_SESSION] =
    g_param_spec_object ("session", NULL, NULL,
                         SOUP_TYPE_SESSION,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
network_http_action_init (NetworkHttpAction *self)
{
  bs_icon_set_icon_name (bs_action_get_icon (BS_ACTION (self)),
                         "network-transmit-symbolic");

  self->method = METHOD_GET;
  self->source = SRC_TEXT;
  self->cancellable = g_cancellable_new ();

  self->prefs = network_http_action_prefs_new (self);
  g_object_ref_sink (self->prefs);
}

BsAction *
network_http_action_new (SoupSession *session)
{
  return g_object_new (NETWORK_TYPE_HTTP_ACTION,
                       "session", session,
                       NULL);
}

GFile *
network_http_action_get_file (NetworkHttpAction *self)
{
  g_return_val_if_fail (NETWORK_IS_HTTP_ACTION (self), NULL);

  if (self->method != METHOD_POST || self->source != SRC_FILE)
    return NULL;

  return self->file;
}

void
network_http_action_set_file (NetworkHttpAction *self,
                              GFile             *file)
{
  g_return_if_fail (NETWORK_IS_HTTP_ACTION (self));

  if (g_set_object (&self->file, file))
    maybe_save_action (self);
}

void
network_http_action_set_uri (NetworkHttpAction   *self,
                             const char          *uri)
{
  g_autofree char *stripped_uri = NULL;

  g_return_if_fail (NETWORK_IS_HTTP_ACTION (self));

  stripped_uri = g_strstrip (g_strdup (uri));

  if (g_set_str (&self->uri, stripped_uri))
    maybe_save_action (self);
}

void
network_http_action_set_method (NetworkHttpAction *self,
                                RequestMethod      method)
{
  g_return_if_fail (NETWORK_IS_HTTP_ACTION (self));

  if (self->method == method)
    return;

  self->method = method;
  maybe_save_action (self);
}

void
network_http_action_set_payload_src (NetworkHttpAction *self,
                                     PayloadSource      src)
{
  g_return_if_fail (NETWORK_IS_HTTP_ACTION (self));

  self->source = src;
  maybe_save_action (self);
}

void
network_http_action_set_payload_text (NetworkHttpAction *self,
                                      const char        *payload_text)
{
  g_return_if_fail (NETWORK_IS_HTTP_ACTION (self));

  if (g_set_str (&self->text, payload_text))
    {
      maybe_save_action (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PAYLOAD_TEXT]);
    }
}
