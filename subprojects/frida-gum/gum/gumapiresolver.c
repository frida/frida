/*
 * Copyright (C) 2016-2023 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2023 Håvard Sørbø <havard@hsorbo.no>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

/**
 * GumApiResolver:
 *
 * Resolves in-memory APIs by name, with globs permitted.
 *
 * ## Using `GumApiResolver`
 *
 * ### Exports and imports
 *
 * ```c
 * void
 * start (void)
 * {
 *   g_autoptr(GumApiResolver) resolver = gum_api_resolver_make ("module");
 *
 *   gum_api_resolver_enumerate_matches (resolver,
 *                                       "exports:libc*.so!open*",
 *                                       // case-insensitive: "exports:*!open/i"
 *                                       // imports: "imports:example.so!open*"
 *                                       instrument_c_function,
 *                                       NULL,
 *                                       NULL);
 * }
 *
 * static gboolean
 * instrument_c_function (const GumApiDetails *details,
 *                        gpointer user_data)
 * {
 *   g_print ("Found %s at %" G_GINT64_MODIFIER "x\n",
 *            details->name,
 *            details->address);
 *   // e.g.: "Found /system/lib/libc.so at 0x7fff870135c9"
 *
 *   return TRUE; // keep enumerating
 * }
 * ```
 *
 * ### Objective-C methods
 *
 * ```c
 * void
 * start (void)
 * {
 *   g_autoptr(GumApiResolver) resolver = gum_api_resolver_make ("objc");
 *
 *   gum_api_resolver_enumerate_matches (resolver,
 *                                       "-[NSURL* *HTTP*]",
 *                                       instrument_objc_method,
 *                                       NULL,
 *                                       NULL);
 * }
 *
 * static gboolean
 * instrument_objc_method (const GumApiDetails *details,
 *                         gpointer user_data)
 * {
 *   g_print ("Found %s at %" G_GINT64_MODIFIER "x\n",
 *            details->name,
 *            details->address);
 *   // e.g.: "Found -[NSURLRequest valueForHTTPHeaderField:] at 0x7fff94183e22"
 *
 *   return TRUE; // keep enumerating
 * }
 * ```
 */

#include "gumapiresolver.h"

#include "gummoduleapiresolver.h"
#include "gumswiftapiresolver.h"
#ifdef HAVE_DARWIN
# include "backend-darwin/gumobjcapiresolver.h"
#endif

#include <string.h>

G_DEFINE_INTERFACE (GumApiResolver, gum_api_resolver, G_TYPE_OBJECT)

static void
gum_api_resolver_default_init (GumApiResolverInterface * iface)
{
}

/**
 * gum_api_resolver_make:
 * @type: (not nullable): the resolver type to make
 *
 * Creates a new resolver of the given `type`. Available resolvers:
 *
 *  - `module`: Resolves exported and imported functions of shared libraries
 *    currently loaded. Always available.
 *  - `objc`: Resolves Objective-C methods of classes currently loaded. Available
 *    on macOS and iOS in processes that have the Objective-C runtime loaded.
 *
 * The resolver will load the minimum amount of data required on creation, and
 * lazy-load the rest depending on the queries it receives. You should use the
 * same instance for a batch of queries, but recreate it for future batches to
 * avoid looking at stale data.
 *
 * Returns: (nullable) (transfer full): the newly created resolver instance
 */
GumApiResolver *
gum_api_resolver_make (const gchar * type)
{
  if (strcmp (type, "module") == 0)
    return gum_module_api_resolver_new ();

  if (strcmp (type, "swift") == 0)
    return gum_swift_api_resolver_new ();

#ifdef HAVE_DARWIN
  if (strcmp (type, "objc") == 0)
    return gum_objc_api_resolver_new ();
#endif

  return NULL;
}

/**
 * gum_api_resolver_enumerate_matches:
 * @self: a resolver
 * @query: (not nullable): the query to perform
 * @func: (not nullable) (scope call): the function called with each match
 * @user_data: (nullable): the data to pass to `func`
 * @error: (inout) (nullable) (optional): the return location for a #GError
 *
 * Performs the resolver-specific `query`, optionally suffixed with `/i` to
 * perform case-insensitive matching. Calls `func` with each match found.
 */
void
gum_api_resolver_enumerate_matches (GumApiResolver * self,
                                    const gchar * query,
                                    GumFoundApiFunc func,
                                    gpointer user_data,
                                    GError ** error)
{
  GUM_API_RESOLVER_GET_IFACE (self)->enumerate_matches (self, query, func,
      user_data, error);
}
