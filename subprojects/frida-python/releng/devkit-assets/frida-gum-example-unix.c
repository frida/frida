#include "frida-gum.h"

#include <fcntl.h>
#include <unistd.h>

typedef struct _ExampleListenerData ExampleListenerData;
typedef enum _ExampleHookId ExampleHookId;

struct _ExampleListenerData
{
  guint num_calls;
};

enum _ExampleHookId
{
  EXAMPLE_HOOK_OPEN,
  EXAMPLE_HOOK_CLOSE
};

static void example_listener_on_enter (GumInvocationContext * ic, gpointer user_data);
static void example_listener_on_leave (GumInvocationContext * ic, gpointer user_data);

int
main (int argc,
      char * argv[])
{
  GumInterceptor * interceptor;
  ExampleListenerData * data;
  GumInvocationListener * listener;
  GumAttachOptions open_options = { 0, }, close_options = { 0, };

  gum_init_embedded ();

  interceptor = gum_interceptor_obtain ();

  data = g_new0 (ExampleListenerData, 1);
  listener = gum_make_call_listener (example_listener_on_enter, example_listener_on_leave, data, g_free);

  gum_interceptor_begin_transaction (interceptor);
  open_options.listener_function_data = GSIZE_TO_POINTER (EXAMPLE_HOOK_OPEN);
  gum_interceptor_attach (interceptor,
      GSIZE_TO_POINTER (gum_module_find_global_export_by_name ("open")),
      listener,
      &open_options);
  close_options.listener_function_data = GSIZE_TO_POINTER (EXAMPLE_HOOK_CLOSE);
  gum_interceptor_attach (interceptor,
      GSIZE_TO_POINTER (gum_module_find_global_export_by_name ("close")),
      listener,
      &close_options);
  gum_interceptor_end_transaction (interceptor);

  close (open ("/etc/hosts", O_RDONLY));
  close (open ("/etc/fstab", O_RDONLY));

  g_print ("[*] listener got %u calls\n", data->num_calls);

  gum_interceptor_detach (interceptor, listener);

  close (open ("/etc/hosts", O_RDONLY));
  close (open ("/etc/fstab", O_RDONLY));

  g_print ("[*] listener still has %u calls\n", data->num_calls);

  g_object_unref (listener);
  g_object_unref (interceptor);

  gum_deinit_embedded ();

  return 0;
}

static void
example_listener_on_enter (GumInvocationContext * ic,
                           gpointer user_data)
{
  ExampleListenerData * data = user_data;
  ExampleHookId hook_id;

  hook_id = GUM_IC_GET_FUNC_DATA (ic, ExampleHookId);

  switch (hook_id)
  {
    case EXAMPLE_HOOK_OPEN:
      g_print ("[*] open(\"%s\")\n", (const gchar *) gum_invocation_context_get_nth_argument (ic, 0));
      break;
    case EXAMPLE_HOOK_CLOSE:
      g_print ("[*] close(%d)\n", GPOINTER_TO_INT (gum_invocation_context_get_nth_argument (ic, 0)));
      break;
  }

  data->num_calls++;
}

static void
example_listener_on_leave (GumInvocationContext * ic,
                           gpointer user_data)
{
}
