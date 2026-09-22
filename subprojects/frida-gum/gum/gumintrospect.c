/*
 * Copyright (C) 2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gum.h"

#include <girepository.h>
#include <string.h>

gint
main (gint argc,
      gchar * argv[])
{
  const gchar * expected_prefix = "--introspect-dump=";
  GError * error = NULL;

  if (argc != 2 || !g_str_has_prefix (argv[1], expected_prefix))
    goto bad_usage;

  gum_init ();

  if (!g_irepository_dump (argv[1] + strlen (expected_prefix), &error))
    goto dump_failed;

  return 0;

bad_usage:
  {
    g_printerr ("usage: %s --introspect-dump=types.txt,out.xml\\n", argv[0]);
    return 1;
  }
dump_failed:
  {
    g_printerr ("%s\n", error->message);
    return 1;
  }
}
