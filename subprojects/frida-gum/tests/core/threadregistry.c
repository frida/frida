/*
 * Copyright (C) 2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumthreadregistry.h"

#include "testutil.h"

#define TESTCASE(NAME) \
    void test_thread_registry_ ## NAME (void)
#define TESTENTRY(NAME) \
    TESTENTRY_SIMPLE ("Core/ThreadRegistry", test_thread_registry, NAME)

TESTLIST_BEGIN (thread_registry)
  TESTENTRY (thread_registry_should_emit_signal_on_add)
TESTLIST_END ()

static gboolean print_existing_thread (const GumThreadDetails * thread,
    gpointer user_data);
static void on_thread_added (GumThreadRegistry * registry,
    const GumThreadDetails * thread, gpointer user_data);
static void on_thread_removed (GumThreadRegistry * registry,
    const GumThreadDetails * thread, gpointer user_data);
static void on_thread_renamed (GumThreadRegistry * registry,
    const GumThreadDetails * thread, const gchar * previous_name,
    gpointer user_data);
static void print_thread (const GumThreadDetails * thread,
    const gchar * prefix);

static gpointer hello_proc (gpointer data);
static gpointer hello2_proc (gpointer data);
static gpointer hello3_proc (gpointer data);

TESTCASE (thread_registry_should_emit_signal_on_add)
{
  GumThreadRegistry * registry;
  guint i;

  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }

  g_thread_unref (g_thread_new ("hello", hello_proc, GSIZE_TO_POINTER (1337)));
  g_usleep (50000);

  registry = gum_thread_registry_obtain ();
  gum_thread_registry_enumerate_threads (registry, print_existing_thread, NULL);
  g_signal_connect (registry, "thread-added", G_CALLBACK (on_thread_added),
      NULL);
  g_signal_connect (registry, "thread-removed", G_CALLBACK (on_thread_removed),
      NULL);
  g_signal_connect (registry, "thread-renamed", G_CALLBACK (on_thread_renamed),
      NULL);
  g_printerr ("Sleeping in PID %u...\n", gum_process_get_id ());

  for (i = 0; ; i++)
  {
    g_usleep (G_USEC_PER_SEC);
    if (i == 1)
    {
      g_thread_unref (
          g_thread_new ("hello3", hello3_proc, GSIZE_TO_POINTER (42)));
    }
  }
}

static gboolean
print_existing_thread (const GumThreadDetails * thread,
                       gpointer user_data)
{
  print_thread (thread, "Existing thread");
  return TRUE;
}

static void
on_thread_added (GumThreadRegistry * registry,
                 const GumThreadDetails * thread,
                 gpointer user_data)
{
  print_thread (thread, G_STRFUNC);
}

static void
on_thread_removed (GumThreadRegistry * registry,
                   const GumThreadDetails * thread,
                   gpointer user_data)
{
  print_thread (thread, G_STRFUNC);
}

static void
on_thread_renamed (GumThreadRegistry * registry,
                   const GumThreadDetails * thread,
                   const gchar * previous_name,
                   gpointer user_data)
{
  print_thread (thread, G_STRFUNC);
  if (previous_name != NULL)
    g_printerr ("\tprevious_name=\"%s\"\n", previous_name);
}

static void
print_thread (const GumThreadDetails * thread,
              const gchar * prefix)
{
  GString * message;

  message = g_string_sized_new (128);

  g_string_append (message, prefix);
  g_string_append_printf (message, ": id=%" G_GSIZE_MODIFIER "u", thread->id);

  if ((thread->flags & GUM_THREAD_FLAGS_NAME) != 0)
    g_string_append_printf (message, ", name=\"%s\"", thread->name);

  if ((thread->flags & GUM_THREAD_FLAGS_ENTRYPOINT_ROUTINE) != 0)
  {
    g_string_append_printf (message,
        ", entrypoint.routine=0x%" G_GINT64_MODIFIER "x",
        thread->entrypoint.routine);
  }
  if ((thread->flags & GUM_THREAD_FLAGS_ENTRYPOINT_PARAMETER) != 0)
  {
    g_string_append_printf (message,
        ", entrypoint.parameter=0x%" G_GINT64_MODIFIER "x",
        thread->entrypoint.parameter);
  }

  g_printerr ("%s\n", message->str);

  g_string_free (message, TRUE);
}

static gpointer
hello_proc (gpointer data)
{
  g_thread_unref (
      g_thread_new ("hello2", hello2_proc, GSIZE_TO_POINTER (1337)));

  while (TRUE)
  {
    g_printerr ("Hello! TID=%zu\n", gum_process_get_current_thread_id ());
    g_usleep (G_USEC_PER_SEC);
  }

  return NULL;
}

static gpointer
hello2_proc (gpointer data)
{
  while (TRUE)
  {
    g_printerr ("Hello2! TID=%zu\n", gum_process_get_current_thread_id ());
    g_usleep (G_USEC_PER_SEC);
  }

  return NULL;
}

static gpointer
hello3_proc (gpointer data)
{
  while (TRUE)
  {
    g_printerr ("Hello3! TID=%zu\n", gum_process_get_current_thread_id ());
    g_usleep (G_USEC_PER_SEC);
  }

  return NULL;
}
