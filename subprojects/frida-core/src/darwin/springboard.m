#import "springboard.h"

#include <dlfcn.h>
#include <gum/gum.h>

#define FRIDA_ASSIGN_SBS_FUNC(N) \
    api->N = dlsym (api->sbs, G_STRINGIFY (N)); \
    g_assert (api->N != NULL)
#define FRIDA_ASSIGN_SBS_CONSTANT(N) \
    str = dlsym (api->sbs, G_STRINGIFY (N)); \
    g_assert (str != NULL); \
    api->N = *str
#define FRIDA_ASSIGN_FBS_CONSTANT(N) \
    str = dlsym (api->fbs, G_STRINGIFY (N)); \
    g_assert (str != NULL); \
    api->N = *str

#define FRIDA_IOS_MOBILE_USER_ID 501

extern kern_return_t bootstrap_look_up (mach_port_t bp, const char * service_name, mach_port_t * sp);
extern kern_return_t bootstrap_look_up_per_user (mach_port_t bp, const char * service_name, uid_t target_user, mach_port_t * sp);

extern mach_port_t bootstrap_port;

#ifndef HAVE_TVOS
static kern_return_t frida_replacement_bootstrap_look_up (mach_port_t bp, const char * service_name, mach_port_t * sp);
static kern_return_t frida_replacement_xpc_look_up_endpoint (const char * service_name, uint32_t type, uint64_t handle,
    uint64_t lookup_handle, const uint8_t * instance, uint64_t flags, void * cputypes, mach_port_t * port, bool * non_launching);

typedef kern_return_t (* FridaXpcLookUpEndpointFunc) (const char * service_name, uint32_t type, uint64_t handle, uint64_t lookup_handle,
    const uint8_t * instance, uint64_t flags, void * cputypes, mach_port_t * port, bool * non_launching);

static FridaXpcLookUpEndpointFunc frida_find_xpc_look_up_endpoint_implementation (void);
static gboolean frida_is_bl_imm (guint32 insn);
#endif

static FridaSpringboardApi * frida_springboard_api = NULL;
#ifndef HAVE_TVOS
static FridaXpcLookUpEndpointFunc frida_xpc_look_up_endpoint;
#endif

FridaSpringboardApi *
_frida_get_springboard_api (void)
{
  if (frida_springboard_api == NULL)
  {
    FridaSpringboardApi * api;
    NSString ** str;
    id (* objc_get_class_impl) (const gchar * name);

    api = g_new0 (FridaSpringboardApi, 1);

    api->sbs = dlopen ("/System/Library/PrivateFrameworks/SpringBoardServices.framework/SpringBoardServices", RTLD_GLOBAL | RTLD_LAZY);
    g_assert (api->sbs != NULL);

    api->fbs = dlopen ("/System/Library/PrivateFrameworks/FrontBoardServices.framework/FrontBoardServices", RTLD_GLOBAL | RTLD_LAZY);

    FRIDA_ASSIGN_SBS_FUNC (SBSSpringBoardBackgroundServerPort);
    FRIDA_ASSIGN_SBS_FUNC (SBSCopyFrontmostApplicationDisplayIdentifier);
    FRIDA_ASSIGN_SBS_FUNC (SBSCopyApplicationDisplayIdentifiers);
    FRIDA_ASSIGN_SBS_FUNC (SBSCopyDisplayIdentifierForProcessID);
    FRIDA_ASSIGN_SBS_FUNC (SBSCopyLocalizedApplicationNameForDisplayIdentifier);
    FRIDA_ASSIGN_SBS_FUNC (SBSCopyIconImagePNGDataForDisplayIdentifier);
    FRIDA_ASSIGN_SBS_FUNC (SBSCopyInfoForApplicationWithProcessID);
    FRIDA_ASSIGN_SBS_FUNC (SBSLaunchApplicationWithIdentifierAndLaunchOptions);
    FRIDA_ASSIGN_SBS_FUNC (SBSLaunchApplicationWithIdentifierAndURLAndLaunchOptions);
    FRIDA_ASSIGN_SBS_FUNC (SBSApplicationLaunchingErrorString);

    FRIDA_ASSIGN_SBS_CONSTANT (SBSApplicationLaunchOptionUnlockDeviceKey);

    objc_get_class_impl = dlsym (RTLD_DEFAULT, "objc_getClass");
    g_assert (objc_get_class_impl != NULL);

    if (api->fbs != NULL)
    {
      api->FBSSystemService = objc_get_class_impl ("FBSSystemService");
      g_assert (api->FBSSystemService != nil);

      FRIDA_ASSIGN_FBS_CONSTANT (FBSOpenApplicationOptionKeyUnlockDevice);
      FRIDA_ASSIGN_FBS_CONSTANT (FBSOpenApplicationOptionKeyDebuggingOptions);

      FRIDA_ASSIGN_FBS_CONSTANT (FBSDebugOptionKeyArguments);
      FRIDA_ASSIGN_FBS_CONSTANT (FBSDebugOptionKeyEnvironment);
      FRIDA_ASSIGN_FBS_CONSTANT (FBSDebugOptionKeyStandardOutPath);
      FRIDA_ASSIGN_FBS_CONSTANT (FBSDebugOptionKeyStandardErrorPath);
      FRIDA_ASSIGN_FBS_CONSTANT (FBSDebugOptionKeyDisableASLR);
    }

    api->mcs = dlopen ("/System/Library/Frameworks/MobileCoreServices.framework/MobileCoreServices", RTLD_GLOBAL | RTLD_LAZY);
    g_assert (api->mcs != NULL);

    api->LSApplicationProxy = objc_get_class_impl ("LSApplicationProxy");
    g_assert (api->LSApplicationProxy != nil);

    api->LSApplicationWorkspace = objc_get_class_impl ("LSApplicationWorkspace");
    g_assert (api->LSApplicationWorkspace != nil);

#ifndef HAVE_TVOS
    if (api->SBSSpringBoardBackgroundServerPort () == MACH_PORT_NULL)
    {
      GumInterceptor * interceptor;

      interceptor = gum_interceptor_obtain ();

      gum_interceptor_replace (interceptor, bootstrap_look_up, frida_replacement_bootstrap_look_up, NULL, NULL);

      frida_xpc_look_up_endpoint = frida_find_xpc_look_up_endpoint_implementation ();
      if (frida_xpc_look_up_endpoint != NULL)
        gum_interceptor_replace (interceptor, frida_xpc_look_up_endpoint, frida_replacement_xpc_look_up_endpoint, NULL, NULL);
      else
        g_error ("Unable to locate _xpc_look_up_endpoint(); please file a bug");
    }
#endif

    frida_springboard_api = api;
  }

  return frida_springboard_api;
}

#ifndef HAVE_TVOS

static kern_return_t
frida_replacement_bootstrap_look_up (mach_port_t bp, const char * service_name, mach_port_t * sp)
{
  if (strcmp (service_name, "com.apple.springboard.backgroundappservices") == 0)
    return bootstrap_look_up_per_user (bp, service_name, FRIDA_IOS_MOBILE_USER_ID, sp);

  return bootstrap_look_up (bp, service_name, sp);
}

static kern_return_t
frida_replacement_xpc_look_up_endpoint (const char * service_name, uint32_t type, uint64_t handle, uint64_t lookup_handle,
    const uint8_t * instance, uint64_t flags, void * cputypes, mach_port_t * port, bool * non_launching)
{
  if (strcmp (service_name, "com.apple.containermanagerd") == 0 ||
      strcmp (service_name, "com.apple.frontboard.systemappservices") == 0 ||
      strcmp (service_name, "com.apple.lsd.icons") == 0 ||
      strcmp (service_name, "com.apple.lsd.mapdb") == 0 ||
      strcmp (service_name, "com.apple.runningboard") == 0 ||
      g_str_has_prefix (service_name, "com.apple.distributed_notifications"))
  {
    if (non_launching != NULL)
      *non_launching = false;
    return bootstrap_look_up_per_user (bootstrap_port, service_name, FRIDA_IOS_MOBILE_USER_ID, port);
  }

  return frida_xpc_look_up_endpoint (service_name, type, handle, lookup_handle, instance, flags, cputypes, port, non_launching);
}

static FridaXpcLookUpEndpointFunc
frida_find_xpc_look_up_endpoint_implementation (void)
{
  GumModule * libxpc;
  guint32 * cursor;

  libxpc = gum_process_find_module_by_name ("/usr/lib/system/libxpc.dylib");
  cursor = GSIZE_TO_POINTER (gum_strip_code_address (gum_module_find_export_by_name (libxpc, "xpc_endpoint_create_bs_named")));
  g_object_unref (libxpc);
  if (cursor == NULL)
    return NULL;

  do
  {
    guint32 insn = *cursor;

    if (frida_is_bl_imm (insn))
    {
      union
      {
        gint32 i;
        guint32 u;
      } distance;

      distance.u = insn & GUM_INT26_MASK;
      if ((distance.u & (1 << (26 - 1))) != 0)
        distance.u |= 0xfc000000;

      return (FridaXpcLookUpEndpointFunc) (cursor + distance.i);
    }

    cursor++;
  }
  while (TRUE);
}

static gboolean
frida_is_bl_imm (guint32 insn)
{
  return (insn & ~GUM_INT26_MASK) == 0x94000000;
}

#endif
