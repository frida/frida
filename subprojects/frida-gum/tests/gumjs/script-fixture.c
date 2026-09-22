/*
 * Copyright (C) 2010-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2013 Karl Trygve Kalleberg <karltk@boblycat.org>
 * Copyright (C) 2020-2021 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2020 Marcus Mengs <mame8282@googlemail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "testutil.h"

#include "gum-init.h"
#include "guminspectorserver.h"
#include "gumquickscriptbackend.h"
#include "gumscriptbackend.h"
#include "gumusertimesampler.h"
#include "valgrind.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#ifdef HAVE_WINDOWS
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <intrin.h>
# include <windows.h>
# include <winsock2.h>
# include <ws2tcpip.h>
#else
# include <errno.h>
# include <fcntl.h>
# include <netinet/in.h>
# include <sys/socket.h>
# include <sys/un.h>
# include <unistd.h>
#endif
#if HAVE_DARWIN
# include <mach/mach.h>
# include <sys/sysctl.h>
#endif
#ifdef HAVE_QNX
# include <unix.h>
#endif

#define ANY_LINE_NUMBER -1
#define SCRIPT_MESSAGE_DEFAULT_TIMEOUT_MSEC 500
#define NUM_THREADS 10

#ifndef SCRIPT_SUITE
# define SCRIPT_SUITE ""
#endif
#define TESTCASE(NAME) \
    void test_script_ ## NAME (TestScriptFixture * fixture, gconstpointer data)
#define TESTENTRY(NAME)                                                   \
    G_STMT_START                                                          \
    {                                                                     \
      extern void test_script_ ##NAME (TestScriptFixture * fixture,       \
          gconstpointer data);                                            \
      gchar * path;                                                       \
                                                                          \
      path = g_strconcat ("/GumJS/Script/" SCRIPT_SUITE, group, #NAME "#",\
          GUM_QUICK_IS_SCRIPT_BACKEND (fixture_data) ? "QJS" : "V8",      \
          NULL);                                                          \
                                                                          \
      g_test_add (path,                                                   \
          TestScriptFixture,                                              \
          fixture_data,                                                   \
          test_script_fixture_setup,                                      \
          test_script_ ##NAME,                                            \
          test_script_fixture_teardown);                                  \
                                                                          \
      g_free (path);                                                      \
    }                                                                     \
    G_STMT_END;

#define COMPILE_AND_LOAD_SCRIPT(SOURCE, ...) \
    test_script_fixture_compile_and_load_script (fixture, SOURCE, \
    ## __VA_ARGS__)
#define UNLOAD_SCRIPT() \
    gum_script_unload_sync (fixture->script, NULL); \
    g_object_unref (fixture->script); \
    fixture->script = NULL;
#define POST_MESSAGE(MSG) \
    gum_script_post (fixture->script, MSG, NULL)
#define EXPECT_NO_MESSAGES() \
    g_assert_null (test_script_fixture_try_pop_message (fixture, 1))
#define EXPECT_SEND_MESSAGE_WITH(PAYLOAD, ...) \
    test_script_fixture_expect_send_message_with (fixture, G_STRFUNC, \
        __FILE__, __LINE__, PAYLOAD, ## __VA_ARGS__)
#define EXPECT_SEND_MESSAGE_WITH_PREFIX(PREFIX, ...) \
    test_script_fixture_expect_send_message_with_prefix (fixture, G_STRFUNC, \
        __FILE__, __LINE__, PREFIX, ## __VA_ARGS__)
#define EXPECT_SEND_MESSAGE_WITH_PAYLOAD_AND_DATA(PAYLOAD, DATA) \
    test_script_fixture_expect_send_message_with_payload_and_data (fixture, \
        G_STRFUNC, __FILE__, __LINE__, PAYLOAD, DATA)
#define EXPECT_SEND_MESSAGE_WITH_POINTER() \
    test_script_fixture_expect_send_message_with_pointer (fixture, G_STRFUNC, \
        __FILE__, __LINE__)
#define EXPECT_ERROR_MESSAGE_WITH(LINE_NUMBER, DESC) \
    test_script_fixture_expect_error_message_with (fixture, G_STRFUNC, \
        __FILE__, __LINE__, LINE_NUMBER, DESC)
#define EXPECT_ERROR_MESSAGE_MATCHING(LINE_NUMBER, PATTERN) \
    test_script_fixture_expect_error_message_matching (fixture, G_STRFUNC, \
        __FILE__, __LINE__, LINE_NUMBER, PATTERN)
#define EXPECT_LOG_MESSAGE_WITH(LEVEL, PAYLOAD, ...) \
    test_script_fixture_expect_log_message_with (fixture, G_STRFUNC, \
        __FILE__, __LINE__, LEVEL, PAYLOAD, ## __VA_ARGS__)
#define PUSH_TIMEOUT(value) test_script_fixture_push_timeout (fixture, value)
#define POP_TIMEOUT() test_script_fixture_pop_timeout (fixture)
#define DISABLE_LOG_MESSAGE_HANDLING() \
    fixture->enable_log_message_handling = FALSE
#define MAKE_TEMPFILE_CONTAINING(str) \
    test_script_fixture_make_tempfile_containing (fixture, str)
#define ESCAPE_PATH(path) \
    test_script_fixture_escape_path (fixture, path)
#define test_script_fixture_pop_message(fixture) \
    _test_script_fixture_pop_message (fixture, G_STRFUNC, __FILE__, __LINE__)

#define GUM_PTR_CONST "ptr(\"0x%" G_GSIZE_MODIFIER "x\")"

#define gum_assert_cmpstr(func, file, line, s1, cmp, s2) \
    G_STMT_START \
    { \
      const char * __s1 = (s1), * __s2 = (s2); \
      if (g_strcmp0 (__s1, __s2) cmp 0) ; else \
        g_assertion_message_cmpstr (G_LOG_DOMAIN, file, line, func, \
            #s1 " " #cmp " " #s2, __s1, #cmp, __s2); \
    } \
    G_STMT_END
#define gum_assert_true(func, file, line, expr) \
    G_STMT_START \
    { \
      if G_LIKELY (expr) ; else \
        g_assertion_message (G_LOG_DOMAIN, file, line, func, \
            "'" #expr "' should be TRUE"); \
    } \
    G_STMT_END
#define gum_assert_false(func, file, line, expr) \
    G_STMT_START \
    { \
      if G_LIKELY (!(expr)) ; else \
        g_assertion_message (G_LOG_DOMAIN, file, line, func, \
            "'" #expr "' should be FALSE"); \
    } \
    G_STMT_END
#define gum_assert_null(func, file, line, expr) \
    G_STMT_START \
    { \
      if G_LIKELY ((expr) == NULL) ; else \
        g_assertion_message (G_LOG_DOMAIN, file, line, func, \
            "'" #expr "' should be NULL"); \
    } \
    G_STMT_END
#define gum_assert_nonnull(func, file, line, expr) \
    G_STMT_START \
    { \
      if G_LIKELY ((expr) != NULL) ; else \
        g_assertion_message (G_LOG_DOMAIN, file, line, func, \
            "'" #expr "' should not be NULL"); \
    } \
    G_STMT_END
#define gum_assert_cmpint(func, file, line, n1, cmp, n2) \
    G_STMT_START \
    { \
      gint64 __n1 = (n1), __n2 = (n2); \
      if (__n1 cmp __n2) ; else \
        g_assertion_message_cmpnum (G_LOG_DOMAIN, file, line, func, \
            #n1 " " #cmp " " #n2, (long double) __n1, #cmp,\
            (long double) __n2, 'i'); \
    } \
    G_STMT_END

#ifdef HAVE_WINDOWS
# define GUM_CLOSE_SOCKET(s) closesocket (s)
#else
# define GUM_CLOSE_SOCKET(s) close (s)
#endif

#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
# define GUM_RETURN_VALUE_REGISTER_NAME "eax"
#elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
# define GUM_RETURN_VALUE_REGISTER_NAME "rax"
#elif defined (HAVE_ARM)
# define GUM_RETURN_VALUE_REGISTER_NAME "r0"
#elif defined (HAVE_ARM64)
# define GUM_RETURN_VALUE_REGISTER_NAME "x0"
#elif defined (HAVE_MIPS)
# define GUM_RETURN_VALUE_REGISTER_NAME "v0"
#else
# error Unsupported architecture
#endif

typedef struct _TestScriptFixture TestScriptFixture;
typedef struct _TestScriptMessageItem TestScriptMessageItem;

struct _TestScriptFixture
{
  GumScriptBackend * backend;
  GumScript * script;
  GMainLoop * loop;
  GMainContext * context;
  GQueue messages;
  GQueue timeouts;
  GQueue tempfiles;
  GQueue heap_blocks;
  gboolean enable_log_message_handling;
};

struct _TestScriptMessageItem
{
  gchar * message;
  gchar * data;
  GBytes * raw_data;
};

static void test_script_message_item_free (TestScriptMessageItem * item);
static gboolean test_script_fixture_try_handle_log_message (
    TestScriptFixture * self, const gchar * raw_message);
static TestScriptMessageItem * test_script_fixture_try_pop_message (
    TestScriptFixture * fixture, guint timeout);
static gboolean test_script_fixture_stop_loop (TestScriptFixture * fixture);
static void test_script_fixture_expect_send_message_with_prefix (
    TestScriptFixture * fixture, const gchar * func, const gchar * file,
    gint line, const gchar * prefix_template, ...);
static void test_script_fixture_expect_send_message_with_payload_and_data (
    TestScriptFixture * fixture, const gchar * func, const gchar * file,
    gint line, const gchar * payload, const gchar * data);
static void test_script_fixture_expect_error_message_with (
    TestScriptFixture * fixture, const gchar * func, const gchar * file,
    gint line, gint line_number, const gchar * description);
static void test_script_fixture_expect_error_message_matching (
    TestScriptFixture * fixture, const gchar * func, const gchar * file,
    gint line, gint line_number, const gchar * pattern);
static void test_script_fixture_expect_log_message_with (
    TestScriptFixture * fixture, const gchar * func, const gchar * file,
    gint line, const gchar * level, const gchar * payload_template, ...);
static void test_script_fixture_push_timeout (TestScriptFixture * fixture,
    guint timeout);
static void test_script_fixture_pop_timeout (TestScriptFixture * fixture);

static GumExceptor * exceptor = NULL;

static void
test_script_fixture_deinit (void)
{
  g_object_unref (exceptor);
  exceptor = NULL;
}

static void
test_script_fixture_setup (TestScriptFixture * fixture,
                           gconstpointer data)
{
  (void) test_script_fixture_expect_send_message_with_prefix;
  (void) test_script_fixture_expect_send_message_with_payload_and_data;
  (void) test_script_fixture_expect_error_message_with;
  (void) test_script_fixture_expect_error_message_matching;
  (void) test_script_fixture_expect_log_message_with;
  (void) test_script_fixture_pop_timeout;

  fixture->backend = (GumScriptBackend *) data;
  fixture->context = g_main_context_ref_thread_default ();
  fixture->loop = g_main_loop_new (fixture->context, FALSE);
  g_queue_init (&fixture->messages);
  g_queue_init (&fixture->timeouts);
  g_queue_init (&fixture->tempfiles);
  g_queue_init (&fixture->heap_blocks);
  fixture->enable_log_message_handling = TRUE;

  test_script_fixture_push_timeout (fixture,
      SCRIPT_MESSAGE_DEFAULT_TIMEOUT_MSEC);

  if (exceptor == NULL)
  {
    exceptor = gum_exceptor_obtain ();
    _gum_register_destructor (test_script_fixture_deinit);
  }
}

static void
test_script_fixture_teardown (TestScriptFixture * fixture,
                              gconstpointer data)
{
  gchar * path;
  TestScriptMessageItem * item;

  if (fixture->script != NULL)
  {
    gum_script_unload_sync (fixture->script, NULL);
    g_object_unref (fixture->script);
  }

  while (g_main_context_pending (fixture->context))
    g_main_context_iteration (fixture->context, FALSE);

  g_queue_clear_full (&fixture->heap_blocks, g_free);

  while ((path = g_queue_pop_tail (&fixture->tempfiles)) != NULL)
  {
    g_unlink (path);
    g_free (path);
  }

  while ((item = test_script_fixture_try_pop_message (fixture, 1)) != NULL)
  {
    test_script_message_item_free (item);
  }

  g_queue_clear (&fixture->timeouts);

  g_main_loop_unref (fixture->loop);
  g_main_context_unref (fixture->context);
}

static void
test_script_message_item_free (TestScriptMessageItem * item)
{
  g_free (item->message);
  g_free (item->data);
  g_bytes_unref (item->raw_data);
  g_slice_free (TestScriptMessageItem, item);
}

static void
test_script_fixture_store_message (const gchar * message,
                                   GBytes * data,
                                   gpointer user_data)
{
  TestScriptFixture * self = (TestScriptFixture *) user_data;
  TestScriptMessageItem * item;

  if (test_script_fixture_try_handle_log_message (self, message))
    return;

  item = g_slice_new (TestScriptMessageItem);
  item->message = g_strdup (message);

  if (data != NULL)
  {
    const guint8 * data_elements;
    gsize data_size, i;
    GString * s;

    data_elements = g_bytes_get_data (data, &data_size);

    s = g_string_sized_new (3 * data_size);
    for (i = 0; i != data_size; i++)
    {
      if (i != 0)
        g_string_append_c (s, ' ');
      g_string_append_printf (s, "%02x", (int) data_elements[i]);
    }

    item->data = g_string_free (s, FALSE);
    item->raw_data = g_bytes_ref (data);
  }
  else
  {
    item->data = NULL;
    item->raw_data = NULL;
  }

  g_queue_push_tail (&self->messages, item);
  g_main_loop_quit (self->loop);
}

static gboolean
test_script_fixture_try_handle_log_message (TestScriptFixture * self,
                                            const gchar * raw_message)
{
  gboolean handled = FALSE;
  JsonNode * message;
  JsonReader * reader;
  const gchar * text;
  const gchar * level;
  guint color = 37;

  if (!self->enable_log_message_handling)
    return FALSE;

  message = json_from_string (raw_message, NULL);
  reader = json_reader_new (message);
  json_node_unref (message);

  json_reader_read_member (reader, "type");
  if (strcmp (json_reader_get_string_value (reader), "log") != 0)
    goto beach;
  json_reader_end_member (reader);

  json_reader_read_member (reader, "payload");
  text = json_reader_get_string_value (reader);
  json_reader_end_member (reader);

  json_reader_read_member (reader, "level");
  level = json_reader_get_string_value (reader);
  json_reader_end_member (reader);
  if (strcmp (level, "info") == 0)
    color = 36;
  else if (strcmp (level, "warning") == 0)
    color = 33;
  else if (strcmp (level, "error") == 0)
    color = 31;
  else
    g_assert_not_reached ();

  g_printerr (
      "\033[0;%um"
      "%s"
      "\033[0m"
      "\n",
      color, text);

  handled = TRUE;

beach:
  g_object_unref (reader);

  return handled;
}

static void
test_script_fixture_compile_and_load_script (TestScriptFixture * fixture,
                                             const gchar * source_template,
                                             ...)
{
  va_list args;
  gchar * source;
  GError * err = NULL;

  if (fixture->script != NULL)
  {
    gum_script_unload_sync (fixture->script, NULL);
    g_object_unref (fixture->script);
    fixture->script = NULL;
  }

  va_start (args, source_template);
  source = g_strdup_vprintf (source_template, args);
  va_end (args);

  fixture->script = gum_script_backend_create_sync (fixture->backend,
      "testcase", source, NULL, NULL, &err);
  if (err != NULL)
    g_printerr ("%s\n", err->message);
  g_assert_nonnull (fixture->script);
  g_assert_null (err);

  g_free (source);

  gum_script_set_message_handler (fixture->script,
      test_script_fixture_store_message, fixture, NULL);

  gum_script_load_sync (fixture->script, NULL);
}

static TestScriptMessageItem *
test_script_fixture_try_pop_message (TestScriptFixture * fixture,
                                     guint timeout)
{
  if (g_queue_is_empty (&fixture->messages))
  {
    GSource * source;

    source = g_timeout_source_new (timeout);
    g_source_set_callback (source, (GSourceFunc) test_script_fixture_stop_loop,
        fixture, NULL);
    g_source_attach (source, fixture->context);

    g_main_loop_run (fixture->loop);

    g_source_destroy (source);
    g_source_unref (source);
  }

  return g_queue_pop_head (&fixture->messages);
}

static gboolean
test_script_fixture_stop_loop (TestScriptFixture * fixture)
{
  g_main_loop_quit (fixture->loop);

  return FALSE;
}

static TestScriptMessageItem *
_test_script_fixture_pop_message (TestScriptFixture * fixture,
                                  const gchar * func,
                                  const gchar * file,
                                  gint line)
{
  guint timeout;
  TestScriptMessageItem * item;

  timeout = GPOINTER_TO_UINT (g_queue_peek_tail (&fixture->timeouts));

  item = test_script_fixture_try_pop_message (fixture, timeout);
  gum_assert_nonnull (func, file, line, item);

  return item;
}

static void
test_script_fixture_expect_send_message_with (TestScriptFixture * fixture,
                                              const gchar * func,
                                              const gchar * file,
                                              gint line,
                                              const gchar * payload_template,
                                              ...)
{
  va_list args;
  gchar * payload;
  TestScriptMessageItem * item;
  gchar * expected_message;

  va_start (args, payload_template);
  payload = g_strdup_vprintf (payload_template, args);
  va_end (args);

  item = _test_script_fixture_pop_message (fixture, func, file, line);
  expected_message =
      g_strconcat ("{\"type\":\"send\",\"payload\":", payload, "}", NULL);
  gum_assert_cmpstr (func, file, line, item->message, ==, expected_message);
  test_script_message_item_free (item);
  g_free (expected_message);

  g_free (payload);
}

static void
test_script_fixture_expect_send_message_with_prefix (
    TestScriptFixture * fixture,
    const gchar * func,
    const gchar * file,
    gint line,
    const gchar * prefix_template,
    ...)
{
  va_list args;
  gchar * prefix;
  TestScriptMessageItem * item;
  gchar * expected_message_prefix;

  va_start (args, prefix_template);
  prefix = g_strdup_vprintf (prefix_template, args);
  va_end (args);

  item = _test_script_fixture_pop_message (fixture, func, file, line);
  expected_message_prefix =
      g_strconcat ("{\"type\":\"send\",\"payload\":", prefix, NULL);
  gum_assert_true (func, file, line,
      g_str_has_prefix (item->message, expected_message_prefix));
  test_script_message_item_free (item);
  g_free (expected_message_prefix);

  g_free (prefix);
}

static void
test_script_fixture_expect_send_message_with_payload_and_data (
    TestScriptFixture * fixture,
    const gchar * func,
    const gchar * file,
    gint line,
    const gchar * payload,
    const gchar * data)
{
  TestScriptMessageItem * item;
  gchar * expected_message;

  item = _test_script_fixture_pop_message (fixture, func, file, line);
  expected_message =
      g_strconcat ("{\"type\":\"send\",\"payload\":", payload, "}", NULL);
  gum_assert_cmpstr (func, file, line, item->message, ==, expected_message);
  if (data != NULL)
  {
    gum_assert_nonnull (func, file, line, item->data);
    gum_assert_cmpstr (func, file, line, item->data, ==, data);
  }
  else
  {
    gum_assert_null (func, file, line, item->data);
  }
  test_script_message_item_free (item);
  g_free (expected_message);
}

static gpointer
test_script_fixture_expect_send_message_with_pointer (
    TestScriptFixture * fixture,
    const gchar * func,
    const gchar * file,
    gint line)
{
  TestScriptMessageItem * item;
  gpointer ptr;

  item = _test_script_fixture_pop_message (fixture, func, file, line);
  ptr = NULL;
  sscanf (item->message, "{\"type\":\"send\",\"payload\":"
      "\"0x%" G_GSIZE_MODIFIER "x\"}", (gsize *) &ptr);
  test_script_message_item_free (item);

  return ptr;
}

static gchar *
test_script_fixture_pop_error_description (TestScriptFixture * fixture,
                                           const gchar * func,
                                           const gchar * file,
                                           gint caller_line,
                                           gint * line_number)
{
  TestScriptMessageItem * item;
  gchar description[1024], stack[1024], file_name[64];
  gint line, column;

  item = _test_script_fixture_pop_message (fixture, func, file, caller_line);

  description[0] = '\0';
  stack[0] = '\0';
  file_name[0] = '\0';
  line = -1;
  column = -1;
  sscanf (item->message, "{"
          "\"type\":\"error\","
          "\"description\":\"%[^\"]\","
          "\"stack\":\"%[^\"]\","
          "\"fileName\":\"%[^\"]\","
          "\"lineNumber\":%d,"
          "\"columnNumber\":%d"
      "}",
      description,
      stack,
      file_name,
      &line,
      &column);
  if (column == -1)
  {
    sscanf (item->message, "{"
            "\"type\":\"error\","
            "\"description\":\"%[^\"]\""
        "}",
        description);
  }

  test_script_message_item_free (item);

  gum_assert_false (func, file, line, description[0] == '\0');

  if (line_number != NULL)
    *line_number = line;

  return g_strdup (description);
}

static void
test_script_fixture_expect_error_message_with (TestScriptFixture * fixture,
                                               const gchar * func,
                                               const gchar * file,
                                               gint line,
                                               gint line_number,
                                               const gchar * description)
{
  gchar * actual_description;
  gint actual_line_number;

  actual_description =
      test_script_fixture_pop_error_description (fixture, func, file, line,
          &actual_line_number);

  if (line_number != ANY_LINE_NUMBER)
    gum_assert_cmpint (func, file, line, actual_line_number, ==, line_number);

  gum_assert_cmpstr (func, file, line, actual_description, ==, description);

  g_free (actual_description);
}

static void
test_script_fixture_expect_error_message_matching (TestScriptFixture * fixture,
                                                   const gchar * func,
                                                   const gchar * file,
                                                   gint line,
                                                   gint line_number,
                                                   const gchar * pattern)
{
  gchar * actual_description;
  gint actual_line_number;

  actual_description =
      test_script_fixture_pop_error_description (fixture, func, file, line,
          &actual_line_number);

  if (line_number != ANY_LINE_NUMBER)
    gum_assert_cmpint (func, file, line, actual_line_number, ==, line_number);

  gum_assert_true (func, file, line,
      g_regex_match_simple (pattern, actual_description, 0, 0));

  g_free (actual_description);
}

static void
test_script_fixture_expect_log_message_with (TestScriptFixture * fixture,
                                             const gchar * func,
                                             const gchar * file,
                                             gint line,
                                             const gchar * level,
                                             const gchar * payload_template,
                                             ...)
{
  va_list args;
  gchar * payload;
  TestScriptMessageItem * item;
  gchar * expected_message;

  va_start (args, payload_template);
  payload = g_strdup_vprintf (payload_template, args);
  va_end (args);

  item = _test_script_fixture_pop_message (fixture, func, file, line);
  expected_message = g_strconcat ("{\"type\":\"log\",\"level\":\"", level,
      "\",\"payload\":\"", payload, "\"}", NULL);
  gum_assert_cmpstr (func, file, line, item->message, ==, expected_message);
  test_script_message_item_free (item);
  g_free (expected_message);

  g_free (payload);
}

static void
test_script_fixture_push_timeout (TestScriptFixture * fixture,
                                  guint timeout)
{
  g_queue_push_tail (&fixture->timeouts, GUINT_TO_POINTER (timeout));
}

static void
test_script_fixture_pop_timeout (TestScriptFixture * fixture)
{
  g_queue_pop_tail (&fixture->timeouts);
}

static const gchar *
test_script_fixture_make_tempfile_containing (TestScriptFixture * fixture,
                                              const gchar * contents)
{
  gchar * path;
  gint fd;
  FILE * file;

  fd = g_file_open_tmp ("gum-tests.XXXXXX", &path, NULL);
  g_assert_cmpint (fd, !=, -1);

#ifdef _MSC_VER
  file = _fdopen (fd, "wb");
#else
  file = fdopen (fd, "wb");
#endif
  g_assert_nonnull (file);

  fputs (contents, file);

  fclose (file);

  g_queue_push_tail (&fixture->tempfiles, path);

  return path;
}

static const gchar *
test_script_fixture_escape_path (TestScriptFixture * fixture,
                                 const gchar * path)
{
#ifdef HAVE_WINDOWS
  gchar * result;
  GString * escaped;

  escaped = g_string_new (path);
  g_string_replace (escaped, "\\", "\\\\", 0);
  result = g_string_free (escaped, FALSE);

  g_queue_push_tail (&fixture->heap_blocks, result);

  return result;
#else
  return path;
#endif
}

static gboolean
gum_is_running_under_rosetta (void)
{
#if defined (HAVE_DARWIN) && defined (HAVE_I386)
  int translated = 0;
  size_t size = sizeof (translated);

  if (sysctlbyname ("sysctl.proc_translated", &translated, &size, NULL, 0) != 0)
    return FALSE;

  return translated != 0;
#else
  return FALSE;
#endif
}
