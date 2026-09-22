/*
 * Copyright (C) 2008-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "lowlevelhelpers.h"

#include "gummemory.h"
#ifdef HAVE_I386
# include "gumx86writer.h"
#endif
#ifdef HAVE_ARM
# include "gumarmwriter.h"
# include "gumthumbwriter.h"
#endif
#ifdef HAVE_ARM64
# include "gumarm64writer.h"
#endif

#ifdef HAVE_WINDOWS
#define VC_EXTRALEAN
#include <windows.h>
#else
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#endif
#include <string.h>

typedef struct _GumEmitTestClobberRegsContext GumEmitTestClobberRegsContext;
typedef struct _GumEmitTestClobberFlagsContext GumEmitTestClobberFlagsContext;

struct _GumEmitTestClobberRegsContext
{
  ClobberTestFunc target_func;
  GumAddress start_address;
};

struct _GumEmitTestClobberFlagsContext
{
  ClobberTestFunc target_func;
  GumAddress start_address;
};

ClobberTestFunc clobber_test_functions[3] = { NULL, };

typedef void (* InvokeWithCpuContextFunc) (const GumCpuContext * input,
    GumCpuContext * output);
typedef void (* InvokeWithCpuFlagsFunc) (gsize * flags_input,
    gsize * flags_output);

static void gum_emit_clobber_test_functions (gpointer mem, gpointer user_data);
static void gum_emit_test_clobber_regs_function (gpointer mem,
    GumEmitTestClobberRegsContext * ctx);
static void gum_emit_test_clobber_flags_function (gpointer mem,
    GumEmitTestClobberFlagsContext * ctx);
static gpointer allocate_clobber_test_invoker_func (ClobberTestFunc target_func,
    gsize * code_size);

void
lowlevel_helpers_init (void)
{
  gsize page_size, code_size;

  g_assert_null (clobber_test_functions[0]);

  page_size = gum_query_page_size ();
  code_size = page_size;

  clobber_test_functions[0] = GUM_POINTER_TO_FUNCPTR (ClobberTestFunc,
      gum_sign_code_pointer (
        gum_memory_allocate (NULL, code_size, page_size, GUM_PAGE_RW)));
  gum_memory_patch_code (clobber_test_functions[0], 64,
      gum_emit_clobber_test_functions, NULL);
}

void
lowlevel_helpers_deinit (void)
{
  g_assert_nonnull (clobber_test_functions[0]);

  gum_memory_free (GUM_FUNCPTR_TO_POINTER (clobber_test_functions[0]),
      gum_query_page_size ());
  clobber_test_functions[0] = NULL;
  clobber_test_functions[1] = NULL;
}

static void
gum_emit_clobber_test_functions (gpointer mem,
                                 gpointer user_data)
{
#if defined (HAVE_I386)
  GumX86Writer cw;

  gum_x86_writer_init (&cw, mem);
  cw.pc = gum_strip_code_address (GUM_ADDRESS (clobber_test_functions[0]));

  gum_x86_writer_put_nop (&cw);
  gum_x86_writer_put_nop (&cw);
  gum_x86_writer_put_nop (&cw);
  gum_x86_writer_put_nop (&cw);
  gum_x86_writer_put_nop (&cw);
  gum_x86_writer_put_ret (&cw);

  gum_x86_writer_clear (&cw);
#elif defined (HAVE_ARM)
  GumArmWriter aw;
  GumThumbWriter tw;

  gum_arm_writer_init (&aw, mem);
  aw.pc = gum_strip_code_address (GUM_ADDRESS (clobber_test_functions[0]));

  gum_arm_writer_put_nop (&aw);
  gum_arm_writer_put_nop (&aw);
  gum_arm_writer_put_nop (&aw);
  gum_arm_writer_put_ret (&aw);

  gum_arm_writer_flush (&aw);

  clobber_test_functions[1] = GUM_POINTER_TO_FUNCPTR (ClobberTestFunc,
      (guint8 *) clobber_test_functions[0] + gum_arm_writer_offset (&aw) + 1);

  gum_thumb_writer_init (&tw, (guint16 *) aw.code);
  tw.pc = aw.pc;

  gum_thumb_writer_put_nop (&tw);
  gum_thumb_writer_put_nop (&tw);
  gum_thumb_writer_put_nop (&tw);
  gum_thumb_writer_put_bx_reg (&tw, ARM_REG_LR);

  gum_thumb_writer_clear (&tw);
  gum_arm_writer_clear (&aw);
#elif defined (HAVE_ARM64)
  GumArm64Writer cw;

  gum_arm64_writer_init (&cw, mem);
  cw.pc = gum_strip_code_address (GUM_ADDRESS (clobber_test_functions[0]));

  gum_arm64_writer_put_nop (&cw);
  gum_arm64_writer_put_nop (&cw);
  gum_arm64_writer_put_nop (&cw);
  gum_arm64_writer_put_ret (&cw);

  gum_arm64_writer_clear (&cw);
#endif
}

void
fill_cpu_context_with_magic_values (GumCpuContext * ctx)
{
#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
  ctx->eip = 0;

  ctx->edi = 0x1234a001;
  ctx->esi = 0x12340b02;
  ctx->ebp = 0x123400c3;
  ctx->esp = 0;
  ctx->ebx = 0x12340d04;
  ctx->edx = 0x1234e005;
  ctx->ecx = 0x12340f06;
  ctx->eax = 0x12340107;
#elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
  ctx->rip = 0;

  ctx->r15 = G_GUINT64_CONSTANT (0x8765abcd1234a001);
  ctx->r14 = G_GUINT64_CONSTANT (0x8765abcd12340b02);
  ctx->r13 = G_GUINT64_CONSTANT (0x8765abcd123400c3);
  ctx->r12 = G_GUINT64_CONSTANT (0x8765abcd12340d04);
  ctx->r11 = G_GUINT64_CONSTANT (0x8765abcd1234e005);
  ctx->r10 = G_GUINT64_CONSTANT (0x8765abcd12340f06);
  ctx->r9  = G_GUINT64_CONSTANT (0x8765abcd12340107);
  ctx->r8  = G_GUINT64_CONSTANT (0x8765abcd12340108);

  ctx->rdi = G_GUINT64_CONSTANT (0x876543211234a001);
  ctx->rsi = G_GUINT64_CONSTANT (0x8765432112340b02);
  ctx->rbp = G_GUINT64_CONSTANT (0x87654321123400c3);
  ctx->rsp = 0;
  ctx->rbx = G_GUINT64_CONSTANT (0x8765432112340d04);
  ctx->rdx = G_GUINT64_CONSTANT (0x876543211234e005);
  ctx->rcx = G_GUINT64_CONSTANT (0x8765432112340f06);
  ctx->rax = G_GUINT64_CONSTANT (0x8765432112340107);
#elif defined (HAVE_ARM)
  guint i;

  ctx->pc = 0;
  ctx->sp = 0;
  ctx->cpsr = 0;

  ctx->r8 = 0x123400c3;
  ctx->r9 = 0x12340d04;
  ctx->r10 = 0x1234e005;
  ctx->r11 = 0x12340f06;
  ctx->r12 = 0x12340107;

  for (i = 0; i != G_N_ELEMENTS (ctx->v); i++)
  {
    ctx->v[i].d[0] = (gdouble) ((2 * i) + 1);
    ctx->v[i].d[1] = (gdouble) ((2 * i) + 2);
  }

  ctx->r[0] = 0x12340908;
  ctx->r[1] = 0x12340809;
  ctx->r[2] = 0x1234070a;
  ctx->r[3] = 0x1234060b;
  ctx->r[4] = 0x1234050c;
  ctx->r[5] = 0x1234040d;
  ctx->r[6] = 0x1234030e;
  ctx->r[7] = 0x1234020f;
  ctx->lr = 0;
#elif defined (HAVE_ARM64)
  guint i;

  ctx->pc = 0;
  ctx->sp = 0;
  ctx->nzcv = 0;

  for (i = 0; i != G_N_ELEMENTS (ctx->x); i++)
  {
    if (i == 18)
    {
      /* X18 is typically reserved and should not be modified */
      ctx->x[i] = 0;
      continue;
    }
    ctx->x[i] = G_GUINT64_CONSTANT (0x8765abcd1234a001) + i;
  }
  ctx->fp = G_GUINT64_CONSTANT (0x8765abcd12340b02);
  ctx->lr = 0;

  for (i = 0; i != G_N_ELEMENTS (ctx->v); i++)
  {
    guint j;

    for (j = 0; j != sizeof (GumArm64VectorReg); j++)
    {
      ctx->v[i].q[j] = 1 + (i * sizeof (GumArm64VectorReg)) + j;
    }
  }
#endif
}

void
assert_cpu_contexts_are_equal (const GumCpuContext * input,
                               const GumCpuContext * output)
{
#define GUM_CHECK(r) \
    g_assert_cmphex (output->r, ==, input->r)

#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
  GUM_CHECK (edi);
  GUM_CHECK (esi);
  GUM_CHECK (ebp);
  GUM_CHECK (ebx);
  GUM_CHECK (edx);
  GUM_CHECK (ecx);
  GUM_CHECK (eax);
#elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
  GUM_CHECK (rdi);
  GUM_CHECK (rsi);
  GUM_CHECK (rbp);
  GUM_CHECK (rbx);
  GUM_CHECK (rdx);
  GUM_CHECK (rcx);
  GUM_CHECK (rax);

  GUM_CHECK (r15);
  GUM_CHECK (r14);
  GUM_CHECK (r13);
  GUM_CHECK (r12);
  GUM_CHECK (r11);
  GUM_CHECK (r10);
  GUM_CHECK (r9);
  GUM_CHECK (r8);
#elif defined (HAVE_ARM)
  GumCpuFeatures cpu_features;

  GUM_CHECK (r8);
  GUM_CHECK (r9);
  GUM_CHECK (r10);
  GUM_CHECK (r11);
  GUM_CHECK (r12);

  cpu_features = gum_query_cpu_features ();

# define GUM_CHECK_VECTOR_REG(i) \
    GUM_CHECK (v[i].d[0]); \
    GUM_CHECK (v[i].d[1])

  if ((cpu_features & GUM_CPU_VFP2) != 0)
  {
    GUM_CHECK_VECTOR_REG (0);
    GUM_CHECK_VECTOR_REG (1);
    GUM_CHECK_VECTOR_REG (2);
    GUM_CHECK_VECTOR_REG (3);
    GUM_CHECK_VECTOR_REG (4);
    GUM_CHECK_VECTOR_REG (5);
    GUM_CHECK_VECTOR_REG (6);
    GUM_CHECK_VECTOR_REG (7);

    if ((cpu_features & GUM_CPU_VFPD32) != 0)
    {
      GUM_CHECK_VECTOR_REG (8);
      GUM_CHECK_VECTOR_REG (9);
      GUM_CHECK_VECTOR_REG (10);
      GUM_CHECK_VECTOR_REG (11);
      GUM_CHECK_VECTOR_REG (12);
      GUM_CHECK_VECTOR_REG (13);
      GUM_CHECK_VECTOR_REG (14);
      GUM_CHECK_VECTOR_REG (15);
    }
  }

# undef GUM_CHECK_VECTOR_REG

  GUM_CHECK (r[0]);
  GUM_CHECK (r[1]);
  GUM_CHECK (r[2]);
  GUM_CHECK (r[3]);
  GUM_CHECK (r[4]);
  GUM_CHECK (r[5]);
  GUM_CHECK (r[6]);
  GUM_CHECK (r[7]);
#elif defined (HAVE_ARM64)
  GUM_CHECK (x[0]);
  GUM_CHECK (x[1]);
  GUM_CHECK (x[2]);
  GUM_CHECK (x[3]);
  GUM_CHECK (x[4]);
  GUM_CHECK (x[5]);
  GUM_CHECK (x[6]);
  GUM_CHECK (x[7]);
  GUM_CHECK (x[8]);
  GUM_CHECK (x[9]);
  GUM_CHECK (x[10]);
  GUM_CHECK (x[11]);
  GUM_CHECK (x[12]);
  GUM_CHECK (x[13]);
  GUM_CHECK (x[14]);
  GUM_CHECK (x[15]);
  /* TODO: GUM_CHECK (x[16]); */
  /* TODO: GUM_CHECK (x[17]); */
  GUM_CHECK (x[19]);
  GUM_CHECK (x[20]);
  GUM_CHECK (x[21]);
  GUM_CHECK (x[22]);
  GUM_CHECK (x[23]);
  GUM_CHECK (x[24]);
  GUM_CHECK (x[25]);
  GUM_CHECK (x[26]);
  GUM_CHECK (x[27]);
  GUM_CHECK (x[28]);
  GUM_CHECK (fp);

# define GUM_CHECK_VECTOR_REG(i) \
    g_assert_cmpmem (output->v[i].q, sizeof (GumArm64VectorReg), \
        input->v[i].q, sizeof (GumArm64VectorReg))

  GUM_CHECK_VECTOR_REG (0);
  GUM_CHECK_VECTOR_REG (1);
  GUM_CHECK_VECTOR_REG (2);
  GUM_CHECK_VECTOR_REG (3);
  GUM_CHECK_VECTOR_REG (4);
  GUM_CHECK_VECTOR_REG (5);
  GUM_CHECK_VECTOR_REG (6);
  GUM_CHECK_VECTOR_REG (7);
  GUM_CHECK_VECTOR_REG (8);
  GUM_CHECK_VECTOR_REG (9);
  GUM_CHECK_VECTOR_REG (10);
  GUM_CHECK_VECTOR_REG (11);
  GUM_CHECK_VECTOR_REG (12);
  GUM_CHECK_VECTOR_REG (13);
  GUM_CHECK_VECTOR_REG (14);
  GUM_CHECK_VECTOR_REG (15);
  GUM_CHECK_VECTOR_REG (16);
  GUM_CHECK_VECTOR_REG (17);
  GUM_CHECK_VECTOR_REG (18);
  GUM_CHECK_VECTOR_REG (19);
  GUM_CHECK_VECTOR_REG (20);
  GUM_CHECK_VECTOR_REG (21);
  GUM_CHECK_VECTOR_REG (22);
  GUM_CHECK_VECTOR_REG (23);
  GUM_CHECK_VECTOR_REG (24);
  GUM_CHECK_VECTOR_REG (25);
  GUM_CHECK_VECTOR_REG (26);
  GUM_CHECK_VECTOR_REG (27);
  GUM_CHECK_VECTOR_REG (28);
  GUM_CHECK_VECTOR_REG (29);
  GUM_CHECK_VECTOR_REG (30);
  GUM_CHECK_VECTOR_REG (31);

# undef GUM_CHECK_VECTOR_REG
#endif

#undef GUM_CHECK
}

void
invoke_clobber_test_function_with_cpu_context (ClobberTestFunc target_func,
                                               const GumCpuContext * input,
                                               GumCpuContext * output)
{
  guint8 * code;
  gsize code_size;
  GumEmitTestClobberRegsContext ctx;
  InvokeWithCpuContextFunc func;

  code = allocate_clobber_test_invoker_func (target_func, &code_size);

  ctx.target_func = target_func;
  ctx.start_address = GUM_ADDRESS (code);

  gum_memory_patch_code (code, 1024,
      (GumMemoryPatchApplyFunc) gum_emit_test_clobber_regs_function,
      &ctx);

  func = GUM_POINTER_TO_FUNCPTR (InvokeWithCpuContextFunc,
      gum_sign_code_pointer (code));
  func (input, output);

  gum_memory_free (code, code_size);
}

static void
gum_emit_test_clobber_regs_function (gpointer mem,
                                     GumEmitTestClobberRegsContext * ctx)
{
#if defined (HAVE_I386)
  GumX86Writer cw;
  gint align_correction = 0;

# if GLIB_SIZEOF_VOID_P == 4
  align_correction = 8;
# endif

  gum_x86_writer_init (&cw, mem);
  cw.pc = ctx->start_address;

  gum_x86_writer_put_pushax (&cw);

  gum_x86_writer_put_push_reg (&cw, GUM_X86_XAX); /* Placeholder for xip */

# if GLIB_SIZEOF_VOID_P == 4
  /* Load first argument */
  gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_ECX,
      GUM_X86_ESP, GUM_CPU_CONTEXT_OFFSET_XAX + sizeof (gpointer) + 4);

  gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_EAX,
      GUM_X86_ECX, G_STRUCT_OFFSET (GumCpuContext, eax));
  /* Leave GUM_X86_ECX for last */
  gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_EDX,
      GUM_X86_ECX, G_STRUCT_OFFSET (GumCpuContext, edx));
  gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_EBX,
      GUM_X86_ECX, G_STRUCT_OFFSET (GumCpuContext, ebx));
  gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_EBP,
      GUM_X86_ECX, G_STRUCT_OFFSET (GumCpuContext, ebp));
  gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_ESI,
      GUM_X86_ECX, G_STRUCT_OFFSET (GumCpuContext, esi));
  gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_EDI,
      GUM_X86_ECX, G_STRUCT_OFFSET (GumCpuContext, edi));

  gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_ECX,
      GUM_X86_ECX, G_STRUCT_OFFSET (GumCpuContext, ecx));
# else
  if (cw.target_abi == GUM_ABI_UNIX)
    gum_x86_writer_put_mov_reg_reg (&cw, GUM_X86_RCX, GUM_X86_RDI);

  gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_RAX,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, rax));
  /* Leave GUM_X86_RCX for last */
  gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_RDX,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, rdx));
  gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_RBX,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, rbx));
  gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_RBP,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, rbp));
  gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_RSI,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, rsi));
  gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_RDI,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, rdi));

  gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_R8,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, r8));
  gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_R9,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, r9));
  gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_R10,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, r10));
  gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_R11,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, r11));
  gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_R12,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, r12));
  gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_R13,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, r13));
  gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_R14,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, r14));
  gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_R15,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, r15));

  gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_RCX,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, rcx));
# endif

  if (align_correction != 0)
  {
    gum_x86_writer_put_lea_reg_reg_offset (&cw, GUM_X86_XSP,
        GUM_X86_XSP, -align_correction);
  }

  gum_x86_writer_put_call_address (&cw, GUM_ADDRESS (ctx->target_func));

  if (align_correction != 0)
  {
    gum_x86_writer_put_lea_reg_reg_offset (&cw, GUM_X86_XSP,
        GUM_X86_XSP, align_correction);
  }

  gum_x86_writer_put_push_reg (&cw, GUM_X86_XCX);

# if GLIB_SIZEOF_VOID_P == 4
  /* Load second argument */
  gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_ECX,
      GUM_X86_ESP, 4 + GUM_CPU_CONTEXT_OFFSET_XAX + sizeof (gpointer) + 8);

  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw,
      GUM_X86_ECX, G_STRUCT_OFFSET (GumCpuContext, eax),
      GUM_X86_EAX);
  /* Leave GUM_X86_ECX for last */
  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw,
      GUM_X86_ECX, G_STRUCT_OFFSET (GumCpuContext, edx),
      GUM_X86_EDX);
  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw,
      GUM_X86_ECX, G_STRUCT_OFFSET (GumCpuContext, ebx),
      GUM_X86_EBX);
  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw,
      GUM_X86_ECX, G_STRUCT_OFFSET (GumCpuContext, ebp),
      GUM_X86_EBP);
  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw,
      GUM_X86_ECX, G_STRUCT_OFFSET (GumCpuContext, esi),
      GUM_X86_ESI);
  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw,
      GUM_X86_ECX, G_STRUCT_OFFSET (GumCpuContext, edi),
      GUM_X86_EDI);

  gum_x86_writer_put_pop_reg (&cw, GUM_X86_EDX);
  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw,
      GUM_X86_ECX, G_STRUCT_OFFSET (GumCpuContext, ecx),
      GUM_X86_EDX);
# else
  if (cw.target_abi == GUM_ABI_UNIX)
  {
    gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_RCX,
        GUM_X86_RSP, 8 + G_STRUCT_OFFSET (GumCpuContext, rsi));
  }
  else
  {
    gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_RCX,
        GUM_X86_RSP, 8 + G_STRUCT_OFFSET (GumCpuContext, rdx));
  }

  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, rax),
      GUM_X86_RAX);
  /* Leave GUM_X86_RCX for last */
  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, rdx),
      GUM_X86_RDX);
  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, rbx),
      GUM_X86_RBX);
  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, rbp),
      GUM_X86_RBP);
  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, rsi),
      GUM_X86_RSI);
  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, rdi),
      GUM_X86_RDI);

  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, r8),
      GUM_X86_R8);
  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, r9),
      GUM_X86_R9);
  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, r10),
      GUM_X86_R10);
  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, r11),
      GUM_X86_R11);
  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, r12),
      GUM_X86_R12);
  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, r13),
      GUM_X86_R13);
  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, r14),
      GUM_X86_R14);
  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, r15),
      GUM_X86_R15);

  gum_x86_writer_put_pop_reg (&cw, GUM_X86_RDX);
  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw,
      GUM_X86_RCX, G_STRUCT_OFFSET (GumCpuContext, rcx),
      GUM_X86_RDX);
# endif

  gum_x86_writer_put_pop_reg (&cw, GUM_X86_XAX);
  gum_x86_writer_put_popax (&cw);
  gum_x86_writer_put_ret (&cw);

  gum_x86_writer_clear (&cw);
#elif defined (HAVE_ARM)
  GumAddress target_func = GUM_ADDRESS (ctx->target_func);
  GumArmWriter cw;
  GumCpuFeatures cpu_features;
  guint sp_distance_to_saved_r1 = 4;
  gint num_vfp_regs, i;

  gum_arm_writer_init (&cw, mem);
  cw.pc = ctx->start_address;

  gum_arm_writer_put_push_regs (&cw, 14,
      ARM_REG_R0, ARM_REG_R1, ARM_REG_R2, ARM_REG_R3,
      ARM_REG_R4, ARM_REG_R5, ARM_REG_R6, ARM_REG_R7,
      ARM_REG_R8, ARM_REG_R9, ARM_REG_R10, ARM_REG_R11,
      ARM_REG_R12, ARM_REG_LR);

  cpu_features = gum_query_cpu_features ();

  if ((cpu_features & GUM_CPU_VFP2) != 0)
    num_vfp_regs = ((cpu_features & GUM_CPU_VFPD32) != 0) ? 16 : 8;
  else
    num_vfp_regs = 0;

  if (num_vfp_regs != 0)
  {
    gum_arm_writer_put_vpush_range (&cw, ARM_REG_Q4, ARM_REG_Q7);
    sp_distance_to_saved_r1 += 4 * sizeof (GumArmVectorReg);
  }

  for (i = 0; i != 5; i++)
  {
    gum_arm_writer_put_ldr_reg_reg_offset (&cw, ARM_REG_R8 + i,
        ARM_REG_R0, G_STRUCT_OFFSET (GumCpuContext, r8) + (i * 4));
  }

  for (i = 0; i != num_vfp_regs; i++)
  {
    const guint offset =
        G_STRUCT_OFFSET (GumCpuContext, v) + (i * sizeof (GumArmVectorReg));

    gum_arm_writer_put_ldr_reg_reg_offset (&cw, ARM_REG_R1,
        ARM_REG_R0, offset + 0);
    gum_arm_writer_put_ldr_reg_reg_offset (&cw, ARM_REG_R2,
        ARM_REG_R0, offset + 4);
    gum_arm_writer_put_ldr_reg_reg_offset (&cw, ARM_REG_R3,
        ARM_REG_R0, offset + 8);
    gum_arm_writer_put_ldr_reg_reg_offset (&cw, ARM_REG_R4,
        ARM_REG_R0, offset + 12);
    gum_arm_writer_put_push_regs (&cw, 4,
        ARM_REG_R1, ARM_REG_R2, ARM_REG_R3, ARM_REG_R4);

    gum_arm_writer_put_vpop_range (&cw, ARM_REG_Q0 + i, ARM_REG_Q0 + i);
  }

  for (i = 7; i >= 0; i--)
  {
    gum_arm_writer_put_ldr_reg_reg_offset (&cw, ARM_REG_R0 + i,
        ARM_REG_R0, G_STRUCT_OFFSET (GumCpuContext, r) + (i * 4));
  }

  if ((target_func & 1) != 0)
    gum_arm_writer_put_blx_imm (&cw, target_func);
  else
    gum_arm_writer_put_bl_imm (&cw, target_func);

  gum_arm_writer_put_push_regs (&cw, 1, ARM_REG_R0);
  sp_distance_to_saved_r1 += 4;
  gum_arm_writer_put_ldr_reg_reg_offset (&cw, ARM_REG_R0,
      ARM_REG_SP, sp_distance_to_saved_r1);

  for (i = 0; i != 5; i++)
  {
    gum_arm_writer_put_str_reg_reg_offset (&cw, ARM_REG_R8 + i,
        ARM_REG_R0, G_STRUCT_OFFSET (GumCpuContext, r8) + (i * 4));
  }

  for (i = 0; i != num_vfp_regs; i++)
  {
    const guint offset =
        G_STRUCT_OFFSET (GumCpuContext, v) + (i * sizeof (GumArmVectorReg));

    gum_arm_writer_put_vpush_range (&cw, ARM_REG_Q0 + i, ARM_REG_Q0 + i);

    gum_arm_writer_put_pop_regs (&cw, 4,
        ARM_REG_R8, ARM_REG_R9, ARM_REG_R10, ARM_REG_R11);
    gum_arm_writer_put_str_reg_reg_offset (&cw, ARM_REG_R8,
        ARM_REG_R0, offset + 0);
    gum_arm_writer_put_str_reg_reg_offset (&cw, ARM_REG_R9,
        ARM_REG_R0, offset + 4);
    gum_arm_writer_put_str_reg_reg_offset (&cw, ARM_REG_R10,
        ARM_REG_R0, offset + 8);
    gum_arm_writer_put_str_reg_reg_offset (&cw, ARM_REG_R11,
        ARM_REG_R0, offset + 12);
  }

  for (i = 7; i >= 1; i--)
  {
    gum_arm_writer_put_str_reg_reg_offset (&cw, ARM_REG_R0 + i,
        ARM_REG_R0, G_STRUCT_OFFSET (GumCpuContext, r) + (i * 4));
  }
  gum_arm_writer_put_pop_regs (&cw, 1, ARM_REG_R1);
  gum_arm_writer_put_str_reg_reg_offset (&cw, ARM_REG_R1,
      ARM_REG_R0, G_STRUCT_OFFSET (GumCpuContext, r[0]));

  if (num_vfp_regs != 0)
    gum_arm_writer_put_vpop_range (&cw, ARM_REG_Q4, ARM_REG_Q7);

  gum_arm_writer_put_pop_regs (&cw, 14,
      ARM_REG_R0, ARM_REG_R1, ARM_REG_R2, ARM_REG_R3,
      ARM_REG_R4, ARM_REG_R5, ARM_REG_R6, ARM_REG_R7,
      ARM_REG_R8, ARM_REG_R9, ARM_REG_R10, ARM_REG_R11,
      ARM_REG_R12, ARM_REG_PC);

  gum_arm_writer_clear (&cw);
#elif defined (HAVE_ARM64)
  GumArm64Writer cw;
  guint sp_distance_to_saved_x1 = (15 * 16) + 8;
  gint i;

  gum_arm64_writer_init (&cw, mem);
  cw.pc = ctx->start_address;

  gum_arm64_writer_put_push_all_q_registers (&cw);
  gum_arm64_writer_put_push_all_x_registers (&cw);

  for (i = 31; i != -1; i--)
  {
    gum_arm64_writer_put_ldr_reg_reg_offset (&cw, ARM64_REG_Q0 + i,
        ARM64_REG_X0,
        G_STRUCT_OFFSET (GumCpuContext, v) + (i * sizeof (GumArm64VectorReg)));
  }

  gum_arm64_writer_put_ldr_reg_reg_offset (&cw, ARM64_REG_FP,
      ARM64_REG_X0, G_STRUCT_OFFSET (GumCpuContext, fp));
  for (i = 28; i != -1; i--)
  {
    gboolean is_platform_register = i == 18;
    if (is_platform_register)
      continue;
    gum_arm64_writer_put_ldr_reg_reg_offset (&cw, ARM64_REG_X0 + i,
        ARM64_REG_X0, G_STRUCT_OFFSET (GumCpuContext, x) + (i * 8));
  }

  gum_arm64_writer_put_bl_imm (&cw,
      gum_strip_code_address (GUM_ADDRESS (ctx->target_func)));

  gum_arm64_writer_put_push_reg_reg (&cw, ARM64_REG_X0, ARM64_REG_X1);
  sp_distance_to_saved_x1 += 16;
  gum_arm64_writer_put_ldr_reg_reg_offset (&cw, ARM64_REG_X0,
      ARM64_REG_SP, sp_distance_to_saved_x1);

  gum_arm64_writer_put_str_reg_reg_offset (&cw, ARM64_REG_FP,
      ARM64_REG_X0, G_STRUCT_OFFSET (GumCpuContext, fp));
  for (i = 28; i != 0; i--)
  {
    gum_arm64_writer_put_str_reg_reg_offset (&cw, ARM64_REG_X0 + i,
        ARM64_REG_X0, G_STRUCT_OFFSET (GumCpuContext, x) + (i * 8));
  }
  gum_arm64_writer_put_pop_reg_reg (&cw, ARM64_REG_X1, ARM64_REG_X2);
  gum_arm64_writer_put_str_reg_reg_offset (&cw, ARM64_REG_X1,
      ARM64_REG_X0, G_STRUCT_OFFSET (GumCpuContext, x[0]));

  for (i = 31; i != -1; i--)
  {
    gum_arm64_writer_put_str_reg_reg_offset (&cw, ARM64_REG_Q0 + i,
        ARM64_REG_X0,
        G_STRUCT_OFFSET (GumCpuContext, v) + (i * sizeof (GumArm64VectorReg)));
  }

  gum_arm64_writer_put_pop_all_x_registers (&cw);
  gum_arm64_writer_put_pop_all_q_registers (&cw);
  gum_arm64_writer_put_ret (&cw);

  gum_arm64_writer_clear (&cw);
#endif
}

void
invoke_clobber_test_function_with_carry_set (ClobberTestFunc target_func,
                                             gsize * flags_input,
                                             gsize * flags_output)
{
  guint8 * code;
  gsize code_size;
  GumEmitTestClobberFlagsContext ctx;
  InvokeWithCpuFlagsFunc func;

  code = allocate_clobber_test_invoker_func (target_func, &code_size);

  ctx.target_func = target_func;
  ctx.start_address = GUM_ADDRESS (code);

  gum_memory_patch_code (code, 1024,
      (GumMemoryPatchApplyFunc) gum_emit_test_clobber_flags_function,
      &ctx);

  func = GUM_POINTER_TO_FUNCPTR (InvokeWithCpuFlagsFunc,
      gum_sign_code_pointer (code));
  func (flags_input, flags_output);

  gum_memory_free (code, code_size);
}

static void
gum_emit_test_clobber_flags_function (gpointer mem,
                                      GumEmitTestClobberFlagsContext * ctx)
{
#if defined (HAVE_I386)
  GumX86Writer cw;
  gint align_correction;

# if GLIB_SIZEOF_VOID_P == 8
  align_correction = 8;
# else
  align_correction = 12;
# endif

  gum_x86_writer_init (&cw, mem);
  cw.pc = ctx->start_address;

# if GLIB_SIZEOF_VOID_P == 4
  /* Load arguments */
  gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_ECX, GUM_X86_ESP, 4);
  gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_EDX, GUM_X86_ESP, 8);
# endif

  gum_x86_writer_put_stc (&cw); /* set carry flag, likely to get clobbered */

  gum_x86_writer_put_pushfx (&cw);
  gum_x86_writer_put_pop_reg (&cw, GUM_X86_XAX);
  gum_x86_writer_put_mov_reg_ptr_reg (&cw,
      gum_x86_writer_get_cpu_register_for_nth_argument (&cw, 0), GUM_X86_XAX);

  gum_x86_writer_put_lea_reg_reg_offset (&cw, GUM_X86_XSP,
      GUM_X86_XSP, -align_correction);

  gum_x86_writer_put_call_address (&cw, GUM_ADDRESS (ctx->target_func));

  gum_x86_writer_put_lea_reg_reg_offset (&cw, GUM_X86_XSP,
      GUM_X86_XSP, align_correction);

  gum_x86_writer_put_pushfx (&cw);
  gum_x86_writer_put_pop_reg (&cw, GUM_X86_XAX);
  gum_x86_writer_put_mov_reg_ptr_reg (&cw,
      gum_x86_writer_get_cpu_register_for_nth_argument (&cw, 1), GUM_X86_XAX);

  gum_x86_writer_put_ret (&cw);

  gum_x86_writer_clear (&cw);
#elif defined (HAVE_ARM)
  GumAddress target_func = GUM_ADDRESS (ctx->target_func);
  GumArmWriter cw;

  gum_arm_writer_init (&cw, mem);
  cw.pc = ctx->start_address;

  gum_arm_writer_put_push_regs (&cw, 3, ARM_REG_R0, ARM_REG_R1, ARM_REG_LR);

  gum_arm_writer_put_ldr_reg_u32 (&cw, ARM_REG_R3, 0xff);
  gum_arm_writer_put_ands_reg_reg_imm (&cw, ARM_REG_R3, ARM_REG_R3, 0xff);
  gum_arm_writer_put_mov_reg_cpsr (&cw, ARM_REG_R2);
  gum_arm_writer_put_str_reg_reg (&cw, ARM_REG_R2, ARM_REG_R0);

  if ((target_func & 1) != 0)
    gum_arm_writer_put_blx_imm (&cw, target_func);
  else
    gum_arm_writer_put_bl_imm (&cw, target_func);

  gum_arm_writer_put_pop_regs (&cw, 2, ARM_REG_R0, ARM_REG_R1);
  gum_arm_writer_put_mov_reg_cpsr (&cw, ARM_REG_R2);
  gum_arm_writer_put_str_reg_reg (&cw, ARM_REG_R2, ARM_REG_R1);

  gum_arm_writer_put_pop_regs (&cw, 1, ARM_REG_PC);

  gum_arm_writer_clear (&cw);
#elif defined (HAVE_ARM64)
  GumArm64Writer cw;

  gum_arm64_writer_init (&cw, mem);
  cw.pc = ctx->start_address;

  gum_arm64_writer_put_push_reg_reg (&cw, ARM64_REG_X1, ARM64_REG_LR);

  gum_arm64_writer_put_mov_nzcv_reg (&cw, ARM64_REG_XZR);
  gum_arm64_writer_put_mov_reg_nzcv (&cw, ARM64_REG_X2);
  gum_arm64_writer_put_str_reg_reg (&cw, ARM64_REG_X2, ARM64_REG_X0);

  gum_arm64_writer_put_bl_imm (&cw,
      gum_strip_code_address (GUM_ADDRESS (ctx->target_func)));

  gum_arm64_writer_put_pop_reg_reg (&cw, ARM64_REG_X1, ARM64_REG_LR);

  gum_arm64_writer_put_mov_reg_nzcv (&cw, ARM64_REG_X2);
  gum_arm64_writer_put_str_reg_reg (&cw, ARM64_REG_X2, ARM64_REG_X1);

  gum_arm64_writer_put_ret (&cw);

  gum_arm64_writer_clear (&cw);
#endif
}

static gpointer
allocate_clobber_test_invoker_func (ClobberTestFunc target_func,
                                    gsize * code_size)
{
  gpointer code;
  GumAddressSpec addr_spec;
  gsize page_size;

  addr_spec.near_address =
      gum_strip_code_pointer (GUM_FUNCPTR_TO_POINTER (target_func));

  page_size = gum_query_page_size ();
#if defined (HAVE_ARM)
  addr_spec.max_distance = GUM_ARM_B_MAX_DISTANCE;
#elif defined (HAVE_ARM64)
  addr_spec.max_distance = GUM_ARM64_B_MAX_DISTANCE;
#else
  addr_spec.max_distance = G_MAXINT32 - page_size;
#endif

  *code_size = page_size;

  code = gum_memory_allocate_near (&addr_spec, *code_size, page_size,
      gum_memory_can_remap_writable () ? GUM_PAGE_RX : GUM_PAGE_RW);
  g_assert_nonnull (code);

  return code;
}

UnsupportedFunction *
unsupported_function_list_new (guint * count)
{
  static const UnsupportedFunction unsupported_functions[] =
  {
#if defined (HAVE_I386)
    { "ret",   1, 0, { 0xc3                                           } },
    { "retf",  1, 0, { 0xcb                                           } },
#elif defined (HAVE_ARM)
    { "ret",   2, 1, { 0x70, 0x47                                     } },
#elif defined (HAVE_ARM64)
    { "ret",   12, 0,
      {
        0xb0, 0x01, 0x80, 0xd2, /* mov x16, #13 */
        0xb1, 0x04, 0x80, 0xd2, /* mov x17, #37 */
        0xc0, 0x03, 0x5f, 0xd6, /* ret          */
      }
    },
#endif
  };
  UnsupportedFunction * result;
  gsize page_size;

  page_size = gum_query_page_size ();
  result = (UnsupportedFunction *) gum_memory_allocate (NULL, page_size,
      page_size, GUM_PAGE_RW);
  memcpy (result, unsupported_functions, sizeof (unsupported_functions));
  *count = G_N_ELEMENTS (unsupported_functions);

  return result;
}

void
unsupported_function_list_free (UnsupportedFunction * functions)
{
  gum_memory_free (functions, gum_query_page_size ());
}

#ifdef HAVE_I386

ProxyFunc
proxy_func_new_relative_with_target (TargetFunc target_func)
{
  GumAddressSpec addr_spec;
  gsize page_size;
  guint8 * func;

  addr_spec.near_address = target_func;
  page_size = gum_query_page_size ();
  addr_spec.max_distance = G_MAXINT32 - page_size;
  func = (guint8 *) gum_memory_allocate_near (&addr_spec,
      page_size, page_size, GUM_PAGE_RWX);
  func[0] = 0xe9;
  *((gint32 *) (func + 1)) =
      ((gssize) target_func) - (gssize) (func + 5);

  return GUM_POINTER_TO_FUNCPTR (ProxyFunc, func);
}

ProxyFunc
proxy_func_new_absolute_indirect_with_target (TargetFunc target_func)
{
  guint8 * func;
  gsize page_size;

  page_size = gum_query_page_size ();
  func = (guint8 *) gum_memory_allocate (NULL, page_size, page_size,
      GUM_PAGE_RWX);
  func[0] = 0xff;
  func[1] = 0x25;
# if GLIB_SIZEOF_VOID_P == 4
  *((gpointer *) (func + 2)) = func + 6;
# else
  *((gint32 *) (func + 2)) = 0;
# endif
  *((TargetFunc *) (func + 6)) = target_func;

  return GUM_POINTER_TO_FUNCPTR (ProxyFunc, func);
}

ProxyFunc
proxy_func_new_two_jumps_with_target (TargetFunc target_func)
{
  guint8 * func;
  gsize page_size;

  page_size = gum_query_page_size ();
  func = (guint8 *) gum_memory_allocate (NULL, page_size, page_size,
      GUM_PAGE_RWX);
  func[0] = 0xe9;
  *((gint32 *) (func + 1)) = (guint8 *) (func + 20) - (func + 5);

  func[20] = 0xff;
  func[21] = 0x25;
# if GLIB_SIZEOF_VOID_P == 4
  *((gpointer *)   (func + 22)) = func + 30;
# else
  *((gint32 *)     (func + 22)) = 4;
# endif
  *((TargetFunc *) (func + 30)) = target_func;

  return GUM_POINTER_TO_FUNCPTR (ProxyFunc, func);
}

ProxyFunc
proxy_func_new_early_call_with_target (TargetFunc target_func)
{
  GumAddressSpec addr_spec;
  gsize page_size;
  guint8 * func, * code;

  addr_spec.near_address = target_func;
  page_size = gum_query_page_size ();
  addr_spec.max_distance = G_MAXINT32 - page_size;
  func = (guint8 *) gum_memory_allocate_near (&addr_spec,
      page_size, page_size, GUM_PAGE_RWX);

  code = func;

# if GLIB_SIZEOF_VOID_P == 4
  code[0] = 0xff; /* push dword [esp + 4] */
  code[1] = 0x74;
  code[2] = 0x24;
  code[3] = 0x04;
  code += 4;
# else
  code[0] = 0x48; /* sub rsp, 0x28 (4 * sizeof (gpointer) + 8) */
  code[1] = 0x83;
  code[2] = 0xec;
  code[3] = 0x28;
  code += 4;
# endif

  code[0] = 0xe8; /* call */
  *((gssize *) (code + 1)) = (gssize) target_func - (gssize) (code + 5);
  code += 5;

# if GLIB_SIZEOF_VOID_P == 4
  code[0] = 0x83; /* add esp, 4 */
  code[1] = 0xc4;
  code[2] = 0x04;
  code += 3;
# else
  code[0] = 0x48; /* add rsp, 0x28 */
  code[1] = 0x83;
  code[2] = 0xc4;
  code[3] = 0x28;
  code += 4;
# endif

  *code++ = 0xc3; /* ret */

  return GUM_POINTER_TO_FUNCPTR (ProxyFunc, func);
}

# if GLIB_SIZEOF_VOID_P == 8

ProxyFunc
proxy_func_new_early_rip_relative_call_with_target (TargetFunc target_func)
{
  GumAddressSpec addr_spec;
  gsize page_size;
  guint8 * func, * code;

  addr_spec.near_address = target_func;
  page_size = gum_query_page_size ();
  addr_spec.max_distance = G_MAXINT32 - page_size;
  func = gum_memory_allocate_near (&addr_spec, page_size,
      page_size, GUM_PAGE_RWX);

  code = func;

  code[0] = 0x48; /* sub rsp, 0x38 */
  code[1] = 0x83;
  code[2] = 0xec;
  code[3] = 0x38;
  code += 4;

  code[0] = 0xff; /* call [rip + x] */
  code[1] = 0x15;
  *((gint32 *) (code + 2)) = 13;
  code += 6;

  code[0] = 0x48; /* add rsp, 0x38 */
  code[1] = 0x83;
  code[2] = 0xc4;
  code[3] = 0x38;
  code += 4;

  code[0] = 0xc3; /* ret */
  code++;

  code += 8;
  *((TargetFunc *) code) = target_func;

  return GUM_POINTER_TO_FUNCPTR (ProxyFunc, func);
}

# endif

void
proxy_func_free (ProxyFunc proxy_func)
{
  gum_memory_free ((gpointer) (gsize) proxy_func, gum_query_page_size ());
}

#endif
