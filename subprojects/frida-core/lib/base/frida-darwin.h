#ifndef __FRIDA_DARWIN_H__
#define __FRIDA_DARWIN_H__

#ifdef HAVE_MACOS

#include <glib.h>
#include <xpc/xpc.h>

G_BEGIN_DECLS

typedef void (* FridaXpcHandler) (xpc_object_t object, gpointer user_data);
typedef gboolean (* FridaXpcDictionaryApplier) (const gchar * key, xpc_object_t val, gpointer user_data);

gpointer _frida_dispatch_retain (gpointer object);

void _frida_xpc_connection_set_event_handler (xpc_connection_t connection, FridaXpcHandler handler, gpointer user_data);
void _frida_xpc_connection_send_message_with_reply (xpc_connection_t connection, xpc_object_t message, dispatch_queue_t replyq,
    FridaXpcHandler handler, gpointer user_data, GDestroyNotify notify);
gchar * _frida_xpc_object_to_string (xpc_object_t object);
gboolean _frida_xpc_dictionary_apply (xpc_object_t dict, FridaXpcDictionaryApplier applier, gpointer user_data);

G_END_DECLS

#endif

#endif
