/*
 * Copyright (C) 2016-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2023 Håvard Sørbø <havard@hsorbo.no>
 * Copyright (C) 2025 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "apiresolver-fixture.c"

TESTLIST_BEGIN (api_resolver)
  TESTENTRY (module_exports_can_be_resolved_case_sensitively)
  TESTENTRY (module_exports_can_be_resolved_case_insensitively)
  TESTENTRY (module_imports_can_be_resolved)
  TESTENTRY (module_sections_can_be_resolved)
  TESTENTRY (objc_methods_can_be_resolved_case_sensitively)
  TESTENTRY (objc_methods_can_be_resolved_case_insensitively)
#ifdef HAVE_DARWIN
  TESTENTRY (objc_method_can_be_resolved_from_class_method_address)
  TESTENTRY (objc_method_can_be_resolved_from_instance_method_address)
  TESTENTRY (swift_method_can_be_resolved)
#endif
#ifdef HAVE_ANDROID
  TESTENTRY (linker_exports_can_be_resolved_on_android)
#endif
TESTLIST_END ()

TESTCASE (module_exports_can_be_resolved_case_sensitively)
{
  TestForEachContext ctx;
  GError * error = NULL;
#ifdef HAVE_WINDOWS
  const gchar * query = "exports:*!_open*";
#else
  const gchar * query = "exports:*!open*";
#endif

  fixture->resolver = gum_api_resolver_make ("module");
  g_assert_nonnull (fixture->resolver);

  ctx.number_of_calls = 0;
  ctx.value_to_return = TRUE;
  gum_api_resolver_enumerate_matches (fixture->resolver, query, match_found_cb,
      &ctx, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (ctx.number_of_calls, >, 1);

  ctx.number_of_calls = 0;
  ctx.value_to_return = FALSE;
  gum_api_resolver_enumerate_matches (fixture->resolver, query, match_found_cb,
      &ctx, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (ctx.number_of_calls, ==, 1);
}

TESTCASE (module_exports_can_be_resolved_case_insensitively)
{
  TestForEachContext ctx;
  GError * error = NULL;
#ifdef HAVE_WINDOWS
  const gchar * query = "exports:*!_OpEn*/i";
#else
  const gchar * query = "exports:*!OpEn*/i";
#endif

  fixture->resolver = gum_api_resolver_make ("module");
  g_assert_nonnull (fixture->resolver);

  ctx.number_of_calls = 0;
  ctx.value_to_return = TRUE;
  gum_api_resolver_enumerate_matches (fixture->resolver, query, match_found_cb,
      &ctx, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (ctx.number_of_calls, >, 1);
}

TESTCASE (module_imports_can_be_resolved)
{
#ifdef HAVE_DARWIN
  GError * error = NULL;
  const gchar * query = "imports:gum-tests!*";
  guint number_of_imports_seen = 0;

  fixture->resolver = gum_api_resolver_make ("module");
  g_assert_nonnull (fixture->resolver);

  gum_api_resolver_enumerate_matches (fixture->resolver, query,
      check_module_import, &number_of_imports_seen, &error);
  g_assert_no_error (error);
#else
  (void) check_module_import;
#endif
}

static gboolean
check_module_import (const GumApiDetails * details,
                     gpointer user_data)
{
  guint * number_of_imports_seen = user_data;

  g_assert_null (strstr (details->name, "gum-tests"));

  (*number_of_imports_seen)++;

  return TRUE;
}

TESTCASE (module_sections_can_be_resolved)
{
#if defined (HAVE_DARWIN) || defined (HAVE_ELF)
  GError * error = NULL;
  const gchar * query = "sections:gum-tests!*data*";
  guint number_of_sections_seen = 0;

  fixture->resolver = gum_api_resolver_make ("module");
  g_assert_nonnull (fixture->resolver);

  gum_api_resolver_enumerate_matches (fixture->resolver, query, check_section,
      &number_of_sections_seen, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (number_of_sections_seen, >, 1);
#else
  (void) check_section;
#endif
}

static gboolean
check_section (const GumApiDetails * details,
               gpointer user_data)
{
  guint * number_of_sections_seen = user_data;

  g_assert_nonnull (strstr (details->name, "data"));

  (*number_of_sections_seen)++;

  return TRUE;
}

TESTCASE (objc_methods_can_be_resolved_case_sensitively)
{
  TestForEachContext ctx;
  GError * error = NULL;

  fixture->resolver = gum_api_resolver_make ("objc");
  if (fixture->resolver == NULL)
  {
    g_print ("<skipping, not available> ");
    return;
  }

  ctx.number_of_calls = 0;
  ctx.value_to_return = TRUE;
  gum_api_resolver_enumerate_matches (fixture->resolver, "+[*Arr* arr*]",
      match_found_cb, &ctx, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (ctx.number_of_calls, >, 1);

  ctx.number_of_calls = 0;
  ctx.value_to_return = FALSE;
  gum_api_resolver_enumerate_matches (fixture->resolver, "+[*Arr* arr*]",
      match_found_cb, &ctx, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (ctx.number_of_calls, ==, 1);
}

TESTCASE (objc_methods_can_be_resolved_case_insensitively)
{
  TestForEachContext ctx;
  GError * error = NULL;

  fixture->resolver = gum_api_resolver_make ("objc");
  if (fixture->resolver == NULL)
  {
    g_print ("<skipping, not available> ");
    return;
  }

  ctx.number_of_calls = 0;
  ctx.value_to_return = TRUE;
  gum_api_resolver_enumerate_matches (fixture->resolver, "+[*Arr* aRR*]/i",
      match_found_cb, &ctx, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (ctx.number_of_calls, >, 1);
}

static gboolean
match_found_cb (const GumApiDetails * details,
                gpointer user_data)
{
  TestForEachContext * ctx = (TestForEachContext *) user_data;

  ctx->number_of_calls++;

  return ctx->value_to_return;
}

#ifdef HAVE_DARWIN

static gboolean resolve_method_impl (const GumApiDetails * details,
    gpointer user_data);
static gboolean accumulate_matches (const GumApiDetails * details,
    gpointer user_data);

TESTCASE (objc_method_can_be_resolved_from_class_method_address)
{
  GumAddress address;
  GumModule * address_module;
  gchar * method = NULL;
  GError * error = NULL;

  fixture->resolver = gum_api_resolver_make ("objc");
  if (fixture->resolver == NULL)
  {
    g_print ("<skipping, not available> ");
    return;
  }

  gum_api_resolver_enumerate_matches (fixture->resolver, "+[NSArray array]",
      resolve_method_impl, &address, &error);
  g_assert_no_error (error);

  address_module = gum_process_find_module_by_address (address);
  g_assert_nonnull (address_module);

  method = _gum_objc_api_resolver_find_method_by_address (fixture->resolver,
      address, address_module);
  g_assert_nonnull (method);
  g_free (method);
}

TESTCASE (objc_method_can_be_resolved_from_instance_method_address)
{
  GumAddress address;
  GumModule * address_module;
  gchar * method = NULL;
  GError * error = NULL;

  fixture->resolver = gum_api_resolver_make ("objc");
  if (fixture->resolver == NULL)
  {
    g_print ("<skipping, not available> ");
    return;
  }

  gum_api_resolver_enumerate_matches (fixture->resolver,
      "-[NSArray initWithArray:]", resolve_method_impl, &address, &error);
  g_assert_no_error (error);

  address_module = gum_process_find_module_by_address (address);
  g_assert_nonnull (address_module);

  method = _gum_objc_api_resolver_find_method_by_address (fixture->resolver,
      address, address_module);
  g_assert_nonnull (method);
  g_free (method);
}

TESTCASE (swift_method_can_be_resolved)
{
  guint num_matches;
  GError * error = NULL;

  fixture->resolver = gum_api_resolver_make ("swift");

  num_matches = 0;
  gum_api_resolver_enumerate_matches (fixture->resolver,
      "functions:*!*", accumulate_matches, &num_matches, &error);
  if (g_error_matches (error, GUM_ERROR, GUM_ERROR_NOT_SUPPORTED))
    goto not_supported;
  g_assert_no_error (error);

  return;

not_supported:
  {
    g_print ("<skipping, not available> ");

    g_error_free (error);
  }
}

static gboolean
resolve_method_impl (const GumApiDetails * details,
                     gpointer user_data)
{
  GumAddress * address = user_data;

  *address = details->address;

  return FALSE;
}

static gboolean
accumulate_matches (const GumApiDetails * details,
                    gpointer user_data)
{
  guint * total = user_data;

  (*total)++;

  return TRUE;
}

#endif

#ifdef HAVE_ANDROID

typedef struct _TestLinkerExportsContext TestLinkerExportsContext;

struct _TestLinkerExportsContext
{
  guint number_of_calls;

  gchar * expected_name;
  GumAddress expected_address;
};

static gboolean check_linker_export (const GumApiDetails * details,
    gpointer user_data);

TESTCASE (linker_exports_can_be_resolved_on_android)
{
  const gchar * linker_name, * libdl_name;
  const gchar * linker_exports[] =
  {
    "dlopen",
    "dlsym",
    "dlclose",
    "dlerror",
  };
  GumModule * correct_module, * incorrect_module;
  guint i;

  if (gum_android_get_api_level () >= 29)
  {
    linker_name = (sizeof (gpointer) == 4)
        ? "/apex/com.android.runtime/bin/linker"
        : "/apex/com.android.runtime/bin/linker64";
    libdl_name = (sizeof (gpointer) == 4)
        ? "/apex/com.android.runtime/lib/bionic/libdl.so"
        : "/apex/com.android.runtime/lib64/bionic/libdl.so";
  }
  else
  {
    linker_name = (sizeof (gpointer) == 4)
        ? "/system/bin/linker"
        : "/system/bin/linker64";
    libdl_name = (sizeof (gpointer) == 4)
        ? "/system/lib/libdl.so"
        : "/system/lib64/libdl.so";
  }

  if (gum_android_get_api_level () >= 26)
  {
    correct_module = gum_process_find_module_by_name (libdl_name);
    incorrect_module = gum_process_find_module_by_name (linker_name);
  }
  else
  {
    correct_module = gum_process_find_module_by_name (linker_name);
    incorrect_module = gum_process_find_module_by_name (libdl_name);
  }

  fixture->resolver = gum_api_resolver_make ("module");
  g_assert_nonnull (fixture->resolver);

  for (i = 0; i != G_N_ELEMENTS (linker_exports); i++)
  {
    const gchar * func_name = linker_exports[i];
    gchar * query;
    TestLinkerExportsContext ctx;
    GError * error = NULL;

    query = g_strconcat ("exports:*!", func_name, NULL);

    g_assert_true (
        gum_module_find_export_by_name (incorrect_module, func_name) == 0);

    ctx.number_of_calls = 0;
    ctx.expected_name = g_strdup_printf ("%s!%s",
        gum_module_get_name (correct_module),
        func_name);
    ctx.expected_address =
        gum_module_find_export_by_name (correct_module, func_name);
    g_assert_cmpuint (ctx.expected_address, !=, 0);

    gum_api_resolver_enumerate_matches (fixture->resolver, query,
        check_linker_export, &ctx, &error);
    g_assert_no_error (error);
    g_assert_cmpuint (ctx.number_of_calls, >=, 1);

    g_free (ctx.expected_name);

    g_free (query);
  }

  g_object_unref (incorrect_module);
  g_object_unref (correct_module);
}

static gboolean
check_linker_export (const GumApiDetails * details,
                     gpointer user_data)
{
  TestLinkerExportsContext * ctx = (TestLinkerExportsContext *) user_data;

  g_assert_cmpstr (details->name, ==, ctx->expected_name);
  g_assert_cmphex (details->address, ==, ctx->expected_address);

  ctx->number_of_calls++;

  return TRUE;
}

#endif
