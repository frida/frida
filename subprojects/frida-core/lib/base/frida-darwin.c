#include "frida-darwin.h"

gpointer
_frida_dispatch_retain (gpointer object)
{
  dispatch_retain (object);
  return object;
}

void
_frida_xpc_connection_set_event_handler (xpc_connection_t connection, FridaXpcHandler handler, gpointer user_data)
{
  xpc_connection_set_event_handler (connection, ^(xpc_object_t object)
      {
        handler (object, user_data);
      });
}

void
_frida_xpc_connection_send_message_with_reply (xpc_connection_t connection, xpc_object_t message, dispatch_queue_t replyq,
    FridaXpcHandler handler, gpointer user_data, GDestroyNotify notify)
{
  xpc_connection_send_message_with_reply (connection, message, replyq, ^(xpc_object_t object)
      {
        handler (object, user_data);
        if (notify != NULL)
          notify (user_data);
      });
}

gchar *
_frida_xpc_object_to_string (xpc_object_t object)
{
  gchar * result;
  char * str;

  str = xpc_copy_description (object);
  result = g_strdup (str);
  free (str);

  return result;
}

gboolean
_frida_xpc_dictionary_apply (xpc_object_t dict, FridaXpcDictionaryApplier applier, gpointer user_data)
{
  return xpc_dictionary_apply (dict, ^bool (const char * key, xpc_object_t val)
      {
        return applier (key, val, user_data);
      });
}
