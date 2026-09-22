#ifndef __GUMPP_OBJECT_WRAPPER_HPP__
#define __GUMPP_OBJECT_WRAPPER_HPP__

#include <glib-object.h>

namespace Gum
{
  template <class D, class B, typename T>
  class ObjectWrapper : public B
  {
  public:
    ObjectWrapper ()
      : handle (NULL)
    {
    }

    virtual ~ObjectWrapper ()
    {
    }

    virtual void ref ()
    {
      g_object_ref (handle);
    }

    virtual void unref ()
    {
      g_object_unref (handle);
    }

    virtual void * get_handle () const
    {
      return handle;
    }

  protected:
    void assign_handle (T * h)
    {
      handle = h;
      g_object_weak_ref (G_OBJECT (handle), delete_wrapper, this);
    }

    static void delete_wrapper (gpointer data, GObject * where_the_object_was)
    {
      D * impl = static_cast<D *> (data);
      g_assert (impl->handle == (gpointer) where_the_object_was);
      delete impl;
    }

    T * handle;
  };
}

#endif
