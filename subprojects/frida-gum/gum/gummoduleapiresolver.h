/*
 * Copyright (C) 2016-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_MODULE_API_RESOLVER_H__
#define __GUM_MODULE_API_RESOLVER_H__

#include <gum/gumapiresolver.h>

G_BEGIN_DECLS

#define GUM_TYPE_MODULE_API_RESOLVER (gum_module_api_resolver_get_type ())
G_DECLARE_FINAL_TYPE (GumModuleApiResolver, gum_module_api_resolver, GUM,
                      MODULE_API_RESOLVER, GObject)

GUM_API GumApiResolver * gum_module_api_resolver_new (void);

G_END_DECLS

#endif
