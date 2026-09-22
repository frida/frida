#include "gumpp.hpp"

#include "objectwrapper.hpp"
#include "runtime.hpp"

#include <gum/gum-prof.h>

namespace Gum
{
  class SamplerImpl : public ObjectWrapper<SamplerImpl, Sampler, GumSampler>
  {
  public:
    SamplerImpl (GumSampler * handle)
    {
      assign_handle (handle);
    }

    virtual ~SamplerImpl ()
    {
      Runtime::unref ();
    }

    virtual Sample sample () const
    {
      return gum_sampler_sample (handle);
    }
  };

  class CallCountSamplerImpl : public ObjectWrapper<CallCountSamplerImpl, CallCountSampler, GumCallCountSampler>
  {
  public:
    CallCountSamplerImpl (GumCallCountSampler * handle)
    {
      assign_handle (handle);
    }

    virtual ~CallCountSamplerImpl ()
    {
      Runtime::unref ();
    }

    virtual Sample sample () const
    {
      return gum_sampler_sample (GUM_SAMPLER (handle));
    }

    virtual void add_function (void * function_address)
    {
      gum_call_count_sampler_add_function (handle, function_address);
    }

    virtual Sample peek_total_count () const
    {
      return gum_call_count_sampler_peek_total_count (handle);
    }
  };

  extern "C" Sampler * BusyCycleSampler_new () { Runtime::ref (); return new SamplerImpl (gum_busy_cycle_sampler_new ()); }
  extern "C" Sampler * CycleSampler_new () { Runtime::ref (); return new SamplerImpl (gum_cycle_sampler_new ()); }
  extern "C" Sampler * MallocCountSampler_new () { Runtime::ref (); return new SamplerImpl (gum_malloc_count_sampler_new ()); }
  extern "C" Sampler * WallClockSampler_new () { Runtime::ref (); return new SamplerImpl (gum_wall_clock_sampler_new ()); }

  extern "C" CallCountSampler * CallCountSampler_new (void * first_function, ...)
  {
    Runtime::ref ();

    va_list args;
    va_start (args, first_function);
    GumSampler * sampler = gum_call_count_sampler_new_valist (first_function, args);
    va_end (args);

    return new CallCountSamplerImpl (GUM_CALL_COUNT_SAMPLER (sampler));
  }

  extern "C" CallCountSampler * CallCountSampler_new_by_name (const char * first_function_name, ...)
  {
    Runtime::ref ();

    va_list args;
    va_start (args, first_function_name);
    GumSampler * sampler = gum_call_count_sampler_new_by_name_valist (first_function_name, args);
    va_end (args);

    return new CallCountSamplerImpl (GUM_CALL_COUNT_SAMPLER (sampler));
  }
}
