/*
 * Copyright (C) 2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2024 Håvard Sørbø <havard@hsorbo.no>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumprocess-priv.h"

#define GUM_BCR_ENABLE ((guint32) (1U << 0))

#define GUM_WCR_ENABLE ((guint32) (1U << 0))
#define GUM_WCR_LOAD   ((guint32) (1U << 3))
#define GUM_WCR_STORE  ((guint32) (1U << 4))

#define GUM_BAS_ANY ((guint32) 15U)

#define GUM_S_USER ((guint32) (2U << 1))

void
_gum_arm_set_breakpoint (guint32 * bcr,
                         guint32 * bvr,
                         guint breakpoint_id,
                         GumAddress address)
{
  bcr[breakpoint_id] =
      (GUM_BAS_ANY << 5) |
      GUM_S_USER |
      GUM_BCR_ENABLE;
  bvr[breakpoint_id] = address & ~1;
}

void
_gum_arm_unset_breakpoint (guint32 * bcr,
                           guint32 * bvr,
                           guint breakpoint_id)
{
  bcr[breakpoint_id] = 0;
  bvr[breakpoint_id] = 0;
}

void
_gum_arm_set_watchpoint (guint32 * wcr,
                         guint32 * wvr,
                         guint watchpoint_id,
                         GumAddress address,
                         gsize size,
                         GumWatchConditions conditions)
{
  guint32 aligned_address;
  guint32 offset, byte_address_select;

  aligned_address = address & ~7U;
  offset = address & 7U;

  byte_address_select = ((1 << size) - 1) << offset;

  wcr[watchpoint_id] =
      (byte_address_select << 5) |
      (((conditions & GUM_WATCH_WRITE) != 0) ? GUM_WCR_STORE : 0U) |
      (((conditions & GUM_WATCH_READ) != 0) ? GUM_WCR_LOAD : 0U) |
      GUM_S_USER |
      GUM_WCR_ENABLE;
  wvr[watchpoint_id] = aligned_address;
}

void
_gum_arm_unset_watchpoint (guint32 * wcr,
                           guint32 * wvr,
                           guint watchpoint_id)
{
  wcr[watchpoint_id] = 0;
  wvr[watchpoint_id] = 0;
}
