#include "invocationlistener.hpp"

#include "invocationcontext.hpp"

#include <gum/gum.h>

namespace Gum
{
  class InvocationListenerProxy;

  typedef struct _GumInvocationListenerProxyClass GumInvocationListenerProxyClass;

  struct _GumInvocationListenerProxy
  {
    GObject parent;
    InvocationListenerProxy * proxy;
  };

  struct _GumInvocationListenerProxyClass
  {
    GObjectClass parent_class;
  };

  static GType gum_invocation_listener_proxy_get_type ();
  static void gum_invocation_listener_proxy_iface_init (gpointer g_iface, gpointer iface_data);

  InvocationListenerProxy::InvocationListenerProxy (InvocationListener * listener)
    : cproxy (static_cast<GumInvocationListenerProxy *> (g_object_new (gum_invocation_listener_proxy_get_type (), NULL))),
      listener (listener)
  {
    cproxy->proxy = this;
  }

  InvocationListenerProxy::~InvocationListenerProxy ()
  {
  }

  void InvocationListenerProxy::ref ()
  {
    g_object_ref (cproxy);
  }

  void InvocationListenerProxy::unref ()
  {
    g_object_unref (cproxy);
  }

  void * InvocationListenerProxy::get_handle () const
  {
    return cproxy;
  }

  void InvocationListenerProxy::on_enter (InvocationContext * context)
  {
    listener->on_enter (context);
  }

  void InvocationListenerProxy::on_leave (InvocationContext * context)
  {
    listener->on_leave (context);
  }

  G_DEFINE_TYPE_EXTENDED (GumInvocationListenerProxy,
                          gum_invocation_listener_proxy,
                          G_TYPE_OBJECT,
                          0,
                          G_IMPLEMENT_INTERFACE (GUM_TYPE_INVOCATION_LISTENER,
                              gum_invocation_listener_proxy_iface_init))

  static void
  gum_invocation_listener_proxy_init (GumInvocationListenerProxy * self)
  {
  }

  static void
  gum_invocation_listener_proxy_finalize (GObject * obj)
  {
    delete reinterpret_cast<GumInvocationListenerProxy *> (obj)->proxy;

    G_OBJECT_CLASS (gum_invocation_listener_proxy_parent_class)->finalize (obj);
  }

  static void
  gum_invocation_listener_proxy_class_init (GumInvocationListenerProxyClass * klass)
  {
    G_OBJECT_CLASS (klass)->finalize = gum_invocation_listener_proxy_finalize;
  }

  static void
  gum_invocation_listener_proxy_on_enter (GumInvocationListener * listener,
                                          GumInvocationContext * context)
  {
    InvocationContextImpl ic (context);
    reinterpret_cast<GumInvocationListenerProxy *> (listener)->proxy->on_enter (&ic);
  }

  static void
  gum_invocation_listener_proxy_on_leave (GumInvocationListener * listener,
                                          GumInvocationContext * context)
  {
    InvocationContextImpl ic (context);
    reinterpret_cast<GumInvocationListenerProxy *> (listener)->proxy->on_leave (&ic);
  }

  static void
  gum_invocation_listener_proxy_iface_init (gpointer g_iface,
                                            gpointer iface_data)
  {
    GumInvocationListenerInterface * iface =
        static_cast<GumInvocationListenerInterface *> (g_iface);

    iface->on_enter = gum_invocation_listener_proxy_on_enter;
    iface->on_leave = gum_invocation_listener_proxy_on_leave;
  }
}
