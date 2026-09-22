/*
* Copyright (C) 2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
*
* Licence: wxWindows Library Licence, Version 3.1
*/

#include "gumtls.h"

G_GNUC_WEAK GumTlsKey
gum_tls_key_new (void)
{
  G_PANIC_MISSING_IMPLEMENTATION ();
  return 0;
}

G_GNUC_WEAK void
gum_tls_key_free (GumTlsKey key)
{
  G_PANIC_MISSING_IMPLEMENTATION ();
}

void
_gum_tls_init (void)
{
}

void
_gum_tls_realize (void)
{
}

void
_gum_tls_deinit (void)
{
}

G_GNUC_WEAK gpointer
gum_tls_key_get_value (GumTlsKey key)
{
  G_PANIC_MISSING_IMPLEMENTATION ();
  return NULL;
}

G_GNUC_WEAK void
gum_tls_key_set_value (GumTlsKey key,
                       gpointer value)
{
  G_PANIC_MISSING_IMPLEMENTATION ();
}
