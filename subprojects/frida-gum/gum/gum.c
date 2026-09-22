/*
 * Copyright (C) 2008-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2023 Stefano Moioli <smxdev4@gmail.com>
 * Copyright (C) 2024 Yannis Juglaret <yjuglaret@mozilla.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gum.h"

#include "gum-init.h"
#include "gumexceptorbackend.h"
#include "guminterceptor-priv.h"
#include "gummemory-priv.h"
#include "gumprintf.h"
#include "gumtls-priv.h"
#include "valgrind.h"
#ifdef HAVE_I386
# ifdef _MSC_VER
#  include <intrin.h>
# else
#  include <cpuid.h>
# endif
#elif defined (HAVE_ARM64) && defined (HAVE_DARWIN)
# include "gum/gumdarwin.h"
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_WINDOWS
# include <windows.h>
#endif
#if !defined (GUM_USE_SYSTEM_ALLOC) && defined (HAVE_FRIDA_LIBFFI)
# include <ffi.h>
#endif

#define DEBUG_HEAP_LEAKS 0

typedef struct _GumInternalThreadDetails GumInternalThreadDetails;

struct _GumInternalThreadDetails
{
  GumThreadId thread_id;
  guint n_cloaked_ranges;
  GumMemoryRange cloaked_ranges[GUM_MAX_THREAD_RANGES];
};

static void gum_destructor_invoke (GumDestructorFunc destructor);

#if !defined (GUM_USE_SYSTEM_ALLOC) && defined (HAVE_FRIDA_LIBFFI)
static void gum_on_ffi_allocate (void * base_address, size_t size);
static void gum_on_ffi_deallocate (void * base_address, size_t size);
#endif
#ifdef HAVE_FRIDA_GLIB
static void gum_on_thread_init (void);
static void gum_on_thread_realize (void);
static void gum_on_thread_dispose (void);
static void gum_on_thread_finalize (void);
static void gum_internal_thread_details_free (
    GumInternalThreadDetails * details);
static void gum_on_fd_opened (gint fd, const gchar * description);
static void gum_on_fd_closed (gint fd, const gchar * description);
#endif

static void gum_on_log_message (const gchar * log_domain,
    GLogLevelFlags log_level, const gchar * message, gpointer user_data);

#if defined (HAVE_LINUX) && defined (HAVE_GLIBC)
# include <dlfcn.h>
# define GUM_RTLD_DLOPEN 0x80000000
extern void * __libc_dlopen_mode (char * name, int flags)
    __attribute__ ((weak));
static void gum_libdl_prevent_unload (void);
#endif

#ifdef HAVE_ANDROID
# include <android/log.h>
#else
# include <stdio.h>
# ifdef HAVE_DARWIN
#  include <CoreFoundation/CoreFoundation.h>
#  include <dlfcn.h>

typedef struct _GumCFApi GumCFApi;
typedef gint32 CFLogLevel;

enum _CFLogLevel
{
  kCFLogLevelEmergency = 0,
  kCFLogLevelAlert     = 1,
  kCFLogLevelCritical  = 2,
  kCFLogLevelError     = 3,
  kCFLogLevelWarning   = 4,
  kCFLogLevelNotice    = 5,
  kCFLogLevelInfo      = 6,
  kCFLogLevelDebug     = 7
};

struct _GumCFApi
{
  CFStringRef (* CFStringCreateWithCString) (CFAllocatorRef alloc,
      const char * c_str, CFStringEncoding encoding);
  void (* CFRelease) (CFTypeRef cf);
  void (* CFLog) (CFLogLevel level, CFStringRef format, ...);
};

# endif
#endif

static void gum_do_init (void);

static GumAddress * gum_address_copy (const GumAddress * address);
static void gum_address_free (GumAddress * address);

static GumCpuFeatures gum_do_query_cpu_features (void);

static gboolean gum_initialized = FALSE;
static GSList * gum_early_destructors = NULL;
static GSList * gum_final_destructors = NULL;

#ifdef HAVE_FRIDA_GLIB
static GPrivate gum_internal_thread_details_key = G_PRIVATE_INIT (
    (GDestroyNotify) gum_internal_thread_details_free);
#endif

static GumInterceptor * gum_cached_interceptor = NULL;

G_DEFINE_QUARK (gum-error-quark, gum_error)

G_DEFINE_BOXED_TYPE (GumAddress, gum_address, gum_address_copy,
                     gum_address_free)

void
gum_init (void)
{
  if (gum_initialized)
    return;
  gum_initialized = TRUE;

  gum_internal_heap_ref ();
  gum_do_init ();
}

void
gum_shutdown (void)
{
  g_slist_foreach (gum_early_destructors, (GFunc) gum_destructor_invoke, NULL);
  g_slist_free (gum_early_destructors);
  gum_early_destructors = NULL;
}

void
gum_deinit (void)
{
  g_assert (gum_initialized);

  gum_shutdown ();

  _gum_tls_deinit ();

  g_slist_foreach (gum_final_destructors, (GFunc) gum_destructor_invoke, NULL);
  g_slist_free (gum_final_destructors);
  gum_final_destructors = NULL;

  _gum_interceptor_deinit ();

  gum_initialized = FALSE;
}

static void
gum_do_init (void)
{
#ifndef GUM_USE_SYSTEM_ALLOC
  cs_opt_mem gum_cs_mem_callbacks = {
    gum_internal_malloc,
    gum_internal_calloc,
    gum_internal_realloc,
    gum_internal_free,
    (cs_vsnprintf_t) gum_vsnprintf
  };
#endif

#ifdef HAVE_FRIDA_GLIB
  glib_init ();
  gobject_init ();
#endif

#ifndef GUM_USE_SYSTEM_ALLOC
  cs_option (0, CS_OPT_MEM, GPOINTER_TO_SIZE (&gum_cs_mem_callbacks));
#endif

  _gum_tls_init ();
  _gum_interceptor_init ();
  _gum_tls_realize ();
}

void
_gum_register_early_destructor (GumDestructorFunc destructor)
{
  gum_early_destructors = g_slist_prepend (gum_early_destructors,
      GUM_FUNCPTR_TO_POINTER (destructor));
}

void
_gum_register_destructor (GumDestructorFunc destructor)
{
  gum_final_destructors = g_slist_prepend (gum_final_destructors,
      GUM_FUNCPTR_TO_POINTER (destructor));
}

static void
gum_destructor_invoke (GumDestructorFunc destructor)
{
  destructor ();
}

void
gum_init_embedded (void)
{
#if !defined (GUM_USE_SYSTEM_ALLOC) && defined (HAVE_FRIDA_LIBFFI)
  ffi_mem_callbacks ffi_callbacks = {
    (void * (*) (size_t)) gum_malloc,
    (void * (*) (size_t, size_t)) gum_calloc,
    gum_free,
    gum_on_ffi_allocate,
    gum_on_ffi_deallocate
  };
#endif
#ifdef HAVE_FRIDA_GLIB
  GThreadCallbacks thread_callbacks = {
    gum_on_thread_init,
    gum_on_thread_realize,
    gum_on_thread_dispose,
    gum_on_thread_finalize
  };
  GFDCallbacks fd_callbacks = {
    gum_on_fd_opened,
    gum_on_fd_closed
  };
#endif
#if !defined (GUM_USE_SYSTEM_ALLOC) && defined (HAVE_FRIDA_GLIB) && \
    !DEBUG_HEAP_LEAKS && !defined (HAVE_ASAN)
  GMemVTable mem_vtable = {
    gum_malloc,
    gum_realloc,
    gum_memalign,
    gum_free,
    gum_calloc,
    gum_malloc,
    gum_realloc
  };
#endif
#if defined (HAVE_WINDOWS) && DEBUG_HEAP_LEAKS
  int tmp_flag;
#endif

  if (gum_initialized)
    return;
  gum_initialized = TRUE;

#if defined (HAVE_WINDOWS) && DEBUG_HEAP_LEAKS
  /*_CrtSetBreakAlloc (1337);*/

  _CrtSetReportMode (_CRT_ERROR, _CRTDBG_MODE_FILE);
  _CrtSetReportFile (_CRT_ERROR, _CRTDBG_FILE_STDERR);

  tmp_flag = _CrtSetDbgFlag (_CRTDBG_REPORT_FLAG);

  tmp_flag |= _CRTDBG_ALLOC_MEM_DF;
  tmp_flag |= _CRTDBG_LEAK_CHECK_DF;
  tmp_flag &= ~_CRTDBG_CHECK_CRT_DF;

  _CrtSetDbgFlag (tmp_flag);
#endif

  gum_internal_heap_ref ();
#if !defined (GUM_USE_SYSTEM_ALLOC) && defined (HAVE_FRIDA_LIBFFI)
  ffi_set_mem_callbacks (&ffi_callbacks);
#endif
#ifdef HAVE_FRIDA_GLIB
  g_thread_set_callbacks (&thread_callbacks);
  g_platform_audit_set_fd_callbacks (&fd_callbacks);
#endif
#if !DEBUG_HEAP_LEAKS && !defined (HAVE_ASAN)
  if (RUNNING_ON_VALGRIND)
  {
    g_setenv ("G_SLICE", "always-malloc", TRUE);
  }
  else
  {
#if !defined (GUM_USE_SYSTEM_ALLOC) && defined (HAVE_FRIDA_GLIB)
    g_mem_set_vtable (&mem_vtable);
#endif
  }
#else
  g_setenv ("G_SLICE", "always-malloc", TRUE);
#endif
#ifdef HAVE_FRIDA_GLIB
  glib_init ();
#endif
  g_log_set_default_handler (gum_on_log_message, NULL);
  gum_do_init ();

  g_set_prgname ("frida");

#if defined (HAVE_LINUX) && defined (HAVE_GLIBC)
  gum_libdl_prevent_unload ();
#endif

  gum_cached_interceptor = gum_interceptor_obtain ();
}

void
gum_deinit_embedded (void)
{
  g_assert (gum_initialized);

  gum_shutdown ();
#ifdef HAVE_FRIDA_GLIB
  glib_shutdown ();
#endif

  g_clear_object (&gum_cached_interceptor);

  gum_deinit ();
#ifdef HAVE_FRIDA_GLIB
  glib_deinit ();
#endif
#if !defined (GUM_USE_SYSTEM_ALLOC) && defined (HAVE_FRIDA_LIBFFI)
  ffi_deinit ();
#endif
  gum_internal_heap_unref ();

  gum_initialized = FALSE;
}

void
gum_prepare_to_fork (void)
{
  _gum_exceptor_backend_prepare_to_fork ();
}

void
gum_recover_from_fork_in_parent (void)
{
  _gum_exceptor_backend_recover_from_fork_in_parent ();
}

void
gum_recover_from_fork_in_child (void)
{
  _gum_exceptor_backend_recover_from_fork_in_child ();
}

#if !defined (GUM_USE_SYSTEM_ALLOC) && defined (HAVE_FRIDA_LIBFFI)

static void
gum_on_ffi_allocate (void * base_address,
                     size_t size)
{
  GumMemoryRange range;
  range.base_address = GUM_ADDRESS (base_address);
  range.size = size;
  gum_cloak_add_range (&range);
}

static void
gum_on_ffi_deallocate (void * base_address,
                       size_t size)
{
  GumMemoryRange range;
  range.base_address = GUM_ADDRESS (base_address);
  range.size = size;
  gum_cloak_remove_range (&range);
}

#endif

#ifdef HAVE_FRIDA_GLIB

static void
gum_on_thread_init (void)
{
}

static void
gum_on_thread_realize (void)
{
  GumInternalThreadDetails * details;
  guint i;

  gum_interceptor_ignore_current_thread (gum_cached_interceptor);

  details = g_slice_new (GumInternalThreadDetails);
  details->thread_id = gum_process_get_current_thread_id ();
  details->n_cloaked_ranges =
      gum_thread_try_get_ranges (details->cloaked_ranges,
          GUM_MAX_THREAD_RANGES);

  gum_cloak_add_thread (details->thread_id);

  for (i = 0; i != details->n_cloaked_ranges; i++)
    gum_cloak_add_range (&details->cloaked_ranges[i]);

  /* This allows us to free the data no matter how the thread exits */
  g_private_set (&gum_internal_thread_details_key, details);
}

static void
gum_on_thread_dispose (void)
{
  if (gum_cached_interceptor != NULL)
    gum_interceptor_ignore_current_thread (gum_cached_interceptor);
}

static void
gum_on_thread_finalize (void)
{
  if (gum_cached_interceptor != NULL)
    gum_interceptor_unignore_current_thread (gum_cached_interceptor);
}

static void
gum_internal_thread_details_free (GumInternalThreadDetails * details)
{
  GumThreadId thread_id;
  guint i;

  thread_id = details->thread_id;

  for (i = 0; i != details->n_cloaked_ranges; i++)
    gum_cloak_remove_range (&details->cloaked_ranges[i]);

  g_slice_free (GumInternalThreadDetails, details);

  gum_cloak_remove_thread (thread_id);
}

static void
gum_on_fd_opened (gint fd,
                  const gchar * description)
{
  gum_cloak_add_file_descriptor (fd);
}

static void
gum_on_fd_closed (gint fd,
                  const gchar * description)
{
  gum_cloak_remove_file_descriptor (fd);
}

#endif

#if defined (HAVE_LINUX) && defined (HAVE_GLIBC)

static void
gum_libdl_prevent_unload (void)
{
  if (__libc_dlopen_mode == NULL)
    return;

  __libc_dlopen_mode ("libdl.so.2", RTLD_LAZY | GUM_RTLD_DLOPEN);
}

#endif

static void
gum_on_log_message (const gchar * log_domain,
                    GLogLevelFlags log_level,
                    const gchar * message,
                    gpointer user_data)
{
#if defined (HAVE_WINDOWS)
  gunichar2 * message_utf16;

  message_utf16 = g_utf8_to_utf16 (message, -1, NULL, NULL, NULL);
  OutputDebugStringW (message_utf16);
  g_free (message_utf16);
#elif defined (HAVE_ANDROID)
  int priority;

  switch (log_level & G_LOG_LEVEL_MASK)
  {
    case G_LOG_LEVEL_ERROR:
    case G_LOG_LEVEL_CRITICAL:
    case G_LOG_LEVEL_WARNING:
      priority = ANDROID_LOG_FATAL;
      break;
    case G_LOG_LEVEL_MESSAGE:
    case G_LOG_LEVEL_INFO:
      priority = ANDROID_LOG_INFO;
      break;
    case G_LOG_LEVEL_DEBUG:
    default:
      priority = ANDROID_LOG_DEBUG;
      break;
  }

  __android_log_write (priority, log_domain, message);
#else
# ifdef HAVE_DARWIN
  static gsize api_value = 0;
  GumCFApi * api;

  if (g_once_init_enter (&api_value))
  {
    const gchar * cf_path = "/System/Library/Frameworks/"
        "CoreFoundation.framework/CoreFoundation";
    GumModule * cf;

    /*
     * CoreFoundation must be loaded by the main thread, so we should avoid
     * loading it.
     */
    cf = gum_process_find_module_by_name (cf_path);
    if (cf != NULL)
    {
      GumModule * foundation;

      api = g_slice_new (GumCFApi);

      api->CFStringCreateWithCString = GSIZE_TO_POINTER (
          gum_module_find_export_by_name (cf, "CFStringCreateWithCString"));
      g_assert (api->CFStringCreateWithCString != NULL);

      api->CFRelease = GSIZE_TO_POINTER (
          gum_module_find_export_by_name (cf, "CFRelease"));
      g_assert (api->CFRelease != NULL);

      api->CFLog = GSIZE_TO_POINTER (
          gum_module_find_export_by_name (cf, "CFLog"));
      g_assert (api->CFLog != NULL);

      g_object_unref (cf);

      /*
       * In case Foundation is also loaded, make sure it's initialized
       * so CFLog() doesn't crash if called early.
       */
      foundation = gum_process_find_module_by_name (
          "/System/Library/Frameworks/Foundation.framework/Foundation");
      if (foundation != NULL)
      {
        gum_module_ensure_initialized (foundation);
        g_object_unref (foundation);
      }
    }
    else
    {
      api = NULL;
    }

    g_once_init_leave (&api_value, 1 + GPOINTER_TO_SIZE (api));
  }

  api = GSIZE_TO_POINTER (api_value - 1);
  if (api != NULL)
  {
    CFLogLevel cf_log_level;
    CFStringRef message_str, template_str;

    switch (log_level & G_LOG_LEVEL_MASK)
    {
      case G_LOG_LEVEL_ERROR:
        cf_log_level = kCFLogLevelError;
        break;
      case G_LOG_LEVEL_CRITICAL:
        cf_log_level = kCFLogLevelCritical;
        break;
      case G_LOG_LEVEL_WARNING:
        cf_log_level = kCFLogLevelWarning;
        break;
      case G_LOG_LEVEL_MESSAGE:
        cf_log_level = kCFLogLevelNotice;
        break;
      case G_LOG_LEVEL_INFO:
        cf_log_level = kCFLogLevelInfo;
        break;
      case G_LOG_LEVEL_DEBUG:
        cf_log_level = kCFLogLevelDebug;
        break;
      default:
        g_assert_not_reached ();
    }

    message_str = api->CFStringCreateWithCString (NULL, message,
        kCFStringEncodingUTF8);
    if (log_domain != NULL)
    {
      CFStringRef log_domain_str;

      template_str = api->CFStringCreateWithCString (NULL, "%@: %@",
          kCFStringEncodingUTF8);
      log_domain_str = api->CFStringCreateWithCString (NULL, log_domain,
          kCFStringEncodingUTF8);
      api->CFLog (cf_log_level, template_str, log_domain_str, message_str);
      api->CFRelease (log_domain_str);
    }
    else
    {
      template_str = api->CFStringCreateWithCString (NULL, "%@",
          kCFStringEncodingUTF8);
      api->CFLog (cf_log_level, template_str, message_str);
    }
    api->CFRelease (template_str);
    api->CFRelease (message_str);

    return;
  }
  /* else: fall through to stdout/stderr logging */
# endif

  FILE * file = NULL;
  const gchar * severity = NULL;

  switch (log_level & G_LOG_LEVEL_MASK)
  {
    case G_LOG_LEVEL_ERROR:
      file = stderr;
      severity = "ERROR";
      break;
    case G_LOG_LEVEL_CRITICAL:
      file = stderr;
      severity = "CRITICAL";
      break;
    case G_LOG_LEVEL_WARNING:
      file = stderr;
      severity = "WARNING";
      break;
    case G_LOG_LEVEL_MESSAGE:
      file = stderr;
      severity = "MESSAGE";
      break;
    case G_LOG_LEVEL_INFO:
      file = stdout;
      severity = "INFO";
      break;
    case G_LOG_LEVEL_DEBUG:
      file = stdout;
      severity = "DEBUG";
      break;
    default:
      g_assert_not_reached ();
  }

  fprintf (file, "[%s %s] %s\n", log_domain, severity, message);
  fflush (file);
#endif
}

void
gum_panic (const gchar * format,
           ...)
{
  va_list args;

  va_start (args, format);
  g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, format, args);
  va_end (args);

  g_abort ();
}

static GumAddress *
gum_address_copy (const GumAddress * address)
{
  return g_slice_dup (GumAddress, address);
}

static void
gum_address_free (GumAddress * address)
{
  g_slice_free (GumAddress, address);
}

GumCpuFeatures
gum_query_cpu_features (void)
{
  static gsize cached_result = 0;

  if (g_once_init_enter (&cached_result))
  {
    GumCpuFeatures features = gum_do_query_cpu_features ();

    g_once_init_leave (&cached_result, features + 1);
  }

  return cached_result - 1;
}

#if defined (HAVE_I386)

static gboolean gum_get_cpuid (guint level, guint * a, guint * b, guint * c,
    guint * d);

static GumCpuFeatures
gum_do_query_cpu_features (void)
{
  GumCpuFeatures features = 0;
  gboolean cpu_supports_avx2 = FALSE;
  gboolean cpu_supports_avx512 = FALSE;
  gboolean cpu_supports_cet_ss = FALSE;
  gboolean cpu_supports_cet_ibt = FALSE;
  gboolean os_enabled_xsave = FALSE;
  guint a, b, c, d;

  if (gum_get_cpuid (7, &a, &b, &c, &d))
  {
    cpu_supports_avx2 = (b & (1 << 5)) != 0;
    cpu_supports_avx512 = (b & (1 << 16)) != 0;
    cpu_supports_cet_ss = (c & (1 << 7)) != 0;
    cpu_supports_cet_ibt = (d & (1 << 20)) != 0;
  }

  if (gum_get_cpuid (1, &a, &b, &c, &d))
    os_enabled_xsave = (c & (1 << 27)) != 0;

  if (cpu_supports_avx2 && os_enabled_xsave)
    features |= GUM_CPU_AVX2;

  if (cpu_supports_avx512 && os_enabled_xsave)
    features |= GUM_CPU_AVX512;

  if (cpu_supports_cet_ss)
    features |= GUM_CPU_CET_SS;

  if (cpu_supports_cet_ibt)
    features |= GUM_CPU_CET_IBT;

  return features;
}

static gboolean
gum_get_cpuid (guint level,
               guint * a,
               guint * b,
               guint * c,
               guint * d)
{
#ifdef _MSC_VER
  gint info[4];
  guint n;

  __cpuid (info, 0);
  n = info[0];
  if (n < level)
    return FALSE;

  __cpuid (info, level);

  *a = info[0];
  *b = info[1];
  *c = info[2];
  *d = info[3];

  return TRUE;
#else
  guint n;

  n = __get_cpuid_max (0, NULL);
  if (n < level)
    return FALSE;

  __cpuid_count (level, 0, *a, *b, *c, *d);

  return TRUE;
#endif
}

#elif defined (HAVE_ARM)

static GumCpuFeatures
gum_do_query_cpu_features (void)
{
  GumCpuFeatures features = 0;

#if defined (HAVE_LINUX) && !defined (HAVE_ANDROID)
# if __ARM_ARCH > 4 || defined (__THUMB_INTERWORK__)
  features |= GUM_CPU_THUMB_INTERWORK;
# endif
#else
  features |= GUM_CPU_THUMB_INTERWORK;
#endif

#ifdef __ARM_VFPV2__
  features |= GUM_CPU_VFP2;
#endif

#ifdef __ARM_VFPV3__
  features |= GUM_CPU_VFP3;
#endif

#ifdef __ARM_NEON__
  features |= GUM_CPU_VFPD32;
#endif

#if defined (HAVE_LINUX) && defined (__ARM_EABI__) && \
    !(defined (__ARM_VFPV2__) && defined (__ARM_VFPV3__) && \
        defined (__ARM_NEON__))
  {
    gchar * info = NULL;
    gchar ** items = NULL;
    gchar * start, * end, * item;
    guint i;

    if (!g_file_get_contents ("/proc/cpuinfo", &info, NULL, NULL))
      goto beach;

    start = strstr (info, "\nFeatures");
    if (start == NULL)
      goto beach;
    start += 9;

    start = strchr (start, ':');
    if (start == NULL)
      goto beach;
    start += 2;

    end = strchr (start, '\n');
    if (end == NULL)
      goto beach;
    *end = '\0';

    items = g_strsplit (start, " ", -1);

    for (i = 0; (item = items[i]) != NULL; i++)
    {
      if (strcmp (item, "vfp") == 0)
      {
        features |= GUM_CPU_VFP2;
      }
      else if (strcmp (item, "vfpv3") == 0)
      {
        features |= GUM_CPU_VFP3;
      }
      else if (strcmp (item, "vfpd32") == 0 || strcmp (item, "neon") == 0)
      {
        features |= GUM_CPU_VFPD32;
      }
      else if (strcmp (item, "asimd") == 0)
      {
        features |= GUM_CPU_VFP2 | GUM_CPU_VFP3 | GUM_CPU_VFPD32;
      }
    }

beach:
    g_strfreev (items);

    g_free (info);
  }
#endif

  return features;
}

#elif defined (HAVE_ARM64) && defined (HAVE_DARWIN)

static GumCpuFeatures
gum_do_query_cpu_features (void)
{
  GumCpuFeatures features = 0;
  GumDarwinAllImageInfos infos;
  GumDarwinCpuSubtype subtype;

  gum_darwin_query_all_image_infos (mach_task_self (), &infos, NULL);

  subtype = *((GumDarwinCpuSubtype *) (infos.dyld_image_load_address + 8));
  if ((subtype & GUM_DARWIN_CPU_SUBTYPE_MASK) == GUM_DARWIN_CPU_SUBTYPE_ARM64E)
    features |= GUM_CPU_PTRAUTH;

  return features;
}

#else

static GumCpuFeatures
gum_do_query_cpu_features (void)
{
  return 0;
}

#endif
