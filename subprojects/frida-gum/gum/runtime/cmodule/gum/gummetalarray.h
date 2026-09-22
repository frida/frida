#ifndef __GUM_METAL_ARRAY_H__
#define __GUM_METAL_ARRAY_H__

#include <glib.h>

typedef struct _GumMetalArray GumMetalArray;

struct _GumMetalArray
{
  gpointer data;
  guint length;
  guint capacity;

  guint element_size;
};

#endif
