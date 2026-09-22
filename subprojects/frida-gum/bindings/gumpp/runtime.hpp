#ifndef __RUNTIME_HPP__
#define __RUNTIME_HPP__

namespace Gum
{
  class Runtime
  {
  public:
    static void ref ();
    static void unref ();

  private:
    static volatile int ref_count;
  };
}

#endif
