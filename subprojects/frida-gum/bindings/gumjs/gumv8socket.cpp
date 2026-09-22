/*
 * Copyright (C) 2010-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumv8socket.h"

#include "gumv8macros.h"
#include "gumv8scope.h"
#include "gumv8script-priv.h"

#include <gio/gnetworking.h>
#ifdef HAVE_WINDOWS
# define GUM_SOCKOPT_OPTVAL(v) ((char *) (v))
  typedef int gum_socklen_t;
#else
# include <errno.h>
# define GUM_SOCKOPT_OPTVAL(v) (v)
  typedef socklen_t gum_socklen_t;
#endif
#ifdef G_OS_UNIX
# include <gio/gunixsocketaddress.h>
#endif

#define GUMJS_MODULE_NAME Socket

using namespace v8;

struct GumV8ListenOperation : public GumV8ModuleOperation<GumV8Socket>
{
  guint16 port;

  gchar * path;

  GSocketAddress * address;
  gint backlog;
};

struct GumV8ConnectOperation : public GumV8ModuleOperation<GumV8Socket>
{
  GSocketClient * client;
  GSocketFamily family;

  gchar * host;
  guint16 port;

  GSocketConnectable * connectable;

  gboolean tls;
};

struct GumV8CloseListenerOperation
    : public GumV8ObjectOperation<GSocketListener, GumV8Socket>
{
};

struct GumV8AcceptOperation
    : public GumV8ObjectOperation<GSocketListener, GumV8Socket>
{
};

struct GumV8SetNoDelayOperation
    : public GumV8ObjectOperation<GSocketConnection, GumV8Stream>
{
  gboolean no_delay;
};

GUMJS_DECLARE_FUNCTION (gumjs_socket_listen)
static void gum_v8_listen_operation_dispose (GumV8ListenOperation * self);
static void gum_v8_listen_operation_perform (GumV8ListenOperation * self);
GUMJS_DECLARE_FUNCTION (gumjs_socket_connect)
static void gum_v8_connect_operation_dispose (GumV8ConnectOperation * self);
static void gum_v8_connect_operation_start (GumV8ConnectOperation * self);
static void gum_v8_connect_operation_finish (GSocketClient * client,
    GAsyncResult * result, GumV8ConnectOperation * self);
GUMJS_DECLARE_FUNCTION (gumjs_socket_get_type)
GUMJS_DECLARE_FUNCTION (gumjs_socket_get_local_address)
GUMJS_DECLARE_FUNCTION (gumjs_socket_get_peer_address)

static Local<Object> gum_v8_socket_listener_new (GSocketListener * listener,
    GumV8Socket * module);
GUMJS_DECLARE_CONSTRUCTOR (gumjs_socket_listener_construct)
GUMJS_DECLARE_FUNCTION (gumjs_socket_listener_close)
static void gum_v8_close_listener_operation_perform (
    GumV8CloseListenerOperation * self);
GUMJS_DECLARE_FUNCTION (gumjs_socket_listener_accept)
static void gum_v8_accept_operation_start (GumV8AcceptOperation * self);
static void gum_v8_accept_operation_finish (GSocketListener * listener,
    GAsyncResult * result, GumV8AcceptOperation * self);

static Local<Object> gum_v8_socket_connection_new (
    GSocketConnection * connection, GumV8Socket * module);
GUMJS_DECLARE_CONSTRUCTOR (gumjs_socket_connection_construct)
GUMJS_DECLARE_FUNCTION (gumjs_socket_connection_set_no_delay)
static void gum_v8_set_no_delay_operation_perform (
    GumV8SetNoDelayOperation * self);

static gboolean gum_v8_socket_family_get (Local<Value> value,
    GSocketFamily * family, GumV8Core * core);
static gboolean gum_v8_unix_socket_address_type_get (Local<Value> value,
    GUnixSocketAddressType * type, GumV8Core * core);

static Local<Value> gum_v8_socket_address_to_value (
    struct sockaddr * addr, GumV8Core * core);

static const GumV8Function gumjs_socket_functions[] =
{
  { "_listen", gumjs_socket_listen },
  { "_connect", gumjs_socket_connect },
  { "type", gumjs_socket_get_type },
  { "localAddress", gumjs_socket_get_local_address },
  { "peerAddress", gumjs_socket_get_peer_address },

  { NULL, NULL }
};

static const GumV8Function gumjs_socket_listener_functions[] =
{
  { "_close", gumjs_socket_listener_close },
  { "_accept", gumjs_socket_listener_accept },

  { NULL, NULL }
};

static const GumV8Function gumjs_socket_connection_functions[] =
{
  { "_setNoDelay", gumjs_socket_connection_set_no_delay },

  { NULL, NULL }
};

void
_gum_v8_socket_init (GumV8Socket * self,
                     GumV8Core * core,
                     Local<ObjectTemplate> scope)
{
  auto isolate = core->isolate;

  self->core = core;

  auto module = External::New (isolate, self);

  auto socket = _gum_v8_create_module ("Socket", scope, isolate);
  _gum_v8_module_add (module, socket, gumjs_socket_functions, isolate);

  auto listener = _gum_v8_create_class ("SocketListener",
      gumjs_socket_listener_construct, scope, module, isolate);
  _gum_v8_class_add (listener, gumjs_socket_listener_functions, module,
      isolate);
  self->listener = new Global<FunctionTemplate> (isolate, listener);

  auto connection = _gum_v8_create_class ("SocketConnection",
      gumjs_socket_connection_construct, scope, module, isolate);
  auto io_stream (Local<FunctionTemplate>::New (isolate,
      *core->script->stream.io_stream));
  connection->Inherit (io_stream);
  _gum_v8_class_add (connection, gumjs_socket_connection_functions, module,
      isolate);
  self->connection = new Global<FunctionTemplate> (isolate, connection);
}

void
_gum_v8_socket_realize (GumV8Socket * self)
{
  gum_v8_object_manager_init (&self->objects);
}

void
_gum_v8_socket_flush (GumV8Socket * self)
{
  gum_v8_object_manager_flush (&self->objects);
}

void
_gum_v8_socket_dispose (GumV8Socket * self)
{
  gum_v8_object_manager_free (&self->objects);
}

void
_gum_v8_socket_finalize (GumV8Socket * self)
{
  delete self->listener;
  delete self->connection;
  self->listener = nullptr;
  self->connection = nullptr;
}

GUMJS_DEFINE_FUNCTION (gumjs_socket_listen)
{
  Local<Value> family_value;
  gchar * host;
  guint port;
  Local<Value> type_value;
  gchar * path;
  guint backlog;
  Local<Function> callback;
  if (!_gum_v8_args_parse (args, "Vs?uVs?uF", &family_value, &host, &port,
      &type_value, &path, &backlog, &callback))
    return;

  GSocketFamily family;
  GUnixSocketAddressType type;
  if (!gum_v8_socket_family_get (family_value, &family, core) ||
      !gum_v8_unix_socket_address_type_get (type_value, &type, core))
  {
    g_free (host);
    g_free (path);
    return;
  }

  GSocketAddress * address = NULL;
  if (host != NULL)
  {
    address = g_inet_socket_address_new_from_string (host, port);
    g_clear_pointer (&host, g_free);
    if (address == NULL)
    {
      g_free (path);
      _gum_v8_throw_ascii_literal (isolate, "invalid host");
      return;
    }
  }
  else if (path != NULL)
  {
#ifdef G_OS_UNIX
    address = g_unix_socket_address_new_with_type (path, -1, type);
    g_assert (address != NULL);
#else
    g_free (path);
    _gum_v8_throw_ascii_literal (isolate, "UNIX sockets not available");
    return;
#endif
  }
  else if (family != G_SOCKET_FAMILY_INVALID)
  {
    address = g_inet_socket_address_new_from_string (
        (family == G_SOCKET_FAMILY_IPV4) ? "0.0.0.0" : "::",
        port);
    g_assert (address != NULL);
  }

  auto op = gum_v8_module_operation_new (module, callback,
      gum_v8_listen_operation_perform, gum_v8_listen_operation_dispose);
  op->port = port;
  op->path = path;
  op->address = address;
  op->backlog = backlog;
  gum_v8_module_operation_schedule (op);
}

static void
gum_v8_listen_operation_dispose (GumV8ListenOperation * self)
{
  g_clear_object (&self->address);
  g_free (self->path);
}

static void
gum_v8_listen_operation_perform (GumV8ListenOperation * self)
{
  auto listener = G_SOCKET_LISTENER (g_object_new (G_TYPE_SOCKET_LISTENER,
      "listen-backlog", self->backlog,
      NULL));

  GSocketAddress * effective_address = NULL;
  GError * error = NULL;
  if (self->address != NULL)
  {
    g_socket_listener_add_address (listener, self->address,
        G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_DEFAULT, NULL,
        &effective_address, &error);
  }
  else
  {
    if (self->port != 0)
    {
      g_socket_listener_add_inet_port (listener, self->port, NULL, &error);
    }
    else
    {
      self->port =
          g_socket_listener_add_any_inet_port (listener, NULL, &error);
    }
  }

  if (error != NULL)
    g_clear_object (&listener);

  {
    auto core = self->core;
    ScriptScope scope (core->script);
    auto isolate = core->isolate;
    auto context = isolate->GetCurrentContext ();

    Local<Value> error_value;
    Local<Value> listener_value;
    if (error == NULL)
    {
      error_value = Null (isolate);
      listener_value = gum_v8_socket_listener_new (listener, self->module);

      auto listener_object = listener_value.As<Object> ();
      if (self->path != NULL)
      {
        _gum_v8_object_set_utf8 (listener_object, "path", self->path, core);
      }
      else
      {
        if (effective_address != NULL)
        {
          self->port = g_inet_socket_address_get_port (
              G_INET_SOCKET_ADDRESS (effective_address));
          g_clear_object (&effective_address);
        }

        _gum_v8_object_set_uint (listener_object, "port", self->port, core);
      }
    }
    else
    {
      error_value = _gum_v8_error_new_take_error (isolate, &error);
      listener_value = Null (isolate);
    }

    Local<Value> argv[] = { error_value, listener_value };
    auto callback (Local<Function>::New (isolate, *self->callback));
    auto recv = Undefined (isolate);
    auto result = callback->Call (context, recv, G_N_ELEMENTS (argv), argv);
    _gum_v8_ignore_result (result);
  }

  gum_v8_module_operation_finish (self);
}

GUMJS_DEFINE_FUNCTION (gumjs_socket_connect)
{
  Local<Value> family_value;
  gchar * host;
  guint port;
  Local<Value> type_value;
  gchar * path;
  gboolean tls;
  Local<Function> callback;
  if (!_gum_v8_args_parse (args, "Vs?uVs?tF", &family_value, &host, &port,
      &type_value, &path, &tls, &callback))
    return;

  GSocketFamily family;
  GUnixSocketAddressType type;
  if (!gum_v8_socket_family_get (family_value, &family, core) ||
      !gum_v8_unix_socket_address_type_get (type_value, &type, core))
  {
    g_free (host);
    g_free (path);
    return;
  }

  GSocketConnectable * connectable = NULL;
  if (path != NULL)
  {
#ifdef G_OS_UNIX
    family = G_SOCKET_FAMILY_UNIX;
    connectable = G_SOCKET_CONNECTABLE (g_unix_socket_address_new_with_type (
        path, -1, type));
    g_assert (connectable != NULL);
    g_clear_pointer (&path, g_free);
#else
    g_free (host);
    g_free (path);
    _gum_v8_throw_ascii_literal (isolate, "UNIX sockets not available");
    return;
#endif
  }

  auto op = gum_v8_module_operation_new (module, callback,
      gum_v8_connect_operation_start, gum_v8_connect_operation_dispose);
  op->client = NULL;
  op->family = family;
  op->host = host;
  op->port = port;
  op->connectable = connectable;
  op->tls = tls;
  gum_v8_module_operation_schedule (op);
}

static void
gum_v8_connect_operation_dispose (GumV8ConnectOperation * self)
{
  g_clear_object (&self->connectable);
  g_free (self->host);
  g_object_unref (self->client);
}

static void
gum_v8_connect_operation_start (GumV8ConnectOperation * self)
{
  self->client = G_SOCKET_CLIENT (g_object_new (G_TYPE_SOCKET_CLIENT,
      "family", self->family,
      "tls", self->tls,
      NULL));

  if (self->connectable != NULL)
  {
    g_socket_client_connect_async (self->client, self->connectable,
        self->cancellable,
        (GAsyncReadyCallback) gum_v8_connect_operation_finish, self);
  }
  else
  {
    g_socket_client_connect_to_host_async (self->client, self->host, self->port,
        self->cancellable,
        (GAsyncReadyCallback) gum_v8_connect_operation_finish, self);
  }
}

static void
gum_v8_connect_operation_finish (GSocketClient * client,
                                 GAsyncResult * result,
                                 GumV8ConnectOperation * self)
{
  GSocketConnection * connection;
  GError * error = NULL;
  if (self->connectable != NULL)
  {
    connection = g_socket_client_connect_finish (client, result, &error);
  }
  else
  {
    connection = g_socket_client_connect_to_host_finish (client, result,
        &error);
  }

  {
    auto core = self->core;
    ScriptScope scope (core->script);
    auto isolate = core->isolate;
    auto context = isolate->GetCurrentContext ();

    Local<Value> error_value;
    Local<Value> connection_value;
    if (error == NULL)
    {
      error_value = Null (isolate);
      connection_value =
          gum_v8_socket_connection_new (connection, self->module);
    }
    else
    {
      error_value = _gum_v8_error_new_take_error (isolate, &error);
      connection_value = Null (isolate);
    }

    Local<Value> argv[] = { error_value, connection_value };
    auto callback (Local<Function>::New (isolate, *self->callback));
    auto recv = Undefined (isolate);
    auto res = callback->Call (context, recv, G_N_ELEMENTS (argv), argv);
    _gum_v8_ignore_result (res);
  }

  gum_v8_module_operation_finish (self);
}

GUMJS_DEFINE_FUNCTION (gumjs_socket_get_type)
{
  gint handle;
  if (!_gum_v8_args_parse (args, "i", &handle))
    return;

  const gchar * res = NULL;
  int type;
  gum_socklen_t len = sizeof (int);
  if (getsockopt (handle, SOL_SOCKET, SO_TYPE, GUM_SOCKOPT_OPTVAL (&type),
      &len) == 0)
  {
    int family;

    struct sockaddr_in6 addr;
    len = sizeof (addr);
    if (getsockname (handle, (struct sockaddr *) &addr, &len) == 0)
    {
      family = addr.sin6_family;
    }
    else
    {
      struct sockaddr_in invalid_sockaddr;
      invalid_sockaddr.sin_family = AF_INET;
      invalid_sockaddr.sin_port = GUINT16_TO_BE (0);
      invalid_sockaddr.sin_addr.s_addr = GUINT32_TO_BE (0xffffffff);
      bind (handle, (struct sockaddr *) &invalid_sockaddr,
          sizeof (invalid_sockaddr));
#ifdef HAVE_WINDOWS
      family = (WSAGetLastError () == WSAEADDRNOTAVAIL) ? AF_INET : AF_INET6;
#else
      family = (errno == EADDRNOTAVAIL) ? AF_INET : AF_INET6;
#endif
    }

    switch (family)
    {
      case AF_INET:
        switch (type)
        {
          case SOCK_STREAM: res = "tcp"; break;
          case  SOCK_DGRAM: res = "udp"; break;
        }
        break;
      case AF_INET6:
        switch (type)
        {
          case SOCK_STREAM: res = "tcp6"; break;
          case  SOCK_DGRAM: res = "udp6"; break;
        }
        break;
#ifndef HAVE_WINDOWS
      case AF_UNIX:
        switch (type)
        {
          case SOCK_STREAM: res = "unix:stream"; break;
          case  SOCK_DGRAM: res = "unix:dgram";  break;
        }
        break;
#endif
    }
  }

  if (res != NULL)
  {
    info.GetReturnValue ().Set (String::NewFromUtf8 (isolate, res)
        .ToLocalChecked ());
  }
  else
  {
    info.GetReturnValue ().SetNull ();
  }
}

GUMJS_DEFINE_FUNCTION (gumjs_socket_get_local_address)
{
  gint handle;
  if (!_gum_v8_args_parse (args, "i", &handle))
    return;

  struct sockaddr_in6 large_addr;
  auto addr = (struct sockaddr *) &large_addr;
  gum_socklen_t len = sizeof (large_addr);
  if (getsockname (handle, addr, &len) == 0)
    info.GetReturnValue ().Set (gum_v8_socket_address_to_value (addr, core));
  else
    info.GetReturnValue ().SetNull ();
}

GUMJS_DEFINE_FUNCTION (gumjs_socket_get_peer_address)
{
  gint handle;
  if (!_gum_v8_args_parse (args, "i", &handle))
    return;

  struct sockaddr_in6 large_addr;
  auto addr = (struct sockaddr *) (&large_addr);
  gum_socklen_t len = sizeof (large_addr);
  if (getpeername (handle, addr, &len) == 0)
    info.GetReturnValue ().Set (gum_v8_socket_address_to_value (addr, core));
  else
    info.GetReturnValue ().SetNull ();
}

static Local<Object>
gum_v8_socket_listener_new (GSocketListener * listener,
                            GumV8Socket * module)
{
  auto isolate = module->core->isolate;
  auto context = isolate->GetCurrentContext ();

  auto ctor (Local<FunctionTemplate>::New (isolate, *module->listener));
  Local<Value> argv[] = { External::New (isolate, listener) };
  return ctor->GetFunction (context).ToLocalChecked ()
      ->NewInstance (context, G_N_ELEMENTS (argv), argv).ToLocalChecked ();
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_socket_listener_construct)
{
  GSocketListener * listener;
  if (!_gum_v8_args_parse (args, "X", &listener))
    return;

  gum_v8_object_manager_add (&module->objects, wrapper, listener, module);
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_socket_listener_close, GumV8SocketListener)
{
  Local<Function> callback;
  if (!_gum_v8_args_parse (args, "F", &callback))
    return;

  g_cancellable_cancel (self->cancellable);

  auto op = gum_v8_object_operation_new (self, callback,
      gum_v8_close_listener_operation_perform);
  gum_v8_object_operation_schedule_when_idle (op);
}

static void
gum_v8_close_listener_operation_perform (GumV8CloseListenerOperation * self)
{
  g_socket_listener_close (self->object->handle);

  {
    auto core = self->core;
    ScriptScope scope (core->script);
    auto isolate = core->isolate;
    auto context = isolate->GetCurrentContext ();

    auto callback (Local<Function>::New (isolate, *self->callback));
    auto recv = Undefined (isolate);
    auto result = callback->Call (context, recv, 0, nullptr);
    _gum_v8_ignore_result (result);
  }

  gum_v8_object_operation_finish (self);
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_socket_listener_accept, GumV8SocketListener)
{
  Local<Function> callback;
  if (!_gum_v8_args_parse (args, "F", &callback))
    return;

  auto op = gum_v8_object_operation_new (self, callback,
      gum_v8_accept_operation_start);
  gum_v8_object_operation_schedule (op);
}

static void
gum_v8_accept_operation_start (GumV8AcceptOperation * self)
{
  auto listener = self->object;

  g_socket_listener_accept_async (listener->handle, listener->cancellable,
      (GAsyncReadyCallback) gum_v8_accept_operation_finish, self);
}

static void
gum_v8_accept_operation_finish (GSocketListener * listener,
                                GAsyncResult * result,
                                GumV8AcceptOperation * self)
{
  GError * error = NULL;
  GSocketConnection * connection;

  connection = g_socket_listener_accept_finish (listener, result, NULL, &error);

  {
    auto core = self->core;
    ScriptScope scope (core->script);
    auto isolate = core->isolate;
    auto context = isolate->GetCurrentContext ();

    Local<Value> error_value;
    Local<Value> connection_value;
    if (error == NULL)
    {
      error_value = Null (isolate);
      connection_value =
          gum_v8_socket_connection_new (connection, self->object->module);
    }
    else
    {
      error_value = _gum_v8_error_new_take_error (isolate, &error);
      connection_value = Null (isolate);
    }

    Local<Value> argv[] = { error_value, connection_value };
    auto callback (Local<Function>::New (isolate, *self->callback));
    auto recv = Undefined (isolate);
    auto res = callback->Call (context, recv, G_N_ELEMENTS (argv), argv);
    _gum_v8_ignore_result (res);
  }

  gum_v8_object_operation_finish (self);
}

static Local<Object>
gum_v8_socket_connection_new (GSocketConnection * connection,
                              GumV8Socket * module)
{
  auto isolate = module->core->isolate;
  auto context = isolate->GetCurrentContext ();

  Local<FunctionTemplate> ctor (
      Local<FunctionTemplate>::New (isolate, *module->connection));
  Local<Value> argv[] = { External::New (isolate, connection) };
  return ctor->GetFunction (context).ToLocalChecked ()
      ->NewInstance (context, G_N_ELEMENTS (argv), argv).ToLocalChecked ();
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_socket_connection_construct)
{
  auto context = isolate->GetCurrentContext ();

  GSocketConnection * connection;
  if (!_gum_v8_args_parse (args, "X", &connection))
    return;

  auto base_ctor (Local<FunctionTemplate>::New (isolate,
      *core->script->stream.io_stream));
  Local<Value> argv[] = { External::New (isolate, connection) };
  base_ctor->GetFunction (context).ToLocalChecked ()
      ->Call (context, wrapper, G_N_ELEMENTS (argv), argv).ToLocalChecked ();
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_socket_connection_set_no_delay, GumV8IOStream)
{
  gboolean no_delay;
  Local<Function> callback;
  if (!_gum_v8_args_parse (args, "tF", &no_delay, &callback))
    return;

  auto op = gum_v8_object_operation_new (self, callback,
      gum_v8_set_no_delay_operation_perform);
  op->no_delay = no_delay;
  gum_v8_object_operation_schedule (op);
}

static void
gum_v8_set_no_delay_operation_perform (GumV8SetNoDelayOperation * self)
{
  GSocket * socket = g_socket_connection_get_socket (self->object->handle);

  GError * error = NULL;
  gboolean success = g_socket_set_option (socket, IPPROTO_TCP, TCP_NODELAY,
      self->no_delay, &error);

  {
    auto core = self->core;
    ScriptScope scope (core->script);
    auto isolate = core->isolate;
    auto context = isolate->GetCurrentContext ();

    Local<Value> error_value = _gum_v8_error_new_take_error (isolate, &error);
    auto success_value = success ? True (isolate) : False (isolate);

    Local<Value> argv[] = { error_value, success_value };
    auto callback (Local<Function>::New (isolate, *self->callback));
    auto recv = Undefined (isolate);
    auto result = callback->Call (context, recv, G_N_ELEMENTS (argv), argv);
    _gum_v8_ignore_result (result);
  }

  gum_v8_object_operation_finish (self);
}

static gboolean
gum_v8_socket_family_get (Local<Value> value,
                          GSocketFamily * family,
                          GumV8Core * core)
{
  auto isolate = core->isolate;

  if (value->IsNull ())
  {
    *family = G_SOCKET_FAMILY_INVALID;
    return TRUE;
  }

  if (!value->IsString ())
  {
    _gum_v8_throw_ascii_literal (isolate, "invalid socket address family");
    return FALSE;
  }
  String::Utf8Value value_utf8 (isolate, value);
  auto value_str = *value_utf8;

  if (strcmp (value_str, "unix") == 0)
  {
    *family = G_SOCKET_FAMILY_UNIX;
    return TRUE;
  }

  if (strcmp (value_str, "ipv4") == 0)
  {
    *family = G_SOCKET_FAMILY_IPV4;
    return TRUE;
  }

  if (strcmp (value_str, "ipv6") == 0)
  {
    *family = G_SOCKET_FAMILY_IPV6;
    return TRUE;
  }

  _gum_v8_throw_ascii_literal (isolate, "invalid socket address family");
  return FALSE;
}

static gboolean
gum_v8_unix_socket_address_type_get (Local<Value> value,
                                     GUnixSocketAddressType * type,
                                     GumV8Core * core)
{
  auto isolate = core->isolate;

  if (value->IsNull ())
  {
    *type = G_UNIX_SOCKET_ADDRESS_PATH;
    return TRUE;
  }

  if (!value->IsString ())
  {
    _gum_v8_throw_ascii_literal (isolate, "invalid UNIX socket address type");
    return FALSE;
  }
  String::Utf8Value value_utf8 (isolate, value);
  auto value_str = *value_utf8;

  if (strcmp (value_str, "anonymous") == 0)
  {
    *type = G_UNIX_SOCKET_ADDRESS_ANONYMOUS;
    return TRUE;
  }

  if (strcmp (value_str, "path") == 0)
  {
    *type = G_UNIX_SOCKET_ADDRESS_PATH;
    return TRUE;
  }

  if (strcmp (value_str, "abstract") == 0)
  {
    *type = G_UNIX_SOCKET_ADDRESS_ABSTRACT;
    return TRUE;
  }

  if (strcmp (value_str, "abstract-padded") == 0)
  {
    *type = G_UNIX_SOCKET_ADDRESS_ABSTRACT_PADDED;
    return TRUE;
  }

  _gum_v8_throw_ascii_literal (isolate, "invalid UNIX socket address type");
  return FALSE;
}

static Local<Value>
gum_v8_socket_address_to_value (struct sockaddr * addr,
                                GumV8Core * core)
{
  auto isolate = core->isolate;

  switch (addr->sa_family)
  {
    case AF_INET:
    {
      auto inet_addr = (struct sockaddr_in *) addr;
#ifdef HAVE_WINDOWS
      gunichar2 ip_utf16[15 + 1 + 5 + 1];
      gchar ip[15 + 1 + 5 + 1];
      DWORD len = G_N_ELEMENTS (ip_utf16);
      WSAAddressToStringW (addr, sizeof (struct sockaddr_in), NULL,
          (LPWSTR) ip_utf16, &len);
      WideCharToMultiByte (CP_UTF8, 0, (LPWSTR) ip_utf16, -1, ip, len, NULL,
          NULL);
      gchar * p = strchr (ip, ':');
      if (p != NULL)
        *p = '\0';
#else
      gchar ip[INET_ADDRSTRLEN];
      inet_ntop (AF_INET, &inet_addr->sin_addr, ip, sizeof (ip));
#endif
      Local<Object> result (Object::New (isolate));
      _gum_v8_object_set_ascii (result, "ip", ip, core);
      _gum_v8_object_set_uint (result, "port",
          GUINT16_FROM_BE (inet_addr->sin_port), core);
      return result;
    }
    case AF_INET6:
    {
      auto inet_addr = (struct sockaddr_in6 *) addr;
#ifdef HAVE_WINDOWS
      gunichar2 ip_utf16[45 + 1 + 5 + 1];
      gchar ip[45 + 1 + 5 + 1];
      DWORD len = G_N_ELEMENTS (ip_utf16);
      WSAAddressToStringW (addr, sizeof (struct sockaddr_in6), NULL,
          (LPWSTR) ip_utf16, &len);
      WideCharToMultiByte (CP_UTF8, 0, (LPWSTR) ip_utf16, -1, ip, len, NULL,
          NULL);
      gchar * p = strrchr (ip, ':');
      if (p != NULL)
        *p = '\0';
#else
      gchar ip[INET6_ADDRSTRLEN];
      inet_ntop (AF_INET6, &inet_addr->sin6_addr, ip, sizeof (ip));
#endif
      auto result (Object::New (isolate));
      _gum_v8_object_set_ascii (result, "ip", ip, core);
      _gum_v8_object_set_uint (result, "port",
          GUINT16_FROM_BE (inet_addr->sin6_port), core);
      return result;
    }
    case AF_UNIX:
    {
      auto result (Object::New (isolate));
      _gum_v8_object_set_ascii (result, "path", "", core); /* FIXME */
      return result;
    }
  }

  return Null (isolate);
}
