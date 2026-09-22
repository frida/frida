#ifndef __FRIDA_SERVER_GLUE_H__
#define __FRIDA_SERVER_GLUE_H__

#include <gio/gio.h>

G_BEGIN_DECLS

G_GNUC_INTERNAL void frida_server_environment_init (void);
G_GNUC_INTERNAL void frida_server_environment_set_verbose_logging_enabled (gboolean enabled);
G_GNUC_INTERNAL void frida_server_environment_configure (void);

G_END_DECLS

#endif
