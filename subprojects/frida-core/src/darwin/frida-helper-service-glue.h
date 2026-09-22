#ifndef __FRIDA_HELPER_SERVICE_GLUE_H__
#define __FRIDA_HELPER_SERVICE_GLUE_H__

#include <glib.h>

G_BEGIN_DECLS

G_GNUC_INTERNAL void _frida_start_run_loop (void);
G_GNUC_INTERNAL void _frida_stop_run_loop (void);

G_END_DECLS

#endif
