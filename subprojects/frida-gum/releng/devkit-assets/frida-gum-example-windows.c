/*
 * To build, set up your Release configuration like this:
 *
 * [Runtime Library]
 * Multi-threaded (/MT)
 *
 * Visit https://frida.re to learn more about Frida.
 */

#include "frida-gum.h"

#include <windows.h>

typedef struct _ExampleListenerData ExampleListenerData;
typedef enum _ExampleHookId ExampleHookId;

struct _ExampleListenerData
{
  guint num_calls;
};

enum _ExampleHookId
{
  EXAMPLE_HOOK_MESSAGE_BEEP,
  EXAMPLE_HOOK_SLEEP
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
  GumModule * user32, * kernel32;
  GumAttachOptions beep_options = { 0, }, sleep_options = { 0, };

  gum_init_embedded ();

  interceptor = gum_interceptor_obtain ();

  data = g_new0 (ExampleListenerData, 1);
  listener = gum_make_call_listener (example_listener_on_enter, example_listener_on_leave, data, g_free);

  user32 = gum_process_find_module_by_name ("user32.dll");
  kernel32 = gum_process_find_module_by_name ("kernel32.dll");

  gum_interceptor_begin_transaction (interceptor);
  beep_options.listener_function_data =
      GSIZE_TO_POINTER (EXAMPLE_HOOK_MESSAGE_BEEP);
  gum_interceptor_attach (interceptor,
      GSIZE_TO_POINTER (gum_module_find_export_by_name (user32, "MessageBeep")),
      listener,
      &beep_options);
  sleep_options.listener_function_data = GSIZE_TO_POINTER (EXAMPLE_HOOK_SLEEP);
  gum_interceptor_attach (interceptor,
      GSIZE_TO_POINTER (gum_module_find_export_by_name (kernel32, "Sleep")),
      listener,
      &sleep_options);
  gum_interceptor_end_transaction (interceptor);

  MessageBeep (MB_ICONINFORMATION);
  Sleep (1);

  g_print ("[*] listener got %u calls\n", data->num_calls);

  gum_interceptor_detach (interceptor, listener);

  MessageBeep (MB_ICONINFORMATION);
  Sleep (1);

  g_print ("[*] listener still has %u calls\n", data->num_calls);

  g_object_unref (kernel32);
  g_object_unref (user32);
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
    case EXAMPLE_HOOK_MESSAGE_BEEP:
      g_print ("[*] MessageBeep(%u)\n", GPOINTER_TO_UINT (gum_invocation_context_get_nth_argument (ic, 0)));
      break;
    case EXAMPLE_HOOK_SLEEP:
      g_print ("[*] Sleep(%u)\n", GPOINTER_TO_UINT (gum_invocation_context_get_nth_argument (ic, 0)));
      break;
  }

  data->num_calls++;
}

static void
example_listener_on_leave (GumInvocationContext * ic,
                           gpointer user_data)
{
}
