/*
 * Copyright (C) 2018-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "guminspectorserver.h"

#include <gum/gumprocess.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>
#include <string.h>

#define GUM_INSPECTOR_DEFAULT_PORT 9229

typedef struct _GumInspectorPeer GumInspectorPeer;

struct _GumInspectorServer
{
  GObject parent;

  guint port;

  gchar * id;
  gchar * title;
  SoupServer * server;
  GHashTable * peers;
  guint next_peer_id;
};

struct _GumInspectorPeer
{
  guint id;
  SoupWebsocketConnection * connection;

  gulong closed_handler;
  gulong message_handler;

  GumInspectorServer * server;
};

enum
{
  MESSAGE,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_PORT
};

static void gum_inspector_server_dispose (GObject * object);
static void gum_inspector_server_finalize (GObject * object);
static void gum_inspector_server_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gum_inspector_server_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);

static void gum_inspector_server_on_list (SoupServer * server,
    SoupServerMessage * msg, const char * path, GHashTable * query,
    gpointer user_data);
static void gum_inspector_server_on_version (SoupServer * server,
    SoupServerMessage * msg, const char * path, GHashTable * query,
    gpointer user_data);
static void gum_inspector_server_on_websocket_opened (SoupServer * server,
    SoupServerMessage * msg, const char * path,
    SoupWebsocketConnection * connection, gpointer user_data);
static void gum_inspector_server_emit_message (GumInspectorServer * self,
    const gchar * format, ...);

static gboolean gum_inspector_server_check_method (SoupServerMessage * msg,
    const gchar * expected_method);
static void gum_inspector_server_add_json_headers (
    SoupMessageHeaders * headers);
static void gum_inspector_server_append_json_body (SoupMessageBody * body,
    JsonBuilder * builder);

static GumInspectorPeer * gum_inspector_peer_new (GumInspectorServer * server,
    SoupWebsocketConnection * connection);
static void gum_inspector_peer_free (GumInspectorPeer * peer);
static void gum_inspector_peer_post_stanza (GumInspectorPeer * self,
    const gchar * stanza);
static void gum_inspector_peer_on_closed (GumInspectorPeer * self);
static void gum_inspector_peer_on_message (GumInspectorPeer * self, gint type,
    GBytes * message);

G_DEFINE_TYPE (GumInspectorServer, gum_inspector_server, G_TYPE_OBJECT)

static guint gum_inspector_server_signals[LAST_SIGNAL] = { 0, };

static void
gum_inspector_server_class_init (GumInspectorServerClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gum_inspector_server_dispose;
  object_class->finalize = gum_inspector_server_finalize;
  object_class->get_property = gum_inspector_server_get_property;
  object_class->set_property = gum_inspector_server_set_property;

  g_object_class_install_property (object_class, PROP_PORT,
      g_param_spec_uint ("port", "Port", "Port to listen on", 1, G_MAXUINT16,
      GUM_INSPECTOR_DEFAULT_PORT,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  gum_inspector_server_signals[MESSAGE] = g_signal_new ("message",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
gum_inspector_server_init (GumInspectorServer * self)
{
  SoupServer * server;
  gchar * ws_path;

  self->id = g_uuid_string_random ();
  self->title = g_strdup_printf ("%s[%u]",
      gum_module_get_name (gum_process_get_main_module ()),
      gum_process_get_id ());

  server = g_object_new (SOUP_TYPE_SERVER, NULL);

  soup_server_add_handler (server, "/json",
      gum_inspector_server_on_list, self, NULL);
  soup_server_add_handler (server, "/json/list",
      gum_inspector_server_on_list, self, NULL);
  soup_server_add_handler (server, "/json/version",
      gum_inspector_server_on_version, self, NULL);

  ws_path = g_strconcat ("/", self->id, NULL);
  soup_server_add_websocket_handler (server, ws_path, NULL, NULL,
      gum_inspector_server_on_websocket_opened, self, NULL);
  g_free (ws_path);

  self->server = server;

  self->peers = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) gum_inspector_peer_free);
  self->next_peer_id = 1;
}

static void
gum_inspector_server_dispose (GObject * object)
{
  GumInspectorServer * self = GUM_INSPECTOR_SERVER (object);

  g_clear_pointer (&self->peers, g_hash_table_unref);

  if (self->server != NULL)
    soup_server_disconnect (self->server);

  g_clear_object (&self->server);

  G_OBJECT_CLASS (gum_inspector_server_parent_class)->dispose (object);
}

static void
gum_inspector_server_finalize (GObject * object)
{
  GumInspectorServer * self = GUM_INSPECTOR_SERVER (object);

  g_free (self->id);
  g_free (self->title);

  G_OBJECT_CLASS (gum_inspector_server_parent_class)->finalize (object);
}

static void
gum_inspector_server_get_property (GObject * object,
                                   guint property_id,
                                   GValue * value,
                                   GParamSpec * pspec)
{
  GumInspectorServer * self = GUM_INSPECTOR_SERVER (object);

  switch (property_id)
  {
    case PROP_PORT:
      g_value_set_uint (value, self->port);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
gum_inspector_server_set_property (GObject * object,
                                   guint property_id,
                                   const GValue * value,
                                   GParamSpec * pspec)
{
  GumInspectorServer * self = GUM_INSPECTOR_SERVER (object);

  switch (property_id)
  {
    case PROP_PORT:
      self->port = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

GumInspectorServer *
gum_inspector_server_new (void)
{
  return g_object_new (GUM_TYPE_INSPECTOR_SERVER, NULL);
}

GumInspectorServer *
gum_inspector_server_new_with_port (guint port)
{
  return g_object_new (GUM_TYPE_INSPECTOR_SERVER,
      "port", port,
      NULL);
}

gboolean
gum_inspector_server_start (GumInspectorServer * self,
                            GError ** error)
{
  GError * listen_error = NULL;

  if (!soup_server_listen_local (self->server, self->port, 0, &listen_error))
    goto listen_failed;

  return TRUE;

listen_failed:
  {
    g_set_error (error,
        GUM_ERROR,
        GUM_ERROR_FAILED,
        "%s",
        listen_error->message);

    g_error_free (listen_error);

    return FALSE;
  }
}

void
gum_inspector_server_stop (GumInspectorServer * self)
{
  soup_server_disconnect (self->server);
}

void
gum_inspector_server_post_message (GumInspectorServer * self,
                                   const gchar * message)
{
  const gchar * id_start, * id_end;
  guint id;
  GumInspectorPeer * peer;

  id_start = strchr (message, ' ');
  if (id_start == NULL)
    return;
  id_start++;

  id = (guint) g_ascii_strtoull (id_start, (gchar **) &id_end, 10);
  if (id_end == id_start)
    return;

  peer = g_hash_table_lookup (self->peers, GUINT_TO_POINTER (id));
  if (peer == NULL)
    return;

  if (g_str_has_prefix (message, "DISPATCH "))
  {
    const gchar * stanza;

    if (*id_end != ' ')
      return;
    stanza = id_end + 1;

    gum_inspector_peer_post_stanza (peer, stanza);
  }
}

static void
gum_inspector_server_on_list (SoupServer * server,
                              SoupServerMessage * msg,
                              const char * path,
                              GHashTable * query,
                              gpointer user_data)
{
  GumInspectorServer * self = user_data;
  JsonBuilder * builder;
  gchar * host_port;
  GSList * uris, * cur;
  gchar * url;

  if (!gum_inspector_server_check_method (msg, "GET"))
    return;

  soup_server_message_set_status (msg, SOUP_STATUS_OK, NULL);

  gum_inspector_server_add_json_headers (
      soup_server_message_get_response_headers (msg));

  builder = json_builder_new ();

  json_builder_begin_array (builder);

  json_builder_begin_object (builder);

  json_builder_set_member_name (builder, "id");
  json_builder_add_string_value (builder, self->id);

  json_builder_set_member_name (builder, "title");
  json_builder_add_string_value (builder, self->title);

  json_builder_set_member_name (builder, "description");
  json_builder_add_string_value (builder, "Frida Agent");

  json_builder_set_member_name (builder, "url");
  json_builder_add_string_value (builder, "file://");

  json_builder_set_member_name (builder, "faviconUrl");
  json_builder_add_string_value (builder, "https://frida.re/favicon.ico");

  json_builder_set_member_name (builder, "type");
  json_builder_add_string_value (builder, "node");

  host_port = NULL;
  uris = soup_server_get_uris (self->server);
  for (cur = uris; cur != NULL; cur = cur->next)
  {
    GUri * uri = cur->data;

    host_port = g_strdup_printf ("%s:%d",
        g_uri_get_host (uri),
        g_uri_get_port (uri));
    break;
  }
  g_slist_free_full (uris, (GDestroyNotify) g_uri_unref);

  json_builder_set_member_name (builder, "devtoolsFrontendUrl");
  url = g_strdup_printf ("devtools://devtools/bundled/js_app.html"
      "?experiments=true&v8only=true&ws=%s/%s", host_port, self->id);
  json_builder_add_string_value (builder, url);
  g_free (url);

  json_builder_set_member_name (builder, "devtoolsFrontendUrlCompat");
  url = g_strdup_printf ("devtools://devtools/bundled/inspector.html"
      "?experiments=true&v8only=true&ws=%s/%s", host_port, self->id);
  json_builder_add_string_value (builder, url);
  g_free (url);

  json_builder_set_member_name (builder, "webSocketDebuggerUrl");
  url = g_strdup_printf ("ws://%s/%s", host_port, self->id);
  json_builder_add_string_value (builder, url);
  g_free (url);

  g_free (host_port);

  json_builder_end_object (builder);

  json_builder_end_array (builder);

  gum_inspector_server_append_json_body (
      soup_server_message_get_response_body (msg), builder);
}

static void
gum_inspector_server_on_version (SoupServer * server,
                                 SoupServerMessage * msg,
                                 const char * path,
                                 GHashTable * query,
                                 gpointer user_data)
{
  JsonBuilder * builder;

  if (!gum_inspector_server_check_method (msg, "GET"))
    return;

  soup_server_message_set_status (msg, SOUP_STATUS_OK, NULL);

  gum_inspector_server_add_json_headers (
      soup_server_message_get_response_headers (msg));

  builder = json_builder_new ();

  json_builder_begin_object (builder);

  json_builder_set_member_name (builder, "Browser");
  json_builder_add_string_value (builder, "Frida/v" FRIDA_VERSION);

  json_builder_set_member_name (builder, "Protocol-Version");
  json_builder_add_string_value (builder, "1.1");

  json_builder_end_object (builder);

  gum_inspector_server_append_json_body (
      soup_server_message_get_response_body (msg), builder);
}

static void
gum_inspector_server_on_websocket_opened (SoupServer * server,
                                          SoupServerMessage * msg,
                                          const char * path,
                                          SoupWebsocketConnection * connection,
                                          gpointer user_data)
{
  GumInspectorServer * self = user_data;
  GumInspectorPeer * peer;

  peer = gum_inspector_peer_new (self, connection);
  g_hash_table_insert (self->peers, GUINT_TO_POINTER (peer->id), peer);

  gum_inspector_server_emit_message (self, "CONNECT %u", peer->id);
}

static void
gum_inspector_server_on_websocket_closed (GumInspectorServer * self,
                                          GumInspectorPeer * peer)
{
  gum_inspector_server_emit_message (self, "DISCONNECT %u", peer->id);

  g_hash_table_remove (self->peers, GUINT_TO_POINTER (peer->id));
}

static void
gum_inspector_server_on_websocket_stanza (GumInspectorServer * self,
                                          GumInspectorPeer * peer,
                                          const gchar * stanza)
{
  gum_inspector_server_emit_message (self, "DISPATCH %u %s", peer->id, stanza);
}

static void
gum_inspector_server_emit_message (GumInspectorServer * self,
                                   const gchar * format,
                                   ...)
{
  va_list args;
  gchar * message;

  va_start (args, format);
  message = g_strdup_vprintf (format, args);
  va_end (args);

  g_signal_emit (self, gum_inspector_server_signals[MESSAGE], 0, message);

  g_free (message);
}

static gboolean
gum_inspector_server_check_method (SoupServerMessage * msg,
                                   const gchar * expected_method)
{
  if (strcmp (soup_server_message_get_method (msg), expected_method) != 0)
  {
    soup_server_message_set_status (msg, SOUP_STATUS_METHOD_NOT_ALLOWED, NULL);
    return FALSE;
  }

  return TRUE;
}

static void
gum_inspector_server_add_json_headers (SoupMessageHeaders * headers)
{
  GHashTable * content_params;

  content_params = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (content_params, "charset", "UTF-8");
  soup_message_headers_set_content_type (headers,
      "application/json", content_params);
  g_hash_table_unref (content_params);

  soup_message_headers_replace (headers, "Cache-Control", "no-cache");
}

static void
gum_inspector_server_append_json_body (SoupMessageBody * body,
                                       JsonBuilder * builder)
{
  JsonNode * root;
  gchar * json;

  root = json_builder_get_root (builder);
  json = json_to_string (root, FALSE);
  soup_message_body_append_take (body, (guchar *) json, strlen (json));
  json_node_unref (root);

  g_object_unref (builder);
}

static GumInspectorPeer *
gum_inspector_peer_new (GumInspectorServer * server,
                        SoupWebsocketConnection * connection)
{
  GumInspectorPeer * peer;

  peer = g_slice_new (GumInspectorPeer);
  peer->id = server->next_peer_id++;
  peer->connection = g_object_ref (connection);

  peer->closed_handler = g_signal_connect_swapped (connection, "closed",
      G_CALLBACK (gum_inspector_peer_on_closed), peer);
  peer->message_handler = g_signal_connect_swapped (connection, "message",
      G_CALLBACK (gum_inspector_peer_on_message), peer);

  peer->server = server;

  return peer;
}

static void
gum_inspector_peer_free (GumInspectorPeer * peer)
{
  SoupWebsocketConnection * connection = peer->connection;

  g_signal_handler_disconnect (connection, peer->closed_handler);
  g_signal_handler_disconnect (connection, peer->message_handler);
  g_object_unref (connection);

  g_slice_free (GumInspectorPeer, peer);
}

static void
gum_inspector_peer_post_stanza (GumInspectorPeer * self,
                                const gchar * stanza)
{
  soup_websocket_connection_send_text (self->connection, stanza);
}

static void
gum_inspector_peer_on_closed (GumInspectorPeer * self)
{
  gum_inspector_server_on_websocket_closed (self->server, self);
}

static void
gum_inspector_peer_on_message (GumInspectorPeer * self,
                               gint type,
                               GBytes * message)
{
  if (type == SOUP_WEBSOCKET_DATA_TEXT)
  {
    gum_inspector_server_on_websocket_stanza (self->server, self,
        g_bytes_get_data (message, NULL));
  }
}
