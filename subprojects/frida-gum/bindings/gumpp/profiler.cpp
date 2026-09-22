#include "gumpp.hpp"

#include "invocationcontext.hpp"
#include "objectwrapper.hpp"
#include "runtime.hpp"
#include "string.hpp"

#include <gum/gum-prof.h>

namespace Gum
{
  class ProfileReportImpl : public ObjectWrapper<ProfileReportImpl, ProfileReport, GumProfileReport>
  {
  public:
    ProfileReportImpl (GumProfileReport * handle)
    {
      assign_handle (handle);
    }

    virtual String * emit_xml ()
    {
      return new StringImpl (gum_profile_report_emit_xml (handle));
    }
  };

  class ProfilerImpl : public ObjectWrapper<ProfilerImpl, Profiler, GumProfiler>
  {
  public:
    ProfilerImpl ()
    {
      Runtime::ref ();
      assign_handle (gum_profiler_new ());
    }

    virtual ~ProfilerImpl ()
    {
      Runtime::unref ();
    }

    virtual void instrument_functions_matching (const char * match_str, Sampler * sampler, FunctionMatchCallbacks * match_callbacks)
    {
      gum_profiler_instrument_functions_matching (handle, match_str, GUM_SAMPLER (sampler->get_handle ()),
          match_callbacks != NULL ? match_cb : NULL, match_callbacks);
    }

    virtual bool instrument_function (void * function_address, Sampler * sampler)
    {
      return gum_profiler_instrument_function (handle, function_address, GUM_SAMPLER (sampler->get_handle ())) == GUM_INSTRUMENT_OK;
    }

    virtual bool instrument_function_with_inspector (void * function_address, Sampler * sampler, InspectorCallbacks * inspector_callbacks)
    {
      return gum_profiler_instrument_function_with_inspector (handle, function_address, GUM_SAMPLER (sampler->get_handle ()), inspector_cb,
          inspector_callbacks, NULL) == GUM_INSTRUMENT_OK;
    }

    virtual ProfileReport * generate_report ()
    {
      return new ProfileReportImpl (gum_profiler_generate_report (handle));
    }

  private:
    static gboolean match_cb (const gchar * function_name, gpointer user_data)
    {
      return static_cast<FunctionMatchCallbacks *> (user_data)->match_should_include (function_name) ? TRUE : FALSE;
    }

    static void inspector_cb (GumInvocationContext * context, gchar * output_buf, guint output_buf_len, gpointer user_data)
    {
      InvocationContextImpl ic (context);
      static_cast<InspectorCallbacks *> (user_data)->inspect_worst_case (&ic, output_buf, output_buf_len);
    }
  };

  extern "C" Profiler * Profiler_new (void) { return new ProfilerImpl; }
}
