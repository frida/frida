/*
 * Copyright (C) 2009-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_X86_RELOCATOR_H__
#define __GUM_X86_RELOCATOR_H__

#include "gumx86writer.h"

#include <capstone.h>

G_BEGIN_DECLS

typedef struct _GumX86Relocator GumX86Relocator;

struct _GumX86Relocator
{
  volatile gint ref_count;

  csh capstone;

  const guint8 * input_start;
  const guint8 * input_cur;
  GumAddress input_pc;
  cs_insn ** input_insns;
  GumX86Writer * output;

  guint inpos;
  guint outpos;

  gboolean eob;
  gboolean eoi;
};

GUM_API GumX86Relocator * gum_x86_relocator_new (gconstpointer input_code,
    GumX86Writer * output);
GUM_API GumX86Relocator * gum_x86_relocator_ref (GumX86Relocator * relocator);
GUM_API void gum_x86_relocator_unref (GumX86Relocator * relocator);

GUM_API void gum_x86_relocator_init (GumX86Relocator * relocator,
    gconstpointer input_code, GumX86Writer * output);
GUM_API void gum_x86_relocator_clear (GumX86Relocator * relocator);

GUM_API void gum_x86_relocator_reset (GumX86Relocator * relocator,
    gconstpointer input_code, GumX86Writer * output);

GUM_API guint gum_x86_relocator_read_one (GumX86Relocator * self,
    const cs_insn ** instruction);

GUM_API cs_insn * gum_x86_relocator_peek_next_write_insn (
    GumX86Relocator * self);
GUM_API gpointer gum_x86_relocator_peek_next_write_source (
    GumX86Relocator * self);
GUM_API void gum_x86_relocator_skip_one (GumX86Relocator * self);
GUM_API void gum_x86_relocator_skip_one_no_label (GumX86Relocator * self);
GUM_API gboolean gum_x86_relocator_write_one (GumX86Relocator * self);
GUM_API gboolean gum_x86_relocator_write_one_no_label (GumX86Relocator * self);
GUM_API void gum_x86_relocator_write_all (GumX86Relocator * self);

GUM_API gboolean gum_x86_relocator_eob (GumX86Relocator * self);
GUM_API gboolean gum_x86_relocator_eoi (GumX86Relocator * self);

GUM_API gboolean gum_x86_relocator_can_relocate (gpointer address,
    guint min_bytes, GumRelocationScenario scenario, guint * maximum);
GUM_API guint gum_x86_relocator_relocate (gpointer from, guint min_bytes,
    gpointer to);

G_END_DECLS

#endif
