/*
 * Copyright (C) 2022-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2023 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumprocess-priv.h"

#include "gum-init.h"
#include "gummodule-elf.h"
#include "gum/gumfreebsd.h"

#include <dlfcn.h>
#include <errno.h>
#include <pthread_np.h>
#include <stdlib.h>
#include <strings.h>
#include <ucontext.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/sysctl.h>
#include <sys/thr.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>

#define GUM_TEMP_FAILURE_RETRY(expression) \
    ({ \
      gssize __result; \
      \
      do __result = (gssize) (expression); \
      while (__result == -EINTR); \
      \
      __result; \
    })

typedef struct _GumModifyThreadContext GumModifyThreadContext;

struct _GumModifyThreadContext
{
  gint fd[2];
  pid_t pid;
  lwpid_t target_thread;
  lwpid_t interruptible_thread;
};

static void gum_deinit_libc_module (void);

static void gum_do_modify_thread (GumModifyThreadContext * ctx);
static gboolean gum_read_chunk (gint fd, gpointer buffer, gsize length);
static gboolean gum_write_chunk (gint fd, gconstpointer buffer, gsize length);
static gboolean gum_wait_for_child_signal (pid_t pid, gint expected_signal);

static void gum_store_cpu_context (GumThreadId thread_id,
    GumCpuContext * cpu_context, gpointer user_data);

static gchar * gum_query_program_path_for_target (int target, GError ** error);

static struct kinfo_proc * gum_query_threads (guint * count);
static void gum_thread_details_from_proc (GumThreadDetails * thread,
    const struct kinfo_proc * p, GumThreadFlags flags);
static GumThreadState gum_thread_state_from_proc (const struct kinfo_proc * p);
static GumPageProtection gum_page_protection_from_vmentry (int native_prot);

static GumModule * gum_libc_module;

GumModule *
gum_process_get_libc_module (void)
{
  static gsize modules_value = 0;

  if (g_once_init_enter (&modules_value))
  {
    const gchar * symbol_in_libc = "exit";
    gpointer addr_in_libc;

    addr_in_libc = dlsym (RTLD_NEXT, symbol_in_libc);
    if (addr_in_libc == NULL)
    {
      addr_in_libc = dlsym (RTLD_DEFAULT, symbol_in_libc);
      g_assert (addr_in_libc != NULL);
    }

    gum_libc_module =
        gum_process_find_module_by_address (GUM_ADDRESS (addr_in_libc));

    _gum_register_destructor (gum_deinit_libc_module);

    g_once_init_leave (&modules_value, GPOINTER_TO_SIZE (gum_libc_module));
  }

  return GSIZE_TO_POINTER (modules_value);
}

static void
gum_deinit_libc_module (void)
{
  g_object_unref (gum_libc_module);
}

gboolean
gum_process_is_debugger_attached (void)
{
  int mib[4];
  struct kinfo_proc info;
  size_t size;
  int result G_GNUC_UNUSED;

  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_PID;
  mib[3] = getpid ();

  size = sizeof (info);

  result = sysctl (mib, G_N_ELEMENTS (mib), &info, &size, NULL, 0);
  g_assert (result == 0);

  return (info.ki_flag & P_TRACED) != 0;
}

GumProcessId
gum_process_get_id (void)
{
  return getpid ();
}

GumThreadId
gum_process_get_current_thread_id (void)
{
  return pthread_getthreadid_np ();
}

gboolean
gum_process_has_thread (GumThreadId thread_id)
{
  return thr_kill (thread_id, 0) == 0;
}

GumThreadDetails *
gum_process_find_thread_by_id (GumThreadId thread_id,
                               GumThreadFlags flags)
{
  GumThreadDetails * result = NULL;
  struct kinfo_proc * threads;
  guint n, i;

  threads = gum_query_threads (&n);
  if (threads == NULL)
    return NULL;

  for (i = 0; i != n; i++)
  {
    const struct kinfo_proc * p = &threads[i];
    GumThreadDetails thread = { 0, };

    if (p->ki_tid != thread_id)
      continue;

    if (gum_thread_details_from_proc (&thread, p, flags))
      result = gum_thread_details_copy (&thread);
    break;
  }

  g_free (threads);

  return result;
}

gboolean
gum_process_modify_thread (GumThreadId thread_id,
                           GumModifyThreadFunc func,
                           gpointer user_data,
                           GumModifyThreadFlags flags)
{
  gboolean success = FALSE;

  if (thread_id == gum_process_get_current_thread_id ())
  {
    ucontext_t uc;
    volatile gboolean modified = FALSE;

    getcontext (&uc);
    if (!modified)
    {
      GumCpuContext cpu_context;

      gum_freebsd_parse_ucontext (&uc, &cpu_context);
      func (thread_id, &cpu_context, user_data);
      gum_freebsd_unparse_ucontext (&cpu_context, &uc);

      modified = TRUE;
      setcontext (&uc);
    }

    success = TRUE;
  }
  else
  {
    GumModifyThreadContext ctx;
    gint child, fd;
    GumCpuContext cpu_context;
    guint i;
    guint8 close_ack;
    ssize_t n;
    int status;

    if (socketpair (AF_UNIX, SOCK_STREAM, 0, ctx.fd) != 0)
      return FALSE;
    ctx.pid = getpid ();
    ctx.target_thread = thread_id;
    ctx.interruptible_thread = pthread_getthreadid_np ();

    child = fork ();
    if (child == -1)
      goto beach;
    if (child == 0)
    {
      gum_do_modify_thread (&ctx);
      _Exit (0);
    }

    fd = ctx.fd[0];
    close (ctx.fd[1]);
    ctx.fd[1] = -1;

    if (!gum_read_chunk (fd, &cpu_context, sizeof (GumCpuContext)))
      goto beach;

    func (thread_id, &cpu_context, user_data);

    if (!gum_write_chunk (fd, &cpu_context, sizeof (GumCpuContext)))
      goto beach;

    n = GUM_TEMP_FAILURE_RETRY (read (fd, &close_ack, sizeof (close_ack)));
    if (n != 0)
      goto beach;

    waitpid (child, &status, 0);

    success = TRUE;

beach:
    for (i = 0; i != G_N_ELEMENTS (ctx.fd); i++)
    {
      gint sockfd = ctx.fd[i];
      if (sockfd != -1)
        close (sockfd);
    }
  }

  return success;
}

static void
gum_do_modify_thread (GumModifyThreadContext * ctx)
{
  const gint fd = ctx->fd[1];
  gboolean attached;
  struct reg regs;
  GumCpuContext cpu_context;

  attached = FALSE;

  close (ctx->fd[0]);
  ctx->fd[0] = -1;

  if (ptrace (PT_ATTACH, ctx->pid, NULL, 0) != 0)
    goto beach;
  attached = TRUE;
  if (!gum_wait_for_child_signal (ctx->pid, SIGSTOP))
    goto beach;

  if (ptrace (PT_GETREGS, ctx->target_thread, (caddr_t) &regs, 0) != 0)
    goto beach;
  if (ptrace (PT_SUSPEND, ctx->target_thread, NULL, 0) != 0)
    goto beach;
  if (ptrace (PT_CONTINUE, ctx->pid, GSIZE_TO_POINTER (1), 0) != 0)
    goto beach;

  gum_freebsd_parse_regs (&regs, &cpu_context);
  if (!gum_write_chunk (fd, &cpu_context, sizeof (GumCpuContext)))
    goto beach;

  if (!gum_read_chunk (fd, &cpu_context, sizeof (GumCpuContext)))
    goto beach;
  gum_freebsd_unparse_regs (&cpu_context, &regs);

  if (thr_kill2 (ctx->pid, ctx->interruptible_thread, SIGSTOP) != 0)
    goto beach;
  if (!gum_wait_for_child_signal (ctx->pid, SIGSTOP))
    goto beach;
  if (ptrace (PT_SETREGS, ctx->target_thread, (caddr_t) &regs, 0) != 0)
    goto beach;

  goto beach;

beach:
  {
    if (attached)
      ptrace (PT_DETACH, ctx->pid, NULL, 0);

    close (fd);

    return;
  }
}

static gboolean
gum_read_chunk (gint fd,
                gpointer buffer,
                gsize length)
{
  gpointer cursor = buffer;
  gsize remaining = length;

  while (remaining != 0)
  {
    ssize_t n;

    n = GUM_TEMP_FAILURE_RETRY (read (fd, cursor, remaining));
    if (n <= 0)
      return FALSE;

    cursor += n;
    remaining -= n;
  }

  return TRUE;
}

static gboolean
gum_write_chunk (gint fd,
                 gconstpointer buffer,
                 gsize length)
{
  gconstpointer cursor = buffer;
  gsize remaining = length;

  while (remaining != 0)
  {
    ssize_t n;

    n = GUM_TEMP_FAILURE_RETRY (write (fd, cursor, remaining));
    if (n <= 0)
      return FALSE;

    cursor += n;
    remaining -= n;
  }

  return TRUE;
}

static gboolean
gum_wait_for_child_signal (pid_t pid,
                           gint expected_signal)
{
  int status;

  if (waitpid (pid, &status, 0) == -1)
    return FALSE;

  if (!WIFSTOPPED (status))
    return FALSE;

  return WSTOPSIG (status) == expected_signal;
}

void
_gum_process_enumerate_threads (GumFoundThreadFunc func,
                                gpointer user_data,
                                GumThreadFlags flags)
{
  struct kinfo_proc * threads;
  guint n, i;

  threads = gum_query_threads (&n);
  if (threads == NULL)
    return;

  for (i = 0; i != n; i++)
  {
    GumThreadDetails thread = { 0, };

    if (!gum_thread_details_from_proc (&thread, &threads[i], flags))
      continue;

    if (!func (&thread, user_data))
      break;
  }

  g_free (threads);
}

static struct kinfo_proc *
gum_query_threads (guint * count)
{
  struct kinfo_proc * threads = NULL;
  int mib[4];
  size_t size;

  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_PID | KERN_PROC_INC_THREAD;
  mib[3] = getpid ();

  size = 0;
  if (sysctl (mib, G_N_ELEMENTS (mib), NULL, &size, NULL, 0) != 0)
    return NULL;

  while (TRUE)
  {
    size_t previous_size;
    gboolean still_too_small;

    threads = g_realloc (threads, size);

    previous_size = size;
    if (sysctl (mib, G_N_ELEMENTS (mib), threads, &size, NULL, 0) == 0)
      break;

    still_too_small = errno == ENOMEM && size == previous_size;
    if (!still_too_small)
    {
      g_free (threads);
      return NULL;
    }

    size += size / 10;
  }

  *count = size / sizeof (struct kinfo_proc);

  return threads;
}

static gboolean
gum_thread_details_from_proc (GumThreadDetails * thread,
                              const struct kinfo_proc * p,
                              GumThreadFlags flags)
{
  thread->id = p->ki_tid;

  if ((flags & GUM_THREAD_FLAGS_NAME) != 0)
  {
    if (p->ki_tdname[0] != '\0')
    {
      thread->name = p->ki_tdname;
      thread->flags |= GUM_THREAD_FLAGS_NAME;
    }
  }

  if ((flags & GUM_THREAD_FLAGS_STATE) != 0)
  {
    thread->state = gum_thread_state_from_proc (p);
    thread->flags |= GUM_THREAD_FLAGS_STATE;
  }

  if ((flags & GUM_THREAD_FLAGS_CPU_CONTEXT) != 0)
  {
    if (!gum_process_modify_thread (thread->id, gum_store_cpu_context,
          &thread->cpu_context, GUM_MODIFY_THREAD_FLAGS_ABORT_SAFELY))
      return FALSE;
    thread->flags |= GUM_THREAD_FLAGS_CPU_CONTEXT;
  }

  return TRUE;
}

static void
gum_store_cpu_context (GumThreadId thread_id,
                       GumCpuContext * cpu_context,
                       gpointer user_data)
{
  memcpy (user_data, cpu_context, sizeof (GumCpuContext));
}

gchar *
gum_freebsd_query_program_path_for_self (GError ** error)
{
  return gum_query_program_path_for_target (-1, error);
}

gchar *
gum_freebsd_query_program_path_for_pid (pid_t pid,
                                        GError ** error)
{
  return gum_query_program_path_for_target (pid, error);
}

static gchar *
gum_query_program_path_for_target (int target,
                                   GError ** error)
{
  gchar * path;
  size_t size;
  int mib[4];

  size = PATH_MAX;
  path = g_malloc (size);

  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_PATHNAME;
  mib[3] = target;

  if (sysctl (mib, G_N_ELEMENTS (mib), path, &size, NULL, 0) != 0)
    goto failure;

  if (size == 0)
    path[0] = '\0';

  return path;

failure:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_FAILED, "%s", g_strerror (errno));
    g_free (path);
    return NULL;
  }
}

gboolean
_gum_process_collect_main_module (GumModule * module,
                                  gpointer user_data)
{
  GumModule ** out = user_data;

  *out = g_object_ref (module);

  return FALSE;
}

void
_gum_process_enumerate_modules (GumFoundModuleFunc func,
                                gpointer user_data)
{
}

void
_gum_process_enumerate_ranges (GumPageProtection prot,
                               GumFoundRangeFunc func,
                               gpointer user_data)
{
  gum_freebsd_enumerate_ranges (getpid (), prot, func, user_data);
}

void
gum_freebsd_enumerate_ranges (pid_t pid,
                              GumPageProtection prot,
                              GumFoundRangeFunc func,
                              gpointer user_data)
{
  int mib[4];
  gpointer entries = NULL;
  gpointer cursor, end;
  size_t size;

  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_VMMAP;
  mib[3] = pid;

  size = 0;
  if (sysctl (mib, G_N_ELEMENTS (mib), NULL, &size, NULL, 0) != 0)
    goto beach;

  while (TRUE)
  {
    size_t previous_size;
    gboolean still_too_small;

    entries = g_realloc (entries, size);

    previous_size = size;
    if (sysctl (mib, G_N_ELEMENTS (mib), entries, &size, NULL, 0) == 0)
      break;

    still_too_small = errno == ENOMEM && size == previous_size;
    if (!still_too_small)
      goto beach;

    size = size * 4 / 3;
  }

  cursor = entries;
  end = entries + size;

  while (cursor != end)
  {
    struct kinfo_vmentry * e = cursor;
    GumRangeDetails details;
    GumMemoryRange range;
    GumFileMapping file;

    if (e->kve_structsize == 0)
      break;

    range.base_address = e->kve_start;
    range.size = e->kve_end - e->kve_start;

    details.range = &range;
    details.protection = gum_page_protection_from_vmentry (e->kve_protection);
    if (e->kve_type == KVME_TYPE_VNODE)
    {
      file.path = e->kve_path;
      file.offset = e->kve_offset;
      file.size = e->kve_vn_size;

      details.file = &file;
    }
    else
    {
      details.file = NULL;
    }

    if ((details.protection & prot) == prot)
    {
      if (!func (&details, user_data))
        goto beach;
    }

    cursor += e->kve_structsize;
  }

beach:
  g_free (entries);
}

void
gum_process_enumerate_malloc_ranges (GumFoundMallocRangeFunc func,
                                     gpointer user_data)
{
  /* Not implemented */
}

guint
gum_thread_try_get_ranges (GumMemoryRange * ranges,
                           guint max_length)
{
  guint n = 0;
  pthread_attr_t attr;
  void * stack_addr;
  size_t stack_size;
  GumMemoryRange * range;

  pthread_attr_init (&attr);

  if (pthread_attr_get_np (pthread_self (), &attr) != 0)
    goto beach;

  if (pthread_attr_getstack (&attr, &stack_addr, &stack_size) != 0)
    goto beach;

  range = &ranges[0];
  range->base_address = GUM_ADDRESS (stack_addr);
  range->size = stack_size;

  n = 1;

beach:
  pthread_attr_destroy (&attr);

  return n;
}

gint
gum_thread_get_system_error (void)
{
  return errno;
}

void
gum_thread_set_system_error (gint value)
{
  errno = value;
}

gboolean
gum_thread_suspend (GumThreadId thread_id,
                    GError ** error)
{
  if (thr_kill (thread_id, SIGSTOP) != 0)
    goto failure;

  return TRUE;

failure:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_FAILED, "%s", g_strerror (errno));
    return FALSE;
  }
}

gboolean
gum_thread_resume (GumThreadId thread_id,
                   GError ** error)
{
  if (thr_kill (thread_id, SIGCONT) != 0)
    goto failure;

  return TRUE;

failure:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_FAILED, "%s", g_strerror (errno));
    return FALSE;
  }
}

gboolean
gum_thread_set_hardware_breakpoint (GumThreadId thread_id,
                                    guint breakpoint_id,
                                    GumAddress address,
                                    GError ** error)
{
  g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_SUPPORTED,
      "Hardware breakpoints are not yet supported on this platform");
  return FALSE;
}

gboolean
gum_thread_unset_hardware_breakpoint (GumThreadId thread_id,
                                      guint breakpoint_id,
                                      GError ** error)
{
  g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_SUPPORTED,
      "Hardware breakpoints are not yet supported on this platform");
  return FALSE;
}

gboolean
gum_thread_set_hardware_watchpoint (GumThreadId thread_id,
                                    guint watchpoint_id,
                                    GumAddress address,
                                    gsize size,
                                    GumWatchConditions wc,
                                    GError ** error)
{
  g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_SUPPORTED,
      "Hardware watchpoints are not yet supported on this platform");
  return FALSE;
}

gboolean
gum_thread_unset_hardware_watchpoint (GumThreadId thread_id,
                                      guint watchpoint_id,
                                      GError ** error)
{
  g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_SUPPORTED,
      "Hardware watchpoints are not yet supported on this platform");
  return FALSE;
}

static GumThreadState
gum_thread_state_from_proc (const struct kinfo_proc * p)
{
  switch (p->ki_stat)
  {
    case SRUN:
      return GUM_THREAD_RUNNING;
    case SSTOP:
      return GUM_THREAD_STOPPED;
    case SIDL:
    case SSLEEP:
    case SWAIT:
    case SLOCK:
      return GUM_THREAD_WAITING;
    case SZOMB:
      return GUM_THREAD_UNINTERRUPTIBLE;
    default:
      g_assert_not_reached ();
  }
}

static GumPageProtection
gum_page_protection_from_vmentry (int native_prot)
{
  GumPageProtection prot = GUM_PAGE_NO_ACCESS;

  if ((native_prot & KVME_PROT_READ) != 0)
    prot |= GUM_PAGE_READ;
  if ((native_prot & KVME_PROT_WRITE) != 0)
    prot |= GUM_PAGE_WRITE;
  if ((native_prot & KVME_PROT_EXEC) != 0)
    prot |= GUM_PAGE_EXECUTE;

  return prot;
}

void
gum_freebsd_parse_ucontext (const ucontext_t * uc,
                            GumCpuContext * ctx)
{
#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
  const mcontext_t * mc = &uc->uc_mcontext;

  ctx->eip = mc->mc_eip;

  ctx->edi = mc->mc_edi;
  ctx->esi = mc->mc_esi;
  ctx->ebp = mc->mc_ebp;
  ctx->esp = mc->mc_esp;
  ctx->ebx = mc->mc_ebx;
  ctx->edx = mc->mc_edx;
  ctx->ecx = mc->mc_ecx;
  ctx->eax = mc->mc_eax;

  ctx->xmm = NULL;
#elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
  const mcontext_t * mc = &uc->uc_mcontext;

  ctx->rip = mc->mc_rip;

  ctx->r15 = mc->mc_r15;
  ctx->r14 = mc->mc_r14;
  ctx->r13 = mc->mc_r13;
  ctx->r12 = mc->mc_r12;
  ctx->r11 = mc->mc_r11;
  ctx->r10 = mc->mc_r10;
  ctx->r9 = mc->mc_r9;
  ctx->r8 = mc->mc_r8;

  ctx->rdi = mc->mc_rdi;
  ctx->rsi = mc->mc_rsi;
  ctx->rbp = mc->mc_rbp;
  ctx->rsp = mc->mc_rsp;
  ctx->rbx = mc->mc_rbx;
  ctx->rdx = mc->mc_rdx;
  ctx->rcx = mc->mc_rcx;
  ctx->rax = mc->mc_rax;

  ctx->xmm = (GumX86VectorReg *)
      ((const guint8 *) mc->mc_fpstate + GUM_FXSAVE_OFFSET_XMM);
#elif defined (HAVE_ARM64)
  const struct gpregs * gp = &uc->uc_mcontext.mc_gpregs;
  gsize i;

  ctx->pc = gp->gp_elr;
  ctx->sp = gp->gp_sp;
  ctx->nzcv = 0;

  for (i = 0; i != G_N_ELEMENTS (ctx->x); i++)
    ctx->x[i] = gp->gp_x[i];
  ctx->fp = gp->gp_x[29];
  ctx->lr = gp->gp_lr;

  if ((uc->uc_mcontext.mc_flags & _MC_FP_VALID) != 0)
    memcpy (ctx->v, uc->uc_mcontext.mc_fpregs.fp_q, sizeof (ctx->v));
  else
    memset (ctx->v, 0, sizeof (ctx->v));
#else
# error FIXME
#endif
}

void
gum_freebsd_unparse_ucontext (const GumCpuContext * ctx,
                              ucontext_t * uc)
{
#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
  mcontext_t * mc = &uc->uc_mcontext;

  mc->mc_eip = ctx->eip;

  mc->mc_edi = ctx->edi;
  mc->mc_esi = ctx->esi;
  mc->mc_ebp = ctx->ebp;
  mc->mc_esp = ctx->esp;
  mc->mc_ebx = ctx->ebx;
  mc->mc_edx = ctx->edx;
  mc->mc_ecx = ctx->ecx;
  mc->mc_eax = ctx->eax;
#elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
  mcontext_t * mc = &uc->uc_mcontext;

  mc->mc_rip = ctx->rip;

  mc->mc_r15 = ctx->r15;
  mc->mc_r14 = ctx->r14;
  mc->mc_r13 = ctx->r13;
  mc->mc_r12 = ctx->r12;
  mc->mc_r11 = ctx->r11;
  mc->mc_r10 = ctx->r10;
  mc->mc_r9 = ctx->r9;
  mc->mc_r8 = ctx->r8;

  mc->mc_rdi = ctx->rdi;
  mc->mc_rsi = ctx->rsi;
  mc->mc_rbp = ctx->rbp;
  mc->mc_rsp = ctx->rsp;
  mc->mc_rbx = ctx->rbx;
  mc->mc_rdx = ctx->rdx;
  mc->mc_rcx = ctx->rcx;
  mc->mc_rax = ctx->rax;
#elif defined (HAVE_ARM64)
  struct gpregs * gp = &uc->uc_mcontext.mc_gpregs;
  gsize i;

  gp->gp_elr = ctx->pc;
  gp->gp_sp = ctx->sp;

  for (i = 0; i != G_N_ELEMENTS (ctx->x); i++)
    gp->gp_x[i] = ctx->x[i];
  gp->gp_x[29] = ctx->fp;
  gp->gp_lr = ctx->lr;

  uc->uc_mcontext.mc_flags = _MC_FP_VALID;
  memcpy (uc->uc_mcontext.mc_fpregs.fp_q, ctx->v, sizeof (ctx->v));
#else
# error FIXME
#endif
}

void
gum_freebsd_parse_regs (const struct reg * regs,
                        GumCpuContext * ctx)
{
#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
  ctx->eip = regs->r_eip;

  ctx->edi = regs->r_edi;
  ctx->esi = regs->r_esi;
  ctx->ebp = regs->r_ebp;
  ctx->esp = regs->r_esp;
  ctx->ebx = regs->r_ebx;
  ctx->edx = regs->r_edx;
  ctx->ecx = regs->r_ecx;
  ctx->eax = regs->r_eax;

  ctx->xmm = NULL;
#elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
  ctx->rip = regs->r_rip;

  ctx->r15 = regs->r_r15;
  ctx->r14 = regs->r_r14;
  ctx->r13 = regs->r_r13;
  ctx->r12 = regs->r_r12;
  ctx->r11 = regs->r_r11;
  ctx->r10 = regs->r_r10;
  ctx->r9 = regs->r_r9;
  ctx->r8 = regs->r_r8;

  ctx->rdi = regs->r_rdi;
  ctx->rsi = regs->r_rsi;
  ctx->rbp = regs->r_rbp;
  ctx->rsp = regs->r_rsp;
  ctx->rbx = regs->r_rbx;
  ctx->rdx = regs->r_rdx;
  ctx->rcx = regs->r_rcx;
  ctx->rax = regs->r_rax;

  ctx->xmm = NULL;
#elif defined (HAVE_ARM64)
  gsize i;

  ctx->pc = regs->elr;
  ctx->sp = regs->sp;

  for (i = 0; i != G_N_ELEMENTS (ctx->x); i++)
    ctx->x[i] = regs->x[i];
  ctx->fp = regs->x[29];
  ctx->lr = regs->lr;
#else
# error FIXME
#endif
}

void
gum_freebsd_unparse_regs (const GumCpuContext * ctx,
                          struct reg * regs)
{
#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
  regs->r_eip = ctx->eip;

  regs->r_edi = ctx->edi;
  regs->r_esi = ctx->esi;
  regs->r_ebp = ctx->ebp;
  regs->r_esp = ctx->esp;
  regs->r_ebx = ctx->ebx;
  regs->r_edx = ctx->edx;
  regs->r_ecx = ctx->ecx;
  regs->r_eax = ctx->eax;
#elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
  regs->r_rip = ctx->rip;

  regs->r_r15 = ctx->r15;
  regs->r_r14 = ctx->r14;
  regs->r_r13 = ctx->r13;
  regs->r_r12 = ctx->r12;
  regs->r_r11 = ctx->r11;
  regs->r_r10 = ctx->r10;
  regs->r_r9 = ctx->r9;
  regs->r_r8 = ctx->r8;

  regs->r_rdi = ctx->rdi;
  regs->r_rsi = ctx->rsi;
  regs->r_rbp = ctx->rbp;
  regs->r_rsp = ctx->rsp;
  regs->r_rbx = ctx->rbx;
  regs->r_rdx = ctx->rdx;
  regs->r_rcx = ctx->rcx;
  regs->r_rax = ctx->rax;
#elif defined (HAVE_ARM64)
  gsize i;

  regs->elr = ctx->pc;
  regs->sp = ctx->sp;

  for (i = 0; i != G_N_ELEMENTS (ctx->x); i++)
    regs->x[i] = ctx->x[i];
  regs->x[29] = ctx->fp;
  regs->lr = ctx->lr;
#else
# error FIXME
#endif
}
