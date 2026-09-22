/*
 * Copyright (C) 2010-2017 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_TLS_H__
#define __GUM_TLS_H__

#include <gum/gumdefs.h>

G_BEGIN_DECLS

typedef gsize GumTlsKey;

GUM_API GumTlsKey gum_tls_key_new (void);
GUM_API void gum_tls_key_free (GumTlsKey key);

GUM_API gpointer gum_tls_key_get_value (GumTlsKey key);
GUM_API void gum_tls_key_set_value (GumTlsKey key, gpointer value);

G_END_DECLS

#endif
