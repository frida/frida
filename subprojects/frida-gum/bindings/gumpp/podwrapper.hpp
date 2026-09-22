#ifndef __GUMPP_POD_WRAPPER_HPP__
#define __GUMPP_POD_WRAPPER_HPP__

#include <glib.h>

namespace Gum
{
  template <class D, class B, typename T>
  class PodWrapper : public B
  {
  public:
    PodWrapper ()
      : refcount (1),
        handle (NULL)
    {
    }

    virtual ~PodWrapper ()
    {
    }

    virtual void ref ()
    {
      g_atomic_int_add (&refcount, 1);
    }

    virtual void unref ()
    {
      if (g_atomic_int_dec_and_test (&refcount))
        delete static_cast<D *> (this);
    }

    virtual void * get_handle () const
    {
      return handle;
    }

  protected:
    void assign_handle (T * h)
    {
      handle = h;
    }

    volatile gint refcount;
    T * handle;
  };
}

#endif
