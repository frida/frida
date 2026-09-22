#ifndef __FRIDA_SERVER_DARWIN_H__
#define __FRIDA_SERVER_DARWIN_H__

#include <glib.h>

G_BEGIN_DECLS

G_GNUC_INTERNAL void _frida_server_start_run_loop (void);
G_GNUC_INTERNAL void _frida_server_stop_run_loop (void);

G_END_DECLS

#endif
