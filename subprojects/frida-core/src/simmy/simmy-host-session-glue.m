#include "frida-core.h"

#include <dlfcn.h>

#import <Foundation/Foundation.h>

typedef struct _FridaSimmyContext FridaSimmyContext;

struct _FridaSimmyContext
{
  dispatch_queue_t dispatch_queue;

  void * core_simulator;

  NSString * SimDeviceLaunchApplicationKeyArguments;
  NSString * SimDeviceLaunchApplicationKeyEnvironment;
  NSString * SimDeviceLaunchApplicationKeyStandardErrPath;
  NSString * SimDeviceLaunchApplicationKeyStandardOutPath;
  NSString * SimDeviceLaunchApplicationKeyTerminateRunningProcess;
  NSString * SimDeviceLaunchApplicationKeyWaitForDebugger;

  NSString * SimDeviceSpawnKeyArguments;
  NSString * SimDeviceSpawnKeyEnvironment;
  NSString * SimDeviceSpawnKeyStderr;
  NSString * SimDeviceSpawnKeyStdin;
  NSString * SimDeviceSpawnKeyStdout;
  NSString * SimDeviceSpawnKeyWaitForDebugger;

  FridaSimmyHostSessionBackendDeviceAddedFunc on_device_added;
  gpointer on_device_added_target;
};

static void frida_simmy_context_destroy (FridaSimmyContext * self);
static void frida_simmy_context_emit_devices (FridaSimmyContext * self);

static NSArray * frida_argv_to_arguments_array (gchar * const * argv, gint argv_length, gint start_index);
static NSDictionary * frida_envp_to_environment_dictionary (gchar * const * envp, gint envp_length);

@interface SimRuntime : NSObject
@property (nonatomic, readonly, strong) NSString * identifier;
@property (nonatomic, readonly, strong) NSString * shortName;
@property (nonatomic, readonly, strong) NSString * versionString;
@property (nonatomic, readonly, strong) NSString * root;
@end

@interface SimDevice : NSObject
@property (nonatomic, readonly, strong) NSUUID * UDID;
@property (nonatomic, readonly, strong) NSString * name;
@property (nonatomic, readonly, strong) SimRuntime * runtime;
@property (nonatomic, readonly, strong) NSString * stateString;
- (NSString *)getenv:(NSString *)name
               error:(NSError * _Nullable * _Nullable)error;
- (NSDictionary<NSString *, NSDictionary<NSString *, id> *> *)installedAppsWithError:(NSError * _Nullable * _Nullable)error;
- (void)launchApplicationAsyncWithID:(NSString *)identifier
                             options:(NSDictionary<NSString *, id> * _Nullable)options
                     completionQueue:(dispatch_queue_t)queue
                   completionHandler:(void (^) (NSError * error, int pid))handler;
- spawnAsyncWithPath:(NSString *)path
             options:(NSDictionary<NSString *, id> * _Nullable)options
    terminationQueue:(dispatch_queue_t)tq
  terminationHandler:(void (^) (int status))th
     completionQueue:(dispatch_queue_t)cq
   completionHandler:(void (^) (NSError * error, int pid))ch;
@end

@interface SimDeviceSet : NSObject
@property (nonatomic, readonly, strong) NSArray<SimDevice *> *devices;
@end

@protocol SimServiceContextClass
+ (instancetype)serviceContextForDeveloperDir:(NSString *)dir
                                        error:(NSError * _Nullable * _Nullable)error;
@end

@interface SimServiceContext : NSObject
- (SimDeviceSet *)defaultDeviceSetWithError:(NSError * _Nullable * _Nullable)error;
@end

void *
_frida_simmy_host_session_backend_start (FridaSimmyHostSessionBackendDeviceAddedFunc on_device_added, gpointer on_device_added_target,
    FridaSimmyCompleteFunc on_complete, gpointer on_complete_target, GDestroyNotify on_complete_target_destroy_notify)
{
  FridaSimmyContext * ctx;

  ctx = g_slice_new0 (FridaSimmyContext);
  ctx->dispatch_queue = dispatch_queue_create ("re.frida.simmy.queue", DISPATCH_QUEUE_SERIAL);

  ctx->on_device_added = on_device_added;
  ctx->on_device_added_target = on_device_added_target;

  dispatch_async (ctx->dispatch_queue, ^
  {
    frida_simmy_context_emit_devices (ctx);

    on_complete (on_complete_target);

    if (on_complete_target_destroy_notify != NULL)
      on_complete_target_destroy_notify (on_complete_target);
  });

  return ctx;
}

void
_frida_simmy_host_session_backend_stop (void * simmy_context, FridaSimmyCompleteFunc on_complete,
    gpointer on_complete_target, GDestroyNotify on_complete_target_destroy_notify)
{
  FridaSimmyContext * ctx = simmy_context;

  dispatch_async (ctx->dispatch_queue, ^
  {
    frida_simmy_context_destroy (ctx);
    g_slice_free (FridaSimmyContext, ctx);

    on_complete (on_complete_target);

    if (on_complete_target_destroy_notify != NULL)
      on_complete_target_destroy_notify (on_complete_target);
  });
}

static void
frida_simmy_context_destroy (FridaSimmyContext * self)
{
  g_clear_pointer (&self->core_simulator, dlclose);

  dispatch_release (self->dispatch_queue);
}

static void
frida_simmy_context_emit_devices (FridaSimmyContext * self)
{
  void * xcselect_module;
  bool (* xcselect_get_developer_dir_path) (char *, size_t, bool *, bool *, bool *);
  char developer_dir[1024] = { 0, };
  bool from_override, is_command_line_tools, from_fallback;
  void * cs;
  NSString ** str;
  Class<SimServiceContextClass> SimServiceContextClass;
  SimServiceContext * ctx;
  SimDeviceSet * set;

  xcselect_module = dlopen ("/usr/lib/libxcselect.dylib", RTLD_GLOBAL | RTLD_LAZY);
  if (xcselect_module == NULL)
    goto beach;

  xcselect_get_developer_dir_path = dlsym (xcselect_module, "xcselect_get_developer_dir_path");

  if (!xcselect_get_developer_dir_path (developer_dir, sizeof (developer_dir), &from_override, &is_command_line_tools, &from_fallback))
    goto beach;

  cs = dlopen ("/Library/Developer/PrivateFrameworks/CoreSimulator.framework/CoreSimulator", RTLD_GLOBAL | RTLD_LAZY);
  if (cs == NULL)
    goto beach;
  self->core_simulator = cs;

#define FRIDA_ASSIGN_CS_CONSTANT(N) \
    str = dlsym (cs, G_STRINGIFY (N)); \
    g_assert (str != NULL); \
    self->N = *str

  FRIDA_ASSIGN_CS_CONSTANT (SimDeviceLaunchApplicationKeyArguments);
  FRIDA_ASSIGN_CS_CONSTANT (SimDeviceLaunchApplicationKeyEnvironment);
  FRIDA_ASSIGN_CS_CONSTANT (SimDeviceLaunchApplicationKeyStandardErrPath);
  FRIDA_ASSIGN_CS_CONSTANT (SimDeviceLaunchApplicationKeyStandardOutPath);
  FRIDA_ASSIGN_CS_CONSTANT (SimDeviceLaunchApplicationKeyTerminateRunningProcess);
  FRIDA_ASSIGN_CS_CONSTANT (SimDeviceLaunchApplicationKeyWaitForDebugger);

  FRIDA_ASSIGN_CS_CONSTANT (SimDeviceSpawnKeyArguments);
  FRIDA_ASSIGN_CS_CONSTANT (SimDeviceSpawnKeyEnvironment);
  FRIDA_ASSIGN_CS_CONSTANT (SimDeviceSpawnKeyStderr);
  FRIDA_ASSIGN_CS_CONSTANT (SimDeviceSpawnKeyStdin);
  FRIDA_ASSIGN_CS_CONSTANT (SimDeviceSpawnKeyStdout);
  FRIDA_ASSIGN_CS_CONSTANT (SimDeviceSpawnKeyWaitForDebugger);

#undef FRIDA_ASSIGN_CS_CONSTANT

  SimServiceContextClass = NSClassFromString (@"SimServiceContext");

  ctx = [SimServiceContextClass serviceContextForDeveloperDir:[NSString stringWithUTF8String:developer_dir] error:nil];

  set = [ctx defaultDeviceSetWithError:nil];

  for (SimDevice * device in set.devices)
  {
    FridaSimmyRuntime * runtime;
    FridaSimmyDevice * d;

    if (![device.stateString isEqualToString:@"Booted"])
      continue;

    runtime = frida_simmy_runtime_new (device.runtime);

    d = frida_simmy_device_new ([device retain], device.UDID.UUIDString.UTF8String, device.name.UTF8String,
        [device getenv:@"SIMULATOR_MODEL_IDENTIFIER" error:nil].UTF8String, runtime, self);
    self->on_device_added (d, self->on_device_added_target);
    g_object_unref (d);

    g_object_unref (runtime);
  }

beach:
  if (xcselect_module != NULL)
    dlclose (xcselect_module);
}

void
_frida_simmy_device_list_applications (FridaSimmyDevice * self, FridaSimmyDeviceListApplicationsCompleteFunc on_complete,
    gpointer on_complete_target, GDestroyNotify on_complete_target_destroy_notify)
{
  FridaSimmyContext * ctx = frida_simmy_device_get_simmy_context (self);

  dispatch_async (ctx->dispatch_queue, ^
  {
    GeeArrayList * applications;
    SimDevice * device;

    applications = gee_array_list_new (FRIDA_SIMMY_TYPE_APPLICATION, g_object_ref, g_object_unref, NULL, NULL, NULL);

    device = frida_simmy_device_get_handle (self);
    NSDictionary<NSString *, NSDictionary<NSString *, id> *> * bundles = [device installedAppsWithError:nil];
    for (NSString * identifier in bundles)
    {
      NSDictionary<NSString *, id> * bundle;
      NSString * display_name;
      FridaSimmyApplication * app;

      bundle = bundles[identifier];
      display_name = bundle[@"CFBundleDisplayName"];

      app = frida_simmy_application_new (identifier.UTF8String, display_name.UTF8String);
      gee_collection_add (GEE_COLLECTION (applications), app);
      g_object_unref (app);
    }

    on_complete (GEE_LIST (applications), on_complete_target);

    if (on_complete_target_destroy_notify != NULL)
      on_complete_target_destroy_notify (on_complete_target);

    g_object_unref (applications);
  });
}

void
_frida_simmy_device_launch_application (FridaSimmyDevice * self, const gchar * identifier, FridaHostSpawnOptions * opts,
    FridaSimmyDeviceLaunchApplicationCompleteFunc on_complete, gpointer on_complete_target,
    GDestroyNotify on_complete_target_destroy_notify)
{
  SimDevice * device;
  FridaSimmyContext * ctx;
  __block FridaStdioPipes * pipes;
  __block FridaFileDescriptor * out_fd, * err_fd;
  gchar * out_name, * err_name;
  GError * error = NULL;
  __block GMainContext * main_context;

  device = frida_simmy_device_get_handle (self);
  ctx = frida_simmy_device_get_simmy_context (self);

  if (opts->has_envp)
    goto envp_not_supported;

  pipes = frida_make_stdio_pipes (opts->stdio, FALSE, NULL, NULL, &out_fd, &out_name, &err_fd, &err_name, &error);
  if (error != NULL)
    goto propagate_error;

  main_context = g_main_context_ref_thread_default ();

  @autoreleasepool
  {
    NSMutableDictionary<NSString *, id> * launch_opts;

    launch_opts = [@{
      ctx->SimDeviceLaunchApplicationKeyWaitForDebugger: @YES,
      ctx->SimDeviceLaunchApplicationKeyTerminateRunningProcess: @YES,
    } mutableCopy];

    if (opts->has_argv)
      launch_opts[ctx->SimDeviceLaunchApplicationKeyArguments] = frida_argv_to_arguments_array (opts->argv, opts->argv_length1, 1);

    if (opts->has_env)
      launch_opts[ctx->SimDeviceLaunchApplicationKeyEnvironment] = frida_envp_to_environment_dictionary (opts->env, opts->env_length1);

    if (opts->stdio == FRIDA_STDIO_PIPE)
    {
      launch_opts[ctx->SimDeviceLaunchApplicationKeyStandardOutPath] = @(out_name);
      launch_opts[ctx->SimDeviceLaunchApplicationKeyStandardErrPath] = @(err_name);
    };

    [device launchApplicationAsyncWithID:[NSString stringWithUTF8String:identifier]
                                 options:launch_opts
                         completionQueue:ctx->dispatch_queue
                       completionHandler:
      ^(NSError * error, int pid)
      {
        FridaSimmySpawnedProcess * process = NULL;

        if (error == nil)
          process = frida_simmy_spawned_process_new (pid, pipes, main_context);

        g_main_context_unref (main_context);
        main_context = NULL;

        g_clear_object (&pipes);
        g_clear_object (&out_fd);
        g_clear_object (&err_fd);

        on_complete (error.localizedDescription.UTF8String, process, on_complete_target);

        g_clear_object (&process);

        if (on_complete_target_destroy_notify != NULL)
          on_complete_target_destroy_notify (on_complete_target);
      }];
  }

  g_free (out_name);
  g_free (err_name);

  return;

envp_not_supported:
  {
    error = g_error_new_literal (
        FRIDA_ERROR,
        FRIDA_ERROR_NOT_SUPPORTED,
        "The 'envp' option is not supported when spawning Simmy apps, use the 'env' option instead");
    goto propagate_error;
  }
propagate_error:
  {
    dispatch_async (ctx->dispatch_queue, ^
    {
      on_complete (error->message, NULL, on_complete_target);

      g_error_free (error);

      if (on_complete_target_destroy_notify != NULL)
        on_complete_target_destroy_notify (on_complete_target);
    });

    return;
  }
}

void
_frida_simmy_device_spawn_program (FridaSimmyDevice * self, const gchar * program, FridaHostSpawnOptions * opts,
    FridaSimmyDeviceSpawnProgramCompleteFunc on_complete, gpointer on_complete_target,
    GDestroyNotify on_complete_target_destroy_notify)
{
  const FridaStdio stdio = opts->stdio;
  SimDevice * device;
  FridaSimmyContext * ctx;
  __block FridaStdioPipes * pipes;
  __block FridaFileDescriptor * in_fd, * out_fd, * err_fd;
  GError * error = NULL;
  __block GMainContext * main_context;

  device = frida_simmy_device_get_handle (self);
  ctx = frida_simmy_device_get_simmy_context (self);

  if (opts->has_envp)
    goto envp_not_supported;

  pipes = frida_make_stdio_pipes (opts->stdio, TRUE, &in_fd, NULL, &out_fd, NULL, &err_fd, NULL, &error);
  if (error != NULL)
    goto propagate_error;

  main_context = g_main_context_ref_thread_default ();

  @autoreleasepool
  {
    NSMutableDictionary<NSString *, id> * spawn_opts;
    __block FridaSimmySpawnedProcess * process = NULL;

    spawn_opts = [@{
      ctx->SimDeviceSpawnKeyWaitForDebugger: @YES,
    } mutableCopy];

    if (opts->has_argv)
      spawn_opts[ctx->SimDeviceSpawnKeyArguments] = frida_argv_to_arguments_array (opts->argv, opts->argv_length1, 0);

    if (opts->has_env)
      spawn_opts[ctx->SimDeviceSpawnKeyEnvironment] = frida_envp_to_environment_dictionary (opts->env, opts->env_length1);

    if (stdio == FRIDA_STDIO_PIPE)
    {
      spawn_opts[ctx->SimDeviceSpawnKeyStdin] = @(in_fd->handle);
      spawn_opts[ctx->SimDeviceSpawnKeyStdout] = @(out_fd->handle);
      spawn_opts[ctx->SimDeviceSpawnKeyStderr] = @(err_fd->handle);
    };

    [device spawnAsyncWithPath:[NSString stringWithUTF8String:program]
                       options:spawn_opts
              terminationQueue:ctx->dispatch_queue
            terminationHandler:
      ^(int status)
      {
        _frida_simmy_spawned_process_on_termination (process, status);

        g_object_unref (process);
        process = NULL;
      }
               completionQueue:ctx->dispatch_queue
             completionHandler:
      ^(NSError * error, int pid)
      {
        if (error == nil)
          process = frida_simmy_spawned_process_new (pid, pipes, main_context);

        g_main_context_unref (main_context);
        main_context = NULL;

        g_clear_object (&pipes);
        g_clear_object (&in_fd);
        g_clear_object (&out_fd);
        g_clear_object (&err_fd);

        on_complete (error.localizedDescription.UTF8String, process, on_complete_target);

        if (on_complete_target_destroy_notify != NULL)
          on_complete_target_destroy_notify (on_complete_target);
      }];
  }

  return;

envp_not_supported:
  {
    error = g_error_new_literal (
        FRIDA_ERROR,
        FRIDA_ERROR_NOT_SUPPORTED,
        "The 'envp' option is not supported when spawning Simmy programs, use the 'env' option instead");
    goto propagate_error;
  }
propagate_error:
  {
    dispatch_async (ctx->dispatch_queue, ^
    {
      on_complete (error->message, NULL, on_complete_target);

      g_error_free (error);

      if (on_complete_target_destroy_notify != NULL)
        on_complete_target_destroy_notify (on_complete_target);
    });
    return;
  }
}

const gchar *
_frida_simmy_runtime_get_identifier (FridaSimmyRuntime * self)
{
  return ((SimRuntime *) frida_simmy_runtime_get_handle (self)).identifier.UTF8String;
}

const gchar *
_frida_simmy_runtime_get_short_name (FridaSimmyRuntime * self)
{
  return ((SimRuntime *) frida_simmy_runtime_get_handle (self)).shortName.UTF8String;
}

const gchar *
_frida_simmy_runtime_get_version_string (FridaSimmyRuntime * self)
{
  return ((SimRuntime *) frida_simmy_runtime_get_handle (self)).versionString.UTF8String;
}

const gchar *
_frida_simmy_runtime_get_root (FridaSimmyRuntime * self)
{
  return ((SimRuntime *) frida_simmy_runtime_get_handle (self)).root.UTF8String;
}

static NSArray *
frida_argv_to_arguments_array (gchar * const * argv, gint argv_length, gint start_index)
{
  NSMutableArray * result;
  gint i;

  result = [NSMutableArray arrayWithCapacity:argv_length];
  for (i = start_index; i < argv_length; i++)
    [result addObject:[NSString stringWithUTF8String:argv[i]]];

  return result;
}

static NSDictionary *
frida_envp_to_environment_dictionary (gchar * const * envp, gint envp_length)
{
  NSMutableDictionary * result;
  gint i;

  result = [NSMutableDictionary dictionaryWithCapacity:envp_length];
  for (i = 0; i != envp_length; i++)
  {
    const gchar * pair, * equals_sign, * name_start, * value_start;
    NSUInteger name_size, value_size;
    NSString * name, * value;

    pair = envp[i];

    equals_sign = strchr (pair, '=');
    if (equals_sign == NULL)
      continue;

    name_start = pair;
    name_size = equals_sign - name_start;

    value_start = equals_sign + 1;
    value_size = pair + strlen (pair) - value_start;

    name = [[NSString alloc] initWithBytes:name_start
                                    length:name_size
                                  encoding:NSUTF8StringEncoding];
    value = [[NSString alloc] initWithBytes:value_start
                                     length:value_size
                                   encoding:NSUTF8StringEncoding];

    [result setObject:value forKey:name];

    [value release];
    [name release];
  }

  return result;
}
