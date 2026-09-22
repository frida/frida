#include "get-prgname.h"

#include <glib.h>

const char *get_prgname_get_name (void)
{
  return g_get_prgname ();
}
