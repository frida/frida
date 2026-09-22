#ifdef HAVE_DARWIN

#include "gum/gumdarwinmapper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mach/mach.h>
#include <sys/socket.h>

typedef struct _DarwinInjectorState DarwinInjectorState;

struct _DarwinInjectorState
{
  GumMemoryRange * mapped_range;
};

typedef void (* AgentEntrypoint) (const gchar * data_string,
    gint * unload_policy, gpointer injector_state);

gint
main (gint argc,
      gchar * argv[])
{
  gint result = 1;
  const gchar * dylib_path;
  mach_port_t task;
  GTimer * timer = NULL;
  GError * error = NULL;
  GumDarwinModuleResolver * resolver = NULL;
  GumDarwinMapper * mapper = NULL;
  const gchar * config;
  gchar * raw_config;
  mach_vm_address_t base_address = 0;
  gsize mapped_size;
  kern_return_t kr;
  GumDarwinModule * module = NULL;
  GumDarwinMapperConstructor constructor;
  GumDarwinMapperDestructor destructor;
  AgentEntrypoint entrypoint;
  gint unload_policy = 0;
  DarwinInjectorState injector_state;
  GumMemoryRange mapped_range;
  char * line;
  size_t line_capacity;

  gum_init ();

  if (argc != 2)
  {
    g_printerr ("usage: %s <dylib_path>\n", argv[0]);
    goto beach;
  }

  dylib_path = argv[1];
  task = mach_task_self ();

  timer = g_timer_new ();

  g_timer_start (timer);
  resolver = gum_darwin_module_resolver_new (task, &error);
  if (error != NULL)
    goto failure;
  g_print ("Created resolver %u ms\n",
      (guint) (g_timer_elapsed (timer, NULL) * 1000.0));

  g_timer_start (timer);
  mapper = gum_darwin_mapper_new_from_file (dylib_path, resolver, &error);
  if (error != NULL)
    goto failure;
  g_print ("Parsed Mach-O in %u ms\n",
      (guint) (g_timer_elapsed (timer, NULL) * 1000.0));

  config =
      "{"
          "\"interaction\":{"
              "\"type\":\"listen\","
              "\"port\":27043,"
              "\"on_port_conflict\":\"pick-next\","
              "\"on_load\":\"resume\""
          "},"
          "\"teardown\":\"full\""
      "}";
  raw_config = g_base64_encode ((const guchar *) config, strlen (config));
  gum_darwin_mapper_add_apple_parameter (mapper, "frida_gadget_config",
      raw_config);
  g_free (raw_config);

  g_timer_start (timer);
  mapped_size = gum_darwin_mapper_size (mapper);
  g_print ("Computed footprint in %u ms\n",
      (guint) (g_timer_elapsed (timer, NULL) * 1000.0));

  kr = mach_vm_allocate (task, &base_address, mapped_size, VM_FLAGS_ANYWHERE);
  g_assert_cmpint (kr, ==, KERN_SUCCESS);

  g_timer_start (timer);
  gum_darwin_mapper_map (mapper, base_address, &error);
  if (error != NULL)
    goto failure;
  g_print ("Mapped in %u ms\n",
      (guint) (g_timer_elapsed (timer, NULL) * 1000.0));

  g_object_get (mapper, "module", &module, NULL);
  g_print ("Base address: 0x%llx\n", module->base_address);

  constructor =
      GSIZE_TO_POINTER (gum_darwin_mapper_constructor (mapper));
  destructor =
      GSIZE_TO_POINTER (gum_darwin_mapper_destructor (mapper));
  entrypoint =
      GSIZE_TO_POINTER (gum_darwin_mapper_resolve (mapper, "frida_agent_main"));

  g_timer_start (timer);
  constructor ();
  g_print ("Ran constructor in %u ms\n",
      (guint) (g_timer_elapsed (timer, NULL) * 1000.0));

  injector_state.mapped_range = &mapped_range;
  mapped_range.base_address = base_address;
  mapped_range.size = mapped_size;

  if (entrypoint != NULL)
  {
    int fds[2];
    gchar * agent_parameters;

    socketpair (AF_UNIX, SOCK_STREAM, 0, fds);

    close (fds[0]);
    agent_parameters = g_strdup_printf ("socket:%d", fds[1]);

    entrypoint (agent_parameters, &unload_policy, &injector_state);

    g_free (agent_parameters);
    close (fds[1]);
  }

  g_print ("Running. Hit ENTER to stop.\n");

  line = NULL;
  line_capacity = 0;
  getline (&line, &line_capacity, stdin);
  free (line);

  g_print ("Stopping\n");

  g_timer_start (timer);
  destructor ();
  g_print ("Ran destructor in %u ms\n",
      (guint) (g_timer_elapsed (timer, NULL) * 1000.0));

  result = 0;
  goto beach;

failure:
  {
    g_printerr ("%s\n", error->message);
    g_error_free (error);

    goto beach;
  }
beach:
  {
    if (base_address != 0)
    {
      kr = mach_vm_deallocate (task, base_address, mapped_size);
      g_assert_cmpint (kr, ==, KERN_SUCCESS);
    }

    g_clear_object (&module);
    g_clear_object (&mapper);
    g_clear_object (&resolver);

    g_clear_pointer (&timer, g_timer_destroy);

    return result;
  }
}

#else

int
main (int argc, char * argv[])
{
  return 0;
}

#endif
