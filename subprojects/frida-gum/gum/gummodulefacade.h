/*
 * Copyright (C) 2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_MODULE_FACADE_H__
#define __GUM_MODULE_FACADE_H__

#include <gum/gummodule.h>

G_BEGIN_DECLS

#define GUM_TYPE_MODULE_FACADE (gum_module_facade_get_type ())
G_DECLARE_FINAL_TYPE (GumModuleFacade, gum_module_facade, GUM, MODULE_FACADE,
                      GObject)

G_GNUC_INTERNAL GumModuleFacade * _gum_module_facade_new (GumModule * module,
    GObject * resolver);

G_GNUC_INTERNAL GumModule * _gum_module_facade_get_module (
    GumModuleFacade * self);

G_END_DECLS

#endif
