#ifndef __GUMPP_INVOCATION_LISTENER_HPP__
#define __GUMPP_INVOCATION_LISTENER_HPP__

#include "gumpp.hpp"

namespace Gum
{
  typedef struct _GumInvocationListenerProxy GumInvocationListenerProxy;

  class InvocationListenerProxy : public Object
  {
  public:
    InvocationListenerProxy (InvocationListener * listener);
    virtual ~InvocationListenerProxy ();

    virtual void ref ();
    virtual void unref ();
    virtual void * get_handle () const;

    virtual void on_enter (InvocationContext * context);
    virtual void on_leave (InvocationContext * context);

  protected:
    GumInvocationListenerProxy * cproxy;
    InvocationListener * listener;
  };
}

#endif
