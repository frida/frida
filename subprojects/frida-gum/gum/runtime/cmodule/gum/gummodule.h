#ifndef __GUM_MODULE_H__
#define __GUM_MODULE_H__

#include "gumdefs.h"

typedef struct _GumModule GumModule;

const gchar * gum_module_get_name (GumModule * self);
const gchar * gum_module_get_path (GumModule * self);
const GumMemoryRange * gum_module_get_range (GumModule * self);

#endif
