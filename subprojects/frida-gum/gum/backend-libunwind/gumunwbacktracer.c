/*
 * Copyright (C) 2015-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2021 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gum/gumunwbacktracer.h"

#include "guminterceptor.h"
#ifdef HAVE_LINUX
# include "gum/gumlinux.h"
# define gum_os_unparse_ucontext gum_linux_unparse_ucontext
#endif
#ifdef HAVE_FREEBSD
# include "gum/gumfreebsd.h"
# define gum_os_unparse_ucontext gum_freebsd_unparse_ucontext
#endif
#ifdef HAVE_QNX
# include "gum/gumqnx.h"
# define gum_os_unparse_ucontext gum_qnx_unparse_ucontext
#endif

#define UNW_LOCAL_ONLY
#include <libunwind.h>

struct _GumUnwBacktracer
{
  GObject parent;
};

static void gum_unw_backtracer_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_unw_backtracer_generate (GumBacktracer * backtracer,
    const GumCpuContext * cpu_context, GumReturnAddressArray * return_addresses,
    guint limit);

static void gum_cpu_context_to_unw (const GumCpuContext * ctx,
    unw_context_t * uc);

G_DEFINE_TYPE_EXTENDED (GumUnwBacktracer,
                        gum_unw_backtracer,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_BACKTRACER,
                                               gum_unw_backtracer_iface_init))

static void
gum_unw_backtracer_class_init (GumUnwBacktracerClass * klass)
{
}

static void
gum_unw_backtracer_iface_init (gpointer g_iface,
                               gpointer iface_data)
{
  GumBacktracerInterface * iface = g_iface;

  iface->generate = gum_unw_backtracer_generate;
}

static void
gum_unw_backtracer_init (GumUnwBacktracer * self)
{
}

GumBacktracer *
gum_unw_backtracer_new (void)
{
  return g_object_new (GUM_TYPE_UNW_BACKTRACER, NULL);
}

static void
gum_unw_backtracer_generate (GumBacktracer * backtracer,
                             const GumCpuContext * cpu_context,
                             GumReturnAddressArray * return_addresses,
                             guint limit)
{
  unw_context_t context;
  unw_cursor_t cursor;
  guint start_index, depth, i;
  GumInvocationStack * invocation_stack;

  if (cpu_context != NULL)
  {
#if defined (HAVE_I386)
    return_addresses->items[0] = *((GumReturnAddress *) GSIZE_TO_POINTER (
        GUM_CPU_CONTEXT_XSP (cpu_context)));
#elif defined (HAVE_ARM) || defined (HAVE_ARM64)
    return_addresses->items[0] = GSIZE_TO_POINTER (cpu_context->lr);
#elif defined (HAVE_MIPS)
    return_addresses->items[0] = GSIZE_TO_POINTER (cpu_context->ra);
#else
# error Unsupported architecture
#endif
    start_index = 1;

    gum_cpu_context_to_unw (cpu_context, &context);
  }
  else
  {
    start_index = 0;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Winline-asm"
#endif
    unw_getcontext (&context);
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#pragma GCC diagnostic pop
  }

  depth = MIN (limit, G_N_ELEMENTS (return_addresses->items));

  unw_init_local (&cursor, &context);
  for (i = start_index;
      i < depth && unw_step (&cursor) > 0;
      i++)
  {
    unw_word_t pc;

    unw_get_reg (&cursor, UNW_REG_IP, &pc);
    return_addresses->items[i] = GSIZE_TO_POINTER (pc);
  }
  return_addresses->len = i;

  invocation_stack = gum_interceptor_get_current_stack ();
  for (i = 0; i != return_addresses->len; i++)
  {
    return_addresses->items[i] = gum_invocation_stack_translate (
        invocation_stack, return_addresses->items[i]);
  }
}

static void
gum_cpu_context_to_unw (const GumCpuContext * ctx,
                        unw_context_t * uc)
{
#if defined (UNW_TARGET_X86) || defined (UNW_TARGET_X86_64) || \
    defined (UNW_TARGET_AARCH64)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunused-value"
  unw_getcontext (uc);
# pragma GCC diagnostic pop

  gum_os_unparse_ucontext (ctx, (ucontext_t *) uc);

# if defined (UNW_TARGET_AARCH64)
#  ifdef HAVE_FREEBSD
  uc->uc_mcontext.mc_gpregs.gp_elr -= 4;
#  else
  uc->uc_mcontext.pc -= 4;
#  endif
# endif
#elif defined (UNW_TARGET_ARM)
  uc->regs[UNW_ARM_R15] = ctx->lr;
  uc->regs[UNW_ARM_R13] = ctx->sp;

  uc->regs[UNW_ARM_R8] = ctx->r8;
  uc->regs[UNW_ARM_R9] = ctx->r9;
  uc->regs[UNW_ARM_R10] = ctx->r10;
  uc->regs[UNW_ARM_R11] = ctx->r11;
  uc->regs[UNW_ARM_R12] = ctx->r12;

  {
    guint i;

    for (i = 0; i != G_N_ELEMENTS (ctx->r); i++)
      uc->regs[i] = ctx->r[i];
  }

  uc->regs[UNW_ARM_R14] = ctx->lr;
#elif defined (UNW_TARGET_MIPS)
  greg_t * gr = uc->uc_mcontext.gregs;

  gr[1] = ctx->at;

  gr[2] = ctx->v0;
  gr[3] = ctx->v1;

  gr[4] = ctx->a0;
  gr[5] = ctx->a1;
  gr[6] = ctx->a2;
  gr[7] = ctx->a3;

  gr[8] = ctx->t0;
  gr[9] = ctx->t1;
  gr[10] = ctx->t2;
  gr[11] = ctx->t3;
  gr[12] = ctx->t4;
  gr[13] = ctx->t5;
  gr[14] = ctx->t6;
  gr[15] = ctx->t7;

  gr[16] = ctx->s0;
  gr[17] = ctx->s1;
  gr[18] = ctx->s2;
  gr[19] = ctx->s3;
  gr[20] = ctx->s4;
  gr[21] = ctx->s5;
  gr[22] = ctx->s6;
  gr[23] = ctx->s7;

  gr[24] = ctx->t8;
  gr[25] = ctx->t9;

  gr[26] = ctx->k0;
  gr[27] = ctx->k1;

  gr[28] = ctx->gp;
  gr[29] = ctx->sp;
  gr[30] = ctx->fp;
  gr[31] = ctx->ra;

  uc->uc_mcontext.mdhi = ctx->hi;
  uc->uc_mcontext.mdlo = ctx->lo;

  uc->uc_mcontext.pc = ctx->pc;
#else
# error FIXME
#endif
}
