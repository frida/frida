#include "server-glue.h"

#include "frida-core.h"
#if defined (HAVE_IOS) || defined (HAVE_TVOS) || defined (HAVE_XROS)
# include "server-ios-tvos.h"
#endif
#ifdef HAVE_ANDROID
# include "frida-selinux.h"
#endif

#if defined (HAVE_DARWIN)
# include <CoreFoundation/CoreFoundation.h>

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

void CFLog (CFLogLevel level, CFStringRef format, ...);

#elif defined (HAVE_ANDROID)
# include <android/log.h>
#else
# include <stdio.h>
#endif

static void frida_server_on_log_message (const gchar * log_domain, GLogLevelFlags log_level, const gchar * message, gpointer user_data);

static gboolean frida_verbose_logging_enabled = FALSE;

void
frida_server_environment_init (void)
{
  frida_init_with_runtime (FRIDA_RUNTIME_GLIB);

  g_log_set_default_handler (frida_server_on_log_message, NULL);
}

void
frida_server_environment_set_verbose_logging_enabled (gboolean enabled)
{
  frida_verbose_logging_enabled = enabled;
}

void
frida_server_environment_configure (void)
{
#if defined (HAVE_IOS) || defined (HAVE_TVOS) || defined (HAVE_XROS)
  _frida_server_ios_tvos_configure ();
#endif

#ifdef HAVE_ANDROID
  frida_selinux_patch_policy ();
#endif
}

static void
frida_server_on_log_message (const gchar * log_domain, GLogLevelFlags log_level, const gchar * message, gpointer user_data)
{
  if (!frida_verbose_logging_enabled && (log_level & G_LOG_LEVEL_MASK) >= G_LOG_LEVEL_DEBUG)
    return;

#if defined (HAVE_DARWIN)
  CFLogLevel cf_log_level;
  CFStringRef message_str;

  (void) user_data;

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

  message_str = CFStringCreateWithCString (NULL, message, kCFStringEncodingUTF8);
  if (log_domain != NULL)
  {
    CFStringRef log_domain_str;

    log_domain_str = CFStringCreateWithCString (NULL, log_domain, kCFStringEncodingUTF8);
    CFLog (cf_log_level, CFSTR ("%@: %@"), log_domain_str, message_str);
    CFRelease (log_domain_str);
  }
  else
  {
    CFLog (cf_log_level, CFSTR ("%@"), message_str);
  }
  CFRelease (message_str);
#elif defined (HAVE_ANDROID)
  int priority;

  (void) user_data;

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
      priority = ANDROID_LOG_DEBUG;
      break;
    default:
      g_assert_not_reached ();
  }

  __android_log_write (priority, log_domain, message);
#else
  FILE * file = NULL;
  const gchar * severity = NULL;

  (void) user_data;

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

