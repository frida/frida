/*
 * Copyright (C) 2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2024 Håvard Sørbø <havard@hsorbo.no>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumprocess-priv.h"

#define GUM_WATCHLO_W ((guint32) (1U << 0))
#define GUM_WATCHLO_R ((guint32) (1U << 1))
#define GUM_WATCHLO_I ((guint32) (1U << 2))

static void gum_compute_base_and_mask (GumAddress address, gsize size,
    GumAddress * base, GumAddress * mask);
static GumAddress gum_set_low_bits_in_mask (GumAddress mask);

void
_gum_mips_set_breakpoint (gsize * watch_lo,
                          guint16 * watch_hi,
                          guint breakpoint_id,
                          GumAddress address)
{
  GumAddress base, mask;

  gum_compute_base_and_mask (address, 4, &base, &mask);

  watch_lo[breakpoint_id] = (base << 3) | GUM_WATCHLO_I;
  watch_hi[breakpoint_id] = mask << 3;
}

void
_gum_mips_unset_breakpoint (gsize * watch_lo,
                            guint16 * watch_hi,
                            guint breakpoint_id)
{
  watch_lo[breakpoint_id] = 0;
  watch_hi[breakpoint_id] = 0;
}

void
_gum_mips_set_watchpoint (gsize * watch_lo,
                          guint16 * watch_hi,
                          guint watchpoint_id,
                          GumAddress address,
                          gsize size,
                          GumWatchConditions conditions)
{
  GumAddress base, mask;

  gum_compute_base_and_mask (address, size, &base, &mask);

  watch_lo[watchpoint_id] =
      (base << 3) |
      (((conditions & GUM_WATCH_READ) != 0) ? GUM_WATCHLO_R : 0U) |
      (((conditions & GUM_WATCH_WRITE) != 0) ? GUM_WATCHLO_W : 0U);
  watch_hi[watchpoint_id] = mask << 3;
}

void
_gum_mips_unset_watchpoint (gsize * watch_lo,
                            guint16 * watch_hi,
                            guint watchpoint_id)
{
  watch_lo[watchpoint_id] = 0;
  watch_hi[watchpoint_id] = 0;
}

static void
gum_compute_base_and_mask (GumAddress address,
                           gsize size,
                           GumAddress * base,
                           GumAddress * mask)
{
  GumAddress upper_bound;

  upper_bound = address + size - 1;

  *mask = gum_set_low_bits_in_mask (address ^ upper_bound) >> 3;
  *base = (address >> 3) & ~*mask;
}

static GumAddress
gum_set_low_bits_in_mask (GumAddress mask)
{
  GumAddress result, bit;

  result = mask;
  for (bit = 1; bit != 0 && bit < result; bit <<= 1)
    result |= bit;

  return result;
}
