#include <glib.h>

#ifdef _MSC_VER
# define GUM_NOINLINE __declspec (noinline)
#else
# define GUM_NOINLINE __attribute__ ((noinline))
#endif

gpointer GUM_NOINLINE
gum_test_special_function (GString * str)
{
  if (str != NULL)
    g_string_append_c (str, '|');
  else
    g_usleep (G_USEC_PER_SEC / 100);

  return NULL;
}
