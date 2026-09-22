/*
 * Copyright (C) 2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumquicksocket.h"

#include "gumquickmacros.h"

#ifdef _MSC_VER
# pragma warning (push)
# pragma warning (disable: 4214)
#endif
#include <gio/gnetworking.h>
#ifdef _MSC_VER
# pragma warning (pop)
#endif
#ifdef HAVE_WINDOWS
# define GUM_SOCKOPT_OPTVAL(v) (gchar *) (v)
  typedef int gum_socklen_t;
#else
# include <errno.h>
# define GUM_SOCKOPT_OPTVAL(v) (v)
  typedef socklen_t gum_socklen_t;
#endif
#ifdef G_OS_UNIX
# include <gio/gunixsocketaddress.h>
#endif
#include <string.h>

typedef struct _GumQuickListenOperation GumQuickListenOperation;
typedef struct _GumQuickConnectOperation GumQuickConnectOperation;

typedef struct _GumQuickCloseListenerOperation GumQuickCloseListenerOperation;
typedef struct _GumQuickAcceptOperation GumQuickAcceptOperation;

typedef struct _GumQuickSetNoDelayOperation GumQuickSetNoDelayOperation;

struct _GumQuickListenOperation
{
  GumQuickModuleOperation operation;

  guint16 port;

  gchar * path;

  GSocketAddress * address;
  gint backlog;
};

struct _GumQuickConnectOperation
{
  GumQuickModuleOperation operation;

  GSocketClient * client;
  GSocketFamily family;

  gchar * host;
  guint16 port;

  GSocketConnectable * connectable;

  gboolean tls;
};

struct _GumQuickCloseListenerOperation
{
  GumQuickObjectOperation operation;
};

struct _GumQuickAcceptOperation
{
  GumQuickObjectOperation operation;
};

struct _GumQuickSetNoDelayOperation
{
  GumQuickObjectOperation parent;
  gboolean no_delay;
};

GUMJS_DECLARE_FUNCTION (gumjs_socket_listen)
static void gum_quick_listen_operation_dispose (GumQuickListenOperation * self);
static void gum_quick_listen_operation_perform (GumQuickListenOperation * self);
GUMJS_DECLARE_FUNCTION (gumjs_socket_connect)
static void gum_quick_connect_operation_dispose (
    GumQuickConnectOperation * self);
static void gum_quick_connect_operation_start (GumQuickConnectOperation * self);
static void gum_quick_connect_operation_finish (GSocketClient * client,
    GAsyncResult * result, GumQuickConnectOperation * self);
GUMJS_DECLARE_FUNCTION (gumjs_socket_get_type)
GUMJS_DECLARE_FUNCTION (gumjs_socket_get_local_address)
GUMJS_DECLARE_FUNCTION (gumjs_socket_get_peer_address)

static JSValue gum_quick_socket_listener_new (JSContext * ctx,
    GSocketListener * listener, GumQuickSocket * parent);
GUMJS_DECLARE_CONSTRUCTOR (gumjs_socket_listener_construct)
GUMJS_DECLARE_FUNCTION (gumjs_socket_listener_close)
static void gum_quick_close_listener_operation_perform (
    GumQuickCloseListenerOperation * self);
GUMJS_DECLARE_FUNCTION (gumjs_socket_listener_accept)
static void gum_quick_accept_operation_start (GumQuickAcceptOperation * self);
static void gum_quick_accept_operation_finish (GSocketListener * listener,
    GAsyncResult * result, GumQuickAcceptOperation * self);

static JSValue gum_quick_socket_connection_new (JSContext * ctx,
    GSocketConnection * connection, GumQuickSocket * parent);
GUMJS_DECLARE_CONSTRUCTOR (gumjs_socket_connection_construct)
GUMJS_DECLARE_FUNCTION (gumjs_socket_connection_set_no_delay)
static void gum_quick_set_no_delay_operation_perform (
    GumQuickSetNoDelayOperation * self);

static gboolean gum_quick_socket_family_get (JSContext * ctx, JSValue val,
    GSocketFamily * family);
static gboolean gum_quick_unix_socket_address_type_get (JSContext * ctx,
    JSValue val, GUnixSocketAddressType * type);
static JSValue gum_quick_socket_address_new (JSContext * ctx,
    struct sockaddr * addr, GumQuickCore * core);

static const JSCFunctionListEntry gumjs_socket_entries[] =
{
  JS_CFUNC_DEF ("_listen", 0, gumjs_socket_listen),
  JS_CFUNC_DEF ("_connect", 0, gumjs_socket_connect),
  JS_CFUNC_DEF ("type", 0, gumjs_socket_get_type),
  JS_CFUNC_DEF ("localAddress", 0, gumjs_socket_get_local_address),
  JS_CFUNC_DEF ("peerAddress", 0, gumjs_socket_get_peer_address),
};

static const JSClassDef gumjs_socket_listener_def =
{
  .class_name = "SocketListener",
};

static const JSCFunctionListEntry gumjs_socket_listener_entries[] =
{
  JS_CFUNC_DEF ("_close", 0, gumjs_socket_listener_close),
  JS_CFUNC_DEF ("_accept", 0, gumjs_socket_listener_accept),
};

static const JSClassDef gumjs_socket_connection_def =
{
  .class_name = "SocketConnection",
};

static const JSCFunctionListEntry gumjs_socket_connection_entries[] =
{
  JS_CFUNC_DEF ("_setNoDelay", 0, gumjs_socket_connection_set_no_delay),
};

void
_gum_quick_socket_init (GumQuickSocket * self,
                        JSValue ns,
                        GumQuickStream * stream,
                        GumQuickCore * core)
{
  JSContext * ctx = core->ctx;
  JSValue obj, proto, ctor;

  self->stream = stream;
  self->core = core;

  _gum_quick_core_store_module_data (core, "socket", self);

  obj = JS_NewObject (ctx);
  JS_SetPropertyFunctionList (ctx, obj, gumjs_socket_entries,
      G_N_ELEMENTS (gumjs_socket_entries));
  JS_DefinePropertyValueStr (ctx, ns, "Socket", obj, JS_PROP_C_W_E);

  _gum_quick_create_class (ctx, &gumjs_socket_listener_def, core,
      &self->socket_listener_class, &proto);
  ctor = JS_NewCFunction2 (ctx, gumjs_socket_listener_construct,
      gumjs_socket_listener_def.class_name, 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor (ctx, ctor, proto);
  JS_SetPropertyFunctionList (ctx, proto, gumjs_socket_listener_entries,
      G_N_ELEMENTS (gumjs_socket_listener_entries));
  JS_DefinePropertyValueStr (ctx, ns, gumjs_socket_listener_def.class_name,
      ctor, JS_PROP_C_W_E);

  _gum_quick_create_subclass (ctx, &gumjs_socket_connection_def,
      stream->io_stream_class, stream->io_stream_proto, core,
      &self->socket_connection_class, &proto);
  ctor = JS_NewCFunction2 (ctx, gumjs_socket_connection_construct,
      gumjs_socket_connection_def.class_name, 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor (ctx, ctor, proto);
  JS_SetPropertyFunctionList (ctx, proto, gumjs_socket_connection_entries,
      G_N_ELEMENTS (gumjs_socket_connection_entries));
  JS_DefinePropertyValueStr (ctx, ns, gumjs_socket_connection_def.class_name,
      ctor, JS_PROP_C_W_E);

  _gum_quick_object_manager_init (&self->objects, self, core);
}

void
_gum_quick_socket_flush (GumQuickSocket * self)
{
  _gum_quick_object_manager_flush (&self->objects);
}

void
_gum_quick_socket_dispose (GumQuickSocket * self)
{
  _gum_quick_object_manager_free (&self->objects);
}

void
_gum_quick_socket_finalize (GumQuickSocket * self)
{
}

static GumQuickSocket *
gumjs_get_parent_module (GumQuickCore * core)
{
  return _gum_quick_core_load_module_data (core, "socket");
}

GUMJS_DEFINE_FUNCTION (gumjs_socket_listen)
{
  GumQuickSocket * parent;
  GSocketFamily family;
  JSValue family_value;
  const gchar * host;
  guint port;
  GUnixSocketAddressType type;
  JSValue type_val;
  const gchar * path;
  guint backlog;
  JSValue callback;
  GSocketAddress * address = NULL;
  GumQuickListenOperation * op;

  parent = gumjs_get_parent_module (core);

  if (!_gum_quick_args_parse (args, "V?s?uV?s?uF", &family_value, &host, &port,
      &type_val, &path, &backlog, &callback))
    return JS_EXCEPTION;
  if (!gum_quick_socket_family_get (ctx, family_value, &family))
    return JS_EXCEPTION;
  if (!gum_quick_unix_socket_address_type_get (ctx, type_val, &type))
    return JS_EXCEPTION;

  if (host != NULL)
  {
    address = g_inet_socket_address_new_from_string (host, port);
    if (address == NULL)
      return _gum_quick_throw_literal (ctx, "invalid host");
  }
  else if (path != NULL)
  {
#ifdef G_OS_UNIX
    address = g_unix_socket_address_new_with_type (path, -1, type);
    g_assert (address != NULL);
#else
    return _gum_quick_throw_literal (ctx, "UNIX sockets not available");
#endif
  }
  else if (family != G_SOCKET_FAMILY_INVALID)
  {
    address = g_inet_socket_address_new_from_string (
        (family == G_SOCKET_FAMILY_IPV4) ? "0.0.0.0" : "::",
        port);
    g_assert (address != NULL);
  }

  op = _gum_quick_module_operation_new (GumQuickListenOperation, parent,
      callback, gum_quick_listen_operation_perform,
      gum_quick_listen_operation_dispose);
  op->port = port;
  op->path = g_strdup (path);
  op->address = address;
  op->backlog = backlog;
  _gum_quick_module_operation_schedule (op);

  return JS_UNDEFINED;
}

static void
gum_quick_listen_operation_dispose (GumQuickListenOperation * self)
{
  g_clear_object (&self->address);
  g_free (self->path);
}

static void
gum_quick_listen_operation_perform (GumQuickListenOperation * self)
{
  GumQuickModuleOperation * op = GUM_QUICK_MODULE_OPERATION (self);
  GumQuickCore * core = op->core;
  JSContext * ctx = core->ctx;
  GSocketListener * listener;
  GSocketAddress * effective_address = NULL;
  GError * error = NULL;
  GumQuickScope scope;
  JSValue argv[2];

  listener = g_object_new (G_TYPE_SOCKET_LISTENER,
      "listen-backlog", self->backlog,
      NULL);

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

  _gum_quick_scope_enter (&scope, core);

  if (error == NULL)
  {
    JSValue listener_obj =
        gum_quick_socket_listener_new (ctx, listener, op->module);

    if (self->path != NULL)
    {
      JS_DefinePropertyValue (ctx, listener_obj,
          GUM_QUICK_CORE_ATOM (core, path),
          JS_NewString (ctx, self->path),
          JS_PROP_C_W_E);
    }
    else
    {
      guint16 port;

      if (effective_address != NULL)
      {
        port = g_inet_socket_address_get_port (
            G_INET_SOCKET_ADDRESS (effective_address));
        g_clear_object (&effective_address);
      }
      else
      {
        port = self->port;
      }

      JS_DefinePropertyValue (ctx, listener_obj,
          GUM_QUICK_CORE_ATOM (core, port),
          JS_NewInt32 (ctx, port),
          JS_PROP_C_W_E);
    }

    argv[0] = JS_NULL;
    argv[1] = listener_obj;
  }
  else
  {
    argv[0] = _gum_quick_error_new_take_error (ctx, &error, core);
    argv[1] = JS_NULL;
  }

  _gum_quick_scope_call_void (&scope, op->callback, JS_UNDEFINED,
      G_N_ELEMENTS (argv), argv);

  JS_FreeValue (ctx, argv[0]);
  JS_FreeValue (ctx, argv[1]);

  _gum_quick_scope_leave (&scope);

  _gum_quick_module_operation_finish (op);
}

GUMJS_DEFINE_FUNCTION (gumjs_socket_connect)
{
  GumQuickSocket * parent;
  GSocketFamily family;
  JSValue family_value;
  const gchar * host;
  guint port;
  GUnixSocketAddressType type;
  JSValue type_val;
  const gchar * path;
  gboolean tls;
  JSValue callback;
  GSocketConnectable * connectable = NULL;
  GumQuickConnectOperation * op;

  parent = gumjs_get_parent_module (core);

  if (!_gum_quick_args_parse (args, "V?s?uV?s?tF", &family_value, &host, &port,
      &type_val, &path, &tls, &callback))
    return JS_EXCEPTION;
  if (!gum_quick_socket_family_get (ctx, family_value, &family))
    return JS_EXCEPTION;
  if (!gum_quick_unix_socket_address_type_get (ctx, type_val, &type))
    return JS_EXCEPTION;

  if (path != NULL)
  {
#ifdef G_OS_UNIX
    family = G_SOCKET_FAMILY_UNIX;
    connectable = G_SOCKET_CONNECTABLE (g_unix_socket_address_new_with_type (
        path, -1, type));
    g_assert (connectable != NULL);
#else
    return _gum_quick_throw_literal (ctx, "UNIX sockets not available");
#endif
  }

  op = _gum_quick_module_operation_new (GumQuickConnectOperation, parent,
      callback, gum_quick_connect_operation_start,
      gum_quick_connect_operation_dispose);
  op->client = NULL;
  op->family = family;
  op->host = g_strdup (host);
  op->port = port;
  op->connectable = connectable;
  op->tls = tls;
  _gum_quick_module_operation_schedule (op);

  return JS_UNDEFINED;
}

static void
gum_quick_connect_operation_dispose (GumQuickConnectOperation * self)
{
  g_clear_object (&self->connectable);
  g_free (self->host);
  g_object_unref (self->client);
}

static void
gum_quick_connect_operation_start (GumQuickConnectOperation * self)
{
  GumQuickModuleOperation * op = GUM_QUICK_MODULE_OPERATION (self);

  self->client = g_object_new (G_TYPE_SOCKET_CLIENT,
      "family", self->family,
      "tls", self->tls,
      NULL);

  if (self->connectable != NULL)
  {
    g_socket_client_connect_async (self->client, self->connectable,
        op->cancellable,
        (GAsyncReadyCallback) gum_quick_connect_operation_finish,
        self);
  }
  else
  {
    g_socket_client_connect_to_host_async (self->client, self->host, self->port,
        op->cancellable,
        (GAsyncReadyCallback) gum_quick_connect_operation_finish,
        self);
  }
}

static void
gum_quick_connect_operation_finish (GSocketClient * client,
                                    GAsyncResult * result,
                                    GumQuickConnectOperation * self)
{
  GumQuickModuleOperation * op = GUM_QUICK_MODULE_OPERATION (self);
  GumQuickCore * core = op->core;
  JSContext * ctx = core->ctx;
  GError * error = NULL;
  GSocketConnection * connection;
  GumQuickScope scope;
  JSValue argv[2];

  if (self->connectable != NULL)
  {
    connection = g_socket_client_connect_finish (client, result, &error);
  }
  else
  {
    connection = g_socket_client_connect_to_host_finish (client, result,
        &error);
  }

  _gum_quick_scope_enter (&scope, core);

  if (error == NULL)
  {
    argv[0] = JS_NULL;
    argv[1] = gum_quick_socket_connection_new (ctx, connection, op->module);
  }
  else
  {
    argv[0] = _gum_quick_error_new_take_error (ctx, &error, core);
    argv[1] = JS_NULL;
  }

  _gum_quick_scope_call_void (&scope, op->callback, JS_UNDEFINED,
      G_N_ELEMENTS (argv), argv);

  JS_FreeValue (ctx, argv[0]);
  JS_FreeValue (ctx, argv[1]);

  _gum_quick_scope_leave (&scope);

  _gum_quick_module_operation_finish (op);
}

GUMJS_DEFINE_FUNCTION (gumjs_socket_get_type)
{
  const gchar * result = NULL;
  gint sock, type;
  gum_socklen_t len;

  if (!_gum_quick_args_parse (args, "i", &sock))
    return JS_EXCEPTION;

  len = sizeof (gint);
  if (getsockopt (sock, SOL_SOCKET, SO_TYPE, GUM_SOCKOPT_OPTVAL (&type),
      &len) == 0)
  {
    gint family;
    struct sockaddr_in6 addr;

    len = sizeof (addr);
    if (getsockname (sock, (struct sockaddr *) &addr, &len) == 0)
    {
      family = addr.sin6_family;
    }
    else
    {
      struct sockaddr_in invalid_sockaddr;

      invalid_sockaddr.sin_family = AF_INET;
      invalid_sockaddr.sin_port = GUINT16_TO_BE (0);
      invalid_sockaddr.sin_addr.s_addr = GUINT32_TO_BE (0xffffffff);

      bind (sock,
          (struct sockaddr *) &invalid_sockaddr,
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
          case SOCK_STREAM: result = "tcp"; break;
          case  SOCK_DGRAM: result = "udp"; break;
        }
        break;
      case AF_INET6:
        switch (type)
        {
          case SOCK_STREAM: result = "tcp6"; break;
          case  SOCK_DGRAM: result = "udp6"; break;
        }
        break;
#ifndef HAVE_WINDOWS
      case AF_UNIX:
        switch (type)
        {
          case SOCK_STREAM: result = "unix:stream"; break;
          case  SOCK_DGRAM: result = "unix:dgram";  break;
        }
        break;
#endif
    }
  }

  return (result != NULL)
      ? JS_NewString (ctx, result)
      : JS_NULL;
}

GUMJS_DEFINE_FUNCTION (gumjs_socket_get_local_address)
{
  gint sock;
  struct sockaddr_in6 large_addr;
  struct sockaddr * addr = (struct sockaddr *) &large_addr;
  gum_socklen_t len = sizeof (large_addr);

  if (!_gum_quick_args_parse (args, "i", &sock))
    return JS_EXCEPTION;

  if (getsockname (sock, addr, &len) != 0)
    return JS_NULL;

  return gum_quick_socket_address_new (ctx, addr, core);
}

GUMJS_DEFINE_FUNCTION (gumjs_socket_get_peer_address)
{
  gint sock;
  struct sockaddr_in6 large_addr;
  struct sockaddr * addr = (struct sockaddr *) &large_addr;
  gum_socklen_t len = sizeof (large_addr);

  if (!_gum_quick_args_parse (args, "i", &sock))
    return JS_EXCEPTION;

  if (getpeername (sock, addr, &len) != 0)
    return JS_NULL;

  return gum_quick_socket_address_new (ctx, addr, core);
}

static JSValue
gum_quick_socket_listener_new (JSContext * ctx,
                               GSocketListener * listener,
                               GumQuickSocket * parent)
{
  JSValue wrapper = JS_NewObjectClass (ctx, parent->socket_listener_class);

  _gum_quick_object_manager_add (&parent->objects, ctx, wrapper, listener);

  return wrapper;
}

static gboolean
gum_quick_socket_listener_get (JSContext * ctx,
                               JSValueConst val,
                               GumQuickCore * core,
                               GumQuickObject ** object)
{
  return _gum_quick_unwrap (ctx, val,
      gumjs_get_parent_module (core)->socket_listener_class, core,
      (gpointer *) object);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_socket_listener_construct)
{
  return _gum_quick_throw_literal (ctx, "not user-instantiable");
}

GUMJS_DEFINE_FUNCTION (gumjs_socket_listener_close)
{
  GumQuickObject * self;
  JSValue callback;
  GumQuickCloseListenerOperation * op;

  if (!gum_quick_socket_listener_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "F", &callback))
    return JS_EXCEPTION;

  g_cancellable_cancel (self->cancellable);

  op = _gum_quick_object_operation_new (GumQuickCloseListenerOperation, self,
      callback, gum_quick_close_listener_operation_perform, NULL);
  _gum_quick_object_operation_schedule_when_idle (op, NULL);

  return JS_UNDEFINED;
}

static void
gum_quick_close_listener_operation_perform (
    GumQuickCloseListenerOperation * self)
{
  GumQuickObjectOperation * op = GUM_QUICK_OBJECT_OPERATION (self);
  GumQuickScope scope;

  g_socket_listener_close (op->object->handle);

  _gum_quick_scope_enter (&scope, op->core);
  _gum_quick_scope_call_void (&scope, op->callback, JS_UNDEFINED, 0, NULL);
  _gum_quick_scope_leave (&scope);

  _gum_quick_object_operation_finish (op);
}

GUMJS_DEFINE_FUNCTION (gumjs_socket_listener_accept)
{
  GumQuickObject * self;
  JSValue callback;
  GumQuickAcceptOperation * op;

  if (!gum_quick_socket_listener_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "F", &callback))
    return JS_EXCEPTION;

  op = _gum_quick_object_operation_new (GumQuickAcceptOperation, self, callback,
      gum_quick_accept_operation_start, NULL);
  _gum_quick_object_operation_schedule (op);

  return JS_UNDEFINED;
}

static void
gum_quick_accept_operation_start (GumQuickAcceptOperation * self)
{
  GumQuickObjectOperation * op = GUM_QUICK_OBJECT_OPERATION (self);
  GumQuickObject * listener = op->object;

  g_socket_listener_accept_async (listener->handle, listener->cancellable,
      (GAsyncReadyCallback) gum_quick_accept_operation_finish, self);
}

static void
gum_quick_accept_operation_finish (GSocketListener * listener,
                                   GAsyncResult * result,
                                   GumQuickAcceptOperation * self)
{
  GumQuickObjectOperation * op = GUM_QUICK_OBJECT_OPERATION (self);
  GumQuickCore * core = op->core;
  JSContext * ctx = core->ctx;
  GError * error = NULL;
  GSocketConnection * connection;
  GumQuickScope scope;
  JSValue argv[2];

  connection = g_socket_listener_accept_finish (listener, result, NULL, &error);

  _gum_quick_scope_enter (&scope, core);

  if (error == NULL)
  {
    argv[0] = JS_NULL;
    argv[1] = gum_quick_socket_connection_new (ctx, connection,
        gumjs_get_parent_module (core));
  }
  else
  {
    argv[0] = _gum_quick_error_new_take_error (ctx, &error, core);
    argv[1] = JS_NULL;
  }

  _gum_quick_scope_call_void (&scope, op->callback, JS_UNDEFINED,
      G_N_ELEMENTS (argv), argv);

  JS_FreeValue (ctx, argv[0]);
  JS_FreeValue (ctx, argv[1]);

  _gum_quick_scope_leave (&scope);

  _gum_quick_object_operation_finish (op);
}

static JSValue
gum_quick_socket_connection_new (JSContext * ctx,
                                 GSocketConnection * connection,
                                 GumQuickSocket * parent)
{
  JSValue wrapper = JS_NewObjectClass (ctx, parent->socket_connection_class);

  _gum_quick_object_manager_add (&parent->objects, ctx, wrapper, connection);

  return wrapper;
}

static gboolean
gum_quick_socket_connection_get (JSContext * ctx,
                                 JSValueConst val,
                                 GumQuickCore * core,
                                 GumQuickObject ** object)
{
  return _gum_quick_unwrap (ctx, val,
      gumjs_get_parent_module (core)->socket_connection_class, core,
      (gpointer *) object);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_socket_connection_construct)
{
  return _gum_quick_throw_literal (ctx, "not user-instantiable");
}

GUMJS_DEFINE_FUNCTION (gumjs_socket_connection_set_no_delay)
{
  GumQuickObject * self;
  gboolean no_delay;
  JSValue callback;
  GumQuickSetNoDelayOperation * op;

  if (!gum_quick_socket_connection_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "tF", &no_delay, &callback))
    return JS_EXCEPTION;

  op = _gum_quick_object_operation_new (GumQuickSetNoDelayOperation, self,
      callback, gum_quick_set_no_delay_operation_perform, NULL);
  op->no_delay = no_delay;
  _gum_quick_object_operation_schedule (op);

  return JS_UNDEFINED;
}

static void
gum_quick_set_no_delay_operation_perform (GumQuickSetNoDelayOperation * self)
{
  GumQuickObjectOperation * op = GUM_QUICK_OBJECT_OPERATION (self);
  GumQuickCore * core = op->core;
  JSContext * ctx = core->ctx;
  GSocket * socket;
  GError * error = NULL;
  gboolean success;
  GumQuickScope scope;
  JSValue argv[2];

  socket = g_socket_connection_get_socket (op->object->handle);

  success = g_socket_set_option (socket, IPPROTO_TCP, TCP_NODELAY,
      self->no_delay, &error);

  _gum_quick_scope_enter (&scope, core);

  argv[0] = _gum_quick_error_new_take_error (ctx, &error, core);
  argv[1] = JS_NewBool (ctx, success);

  _gum_quick_scope_call_void (&scope, op->callback, JS_UNDEFINED,
      G_N_ELEMENTS (argv), argv);

  JS_FreeValue (ctx, argv[0]);

  _gum_quick_scope_leave (&scope);

  _gum_quick_object_operation_finish (op);
}

static gboolean
gum_quick_socket_family_get (JSContext * ctx,
                             JSValue val,
                             GSocketFamily * family)
{
  gboolean success = FALSE;
  const char * str = NULL;

  if (JS_IsNull (val))
  {
    *family = G_SOCKET_FAMILY_INVALID;
    success = TRUE;
    goto beach;
  }

  if (!JS_IsString (val))
    goto invalid_value;
  str = JS_ToCString (ctx, val);

  if (strcmp (str, "unix") == 0)
    *family = G_SOCKET_FAMILY_UNIX;
  else if (strcmp (str, "ipv4") == 0)
    *family = G_SOCKET_FAMILY_IPV4;
  else if (strcmp (str, "ipv6") == 0)
    *family = G_SOCKET_FAMILY_IPV6;
  else
    goto invalid_value;

  success = TRUE;
  goto beach;

invalid_value:
  {
    _gum_quick_throw_literal (ctx, "invalid socket address family");
    goto beach;
  }
beach:
  {
    JS_FreeCString (ctx, str);

    return success;
  }
}

static gboolean
gum_quick_unix_socket_address_type_get (JSContext * ctx,
                                        JSValue val,
                                        GUnixSocketAddressType * type)
{
  gboolean success = FALSE;
  const char * str = NULL;

  if (JS_IsNull (val))
  {
    *type = G_UNIX_SOCKET_ADDRESS_PATH;
    success = TRUE;
    goto beach;
  }

  if (!JS_IsString (val))
    goto invalid_value;
  str = JS_ToCString (ctx, val);

  if (strcmp (str, "anonymous") == 0)
    *type = G_UNIX_SOCKET_ADDRESS_ANONYMOUS;
  else if (strcmp (str, "path") == 0)
    *type = G_UNIX_SOCKET_ADDRESS_PATH;
  else if (strcmp (str, "abstract") == 0)
    *type = G_UNIX_SOCKET_ADDRESS_ABSTRACT;
  else if (strcmp (str, "abstract-padded") == 0)
    *type = G_UNIX_SOCKET_ADDRESS_ABSTRACT_PADDED;
  else
    goto invalid_value;

  success = TRUE;
  goto beach;

invalid_value:
  {
    _gum_quick_throw_literal (ctx, "invalid UNIX socket address type");
    goto beach;
  }
beach:
  {
    JS_FreeCString (ctx, str);

    return success;
  }
}

static JSValue
gum_quick_socket_address_new (JSContext * ctx,
                              struct sockaddr * addr,
                              GumQuickCore * core)
{
  JSValue obj;

  switch (addr->sa_family)
  {
    case AF_INET:
    {
      struct sockaddr_in * inet_addr = (struct sockaddr_in *) addr;
#ifdef HAVE_WINDOWS
      gunichar2 ip_utf16[15 + 1 + 5 + 1];
      gchar ip[15 + 1 + 5 + 1];
      DWORD len = G_N_ELEMENTS (ip_utf16);
      gchar * p;

      WSAAddressToStringW (addr, sizeof (struct sockaddr_in), NULL,
          (LPWSTR) ip_utf16, &len);
      WideCharToMultiByte (CP_UTF8, 0, (LPWSTR) ip_utf16, -1, ip, len, NULL,
          NULL);
      p = strchr (ip, ':');
      if (p != NULL)
        *p = '\0';
#else
      gchar ip[INET_ADDRSTRLEN];

      inet_ntop (AF_INET, &inet_addr->sin_addr, ip, sizeof (ip));
#endif

      obj = JS_NewObject (ctx);

      JS_DefinePropertyValue (ctx, obj,
          GUM_QUICK_CORE_ATOM (core, ip),
          JS_NewString (ctx, ip),
          JS_PROP_C_W_E);
      JS_DefinePropertyValue (ctx, obj,
          GUM_QUICK_CORE_ATOM (core, port),
          JS_NewInt32 (ctx, GUINT16_FROM_BE (inet_addr->sin_port)),
          JS_PROP_C_W_E);

      break;
    }
    case AF_INET6:
    {
      struct sockaddr_in6 * inet_addr = (struct sockaddr_in6 *) addr;
#ifdef HAVE_WINDOWS
      gunichar2 ip_utf16[45 + 1 + 5 + 1];
      gchar ip[45 + 1 + 5 + 1];
      DWORD len = G_N_ELEMENTS (ip_utf16);
      gchar * p;

      WSAAddressToStringW (addr, sizeof (struct sockaddr_in6), NULL,
          (LPWSTR) ip_utf16, &len);
      WideCharToMultiByte (CP_UTF8, 0, (LPWSTR) ip_utf16, -1, ip, len, NULL,
          NULL);
      p = strrchr (ip, ':');
      if (p != NULL)
        *p = '\0';
#else
      gchar ip[INET6_ADDRSTRLEN];

      inet_ntop (AF_INET6, &inet_addr->sin6_addr, ip, sizeof (ip));
#endif

      obj = JS_NewObject (ctx);

      JS_DefinePropertyValue (ctx, obj,
          GUM_QUICK_CORE_ATOM (core, ip),
          JS_NewString (ctx, ip),
          JS_PROP_C_W_E);
      JS_DefinePropertyValue (ctx, obj,
          GUM_QUICK_CORE_ATOM (core, port),
          JS_NewInt32 (ctx, GUINT16_FROM_BE (inet_addr->sin6_port)),
          JS_PROP_C_W_E);

      break;
    }
    case AF_UNIX:
    {
      const gchar * path = ""; /* FIXME */

      obj = JS_NewObject (ctx);

      JS_DefinePropertyValue (ctx, obj,
          GUM_QUICK_CORE_ATOM (core, path),
          JS_NewString (ctx, path),
          JS_PROP_C_W_E);

      break;
    }
    default:
      return JS_NULL;
  }

  return obj;
}
