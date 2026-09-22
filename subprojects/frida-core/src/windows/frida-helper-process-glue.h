#ifndef __FRIDA_HELPER_PROCESS_GLUE_H__
#define __FRIDA_HELPER_PROCESS_GLUE_H__

#include "frida-helper-backend.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL void * frida_helper_factory_spawn (const gchar * path, const gchar * parameters, FridaPrivilegeLevel level,
    GError ** error);

G_GNUC_INTERNAL gboolean frida_helper_instance_is_process_still_running (void * handle);
G_GNUC_INTERNAL void frida_helper_instance_close_process_handle (void * handle);

G_END_DECLS

#endif
