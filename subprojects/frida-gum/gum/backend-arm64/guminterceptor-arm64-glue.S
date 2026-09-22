#define GUM_PADDING_SIZE              0
#define GUM_CPU_CONTEXT_SIZE          ((34 * 8) + (32 * 16))
#define GUM_NEXT_HOP_SIZE             16
#define GUM_FRAME_SIZE                ( \
                                        GUM_PADDING_SIZE + \
                                        GUM_CPU_CONTEXT_SIZE + \
                                        GUM_NEXT_HOP_SIZE \
                                      )

#define GUM_FRAME_OFFSET_CPU_CONTEXT   GUM_PADDING_SIZE
#define GUM_FRAME_OFFSET_NEXT_HOP     (GUM_PADDING_SIZE + GUM_CPU_CONTEXT_SIZE)

#define GUM_CPU_CONTEXT_OFFSET_LR     (33 * 8)

#ifdef __APPLE__
# define GUM_SYMBOL(s) _##s
#else
# define GUM_SYMBOL(s) s
#endif

.macro gum_emit_prolog
  # Absorb x16 and x17 pushed by caller
  sub sp, sp, GUM_FRAME_SIZE - (2 * 8)

  # We want to clobber x1, so let's save it (and x2) first
  stp x1, x2, [sp, GUM_PADDING_SIZE + (4 * 8)]

  # sample nzcv
  mrs x1, nzcv
  stp x1, x0, [sp, GUM_PADDING_SIZE + (2 * 8)]

  # Compute sp, and save it and x0
  add x1, sp, GUM_FRAME_SIZE
  stp x0, x1, [sp, GUM_PADDING_SIZE + (0 * 8)]

  # GumFunctionContext argument is in x17
  mov x0, x17
  ldp x16, x17, [x1, -(2 * 8)]

  # Save everything else
  stp x3, x4, [sp, GUM_PADDING_SIZE + (6 * 8)]
  stp x5, x6, [sp, GUM_PADDING_SIZE + (8 * 8)]
  stp x7, x8, [sp, GUM_PADDING_SIZE + (10 * 8)]
  stp x9, x10, [sp, GUM_PADDING_SIZE + (12 * 8)]
  stp x11, x12, [sp, GUM_PADDING_SIZE + (14 * 8)]
  stp x13, x14, [sp, GUM_PADDING_SIZE + (16 * 8)]
  stp x15, x16, [sp, GUM_PADDING_SIZE + (18 * 8)]
  stp x17, x18, [sp, GUM_PADDING_SIZE + (20 * 8)]
  stp x19, x20, [sp, GUM_PADDING_SIZE + (22 * 8)]
  stp x21, x22, [sp, GUM_PADDING_SIZE + (24 * 8)]
  stp x23, x24, [sp, GUM_PADDING_SIZE + (26 * 8)]
  stp x25, x26, [sp, GUM_PADDING_SIZE + (28 * 8)]
  stp x27, x28, [sp, GUM_PADDING_SIZE + (30 * 8)]
  stp fp, lr, [sp, GUM_PADDING_SIZE + (32 * 8)]
/* The frame keeps its size either way, so the GumCpuContext offsets the C side
   uses are unchanged; only the transfers go.  */
#ifdef __ARM_FP
  stp q0, q1, [sp, GUM_PADDING_SIZE + (34 * 8) + (0 * 16)]
  stp q2, q3, [sp, GUM_PADDING_SIZE + (34 * 8) + (2 * 16)]
  stp q4, q5, [sp, GUM_PADDING_SIZE + (34 * 8) + (4 * 16)]
  stp q6, q7, [sp, GUM_PADDING_SIZE + (34 * 8) + (6 * 16)]
  stp q8, q9, [sp, GUM_PADDING_SIZE + (34 * 8) + (8 * 16)]
  stp q10, q11, [sp, GUM_PADDING_SIZE + (34 * 8) + (10 * 16)]
  stp q12, q13, [sp, GUM_PADDING_SIZE + (34 * 8) + (12 * 16)]
  stp q14, q15, [sp, GUM_PADDING_SIZE + (34 * 8) + (14 * 16)]
  stp q16, q17, [sp, GUM_PADDING_SIZE + (34 * 8) + (16 * 16)]
  stp q18, q19, [sp, GUM_PADDING_SIZE + (34 * 8) + (18 * 16)]
  stp q20, q21, [sp, GUM_PADDING_SIZE + (34 * 8) + (20 * 16)]
  stp q22, q23, [sp, GUM_PADDING_SIZE + (34 * 8) + (22 * 16)]
  stp q24, q25, [sp, GUM_PADDING_SIZE + (34 * 8) + (24 * 16)]
  stp q26, q27, [sp, GUM_PADDING_SIZE + (34 * 8) + (26 * 16)]
  stp q28, q29, [sp, GUM_PADDING_SIZE + (34 * 8) + (28 * 16)]
  stp q30, q31, [sp, GUM_PADDING_SIZE + (34 * 8) + (30 * 16)]
#endif
.endm

.macro gum_emit_epilog
  ldr x0, [sp, GUM_PADDING_SIZE + (3 * 8)]
  ldp x1, x2, [sp, GUM_PADDING_SIZE + (4 * 8)]
  ldp x3, x4, [sp, GUM_PADDING_SIZE + (6 * 8)]
  ldp x5, x6, [sp, GUM_PADDING_SIZE + (8 * 8)]
  ldp x7, x8, [sp, GUM_PADDING_SIZE + (10 * 8)]
  ldp x9, x10, [sp, GUM_PADDING_SIZE + (12 * 8)]
  ldp x11, x12, [sp, GUM_PADDING_SIZE + (14 * 8)]
  ldp x13, x14, [sp, GUM_PADDING_SIZE + (16 * 8)]
  ldp x15, x16, [sp, GUM_PADDING_SIZE + (18 * 8)]
  ldp x17, x18, [sp, GUM_PADDING_SIZE + (20 * 8)]
  ldp x19, x20, [sp, GUM_PADDING_SIZE + (22 * 8)]
  ldp x21, x22, [sp, GUM_PADDING_SIZE + (24 * 8)]
  ldp x23, x24, [sp, GUM_PADDING_SIZE + (26 * 8)]
  ldp x25, x26, [sp, GUM_PADDING_SIZE + (28 * 8)]
  ldp x27, x28, [sp, GUM_PADDING_SIZE + (30 * 8)]
  ldp fp, lr, [sp, GUM_PADDING_SIZE + (32 * 8)]
#ifdef __ARM_FP
  ldp q0, q1, [sp, GUM_PADDING_SIZE + (34 * 8) + (0 * 16)]
  ldp q2, q3, [sp, GUM_PADDING_SIZE + (34 * 8) + (2 * 16)]
  ldp q4, q5, [sp, GUM_PADDING_SIZE + (34 * 8) + (4 * 16)]
  ldp q6, q7, [sp, GUM_PADDING_SIZE + (34 * 8) + (6 * 16)]
  ldp q8, q9, [sp, GUM_PADDING_SIZE + (34 * 8) + (8 * 16)]
  ldp q10, q11, [sp, GUM_PADDING_SIZE + (34 * 8) + (10 * 16)]
  ldp q12, q13, [sp, GUM_PADDING_SIZE + (34 * 8) + (12 * 16)]
  ldp q14, q15, [sp, GUM_PADDING_SIZE + (34 * 8) + (14 * 16)]
  ldp q16, q17, [sp, GUM_PADDING_SIZE + (34 * 8) + (16 * 16)]
  ldp q18, q19, [sp, GUM_PADDING_SIZE + (34 * 8) + (18 * 16)]
  ldp q20, q21, [sp, GUM_PADDING_SIZE + (34 * 8) + (20 * 16)]
  ldp q22, q23, [sp, GUM_PADDING_SIZE + (34 * 8) + (22 * 16)]
  ldp q24, q25, [sp, GUM_PADDING_SIZE + (34 * 8) + (24 * 16)]
  ldp q26, q27, [sp, GUM_PADDING_SIZE + (34 * 8) + (26 * 16)]
  ldp q28, q29, [sp, GUM_PADDING_SIZE + (34 * 8) + (28 * 16)]
  ldp q30, q31, [sp, GUM_PADDING_SIZE + (34 * 8) + (30 * 16)]
#endif

  ldr x16, [sp, GUM_FRAME_SIZE - (2 * 8)]
  add sp, sp, GUM_FRAME_SIZE
  br x16
.endm

  .text
  .align 2

  .globl GUM_SYMBOL (_gum_interceptor_begin_invocation)
GUM_SYMBOL (_gum_interceptor_begin_invocation):
#ifndef __APPLE__
  .type GUM_SYMBOL (_gum_interceptor_begin_invocation), %function
#endif
  gum_emit_prolog

  add x1, sp, GUM_FRAME_OFFSET_CPU_CONTEXT
  add x2, sp, GUM_FRAME_OFFSET_CPU_CONTEXT + GUM_CPU_CONTEXT_OFFSET_LR
  add x3, sp, GUM_FRAME_OFFSET_NEXT_HOP
  bl GUM_SYMBOL (_gum_function_context_begin_invocation)

  gum_emit_epilog

  .globl GUM_SYMBOL (_gum_interceptor_end_invocation)
GUM_SYMBOL (_gum_interceptor_end_invocation):
#ifndef __APPLE__
  .type GUM_SYMBOL (_gum_interceptor_end_invocation), %function
#endif
  gum_emit_prolog

  add x1, sp, GUM_FRAME_OFFSET_CPU_CONTEXT
  add x2, sp, GUM_FRAME_OFFSET_NEXT_HOP
  bl GUM_SYMBOL (_gum_function_context_end_invocation)

  gum_emit_epilog

