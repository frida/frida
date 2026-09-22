/*
 * Copyright (C) 2017-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "stalker-arm64-fixture.c"

#import <Foundation/Foundation.h>

TESTLIST_BEGIN (stalker_darwin)
  TESTENTRY (foundation)
TESTLIST_END ()

TESTCASE (foundation)
{
  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }

  fixture->sink->mask = GUM_CALL;

  gum_stalker_follow_me (fixture->stalker, fixture->transformer,
      GUM_EVENT_SINK (fixture->sink));

  @autoreleasepool
  {
    [NSDictionary dictionary];
  }

  gum_stalker_unfollow_me (fixture->stalker);

  g_assert_cmpuint (fixture->sink->events->length, >, 0);
}
