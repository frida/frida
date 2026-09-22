/*
 * Copyright (C) 2010-2017 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_THUMB_RELOCATOR_H__
#define __GUM_THUMB_RELOCATOR_H__

#include "gumthumbwriter.h"

#include <capstone.h>

G_BEGIN_DECLS

typedef struct _GumThumbRelocator GumThumbRelocator;
typedef struct _GumITBlock GumITBlock;
typedef guint GumITBranchType;

struct _GumITBlock
{
  gboolean active;
  arm_cc cc;

  const cs_insn * insns[4];
  guint8 offset;
  guint8 size;
  guint8 else_region_size;

  gpointer then_label;
  gpointer end_label;
};

struct _GumThumbRelocator
{
  volatile gint ref_count;

  csh capstone;

  const guint8 * input_start;
  const guint8 * input_cur;
  GumAddress input_pc;
  cs_insn ** input_insns;
  GumThumbWriter * output;

  guint inpos;
  guint outpos;

  gboolean eob;
  gboolean eoi;

  GumITBlock it_block;
  GumITBranchType it_branch_type;
};

enum _GumITBranchType
{
  GUM_IT_BRANCH_INVALID,
  GUM_IT_BRANCH_SHORT,
  GUM_IT_BRANCH_LONG
};

GUM_API GumThumbRelocator * gum_thumb_relocator_new (gconstpointer input_code,
    GumThumbWriter * output);
GUM_API GumThumbRelocator * gum_thumb_relocator_ref (
    GumThumbRelocator * relocator);
GUM_API void gum_thumb_relocator_unref (GumThumbRelocator * relocator);

GUM_API void gum_thumb_relocator_init (GumThumbRelocator * relocator,
    gconstpointer input_code, GumThumbWriter * output);
GUM_API void gum_thumb_relocator_clear (GumThumbRelocator * relocator);

GUM_API void gum_thumb_relocator_reset (GumThumbRelocator * relocator,
    gconstpointer input_code, GumThumbWriter * output);
void gum_thumb_relocator_set_it_branch_type (GumThumbRelocator * self,
    GumITBranchType type);

GUM_API guint gum_thumb_relocator_read_one (GumThumbRelocator * self,
    const cs_insn ** instruction);

GUM_API gboolean gum_thumb_relocator_is_eob_instruction (
    const cs_insn * instruction);

GUM_API cs_insn * gum_thumb_relocator_peek_next_write_insn (
    GumThumbRelocator * self);
GUM_API gpointer gum_thumb_relocator_peek_next_write_source (
    GumThumbRelocator * self);
GUM_API void gum_thumb_relocator_skip_one (GumThumbRelocator * self);
GUM_API gboolean gum_thumb_relocator_write_one (GumThumbRelocator * self);
GUM_API gboolean gum_thumb_relocator_copy_one (GumThumbRelocator * self);
GUM_API void gum_thumb_relocator_write_all (GumThumbRelocator * self);

GUM_API gboolean gum_thumb_relocator_eob (GumThumbRelocator * self);
GUM_API gboolean gum_thumb_relocator_eoi (GumThumbRelocator * self);

GUM_API gboolean gum_thumb_relocator_can_relocate (gpointer address,
    guint min_bytes, GumRelocationScenario scenario, guint * maximum);
GUM_API guint gum_thumb_relocator_relocate (gpointer from, guint min_bytes,
    gpointer to);

G_END_DECLS

#endif
