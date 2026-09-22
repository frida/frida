/*
 * Copyright (C) 2008-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2008 Christian Berentsen <jc.berentsen@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "testutil.h"

#ifdef HAVE_DARWIN
# include "tests/stubs/objc/dummyclass.h"
# include <objc/runtime.h>
#endif

#define TESTCASE(NAME) \
    void test_symbolutil_ ## NAME (void)
#define TESTENTRY(NAME) \
    TESTENTRY_SIMPLE ("Core/SymbolUtil", test_symbolutil, NAME)

TESTLIST_BEGIN (symbolutil)
  TESTENTRY (symbol_details_from_address)
  TESTENTRY (symbol_details_from_address_objc_fallback)
  TESTENTRY (symbol_name_from_address)
  TESTENTRY (find_external_public_function)
  TESTENTRY (find_local_static_function)
  TESTENTRY (find_functions_named)
  TESTENTRY (find_functions_matching)
TESTLIST_END ()

#ifdef HAVE_LINUX
static guint gum_dummy_variable;
#endif

static void GUM_CDECL gum_dummy_function_0 (void);
static void GUM_STDCALL gum_dummy_function_1 (void);

TESTCASE (symbol_details_from_address)
{
  GumDebugSymbolDetails details;

  g_assert_true (gum_symbol_details_from_address (gum_dummy_function_0,
      &details));
  g_assert_cmphex (GPOINTER_TO_SIZE (details.address), ==,
      GPOINTER_TO_SIZE (gum_dummy_function_0));
  g_assert_true (g_str_has_prefix (details.module_name, "gum-tests"));
  g_assert_cmpstr (details.symbol_name, ==, "gum_dummy_function_0");
#ifndef HAVE_IOS
  assert_basename_equals (__FILE__, details.file_name);
  g_assert_cmpuint (details.line_number, >, 0);
#endif

#ifdef HAVE_LINUX
  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }
  g_assert_true (gum_symbol_details_from_address (&gum_dummy_variable,
      &details));
  g_assert_cmphex (GPOINTER_TO_SIZE (details.address), ==,
      GPOINTER_TO_SIZE (&gum_dummy_variable));
  g_assert_true (g_str_has_prefix (details.module_name, "gum-tests"));
  g_assert_cmpuint (details.symbol_name[0], ==, '0');
  g_assert_cmpuint (details.symbol_name[1], ==, 'x');
#endif
}

TESTCASE (symbol_details_from_address_objc_fallback)
{
#ifdef HAVE_DARWIN
  GumDebugSymbolDetails details;
  void * mid_function = dummy_class_get_dummy_method_impl () + 1;
  g_assert_true (gum_symbol_details_from_address (mid_function, &details));
  g_assert_cmpstr (details.symbol_name, ==, "-[DummyClass dummyMethod:]");
#else
  g_print ("<skipping, not available> ");
#endif
}

TESTCASE (symbol_name_from_address)
{
  gchar * symbol_name;

  symbol_name = gum_symbol_name_from_address (gum_dummy_function_1);
  g_assert_cmpstr (symbol_name, ==, "gum_dummy_function_1");
  g_free (symbol_name);
}

TESTCASE (find_external_public_function)
{
  g_assert_nonnull (gum_find_function ("g_thread_new"));
}

TESTCASE (find_local_static_function)
{
  gpointer function_address;

  function_address = gum_find_function ("gum_dummy_function_0");
  g_assert_cmphex (GPOINTER_TO_SIZE (function_address), ==,
      GPOINTER_TO_SIZE (gum_dummy_function_0));
}

TESTCASE (find_functions_named)
{
  GArray * functions;

  functions = gum_find_functions_named ("g_thread_new");
  g_assert_cmpuint (functions->len, >=, 1);
  g_array_free (functions, TRUE);
}

TESTCASE (find_functions_matching)
{
  GArray * functions;
  gpointer a, b;

  functions = gum_find_functions_matching ("gum_dummy_function_*");
  g_assert_cmpuint (functions->len, ==, 2);

  a = g_array_index (functions, gpointer, 0);
  b = g_array_index (functions, gpointer, 1);
  if (a != gum_dummy_function_0)
  {
    gpointer hold = a;

    a = b;
    b = hold;
  }

  g_assert_cmphex (GPOINTER_TO_SIZE (a),
      ==, GPOINTER_TO_SIZE (gum_dummy_function_0));
  g_assert_cmphex (GPOINTER_TO_SIZE (b),
      ==, GPOINTER_TO_SIZE (gum_dummy_function_1));

  g_array_free (functions, TRUE);
}

static void GUM_CDECL
gum_dummy_function_0 (void)
{
  g_print ("%s\n", G_STRFUNC);
}

static void GUM_STDCALL
gum_dummy_function_1 (void)
{
  g_print ("%s\n", G_STRFUNC);
}

