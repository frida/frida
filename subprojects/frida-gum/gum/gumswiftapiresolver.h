/*
 * Copyright (C) 2023 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2023 Håvard Sørbø <havard@hsorbo.no>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_SWIFT_API_RESOLVER_H__
#define __GUM_SWIFT_API_RESOLVER_H__

#include <gum/gumapiresolver.h>

G_BEGIN_DECLS

#define GUM_TYPE_SWIFT_API_RESOLVER (gum_swift_api_resolver_get_type ())
G_DECLARE_FINAL_TYPE (GumSwiftApiResolver, gum_swift_api_resolver, GUM,
                      SWIFT_API_RESOLVER, GObject)

GUM_API GumApiResolver * gum_swift_api_resolver_new (void);

G_END_DECLS

#endif
