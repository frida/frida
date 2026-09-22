/*
 * Copyright (C) 2015-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumexceptorbackend.h"

#include "guminterceptor.h"
#ifdef HAVE_DARWIN
# include "gum/gumdarwin.h"
#endif
#ifdef HAVE_LINUX
# include "gum/gumlinux.h"
#endif
#ifdef HAVE_FREEBSD
# include "gum/gumfreebsd.h"
#endif
#ifdef HAVE_QNX
# include "gum/gumqnx.h"
/* Work around conflict between QNX headers and Capstone. */
# undef ARM_REG_R0
# undef ARM_REG_R1
# undef ARM_REG_R2
# undef ARM_REG_R3
# undef ARM_REG_R4
# undef ARM_REG_R5
# undef ARM_REG_R6
# undef ARM_REG_R7
# undef ARM_REG_R8
# undef ARM_REG_R9
# undef ARM_REG_R10
# undef ARM_REG_R11
# undef ARM_REG_R12
# undef ARM_REG_R13
# undef ARM_REG_R14
# undef ARM_REG_R15
# undef ARM_REG_SPSR
# undef ARM_REG_FP
# undef ARM_REG_IP
# undef ARM_REG_SP
# undef ARM_REG_LR
# undef ARM_REG_PC
#endif

#include <capstone.h>
#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#ifdef HAVE_PTRAUTH
# include <ptrauth.h>
#endif
#ifdef HAVE_QNX
# include <sys/debug.h>
# include <unix.h>
#endif

#if defined (HAVE_DARWIN) || defined (HAVE_FREEBSD) || defined (HAVE_QNX)
typedef sig_t GumSignalHandler;
#else
typedef sighandler_t GumSignalHandler;
#endif

struct _GumExceptorBackend
{
  GObject parent;

  gboolean disposed;

  GumExceptionHandler handler;
  gpointer handler_data;

  struct sigaction ** old_handlers;
  gint num_old_handlers;

  GumInterceptor * interceptor;
};

static void gum_exceptor_backend_dispose (GObject * object);

static void gum_exceptor_backend_attach (GumExceptorBackend * self);
static void gum_exceptor_backend_detach (GumExceptorBackend * self);
static void gum_exceptor_backend_detach_handler (GumExceptorBackend * self,
    int sig);
static sig_t gum_exceptor_backend_replacement_signal (int sig, sig_t handler);
static int gum_exceptor_backend_replacement_sigaction (int sig,
    const struct sigaction * act, struct sigaction * oact);
static void gum_exceptor_backend_on_signal (int sig, siginfo_t * siginfo,
    void * context);
#ifdef HAVE_PTRAUTH
G_GNUC_INTERNAL void _gum_exceptor_perform_divert (GumCpuContext * ctx);
static void gum_exceptor_stage_divert (volatile GumCpuContext * dst,
    const GumCpuContext * src);
#endif
static void gum_exceptor_backend_abort (GumExceptorBackend * self,
    GumExceptionDetails * details);

static gboolean gum_is_signal_handler_chainable (sig_t handler);

static gpointer gum_resolve_symbol (const gchar * symbol_name,
    GPtrArray * module_candidates);
static gpointer gum_try_resolve_symbol (const gchar * symbol_name,
    GPtrArray * module_candidates);

static void gum_parse_context (gconstpointer context,
    GumCpuContext * ctx);
G_GNUC_UNUSED static void gum_unparse_context (const GumCpuContext * ctx,
    gpointer context);

static GumMemoryOperation gum_infer_memory_operation (gconstpointer address,
    GumCpuContext * context);
static cs_insn * gum_disassemble_instruction_at (gconstpointer address,
    GumCpuContext * context);
#if defined (HAVE_I386)
static GumMemoryOperation gum_infer_x86_memory_operation (cs_insn * insn);
#elif defined (HAVE_ARM)
static GumMemoryOperation gum_infer_arm_memory_operation (cs_insn * insn);
#elif defined (HAVE_ARM64)
static GumMemoryOperation gum_infer_arm64_memory_operation (
    cs_insn * insn);
#endif

G_DEFINE_TYPE (GumExceptorBackend, gum_exceptor_backend, G_TYPE_OBJECT)

static GumExceptorBackend * the_backend = NULL;

static GumSignalHandler (* gum_original_signal) (int signum,
    GumSignalHandler handler);
static int (* gum_original_sigaction) (int signum, const struct sigaction * act,
    struct sigaction * oldact);

void
_gum_exceptor_backend_prepare_to_fork (void)
{
}

void
_gum_exceptor_backend_recover_from_fork_in_parent (void)
{
}

void
_gum_exceptor_backend_recover_from_fork_in_child (void)
{
}

static void
gum_exceptor_backend_class_init (GumExceptorBackendClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);
  GPtrArray * module_candidates;
  GumModule * libc;

  object_class->dispose = gum_exceptor_backend_dispose;

  module_candidates = g_ptr_array_new_full (3, g_object_unref);

  libc = gum_process_get_libc_module ();
  g_ptr_array_add (module_candidates, g_object_ref (libc));

#if defined (HAVE_DARWIN)
  gum_original_signal = gum_resolve_symbol ("signal", module_candidates);
#elif defined (HAVE_ANDROID)
  gum_original_signal = gum_try_resolve_symbol ("signal", module_candidates);
  if (gum_original_signal == NULL)
    gum_original_signal = gum_resolve_symbol ("bsd_signal", module_candidates);
#elif defined (HAVE_QNX)
  gum_original_signal = gum_resolve_symbol ("signal", module_candidates);
#else
  {
    gchar * libdir, * pthread_name;
    GumModule * pthread;

    libdir = g_path_get_dirname (gum_module_get_path (libc));
    pthread_name = g_build_filename (libdir, "libpthread.so.0", NULL);

    pthread = gum_process_find_module_by_name (pthread_name);
    if (pthread != NULL)
      g_ptr_array_insert (module_candidates, 0, pthread);

    g_free (pthread_name);
    g_free (libdir);
  }

  gum_original_signal = gum_resolve_symbol ("signal", module_candidates);
#endif

  gum_original_sigaction = gum_resolve_symbol ("sigaction", module_candidates);

  g_ptr_array_unref (module_candidates);
}

static void
gum_exceptor_backend_init (GumExceptorBackend * self)
{
  self->interceptor = gum_interceptor_obtain ();

  the_backend = self;
}

static void
gum_exceptor_backend_dispose (GObject * object)
{
  GumExceptorBackend * self = GUM_EXCEPTOR_BACKEND (object);

  if (!self->disposed)
  {
    self->disposed = TRUE;

    gum_exceptor_backend_detach (self);

    g_object_unref (self->interceptor);
    self->interceptor = NULL;

    the_backend = NULL;
  }

  G_OBJECT_CLASS (gum_exceptor_backend_parent_class)->dispose (object);
}

GumExceptorBackend *
gum_exceptor_backend_new (GumExceptionHandler handler,
                          gpointer user_data)
{
  GumExceptorBackend * backend;

  backend = g_object_new (GUM_TYPE_EXCEPTOR_BACKEND, NULL);
  backend->handler = handler;
  backend->handler_data = user_data;

  gum_exceptor_backend_attach (backend);

  return backend;
}

static void
gum_exceptor_backend_attach (GumExceptorBackend * self)
{
  GumInterceptor * interceptor = self->interceptor;
  const gint handled_signals[] = {
    SIGABRT,
    SIGSEGV,
    SIGBUS,
    SIGILL,
    SIGFPE,
    SIGTRAP,
    SIGSYS,
  };
  gint highest, i;
  struct sigaction action;
  GumReplaceOptions options = { 0, };

  options.replacement_data = self;

  highest = 0;
  for (i = 0; i != G_N_ELEMENTS (handled_signals); i++)
    highest = MAX (handled_signals[i], highest);
  g_assert (highest > 0);
  self->num_old_handlers = highest + 1;
  self->old_handlers = g_new0 (struct sigaction *, self->num_old_handlers);

  action.sa_sigaction = gum_exceptor_backend_on_signal;
  sigemptyset (&action.sa_mask);
  action.sa_flags = SA_SIGINFO | SA_NODEFER;
#ifdef SA_ONSTACK
  action.sa_flags |= SA_ONSTACK;
#endif
  for (i = 0; i != G_N_ELEMENTS (handled_signals); i++)
  {
    gint sig = handled_signals[i];
    struct sigaction * old_handler;

    old_handler = g_slice_new0 (struct sigaction);
    self->old_handlers[sig] = old_handler;
    gum_original_sigaction (sig, &action, old_handler);
  }

  if (_gum_exceptor_get_mode () == GUM_EXCEPTOR_MODE_FULL)
  {
    gum_interceptor_begin_transaction (interceptor);

    gum_interceptor_replace (interceptor, gum_original_signal,
        gum_exceptor_backend_replacement_signal, NULL, &options);
    gum_interceptor_replace (interceptor, gum_original_sigaction,
        gum_exceptor_backend_replacement_sigaction, NULL, &options);

    gum_interceptor_end_transaction (interceptor);
  }
}

static void
gum_exceptor_backend_detach (GumExceptorBackend * self)
{
  GumInterceptor * interceptor = self->interceptor;
  gint i;

  if (_gum_exceptor_get_mode () == GUM_EXCEPTOR_MODE_FULL)
  {
    gum_interceptor_begin_transaction (interceptor);

    gum_interceptor_revert (interceptor, gum_original_signal);
    gum_interceptor_revert (interceptor, gum_original_sigaction);

    gum_interceptor_end_transaction (interceptor);
  }

  for (i = 0; i != self->num_old_handlers; i++)
    gum_exceptor_backend_detach_handler (self, i);
  g_free (self->old_handlers);
  self->old_handlers = NULL;
  self->num_old_handlers = 0;
}

static void
gum_exceptor_backend_detach_handler (GumExceptorBackend * self,
                                     int sig)
{
  struct sigaction * old_handler;

  old_handler = self->old_handlers[sig];
  if (old_handler == NULL)
    return;

  self->old_handlers[sig] = NULL;
  gum_original_sigaction (sig, old_handler, NULL);
  g_slice_free (struct sigaction, old_handler);
}

static struct sigaction *
gum_exceptor_backend_get_old_handler (GumExceptorBackend * self,
                                      gint sig)
{
  if (sig < 0 || sig >= self->num_old_handlers)
    return NULL;

  return self->old_handlers[sig];
}

static sig_t
gum_exceptor_backend_replacement_signal (int sig,
                                         sig_t handler)
{
  GumExceptorBackend * self;
  GumInvocationContext * ctx;
  struct sigaction * old_handler;
  sig_t result;

  ctx = gum_interceptor_get_current_invocation ();
  g_assert (ctx != NULL);

  self = GUM_EXCEPTOR_BACKEND (
      gum_invocation_context_get_replacement_data (ctx));

  old_handler = gum_exceptor_backend_get_old_handler (self, sig);
  if (old_handler == NULL)
    return gum_original_signal (sig, handler);

  result = ((old_handler->sa_flags & SA_SIGINFO) == 0)
      ? old_handler->sa_handler
      : SIG_DFL;

  old_handler->sa_handler = handler;
  old_handler->sa_flags &= ~SA_SIGINFO;

  return result;
}

static int
gum_exceptor_backend_replacement_sigaction (int sig,
                                            const struct sigaction * act,
                                            struct sigaction * oact)
{
  GumExceptorBackend * self;
  GumInvocationContext * ctx;
  struct sigaction * old_handler;
  struct sigaction previous_old_handler;

  ctx = gum_interceptor_get_current_invocation ();
  g_assert (ctx != NULL);

  self = GUM_EXCEPTOR_BACKEND (
      gum_invocation_context_get_replacement_data (ctx));

  old_handler = gum_exceptor_backend_get_old_handler (self, sig);
  if (old_handler == NULL)
    return gum_original_sigaction (sig, act, oact);

  previous_old_handler = *old_handler;
  if (act != NULL)
    *old_handler = *act;
  if (oact != NULL)
    *oact = previous_old_handler;

  return 0;
}

static void
gum_exceptor_backend_on_signal (int sig,
                                siginfo_t * siginfo,
                                void * context)
{
  GumExceptorBackend * self = the_backend;
  GumExceptionDetails ed;
  GumExceptionMemoryDetails * md = &ed.memory;
  GumCpuContext * cpu_context = &ed.context;
  struct sigaction * action;
#ifdef HAVE_PTRAUTH
  volatile GumCpuContext divert_ctx;
  sigjmp_buf divert_env;
#endif

  action = self->old_handlers[sig];

#ifdef HAVE_PTRAUTH
  if (sigsetjmp (divert_env, 1) != 0)
    _gum_exceptor_perform_divert ((GumCpuContext *) &divert_ctx);
#endif

  ed.thread_id = gum_process_get_current_thread_id ();

  switch (sig)
  {
    case SIGABRT:
      ed.type = GUM_EXCEPTION_ABORT;
      break;
    case SIGSEGV:
    case SIGBUS:
      ed.type = GUM_EXCEPTION_ACCESS_VIOLATION;
      break;
    case SIGILL:
      ed.type = GUM_EXCEPTION_ILLEGAL_INSTRUCTION;
      break;
    case SIGFPE:
      ed.type = GUM_EXCEPTION_ARITHMETIC;
      break;
    case SIGTRAP:
      ed.type = GUM_EXCEPTION_BREAKPOINT;
      break;
    default:
      ed.type = GUM_EXCEPTION_SYSTEM;
      break;
  }

  gum_parse_context (context, cpu_context);
  ed.native_context = context;

#if defined (HAVE_I386)
  ed.address = GSIZE_TO_POINTER (GUM_CPU_CONTEXT_XIP (cpu_context));
#elif defined (HAVE_ARM) || defined (HAVE_ARM64)
  ed.address = gum_strip_code_pointer (GSIZE_TO_POINTER (cpu_context->pc));
#elif defined (HAVE_MIPS)
  ed.address = GSIZE_TO_POINTER (cpu_context->pc);
#else
# error Unsupported architecture
#endif

  switch (sig)
  {
    case SIGSEGV:
    case SIGBUS:
      if (siginfo->si_addr == ed.address)
        md->operation = GUM_MEMOP_EXECUTE;
      else
        md->operation = gum_infer_memory_operation (ed.address, cpu_context);
      md->address = siginfo->si_addr;
      break;
    default:
      md->operation = GUM_MEMOP_INVALID;
      md->address = NULL;
      break;
  }

  if (action == NULL)
    gum_exceptor_backend_abort (self, &ed);

  if (self->handler (&ed, self->handler_data))
  {
#ifdef HAVE_PTRAUTH
    gum_exceptor_stage_divert (&divert_ctx, cpu_context);
    siglongjmp (divert_env, 1);
#else
    gum_unparse_context (cpu_context, context);
    return;
#endif
  }

  if ((action->sa_flags & SA_SIGINFO) != 0)
  {
    void (* old_sigaction) (int, siginfo_t *, void *) = action->sa_sigaction;

    if (old_sigaction != NULL)
      old_sigaction (sig, siginfo, context);
    else
      goto panic;
  }
  else
  {
    void (* old_handler) (int) = action->sa_handler;

    if (gum_is_signal_handler_chainable (old_handler))
      old_handler (sig);
    else if (action->sa_handler != SIG_IGN)
      goto panic;
  }

  if (sig == SIGABRT)
    goto panic;

  return;

panic:
  gum_exceptor_backend_detach_handler (self, sig);
}

#ifdef HAVE_PTRAUTH

static void
gum_exceptor_stage_divert (volatile GumCpuContext * dst,
                           const GumCpuContext * src)
{
  *((GumCpuContext *) dst) = *src;
  dst->pc = GPOINTER_TO_SIZE (ptrauth_strip (GSIZE_TO_POINTER (dst->pc),
      ptrauth_key_process_independent_code));
  dst->lr = GPOINTER_TO_SIZE (ptrauth_strip (GSIZE_TO_POINTER (dst->lr),
      ptrauth_key_process_independent_code));
  dst->sp = GPOINTER_TO_SIZE (ptrauth_strip (GSIZE_TO_POINTER (dst->sp),
      ptrauth_key_process_independent_data));
  dst->fp = GPOINTER_TO_SIZE (ptrauth_strip (GSIZE_TO_POINTER (dst->fp),
      ptrauth_key_process_independent_data));
}

#endif

static void
gum_exceptor_backend_abort (GumExceptorBackend * self,
                            GumExceptionDetails * details)
{
  /* TODO: should we create a backtrace and log it? */
  abort ();
}

static gboolean
gum_is_signal_handler_chainable (sig_t handler)
{
  return handler != SIG_DFL && handler != SIG_IGN && handler != SIG_ERR;
}

static gpointer
gum_resolve_symbol (const gchar * symbol_name,
                    GPtrArray * module_candidates)
{
  gpointer result;

  result = gum_try_resolve_symbol (symbol_name, module_candidates);
  if (result == NULL)
    gum_panic ("Unable to locate %s(); please file a bug", symbol_name);

  return result;
}

static gpointer
gum_try_resolve_symbol (const gchar * symbol_name,
                        GPtrArray * module_candidates)
{
  guint i;

  for (i = 0; i != module_candidates->len; i++)
  {
    GumModule * module;
    GumAddress address;

    module = g_ptr_array_index (module_candidates, i);

    address = gum_module_find_export_by_name (module, symbol_name);
    if (address != 0)
      return GSIZE_TO_POINTER (address);
  }

  return NULL;
}

#if defined (HAVE_DARWIN)

static void
gum_parse_context (gconstpointer context,
                   GumCpuContext * ctx)
{
  const ucontext_t * uc = context;

  gum_darwin_parse_native_thread_state (&uc->uc_mcontext->__ss, ctx);
#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
  ctx->xmm = (GumX86VectorReg *) &uc->uc_mcontext->__fs.__fpu_xmm0;
#endif
}

static void
gum_unparse_context (const GumCpuContext * ctx,
                     gpointer context)
{
  ucontext_t * uc = context;

  gum_darwin_unparse_native_thread_state (ctx, &uc->uc_mcontext->__ss);
}

#elif defined (HAVE_LINUX)

static void
gum_parse_context (gconstpointer context,
                   GumCpuContext * ctx)
{
  const ucontext_t * uc = context;

  gum_linux_parse_ucontext (uc, ctx);
}

static void
gum_unparse_context (const GumCpuContext * ctx,
                     gpointer context)
{
  ucontext_t * uc = context;

  gum_linux_unparse_ucontext (ctx, uc);
}

#elif defined (HAVE_FREEBSD)

static void
gum_parse_context (gconstpointer context,
                   GumCpuContext * ctx)
{
  const ucontext_t * uc = context;

  gum_freebsd_parse_ucontext (uc, ctx);
}

static void
gum_unparse_context (const GumCpuContext * ctx,
                     gpointer context)
{
  ucontext_t * uc = context;

  gum_freebsd_unparse_ucontext (ctx, uc);
}

#elif defined (HAVE_QNX)

static void
gum_parse_context (gconstpointer context,
                   GumCpuContext * ctx)
{
  const ucontext_t * uc = context;

  gum_qnx_parse_ucontext (uc, ctx);
}

static void
gum_unparse_context (const GumCpuContext * ctx,
                     gpointer context)
{
  ucontext_t * uc = context;

  gum_qnx_unparse_ucontext (ctx, uc);
}

#endif

static GumMemoryOperation
gum_infer_memory_operation (gconstpointer address,
                            GumCpuContext * context)
{
  GumMemoryOperation op;
  cs_insn * insn;

  insn = gum_disassemble_instruction_at (address, context);
  if (insn == NULL)
    return GUM_MEMOP_READ;

#if defined (HAVE_I386)
  op = gum_infer_x86_memory_operation (insn);
#elif defined (HAVE_ARM)
  op = gum_infer_arm_memory_operation (insn);
#elif defined (HAVE_ARM64)
  op = gum_infer_arm64_memory_operation (insn);
#else
  op = GUM_MEMOP_READ;
#endif

  cs_free (insn, 1);

  return op;
}

static cs_insn *
gum_disassemble_instruction_at (gconstpointer address,
                                GumCpuContext * context)
{
  cs_insn * insn = NULL;
  csh capstone;
  cs_err err;

  gum_cs_arch_register_native ();
#if defined (HAVE_I386)
  err = cs_open (CS_ARCH_X86, GUM_CPU_MODE, &capstone);
#elif defined (HAVE_ARM)
  err = cs_open (CS_ARCH_ARM,
      (((context->cpsr & GUM_PSR_T_BIT) != 0) ? CS_MODE_THUMB : CS_MODE_ARM) |
      CS_MODE_V8 | GUM_DEFAULT_CS_ENDIAN, &capstone);
#elif defined (HAVE_ARM64)
  err = cs_open (CS_ARCH_ARM64, GUM_DEFAULT_CS_ENDIAN, &capstone);
#else
  return NULL;
#endif

  if (err != CS_ERR_OK)
    return NULL;

  cs_option (capstone, CS_OPT_DETAIL, CS_OPT_ON);
  cs_disasm (capstone, address, 16, GPOINTER_TO_SIZE (address), 1, &insn);
  cs_close (&capstone);

  return insn;
}

#if defined (HAVE_I386)

static GumMemoryOperation
gum_infer_x86_memory_operation (cs_insn * insn)
{
  switch (insn->id)
  {
    case X86_INS_CLI:
    case X86_INS_STI:
    case X86_INS_CLC:
    case X86_INS_STC:
    case X86_INS_CLAC:
    case X86_INS_CLGI:
    case X86_INS_CLTS:
    case X86_INS_CLWB:
    case X86_INS_STAC:
    case X86_INS_STGI:
    case X86_INS_CPUID:
    case X86_INS_MOVNTQ:
    case X86_INS_MOVNTDQA:
    case X86_INS_MOVNTDQ:
    case X86_INS_MOVNTI:
    case X86_INS_MOVNTPD:
    case X86_INS_MOVNTPS:
    case X86_INS_MOVNTSD:
    case X86_INS_MOVNTSS:
    case X86_INS_VMOVNTDQA:
    case X86_INS_VMOVNTDQ:
    case X86_INS_VMOVNTPD:
    case X86_INS_VMOVNTPS:
    case X86_INS_MOVSS:
    case X86_INS_MOV:
    case X86_INS_MOVAPS:
    case X86_INS_MOVAPD:
    case X86_INS_MOVZX:
    case X86_INS_MOVUPS:
    case X86_INS_MOVABS:
    case X86_INS_MOVHPD:
    case X86_INS_MOVHPS:
    case X86_INS_MOVLPD:
    case X86_INS_MOVLPS:
    case X86_INS_MOVBE:
    case X86_INS_MOVSB:
    case X86_INS_MOVSD:
    case X86_INS_MOVSQ:
    case X86_INS_MOVSX:
    case X86_INS_MOVSXD:
    case X86_INS_MOVSW:
    case X86_INS_MOVD:
    case X86_INS_MOVQ:
    case X86_INS_MOVDQ2Q:
    case X86_INS_RDRAND:
    case X86_INS_RDSEED:
    case X86_INS_RDMSR:
    case X86_INS_RDPMC:
    case X86_INS_RDTSC:
    case X86_INS_RDTSCP:
    case X86_INS_CRC32:
    case X86_INS_SHA1MSG1:
    case X86_INS_SHA1MSG2:
    case X86_INS_SHA1NEXTE:
    case X86_INS_SHA1RNDS4:
    case X86_INS_SHA256MSG1:
    case X86_INS_SHA256MSG2:
    case X86_INS_SHA256RNDS2:
    case X86_INS_AESDECLAST:
    case X86_INS_AESDEC:
    case X86_INS_AESENCLAST:
    case X86_INS_AESENC:
    case X86_INS_AESIMC:
    case X86_INS_AESKEYGENASSIST:
    case X86_INS_PACKSSDW:
    case X86_INS_PACKSSWB:
    case X86_INS_PACKUSWB:
    case X86_INS_XCHG:
    case X86_INS_CLD:
    case X86_INS_STD:
      switch (insn->detail->x86.operands[0].type)
      {
        case X86_OP_MEM:
          return GUM_MEMOP_WRITE;
        case X86_OP_REG:
          if (insn->detail->x86.operands[1].type == X86_OP_MEM)
            return GUM_MEMOP_READ;
        default:
          return GUM_MEMOP_READ;
      }
    default:
      return GUM_MEMOP_READ;
  }
}

#elif defined (HAVE_ARM)

static GumMemoryOperation
gum_infer_arm_memory_operation (cs_insn * insn)
{
  switch (insn->id)
  {
    case ARM_INS_STREX:
    case ARM_INS_STREXB:
    case ARM_INS_STREXD:
    case ARM_INS_STREXH:
    case ARM_INS_STR:
    case ARM_INS_STRB:
    case ARM_INS_STRD:
    case ARM_INS_STRBT:
    case ARM_INS_STRH:
    case ARM_INS_STRHT:
    case ARM_INS_STRT:
      return GUM_MEMOP_WRITE;
    default:
      return GUM_MEMOP_READ;
  }
}

#elif defined (HAVE_ARM64)

static GumMemoryOperation
gum_infer_arm64_memory_operation (cs_insn * insn)
{
  switch (insn->id)
  {
    case ARM64_INS_STRB:
    case ARM64_INS_STURB:
    case ARM64_INS_STUR:
    case ARM64_INS_STR:
    case ARM64_INS_STP:
    case ARM64_INS_STNP:
    case ARM64_INS_STXR:
    case ARM64_INS_STXRH:
    case ARM64_INS_STLXRH:
    case ARM64_INS_STXRB:
      return GUM_MEMOP_WRITE;
    default:
      return GUM_MEMOP_READ;
  }
}

#endif
