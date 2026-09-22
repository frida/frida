/*
 * Copyright (C) 2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

typedef void (* GumTestUnwindCallback) (void * user_data);

static volatile unsigned int gum_test_mapped_sink = 0;

unsigned int
gum_test_mapped_invoke (GumTestUnwindCallback callback,
                        void * user_data)
{
  callback (user_data);

  gum_test_mapped_sink++;

  return gum_test_mapped_sink;
}
