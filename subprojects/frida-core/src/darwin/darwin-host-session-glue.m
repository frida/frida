#include "frida-core.h"

#include "icon-helpers.h"

#include <sys/sysctl.h>

#ifdef HAVE_MACOS
# import <AppKit/AppKit.h>

typedef struct _FridaMacModel FridaMacModel;

struct _FridaMacModel
{
  const gchar * name;
  const gchar * icon;
};

static const FridaMacModel mac_models[] =
{
  { NULL,         "com.apple.led-cinema-display-27" },
  { "MacBookAir", "com.apple.macbookair-11-unibody" },
  { "MacBookPro", "com.apple.macbookpro-13-unibody" },
  { "MacBook",    "com.apple.macbook-unibody" },
  { "iMac",       "com.apple.imac-unibody-21" },
  { "Macmini",    "com.apple.macmini-unibody" },
  { "MacPro",     "com.apple.macpro" }
};

#endif

GVariant *
_frida_darwin_host_session_provider_try_extract_icon (void)
{
#ifdef HAVE_MACOS
  GVariant * icon;
  size_t size;
  gchar * model_name;
  const FridaMacModel * model;
  guint i;
  gchar * filename;

  size = 0;
  sysctlbyname ("hw.model", NULL, &size, NULL, 0);
  model_name = g_malloc (size);
  sysctlbyname ("hw.model", model_name, &size, NULL, 0);

  for (model = NULL, i = 1; i != G_N_ELEMENTS (mac_models) && model == NULL; i++)
  {
    if (g_str_has_prefix (model_name, mac_models[i].name))
      model = &mac_models[i];
  }
  if (model == NULL)
    model = &mac_models[0];

  filename = g_strconcat ("/System/Library/CoreServices/CoreTypes.bundle/Contents/Resources/", model->icon, ".icns", NULL);
  icon = _frida_icon_from_file (filename, 16, 16);
  g_free (filename);

  g_free (model_name);

  return icon;
#else
  return NULL;
#endif
}

gchar *
_frida_darwin_host_session_path_for_application_identifier (const gchar * identifier, GError ** error)
{
#ifdef HAVE_MACOS
  gchar * result = NULL;
  NSAutoreleasePool * pool;
  NSString * bundle_id;
  NSURL * bundle_url, * executable_url;

  pool = [[NSAutoreleasePool alloc] init];

  bundle_id = [NSString stringWithUTF8String:identifier];

  bundle_url = [[NSWorkspace sharedWorkspace] URLForApplicationWithBundleIdentifier:bundle_id];
  if (bundle_url == nil)
    goto not_found;

  executable_url = [NSBundle bundleWithURL:bundle_url].executableURL;
  if (executable_url == nil)
    goto not_found;

  result = g_strdup (executable_url.path.UTF8String);
  goto beach;

not_found:
  {
    g_set_error (error,
        FRIDA_ERROR,
        FRIDA_ERROR_INVALID_ARGUMENT,
        "Unable to find application with identifier '%s'",
        identifier);
    goto beach;
  }
beach:
  {
    [pool release];

    return result;
  }
#else
  g_set_error (error,
      FRIDA_ERROR,
      FRIDA_ERROR_NOT_SUPPORTED,
      "Not supported on this platform");
  return NULL;
#endif
}
