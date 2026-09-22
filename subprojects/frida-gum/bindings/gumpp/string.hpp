#ifndef __GUMPP_STRING_HPP__
#define __GUMPP_STRING_HPP__

#include "gumpp.hpp"

#include "podwrapper.hpp"

#include <cstring>

namespace Gum
{
  class StringImpl : public PodWrapper<StringImpl, String, gchar>
  {
  public:
    StringImpl (gchar * str)
    {
      assign_handle (str);
    }

    virtual ~StringImpl ()
    {
      g_free (handle);
    }

    virtual const char * c_str ()
    {
      return handle;
    }

    virtual size_t length () const
    {
      return strlen (handle);
    }
  };
}

#endif
