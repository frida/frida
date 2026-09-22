#ifndef __GUM_TLS_H__
#define __GUM_TLS_H__

#include <glib.h>

typedef gsize GumTlsKey;

GumTlsKey gum_tls_key_new (void);
void gum_tls_key_free (GumTlsKey key);

gpointer gum_tls_key_get_value (GumTlsKey key);
void gum_tls_key_set_value (GumTlsKey key, gpointer value);

#endif
