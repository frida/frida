#if defined (HAVE_TVOS) || defined (HAVE_WATCHOS)
# include <Availability.h>
# undef __TVOS_PROHIBITED
# define __TVOS_PROHIBITED
# undef __WATCHOS_PROHIBITED
# define __WATCHOS_PROHIBITED
#endif

#include "frida-gadget.h"

#include "frida-base.h"

#import <Foundation/Foundation.h>
#include <gum/gumdarwin.h>
#include <mach-o/loader.h>
#include <objc/runtime.h>
#include <pthread.h>

static void frida_on_breakpoints_steal_attempt (GumInvocationContext * ic, gpointer user_data);

static GumInvocationListener * frida_dont_steal_my_breakpoints;

gchar *
frida_gadget_environment_detect_bundle_id (void)
{
  @autoreleasepool
  {
    NSString * identifier = NSBundle.mainBundle.bundleIdentifier;
    return g_strdup (identifier.UTF8String);
  }
}

gchar *
frida_gadget_environment_detect_bundle_name (void)
{
  @autoreleasepool
  {
    NSString * name = [NSBundle.mainBundle objectForInfoDictionaryKey:@"CFBundleName"];
    return g_strdup (name.UTF8String);
  }
}

gchar *
frida_gadget_environment_detect_documents_dir (void)
{
#if defined (HAVE_IOS) || defined (HAVE_TVOS) || defined (HAVE_XROS)
  @autoreleasepool
  {
    NSArray<NSString *> * paths = NSSearchPathForDirectoriesInDomains (NSDocumentDirectory, NSUserDomainMask, YES);
    NSString * first = paths.firstObject;
    return g_strdup (first.UTF8String);
  }
#else
  return NULL;
#endif
}

gboolean
frida_gadget_environment_has_objc_class (const gchar * name)
{
  return objc_getClass (name) != NULL;
}

void
frida_gadget_environment_set_thread_name (const gchar * name)
{
  /* For now only implemented on i/macOS as Fruity.Injector relies on it there. */
  pthread_setname_np (name);
}

void
frida_gadget_environment_detect_darwin_location_fields (GumAddress our_address, gchar ** executable_name, gchar ** our_path,
    GumMemoryRange ** our_range)
{
  mach_port_t task;
  GumDarwinModuleResolver * resolver;
  GPtrArray * modules;
  guint i;

  task = mach_task_self ();

  resolver = gum_darwin_module_resolver_new (task, NULL);
  if (resolver == NULL)
    return;

  gum_darwin_module_resolver_fetch_modules (resolver, &modules, NULL);

  for (i = 0; i != modules->len; i++)
  {
    GumModule * module;
    const GumMemoryRange * range;

    module = g_ptr_array_index (modules, i);
    range = gum_module_get_range (module);

    if (*executable_name == NULL)
    {
      gum_mach_header_t * header = GSIZE_TO_POINTER (range->base_address);
      if (header->filetype == MH_EXECUTE)
        *executable_name = g_strdup (gum_module_get_name (module));
    }

    if (our_address >= range->base_address && our_address < range->base_address + range->size)
    {
      if (*our_path == NULL)
        *our_path = g_strdup (gum_module_get_path (module));

      if (*our_range == NULL)
        *our_range = gum_memory_range_copy (range);
    }

    if (*executable_name != NULL && *our_path != NULL && *our_range != NULL)
      break;
  }

  g_ptr_array_unref (modules);

  g_object_unref (resolver);
}

void
frida_gadget_environment_ensure_debugger_breakpoints_only (void)
{
  task_set_exception_ports (
      mach_task_self (),
      EXC_MASK_ALL & ~EXC_MASK_BREAKPOINT,
      MACH_PORT_NULL,
      EXCEPTION_DEFAULT,
      THREAD_STATE_NONE
  );

  if (gum_process_get_code_signing_policy () == GUM_CODE_SIGNING_OPTIONAL &&
      frida_dont_steal_my_breakpoints == NULL &&
      gum_darwin_is_debugger_mapping_enforced ())
  {
    gpointer exceptions_set, exceptions_swap;
    GumInterceptor * interceptor;

    exceptions_set = &task_set_exception_ports;
    exceptions_swap = &task_swap_exception_ports;

    frida_dont_steal_my_breakpoints = gum_make_call_listener (frida_on_breakpoints_steal_attempt,
      NULL, NULL, NULL);

    interceptor = gum_interceptor_obtain ();

    gum_interceptor_attach (interceptor, exceptions_set, frida_dont_steal_my_breakpoints, NULL);
    gum_interceptor_attach (interceptor, exceptions_swap, frida_dont_steal_my_breakpoints, NULL);

    g_object_unref (interceptor);
  }
}

void
frida_gadget_environment_allow_stolen_breakpoints (void)
{
  GumInterceptor * interceptor;

  if (frida_dont_steal_my_breakpoints == NULL)
    return;

  interceptor = gum_interceptor_obtain ();

  gum_interceptor_detach (interceptor, frida_dont_steal_my_breakpoints);

  g_object_unref (frida_dont_steal_my_breakpoints);
  frida_dont_steal_my_breakpoints = NULL;

  g_object_unref (interceptor);
}

static void
frida_on_breakpoints_steal_attempt (GumInvocationContext * ic, gpointer user_data)
{
  exception_mask_t exception_mask;

  exception_mask = GPOINTER_TO_SIZE (gum_invocation_context_get_nth_argument (ic, 1));
  exception_mask &= ~EXC_MASK_BREAKPOINT;
  gum_invocation_context_replace_nth_argument (ic, 1, GSIZE_TO_POINTER (exception_mask));
}

void
frida_gadget_environment_break_and_resume (void)
{
#ifdef HAVE_ARM64
  asm volatile (
      "mov x1, #1337\n\t"
      "mov x2, #1337\n\t"
      "mov x3, %0\n\t"
      "brk #1337\n\t"
      :
      : "r" ((gsize) FRIDA_GADGET_BREAKPOINT_ACTION_RESUME)
      : "x1", "x2", "x3"
  );
#else
  g_assert_not_reached ();
#endif
}

void
frida_gadget_environment_break_and_detach (void)
{
#ifdef HAVE_ARM64
  asm volatile (
      "mov x1, #1337\n\t"
      "mov x2, #1337\n\t"
      "mov x3, %0\n\t"
      "brk #1337\n\t"
      :
      : "r" ((gsize) FRIDA_GADGET_BREAKPOINT_ACTION_DETACH)
      : "x1", "x2", "x3"
  );
#else
  g_assert_not_reached ();
#endif
}
