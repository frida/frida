#include "frida-base.h"

GBytes *
frida_make_bytes_with_owner (GType t_type,
                             GBoxedCopyFunc t_dup_func,
                             GDestroyNotify t_destroy_func,
                             void * data,
                             gsize size,
                             gpointer owner)
{
  return g_bytes_new_with_free_func (data, size, t_destroy_func, owner);
}
