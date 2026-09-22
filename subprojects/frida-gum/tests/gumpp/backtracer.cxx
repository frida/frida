/*
 * Copyright (C) 2011-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumpp.hpp"

#include "testutil.h"

G_BEGIN_DECLS

#define TESTCASE(NAME) \
    void test_gumpp_backtracer_ ## NAME (void)
#define TESTENTRY(NAME) \
    TESTENTRY_SIMPLE ("Gum++/Backtracer", test_gumpp_backtracer, NAME)

TESTLIST_BEGIN (gumpp_backtracer)
  TESTENTRY (can_get_stack_trace_from_invocation_context)
TESTLIST_END ()

static gpointer gumpp_test_target_function (GString * str);

class BacktraceTestListener : public Gum::InvocationListener
{
public:
  BacktraceTestListener ()
    : backtracer (Gum::Backtracer_make_accurate ())
  {
  }

  virtual void on_enter (Gum::InvocationContext * context)
  {
    g_string_append_c (static_cast<GString *> (
        context->get_listener_function_data_ptr ()), '>');

    Gum::ReturnAddressArray return_addresses;
    backtracer->generate (context->get_cpu_context (), return_addresses);
    g_assert_cmpuint (return_addresses.len, >=, 1);

#if !defined (HAVE_DARWIN) && !defined (HAVE_ANDROID)
    Gum::ReturnAddress first_address = return_addresses.items[0];
    Gum::ReturnAddressDetails rad;
    g_assert_true (Gum::ReturnAddressDetails_from_address (first_address, rad));
    g_assert_true (g_str_has_suffix (rad.function_name,
        "_can_get_stack_trace_from_invocation_context"));
    gchar * file_basename = g_path_get_basename (rad.file_name);
    g_assert_cmpstr (file_basename, ==, "backtracer.cxx");
    g_free (file_basename);
#endif
  }

  virtual void on_leave (Gum::InvocationContext * context)
  {
    g_string_append_c (static_cast<GString *> (
        context->get_listener_function_data_ptr ()), '<');
  }

  Gum::RefPtr<Gum::Backtracer> backtracer;
};

TESTCASE (can_get_stack_trace_from_invocation_context)
{
  GumBacktracer * backtracer = gum_backtracer_make_accurate ();
  if (backtracer == NULL)
  {
    g_print ("<skipping, no backtracer support> ");
    return;
  }
  g_object_unref (backtracer);
  backtracer = NULL;

  Gum::RefPtr<Gum::Interceptor> interceptor (Gum::Interceptor_obtain ());

  BacktraceTestListener listener;

  GString * output = g_string_new ("");
  interceptor->attach (reinterpret_cast<void *> (gumpp_test_target_function),
      &listener, output);

  gumpp_test_target_function (output);
  g_assert_cmpstr (output->str, ==, ">|<");

  g_string_free (output, TRUE);

  interceptor->detach (&listener);
}

GUM_HOOK_TARGET static gpointer
gumpp_test_target_function (GString * str)
{
  g_string_append_c (str, '|');

  return NULL;
}

G_END_DECLS
