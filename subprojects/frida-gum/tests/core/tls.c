/*
 * Copyright (C) 2015 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "testutil.h"

#ifdef HAVE_WINDOWS
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
#else
# include <pthread.h>
#endif

#define TESTCASE(NAME) \
    void test_tls_ ## NAME (void)
#define TESTENTRY(NAME) \
    TESTENTRY_SIMPLE ("Core/Tls", test_tls, NAME)

TESTLIST_BEGIN (tls)
  TESTENTRY (get_should_work_like_the_system_implementation)
  TESTENTRY (set_should_work_like_the_system_implementation)
TESTLIST_END ()

TESTCASE (get_should_work_like_the_system_implementation)
{
  GumTlsKey key;
  gsize val = 0x11223344;

  key = gum_tls_key_new ();

#ifdef HAVE_WINDOWS
  TlsSetValue (key, &val);
#else
  pthread_setspecific (key, &val);
#endif
  g_assert_cmphex (GPOINTER_TO_SIZE (gum_tls_key_get_value (key)),
      ==, GPOINTER_TO_SIZE (&val));

  gum_tls_key_free (key);
}

TESTCASE (set_should_work_like_the_system_implementation)
{
  GumTlsKey key;
  gsize val = 0x11223344;

  key = gum_tls_key_new ();

  gum_tls_key_set_value (key, &val);
#ifdef HAVE_WINDOWS
  g_assert_cmphex (GPOINTER_TO_SIZE (TlsGetValue (key)),
      ==, GPOINTER_TO_SIZE (&val));
#else
  g_assert_cmphex (GPOINTER_TO_SIZE (pthread_getspecific (key)),
      ==, GPOINTER_TO_SIZE (&val));
#endif

  gum_tls_key_free (key);
}
