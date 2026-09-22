/*
 * Copyright (C) 2014-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_MIPS_RELOCATOR_H__
#define __GUM_MIPS_RELOCATOR_H__

#include "gummipswriter.h"

#include <capstone.h>

G_BEGIN_DECLS

typedef struct _GumMipsRelocator GumMipsRelocator;

struct _GumMipsRelocator
{
  volatile gint ref_count;

  csh capstone;

  const guint8 * input_start;
  const guint8 * input_cur;
  GumAddress input_pc;
  cs_insn ** input_insns;
  GumMipsWriter * output;

  guint inpos;
  guint outpos;

  gboolean eob;
  gboolean eoi;
  gboolean delay_slot_pending;
};

GUM_API GumMipsRelocator * gum_mips_relocator_new (gconstpointer input_code,
    GumMipsWriter * output);
GUM_API GumMipsRelocator * gum_mips_relocator_ref (
    GumMipsRelocator * relocator);
GUM_API void gum_mips_relocator_unref (GumMipsRelocator * relocator);

GUM_API void gum_mips_relocator_init (GumMipsRelocator * relocator,
    gconstpointer input_code, GumMipsWriter * output);
GUM_API void gum_mips_relocator_clear (GumMipsRelocator * relocator);

GUM_API void gum_mips_relocator_reset (GumMipsRelocator * relocator,
    gconstpointer input_code, GumMipsWriter * output);

GUM_API guint gum_mips_relocator_read_one (GumMipsRelocator * self,
    const cs_insn ** instruction);

GUM_API cs_insn * gum_mips_relocator_peek_next_write_insn (
    GumMipsRelocator * self);
GUM_API gpointer gum_mips_relocator_peek_next_write_source (
    GumMipsRelocator * self);
GUM_API void gum_mips_relocator_skip_one (GumMipsRelocator * self);
GUM_API gboolean gum_mips_relocator_write_one (GumMipsRelocator * self);
GUM_API void gum_mips_relocator_write_all (GumMipsRelocator * self);

GUM_API gboolean gum_mips_relocator_eob (GumMipsRelocator * self);
GUM_API gboolean gum_mips_relocator_eoi (GumMipsRelocator * self);

GUM_API gboolean gum_mips_relocator_can_relocate (gpointer address,
    guint min_bytes, GumRelocationScenario scenario, GumRelocationPolicy policy,
    guint * maximum, mips_reg * available_scratch_reg);
GUM_API guint gum_mips_relocator_relocate (gpointer from, guint min_bytes,
    gpointer to);

G_END_DECLS

#endif
