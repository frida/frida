/*
 * Copyright (C) 2015-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumtls.h"

#include <pthread.h>

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

GumTlsKey
gum_tls_key_new (void)
{
  pthread_key_t key;
  G_GNUC_UNUSED gint res;

  res = pthread_key_create (&key, NULL);
  g_assert (res == 0);

  return key;
}

void
gum_tls_key_free (GumTlsKey key)
{
  pthread_key_delete (key);
}

gpointer
gum_tls_key_get_value (GumTlsKey key)
{
  gpointer result;

#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
  asm (
      "movl %%gs:(,%1,4), %0\n\t"
      : "=r" (result)
      : "r" (key));
#elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
  asm (
      "movq %%gs:(,%1,8), %0\n\t"
      : "=r" (result)
      : "r" (key));
#elif defined (HAVE_ARM)
  gsize tls_base_value;
  gpointer * tls_base;

  asm (
      "mrc p15, #0x0, %0, c13, c0, #0x3\n\t"
      : "=r" (tls_base_value));
  tls_base = (gpointer *) (tls_base_value & ~((gsize) 3));
  result = tls_base[key];
#elif defined (HAVE_ARM64)
  gsize tls_base_value;
  gpointer * tls_base;

  asm (
      "mrs %0, TPIDRRO_EL0\n\t"
      : "=r" (tls_base_value));
  tls_base = (gpointer *) (tls_base_value & ~((gsize) 7));
  result = tls_base[key];
#endif

  return result;
}

void
gum_tls_key_set_value (GumTlsKey key,
                       gpointer value)
{
#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
  asm (
      "movl %1, %%gs:(,%0,4)\n\t"
      :
      : "r" (key), "r" (value));
#elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
  asm (
      "movq %1, %%gs:(,%0,8)\n\t"
      :
      : "r" (key), "r" (value));
#elif defined (HAVE_ARM)
  gsize tls_base_value;
  gpointer * tls_base;

  asm (
      "mrc p15, #0x0, %0, c13, c0, #0x3\n\t"
      : "=r" (tls_base_value));
  tls_base = (gpointer *) (tls_base_value & ~((gsize) 3));
  tls_base[key] = value;
#elif defined (HAVE_ARM64)
  gsize tls_base_value;
  gpointer * tls_base;

  asm (
      "mrs %0, TPIDRRO_EL0\n\t"
      : "=r" (tls_base_value));
  tls_base = (gpointer *) (tls_base_value & ~((gsize) 7));
  tls_base[key] = value;
#endif
}
