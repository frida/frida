#include "Runtime.hpp"

#include <frida-core.h>

namespace Frida
{
  volatile int Runtime::refCount = 0;

  void Runtime::Ref ()
  {
    g_atomic_int_inc (&refCount);
    frida_init ();
  }

  void Runtime::Unref ()
  {
    if (g_atomic_int_dec_and_test (&refCount))
    {
      frida_deinit ();
    }
  }

  class Assembly
  {
  public:
    Assembly ()
    {
      Runtime::Ref ();
    }

    ~Assembly ()
    {
      Runtime::Unref ();
    }
  };
  static Assembly assembly;
}