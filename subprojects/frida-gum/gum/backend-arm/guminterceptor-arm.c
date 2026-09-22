/*
 * Copyright (C) 2010-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2025 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "guminterceptor-priv.h"

#include "gumarmreader.h"
#include "gumarmrelocator.h"
#include "gumarmwriter.h"
#include "gumlibc.h"
#include "gummemory.h"
#include "gumthumbreader.h"
#include "gumthumbrelocator.h"
#include "gumthumbwriter.h"

#include <string.h>
#include <unistd.h>

#define GUM_INTERCEPTOR_ARM_FULL_REDIRECT_SIZE   (4 + 4)
#define GUM_INTERCEPTOR_ARM_TINY_REDIRECT_SIZE   (4)
#define GUM_INTERCEPTOR_THUMB_FULL_REDIRECT_SIZE (8)
#define GUM_INTERCEPTOR_THUMB_LINK_REDIRECT_SIZE (6)
#define GUM_INTERCEPTOR_THUMB_TINY_REDIRECT_SIZE (4)

#define FUNCTION_CONTEXT_ADDRESS_IS_THUMB(ctx) ( \
    (GPOINTER_TO_SIZE (ctx->function_address) & 0x1) == 0x1)

#define GUM_FRAME_OFFSET_NEXT_HOP 0
#define GUM_FRAME_OFFSET_CPU_CONTEXT \
    (GUM_FRAME_OFFSET_NEXT_HOP + (2 * sizeof (gpointer)))

#define GUM_FCDATA(context) \
    ((GumArmFunctionContextData *) (context)->backend_data.storage)

typedef struct _GumArmFunctionContextData GumArmFunctionContextData;

struct _GumInterceptorBackend
{
  GumCodeAllocator * allocator;

  GumArmWriter arm_writer;
  GumArmRelocator arm_relocator;

  GumThumbWriter thumb_writer;
  GumThumbRelocator thumb_relocator;

  GumCodeSlice * arm_thunks;
  GumCodeSlice * thumb_thunks;

  gpointer enter_thunk_arm;
  gpointer enter_thunk_thumb;
  gpointer leave_thunk_arm;
  gpointer leave_thunk_thumb;
};

struct _GumArmFunctionContextData
{
  guint full_redirect_size;
  guint redirect_code_size;
  guint available_space;
};

G_STATIC_ASSERT (sizeof (GumArmFunctionContextData)
    <= sizeof (GumFunctionContextBackendData));

static gboolean gum_interceptor_backend_emit_arm_trampolines (
    GumInterceptorBackend * self, GumFunctionContext * ctx,
    gpointer function_address);
static gboolean gum_interceptor_backend_emit_thumb_trampolines (
    GumInterceptorBackend * self, GumFunctionContext * ctx,
    gpointer function_address);
static gboolean gum_interceptor_backend_write_arm_custom_redirect (
    GumInterceptorBackend * self, GumFunctionContext * ctx, gpointer target);
static gboolean gum_interceptor_backend_write_thumb_custom_redirect (
    GumInterceptorBackend * self, GumFunctionContext * ctx, gpointer target);

static void gum_interceptor_backend_create_thunks (
    GumInterceptorBackend * self);
static void gum_interceptor_backend_destroy_thunks (
    GumInterceptorBackend * self);

static void gum_emit_arm_enter_thunk (GumArmWriter * aw);
static void gum_emit_thumb_enter_thunk (GumThumbWriter * tw);
static void gum_emit_arm_leave_thunk (GumArmWriter * aw);
static void gum_emit_thumb_leave_thunk (GumThumbWriter * tw);

static void gum_emit_arm_push_cpu_context_high_part (GumArmWriter * aw);
static void gum_emit_thumb_push_cpu_context_high_part (GumThumbWriter * tw);
static void gum_emit_arm_prolog (GumArmWriter * aw);
static void gum_emit_thumb_prolog (GumThumbWriter * tw);
static void gum_emit_arm_epilog (GumArmWriter * aw);
static void gum_emit_thumb_epilog (GumThumbWriter * tw);

GumInterceptorBackend *
_gum_interceptor_backend_create (GRecMutex * mutex,
                                 GumCodeAllocator * allocator)
{
  GumInterceptorBackend * backend;

  backend = g_slice_new (GumInterceptorBackend);
  backend->allocator = allocator;

  gum_arm_writer_init (&backend->arm_writer, NULL);
  backend->arm_writer.cpu_features = gum_query_cpu_features ();
  gum_arm_relocator_init (&backend->arm_relocator, NULL, &backend->arm_writer);

  gum_thumb_writer_init (&backend->thumb_writer, NULL);
  gum_thumb_relocator_init (&backend->thumb_relocator, NULL,
      &backend->thumb_writer);

  gum_interceptor_backend_create_thunks (backend);

  return backend;
}

void
_gum_interceptor_backend_destroy (GumInterceptorBackend * backend)
{
  gum_interceptor_backend_destroy_thunks (backend);

  gum_thumb_relocator_clear (&backend->thumb_relocator);
  gum_thumb_writer_clear (&backend->thumb_writer);

  gum_arm_relocator_clear (&backend->arm_relocator);
  gum_arm_writer_clear (&backend->arm_writer);

  g_slice_free (GumInterceptorBackend, backend);
}

gboolean
_gum_interceptor_backend_claim_grafted_trampoline (GumInterceptorBackend * self,
                                                   GumFunctionContext * ctx)
{
  return FALSE;
}

static gboolean
gum_interceptor_backend_prepare_trampoline (GumInterceptorBackend * self,
                                            GumFunctionContext * ctx,
                                            gboolean force)
{
  GumArmFunctionContextData * data = GUM_FCDATA (ctx);
  gpointer function_address;
  gboolean is_thumb;
  GumRelocationScenario scenario;
  guint redirect_limit;

  function_address = _gum_interceptor_backend_get_function_address (ctx);
  is_thumb = FUNCTION_CONTEXT_ADDRESS_IS_THUMB (ctx);
  scenario = (ctx->scenario == GUM_INTERCEPTOR_SCENARIO_OFFLINE)
      ? GUM_SCENARIO_OFFLINE
      : GUM_SCENARIO_ONLINE;

  if (ctx->write_redirect != NULL)
  {
    guint scan_bytes;

    scan_bytes = (ctx->redirect_space_hint != 0)
        ? ctx->redirect_space_hint
        : GUM_INTERCEPTOR_MAX_REDIRECT_SIZE;
    if (is_thumb)
    {
      data->full_redirect_size = GUM_INTERCEPTOR_THUMB_FULL_REDIRECT_SIZE;
      gum_thumb_relocator_can_relocate (function_address, scan_bytes, scenario,
          &data->available_space);
    }
    else
    {
      data->full_redirect_size = GUM_INTERCEPTOR_ARM_FULL_REDIRECT_SIZE;
      gum_arm_relocator_can_relocate (function_address, scan_bytes,
          &data->available_space);
    }
    if (ctx->redirect_space_hint != 0 &&
        data->available_space > ctx->redirect_space_hint)
      data->available_space = ctx->redirect_space_hint;
    if (data->available_space == 0)
      return FALSE;

    data->redirect_code_size = data->full_redirect_size;
    ctx->trampoline_slice = gum_code_allocator_alloc_slice (self->allocator);
    ctx->redirect_code = g_malloc (data->available_space);

    return TRUE;
  }

  if (is_thumb)
  {
    data->full_redirect_size = GUM_INTERCEPTOR_THUMB_FULL_REDIRECT_SIZE;
    if ((GPOINTER_TO_SIZE (function_address) & 3) != 0)
      data->full_redirect_size += 2;

    if (gum_thumb_relocator_can_relocate (function_address,
          data->full_redirect_size, scenario, &redirect_limit))
    {
      data->redirect_code_size = data->full_redirect_size;
    }
    else if (force)
    {
      data->redirect_code_size = data->full_redirect_size;
    }
    else
    {
      if (redirect_limit >= GUM_INTERCEPTOR_THUMB_LINK_REDIRECT_SIZE)
        data->redirect_code_size = GUM_INTERCEPTOR_THUMB_LINK_REDIRECT_SIZE;
      else if (redirect_limit >= GUM_INTERCEPTOR_THUMB_TINY_REDIRECT_SIZE)
        data->redirect_code_size = GUM_INTERCEPTOR_THUMB_TINY_REDIRECT_SIZE;
      else
        return FALSE;
    }
  }
  else
  {
    data->full_redirect_size = GUM_INTERCEPTOR_ARM_FULL_REDIRECT_SIZE;

    if (gum_arm_relocator_can_relocate (function_address,
        data->full_redirect_size, &redirect_limit))
    {
      data->redirect_code_size = data->full_redirect_size;
    }
    else if (force)
    {
      data->redirect_code_size = data->full_redirect_size;
    }
    else
    {
      if (redirect_limit >= GUM_INTERCEPTOR_ARM_TINY_REDIRECT_SIZE)
        data->redirect_code_size = GUM_INTERCEPTOR_ARM_TINY_REDIRECT_SIZE;
      else
        return FALSE;
    }
  }

  ctx->trampoline_slice = gum_code_allocator_alloc_slice (self->allocator);
  return TRUE;
}

gboolean
_gum_interceptor_backend_create_trampoline (GumInterceptorBackend * self,
                                            GumFunctionContext * ctx,
                                            gboolean force)
{
  gpointer func;
  gboolean success;

  func = _gum_interceptor_backend_get_function_address (ctx);

  if (!gum_interceptor_backend_prepare_trampoline (self, ctx, force))
    return FALSE;

  if (FUNCTION_CONTEXT_ADDRESS_IS_THUMB (ctx))
    success = gum_interceptor_backend_emit_thumb_trampolines (self, ctx, func);
  else
    success = gum_interceptor_backend_emit_arm_trampolines (self, ctx, func);
  if (!success)
    return FALSE;

  ctx->overwritten_prologue = g_malloc (ctx->overwritten_prologue_len);
  gum_memcpy (ctx->overwritten_prologue, func, ctx->overwritten_prologue_len);

  return TRUE;
}

static gboolean
gum_interceptor_backend_emit_arm_trampolines (GumInterceptorBackend * self,
                                              GumFunctionContext * ctx,
                                              gpointer function_address)
{
  GumArmFunctionContextData * data = GUM_FCDATA (ctx);
  GumArmWriter * aw = &self->arm_writer;
  GumArmRelocator * ar = &self->arm_relocator;
  gpointer deflector_target;
  guint reloc_bytes;

  gum_arm_writer_reset (aw, ctx->trampoline_slice->data);
  aw->pc = GUM_ADDRESS (ctx->trampoline_slice->pc);

  if (ctx->type == GUM_INTERCEPTOR_TYPE_FAST)
  {
    deflector_target = ctx->replacement_function;
  }
  else
  {
    ctx->on_enter_trampoline =
        ctx->trampoline_slice->pc + gum_arm_writer_offset (aw);
    deflector_target = ctx->on_enter_trampoline;
  }

  if (ctx->write_redirect != NULL &&
      !gum_interceptor_backend_write_arm_custom_redirect (self, ctx,
        deflector_target))
  {
    gum_code_slice_unref (ctx->trampoline_slice);
    ctx->trampoline_slice = NULL;
    return FALSE;
  }

  if (ctx->write_redirect == NULL &&
      data->redirect_code_size != data->full_redirect_size)
  {
    GumAddressSpec caller;
    gpointer return_address;
    gboolean dedicated;

    caller.near_address = function_address + data->redirect_code_size + 4;
    caller.max_distance = GUM_ARM_B_MAX_DISTANCE;

    return_address = function_address + data->redirect_code_size;

    dedicated = TRUE;

    ctx->trampoline_deflector = gum_code_allocator_alloc_deflector (
        self->allocator, &caller, return_address, deflector_target, dedicated);
    if (ctx->trampoline_deflector == NULL)
    {
      gum_code_slice_unref (ctx->trampoline_slice);
      ctx->trampoline_slice = NULL;
      return FALSE;
    }
  }

  if (ctx->type != GUM_INTERCEPTOR_TYPE_FAST)
  {
    gum_emit_arm_push_cpu_context_high_part (aw);
    gum_arm_writer_put_ldr_reg_address (aw, ARM_REG_R6, GUM_ADDRESS (ctx));
    gum_arm_writer_put_ldr_reg_address (aw, ARM_REG_PC,
        GUM_ADDRESS (self->enter_thunk_arm));

    ctx->on_leave_trampoline =
        ctx->trampoline_slice->pc + gum_arm_writer_offset (aw);

    gum_emit_arm_push_cpu_context_high_part (aw);
    gum_arm_writer_put_ldr_reg_address (aw, ARM_REG_R6, GUM_ADDRESS (ctx));
    gum_arm_writer_put_ldr_reg_address (aw, ARM_REG_PC,
        GUM_ADDRESS (self->leave_thunk_arm));

    gum_arm_writer_flush (aw);
    g_assert (gum_arm_writer_offset (aw) <= ctx->trampoline_slice->size);
  }

  ctx->on_invoke_trampoline =
      ctx->trampoline_slice->pc + gum_arm_writer_offset (aw);

  gum_arm_relocator_reset (ar, function_address, aw);

  do
  {
    reloc_bytes = gum_arm_relocator_read_one (ar, NULL);
    if (reloc_bytes == 0)
      reloc_bytes = data->redirect_code_size;
  }
  while (reloc_bytes < data->redirect_code_size);

  gum_arm_relocator_write_all (ar);

  if (!gum_arm_relocator_eoi (ar))
  {
    gum_arm_writer_put_ldr_reg_address (aw, ARM_REG_PC,
        GUM_ADDRESS (function_address + reloc_bytes));
  }

  gum_arm_writer_flush (aw);
  g_assert (gum_arm_writer_offset (aw) <= ctx->trampoline_slice->size);

  ctx->overwritten_prologue_len = reloc_bytes;

  return TRUE;
}

static gboolean
gum_interceptor_backend_emit_thumb_trampolines (GumInterceptorBackend * self,
                                                GumFunctionContext * ctx,
                                                gpointer function_address)
{
  GumArmFunctionContextData * data = GUM_FCDATA (ctx);
  GumThumbWriter * tw = &self->thumb_writer;
  GumThumbRelocator * tr = &self->thumb_relocator;
  gpointer deflector_target;
  GString * signature;
  const cs_insn * insn, * trailing_bl;
  guint reloc_bytes;
  gboolean is_branch_back_needed;
  gboolean is_eligible_for_lr_rewriting;

  gum_thumb_writer_reset (tw, ctx->trampoline_slice->data);
  tw->pc = GUM_ADDRESS (ctx->trampoline_slice->pc);

  if (ctx->type == GUM_INTERCEPTOR_TYPE_FAST)
  {
    deflector_target = ctx->replacement_function;
  }
  else
  {
    ctx->on_enter_trampoline =
        (guint8 *) ctx->trampoline_slice->pc + gum_thumb_writer_offset (tw) + 1;
    deflector_target = ctx->on_enter_trampoline;
  }

  if (ctx->write_redirect != NULL &&
      !gum_interceptor_backend_write_thumb_custom_redirect (self, ctx,
        deflector_target))
  {
    gum_code_slice_unref (ctx->trampoline_slice);
    ctx->trampoline_slice = NULL;
    return FALSE;
  }

  if (ctx->write_redirect == NULL &&
      data->redirect_code_size != data->full_redirect_size)
  {
    GumAddressSpec caller;
    gpointer return_address;
    gboolean dedicated;

    caller.near_address = function_address + data->redirect_code_size;
    caller.max_distance = GUM_THUMB_B_MAX_DISTANCE;

    return_address = function_address + data->redirect_code_size + 1;

    dedicated =
        data->redirect_code_size == GUM_INTERCEPTOR_THUMB_TINY_REDIRECT_SIZE;

    ctx->trampoline_deflector = gum_code_allocator_alloc_deflector (
        self->allocator, &caller, return_address, deflector_target, dedicated);
    if (ctx->trampoline_deflector == NULL)
    {
      gum_code_slice_unref (ctx->trampoline_slice);
      ctx->trampoline_slice = NULL;
      return FALSE;
    }
  }

  if (ctx->type != GUM_INTERCEPTOR_TYPE_FAST)
  {
    if (data->redirect_code_size != GUM_INTERCEPTOR_THUMB_LINK_REDIRECT_SIZE)
    {
      gum_emit_thumb_push_cpu_context_high_part (tw);
    }

    gum_thumb_writer_put_ldr_reg_address (tw, ARM_REG_R6, GUM_ADDRESS (ctx));
    gum_thumb_writer_put_ldr_reg_address (tw, ARM_REG_PC,
        GUM_ADDRESS (self->enter_thunk_thumb));

    ctx->on_leave_trampoline =
        (guint8 *) ctx->trampoline_slice->pc + gum_thumb_writer_offset (tw) + 1;

    gum_emit_thumb_push_cpu_context_high_part (tw);
    gum_thumb_writer_put_ldr_reg_address (tw, ARM_REG_R6, GUM_ADDRESS (ctx));
    gum_thumb_writer_put_ldr_reg_address (tw, ARM_REG_PC,
        GUM_ADDRESS (self->leave_thunk_thumb));

    gum_thumb_writer_flush (tw);
    g_assert (gum_thumb_writer_offset (tw) <= ctx->trampoline_slice->size);
  }

  ctx->on_invoke_trampoline =
      (guint8 *) ctx->trampoline_slice->pc + gum_thumb_writer_offset (tw) + 1;

  gum_thumb_relocator_reset (tr, function_address, tw);

  signature = g_string_sized_new (16);

  insn = NULL;
  do
  {
    reloc_bytes = gum_thumb_relocator_read_one (tr, &insn);

    if (reloc_bytes != 0)
    {
      if (signature->len != 0)
        g_string_append_c (signature, ';');
      g_string_append (signature, insn->mnemonic);
    }
    else
    {
      reloc_bytes = data->redirect_code_size;
    }
  }
  while (reloc_bytes < data->redirect_code_size);

  /*
   * When we are hooking a function already hooked by another copy of
   * Gum, we need to be very careful when relocating BL instructions.
   * This is because the deflector trampoline looks at LR to determine
   * which hook is invoking it. So when the last of the overwritten
   * instructions is a BL, we might as well just transform it so it
   * looks just as if it had executed at its original memory location.
   */
  trailing_bl = (insn != NULL && insn->id == ARM_INS_BL &&
      insn->detail->arm.operands[0].type == ARM_OP_IMM) ? insn : NULL;

  is_branch_back_needed = !gum_thumb_relocator_eoi (tr);

  /*
   * Try to deal with minimal thunks that determine their caller and pass
   * it along to some inner function. This is important to support hooking
   * dlopen() on Android, where the dynamic linker uses the caller address
   * to decide on namespace and whether to allow the particular library to
   * be used by a particular caller.
   *
   * Because we potentially replace LR in order to trap the return, we end
   * up breaking dlopen() in such cases. We work around this by detecting
   * LR being read, and replace that instruction with a load of the actual
   * caller.
   *
   * This is however a bit risky done blindly, so we try to limit the
   * scope to the bare minimum. A potentially better longer term solution
   * is to analyze the function and patch each point of return, so we don't
   * have to replace LR on entry. That is however a bit complex, so we
   * opt for this simpler solution for now.
   */
  is_eligible_for_lr_rewriting = strcmp (signature->str, "mov;b") == 0 ||
      strcmp (signature->str, "mov;bx") == 0 ||
      g_str_has_prefix (signature->str, "push;mov;bl");

  g_string_free (signature, TRUE);

  if (is_eligible_for_lr_rewriting)
  {
    const cs_insn * insn;

    while ((insn = gum_thumb_relocator_peek_next_write_insn (tr)) != NULL)
    {
      if (insn->id == ARM_INS_MOV &&
          insn->detail->arm.operands[1].reg == ARM_REG_LR)
      {
        arm_reg dst_reg = insn->detail->arm.operands[0].reg;
        const arm_reg clobbered_regs[] = {
          ARM_REG_R0, ARM_REG_R1, ARM_REG_R2, ARM_REG_R3,
          ARM_REG_R4, ARM_REG_R5, ARM_REG_R6, ARM_REG_R7,
          ARM_REG_R9, ARM_REG_R12, ARM_REG_LR,
        };
        GArray * saved_regs;
        guint i;
        arm_reg nzcvq_reg;

        saved_regs = g_array_sized_new (FALSE, FALSE, sizeof (arm_reg),
            G_N_ELEMENTS (clobbered_regs));
        for (i = 0; i != G_N_ELEMENTS (clobbered_regs); i++)
        {
          arm_reg reg = clobbered_regs[i];
          if (reg != dst_reg)
            g_array_append_val (saved_regs, reg);
        }

        nzcvq_reg = ARM_REG_R4;
        if (nzcvq_reg == dst_reg)
          nzcvq_reg = ARM_REG_R5;

        gum_thumb_writer_put_push_regs_array (tw, saved_regs->len,
            (const arm_reg *) saved_regs->data);
        gum_thumb_writer_put_mrs_reg_reg (tw, nzcvq_reg,
            ARM_SYSREG_APSR_NZCVQ);

        gum_thumb_writer_put_call_address_with_arguments (tw,
            GUM_ADDRESS (_gum_interceptor_translate_top_return_address), 1,
            GUM_ARG_REGISTER, ARM_REG_LR);
        gum_thumb_writer_put_mov_reg_reg (tw, dst_reg, ARM_REG_R0);

        gum_thumb_writer_put_msr_reg_reg (tw, ARM_SYSREG_APSR_NZCVQ,
            nzcvq_reg);
        gum_thumb_writer_put_pop_regs_array (tw, saved_regs->len,
            (const arm_reg *) saved_regs->data);

        g_array_free (saved_regs, TRUE);

        gum_thumb_relocator_skip_one (tr);
      }
      else
      {
        gum_thumb_relocator_write_one (tr);
      }
    }
  }
  else if (trailing_bl != NULL)
  {
    const cs_arm_op * target = &trailing_bl->detail->arm.operands[0];

    while (gum_thumb_relocator_peek_next_write_insn (tr) != trailing_bl)
      gum_thumb_relocator_write_one (tr);
    gum_thumb_relocator_skip_one (tr);

    gum_thumb_writer_put_ldr_reg_address (tw, ARM_REG_LR,
        trailing_bl->address + trailing_bl->size + 1);
    gum_thumb_writer_put_ldr_reg_address (tw, ARM_REG_PC,
        target->imm | 1);

    is_branch_back_needed = FALSE;
  }
  else
  {
    gum_thumb_relocator_write_all (tr);
  }

  if (is_branch_back_needed)
  {
    gum_thumb_writer_put_push_regs (tw, 2, ARM_REG_R0, ARM_REG_R1);
    gum_thumb_writer_put_ldr_reg_address (tw, ARM_REG_R0,
        GUM_ADDRESS (function_address + reloc_bytes + 1));
    gum_thumb_writer_put_str_reg_reg_offset (tw, ARM_REG_R0,
        ARM_REG_SP, 4);
    gum_thumb_writer_put_pop_regs (tw, 2, ARM_REG_R0, ARM_REG_PC);
  }

  gum_thumb_writer_flush (tw);
  g_assert (gum_thumb_writer_offset (tw) <= ctx->trampoline_slice->size);

  ctx->overwritten_prologue_len = reloc_bytes;

  return TRUE;
}

static gboolean
gum_interceptor_backend_write_arm_custom_redirect (
    GumInterceptorBackend * self,
    GumFunctionContext * ctx,
    gpointer target)
{
  GumArmFunctionContextData * data = GUM_FCDATA (ctx);
  GumRedirectWriteResult result;
  GumArmWriter rw;
  GumRedirectWriteDetails details;

  gum_arm_writer_init (&rw, ctx->redirect_code);
  rw.pc = GUM_ADDRESS (_gum_interceptor_backend_get_function_address (ctx));

  details.writer = &rw;
  details.target = target;
  details.scratch_register = ctx->scratch_register;
  details.capacity = data->available_space;

  result = ctx->write_redirect (&details, ctx->write_redirect_data);

  gum_arm_writer_flush (&rw);
  data->redirect_code_size = gum_arm_writer_offset (&rw);
  gum_arm_writer_clear (&rw);

  g_assert (data->redirect_code_size <= data->available_space);

  return result == GUM_REDIRECT_WRITTEN;
}

static gboolean
gum_interceptor_backend_write_thumb_custom_redirect (
    GumInterceptorBackend * self,
    GumFunctionContext * ctx,
    gpointer target)
{
  GumArmFunctionContextData * data = GUM_FCDATA (ctx);
  GumRedirectWriteResult result;
  GumThumbWriter rw;
  GumRedirectWriteDetails details;

  gum_thumb_writer_init (&rw, ctx->redirect_code);
  rw.pc = GUM_ADDRESS (_gum_interceptor_backend_get_function_address (ctx));

  details.writer = &rw;
  details.target = target;
  details.scratch_register = ctx->scratch_register;
  details.capacity = data->available_space;

  result = ctx->write_redirect (&details, ctx->write_redirect_data);

  gum_thumb_writer_flush (&rw);
  data->redirect_code_size = gum_thumb_writer_offset (&rw);
  gum_thumb_writer_clear (&rw);

  g_assert (data->redirect_code_size <= data->available_space);

  return result == GUM_REDIRECT_WRITTEN;
}

void
_gum_interceptor_backend_destroy_trampoline (GumInterceptorBackend * self,
                                             GumFunctionContext * ctx)
{
  gum_code_slice_unref (ctx->trampoline_slice);
  gum_code_deflector_unref (ctx->trampoline_deflector);
  ctx->trampoline_slice = NULL;
  ctx->trampoline_deflector = NULL;
}

void
_gum_interceptor_backend_activate_trampoline (GumInterceptorBackend * self,
                                              GumFunctionContext * ctx,
                                              gpointer prologue)
{
  GumAddress function_address;
  GumArmFunctionContextData * data = GUM_FCDATA (ctx);

  function_address = GUM_ADDRESS (
      _gum_interceptor_backend_get_function_address (ctx));

  if (FUNCTION_CONTEXT_ADDRESS_IS_THUMB (ctx))
  {
    GumThumbWriter * tw = &self->thumb_writer;

    gum_thumb_writer_reset (tw, prologue);
    tw->pc = function_address;

    if (ctx->write_redirect != NULL)
    {
      gum_thumb_writer_put_bytes (tw, ctx->redirect_code,
          data->redirect_code_size);
    }
    else if (ctx->trampoline_deflector != NULL)
    {
      if (data->redirect_code_size == GUM_INTERCEPTOR_THUMB_LINK_REDIRECT_SIZE)
      {
        gum_emit_thumb_push_cpu_context_high_part (tw);
        gum_thumb_writer_put_bl_imm (tw,
            GUM_ADDRESS (ctx->trampoline_deflector->trampoline));
      }
      else
      {
        g_assert (data->redirect_code_size ==
            GUM_INTERCEPTOR_THUMB_TINY_REDIRECT_SIZE);
        gum_thumb_writer_put_b_imm (tw,
            GUM_ADDRESS (ctx->trampoline_deflector->trampoline));
      }
    }
    else if (ctx->type == GUM_INTERCEPTOR_TYPE_FAST)
    {
      gum_thumb_writer_put_ldr_reg_address (tw, ARM_REG_PC,
          GUM_ADDRESS (ctx->replacement_function));
    }
    else
    {
      gum_thumb_writer_put_ldr_reg_address (tw, ARM_REG_PC,
          GUM_ADDRESS (ctx->on_enter_trampoline));
    }

    gum_thumb_writer_flush (tw);
    g_assert (gum_thumb_writer_offset (tw) <= data->redirect_code_size);
  }
  else
  {
    GumArmWriter * aw = &self->arm_writer;

    gum_arm_writer_reset (aw, prologue);
    aw->pc = function_address;

    if (ctx->write_redirect != NULL)
    {
      gum_arm_writer_put_bytes (aw, ctx->redirect_code,
          data->redirect_code_size);
    }
    else if (ctx->trampoline_deflector != NULL)
    {
      g_assert (data->redirect_code_size ==
          GUM_INTERCEPTOR_ARM_TINY_REDIRECT_SIZE);
      gum_arm_writer_put_b_imm (aw,
          GUM_ADDRESS (ctx->trampoline_deflector->trampoline));
    }
    else if (ctx->type == GUM_INTERCEPTOR_TYPE_FAST)
    {
      gum_arm_writer_put_ldr_reg_address (aw, ARM_REG_PC,
          GUM_ADDRESS (ctx->replacement_function));
    }
    else
    {
      gum_arm_writer_put_ldr_reg_address (aw, ARM_REG_PC,
          GUM_ADDRESS (ctx->on_enter_trampoline));
    }

    gum_arm_writer_flush (aw);
    g_assert (gum_arm_writer_offset (aw) == data->redirect_code_size);
  }
}

void
_gum_interceptor_backend_deactivate_trampoline (GumInterceptorBackend * self,
                                                GumFunctionContext * ctx,
                                                gpointer prologue)
{
  gum_memcpy (prologue, ctx->overwritten_prologue,
      ctx->overwritten_prologue_len);
}

gpointer
_gum_interceptor_backend_get_function_address (GumFunctionContext * ctx)
{
  return GSIZE_TO_POINTER (
      GPOINTER_TO_SIZE (ctx->function_address) & ~((gsize) 1));
}

gpointer
_gum_interceptor_backend_resolve_redirect (GumInterceptorBackend * self,
                                           gpointer address)
{
  gpointer target;

  if ((GPOINTER_TO_SIZE (address) & 1) == 1)
  {
    target = gum_thumb_reader_try_get_relative_jump_target (address);
  }
  else
  {
    target = gum_arm_reader_try_get_relative_jump_target (address);
    if (target == NULL)
      target = gum_arm_reader_try_get_indirect_jump_target (address);
  }

  return target;
}

gsize
_gum_interceptor_backend_detect_hook_size (gconstpointer code,
                                           csh capstone,
                                           cs_insn * insn)
{
  /* TODO: implement hook size detection */
  return 0;
}

static void
gum_interceptor_backend_create_thunks (GumInterceptorBackend * self)
{
  GumArmWriter * aw = &self->arm_writer;
  GumThumbWriter * tw = &self->thumb_writer;

  self->arm_thunks = gum_code_allocator_alloc_slice (self->allocator);
  gum_arm_writer_reset (aw, self->arm_thunks->data);
  aw->pc = GUM_ADDRESS (self->arm_thunks->pc);

  self->enter_thunk_arm = self->arm_thunks->pc + gum_arm_writer_offset (aw);
  gum_emit_arm_enter_thunk (aw);

  self->leave_thunk_arm = self->arm_thunks->pc + gum_arm_writer_offset (aw);
  gum_emit_arm_leave_thunk (aw);

  gum_arm_writer_flush (aw);
  g_assert (gum_arm_writer_offset (aw) <= self->arm_thunks->size);

  self->thumb_thunks = gum_code_allocator_alloc_slice (self->allocator);
  gum_thumb_writer_reset (tw, self->thumb_thunks->data);
  tw->pc = GUM_ADDRESS (self->thumb_thunks->pc);

  self->enter_thunk_thumb =
      self->thumb_thunks->pc + gum_thumb_writer_offset (tw) + 1;
  gum_emit_thumb_enter_thunk (tw);

  self->leave_thunk_thumb =
      self->thumb_thunks->pc + gum_thumb_writer_offset (tw) + 1;
  gum_emit_thumb_leave_thunk (tw);

  gum_thumb_writer_flush (tw);
  g_assert (gum_thumb_writer_offset (tw) <= self->thumb_thunks->size);
}

static void
gum_interceptor_backend_destroy_thunks (GumInterceptorBackend * self)
{
  gum_code_slice_unref (self->thumb_thunks);
  gum_code_slice_unref (self->arm_thunks);
}

static void
gum_emit_arm_enter_thunk (GumArmWriter * aw)
{
  gum_emit_arm_prolog (aw);

  gum_arm_writer_put_add_reg_reg_imm (aw, ARM_REG_R1, ARM_REG_SP,
      GUM_FRAME_OFFSET_CPU_CONTEXT);
  gum_arm_writer_put_sub_reg_reg_imm (aw, ARM_REG_R2, ARM_REG_R4, 4);
  gum_arm_writer_put_add_reg_reg_imm (aw, ARM_REG_R3, ARM_REG_SP,
      GUM_FRAME_OFFSET_NEXT_HOP);

  gum_arm_writer_put_call_address_with_arguments (aw,
      GUM_ADDRESS (_gum_function_context_begin_invocation), 4,
      GUM_ARG_REGISTER, ARM_REG_R6,
      GUM_ARG_REGISTER, ARM_REG_R1,
      GUM_ARG_REGISTER, ARM_REG_R2,
      GUM_ARG_REGISTER, ARM_REG_R3);

  gum_emit_arm_epilog (aw);
}

static void
gum_emit_thumb_enter_thunk (GumThumbWriter * tw)
{
  gum_emit_thumb_prolog (tw);

  gum_thumb_writer_put_add_reg_reg_imm (tw, ARM_REG_R1, ARM_REG_SP,
      GUM_FRAME_OFFSET_CPU_CONTEXT);
  gum_thumb_writer_put_sub_reg_reg_imm (tw, ARM_REG_R2, ARM_REG_R4, 4);
  gum_thumb_writer_put_add_reg_reg_imm (tw, ARM_REG_R3, ARM_REG_SP,
      GUM_FRAME_OFFSET_NEXT_HOP);

  gum_thumb_writer_put_call_address_with_arguments (tw,
      GUM_ADDRESS (_gum_function_context_begin_invocation), 4,
      GUM_ARG_REGISTER, ARM_REG_R6,
      GUM_ARG_REGISTER, ARM_REG_R1,
      GUM_ARG_REGISTER, ARM_REG_R2,
      GUM_ARG_REGISTER, ARM_REG_R3);

  gum_emit_thumb_epilog (tw);
}

static void
gum_emit_arm_leave_thunk (GumArmWriter * aw)
{
  gum_emit_arm_prolog (aw);

  gum_arm_writer_put_add_reg_reg_imm (aw, ARM_REG_R1, ARM_REG_SP,
      GUM_FRAME_OFFSET_CPU_CONTEXT);
  gum_arm_writer_put_add_reg_reg_imm (aw, ARM_REG_R2, ARM_REG_SP,
      GUM_FRAME_OFFSET_NEXT_HOP);

  gum_arm_writer_put_call_address_with_arguments (aw,
      GUM_ADDRESS (_gum_function_context_end_invocation), 3,
      GUM_ARG_REGISTER, ARM_REG_R6,
      GUM_ARG_REGISTER, ARM_REG_R1,
      GUM_ARG_REGISTER, ARM_REG_R2);

  gum_emit_arm_epilog (aw);
}

static void
gum_emit_thumb_leave_thunk (GumThumbWriter * tw)
{
  gum_emit_thumb_prolog (tw);

  gum_thumb_writer_put_add_reg_reg_imm (tw, ARM_REG_R1, ARM_REG_SP,
      GUM_FRAME_OFFSET_CPU_CONTEXT);
  gum_thumb_writer_put_add_reg_reg_imm (tw, ARM_REG_R2, ARM_REG_SP,
      GUM_FRAME_OFFSET_NEXT_HOP);

  gum_thumb_writer_put_call_address_with_arguments (tw,
      GUM_ADDRESS (_gum_function_context_end_invocation), 3,
      GUM_ARG_REGISTER, ARM_REG_R6,
      GUM_ARG_REGISTER, ARM_REG_R1,
      GUM_ARG_REGISTER, ARM_REG_R2);

  gum_emit_thumb_epilog (tw);
}

static void
gum_emit_arm_push_cpu_context_high_part (GumArmWriter * aw)
{
  gum_arm_writer_put_push_regs (aw, 9,
      ARM_REG_R0, ARM_REG_R1, ARM_REG_R2,
      ARM_REG_R3, ARM_REG_R4, ARM_REG_R5,
      ARM_REG_R6, ARM_REG_R7, ARM_REG_LR);
}

static void
gum_emit_thumb_push_cpu_context_high_part (GumThumbWriter * tw)
{
  gum_thumb_writer_put_push_regs (tw, 9,
      ARM_REG_R0, ARM_REG_R1, ARM_REG_R2,
      ARM_REG_R3, ARM_REG_R4, ARM_REG_R5,
      ARM_REG_R6, ARM_REG_R7, ARM_REG_LR);
}

static void
gum_emit_arm_prolog (GumArmWriter * aw)
{
  GumCpuFeatures cpu_features;

  /*
   * Set up our stack frame:
   *
   * [cpu_context] <-- high part already pushed
   * [padding]
   * [next_hop]
   */

  gum_arm_writer_put_mov_reg_cpsr (aw, ARM_REG_R5);
  gum_arm_writer_put_add_reg_reg_imm (aw, ARM_REG_R4, ARM_REG_SP, 9 * 4);

  /* Store vector registers + padding */
  cpu_features = gum_query_cpu_features ();

  if ((cpu_features & GUM_CPU_VFP2) != 0)
  {
    if ((cpu_features & GUM_CPU_VFPD32) != 0)
    {
      gum_arm_writer_put_sub_reg_u16 (aw, ARM_REG_SP, 4);
      gum_arm_writer_put_vpush_range (aw, ARM_REG_Q8, ARM_REG_Q15);
    }
    else
    {
      gum_arm_writer_put_sub_reg_u16 (aw, ARM_REG_SP,
          (8 * sizeof (GumArmVectorReg)) + 4);
    }

    gum_arm_writer_put_vpush_range (aw, ARM_REG_Q0, ARM_REG_Q7);
  }
  else
  {
    gum_arm_writer_put_sub_reg_u16 (aw, ARM_REG_SP,
        (16 * sizeof (GumArmVectorReg)) + 4);
  }

  /* Store SP, CPSR, followed by R8-R12 */
  gum_arm_writer_put_push_regs (aw, 7,
      ARM_REG_R4, ARM_REG_R5,
      ARM_REG_R8, ARM_REG_R9, ARM_REG_R10, ARM_REG_R11, ARM_REG_R12);

  /* Reserve space for next_hop, padding, and the PC placeholder */
  gum_arm_writer_put_sub_reg_u16 (aw, ARM_REG_SP, 3 * 4);
}

static void
gum_emit_thumb_prolog (GumThumbWriter * tw)
{
  GumCpuFeatures cpu_features;

  gum_thumb_writer_put_mov_reg_cpsr (tw, ARM_REG_R5);
  gum_thumb_writer_put_add_reg_reg_imm (tw, ARM_REG_R4, ARM_REG_SP, 9 * 4);

  cpu_features = gum_query_cpu_features ();

  if ((cpu_features & GUM_CPU_VFP2) != 0)
  {
    if ((cpu_features & GUM_CPU_VFPD32) != 0)
    {
      gum_thumb_writer_put_sub_reg_imm (tw, ARM_REG_SP, 4);
      gum_thumb_writer_put_vpush_range (tw, ARM_REG_Q8, ARM_REG_Q15);
    }
    else
    {
      gum_thumb_writer_put_sub_reg_imm (tw, ARM_REG_SP,
          (8 * sizeof (GumArmVectorReg)) + 4);
    }

    gum_thumb_writer_put_vpush_range (tw, ARM_REG_Q0, ARM_REG_Q7);
  }
  else
  {
    gum_thumb_writer_put_sub_reg_imm (tw, ARM_REG_SP,
        (16 * sizeof (GumArmVectorReg)) + 4);
  }

  gum_thumb_writer_put_push_regs (tw, 7,
      ARM_REG_R4, ARM_REG_R5,
      ARM_REG_R8, ARM_REG_R9, ARM_REG_R10, ARM_REG_R11, ARM_REG_R12);

  gum_thumb_writer_put_sub_reg_imm (tw, ARM_REG_SP, 3 * 4);
}

static void
gum_emit_arm_epilog (GumArmWriter * aw)
{
  GumCpuFeatures cpu_features;

  /* Restore LR */
  gum_arm_writer_put_sub_reg_reg_imm (aw, ARM_REG_R0, ARM_REG_R4, 4);
  gum_arm_writer_put_ldr_reg_reg (aw, ARM_REG_LR, ARM_REG_R0);

  /* Replace LR with next_hop so we can pop it straight into PC */
  gum_arm_writer_put_ldr_reg_reg_offset (aw, ARM_REG_R1, ARM_REG_SP,
      GUM_FRAME_OFFSET_NEXT_HOP);
  gum_arm_writer_put_str_reg_reg (aw, ARM_REG_R1, ARM_REG_R0);

  gum_arm_writer_put_ldr_reg_reg_offset (aw, ARM_REG_R5, ARM_REG_SP,
      GUM_FRAME_OFFSET_CPU_CONTEXT + G_STRUCT_OFFSET (GumCpuContext, cpsr));

  /* Skip [next_hop, padding] and [PC, SP, and CPSR] */
  gum_arm_writer_put_add_reg_u16 (aw, ARM_REG_SP,
      GUM_FRAME_OFFSET_CPU_CONTEXT + (3 * 4));

  gum_arm_writer_put_pop_regs (aw, 5,
      ARM_REG_R8, ARM_REG_R9, ARM_REG_R10, ARM_REG_R11, ARM_REG_R12);

  cpu_features = gum_query_cpu_features ();

  if ((cpu_features & GUM_CPU_VFP2) != 0)
  {
    gum_arm_writer_put_vpop_range (aw, ARM_REG_Q0, ARM_REG_Q7);

    if ((cpu_features & GUM_CPU_VFPD32) != 0)
    {
      gum_arm_writer_put_vpop_range (aw, ARM_REG_Q8, ARM_REG_Q15);
      gum_arm_writer_put_add_reg_u16 (aw, ARM_REG_SP, 4);
    }
    else
    {
      gum_arm_writer_put_add_reg_u16 (aw, ARM_REG_SP,
          (8 * sizeof (GumArmVectorReg)) + 4);
    }
  }
  else
  {
    gum_arm_writer_put_add_reg_u16 (aw, ARM_REG_SP,
        (16 * sizeof (GumArmVectorReg)) + 4);
  }

  gum_arm_writer_put_mov_cpsr_reg (aw, ARM_REG_R5);

  gum_arm_writer_put_pop_regs (aw, 9,
      ARM_REG_R0, ARM_REG_R1, ARM_REG_R2,
      ARM_REG_R3, ARM_REG_R4, ARM_REG_R5,
      ARM_REG_R6, ARM_REG_R7, ARM_REG_PC);
}

static void
gum_emit_thumb_epilog (GumThumbWriter * tw)
{
  GumCpuFeatures cpu_features;

  gum_thumb_writer_put_sub_reg_reg_imm (tw, ARM_REG_R0, ARM_REG_R4, 4);
  gum_thumb_writer_put_ldr_reg_reg (tw, ARM_REG_R1, ARM_REG_R0);
  gum_thumb_writer_put_mov_reg_reg (tw, ARM_REG_LR, ARM_REG_R1);

  gum_thumb_writer_put_ldr_reg_reg_offset (tw, ARM_REG_R1, ARM_REG_SP,
      GUM_FRAME_OFFSET_NEXT_HOP);
  gum_thumb_writer_put_str_reg_reg (tw, ARM_REG_R1, ARM_REG_R0);

  gum_thumb_writer_put_ldr_reg_reg_offset (tw, ARM_REG_R5, ARM_REG_SP,
      GUM_FRAME_OFFSET_CPU_CONTEXT + G_STRUCT_OFFSET (GumCpuContext, cpsr));

  gum_thumb_writer_put_add_reg_imm (tw, ARM_REG_SP,
      GUM_FRAME_OFFSET_CPU_CONTEXT + (3 * 4));

  gum_thumb_writer_put_pop_regs (tw, 5,
      ARM_REG_R8, ARM_REG_R9, ARM_REG_R10, ARM_REG_R11, ARM_REG_R12);

  cpu_features = gum_query_cpu_features ();

  if ((cpu_features & GUM_CPU_VFP2) != 0)
  {
    gum_thumb_writer_put_vpop_range (tw, ARM_REG_Q0, ARM_REG_Q7);

    if ((cpu_features & GUM_CPU_VFPD32) != 0)
    {
      gum_thumb_writer_put_vpop_range (tw, ARM_REG_Q8, ARM_REG_Q15);
      gum_thumb_writer_put_add_reg_imm (tw, ARM_REG_SP, 4);
    }
    else
    {
      gum_thumb_writer_put_add_reg_imm (tw, ARM_REG_SP,
          (8 * sizeof (GumArmVectorReg)) + 4);
    }
  }
  else
  {
    gum_thumb_writer_put_add_reg_imm (tw, ARM_REG_SP,
        (16 * sizeof (GumArmVectorReg)) + 4);
  }

  gum_thumb_writer_put_mov_cpsr_reg (tw, ARM_REG_R5);

  gum_thumb_writer_put_pop_regs (tw, 9,
      ARM_REG_R0, ARM_REG_R1, ARM_REG_R2,
      ARM_REG_R3, ARM_REG_R4, ARM_REG_R5,
      ARM_REG_R6, ARM_REG_R7, ARM_REG_PC);
}
