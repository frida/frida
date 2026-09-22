#include "frida-gadget.h"

#include "frida-base.h"
#include "frida-payload.h"

#ifdef HAVE_WINDOWS
# include <windows.h>
#else
# include <signal.h>
#endif
#include <gumjs/gumscriptbackend.h>
#if defined (HAVE_GIOAPPLE)
# include <gioapple.h>
#elif defined (HAVE_GIOOPENSSL)
# include <gioopenssl.h>
#endif

#ifdef HAVE_DARWIN
static void frida_parse_apple_parameters (const gchar * apple[], gboolean * found_range, GumMemoryRange * range, gchar ** config_data);
#endif

static gpointer run_worker_loop (gpointer data);
static gboolean stop_worker_loop (gpointer data);

static GumThreadId worker_tid;
static GThread * worker_thread;
static GMainLoop * worker_loop;
static GMainContext * worker_context;

#if defined (HAVE_WINDOWS)

BOOL WINAPI
DllMain (HINSTANCE instance, DWORD reason, LPVOID reserved)
{
  switch (reason)
  {
    case DLL_PROCESS_ATTACH:
      frida_gadget_load (NULL, NULL, NULL);
      break;
    case DLL_PROCESS_DETACH:
    {
      gboolean is_dynamic_unload = reserved == NULL;
      if (is_dynamic_unload)
        frida_gadget_unload ();
      break;
    }
    default:
      break;
  }

  return TRUE;
}

#elif defined (HAVE_DARWIN)

__attribute__ ((constructor)) static void
frida_on_load (int argc, const char * argv[], const char * envp[], const char * apple[], int * result)
{
  gboolean found_range;
  GumMemoryRange range;
  gchar * config_data;

  frida_parse_apple_parameters (apple, &found_range, &range, &config_data);

  frida_gadget_load (found_range ? &range : NULL, config_data, (config_data != NULL) ? result : NULL);

  g_free (config_data);
}

#else

__attribute__ ((constructor)) static void
frida_on_load (void)
{
  frida_gadget_load (NULL, NULL, NULL);
}

__attribute__ ((destructor)) static void
frida_on_unload (void)
{
  frida_gadget_unload ();
}

#endif

void
frida_gadget_environment_init (void)
{
#ifdef _MSC_VER
  frida_libc_shim_init ();
#endif
  gio_init ();

  g_thread_set_garbage_handler (_frida_gadget_on_pending_thread_garbage, NULL);

#if defined (HAVE_GIOAPPLE)
  g_io_module_apple_register ();
#elif defined (HAVE_GIOOPENSSL)
  g_io_module_openssl_register ();
#endif

  gum_script_backend_get_type (); /* Warm up */
  frida_error_quark (); /* Initialize early so GDBus will pick it up */

#if defined (HAVE_ANDROID) && __ANDROID_API__ < __ANDROID_API_L__
  /*
   * We might be holding the dynamic linker's lock, so force-initialize
   * our bsd_signal() wrapper on this thread.
   */
  bsd_signal (G_MAXINT32, SIG_DFL);
#endif

  worker_context = g_main_context_ref (g_main_context_default ());
  worker_loop = g_main_loop_new (worker_context, FALSE);
  worker_thread = g_thread_new ("frida-gadget", run_worker_loop, NULL);
}

void
frida_gadget_environment_deinit (void)
{
  GSource * source;

  g_assert (worker_loop != NULL);

  frida_libc_shim_prepare_to_deinit ();

  source = g_idle_source_new ();
  g_source_set_priority (source, G_PRIORITY_LOW);
  g_source_set_callback (source, stop_worker_loop, NULL, NULL);
  g_source_attach (source, worker_context);
  g_source_unref (source);

  g_thread_join (worker_thread);
  worker_tid = 0;
  worker_thread = NULL;

  g_main_loop_unref (worker_loop);
  worker_loop = NULL;
  g_main_context_unref (worker_context);
  worker_context = NULL;

  gum_shutdown ();
  gio_shutdown ();
  glib_shutdown ();

  gio_deinit ();

  frida_run_atexit_handlers ();

#if defined (_MSC_VER) || defined (HAVE_DARWIN)
  frida_libc_shim_deinit ();
#endif
}

gboolean
frida_gadget_environment_can_block_at_load_time (void)
{
#ifdef HAVE_WINDOWS
  return FALSE;
#else
  return TRUE;
#endif
}

GumThreadId
frida_gadget_environment_get_worker_tid (void)
{
  return worker_tid;
}

GMainContext *
frida_gadget_environment_get_worker_context (void)
{
  return worker_context;
}

#ifndef HAVE_DARWIN

gchar *
frida_gadget_environment_detect_bundle_id (void)
{
  return NULL;
}

gchar *
frida_gadget_environment_detect_bundle_name (void)
{
  return NULL;
}

gchar *
frida_gadget_environment_detect_documents_dir (void)
{
  return NULL;
}

gboolean
frida_gadget_environment_has_objc_class (const gchar * name)
{
  return FALSE;
}

void
frida_gadget_environment_set_thread_name (const gchar * name)
{
  /* For now only implemented on i/macOS as Fruity.Injector relies on it there. */
}

#endif

static gpointer
run_worker_loop (gpointer data)
{
  worker_tid = gum_process_get_current_thread_id ();

  g_main_context_push_thread_default (worker_context);
  g_main_loop_run (worker_loop);
  g_main_context_pop_thread_default (worker_context);

  return NULL;
}

static gboolean
stop_worker_loop (gpointer data)
{
  g_main_loop_quit (worker_loop);

  return FALSE;
}

void
frida_gadget_log_info (const gchar * message)
{
  g_info ("%s", message);
}

void
frida_gadget_log_warning (const gchar * message)
{
  g_warning ("%s", message);
}

#ifdef HAVE_DARWIN

static void
frida_parse_apple_parameters (const gchar * apple[], gboolean * found_range, GumMemoryRange * range, gchar ** config_data)
{
  const gchar * entry;
  guint i = 0;

  *found_range = FALSE;
  *config_data = NULL;

  while ((entry = apple[i++]) != NULL)
  {
    if (g_str_has_prefix (entry, "frida_dylib_range="))
    {
      *found_range = sscanf (entry, "frida_dylib_range=0x%" G_GINT64_MODIFIER "x,0x%" G_GSIZE_MODIFIER "x",
          &range->base_address, &range->size) == 2;
    }
    else if (g_str_has_prefix (entry, "frida_gadget_config="))
    {
      guchar * data;
      gsize size;

      data = g_base64_decode (entry + 20, &size);
      if (data != NULL)
      {
        *config_data = g_strndup ((const gchar *) data, size);
        g_free (data);
      }
    }
  }
}

#endif
