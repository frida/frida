/*
 * Copyright (C) 2010-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2023-2024 Håvard Sørbø <havard@hsorbo.no>
 * Copyright (C) 2023 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2023 Grant Douglas <me@hexplo.it>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumprocess-priv.h"

#include "gum-init.h"
#include "gumcontrolflowgraph.h"
#include "gummodule-elf.h"
#include "gum/gumandroid.h"
#include "gum/gumlinux.h"
#include "gumlinux-priv.h"
#include "gummodulemap.h"
#include "valgrind.h"
#if defined (HAVE_I386)
# include "gumx86relocator.h"
#endif

#include <capstone.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
typedef guint32 u32;
#include <linux/futex.h>
#ifdef HAVE_PTHREAD_ATTR_GETSTACK
# include <pthread.h>
#endif
#ifdef HAVE_LINK_H
# include <link.h>
#endif
#ifdef HAVE_ASM_PRCTL_H
# include <asm/prctl.h>
#endif
#include <sys/prctl.h>
#include <sys/ptrace.h>
#ifdef HAVE_ASM_PTRACE_H
# include <asm/ptrace.h>
#endif
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#ifdef HAVE_SYS_USER_H
# include <sys/user.h>
#endif

#ifdef HAVE_GLIBC
# include <gnu/libc-version.h>
#endif

#ifndef FUTEX_WAIT_PRIVATE
# define FUTEX_PRIVATE_FLAG 128
# define FUTEX_WAIT_PRIVATE (FUTEX_WAIT | FUTEX_PRIVATE_FLAG)
# define FUTEX_WAKE_PRIVATE (FUTEX_WAKE | FUTEX_PRIVATE_FLAG)
#endif

#define GUM_PSR_THUMB 0x20

#if defined (HAVE_I386)
typedef struct user_regs_struct GumGPRegs;
typedef struct _GumX86DebugRegs GumDebugRegs;
#elif defined (HAVE_ARM)
typedef struct pt_regs GumGPRegs;
typedef struct _GumArmDebugRegs GumDebugRegs;
#elif defined (HAVE_ARM64)
typedef struct user_pt_regs GumGPRegs;
typedef struct _GumArm64DebugRegs GumDebugRegs;
#elif defined (HAVE_MIPS)
typedef struct pt_regs GumGPRegs;
typedef struct _GumMipsDebugRegs GumDebugRegs;
#else
# error Unsupported architecture
#endif

#ifdef HAVE_GLIBC
# define GUM_MAX_LIST_WALK_ATTEMPTS        10
# define GUM_LINUX_MAX_THREADS             (256 * 1024)
# define GUM_MAX_FIND_LIST_HEAD_ATTEMPTS   200
# define GUM_MAX_FIND_LIST_ANCHOR_ATTEMPTS 200
# define GUM_MAX_PTHREAD_SIZE              2048
# define GUM_TID_CHECK_TIMES               5
# define GUM_MAX_INSTRUCTION_SIZE          15
# define GUM_THREAD_STACK_SIZE             0x20000
#endif

typedef guint GumMipsWatchStyle;
typedef struct _GumMips32WatchRegs GumMips32WatchRegs;
typedef struct _GumMips64WatchRegs GumMips64WatchRegs;
typedef union _GumRegs GumRegs;
#ifndef PTRACE_GETREGS
# define PTRACE_GETREGS 12
#endif
#ifndef PTRACE_SETREGS
# define PTRACE_SETREGS 13
#endif
#ifndef PTRACE_GETHBPREGS
# define PTRACE_GETHBPREGS 29
#endif
#ifndef PTRACE_SETHBPREGS
# define PTRACE_SETHBPREGS 30
#endif
#ifndef PTRACE_GET_WATCH_REGS
# define PTRACE_GET_WATCH_REGS 0xd0
#endif
#ifndef PTRACE_SET_WATCH_REGS
# define PTRACE_SET_WATCH_REGS 0xd1
#endif
#ifndef PTRACE_GETREGSET
# define PTRACE_GETREGSET 0x4204
#endif
#ifndef PTRACE_SETREGSET
# define PTRACE_SETREGSET 0x4205
#endif
#ifndef PR_SET_PTRACER
# define PR_SET_PTRACER 0x59616d61
#endif
#ifndef NT_PRSTATUS
# define NT_PRSTATUS 1
#endif

#define GUM_NSIG 65

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
typedef void (* GumLinuxModifyThreadFunc) (GumThreadId thread_id,
    GumRegs * regs, gpointer user_data);
typedef struct _GumLinuxModifyThreadContext GumLinuxModifyThreadContext;
typedef guint GumLinuxRegsType;
typedef guint8 GumModifyThreadAck;

typedef struct _GumSetHardwareBreakpointContext GumSetHardwareBreakpointContext;
typedef struct _GumSetHardwareWatchpointContext GumSetHardwareWatchpointContext;

typedef struct _GumUserDesc GumUserDesc;
typedef struct _GumTcbHead GumTcbHead;

typedef gint (* GumCloneFunc) (gpointer arg);

#if defined (HAVE_GLIBC)
typedef struct _GumLinuxThreadCtx GumLinuxThreadCtx;
typedef struct _GumTestByAddressContext GumTestByAddressContext;
typedef struct _GumLinuxGlobalsFragment GumLinuxGlobalsFragment;
#elif defined (HAVE_MUSL)
typedef struct _GumMuslStartArgs GumMuslStartArgs;
#endif

struct _GumModifyThreadContext
{
  GumModifyThreadFunc func;
  gpointer user_data;
};

enum _GumLinuxRegsType
{
  GUM_REGS_GENERAL_PURPOSE,
  GUM_REGS_DEBUG_BREAK,
  GUM_REGS_DEBUG_WATCH,
};

struct _GumX86DebugRegs
{
  gsize dr0;
  gsize dr1;
  gsize dr2;
  gsize dr3;
  gsize dr6;
  gsize dr7;
};

struct _GumArmDebugRegs
{
  guint32 cr[16];
  guint32 vr[16];
};

struct _GumArm64DebugRegs
{
  guint64 cr[16];
  guint64 vr[16];
};

enum _GumMipsWatchStyle
{
  GUM_MIPS_WATCH_MIPS32,
  GUM_MIPS_WATCH_MIPS64,
};

struct _GumMips32WatchRegs
{
  guint32 watch_lo[8];
  guint16 watch_hi[8];
  guint16 watch_masks[8];
  guint32 num_valid;
} __attribute__ ((aligned (8)));

struct _GumMips64WatchRegs
{
  guint64 watch_lo[8];
  guint16 watch_hi[8];
  guint16 watch_masks[8];
  guint32 num_valid;
} __attribute__ ((aligned (8)));

struct _GumMipsDebugRegs
{
  GumMipsWatchStyle style;
  union
  {
    GumMips32WatchRegs mips32;
    GumMips64WatchRegs mips64;
  };
};

union _GumRegs
{
  GumGPRegs gp;
  GumDebugRegs debug;
};

enum _GumModifyThreadAck
{
  GUM_ACK_INVALID,
  GUM_ACK_READY,
  GUM_ACK_READ_REGISTERS,
  GUM_ACK_MODIFIED_REGISTERS,
  GUM_ACK_WROTE_REGISTERS,
  GUM_ACK_FAILED_TO_ATTACH,
  GUM_ACK_FAILED_TO_WAIT,
  GUM_ACK_FAILED_TO_STOP,
  GUM_ACK_FAILED_TO_READ,
  GUM_ACK_FAILED_TO_WRITE,
  GUM_ACK_FAILED_TO_DETACH
};

struct _GumLinuxModifyThreadContext
{
  GumThreadId thread_id;
  GumLinuxRegsType regs_type;
  GumLinuxModifyThreadFunc func;
  gpointer user_data;

  gint fd[2];
  GumRegs regs_data;
};

struct _GumEmitExecutableModuleContext
{
  const gchar * executable_path;
  GumFoundModuleFunc func;
  gpointer user_data;

  gboolean carry_on;
};

struct _GumSetHardwareBreakpointContext
{
  guint breakpoint_id;
  GumAddress address;
};

struct _GumSetHardwareWatchpointContext
{
  guint watchpoint_id;
  GumAddress address;
  gsize size;
  GumWatchConditions conditions;
};

struct _GumUserDesc
{
  guint entry_number;
  guint base_addr;
  guint limit;
  guint seg_32bit : 1;
  guint contents : 2;
  guint read_exec_only : 1;
  guint limit_in_pages : 1;
  guint seg_not_present : 1;
  guint useable : 1;
};

struct _GumTcbHead
{
#ifdef HAVE_I386
  gpointer tcb;
  gpointer dtv;
  gpointer self;
#else
  gpointer dtv;
  gpointer priv;
#endif
};

#if defined (HAVE_GLIBC)

struct _GumLinuxThreadCtx
{
  pthread_t thread;
  void * stack;

  GMutex mutex;
  GCond cond;
  gboolean start;
  gboolean exit;

  GumThreadId tid;
  gpointer ret;
};

struct _GumTestByAddressContext
{
  GumAddress address;
  gboolean found;
  gboolean is_pthread_globals;
  gchar * last_file;
};

struct _GumLinuxGlobalsFragment
{
  GumGlibcList _dl_stack_used;
  GumGlibcList _dl_stack_user;
  GumGlibcList _dl_stack_cache;
  size_t _dl_stack_cache_actsize;
  uintptr_t _dl_in_flight_stack;
  int _dl_stack_cache_lock;
};

#elif defined (HAVE_MUSL)

struct _GumMuslStartArgs
{
  gpointer start_func;
  gpointer start_arg;
  volatile int control;
  gulong sig_mask[GUM_NSIG / 8 / sizeof (long)];
};

#elif defined (HAVE_ANDROID)

typedef struct _GumFindFunctionByPrefixContext GumFindFunctionByPrefixContext;

struct _GumFindFunctionByPrefixContext
{
  const gchar * prefix;
  GumAddress address;
};

#endif

static gboolean gum_try_resolve_dynamic_symbol (const gchar * name,
    Dl_info * info);

static void gum_collect_thread_entrypoint (GumThreadDetails * entry,
    pthread_t thread, const GumLinuxPThreadSpec * spec, GumThreadFlags flags);
static gboolean gum_fill_thread_details (GumThreadDetails * entry,
    GumThreadFlags flags);

static void gum_do_modify_thread (GumThreadId thread_id, GumRegs * regs,
    gpointer user_data);
static gboolean gum_linux_modify_thread (GumThreadId thread_id,
    GumLinuxRegsType regs_type, GumLinuxModifyThreadFunc func,
    gpointer user_data, GError ** error);
static gpointer gum_linux_handle_modify_thread_comms (gpointer data);
static gint gum_linux_do_modify_thread (gpointer data);
static gboolean gum_await_ack (gint fd, GumModifyThreadAck expected_ack,
    GumModifyThreadAck * received_ack);
static void gum_put_ack (gint fd, GumModifyThreadAck ack);

static GumModule * gum_try_init_libc_module (void);
static void gum_deinit_libc_module (void);
static const Dl_info * gum_try_init_libc_info (void);

static void gum_linux_named_range_free (GumLinuxNamedRange * range);
#ifdef HAVE_GLIBC
static gboolean gum_linux_get_threads_from_list (
    const GumLinuxPThreadSpec * spec, const GumGlibcList * anchor,
    GList ** threads);
#endif
static GumThreadState gum_thread_state_from_proc_status_character (gchar c);
static void gum_store_cpu_context (GumThreadId thread_id,
    GumCpuContext * cpu_context, gpointer user_data);
static void gum_do_set_hardware_breakpoint (GumThreadId thread_id,
    GumRegs * regs, gpointer user_data);
static void gum_do_unset_hardware_breakpoint (GumThreadId thread_id,
    GumRegs * regs, gpointer user_data);
static void gum_do_set_hardware_watchpoint (GumThreadId thread_id,
    GumRegs * regs, gpointer user_data);
static void gum_do_unset_hardware_watchpoint (GumThreadId thread_id,
    GumRegs * regs, gpointer user_data);

static void gum_proc_maps_iter_init_for_path (GumProcMapsIter * iter,
    const gchar * path);

static GumPageProtection gum_page_protection_from_proc_perms_string (
    const gchar * perms);

static gssize gum_get_regs (pid_t pid, guint type, gpointer data, gsize * size);
static gssize gum_set_regs (pid_t pid, guint type, gconstpointer data,
    gsize size);

static void gum_parse_gp_regs (const GumGPRegs * regs, GumCpuContext * ctx);
static void gum_unparse_gp_regs (const GumCpuContext * ctx, GumGPRegs * regs);

static gboolean gum_detect_pthread_internals (GumLinuxPThreadSpec * spec);
#if defined (HAVE_GLIBC)
static gboolean gum_linux_find_list_head (GumLinuxPThreadSpec * spec);
static gboolean gum_linux_find_list_head_offset (GumLinuxPThreadSpec * spec,
    GumLinuxThreadCtx * first, GumLinuxThreadCtx * second);
static gboolean gum_linux_find_start_offsets (GumLinuxPThreadSpec * spec);
static gboolean gum_linux_find_tid_offset (GumLinuxPThreadSpec * spec);
static gboolean gum_linux_check_thread_offset (gsize offset, gboolean * match);
static gboolean gum_linux_find_list_anchor (GumLinuxPThreadSpec * spec,
    gboolean custom_stack);
static void gum_test_by_address_context_init (GumTestByAddressContext * ctx,
    GumAddress address);
static void gum_test_by_address_context_free (GumTestByAddressContext * ctx);
static gboolean gum_test_pthread_globals_if_containing_address (
    const GumRangeDetails * details, GumTestByAddressContext * fc);
static gboolean gum_linux_find_lock (GumLinuxPThreadSpec * spec);
static gboolean gum_linux_get_libc_version (guint * major, guint * minor);
static gboolean gum_linux_find_start_impl (GumLinuxPThreadSpec * spec);
#if defined (HAVE_I386)
static gpointer gum_linux_find_dominating_start_hook (gpointer call_site);
static gboolean gum_linux_try_use_dominating_site (gconstpointer site,
    gsize capacity, gpointer user_data);
#endif
static gboolean gum_linux_is_call (cs_insn * insn);
static gboolean gum_linux_create_thread (GumLinuxThreadCtx * ctx,
    gboolean custom_stack);
static gboolean gum_linux_dispose_thread (GumLinuxThreadCtx * ctx);
static gpointer gum_linux_thread_proc (gpointer param);
static gboolean gum_linux_thread_read_flink (const GumLinuxPThreadSpec * spec,
    pthread_t current, pthread_t * next);
static gboolean gum_linux_thread_read_blink (const GumLinuxPThreadSpec * spec,
    pthread_t current, pthread_t * prev);
static void glibc_lock_acquire (GumGlibcLock * lock);
static void glibc_lock_release (GumGlibcLock * lock);
#endif
#ifdef HAVE_MUSL
# ifdef HAVE_ARM
static gpointer gum_parse_ldrpc (const uint8_t * code, csh capstone,
    cs_insn * insn);
# endif
static GumMuslStartArgs * gum_query_pthread_start_args (pthread_t thread,
    const GumLinuxPThreadSpec * spec);
#endif
#ifdef HAVE_ANDROID
static GumAddress gum_find_function_symbol_by_prefix (GumModule * module,
    const gchar * prefix);
static gboolean gum_store_first_function_symbol_with_prefix (
    const GumSymbolDetails * details, gpointer user_data);
#endif

static gssize gum_libc_clone (GumCloneFunc child_func, gpointer child_stack,
    gint flags, gpointer arg, pid_t * parent_tidptr, GumUserDesc * tls,
    pid_t * child_tidptr);
static gssize gum_libc_read (gint fd, gpointer buf, gsize count);
static gssize gum_libc_write (gint fd, gconstpointer buf, gsize count);
static pid_t gum_libc_waitpid (pid_t pid, int * status, int options);
static gssize gum_libc_ptrace (gsize request, pid_t pid, gpointer address,
    gpointer data);

#define gum_libc_syscall_3(n, a, b, c) gum_libc_syscall_4 (n, a, b, c, 0)
static gssize gum_libc_syscall_4 (gsize n, gsize a, gsize b, gsize c, gsize d);

static GumModule * gum_libc_module;
static Dl_info gum_libc_info;

static gboolean gum_is_regset_supported = TRUE;

G_LOCK_DEFINE_STATIC (gum_dumpable);
static gint gum_dumpable_refcount = 0;
static gint gum_dumpable_previous = 0;

static gboolean
gum_try_resolve_dynamic_symbol (const gchar * name,
                                Dl_info * info)
{
  gpointer address;

  address = dlsym (RTLD_NEXT, name);
  if (address == NULL)
    address = dlsym (RTLD_DEFAULT, name);
  if (address == NULL)
    return FALSE;

  return dladdr (address, info) != 0;
}

gboolean
gum_process_is_debugger_attached (void)
{
  gboolean result;
  gchar * status, * p;

  status = NULL;
  if (!g_file_get_contents ("/proc/self/status", &status, NULL, NULL))
    return FALSE;

  p = strstr (status, "TracerPid:");
  g_assert (p != NULL);

  result = atoi (p + 10) != 0;

  g_free (status);

  return result;
}

GumProcessId
gum_process_get_id (void)
{
  return getpid ();
}

GumThreadId
gum_process_get_current_thread_id (void)
{
  return syscall (__NR_gettid);
}

gboolean
gum_process_has_thread (GumThreadId thread_id)
{
  gchar path[16 + 20 + 1];
  sprintf (path, "/proc/self/task/%" G_GSIZE_MODIFIER "u", thread_id);

  return g_file_test (path, G_FILE_TEST_EXISTS);
}

GumThreadDetails *
gum_process_find_thread_by_id (GumThreadId thread_id,
                               GumThreadFlags flags)
{
  GumThreadDetails * details = NULL;
  const GumLinuxPThreadSpec * spec;
  GumLinuxPThreadIter iter;
  pthread_t thread;

  spec = gum_linux_query_pthread_spec ();

  gum_linux_lock_pthread_list (spec);

  gum_linux_pthread_iter_init (&iter, spec);
  while (gum_linux_pthread_iter_next (&iter, &thread))
  {
    if (gum_linux_query_pthread_tid (thread, spec) == thread_id)
    {
      details = g_slice_new0 (GumThreadDetails);
      details->id = thread_id;
      gum_collect_thread_entrypoint (details, thread, spec, flags);
      break;
    }
  }

  gum_linux_unlock_pthread_list (spec);

  if (details == NULL)
    return NULL;

  if (!gum_fill_thread_details (details, flags))
    goto fill_failed;

  return details;

fill_failed:
  {
    gum_thread_details_free (details);
    return NULL;
  }
}

gboolean
gum_process_modify_thread (GumThreadId thread_id,
                           GumModifyThreadFunc func,
                           gpointer user_data,
                           GumModifyThreadFlags flags)
{
  GumModifyThreadContext ctx;

  ctx.func = func;
  ctx.user_data = user_data;

  return gum_linux_modify_thread (thread_id, GUM_REGS_GENERAL_PURPOSE,
      gum_do_modify_thread, &ctx, NULL);
}

static void
gum_do_modify_thread (GumThreadId thread_id,
                      GumRegs * regs,
                      gpointer user_data)
{
  GumGPRegs * gpr = &regs->gp;
  GumModifyThreadContext * ctx = user_data;
  GumCpuContext cpu_context;

  gum_parse_gp_regs (gpr, &cpu_context);

  ctx->func (thread_id, &cpu_context, ctx->user_data);

  gum_unparse_gp_regs (&cpu_context, gpr);
}

static gboolean
gum_linux_modify_thread (GumThreadId thread_id,
                         GumLinuxRegsType regs_type,
                         GumLinuxModifyThreadFunc func,
                         gpointer user_data,
                         GError ** error)
{
  gboolean success = FALSE;
  GumLinuxModifyThreadContext ctx;
  gssize child;
  gsize page_size = gum_query_page_size ();
  gpointer stack = NULL;
  gpointer tls = NULL;
  GumUserDesc * desc;
  guint32 result;
  GumModifyThreadAck ack;

  ctx.thread_id = thread_id;
  ctx.regs_type = regs_type;
  ctx.func = func;
  ctx.user_data = user_data;

  ctx.fd[0] = -1;
  ctx.fd[1] = -1;

  memset (&ctx.regs_data, 0, sizeof (ctx.regs_data));

  if (socketpair (AF_UNIX, SOCK_STREAM, 0, ctx.fd) != 0)
    goto socketpair_failed;

  stack = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);
  tls = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);

#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
  GumUserDesc segment;
  gint gs;

  asm volatile (
      "movw %%gs, %w0"
      : "=q" (gs)
  );

  segment.entry_number = (gs & 0xffff) >> 3;
  segment.base_addr = GPOINTER_TO_SIZE (tls);
  segment.limit = 0xfffff;
  segment.seg_32bit = 1;
  segment.contents = 0;
  segment.read_exec_only = 0;
  segment.limit_in_pages = 1;
  segment.seg_not_present = 0;
  segment.useable = 1;

  desc = &segment;
#else
  desc = tls;
#endif

#if defined (HAVE_I386)
  {
    GumTcbHead * head = tls;

    head->tcb = tls;
    head->dtv = GSIZE_TO_POINTER (GPOINTER_TO_SIZE (tls) + 1024);
    head->self = tls;
  }
#endif

  /*
   * It seems like the only reliable way to read/write the registers of
   * another thread is to use ptrace(). We used to accomplish this by
   * hi-jacking the target thread by installing a signal handler and sending a
   * real-time signal directed at the target thread, and thus relying on the
   * signal handler getting called in that thread. The signal handler would
   * then provide us with read/write access to its registers. This hack would
   * however not work if a thread was for example blocking in poll(), as the
   * signal would then just get queued and we'd end up waiting indefinitely.
   *
   * It is however not possible to ptrace() another thread when we're in the
   * same process group. This used to be supported in old kernels, but it was
   * buggy and eventually dropped. So in order to use ptrace() we will need to
   * spawn a new thread in a different process group so that it can ptrace()
   * the target thread inside our process group. This is also the solution
   * recommended by Linus:
   *
   * https://lkml.org/lkml/2006/9/1/217
   *
   * Because libc implementations don't expose an API to do this, and the
   * thread setup code is private, where the TLS part is crucial for even just
   * the syscall wrappers - due to them accessing `errno` - we cannot make any
   * libc calls in this thread. And because the libc's clone() syscall wrapper
   * typically writes to the child thread's TLS structures, which we cannot
   * portably set up correctly, we cannot use the libc clone() syscall wrapper
   * either.
   */
  child = gum_libc_clone (
      gum_linux_do_modify_thread,
      stack + page_size,
      CLONE_VM | CLONE_SETTLS,
      &ctx,
      NULL,
      desc,
      NULL);
  if (child == -1)
    goto clone_failed;

  _gum_acquire_dumpability ();

  prctl (PR_SET_PTRACER, child);

  if (thread_id == gum_process_get_current_thread_id ())
  {
    result = GPOINTER_TO_UINT (g_thread_join (g_thread_new (
            "gum-modify-thread-worker",
            gum_linux_handle_modify_thread_comms,
            &ctx)));
  }
  else
  {
    result = GPOINTER_TO_UINT (gum_linux_handle_modify_thread_comms (&ctx));
  }
  success = (result & 0xffff) != 0;
  ack = (GumModifyThreadAck) (result >> 16);

  _gum_release_dumpability ();

  waitpid (child, NULL, __WCLONE);

  if (!success)
    goto attach_failed;

  goto beach;

socketpair_failed:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_PERMISSION_DENIED,
        "Unable to create socketpair");
    goto beach;
  }
clone_failed:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_PERMISSION_DENIED,
        "Unable to set up clone");
    goto beach;
  }
attach_failed:
  {
    const gchar * detail;

    switch (ack)
    {
      case GUM_ACK_INVALID:
        detail = "read from socket";
        break;
      case GUM_ACK_FAILED_TO_ATTACH:
        detail = "attach to thread";
        break;
      case GUM_ACK_FAILED_TO_WAIT:
        detail = "wait for thread";
        break;
      case GUM_ACK_FAILED_TO_STOP:
        detail = "verify thread is stopped";
        break;
      case GUM_ACK_FAILED_TO_READ:
        detail = "read registers";
        break;
      case GUM_ACK_FAILED_TO_WRITE:
        detail = "write registers";
        break;
      case GUM_ACK_FAILED_TO_DETACH:
        detail = "detach from thread";
        break;
      default:
        detail = "communicate with helper thread";
        break;
    }

    g_set_error (error, GUM_ERROR, GUM_ERROR_PERMISSION_DENIED,
        "Unable to %s", detail);
    goto beach;
  }
beach:
  {
    if (tls != NULL)
      gum_memory_free (tls, page_size);
    if (stack != NULL)
      gum_memory_free (stack, page_size);

    if (ctx.fd[0] != -1)
      close (ctx.fd[0]);
    if (ctx.fd[1] != -1)
      close (ctx.fd[1]);

    return success;
  }
}

static gpointer
gum_linux_handle_modify_thread_comms (gpointer data)
{
  GumLinuxModifyThreadContext * ctx = data;
  gint fd = ctx->fd[0];
  gboolean success = FALSE;
  GumModifyThreadAck received_ack;
  guint32 result;

  gum_put_ack (fd, GUM_ACK_READY);

  if (gum_await_ack (fd, GUM_ACK_READ_REGISTERS, &received_ack))
  {
    ctx->func (ctx->thread_id, &ctx->regs_data, ctx->user_data);
    gum_put_ack (fd, GUM_ACK_MODIFIED_REGISTERS);

    success = gum_await_ack (fd, GUM_ACK_WROTE_REGISTERS, &received_ack);
  }

  result = (received_ack << 16) | (success ? 1 : 0);

  return GUINT_TO_POINTER (result);
}

static gint
gum_linux_do_modify_thread (gpointer data)
{
  GumLinuxModifyThreadContext * ctx = data;
  gint fd;
  gboolean attached = FALSE;
  gssize res;
  pid_t wait_result;
  int status;
#if defined (HAVE_I386)
  const guint x86_debugreg_offsets[] = { 0, 1, 2, 3, 6, 7 };
#elif defined (HAVE_ARM)
  guint debug_regs_count = 0;
#elif defined (HAVE_ARM64)
  struct user_hwdebug_state debug_regs;
  const guint debug_regs_type = (ctx->regs_type == GUM_REGS_DEBUG_BREAK)
      ? NT_ARM_HW_BREAK
      : NT_ARM_HW_WATCH;
  gsize debug_regs_size = sizeof (struct user_hwdebug_state);
  guint debug_regs_count = 0;
#endif
#ifndef HAVE_MIPS
  guint i;
#endif

  fd = ctx->fd[1];

  gum_await_ack (fd, GUM_ACK_READY, NULL);

  res = gum_libc_ptrace (PTRACE_ATTACH, ctx->thread_id, NULL, NULL);
  if (res == -1)
    goto failed_to_attach;
  attached = TRUE;

  wait_result = gum_libc_waitpid (ctx->thread_id, &status, __WALL);

  if (wait_result != ctx->thread_id)
    goto failed_to_wait;

  if (!WIFSTOPPED (status))
    goto failed_to_stop;

  /*
   * Although ptrace injects SIGSTOP into our process, it is possible that our
   * target is stopped by another stop signal (e.g. SIGTTIN). The man pages for
   * ptrace mention the possible race condition. For our purposes, however, we
   * only require that the target is stopped so that we can read its registers.
   */
  if (ctx->regs_type == GUM_REGS_GENERAL_PURPOSE)
  {
    gsize regs_size = sizeof (GumGPRegs);

    res = gum_get_regs (ctx->thread_id, NT_PRSTATUS, &ctx->regs_data,
        &regs_size);
    if (res == -1)
      goto failed_to_read;
  }
  else
  {
#if defined (HAVE_I386)
    for (i = 0; i != G_N_ELEMENTS (x86_debugreg_offsets); i++)
    {
      const guint offset = x86_debugreg_offsets[i];

      res = gum_libc_ptrace (PTRACE_PEEKUSER, ctx->thread_id,
          GSIZE_TO_POINTER (
            G_STRUCT_OFFSET (struct user, u_debugreg) +
            (offset * sizeof (gpointer))),
          &ctx->regs_data.debug.dr0 + i);
      if (res == -1)
        goto failed_to_read;
    }
#elif defined (HAVE_ARM)
    guint32 info;
    res = gum_libc_ptrace (PTRACE_GETHBPREGS, ctx->thread_id, 0, &info);
    if (res == -1)
      goto failed_to_read;

    debug_regs_count = (ctx->regs_type == GUM_REGS_DEBUG_BREAK)
        ? info & 0xff
        : (info >> 8) & 0xff;

    long step = (ctx->regs_type == GUM_REGS_DEBUG_WATCH) ? -1 : 1;
    long num = step;
    for (i = 0; i != debug_regs_count; i++)
    {
      res = gum_libc_ptrace (PTRACE_GETHBPREGS, ctx->thread_id,
          GSIZE_TO_POINTER (num), &ctx->regs_data.debug.vr[i]);
      if (res == -1)
        goto failed_to_read;
      num += step;

      res = gum_libc_ptrace (PTRACE_GETHBPREGS, ctx->thread_id,
          GSIZE_TO_POINTER (num), &ctx->regs_data.debug.cr[i]);
      if (res == -1)
        goto failed_to_read;
      num += step;
    }
#elif defined (HAVE_ARM64)
    res = gum_get_regs (ctx->thread_id, debug_regs_type, &debug_regs,
        &debug_regs_size);
    if (res == -1)
      goto failed_to_read;

    debug_regs_count = debug_regs.dbg_info & 0xff;

    for (i = 0; i != G_N_ELEMENTS (debug_regs.dbg_regs); i++)
    {
      ctx->regs_data.debug.cr[i] = debug_regs.dbg_regs[i].ctrl;
      ctx->regs_data.debug.vr[i] = debug_regs.dbg_regs[i].addr;
    }
#elif defined (HAVE_MIPS)
    res = gum_libc_ptrace (PTRACE_GET_WATCH_REGS, ctx->thread_id,
        &ctx->regs_data.debug, NULL);
    if (res == -1)
      goto failed_to_read;
#endif
  }
  gum_put_ack (fd, GUM_ACK_READ_REGISTERS);

  gum_await_ack (fd, GUM_ACK_MODIFIED_REGISTERS, NULL);
  if (ctx->regs_type == GUM_REGS_GENERAL_PURPOSE)
  {
    res = gum_set_regs (ctx->thread_id, NT_PRSTATUS, &ctx->regs_data,
        sizeof (GumGPRegs));
    if (res == -1)
      goto failed_to_write;
  }
  else
  {
#if defined (HAVE_I386)
    for (i = 0; i != G_N_ELEMENTS (x86_debugreg_offsets); i++)
    {
      const guint offset = x86_debugreg_offsets[i];
      res = gum_libc_ptrace (PTRACE_POKEUSER, ctx->thread_id,
          GSIZE_TO_POINTER (
            G_STRUCT_OFFSET (struct user, u_debugreg) +
            (offset * sizeof (gpointer))),
          GSIZE_TO_POINTER ((&ctx->regs_data.debug.dr0)[i]));
      if (res == -1)
        goto failed_to_write;
    }
#elif defined (HAVE_ARM)
    long step = (ctx->regs_type == GUM_REGS_DEBUG_WATCH) ? -1 : 1;
    long num = step;
    for (i = 0; i != debug_regs_count; i++)
    {
      res = gum_libc_ptrace (PTRACE_SETHBPREGS, ctx->thread_id,
          GSIZE_TO_POINTER (num), &ctx->regs_data.debug.vr[i]);
      if (res == -1)
        goto failed_to_write;
      num += step;

      res = gum_libc_ptrace (PTRACE_SETHBPREGS, ctx->thread_id,
          GSIZE_TO_POINTER (num), &ctx->regs_data.debug.cr[i]);
      if (res == -1)
        goto failed_to_write;
      num += step;
    }
#elif defined (HAVE_ARM64)
    for (i = 0; i != debug_regs_count; i++)
    {
      debug_regs.dbg_regs[i].ctrl = ctx->regs_data.debug.cr[i];
      debug_regs.dbg_regs[i].addr = ctx->regs_data.debug.vr[i];
    }

    res = gum_set_regs (ctx->thread_id, debug_regs_type, &debug_regs,
        G_STRUCT_OFFSET (struct user_hwdebug_state, dbg_regs) +
        debug_regs_count * 16);
    if (res == -1)
      goto failed_to_write;
#elif defined (HAVE_MIPS)
    res = gum_libc_ptrace (PTRACE_SET_WATCH_REGS, ctx->thread_id,
        &ctx->regs_data.debug, NULL);
    if (res == -1)
      goto failed_to_write;
#endif
  }

  res = gum_libc_ptrace (PTRACE_DETACH, ctx->thread_id, NULL,
      GINT_TO_POINTER (SIGCONT));

  attached = FALSE;
  if (res == -1)
    goto failed_to_detach;

  gum_put_ack (fd, GUM_ACK_WROTE_REGISTERS);

  goto beach;

failed_to_attach:
  {
    gum_put_ack (fd, GUM_ACK_FAILED_TO_ATTACH);
    goto beach;
  }
failed_to_wait:
  {
    gum_put_ack (fd, GUM_ACK_FAILED_TO_WAIT);
    goto beach;
  }
failed_to_stop:
  {
    gum_put_ack (fd, GUM_ACK_FAILED_TO_STOP);
    goto beach;
  }
failed_to_read:
  {
    gum_put_ack (fd, GUM_ACK_FAILED_TO_READ);
    goto beach;
  }
failed_to_write:
  {
    gum_put_ack (fd, GUM_ACK_FAILED_TO_WRITE);
    goto beach;
  }
failed_to_detach:
  {
    gum_put_ack (fd, GUM_ACK_FAILED_TO_DETACH);
    goto beach;
  }
beach:
  {
    if (attached)
    {
      gum_libc_ptrace (PTRACE_DETACH, ctx->thread_id, NULL,
          GINT_TO_POINTER (SIGCONT));
    }

    return 0;
  }
}

static gboolean
gum_await_ack (gint fd,
               GumModifyThreadAck expected_ack,
               GumModifyThreadAck * received_ack)
{
  guint8 value;
  gssize res;

  res = GUM_TEMP_FAILURE_RETRY (gum_libc_read (fd, &value, sizeof (value)));
  if (res == -1)
  {
    if (received_ack != NULL)
      *received_ack = GUM_ACK_INVALID;
    return FALSE;
  }

  if (received_ack != NULL)
    *received_ack = (GumModifyThreadAck) value;

  return value == expected_ack;
}

static void
gum_put_ack (gint fd,
             GumModifyThreadAck ack)
{
  guint8 value;

  value = ack;
  GUM_TEMP_FAILURE_RETRY (gum_libc_write (fd, &value, sizeof (value)));
}

void
_gum_process_enumerate_threads (GumFoundThreadFunc func,
                                gpointer user_data,
                                GumThreadFlags flags)
{
  GArray * entries;
  const GumLinuxPThreadSpec * spec;
  GumLinuxPThreadIter iter;
  pthread_t thread;
  guint i;

  entries = g_array_new (FALSE, FALSE, sizeof (GumThreadDetails));

  spec = gum_linux_query_pthread_spec ();

  gum_linux_lock_pthread_list (spec);

  gum_linux_pthread_iter_init (&iter, spec);
  while (gum_linux_pthread_iter_next (&iter, &thread))
  {
    GumThreadDetails entry = { 0, };

    entry.id = gum_linux_query_pthread_tid (thread, spec);
    gum_collect_thread_entrypoint (&entry, thread, spec, flags);

    g_array_append_val (entries, entry);
  }

  gum_linux_unlock_pthread_list (spec);

  for (i = 0; i != entries->len; i++)
  {
    GumThreadDetails * entry;
    gboolean carry_on = TRUE;

    entry = &g_array_index (entries, GumThreadDetails, i);

    if (gum_fill_thread_details (entry, flags))
      carry_on = func (entry, user_data);

    g_free ((gpointer) entry->name);

    if (!carry_on)
      break;
  }

  g_array_unref (entries);
}

static void
gum_collect_thread_entrypoint (GumThreadDetails * entry,
                               pthread_t thread,
                               const GumLinuxPThreadSpec * spec,
                               GumThreadFlags flags)
{
  gpointer start_routine;

  start_routine = gum_linux_query_pthread_start_routine (thread, spec);
  if (start_routine == NULL)
    return;

  if ((flags & GUM_THREAD_FLAGS_ENTRYPOINT_ROUTINE) != 0)
  {
    entry->entrypoint.routine = GUM_ADDRESS (start_routine);
    entry->flags |= GUM_THREAD_FLAGS_ENTRYPOINT_ROUTINE;
  }

  if ((flags & GUM_THREAD_FLAGS_ENTRYPOINT_PARAMETER) != 0)
  {
    entry->entrypoint.parameter = GUM_ADDRESS (
        gum_linux_query_pthread_start_parameter (thread, spec));
    entry->flags |= GUM_THREAD_FLAGS_ENTRYPOINT_PARAMETER;
  }
}

static gboolean
gum_fill_thread_details (GumThreadDetails * entry,
                         GumThreadFlags flags)
{
  if ((flags & GUM_THREAD_FLAGS_NAME) != 0)
  {
    gchar * name = gum_linux_query_thread_name (entry->id);
    if (name != NULL)
    {
      entry->name = name;
      entry->flags |= GUM_THREAD_FLAGS_NAME;
    }
  }

  if ((flags & GUM_THREAD_FLAGS_STATE) != 0)
  {
    if (!gum_linux_query_thread_state (entry->id, &entry->state))
      return FALSE;
    entry->flags |= GUM_THREAD_FLAGS_STATE;
  }

  if ((flags & GUM_THREAD_FLAGS_CPU_CONTEXT) != 0)
  {
    if (!gum_linux_query_thread_cpu_context (entry->id, &entry->cpu_context))
      return FALSE;
    entry->flags |= GUM_THREAD_FLAGS_CPU_CONTEXT;
  }

  return TRUE;
}

gboolean
_gum_process_collect_main_module (GumModule * module,
                                  gpointer user_data)
{
  GumModule ** out = user_data;

  *out = g_object_ref (module);

  return FALSE;
}

GumModule *
gum_process_get_libc_module (void)
{
  static GOnce once = G_ONCE_INIT;

  g_once (&once, (GThreadFunc) gum_try_init_libc_module, NULL);

  if (once.retval == NULL)
    gum_panic ("Unable to locate the libc; please file a bug");

  return once.retval;
}

static GumModule *
gum_try_init_libc_module (void)
{
  gum_libc_module = gum_process_find_module_by_address (
      GUM_ADDRESS (_gum_process_get_libc_info ()->dli_fbase));

  _gum_register_destructor (gum_deinit_libc_module);

  return gum_libc_module;
}

static void
gum_deinit_libc_module (void)
{
  g_object_unref (gum_libc_module);
}

const Dl_info *
_gum_process_get_libc_info (void)
{
  static GOnce once = G_ONCE_INIT;

  g_once (&once, (GThreadFunc) gum_try_init_libc_info, NULL);

  if (once.retval == NULL)
    gum_panic ("Unable to locate the libc; please file a bug");

  return once.retval;
}

static const Dl_info *
gum_try_init_libc_info (void)
{
#ifndef HAVE_ANDROID
  if (!gum_try_resolve_dynamic_symbol ("__libc_start_main", &gum_libc_info))
#endif
  {
    if (!gum_try_resolve_dynamic_symbol ("exit", &gum_libc_info))
      return NULL;
  }

  return &gum_libc_info;
}

GHashTable *
gum_linux_collect_named_ranges (void)
{
  GHashTable * result;
  GumProcMapsIter iter;
  gchar * name, * next_name;
  const gchar * line;
  gboolean carry_on = TRUE;
  gboolean got_line = FALSE;

  result = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) gum_linux_named_range_free);

  gum_proc_maps_iter_init_for_self (&iter);

  name = g_malloc (PATH_MAX);
  next_name = g_malloc (PATH_MAX);

  do
  {
    GumAddress start, end;
    gsize size;
    gint n;
    GumLinuxNamedRange * range;

    if (!got_line)
    {
      if (!gum_proc_maps_iter_next (&iter, &line))
        break;
    }
    else
    {
      got_line = FALSE;
    }

    n = sscanf (line,
        "%" G_GINT64_MODIFIER "x-%" G_GINT64_MODIFIER "x "
        "%*4c "
        "%*x %*s %*d "
        "%[^\n]",
        &start, &end,
        name);
    if (n == 2)
      continue;
    g_assert (n == 3);

    _gum_try_translate_vdso_name (name);

    size = end - start;

    while (gum_proc_maps_iter_next (&iter, &line))
    {
      n = sscanf (line,
          "%*x-%" G_GINT64_MODIFIER "x %*c%*c%*c%*c %*x %*s %*d %[^\n]",
          &end,
          next_name);
      if (n == 1)
      {
        continue;
      }
      else if (n == 2 && next_name[0] == '[')
      {
        if (!_gum_try_translate_vdso_name (next_name))
          continue;
      }

      if (n == 2 && strcmp (next_name, name) == 0)
      {
        size = end - start;
      }
      else
      {
        got_line = TRUE;
        break;
      }
    }

    range = g_slice_new (GumLinuxNamedRange);

    range->name = g_strdup (name);
    range->base = GSIZE_TO_POINTER (start);
    range->size = size;

    g_hash_table_insert (result, range->base, range);
  }
  while (carry_on);

  g_free (name);
  g_free (next_name);

  gum_proc_maps_iter_destroy (&iter);

  return result;
}

static void
gum_linux_named_range_free (GumLinuxNamedRange * range)
{
  g_free ((gpointer) range->name);

  g_slice_free (GumLinuxNamedRange, range);
}

gboolean
_gum_try_translate_vdso_name (gchar * name)
{
  if (strcmp (name, "[vdso]") == 0)
  {
    strcpy (name, "linux-vdso.so.1");
    return TRUE;
  }

  return FALSE;
}

#if defined (HAVE_GLIBC)

void
gum_linux_pthread_iter_init (GumLinuxPThreadIter * iter,
                             const GumLinuxPThreadSpec * spec)
{
  gsize i;
  GList * used_list = NULL;
  gboolean walked_used_list = FALSE;
  GList * user_list = NULL;
  gboolean walked_user_list = FALSE;
  gboolean success = FALSE;

  for (i = 0; i != GUM_MAX_LIST_WALK_ATTEMPTS; i++)
  {
    if (gum_linux_get_threads_from_list (spec, spec->stack_used, &used_list))
    {
      walked_used_list = TRUE;
      break;
    }
  }
  if (!walked_used_list)
    goto beach;

  for (i = 0; i != GUM_MAX_LIST_WALK_ATTEMPTS; i++)
  {
    if (gum_linux_get_threads_from_list (spec, spec->stack_user, &user_list))
    {
      walked_user_list = TRUE;
      break;
    }
  }
  if (!walked_user_list)
    goto beach;

  iter->list = g_list_concat (user_list, used_list);

  success = TRUE;

beach:
  if (!success)
  {
    g_list_free (used_list);
    g_list_free (user_list);
  }
}

static gboolean
gum_linux_get_threads_from_list (const GumLinuxPThreadSpec * spec,
                                 const GumGlibcList * anchor,
                                 GList ** threads)
{
  gboolean success = FALSE;
  GList * list = NULL;
  pthread_t first, current;
  guint num_threads = 0;
  pthread_t next, prev;

  first = (pthread_t) ((gpointer) anchor - spec->flink_offset);
  current = first;

  do
  {
    gboolean is_non_anchor_thread;

    if (!gum_linux_thread_read_flink (spec, current, &next))
      goto beach;

    if (!gum_linux_thread_read_blink (spec, next, &prev))
      goto beach;

    if (prev != current)
      goto beach;

    is_non_anchor_thread =
        current != first &&
        gum_linux_query_pthread_tid (current, spec) != 0;
    if (is_non_anchor_thread)
      list = g_list_prepend (list, GSIZE_TO_POINTER (current));

    current = next;
    num_threads++;

    /*
     * If we find more than the maximum expected number of threads without
     * getting back to the start of the list, then terminate to avoid an
     * infinite loop.
     */
    if (num_threads == GUM_LINUX_MAX_THREADS)
      goto beach;
  }
  while (current != first);

  *threads = list;
  success = TRUE;

beach:
  if (!success)
    g_list_free (list);

  return success;
}

#else

void
gum_linux_pthread_iter_init (GumLinuxPThreadIter * iter,
                             const GumLinuxPThreadSpec * spec)
{
  iter->list = NULL;
  iter->node = NULL;
  iter->spec = spec;
}

#endif

#if defined (HAVE_GLIBC)

gboolean
gum_linux_pthread_iter_next (GumLinuxPThreadIter * self,
                             pthread_t * thread)
{
  GList * first = self->list;
  if (first == NULL)
    return FALSE;

  *thread = (pthread_t) (first->data);
  self->list = g_list_next (self->list);
  g_list_free_1 (first);

  return TRUE;
}

#elif defined (HAVE_MUSL)

gboolean
gum_linux_pthread_iter_next (GumLinuxPThreadIter * self,
                             pthread_t * thread)
{
  GumLinuxPThread * list = self->list;
  GumLinuxPThread * node = self->node;

  if (list == NULL)
  {
    list = self->spec->main_thread;
    self->list = list;

    node = list;
  }
  else
  {
    node = node->next;
    if (node == list)
      return FALSE;
  }
  self->node = node;

  *thread = (pthread_t) node;
  return TRUE;
}

#elif defined (HAVE_ANDROID)

gboolean
gum_linux_pthread_iter_next (GumLinuxPThreadIter * self,
                             pthread_t * thread)
{
  GumLinuxPThread * list = self->list;
  GumLinuxPThread * node = self->node;

  if (list == NULL)
  {
    GumLinuxPThread * tail, * cur;

    tail = NULL;
    for (cur = *self->spec->thread_list; cur != NULL; cur = cur->next)
      tail = cur;

    list = tail;
    self->list = list;

    node = list;
  }
  else
  {
    node = node->prev;
  }
  if (node == NULL)
    return FALSE;
  self->node = node;

  *thread = GPOINTER_TO_SIZE (node);
  return TRUE;
}

#endif

void
_gum_process_enumerate_ranges (GumPageProtection prot,
                               GumFoundRangeFunc func,
                               gpointer user_data)
{
  gum_linux_enumerate_ranges (getpid (), prot, func, user_data);
}

void
gum_linux_enumerate_ranges (pid_t pid,
                            GumPageProtection prot,
                            GumFoundRangeFunc func,
                            gpointer user_data)
{
  GumProcMapsIter iter;
  gboolean carry_on = TRUE;
  const gchar * line;

  gum_proc_maps_iter_init_for_pid (&iter, pid);

  while (carry_on && gum_proc_maps_iter_next (&iter, &line))
  {
    GumRangeDetails details;
    GumMemoryRange range;
    GumFileMapping file;
    GumAddress end;
    gchar perms[5] = { 0, };
    guint64 inode;
    gint length;

    sscanf (line,
        "%" G_GINT64_MODIFIER "x-%" G_GINT64_MODIFIER "x "
        "%4c "
        "%" G_GINT64_MODIFIER "x %*s %" G_GINT64_MODIFIER "d"
        "%n",
        &range.base_address, &end,
        perms,
        &file.offset, &inode,
        &length);

    range.size = end - range.base_address;

    details.file = NULL;
    if (inode != 0)
    {
      file.path = strchr (line + length, '/');
      if (file.path != NULL)
      {
        details.file = &file;
        file.size = 0; /* TODO */

        if (RUNNING_ON_VALGRIND && strstr (file.path, "/valgrind/") != NULL)
          continue;
      }
    }

    details.range = &range;
    details.protection = gum_page_protection_from_proc_perms_string (perms);

    if ((details.protection & prot) == prot)
    {
      carry_on = func (&details, user_data);
    }
  }

  gum_proc_maps_iter_destroy (&iter);
}

void
gum_process_enumerate_malloc_ranges (GumFoundMallocRangeFunc func,
                                     gpointer user_data)
{
  /* Not implemented */
}

gchar *
gum_linux_query_thread_name (GumThreadId id)
{
  gchar * name = NULL;
  gchar * path;
  gchar * comm = NULL;

  path = g_strdup_printf ("/proc/self/task/%" G_GSIZE_FORMAT "/comm", id);
  if (!g_file_get_contents (path, &comm, NULL, NULL))
    goto beach;
  name = g_strchomp (g_steal_pointer (&comm));

beach:
  g_free (comm);
  g_free (path);

  return name;
}

gboolean
gum_linux_query_thread_state (GumThreadId tid,
                              GumThreadState * state)
{
  gboolean success = FALSE;
  gchar * path, * info = NULL;

  path = g_strdup_printf ("/proc/self/task/%" G_GSIZE_FORMAT "/stat", tid);
  if (g_file_get_contents (path, &info, NULL, NULL))
  {
    gchar * p;

    p = strrchr (info, ')') + 2;

    *state = gum_thread_state_from_proc_status_character (*p);
    success = TRUE;
  }

  g_free (info);
  g_free (path);

  return success;
}

static GumThreadState
gum_thread_state_from_proc_status_character (gchar c)
{
  switch (g_ascii_toupper (c))
  {
    case 'R': return GUM_THREAD_RUNNING;
    case 'S': return GUM_THREAD_WAITING;
    case 'D': return GUM_THREAD_UNINTERRUPTIBLE;
    case 'Z': return GUM_THREAD_UNINTERRUPTIBLE;
    case 'T': return GUM_THREAD_STOPPED;
    case 'W':
    default:
      return GUM_THREAD_UNINTERRUPTIBLE;
  }
}

gboolean
gum_linux_query_thread_cpu_context (GumThreadId tid,
                                    GumCpuContext * ctx)
{
  return gum_process_modify_thread (tid, gum_store_cpu_context, ctx,
      GUM_MODIFY_THREAD_FLAGS_ABORT_SAFELY);
}

static void
gum_store_cpu_context (GumThreadId thread_id,
                       GumCpuContext * cpu_context,
                       gpointer user_data)
{
  memcpy (user_data, cpu_context, sizeof (GumCpuContext));
}

#ifdef HAVE_GLIBC
GumThreadId
gum_linux_query_pthread_tid (pthread_t thread,
                             const GumLinuxPThreadSpec * spec)
{
  gint * tid = GSIZE_TO_POINTER (thread) + spec->tid_offset;
  return *tid;
}
#else
GumThreadId
gum_linux_query_pthread_tid (pthread_t thread,
                             const GumLinuxPThreadSpec * spec)
{
  GumLinuxPThread * pth = GSIZE_TO_POINTER (thread);

  return pth->tid;
}
#endif

gpointer
gum_linux_query_pthread_start_routine (pthread_t thread,
                                       const GumLinuxPThreadSpec * spec)
{
#ifdef HAVE_MUSL
  GumMuslStartArgs * args = gum_query_pthread_start_args (thread, spec);
  if (args == NULL)
    return NULL;
  return args->start_func;
#else
  return *((gpointer *) ((guint8 *) thread + spec->start_routine_offset));
#endif
}

gpointer
gum_linux_query_pthread_start_parameter (pthread_t thread,
                                         const GumLinuxPThreadSpec * spec)
{
#ifdef HAVE_MUSL
  GumMuslStartArgs * args = gum_query_pthread_start_args (thread, spec);
  if (args == NULL)
    return NULL;
  return args->start_arg;
#else
  return *((gpointer *) ((guint8 *) thread + spec->start_parameter_offset));
#endif
}

guint
gum_thread_try_get_ranges (GumMemoryRange * ranges,
                           guint max_length)
{
#ifdef HAVE_PTHREAD_ATTR_GETSTACK
  guint n = 0;
  pthread_attr_t attr;
  gboolean allocated = FALSE;
  void * stack_addr;
  size_t stack_size, guard_size;
  GumMemoryRange * range;
  stack_t signal_stack;

  if (pthread_getattr_np (pthread_self (), &attr) != 0)
    goto beach;
  allocated = TRUE;

  if (pthread_attr_getstack (&attr, &stack_addr, &stack_size) != 0)
    goto beach;

  guard_size = 0;
  pthread_attr_getguardsize (&attr, &guard_size);

  range = &ranges[n++];
  range->base_address = GUM_ADDRESS (stack_addr) - guard_size;
  range->size = stack_size + guard_size;

  if (n != max_length &&
      sigaltstack (NULL, &signal_stack) == 0 &&
      (signal_stack.ss_flags & SS_DISABLE) == 0 &&
      signal_stack.ss_sp != NULL)
  {
    range = &ranges[n++];
    range->base_address = GUM_ADDRESS (signal_stack.ss_sp);
    range->size = signal_stack.ss_size;
  }

beach:
  if (allocated)
    pthread_attr_destroy (&attr);

  return n;
#else
  return 0;
#endif
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
  if (syscall (__NR_tgkill, getpid (), thread_id, SIGSTOP) != 0)
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
  if (syscall (__NR_tgkill, getpid (), thread_id, SIGCONT) != 0)
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
  GumSetHardwareBreakpointContext bpc;

  bpc.breakpoint_id = breakpoint_id;
  bpc.address = address;

  return gum_linux_modify_thread (thread_id, GUM_REGS_DEBUG_BREAK,
      gum_do_set_hardware_breakpoint, &bpc, error);
}

static void
gum_do_set_hardware_breakpoint (GumThreadId thread_id,
                                GumRegs * regs,
                                gpointer user_data)
{
  GumDebugRegs * dr = &regs->debug;
  GumSetHardwareBreakpointContext * bpc = user_data;

#if defined (HAVE_I386)
  _gum_x86_set_breakpoint (&dr->dr7, &dr->dr0, bpc->breakpoint_id,
      bpc->address);
#elif defined (HAVE_ARM)
  _gum_arm_set_breakpoint (dr->cr, dr->vr, bpc->breakpoint_id, bpc->address);
#elif defined (HAVE_ARM64)
  _gum_arm64_set_breakpoint (dr->cr, dr->vr, bpc->breakpoint_id, bpc->address);
#elif defined (HAVE_MIPS) && GLIB_SIZEOF_VOID_P == 4
  g_assert (dr->style == GUM_MIPS_WATCH_MIPS32);
  _gum_mips_set_breakpoint (dr->mips32.watch_lo, dr->mips32.watch_hi,
      bpc->breakpoint_id, bpc->address);
#elif defined (HAVE_MIPS) && GLIB_SIZEOF_VOID_P == 8
  g_assert (dr->style == GUM_MIPS_WATCH_MIPS64);
  _gum_mips_set_breakpoint (dr->mips64.watch_lo, dr->mips64.watch_hi,
      bpc->breakpoint_id, bpc->address);
#endif
}

gboolean
gum_thread_unset_hardware_breakpoint (GumThreadId thread_id,
                                      guint breakpoint_id,
                                      GError ** error)
{
  return gum_linux_modify_thread (thread_id, GUM_REGS_DEBUG_BREAK,
      gum_do_unset_hardware_breakpoint, GUINT_TO_POINTER (breakpoint_id),
      error);
}

static void
gum_do_unset_hardware_breakpoint (GumThreadId thread_id,
                                  GumRegs * regs,
                                  gpointer user_data)
{
  GumDebugRegs * dr = &regs->debug;
  guint breakpoint_id = GPOINTER_TO_UINT (user_data);

#if defined (HAVE_I386)
  _gum_x86_unset_breakpoint (&dr->dr7, &dr->dr0, breakpoint_id);
#elif defined (HAVE_ARM)
  _gum_arm_unset_breakpoint (dr->cr, dr->vr, breakpoint_id);
#elif defined (HAVE_ARM64)
  _gum_arm64_unset_breakpoint (dr->cr, dr->vr, breakpoint_id);
#elif defined (HAVE_MIPS) && GLIB_SIZEOF_VOID_P == 4
  g_assert (dr->style == GUM_MIPS_WATCH_MIPS32);
  _gum_mips_unset_breakpoint (dr->mips32.watch_lo, dr->mips32.watch_hi,
      breakpoint_id);
#elif defined (HAVE_MIPS) && GLIB_SIZEOF_VOID_P == 8
  g_assert (dr->style == GUM_MIPS_WATCH_MIPS64);
  _gum_mips_unset_breakpoint (dr->mips64.watch_lo, dr->mips64.watch_hi,
      breakpoint_id);
#endif
}

gboolean
gum_thread_set_hardware_watchpoint (GumThreadId thread_id,
                                    guint watchpoint_id,
                                    GumAddress address,
                                    gsize size,
                                    GumWatchConditions wc,
                                    GError ** error)
{
  GumSetHardwareWatchpointContext wpc;

  wpc.watchpoint_id = watchpoint_id;
  wpc.address = address;
  wpc.size = size;
  wpc.conditions = wc;

  return gum_linux_modify_thread (thread_id, GUM_REGS_DEBUG_WATCH,
      gum_do_set_hardware_watchpoint, &wpc, error);
}

static void
gum_do_set_hardware_watchpoint (GumThreadId thread_id,
                                GumRegs * regs,
                                gpointer user_data)
{
  GumDebugRegs * dr = &regs->debug;
  GumSetHardwareWatchpointContext * wpc = user_data;

#if defined (HAVE_I386)
  _gum_x86_set_watchpoint (&dr->dr7, &dr->dr0, wpc->watchpoint_id, wpc->address,
      wpc->size, wpc->conditions);
#elif defined (HAVE_ARM)
  _gum_arm_set_watchpoint (dr->cr, dr->vr, wpc->watchpoint_id, wpc->address,
      wpc->size, wpc->conditions);
#elif defined (HAVE_ARM64)
  _gum_arm64_set_watchpoint (dr->cr, dr->vr, wpc->watchpoint_id, wpc->address,
      wpc->size, wpc->conditions);
#elif defined (HAVE_MIPS) && GLIB_SIZEOF_VOID_P == 4
  g_assert (dr->style == GUM_MIPS_WATCH_MIPS32);
  _gum_mips_set_watchpoint (dr->mips32.watch_lo, dr->mips32.watch_hi,
      wpc->watchpoint_id, wpc->address, wpc->size, wpc->conditions);
#elif defined (HAVE_MIPS) && GLIB_SIZEOF_VOID_P == 8
  g_assert (dr->style == GUM_MIPS_WATCH_MIPS64);
  _gum_mips_set_watchpoint (dr->mips64.watch_lo, dr->mips64.watch_hi,
      wpc->watchpoint_id, wpc->address, wpc->size, wpc->conditions);
#endif
}

gboolean
gum_thread_unset_hardware_watchpoint (GumThreadId thread_id,
                                      guint watchpoint_id,
                                      GError ** error)
{
  return gum_linux_modify_thread (thread_id, GUM_REGS_DEBUG_WATCH,
      gum_do_unset_hardware_watchpoint, GUINT_TO_POINTER (watchpoint_id),
      error);
}

static void
gum_do_unset_hardware_watchpoint (GumThreadId thread_id,
                                  GumRegs * regs,
                                  gpointer user_data)
{
  GumDebugRegs * dr = &regs->debug;
  guint watchpoint_id = GPOINTER_TO_UINT (user_data);

#if defined (HAVE_I386)
  _gum_x86_unset_watchpoint (&dr->dr7, &dr->dr0, watchpoint_id);
#elif defined (HAVE_ARM)
  _gum_arm_unset_watchpoint (dr->cr, dr->vr, watchpoint_id);
#elif defined (HAVE_ARM64)
  _gum_arm64_unset_watchpoint (dr->cr, dr->vr, watchpoint_id);
#elif defined (HAVE_MIPS) && GLIB_SIZEOF_VOID_P == 4
  g_assert (dr->style == GUM_MIPS_WATCH_MIPS32);
  _gum_mips_unset_watchpoint (dr->mips32.watch_lo, dr->mips32.watch_hi,
      watchpoint_id);
#elif defined (HAVE_MIPS) && GLIB_SIZEOF_VOID_P == 8
  g_assert (dr->style == GUM_MIPS_WATCH_MIPS64);
  _gum_mips_unset_watchpoint (dr->mips64.watch_lo, dr->mips64.watch_hi,
      watchpoint_id);
#endif
}

gboolean
gum_linux_check_kernel_version (guint major,
                                guint minor,
                                guint micro)
{
  static gboolean initialized = FALSE;
  static guint kern_major = G_MAXUINT;
  static guint kern_minor = G_MAXUINT;
  static guint kern_micro = G_MAXUINT;

  if (!initialized)
  {
    struct utsname un;
    G_GNUC_UNUSED int res;

    res = uname (&un);
    g_assert (res == 0);

    sscanf (un.release, "%u.%u.%u", &kern_major, &kern_minor, &kern_micro);

    initialized = TRUE;
  }

  if (kern_major > major)
    return TRUE;

  if (kern_major == major && kern_minor > minor)
    return TRUE;

  return kern_major == major && kern_minor == minor && kern_micro >= micro;
}

GumCpuType
gum_linux_cpu_type_from_file (const gchar * path,
                              GError ** error)
{
  GumCpuType result = -1;
  FILE * file;
  guint8 ei_data;
  guint16 e_machine;

  file = fopen (path, "rb");
  if (file == NULL)
    goto fopen_failed;

  if (fseek (file, EI_DATA, SEEK_SET) != 0)
    goto unsupported_executable;
  if (fread (&ei_data, sizeof (ei_data), 1, file) != 1)
    goto unsupported_executable;

  if (fseek (file, 0x12, SEEK_SET) != 0)
    goto unsupported_executable;
  if (fread (&e_machine, sizeof (e_machine), 1, file) != 1)
    goto unsupported_executable;

  if (ei_data == ELFDATA2LSB)
    e_machine = GUINT16_FROM_LE (e_machine);
  else if (ei_data == ELFDATA2MSB)
    e_machine = GUINT16_FROM_BE (e_machine);
  else
    goto unsupported_ei_data;

  switch (e_machine)
  {
    case 0x0003:
      result = GUM_CPU_IA32;
      break;
    case 0x003e:
      result = GUM_CPU_AMD64;
      break;
    case 0x0028:
      result = GUM_CPU_ARM;
      break;
    case 0x00b7:
      result = GUM_CPU_ARM64;
      break;
    case 0x0008:
      result = GUM_CPU_MIPS;
      break;
    default:
      goto unsupported_executable;
  }

  goto beach;

fopen_failed:
  {
    if (errno == ENOENT)
    {
      g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_FOUND,
          "File not found");
    }
    else if (errno == EACCES)
    {
      g_set_error (error, GUM_ERROR, GUM_ERROR_PERMISSION_DENIED,
          "Permission denied");
    }
    else
    {
      g_set_error (error, GUM_ERROR, GUM_ERROR_FAILED,
          "Unable to open file: %s", g_strerror (errno));
    }
    goto beach;
  }
unsupported_ei_data:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_SUPPORTED,
        "Unsupported ELF EI_DATA");
    goto beach;
  }
unsupported_executable:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_SUPPORTED,
        "Unsupported executable");
    goto beach;
  }
beach:
  {
    if (file != NULL)
      fclose (file);

    return result;
  }
}

GumCpuType
gum_linux_cpu_type_from_pid (pid_t pid,
                             GError ** error)
{
  GumCpuType result = -1;
  GError * err;
  gchar * auxv_path, * auxv;
  gsize auxv_size;

  auxv_path = g_strdup_printf ("/proc/%d/auxv", pid);

  auxv = NULL;
  err = NULL;
  if (!g_file_get_contents (auxv_path, &auxv, &auxv_size, &err))
    goto read_failed;
  if (auxv_size == 0)
    goto nearly_dead;

  result = gum_linux_cpu_type_from_auxv (auxv, auxv_size);

  goto beach;

read_failed:
  {
    if (g_error_matches (err, G_FILE_ERROR, G_FILE_ERROR_NOENT))
    {
      g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_FOUND,
          "Process not found");
    }
    else if (g_error_matches (err, G_FILE_ERROR, G_FILE_ERROR_ACCES))
    {
      g_set_error (error, GUM_ERROR, GUM_ERROR_PERMISSION_DENIED,
          "Permission denied");
    }
    else
    {
      g_set_error (error, GUM_ERROR, GUM_ERROR_FAILED,
          "%s", err->message);
    }

    g_error_free (err);

    goto beach;
  }
nearly_dead:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_FOUND,
        "Process not found");
    goto beach;
  }
beach:
  {
    g_free (auxv);
    g_free (auxv_path);

    return result;
  }
}

GumCpuType
gum_linux_cpu_type_from_auxv (gconstpointer auxv,
                              gsize auxv_size)
{
  GumCpuType result = -1;
  GumCpuType cpu32, cpu64;
  gsize i;

  /*
   * If we are building for ILP32, then the logic below doesn't work since
   * although our target process is 64-bit, the address space is constrained
   * to 32-bits. Thus, none of the high bits will be set. On this platform,
   * however, we only support ILP32 processes and so we can assume that they
   * are all 64-bit.
   */
#if defined (HAVE_ARM64) && !(defined (__LP64__) || defined (_WIN64))
  return GUM_CPU_ARM64;
#endif

#if defined (HAVE_I386)
  cpu32 = GUM_CPU_IA32;
  cpu64 = GUM_CPU_AMD64;
#elif defined (HAVE_ARM) || defined (HAVE_ARM64)
  cpu32 = GUM_CPU_ARM;
  cpu64 = GUM_CPU_ARM64;
#elif defined (HAVE_MIPS)
  cpu32 = GUM_CPU_MIPS;
  cpu64 = GUM_CPU_MIPS;
#else
# error Unsupported architecture
#endif

  /*
   * The auxilliary structure format is architecture specific. Most notably,
   * type and value are both natively sized. We therefore detect whether a
   * process is 64-bit by examining each entry and confirming that the low bits
   * of the type field are zero. Note that this is itself endian specific.
   *
   * typedef struct
   * {
   *   uint32_t a_type;
   *   union
   *   {
   *     uint32_t a_val;
   *   } a_un;
   * } Elf32_auxv_t;
   *
   * typedef struct
   * {
   *   uint64_t a_type;
   *   union
   *   {
   *     uint64_t a_val;
   *   } a_un;
   * } Elf64_auxv_t;
   *
   * If the auxiliary vector is 32-bits and contains only an AT_NULL entry (note
   * that the documentation states that "The last entry contains two zeros"),
   * this will mean it has no non-zero type codes and could be mistaken for a
   * 64-bit format auxiliary vector. We therefore handle this special case.
   *
   * If the vector is less than 16 bytes it is not large enough to contain two
   * 64-bit zero values. If it is larger, then if it is a 32-bit format vector,
   * then it must contain at least one non-zero type code and hence the test
   * below should work.
   */

  if (auxv_size < 2 * sizeof (guint64))
  {
    result = cpu32;
  }
  else
  {
    result = cpu64;

    for (i = 0; i + sizeof (guint64) <= auxv_size; i += 16)
    {
      const guint64 * auxv_type = auxv + i;

      if ((*auxv_type & G_GUINT64_CONSTANT (0xffffffff00000000)) != 0)
      {
        result = cpu32;
        break;
      }
    }
  }

  return result;
}

gboolean
gum_linux_module_path_matches (const gchar * path,
                               const gchar * name_or_path)
{
  const gchar * s;

  if (name_or_path[0] == '/')
    return strcmp (name_or_path, path) == 0;

  if ((s = strrchr (path, '/')) != NULL)
    return strcmp (name_or_path, s + 1) == 0;

  return strcmp (name_or_path, path) == 0;
}

void
gum_proc_maps_iter_init_for_self (GumProcMapsIter * iter)
{
  gum_proc_maps_iter_init_for_path (iter, "/proc/self/maps");
}

void
gum_proc_maps_iter_init_for_pid (GumProcMapsIter * iter,
                                 pid_t pid)
{
  gchar path[31 + 1];

  sprintf (path, "/proc/%u/maps", (guint) pid);

  gum_proc_maps_iter_init_for_path (iter, path);
}

static void
gum_proc_maps_iter_init_for_path (GumProcMapsIter * iter,
                                  const gchar * path)
{
  iter->fd = open (path, O_RDONLY | O_CLOEXEC);
  iter->read_cursor = iter->buffer;
  iter->write_cursor = iter->buffer;
}

void
gum_proc_maps_iter_destroy (GumProcMapsIter * iter)
{
  if (iter->fd != -1)
    close (iter->fd);
}

gboolean
gum_proc_maps_iter_next (GumProcMapsIter * iter,
                         const gchar ** line)
{
  gchar * next_newline;
  guint available;
  gboolean need_refill;

  if (iter->fd == -1)
    return FALSE;

  next_newline = NULL;

  available = iter->write_cursor - iter->read_cursor;
  if (available == 0)
  {
    need_refill = TRUE;
  }
  else
  {
    next_newline = strchr (iter->read_cursor, '\n');
    if (next_newline != NULL)
    {
      need_refill = FALSE;
    }
    else
    {
      need_refill = TRUE;
    }
  }

  if (need_refill)
  {
    guint offset;
    gssize res;

    offset = iter->read_cursor - iter->buffer;
    if (offset > 0)
    {
      memmove (iter->buffer, iter->read_cursor, available);
      iter->read_cursor -= offset;
      iter->write_cursor -= offset;
    }

    res = GUM_TEMP_FAILURE_RETRY (gum_libc_read (iter->fd,
        iter->write_cursor,
        iter->buffer + sizeof (iter->buffer) - 1 - iter->write_cursor));
    if (res <= 0)
      return FALSE;

    iter->write_cursor += res;
    iter->write_cursor[0] = '\0';

    next_newline = strchr (iter->read_cursor, '\n');
  }

  *line = iter->read_cursor;
  *next_newline = '\0';

  iter->read_cursor = next_newline + 1;

  return TRUE;
}

void
_gum_acquire_dumpability (void)
{
  G_LOCK (gum_dumpable);

  if (++gum_dumpable_refcount == 1)
  {
    /*
     * Some systems (notably Android on release applications) spawn processes as
     * not dumpable by default, disabling ptrace() and some other things on that
     * process for anyone other than root.
     */
    gum_dumpable_previous = prctl (PR_GET_DUMPABLE);
    if (gum_dumpable_previous != -1 && gum_dumpable_previous != 1)
      prctl (PR_SET_DUMPABLE, 1);
  }

  G_UNLOCK (gum_dumpable);
}

void
_gum_release_dumpability (void)
{
  G_LOCK (gum_dumpable);

  if (--gum_dumpable_refcount == 0)
  {
    if (gum_dumpable_previous != -1 && gum_dumpable_previous != 1)
      prctl (PR_SET_DUMPABLE, gum_dumpable_previous);
  }

  G_UNLOCK (gum_dumpable);
}

void
gum_linux_parse_ucontext (const ucontext_t * uc,
                          GumCpuContext * ctx)
{
#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
  const greg_t * gr = uc->uc_mcontext.gregs;

  ctx->eip = gr[REG_EIP];

  ctx->edi = gr[REG_EDI];
  ctx->esi = gr[REG_ESI];
  ctx->ebp = gr[REG_EBP];
  ctx->esp = gr[REG_ESP];
  ctx->ebx = gr[REG_EBX];
  ctx->edx = gr[REG_EDX];
  ctx->ecx = gr[REG_ECX];
  ctx->eax = gr[REG_EAX];

  ctx->xmm = NULL;
#elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
  const greg_t * gr = uc->uc_mcontext.gregs;

  ctx->rip = gr[REG_RIP];

  ctx->r15 = gr[REG_R15];
  ctx->r14 = gr[REG_R14];
  ctx->r13 = gr[REG_R13];
  ctx->r12 = gr[REG_R12];
  ctx->r11 = gr[REG_R11];
  ctx->r10 = gr[REG_R10];
  ctx->r9 = gr[REG_R9];
  ctx->r8 = gr[REG_R8];

  ctx->rdi = gr[REG_RDI];
  ctx->rsi = gr[REG_RSI];
  ctx->rbp = gr[REG_RBP];
  ctx->rsp = gr[REG_RSP];
  ctx->rbx = gr[REG_RBX];
  ctx->rdx = gr[REG_RDX];
  ctx->rcx = gr[REG_RCX];
  ctx->rax = gr[REG_RAX];

  ctx->xmm = (GumX86VectorReg *) uc->uc_mcontext.fpregs->_xmm;
#elif defined (HAVE_ARM) && defined (HAVE_LEGACY_MCONTEXT)
  const elf_greg_t * gr = uc->uc_mcontext.gregs;

  ctx->pc = gr[R15];
  ctx->sp = gr[R13];
  ctx->cpsr = 0; /* FIXME: Anything we can do about this? */

  ctx->r8 = gr[R8];
  ctx->r9 = gr[R9];
  ctx->r10 = gr[R10];
  ctx->r11 = gr[R11];
  ctx->r12 = gr[R12];

  memset (ctx->v, 0, sizeof (ctx->v));

  ctx->r[0] = gr[R0];
  ctx->r[1] = gr[R1];
  ctx->r[2] = gr[R2];
  ctx->r[3] = gr[R3];
  ctx->r[4] = gr[R4];
  ctx->r[5] = gr[R5];
  ctx->r[6] = gr[R6];
  ctx->r[7] = gr[R7];
  ctx->lr = gr[R14];
#elif defined (HAVE_ARM)
  const mcontext_t * mc = &uc->uc_mcontext;

  ctx->pc = mc->arm_pc;
  ctx->sp = mc->arm_sp;
  ctx->cpsr = mc->arm_cpsr;

  ctx->r8 = mc->arm_r8;
  ctx->r9 = mc->arm_r9;
  ctx->r10 = mc->arm_r10;
  ctx->r11 = mc->arm_fp;
  ctx->r12 = mc->arm_ip;

  memset (ctx->v, 0, sizeof (ctx->v));

  ctx->r[0] = mc->arm_r0;
  ctx->r[1] = mc->arm_r1;
  ctx->r[2] = mc->arm_r2;
  ctx->r[3] = mc->arm_r3;
  ctx->r[4] = mc->arm_r4;
  ctx->r[5] = mc->arm_r5;
  ctx->r[6] = mc->arm_r6;
  ctx->r[7] = mc->arm_r7;
  ctx->lr = mc->arm_lr;
#elif defined (HAVE_ARM64)
  const mcontext_t * mc = &uc->uc_mcontext;
  gsize i;

  ctx->pc = mc->pc;
  ctx->sp = mc->sp;
  ctx->nzcv = 0;

  for (i = 0; i != G_N_ELEMENTS (ctx->x); i++)
    ctx->x[i] = mc->regs[i];
  ctx->fp = mc->regs[29];
  ctx->lr = mc->regs[30];

  memset (ctx->v, 0, sizeof (ctx->v));

  for (i = 0; i + sizeof (struct _aarch64_ctx) <= sizeof (mc->__reserved); )
  {
    struct _aarch64_ctx * head = (struct _aarch64_ctx *) &mc->__reserved[i];

    if (head->magic == 0 || head->size < sizeof (struct _aarch64_ctx) ||
        i + head->size > sizeof (mc->__reserved))
      break;

    switch (head->magic)
    {
      case FPSIMD_MAGIC:
      {
        struct fpsimd_context * fpsimd = (struct fpsimd_context *) head;

        memcpy (&ctx->v, &fpsimd->vregs, sizeof (ctx->v));

        break;
      }
    }

    i += head->size;
  }
#elif defined (HAVE_MIPS)
  const greg_t * gr = uc->uc_mcontext.gregs;

  ctx->at = (guint32) gr[1];

  ctx->v0 = (guint32) gr[2];
  ctx->v1 = (guint32) gr[3];

  ctx->a0 = (guint32) gr[4];
  ctx->a1 = (guint32) gr[5];
  ctx->a2 = (guint32) gr[6];
  ctx->a3 = (guint32) gr[7];

  ctx->t0 = (guint32) gr[8];
  ctx->t1 = (guint32) gr[9];
  ctx->t2 = (guint32) gr[10];
  ctx->t3 = (guint32) gr[11];
  ctx->t4 = (guint32) gr[12];
  ctx->t5 = (guint32) gr[13];
  ctx->t6 = (guint32) gr[14];
  ctx->t7 = (guint32) gr[15];

  ctx->s0 = (guint32) gr[16];
  ctx->s1 = (guint32) gr[17];
  ctx->s2 = (guint32) gr[18];
  ctx->s3 = (guint32) gr[19];
  ctx->s4 = (guint32) gr[20];
  ctx->s5 = (guint32) gr[21];
  ctx->s6 = (guint32) gr[22];
  ctx->s7 = (guint32) gr[23];

  ctx->t8 = (guint32) gr[24];
  ctx->t9 = (guint32) gr[25];

  ctx->k0 = (guint32) gr[26];
  ctx->k1 = (guint32) gr[27];

  ctx->gp = (guint32) gr[28];
  ctx->sp = (guint32) gr[29];
  ctx->fp = (guint32) gr[30];
  ctx->ra = (guint32) gr[31];

  ctx->hi = (guint32) uc->uc_mcontext.mdhi;
  ctx->lo = (guint32) uc->uc_mcontext.mdlo;

  ctx->pc = (guint32) uc->uc_mcontext.pc;
#else
# error FIXME
#endif
}

void
gum_linux_unparse_ucontext (const GumCpuContext * ctx,
                            ucontext_t * uc)
{
#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
  greg_t * gr = uc->uc_mcontext.gregs;

  gr[REG_EIP] = ctx->eip;

  gr[REG_EDI] = ctx->edi;
  gr[REG_ESI] = ctx->esi;
  gr[REG_EBP] = ctx->ebp;
  gr[REG_ESP] = ctx->esp;
  gr[REG_EBX] = ctx->ebx;
  gr[REG_EDX] = ctx->edx;
  gr[REG_ECX] = ctx->ecx;
  gr[REG_EAX] = ctx->eax;
#elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
  greg_t * gr = uc->uc_mcontext.gregs;

  gr[REG_RIP] = ctx->rip;

  gr[REG_R15] = ctx->r15;
  gr[REG_R14] = ctx->r14;
  gr[REG_R13] = ctx->r13;
  gr[REG_R12] = ctx->r12;
  gr[REG_R11] = ctx->r11;
  gr[REG_R10] = ctx->r10;
  gr[REG_R9] = ctx->r9;
  gr[REG_R8] = ctx->r8;

  gr[REG_RDI] = ctx->rdi;
  gr[REG_RSI] = ctx->rsi;
  gr[REG_RBP] = ctx->rbp;
  gr[REG_RSP] = ctx->rsp;
  gr[REG_RBX] = ctx->rbx;
  gr[REG_RDX] = ctx->rdx;
  gr[REG_RCX] = ctx->rcx;
  gr[REG_RAX] = ctx->rax;
#elif defined (HAVE_ARM) && defined (HAVE_LEGACY_MCONTEXT)
  elf_greg_t * gr = uc->uc_mcontext.gregs;

  /* FIXME: Anything we can do about cpsr? */
  gr[R15] = ctx->pc;
  gr[R13] = ctx->sp;

  gr[R8] = ctx->r8;
  gr[R9] = ctx->r9;
  gr[R10] = ctx->r10;
  gr[R11] = ctx->r11;
  gr[R12] = ctx->r12;

  gr[R0] = ctx->r[0];
  gr[R1] = ctx->r[1];
  gr[R2] = ctx->r[2];
  gr[R3] = ctx->r[3];
  gr[R4] = ctx->r[4];
  gr[R5] = ctx->r[5];
  gr[R6] = ctx->r[6];
  gr[R7] = ctx->r[7];
  gr[R14] = ctx->lr;
#elif defined (HAVE_ARM)
  mcontext_t * mc = &uc->uc_mcontext;

  mc->arm_pc = ctx->pc;
  mc->arm_sp = ctx->sp;
  mc->arm_cpsr = ctx->cpsr;

  mc->arm_r8 = ctx->r8;
  mc->arm_r9 = ctx->r9;
  mc->arm_r10 = ctx->r10;
  mc->arm_fp = ctx->r11;
  mc->arm_ip = ctx->r12;

  mc->arm_r0 = ctx->r[0];
  mc->arm_r1 = ctx->r[1];
  mc->arm_r2 = ctx->r[2];
  mc->arm_r3 = ctx->r[3];
  mc->arm_r4 = ctx->r[4];
  mc->arm_r5 = ctx->r[5];
  mc->arm_r6 = ctx->r[6];
  mc->arm_r7 = ctx->r[7];
  mc->arm_lr = ctx->lr;
#elif defined (HAVE_ARM64)
  mcontext_t * mc = &uc->uc_mcontext;
  gsize i;

  mc->pc = ctx->pc;
  mc->sp = ctx->sp;

  for (i = 0; i != G_N_ELEMENTS (ctx->x); i++)
    mc->regs[i] = ctx->x[i];
  mc->regs[29] = ctx->fp;
  mc->regs[30] = ctx->lr;

  for (i = 0; i + sizeof (struct _aarch64_ctx) <= sizeof (mc->__reserved); )
  {
    struct _aarch64_ctx * head = (struct _aarch64_ctx *) &mc->__reserved[i];

    if (head->magic == 0 || head->size < sizeof (struct _aarch64_ctx) ||
        i + head->size > sizeof (mc->__reserved))
      break;

    switch (head->magic)
    {
      case FPSIMD_MAGIC:
      {
        struct fpsimd_context * fpsimd = (struct fpsimd_context *) head;

        memcpy (&fpsimd->vregs, &ctx->v, sizeof (ctx->v));

        break;
      }
    }

    i += head->size;
  }
#elif defined (HAVE_MIPS)
  greg_t * gr = uc->uc_mcontext.gregs;

  gr[1] = (guint64) ctx->at;

  gr[2] = (guint64) ctx->v0;
  gr[3] = (guint64) ctx->v1;

  gr[4] = (guint64) ctx->a0;
  gr[5] = (guint64) ctx->a1;
  gr[6] = (guint64) ctx->a2;
  gr[7] = (guint64) ctx->a3;

  gr[8] = (guint64) ctx->t0;
  gr[9] = (guint64) ctx->t1;
  gr[10] = (guint64) ctx->t2;
  gr[11] = (guint64) ctx->t3;
  gr[12] = (guint64) ctx->t4;
  gr[13] = (guint64) ctx->t5;
  gr[14] = (guint64) ctx->t6;
  gr[15] = (guint64) ctx->t7;

  gr[16] = (guint64) ctx->s0;
  gr[17] = (guint64) ctx->s1;
  gr[18] = (guint64) ctx->s2;
  gr[19] = (guint64) ctx->s3;
  gr[20] = (guint64) ctx->s4;
  gr[21] = (guint64) ctx->s5;
  gr[22] = (guint64) ctx->s6;
  gr[23] = (guint64) ctx->s7;

  gr[24] = (guint64) ctx->t8;
  gr[25] = (guint64) ctx->t9;

  gr[26] = (guint64) ctx->k0;
  gr[27] = (guint64) ctx->k1;

  gr[28] = (guint64) ctx->gp;
  gr[29] = (guint64) ctx->sp;
  gr[30] = (guint64) ctx->fp;
  gr[31] = (guint64) ctx->ra;

  uc->uc_mcontext.mdhi = (guint64) ctx->hi;
  uc->uc_mcontext.mdlo = (guint64) ctx->lo;

  uc->uc_mcontext.pc = (guint64) ctx->pc;
#else
# error FIXME
#endif
}

static void
gum_parse_gp_regs (const GumGPRegs * regs,
                   GumCpuContext * ctx)
{
#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
  ctx->eip = regs->eip;

  ctx->edi = regs->edi;
  ctx->esi = regs->esi;
  ctx->ebp = regs->ebp;
  ctx->esp = regs->esp;
  ctx->ebx = regs->ebx;
  ctx->edx = regs->edx;
  ctx->ecx = regs->ecx;
  ctx->eax = regs->eax;

  ctx->xmm = NULL;
#elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
  ctx->rip = regs->rip;

  ctx->r15 = regs->r15;
  ctx->r14 = regs->r14;
  ctx->r13 = regs->r13;
  ctx->r12 = regs->r12;
  ctx->r11 = regs->r11;
  ctx->r10 = regs->r10;
  ctx->r9 = regs->r9;
  ctx->r8 = regs->r8;

  ctx->rdi = regs->rdi;
  ctx->rsi = regs->rsi;
  ctx->rbp = regs->rbp;
  ctx->rsp = regs->rsp;
  ctx->rbx = regs->rbx;
  ctx->rdx = regs->rdx;
  ctx->rcx = regs->rcx;
  ctx->rax = regs->rax;

  ctx->xmm = NULL;
#elif defined (HAVE_ARM)
  gsize i;

  ctx->pc = regs->ARM_pc;
  ctx->sp = regs->ARM_sp;
  ctx->cpsr = regs->ARM_cpsr;

  ctx->r8 = regs->uregs[8];
  ctx->r9 = regs->uregs[9];
  ctx->r10 = regs->uregs[10];
  ctx->r11 = regs->uregs[11];
  ctx->r12 = regs->uregs[12];

  memset (ctx->v, 0, sizeof (ctx->v));

  for (i = 0; i != G_N_ELEMENTS (ctx->r); i++)
    ctx->r[i] = regs->uregs[i];
  ctx->lr = regs->ARM_lr;
#elif defined (HAVE_ARM64)
  gsize i;

  ctx->pc = regs->pc;
  ctx->sp = regs->sp;
  ctx->nzcv = 0;

  for (i = 0; i != G_N_ELEMENTS (ctx->x); i++)
    ctx->x[i] = regs->regs[i];
  ctx->fp = regs->regs[29];
  ctx->lr = regs->regs[30];

  memset (ctx->v, 0, sizeof (ctx->v));
#elif defined (HAVE_MIPS)
  ctx->at = regs->regs[1];

  ctx->v0 = regs->regs[2];
  ctx->v1 = regs->regs[3];

  ctx->a0 = regs->regs[4];
  ctx->a1 = regs->regs[5];
  ctx->a2 = regs->regs[6];
  ctx->a3 = regs->regs[7];

  ctx->t0 = regs->regs[8];
  ctx->t1 = regs->regs[9];
  ctx->t2 = regs->regs[10];
  ctx->t3 = regs->regs[11];
  ctx->t4 = regs->regs[12];
  ctx->t5 = regs->regs[13];
  ctx->t6 = regs->regs[14];
  ctx->t7 = regs->regs[15];

  ctx->s0 = regs->regs[16];
  ctx->s1 = regs->regs[17];
  ctx->s2 = regs->regs[18];
  ctx->s3 = regs->regs[19];
  ctx->s4 = regs->regs[20];
  ctx->s5 = regs->regs[21];
  ctx->s6 = regs->regs[22];
  ctx->s7 = regs->regs[23];

  ctx->t8 = regs->regs[24];
  ctx->t9 = regs->regs[25];

  ctx->k0 = regs->regs[26];
  ctx->k1 = regs->regs[27];

  ctx->gp = regs->regs[28];
  ctx->sp = regs->regs[29];
  ctx->fp = regs->regs[30];

  ctx->ra = regs->regs[31];

  ctx->hi = regs->hi;
  ctx->lo = regs->lo;

  ctx->pc = regs->cp0_epc;
#else
# error Unsupported architecture
#endif
}

static void
gum_unparse_gp_regs (const GumCpuContext * ctx,
                     GumGPRegs * regs)
{
#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
  regs->eip = ctx->eip;

  regs->edi = ctx->edi;
  regs->esi = ctx->esi;
  regs->ebp = ctx->ebp;
  regs->esp = ctx->esp;
  regs->ebx = ctx->ebx;
  regs->edx = ctx->edx;
  regs->ecx = ctx->ecx;
  regs->eax = ctx->eax;
#elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
  regs->rip = ctx->rip;

  regs->r15 = ctx->r15;
  regs->r14 = ctx->r14;
  regs->r13 = ctx->r13;
  regs->r12 = ctx->r12;
  regs->r11 = ctx->r11;
  regs->r10 = ctx->r10;
  regs->r9 = ctx->r9;
  regs->r8 = ctx->r8;

  regs->rdi = ctx->rdi;
  regs->rsi = ctx->rsi;
  regs->rbp = ctx->rbp;
  regs->rsp = ctx->rsp;
  regs->rbx = ctx->rbx;
  regs->rdx = ctx->rdx;
  regs->rcx = ctx->rcx;
  regs->rax = ctx->rax;
#elif defined (HAVE_ARM)
  gsize i;

  regs->ARM_pc = ctx->pc;
  regs->ARM_sp = ctx->sp;
  regs->ARM_cpsr = ctx->cpsr;

  regs->uregs[8] = ctx->r8;
  regs->uregs[9] = ctx->r9;
  regs->uregs[10] = ctx->r10;
  regs->uregs[11] = ctx->r11;
  regs->uregs[12] = ctx->r12;

  for (i = 0; i != G_N_ELEMENTS (ctx->r); i++)
    regs->uregs[i] = ctx->r[i];
  regs->ARM_lr = ctx->lr;
#elif defined (HAVE_ARM64)
  gsize i;

  regs->pc = ctx->pc;
  regs->sp = ctx->sp;

  for (i = 0; i != G_N_ELEMENTS (ctx->x); i++)
    regs->regs[i] = ctx->x[i];
  regs->regs[29] = ctx->fp;
  regs->regs[30] = ctx->lr;
#elif defined (HAVE_MIPS)
  regs->regs[1] = ctx->at;

  regs->regs[2] = ctx->v0;
  regs->regs[3] = ctx->v1;

  regs->regs[4] = ctx->a0;
  regs->regs[5] = ctx->a1;
  regs->regs[6] = ctx->a2;
  regs->regs[7] = ctx->a3;

  regs->regs[8] = ctx->t0;
  regs->regs[9] = ctx->t1;
  regs->regs[10] = ctx->t2;
  regs->regs[11] = ctx->t3;
  regs->regs[12] = ctx->t4;
  regs->regs[13] = ctx->t5;
  regs->regs[14] = ctx->t6;
  regs->regs[15] = ctx->t7;

  regs->regs[16] = ctx->s0;
  regs->regs[17] = ctx->s1;
  regs->regs[18] = ctx->s2;
  regs->regs[19] = ctx->s3;
  regs->regs[20] = ctx->s4;
  regs->regs[21] = ctx->s5;
  regs->regs[22] = ctx->s6;
  regs->regs[23] = ctx->s7;

  regs->regs[24] = ctx->t8;
  regs->regs[25] = ctx->t9;

  regs->regs[26] = ctx->k0;
  regs->regs[27] = ctx->k1;

  regs->regs[28] = ctx->gp;
  regs->regs[29] = ctx->sp;
  regs->regs[30] = ctx->fp;

  regs->regs[31] = ctx->ra;

  regs->hi = ctx->hi;
  regs->lo = ctx->lo;

  regs->cp0_epc = ctx->pc;
#else
# error Unsupported architecture
#endif
}

static GumPageProtection
gum_page_protection_from_proc_perms_string (const gchar * perms)
{
  GumPageProtection prot = GUM_PAGE_NO_ACCESS;

  if (perms[0] == 'r')
    prot |= GUM_PAGE_READ;
  if (perms[1] == 'w')
    prot |= GUM_PAGE_WRITE;
  if (perms[2] == 'x')
    prot |= GUM_PAGE_EXECUTE;

  return prot;
}

static gssize
gum_get_regs (pid_t pid,
              guint type,
              gpointer data,
              gsize * size)
{
  if (gum_is_regset_supported)
  {
    struct iovec io = {
      .iov_base = data,
      .iov_len = *size
    };
    gssize ret = gum_libc_ptrace (PTRACE_GETREGSET, pid,
        GUINT_TO_POINTER (type), &io);
    if (ret >= 0)
    {
      *size = io.iov_len;
      return ret;
    }
    if (ret == -EPERM || ret == -ESRCH)
      return ret;
    gum_is_regset_supported = FALSE;
  }

  return gum_libc_ptrace (PTRACE_GETREGS, pid, NULL, data);
}

static gssize
gum_set_regs (pid_t pid,
              guint type,
              gconstpointer data,
              gsize size)
{
  if (gum_is_regset_supported)
  {
    struct iovec io = {
      .iov_base = (void *) data,
      .iov_len = size
    };
    gssize ret = gum_libc_ptrace (PTRACE_SETREGSET, pid,
        GUINT_TO_POINTER (type), &io);
    if (ret >= 0)
      return ret;
    if (ret == -EPERM || ret == -ESRCH)
      return ret;
    gum_is_regset_supported = FALSE;
  }

  return gum_libc_ptrace (PTRACE_SETREGS, pid, NULL, (gpointer) data);
}

const GumLinuxPThreadSpec *
gum_linux_query_pthread_spec (void)
{
  static GumLinuxPThreadSpec spec;
  static gsize initialized = FALSE;

  if (g_once_init_enter (&initialized))
  {
    GumModule * libc;

    libc = gum_process_get_libc_module ();
    spec.set_name = GSIZE_TO_POINTER (gum_module_find_export_by_name (libc,
          "pthread_setname_np"));

    if (!gum_detect_pthread_internals (&spec))
      g_error ("Unsupported Linux system; please file a bug");

    g_once_init_leave (&initialized, TRUE);
  }

  return &spec;
}

#if defined (HAVE_GLIBC)

void
gum_linux_lock_pthread_list (const GumLinuxPThreadSpec * spec)
{
  if (spec->stack_lock != NULL)
    glibc_lock_acquire (spec->stack_lock);
}

void
gum_linux_unlock_pthread_list (const GumLinuxPThreadSpec * spec)
{
  if (spec->stack_lock != NULL)
    glibc_lock_release (spec->stack_lock);
}

static gboolean
gum_detect_pthread_internals (GumLinuxPThreadSpec * spec)
{
  guint tries;
  gboolean found_list_head = FALSE;
  gboolean found_lock = FALSE;

  /*
   * We create two threads in quick succession in the hopes that they will be
   * placed adjacent to each other in the thread list. There is a chance that
   * the application may create a thread of its own during the interval and
   * hence a race condition. Therefore we will retry a few times to find the
   * list head.
   */
  for (tries = 0; tries != GUM_MAX_FIND_LIST_HEAD_ATTEMPTS; tries++)
  {
    if (gum_linux_find_list_head (spec))
    {
      found_list_head = TRUE;
      break;
    }
  }
  if (!found_list_head)
    return FALSE;

  if (!gum_linux_find_start_offsets (spec))
    return FALSE;

  if (!gum_linux_find_tid_offset (spec))
    return FALSE;

  /*
   * The two list anchors are global addresses in libc, but a transient
   * just-exited thread can momentarily masquerade as the anchor during the
   * walk, yielding a wrong-but-self-consistent result. gum_linux_find_lock
   * cross-checks the two anchors against the expected globals layout, so we
   * re-detect until they agree.
   */
  for (tries = 0; tries != GUM_MAX_FIND_LIST_ANCHOR_ATTEMPTS; tries++)
  {
    if (gum_linux_find_list_anchor (spec, TRUE) &&
        gum_linux_find_list_anchor (spec, FALSE) &&
        gum_linux_find_lock (spec))
    {
      found_lock = TRUE;
      break;
    }
  }
  if (!found_lock)
    return FALSE;

  if (!gum_linux_find_start_impl (spec))
    return FALSE;

  spec->terminate_impl = GSIZE_TO_POINTER (gum_module_find_export_by_name (
      gum_process_get_libc_module (), "__call_tls_dtors"));
  if (spec->terminate_impl == NULL)
    return FALSE;

  return TRUE;
}

static gboolean
gum_linux_find_list_head (GumLinuxPThreadSpec * spec)
{
  gboolean success = FALSE;
  GumLinuxThreadCtx first, second;
  gboolean created_first = FALSE;
  gboolean created_second = FALSE;

  created_first = gum_linux_create_thread (&first, TRUE);
  if (!created_first)
    goto beach;

  created_second = gum_linux_create_thread (&second, TRUE);
  if (!created_second)
    goto beach;

  if (!gum_linux_find_list_head_offset (spec, &first, &second))
    goto beach;

  success = TRUE;

beach:
  if (created_second)
  {
    if (!gum_linux_dispose_thread (&second))
      success = FALSE;
  }

  if (created_first)
  {
    if (!gum_linux_dispose_thread (&first))
      success = FALSE;
  }

  return success;
}

static gboolean
gum_linux_find_list_head_offset (GumLinuxPThreadSpec * spec,
                                 GumLinuxThreadCtx * first,
                                 GumLinuxThreadCtx * second)
{
  gboolean success = FALSE;
  gboolean found_flink = FALSE;
  gsize offset;
  gpointer * candidate_address, expected_value;
  guint8 * candidate_data;
  gsize bytes_read;

  /*
   * Threads are added to the list head. So the second thread's flink, should
   * point to the first thread's list head.
   */
  for (offset = 0; offset < GUM_MAX_PTHREAD_SIZE; offset += sizeof (gpointer))
  {
    candidate_address = GSIZE_TO_POINTER (second->thread) + offset;
    expected_value = GSIZE_TO_POINTER (first->thread) + offset;

    candidate_data = gum_memory_read (candidate_address, sizeof (gpointer),
        &bytes_read);
    if (candidate_data == NULL || bytes_read != sizeof (gpointer))
      goto beach;

    found_flink = *(gpointer *) candidate_data == expected_value;

    g_free (candidate_data);
    candidate_data = NULL;

    if (found_flink)
    {
      spec->flink_offset = offset;
      break;
    }
  }
  if (!found_flink)
    goto beach;

  /*
   * The first thread's blink, should point to the second thread's list head.
   * And the blink should immediately follow the flink.
   */
  candidate_address = GSIZE_TO_POINTER (first->thread) + spec->flink_offset +
      sizeof (gpointer);

  expected_value = GSIZE_TO_POINTER (second->thread) + spec->flink_offset;

  candidate_data = gum_memory_read (candidate_address, sizeof (gpointer),
      &bytes_read);
  if (candidate_data == NULL || bytes_read != sizeof (gpointer))
    goto beach;

  if (*(gpointer **) candidate_data == expected_value)
  {
    success = TRUE;
    spec->blink_offset = spec->flink_offset + sizeof (gpointer);
  }

beach:
  g_free (candidate_data);

  return success;
}

static gboolean
gum_linux_find_start_offsets (GumLinuxPThreadSpec * spec)
{
  gboolean success = FALSE;
  gboolean created_thread;
  GumLinuxThreadCtx ctx;
  gsize offset;
  gboolean found_start_routine = FALSE;
  gboolean found_start_param = FALSE;

  created_thread = gum_linux_create_thread (&ctx, TRUE);
  if (!created_thread)
    goto beach;

  for (offset = 0; offset < GUM_MAX_PTHREAD_SIZE; offset += sizeof (gpointer))
  {
    gpointer * candidate_address;
    guint8 * candidate_data;
    gsize bytes_read;
    gpointer value;

    candidate_address = GSIZE_TO_POINTER (ctx.thread) + offset;

    candidate_data = gum_memory_read (candidate_address, sizeof (gpointer),
        &bytes_read);
    if (candidate_data == NULL || bytes_read != sizeof (gpointer))
      goto beach;

    value = *((gpointer *) candidate_data);

    g_free (candidate_data);

    if (value == gum_linux_thread_proc)
    {
      found_start_routine = TRUE;
      spec->start_routine_offset = offset;
    }

    if (value == &ctx)
    {
      found_start_param = TRUE;
      spec->start_parameter_offset = offset;
    }

    if (found_start_routine && found_start_param)
      break;
  }

  if (!found_start_routine || !found_start_param)
    goto beach;

  success = TRUE;

beach:
  if (created_thread)
  {
    if (!gum_linux_dispose_thread (&ctx))
      success = FALSE;
  }

  return success;
}

static gboolean
gum_linux_find_tid_offset (GumLinuxPThreadSpec * spec)
{
  gboolean success = FALSE;
  gboolean created_thread;
  GumLinuxThreadCtx ctx;
  gsize offset;
  guint matches = 0;
  gboolean found_tid_offset = FALSE;

  created_thread = gum_linux_create_thread (&ctx, TRUE);
  if (!created_thread)
    goto beach;

  for (offset = 0; offset < GUM_MAX_PTHREAD_SIZE; offset += sizeof (gint))
  {
    gpointer * candidate_address;
    guint8 * candidate_data;
    gsize bytes_read;
    gint value;

    candidate_address = GSIZE_TO_POINTER (ctx.thread) + offset;

    candidate_data = gum_memory_read (candidate_address, sizeof (gint),
        &bytes_read);
    if (candidate_data == NULL || bytes_read != sizeof (gint))
      goto beach;

    value = *((gint *) candidate_data);

    g_free (candidate_data);

    if (value == ctx.tid)
    {
      gsize i;

      /*
       * The TID is quite small, so there is the chance of a false positive,
       * create some more threads and check it is right.
       */
      for (i = 0; i != GUM_TID_CHECK_TIMES; i++)
      {
        gboolean match;

        if (!gum_linux_check_thread_offset (offset, &match))
          goto beach;

        if (!match)
          break;

        matches++;
      }

      if (matches == GUM_TID_CHECK_TIMES)
      {
        found_tid_offset = TRUE;
        spec->tid_offset = offset;
        break;
      }
    }
  }

  if (!found_tid_offset)
    goto beach;

  success = TRUE;

beach:
  if (created_thread)
  {
    if (!gum_linux_dispose_thread (&ctx))
      success = FALSE;
  }

  return success;
}

static gboolean
gum_linux_check_thread_offset (gsize offset,
                               gboolean * match)
{
  gboolean success = FALSE;
  gboolean created_thread;
  GumLinuxThreadCtx ctx;
  gpointer * tid_addr;
  guint8 * tid_data;
  gsize bytes_read;
  gint tid;

  created_thread = gum_linux_create_thread (&ctx, TRUE);
  if (!created_thread)
    goto beach;

  tid_addr = GSIZE_TO_POINTER (ctx.thread) + offset;

  tid_data = gum_memory_read (tid_addr, sizeof (gint), &bytes_read);
  if (tid_data == NULL || bytes_read != sizeof (gint))
    goto beach;

  tid = *((gint *) tid_data);

  g_free (tid_data);

  *match = tid == ctx.tid;

  success = TRUE;

beach:
  if (created_thread)
  {
    if (!gum_linux_dispose_thread (&ctx))
      success = FALSE;
  }

  return success;
}

static gboolean
gum_linux_find_list_anchor (GumLinuxPThreadSpec * spec,
                            gboolean custom_stack)
{
  gboolean success = FALSE;
  gboolean created_thread;
  GumLinuxThreadCtx ctx;
  pthread_t current, next, prev;
  guint num_threads = 0;
  gpointer anchor = NULL;

  created_thread = gum_linux_create_thread (&ctx, custom_stack);
  if (!created_thread)
    goto beach;

  current = ctx.thread;

  do
  {
    gpointer current_list_head;
    GumThreadId tid;
    gboolean is_valid_anchor;

    current_list_head = GSIZE_TO_POINTER (current) + spec->flink_offset;

    /*
     * If we get the TID from our thread and check if exists then if it doesn't
     * we have probably found the list anchor. But we check we don't find more
     * than one that fails.
     */

    tid = gum_linux_query_pthread_tid (current, spec);
    if (gum_process_has_thread (tid))
    {
      /* If the TID is of a valid thread, then this is not the list anchor. */
      is_valid_anchor = FALSE;
    }
    else if (tid == 0)
    {
      GumTestByAddressContext tba;

      /*
       * If our TID is zero, then this can indicate a thread that has exited,
       * but not yet been joined. So in this case, we will perform an additional
       * check that our list anchor is in a mapping backed by a file. It should
       * be in the globals of libc.so or libpthread.so, whereas our pthreads
       * themselves live at the base of the thread stacks. This should avoid any
       * false positives. We don't do this check for every candidate as it
       * requires us to walk the memory map which may add a lot of overhead.
       */
      gum_test_by_address_context_init (&tba, GUM_ADDRESS (current_list_head));

      gum_process_enumerate_ranges (GUM_PAGE_NO_ACCESS,
          (GumFoundRangeFunc) gum_test_pthread_globals_if_containing_address,
          &tba);
      if (!tba.found)
        goto beach;

      is_valid_anchor = tba.is_pthread_globals;

      gum_test_by_address_context_free (&tba);
    }
    else
    {
      is_valid_anchor = TRUE;
    }

    if (is_valid_anchor)
    {
      if (anchor == NULL)
        anchor = current_list_head;
      else
        goto beach;
    }

    if (!gum_linux_thread_read_flink (spec, current, &next))
      goto beach;

    if (!gum_linux_thread_read_blink (spec, next, &prev))
      goto beach;

    if (prev != current)
      goto beach;

    current = next;
    num_threads++;

    /*
     * If we find more than the maximum expected number of threads without
     * getting back to the start of the list, then terminate to avoid an
     * infinite loop.
     */
    if (num_threads == GUM_LINUX_MAX_THREADS)
      goto beach;
  }
  while (current != ctx.thread);

  if (custom_stack)
    spec->stack_user = anchor;
  else
    spec->stack_used = anchor;

  success = TRUE;

beach:
  if (created_thread)
  {
    if (!gum_linux_dispose_thread (&ctx))
      success = FALSE;
  }

  return success;
}

static void
gum_test_by_address_context_init (GumTestByAddressContext * ctx,
                                  GumAddress address)
{
  ctx->address = address;
  ctx->found = FALSE;
  ctx->is_pthread_globals = FALSE;
  ctx->last_file = NULL;
}

static void
gum_test_by_address_context_free (GumTestByAddressContext * ctx)
{
  g_free (ctx->last_file);
}

static gboolean
gum_test_pthread_globals_if_containing_address (const GumRangeDetails * details,
                                                GumTestByAddressContext * fc)
{
  /*
   * Our list anchors are globals within the library containing the pthreads
   * implementatinon. This is typically either within libc.so itself, or a
   * standalone libpthreads.so depending on the configuration of libc.
   *
   * Since our global may reside either within the .data or .bss sections of the
   * library, we check for both. In the case of the .data section, we can check
   * the filename of the mapping to see if it's a pthreads library (this will be
   * a private mapping of the library such that modifications are not written
   * back to the original file).
   *
   * If our global is within the .bss, then this will typically be an anonymous
   * mapping. But should immediately follow the mappings of the library in the
   * address map. We therefore cache the filename of the previous mapping at
   * each iteration, such that we can check for this case too.
   */
  static const gchar * libs[] = { "/libc", "/libpthread" };

  if (GUM_MEMORY_RANGE_INCLUDES (details->range, fc->address))
  {
    guint i;

    fc->found = TRUE;

    for (i = 0; i != G_N_ELEMENTS (libs) && !fc->is_pthread_globals; i++)
    {
      gboolean current_mapping_is_a_pthreads_library;
      gboolean previous_mapping_was_a_pthreads_library;

      current_mapping_is_a_pthreads_library =
          details->file != NULL &&
          details->file->path != NULL &&
          strstr (details->file->path, libs[i]) != NULL;
      if (current_mapping_is_a_pthreads_library)
        fc->is_pthread_globals = TRUE;

      previous_mapping_was_a_pthreads_library =
          fc->last_file != NULL &&
          strstr (fc->last_file, libs[i]) != NULL;
      if (previous_mapping_was_a_pthreads_library)
        fc->is_pthread_globals = TRUE;
    }
  }
  else
  {
    g_free (fc->last_file);

    if (details->file != NULL)
      fc->last_file = g_strdup (details->file->path);
    else
      fc->last_file = NULL;
  }

  return !fc->found;
}

static gboolean
gum_linux_find_lock (GumLinuxPThreadSpec * spec)
{
  guint major;
  guint minor;
  gpointer addr_from_stack_used;
  gpointer addr_from_stack_user;

  if (!gum_linux_get_libc_version (&major, &minor))
    return FALSE;

  /*
   * Prior to glibc 2.33, the location of the lock is unpredicatable, so we
   * will just have to make do without and accept the possibility of a
   * potential race.
   */
  if (major < 2 || (major == 2 && minor < 33))
    return TRUE;

  if (spec->stack_used == NULL || spec->stack_user == NULL)
    return FALSE;

  addr_from_stack_used = ((gpointer) spec->stack_used) -
      G_STRUCT_OFFSET (GumLinuxGlobalsFragment, _dl_stack_used);
  addr_from_stack_user = ((gpointer) spec->stack_user) -
      G_STRUCT_OFFSET (GumLinuxGlobalsFragment, _dl_stack_user);

  if (addr_from_stack_used != addr_from_stack_user)
    return FALSE;

  spec->stack_lock = (GumGlibcLock *) (addr_from_stack_used +
      G_STRUCT_OFFSET (GumLinuxGlobalsFragment, _dl_stack_cache_lock));

  return TRUE;
}

static gboolean
gum_linux_get_libc_version (guint * major,
                            guint * minor)
{
  gboolean success = FALSE;
  const gchar * version;
  gchar ** parts, * end;
  guint64 maj, min;

  *major = 0;
  *minor = 0;

  version = gnu_get_libc_version ();

  parts = g_strsplit (version, ".", 2);
  if (parts[0] == NULL || parts[1] == NULL)
    goto beach;

  maj = g_ascii_strtoull (parts[0], &end, 10);
  if (*end != '\0')
    goto beach;

  min = g_ascii_strtoull (parts[1], &end, 10);
  if (*end != '\0')
    goto beach;

  *major = maj;
  *minor = min;

  success = TRUE;

beach:
  g_strfreev (parts);

  return success;
}

static gboolean
gum_linux_find_start_impl (GumLinuxPThreadSpec * spec)
{
  gboolean success = FALSE;
  gboolean created_thread;
  GumLinuxThreadCtx ctx;
#ifdef HAVE_ARM
  gboolean is_thumb;
#endif
  csh capstone;
  cs_insn * insn = NULL;
  const uint8_t * code;
  gsize i;
  gboolean found_start = FALSE;

  created_thread = gum_linux_create_thread (&ctx, TRUE);
  if (!created_thread)
    goto beach;

  gum_cs_arch_register_native ();
#ifdef HAVE_ARM
  is_thumb = (GPOINTER_TO_SIZE (ctx.ret) & 1) != 0;
  cs_open (GUM_DEFAULT_CS_ARCH,
      is_thumb
        ? CS_MODE_THUMB | CS_MODE_V8 | GUM_DEFAULT_CS_ENDIAN
        : GUM_DEFAULT_CS_MODE,
      &capstone);
  code = GSIZE_TO_POINTER (GPOINTER_TO_SIZE (ctx.ret) & ~1);
#else
  cs_open (GUM_DEFAULT_CS_ARCH, GUM_DEFAULT_CS_MODE, &capstone);
  code = ctx.ret;
#endif
  cs_option (capstone, CS_OPT_DETAIL, CS_OPT_ON);
  cs_option (capstone, CS_OPT_SKIPDATA, CS_OPT_ON);

  for (i = 0; i != GUM_MAX_INSTRUCTION_SIZE; i++)
  {
    if (cs_disasm (capstone, code - i, i, GPOINTER_TO_SIZE (code - i), 1, &insn)
        == 0)
    {
      continue;
    }

    if (insn->size != i)
      continue;

    if (gum_linux_is_call (insn))
    {
      found_start = TRUE;
      spec->start_impl = GSIZE_TO_POINTER (insn->address);
      break;
    }

    cs_free (insn, 1);
    insn = NULL;
  }
  if (!found_start)
    goto beach;

#if defined (HAVE_I386)
  {
    gpointer hook = gum_linux_find_dominating_start_hook (spec->start_impl);
    if (hook != NULL)
      spec->start_impl = hook;
  }
#endif

  success = TRUE;

beach:
  if (insn != NULL)
    cs_free (insn, 1);

  cs_close (&capstone);

  if (created_thread)
  {
    if (!gum_linux_dispose_thread (&ctx))
      success = FALSE;
  }

  return success;
}

#if defined (HAVE_I386)

#define GUM_START_HOOK_REDIRECT_SIZE 5

typedef struct _GumDominatingHookSite GumDominatingHookSite;

struct _GumDominatingHookSite
{
  GumControlFlowGraph * cfg;
  gconstpointer call_site;
  gpointer hook;
};

static gpointer
gum_linux_find_dominating_start_hook (gpointer call_site)
{
  GumMemoryRange function;
  GumControlFlowGraph * cfg;
  GumDominatingHookSite ctx;

  if (!gum_process_find_function_range (call_site, &function))
    return NULL;

  cfg = gum_control_flow_graph_new_for_function (
      GSIZE_TO_POINTER (function.base_address));

  ctx.cfg = cfg;
  ctx.call_site = call_site;
  ctx.hook = NULL;

  gum_control_flow_graph_enumerate_dominating_sites (cfg, call_site,
      gum_linux_try_use_dominating_site, &ctx);

  gum_control_flow_graph_free (cfg);

  return ctx.hook;
}

static gboolean
gum_linux_try_use_dominating_site (gconstpointer site,
                                   gsize capacity,
                                   gpointer user_data)
{
  GumDominatingHookSite * ctx = user_data;
  const cs_insn * insn;
  const cs_detail * detail;
  uint8_t i;

  if (capacity < GUM_START_HOOK_REDIRECT_SIZE)
    return TRUE;

  if ((const guint8 *) site + GUM_START_HOOK_REDIRECT_SIZE >
      (const guint8 *) ctx->call_site)
    return TRUE;

  insn = gum_control_flow_graph_find_instruction_containing (ctx->cfg, site);
  detail = insn->detail;
  for (i = 0; i != detail->groups_count; i++)
  {
    uint8_t group = detail->groups[i];
    if (group == CS_GRP_JUMP || group == CS_GRP_CALL || group == CS_GRP_RET)
      return TRUE;
  }

  if (!gum_x86_relocator_can_relocate ((gpointer) site,
        GUM_START_HOOK_REDIRECT_SIZE, GUM_SCENARIO_ONLINE, NULL))
    return TRUE;

  ctx->hook = (gpointer) site;
  return FALSE;
}

#endif

static gboolean
gum_linux_is_call (cs_insn * insn)
{
  switch (insn->id)
  {
#if defined (HAVE_I386)
    case X86_INS_CALL:
      return TRUE;
#elif defined (HAVE_ARM)
    case ARM_INS_BL:
    case ARM_INS_BLX:
      return TRUE;
#elif defined (HAVE_ARM64)
    case ARM64_INS_BL:
    case ARM64_INS_BLR:
    case ARM64_INS_BLRAA:
    case ARM64_INS_BLRAAZ:
    case ARM64_INS_BLRAB:
    case ARM64_INS_BLRABZ:
      return TRUE;
#elif defined (HAVE_MIPS)
    case MIPS_INS_JAL:
    case MIPS_INS_JALR:
      return TRUE;
#else
# error FIXME
#endif
    default:
      break;
  }

  return FALSE;
}

static gboolean
gum_linux_create_thread (GumLinuxThreadCtx * ctx,
                         gboolean custom_stack)
{
  pthread_attr_t attr;
  guint page_size;
  guint num_pages;

  if (pthread_attr_init (&attr) != 0)
    return FALSE;

  if (custom_stack)
  {
    page_size = gum_query_page_size ();

    num_pages = GUM_THREAD_STACK_SIZE / page_size;
    if (GUM_THREAD_STACK_SIZE % page_size != 0)
      num_pages++;

    ctx->stack = gum_memory_allocate (NULL, num_pages * page_size, page_size,
        GUM_PAGE_RW);
    if (ctx->stack == NULL)
      return FALSE;

    if (pthread_attr_setstack (&attr, ctx->stack, GUM_THREAD_STACK_SIZE) != 0)
      return FALSE;
  }
  else
  {
    ctx->stack = NULL;
  }

  g_mutex_init (&ctx->mutex);
  g_cond_init (&ctx->cond);
  ctx->start = FALSE;
  ctx->exit = FALSE;

  if (pthread_create (&ctx->thread, &attr, gum_linux_thread_proc, ctx) != 0)
    return FALSE;

  g_mutex_lock (&ctx->mutex);
  while (!ctx->start)
    g_cond_wait (&ctx->cond, &ctx->mutex);
  g_mutex_unlock (&ctx->mutex);

  return TRUE;
}

static gboolean
gum_linux_dispose_thread (GumLinuxThreadCtx * ctx)
{
  g_mutex_lock (&ctx->mutex);
  ctx->exit = TRUE;
  g_cond_signal (&ctx->cond);
  g_mutex_unlock (&ctx->mutex);

  if (pthread_join (ctx->thread, NULL) != 0)
    return FALSE;

  if (ctx->stack != NULL)
    gum_memory_free (ctx->stack, GUM_THREAD_STACK_SIZE);

  return TRUE;
}

static gpointer
gum_linux_thread_proc (gpointer param)
{
  GumLinuxThreadCtx * ctx = param;

  ctx->tid = gum_process_get_current_thread_id ();
  ctx->ret = __builtin_extract_return_addr (__builtin_return_address (0));

  g_mutex_lock (&ctx->mutex);

  ctx->start = TRUE;
  g_cond_signal (&ctx->cond);

  while (!ctx->exit)
    g_cond_wait (&ctx->cond, &ctx->mutex);

  g_mutex_unlock (&ctx->mutex);

  return NULL;
}

static gboolean
gum_linux_thread_read_flink (const GumLinuxPThreadSpec * spec,
                             pthread_t current,
                             pthread_t * next)
{
  gboolean success = FALSE;
  gpointer flink;
  guint8 * data;
  gsize bytes_read;
  gpointer next_flink;
  pthread_t thread;

  flink = GSIZE_TO_POINTER (current) + spec->flink_offset;

  data = gum_memory_read (flink, sizeof (gpointer), &bytes_read);
  if (data == NULL || bytes_read != sizeof (gpointer))
    goto beach;

  next_flink = *(gpointer *) data;
  thread = (pthread_t) (next_flink - spec->flink_offset);
  if (thread == current)
    goto beach;

  *next = thread;
  success = TRUE;

beach:
  g_free (data);

  return success;
}

static gboolean
gum_linux_thread_read_blink (const GumLinuxPThreadSpec * spec,
                             pthread_t current,
                             pthread_t * prev)
{
  gboolean success = FALSE;
  gpointer blink;
  guint8 * data;
  gsize bytes_read;
  gpointer next_blink;
  pthread_t thread;

  blink = GSIZE_TO_POINTER (current) + spec->blink_offset;

  data = gum_memory_read (blink, sizeof (gpointer), &bytes_read);
  if (data == NULL || bytes_read != sizeof (gpointer))
    goto beach;

  next_blink = *(gpointer *) data;
  /*
   * Our blink points to the start of the list head (the flink comes first),
   * not the blink field within it.
   */
  thread = (pthread_t) (next_blink - spec->flink_offset);
  if (thread == current)
    goto beach;

  *prev = thread;
  success = TRUE;

beach:
  g_free (data);

  return success;
}

static void
glibc_lock_acquire (GumGlibcLock * lock)
{
  if (!__sync_bool_compare_and_swap (lock, 0, 1))
  {
    if (__atomic_load_n (lock, __ATOMIC_RELAXED) == 2)
      goto wait;

    while (__atomic_exchange_n (lock, 2, __ATOMIC_ACQUIRE) != 0)
    {
wait:
      syscall (SYS_futex, lock, FUTEX_WAIT_PRIVATE, 2, NULL);
    }
  }
}

static void
glibc_lock_release (GumGlibcLock * lock)
{
  if (__atomic_exchange_n (lock, 0, __ATOMIC_RELEASE) != 1)
    syscall (SYS_futex, lock, FUTEX_WAKE_PRIVATE, 1);
}

#elif defined (HAVE_MUSL)

void
gum_linux_lock_pthread_list (const GumLinuxPThreadSpec * spec)
{
  spec->tl_lock ();
}

void
gum_linux_unlock_pthread_list (const GumLinuxPThreadSpec * spec)
{
  spec->tl_unlock ();
}

static gboolean
gum_detect_pthread_internals (GumLinuxPThreadSpec * spec)
{
  int main_tid;
  GumLinuxPThread * cur;
  GumModule * libc;
  gpointer create_prologue;
#ifdef HAVE_ARM
  gboolean is_thumb;
#endif
  csh capstone;
  cs_insn * insn;
  const uint8_t * code;
  size_t size;
  uint64_t addr;

  main_tid = getpid ();
  for (cur = (GumLinuxPThread *) pthread_self ();
      cur->tid != main_tid;
      cur = cur->next)
  {
  }
  spec->main_thread = cur;

  libc = gum_process_get_libc_module ();

  create_prologue = GSIZE_TO_POINTER (gum_module_find_export_by_name (libc,
        "pthread_create"));

  gum_cs_arch_register_native ();
#ifdef HAVE_ARM
  is_thumb = (GPOINTER_TO_SIZE (create_prologue) & 1) != 0;
  cs_open (GUM_DEFAULT_CS_ARCH,
      is_thumb
        ? CS_MODE_THUMB | CS_MODE_V8 | GUM_DEFAULT_CS_ENDIAN
        : GUM_DEFAULT_CS_MODE,
      &capstone);
  code = GSIZE_TO_POINTER (GPOINTER_TO_SIZE (create_prologue) & ~1);
#else
  cs_open (GUM_DEFAULT_CS_ARCH, GUM_DEFAULT_CS_MODE, &capstone);
  code = create_prologue;
#endif
  cs_option (capstone, CS_OPT_DETAIL, CS_OPT_ON);
  cs_option (capstone, CS_OPT_SKIPDATA, CS_OPT_ON);

  insn = cs_malloc (capstone);

  code += gum_interceptor_detect_hook_size (code, capstone, insn);
  size = 1024;
  addr = GPOINTER_TO_SIZE (code);

#if defined (HAVE_I386)
  {
    gconstpointer btr_end = NULL;
    GPtrArray * potential_start_funcs;

    potential_start_funcs = g_ptr_array_sized_new (4);

    while ((spec->tl_lock == NULL || spec->start_impl == NULL) &&
        cs_disasm_iter (capstone, &code, &size, &addr, insn))
    {
      const cs_x86 * x86 = &insn->detail->x86;

      switch (insn->id)
      {
        case X86_INS_BTR:
          btr_end = code;
          break;
        case X86_INS_CALL:
        {
          const cs_x86_op * target = &x86->operands[0];
          if (code - insn->size == btr_end &&
              target->type == X86_OP_IMM)
          {
            spec->tl_lock = GSIZE_TO_POINTER (target->imm);
          }

          break;
        }
        case X86_INS_LEA:
        {
          const cs_x86_op * src = &x86->operands[1];

          if (src->mem.base == X86_REG_RIP)
          {
            g_ptr_array_add (potential_start_funcs,
                GSIZE_TO_POINTER (addr + src->mem.disp));
          }

          break;
        }
        case X86_INS_CMOVE:
          if (potential_start_funcs->len >= 2)
          {
            spec->start_impl = g_ptr_array_index (potential_start_funcs,
                potential_start_funcs->len - 1);
            spec->start_c11_impl = g_ptr_array_index (potential_start_funcs,
                potential_start_funcs->len - 2);
          }

          break;
        default:
          break;
      }
    }

    g_ptr_array_unref (potential_start_funcs);

    if (spec->tl_lock == NULL)
      goto beach;

    code = (gpointer) spec->tl_lock;
    size = 512;
    addr = GPOINTER_TO_SIZE (code);
    while (spec->tl_unlock == NULL &&
        cs_disasm_iter (capstone, &code, &size, &addr, insn))
    {
      if (insn->id == X86_INS_RET)
        spec->tl_unlock = (gpointer) code;
    }
  }
#elif defined (HAVE_ARM)
  {
    gboolean expecting_tl_lock_call = FALSE;

    while ((spec->tl_lock == NULL || spec->start_impl == NULL) &&
        cs_disasm_iter (capstone, &code, &size, &addr, insn))
    {
      const cs_arm * arm = &insn->detail->arm;

      switch (insn->id)
      {
        case ARM_INS_BIC:
        {
          const cs_arm_op * val = &arm->operands[2];

          if (spec->tl_lock == NULL &&
              val->type == ARM_OP_IMM &&
              val->imm == 1)
          {
            expecting_tl_lock_call = TRUE;
          }

          break;
        }
        case ARM_INS_BL:
        {
          const cs_arm_op * target = &arm->operands[0];

          if (expecting_tl_lock_call)
          {
            spec->tl_lock = GSIZE_TO_POINTER (target->imm | (is_thumb ? 1 : 0));

            expecting_tl_lock_call = FALSE;
          }

          break;
        }
        case ARM_INS_CMP:
        {
          const cs_arm_op * val = &arm->operands[1];

          if (spec->tl_lock != NULL &&
              val->type == ARM_OP_IMM &&
              val->imm == 0xffffffffU)
          {
            if (!cs_disasm_iter (capstone, &code, &size, &addr, insn))
              goto beach;
            if (insn->id != ARM_INS_B || arm->cc != ARM_CC_EQ)
              goto beach;

            spec->start_c11_impl = gum_parse_ldrpc (
                GSIZE_TO_POINTER (arm->operands[0].imm), capstone, insn);
            if (spec->start_c11_impl == NULL)
              goto beach;

            spec->start_impl = gum_parse_ldrpc (code, capstone, insn);
            if (spec->start_impl == NULL)
              goto beach;
          }
        }
        default:
          break;
      }
    }

    if (spec->tl_lock == NULL)
      goto beach;

    code = GSIZE_TO_POINTER (GPOINTER_TO_SIZE (spec->tl_lock) & ~1);
    size = 512;
    addr = GPOINTER_TO_SIZE (code);
    while (spec->tl_unlock == NULL &&
        cs_disasm_iter (capstone, &code, &size, &addr, insn))
    {
      if (insn->id == ARM_INS_POP)
      {
        while (cs_disasm_iter (capstone, &code, &size, &addr, insn))
        {
          if (insn->id == ARM_INS_LDR &&
              insn->detail->arm.operands[0].reg == ARM_REG_R2)
          {
            break;
          }
        }

        spec->tl_unlock =
            GSIZE_TO_POINTER ((addr - insn->size) | (is_thumb ? 1 : 0));
      }
    }
  }
#elif defined (HAVE_ARM64)
  {
    GHashTable * regvals;
    gboolean expecting_tl_lock_call = FALSE;

    regvals = g_hash_table_new_full (NULL, NULL, NULL, NULL);

    while ((spec->tl_lock == NULL || spec->start_impl == NULL) &&
        cs_disasm_iter (capstone, &code, &size, &addr, insn))
    {
      const cs_arm64 * arm64 = &insn->detail->arm64;

      switch (insn->id)
      {
        case ARM64_INS_AND:
        {
          const cs_arm64_op * val = &arm64->operands[2];

          if (spec->tl_lock == NULL &&
              val->type == ARM64_OP_IMM &&
              val->imm == G_GUINT64_CONSTANT (0xfffffffeffffffff))
          {
            expecting_tl_lock_call = TRUE;
          }

          break;
        }
        case ARM64_INS_BL:
        {
          const cs_arm64_op * target = &arm64->operands[0];

          if (expecting_tl_lock_call)
          {
            spec->tl_lock = GSIZE_TO_POINTER (target->imm);

            expecting_tl_lock_call = FALSE;
          }

          break;
        }
        case ARM64_INS_ADRP:
        {
          const cs_arm64_op * dst = &arm64->operands[0];
          const cs_arm64_op * val = &arm64->operands[1];

          g_hash_table_insert (regvals, GUINT_TO_POINTER (dst->reg),
              GSIZE_TO_POINTER (val->imm));

          break;
        }
        case ARM64_INS_ADD:
        {
          const cs_arm64_op * dst = &arm64->operands[0];
          const cs_arm64_op * n = &arm64->operands[1];
          const cs_arm64_op * m = &arm64->operands[2];
          gsize old_val, new_val;

          old_val = GPOINTER_TO_SIZE (
              g_hash_table_lookup (regvals, GUINT_TO_POINTER (n->reg)));

          if (m->type == ARM64_OP_IMM)
          {
            new_val = old_val + m->imm;
          }
          else
          {
            new_val = old_val + GPOINTER_TO_SIZE (
                g_hash_table_lookup (regvals, GUINT_TO_POINTER (m->reg)));
          }

          g_hash_table_insert (regvals, GUINT_TO_POINTER (dst->reg),
              GSIZE_TO_POINTER (new_val));

          break;
        }
        case ARM64_INS_CSEL:
        {
          const cs_arm64_op * n = &arm64->operands[1];
          const cs_arm64_op * m = &arm64->operands[2];

          spec->start_impl =
              g_hash_table_lookup (regvals, GUINT_TO_POINTER (n->reg));
          spec->start_c11_impl =
              g_hash_table_lookup (regvals, GUINT_TO_POINTER (m->reg));

          break;
        }
        default:
          break;
      }
    }

    g_hash_table_unref (regvals);

    if (spec->tl_lock == NULL)
      goto beach;

    code = (gpointer) spec->tl_lock;
    size = 512;
    addr = GPOINTER_TO_SIZE (code);
    while (spec->tl_unlock == NULL &&
        cs_disasm_iter (capstone, &code, &size, &addr, insn))
    {
      if (insn->id == ARM64_INS_RET)
        spec->tl_unlock = (gpointer) code;
    }
  }
#else
# error FIXME
#endif

beach:
  cs_free (insn, 1);

  cs_close (&capstone);

  if (spec->tl_unlock == NULL || spec->start_impl == NULL)
    return FALSE;

  spec->terminate_impl = GSIZE_TO_POINTER (gum_module_find_export_by_name (libc,
        "pthread_exit"));

  return spec->terminate_impl != NULL;
}

# ifdef HAVE_ARM

static gpointer
gum_parse_ldrpc (const uint8_t * code,
                 csh capstone,
                 cs_insn * insn)
{
  const cs_arm * arm = &insn->detail->arm;
  size_t size;
  uint64_t addr;
  arm_reg dst_reg;
  const cs_arm_op * src;
  gsize pc, location;
  guint32 delta;
  const cs_arm_op * op1;

  size = 8;
  addr = GPOINTER_TO_SIZE (code);
  pc = addr + 4;
  if (!cs_disasm_iter (capstone, &code, &size, &addr, insn))
    return NULL;
  if (insn->id != ARM_INS_LDR)
    return NULL;
  dst_reg = arm->operands[0].reg;
  src = &arm->operands[1];
  if (src->mem.base != ARM_REG_PC)
    return NULL;
  location = (pc & ~(4 - 1)) + src->mem.disp;
  delta = *((gssize *) GSIZE_TO_POINTER (location));

  pc = addr + 4;
  if (!cs_disasm_iter (capstone, &code, &size, &addr, insn))
    return NULL;
  if (insn->id != ARM_INS_ADD || arm->op_count != 2)
    return NULL;
  if (arm->operands[0].reg != dst_reg)
    return NULL;
  op1 = &arm->operands[1];
  if (op1->type != ARM_OP_REG || op1->reg != ARM_REG_PC)
    return NULL;

  return GSIZE_TO_POINTER (pc + delta);
}

# endif

static GumMuslStartArgs *
gum_query_pthread_start_args (pthread_t thread,
                              const GumLinuxPThreadSpec * spec)
{
  GumLinuxPThread * pth = (GumLinuxPThread *) thread;
  guint8 * stack;

  if (pth == spec->main_thread)
    return NULL;

  stack = pth->stack;
  stack -= GPOINTER_TO_SIZE (stack) % sizeof (gpointer);
  return (GumMuslStartArgs *) (stack - sizeof (GumMuslStartArgs));
}

#elif defined (HAVE_ANDROID)

void
gum_linux_lock_pthread_list (const GumLinuxPThreadSpec * spec)
{
  pthread_rwlock_rdlock (spec->thread_list_lock);
}

void
gum_linux_unlock_pthread_list (const GumLinuxPThreadSpec * spec)
{
  pthread_rwlock_unlock (spec->thread_list_lock);
}

static gboolean
gum_detect_pthread_internals (GumLinuxPThreadSpec * spec)
{
  GumModule * libc;
  gpointer start_prologue;
#ifdef HAVE_ARM
  gboolean is_thumb;
#endif
  csh capstone;
  cs_insn * insn;
  const uint8_t * code;
  size_t size;
  uint64_t addr;

  libc = gum_process_get_libc_module ();

  spec->thread_list = GSIZE_TO_POINTER (gum_module_find_symbol_by_name (libc,
      "_ZL13g_thread_list"));
  spec->thread_list_lock = GSIZE_TO_POINTER (gum_module_find_symbol_by_name (
        libc, "_ZL18g_thread_list_lock"));
  if (spec->thread_list == NULL || spec->thread_list_lock == NULL)
    return FALSE;

  start_prologue = GSIZE_TO_POINTER (gum_find_function_symbol_by_prefix (libc,
        "_ZL15__pthread_startPv"));
  if (start_prologue == NULL)
    return FALSE;

  gum_cs_arch_register_native ();
#ifdef HAVE_ARM
  is_thumb = (GPOINTER_TO_SIZE (start_prologue) & 1) != 0;
  cs_open (GUM_DEFAULT_CS_ARCH,
      is_thumb
        ? CS_MODE_THUMB | CS_MODE_V8 | GUM_DEFAULT_CS_ENDIAN
        : GUM_DEFAULT_CS_MODE,
      &capstone);
  code = GSIZE_TO_POINTER (GPOINTER_TO_SIZE (start_prologue) & ~1);
#else
  cs_open (GUM_DEFAULT_CS_ARCH, GUM_DEFAULT_CS_MODE, &capstone);
  code = start_prologue;
#endif
  cs_option (capstone, CS_OPT_DETAIL, CS_OPT_ON);
  cs_option (capstone, CS_OPT_SKIPDATA, CS_OPT_ON);

  insn = cs_malloc (capstone);

  code += gum_interceptor_detect_hook_size (code, capstone, insn);
  size = 1024;
  addr = GPOINTER_TO_SIZE (code);

#if defined (HAVE_I386)
# if GLIB_SIZEOF_VOID_P == 4
#  define GUM_CS_XSP_REG X86_REG_ESP
#  define GUM_CS_XBP_REG X86_REG_EBP
# else
#  define GUM_CS_XSP_REG X86_REG_RSP
#  define GUM_CS_XBP_REG X86_REG_RBP
# endif
  {
    GArray * sizes;
    guint insn_index;
    guint mov_index = 0;
    gpointer mov_location = NULL;

    sizes = g_array_sized_new (FALSE, FALSE, sizeof (guint16), 32);

    for (insn_index = 0; spec->start_impl == NULL &&
        cs_disasm_iter (capstone, &code, &size, &addr, insn); insn_index++)
    {
      guint16 size = insn->size;
      const cs_x86 * x86 = &insn->detail->x86;

      g_array_append_val (sizes, size);

      switch (insn->id)
      {
        case X86_INS_MOV:
        {
          const cs_x86_op * src = &x86->operands[1];

          if (src->type == X86_OP_MEM &&
              src->mem.segment == X86_REG_INVALID &&
              src->mem.base != GUM_CS_XSP_REG &&
              src->mem.base != GUM_CS_XBP_REG &&
              src->mem.index == X86_REG_INVALID)
          {
            mov_index = insn_index;
            mov_location = (gpointer) (code - insn->size);
            spec->start_parameter_offset = src->mem.disp;
          }

          break;
        }
        case X86_INS_CALL:
        {
          const cs_x86_op * target = &x86->operands[0];

          if (target->type == X86_OP_MEM && mov_location != NULL)
          {
            guint hook_delta, i;
            gpointer hook_location;

            hook_delta = 0;
            for (i = mov_index - 2; i != mov_index; i++)
              hook_delta += g_array_index (sizes, guint16, i);

            hook_location = mov_location - hook_delta;

            spec->start_impl = hook_location;
            spec->start_routine_offset = target->mem.disp;
          }

          break;
        }
        default:
          break;
      }
    }

    g_array_unref (sizes);
  }
#elif defined (HAVE_ARM)
  {
    GArray * sizes;
    guint insn_index;
    guint ldrd_index = 0;
    gpointer ldrd_location = NULL;
    arm_reg func_reg = ARM_REG_INVALID;

    sizes = g_array_sized_new (FALSE, FALSE, sizeof (guint16), 32);

    for (insn_index = 0; spec->start_impl == NULL &&
        cs_disasm_iter (capstone, &code, &size, &addr, insn); insn_index++)
    {
      guint16 size = insn->size;
      const cs_arm * arm = &insn->detail->arm;

      g_array_append_val (sizes, size);

      switch (insn->id)
      {
        case ARM_INS_LDRD:
          ldrd_index = insn_index;
          ldrd_location = (gpointer) (code - insn->size);
          func_reg = arm->operands[0].reg;
          spec->start_routine_offset = arm->operands[2].mem.disp;
          spec->start_parameter_offset = spec->start_routine_offset + 4;
          break;
        case ARM_INS_BLX:
          if (arm->operands[0].type == ARM_OP_REG &&
              arm->operands[0].reg == func_reg)
          {
            guint hook_delta, i;
            gpointer hook_location;

            hook_delta = 0;
            for (i = ldrd_index - 4; i != ldrd_index; i++)
              hook_delta += g_array_index (sizes, guint16, i);

            hook_location = ldrd_location - hook_delta;

            spec->start_impl = is_thumb
                ? GSIZE_TO_POINTER (GPOINTER_TO_SIZE (hook_location) | 1)
                : hook_location;
          }
          break;
        default:
          break;
      }
    }

    g_array_unref (sizes);
  }
#elif defined (HAVE_ARM64)
  {
    gpointer ldp_location = NULL;
    arm64_reg func_reg = ARM64_REG_INVALID;

    while (spec->start_impl == NULL &&
        cs_disasm_iter (capstone, &code, &size, &addr, insn))
    {
      const cs_arm64 * arm64 = &insn->detail->arm64;

      switch (insn->id)
      {
        case ARM64_INS_LDP:
          ldp_location = (gpointer) (code - insn->size);
          func_reg = arm64->operands[0].reg;
          spec->start_routine_offset = arm64->operands[2].mem.disp;
          spec->start_parameter_offset = spec->start_routine_offset + 8;
          break;
        case ARM64_INS_BLR:
          if (arm64->operands[0].reg == func_reg)
            spec->start_impl = (guint8 *) ldp_location - (4 * sizeof (guint32));
          break;
        default:
          break;
      }
    }
  }
#else
# error FIXME
#endif

  cs_free (insn, 1);

  cs_close (&capstone);

  if (spec->start_impl == NULL)
    return FALSE;

  spec->terminate_impl = GSIZE_TO_POINTER (gum_module_find_export_by_name (libc,
        "pthread_exit"));

  return spec->terminate_impl != NULL;
}

static GumAddress
gum_find_function_symbol_by_prefix (GumModule * module,
                                    const gchar * prefix)
{
  GumFindFunctionByPrefixContext ctx;

  ctx.prefix = prefix;
  ctx.address = 0;

  gum_module_enumerate_symbols (module,
      gum_store_first_function_symbol_with_prefix, &ctx);

  return ctx.address;
}

static gboolean
gum_store_first_function_symbol_with_prefix (const GumSymbolDetails * details,
                                             gpointer user_data)
{
  GumFindFunctionByPrefixContext * ctx = user_data;

  if (details->type == GUM_SYMBOL_FUNCTION &&
      g_str_has_prefix (details->name, ctx->prefix))
  {
    ctx->address = details->address;
    return FALSE;
  }

  return TRUE;
}

#endif

static gssize
gum_libc_clone (GumCloneFunc child_func,
                gpointer child_stack,
                gint flags,
                gpointer arg,
                pid_t * parent_tidptr,
                GumUserDesc * tls,
                pid_t * child_tidptr)
{
  gssize result;
  gpointer * child_sp = child_stack;

#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
  *(--child_sp) = arg;
  *(--child_sp) = child_func;

  {
    register          gint ebx asm ("ebx") = flags;
    register    gpointer * ecx asm ("ecx") = child_sp;
    register       pid_t * edx asm ("edx") = parent_tidptr;
    register GumUserDesc * esi asm ("esi") = tls;
    register       pid_t * edi asm ("edi") = child_tidptr;

    asm volatile (
        "int $0x80\n\t"
        "test %%eax, %%eax\n\t"
        "jnz 1f\n\t"

        /* child: */
        "popl %%eax\n\t"
        "call *%%eax\n\t"
        "movl %%eax, %%ebx\n\t"
        "movl %[exit_syscall], %%eax\n\t"
        "int $0x80\n\t"

        /* parent: */
        "1:\n\t"
        : "=a" (result)
        : "0" (__NR_clone),
          "r" (ebx),
          "r" (ecx),
          "r" (edx),
          "r" (esi),
          "r" (edi),
          [exit_syscall] "i" (__NR_exit)
        : "cc", "memory"
    );
  }
#elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
  *(--child_sp) = arg;
  *(--child_sp) = child_func;
  *(--child_sp) = tls;

  {
    register          gint rdi asm ("rdi") = flags;
    register    gpointer * rsi asm ("rsi") = child_sp;
    register       pid_t * rdx asm ("rdx") = parent_tidptr;
    register GumUserDesc * r10 asm ("r10") = tls;
    register       pid_t *  r8 asm ( "r8") = child_tidptr;

    asm volatile (
        "syscall\n\t"
        "test %%rax, %%rax\n\t"
        "jnz 1f\n\t"

        /* child: */
        "movq %[prctl_syscall], %%rax\n\t"
        "movq %[arch_set_fs], %%rdi\n\t"
        "popq %%rsi\n\t"
        "syscall\n\t"

        "popq %%rax\n\t"
        "popq %%rdi\n\t"
        "call *%%rax\n\t"
        "movq %%rax, %%rdi\n\t"
        "movq %[exit_syscall], %%rax\n\t"
        "syscall\n\t"

        /* parent: */
        "1:\n\t"
        : "=a" (result)
        : "0" (__NR_clone),
          "r" (rdi),
          "r" (rsi),
          "r" (rdx),
          "r" (r10),
          "r" (r8),
          [prctl_syscall] "i" (__NR_arch_prctl),
          [arch_set_fs] "i" (ARCH_SET_FS),
          [exit_syscall] "i" (__NR_exit)
        : "rcx", "r11", "cc", "memory"
    );
  }
#elif defined (HAVE_ARM) && defined (__ARM_EABI__)
  *(--child_sp) = child_func;
  *(--child_sp) = arg;

  {
    register        gssize r6 asm ("r6") = __NR_clone;
    register          gint r0 asm ("r0") = flags;
    register    gpointer * r1 asm ("r1") = child_sp;
    register       pid_t * r2 asm ("r2") = parent_tidptr;
    register GumUserDesc * r3 asm ("r3") = tls;
    register       pid_t * r4 asm ("r4") = child_tidptr;

    asm volatile (
        "push {r7}\n\t"
        "mov r7, r6\n\t"
        "swi 0x0\n\t"
        "cmp r0, #0\n\t"
        "bne 1f\n\t"

        /* child: */
        "pop {r0, r1}\n\t"
        "blx r1\n\t"
        "mov r7, %[exit_syscall]\n\t"
        "swi 0x0\n\t"

        /* parent: */
        "1:\n\t"
        "pop {r7}\n\t"
        : "+r" (r0)
        : "r" (r1),
          "r" (r2),
          "r" (r3),
          "r" (r4),
          "r" (r6),
          [exit_syscall] "i" (__NR_exit)
        : "cc", "memory"
    );

    result = r0;
  }
#elif defined (HAVE_ARM)
  *(--child_sp) = child_func;
  *(--child_sp) = arg;

  {
    register          gint r0 asm ("r0") = flags;
    register    gpointer * r1 asm ("r1") = child_sp;
    register       pid_t * r2 asm ("r2") = parent_tidptr;
    register GumUserDesc * r3 asm ("r3") = tls;
    register       pid_t * r4 asm ("r4") = child_tidptr;

    asm volatile (
        "swi %[clone_syscall]\n\t"
        "cmp r0, #0\n\t"
        "bne 1f\n\t"

        /* child: */
        "ldmia sp!, {r0, r1}\n\t"
        "blx r1\n\t"
        "swi %[exit_syscall]\n\t"

        /* parent: */
        "1:\n\t"
        : "+r" (r0)
        : "r" (r1),
          "r" (r2),
          "r" (r3),
          "r" (r4),
          [clone_syscall] "i" (__NR_clone),
          [exit_syscall] "i" (__NR_exit)
        : "cc", "memory"
    );

    result = r0;
  }
#elif defined (HAVE_ARM64)
  *(--child_sp) = child_func;
  *(--child_sp) = arg;

  {
    register        gssize x8 asm ("x8") = __NR_clone;
    register          gint x0 asm ("x0") = flags;
    register    gpointer * x1 asm ("x1") = child_sp;
    register       pid_t * x2 asm ("x2") = parent_tidptr;
    register GumUserDesc * x3 asm ("x3") = tls;
    register       pid_t * x4 asm ("x4") = child_tidptr;

    asm volatile (
        "svc 0x0\n\t"
        "cbnz x0, 1f\n\t"

        /* child: */
        "ldp x0, x1, [sp], #16\n\t"
        "blr x1\n\t"
        "mov x8, %x[exit_syscall]\n\t"
        "svc 0x0\n\t"

        /* parent: */
        "1:\n\t"
        : "+r" (x0)
        : "r" (x1),
          "r" (x2),
          "r" (x3),
          "r" (x4),
          "r" (x8),
          [exit_syscall] "i" (__NR_exit)
        : "cc", "memory"
    );

    result = x0;
  }
#elif defined (HAVE_MIPS)
  *(--child_sp) = child_func;
  *(--child_sp) = arg;

  {
    register          gint a0 asm ("$a0") = flags;
    register    gpointer * a1 asm ("$a1") = child_sp;
    register       pid_t * a2 asm ("$a2") = parent_tidptr;
    register GumUserDesc * a3 asm ("$a3") = tls;
    register       pid_t * a4 asm ("$t0") = child_tidptr;
    int status;
    gssize retval;

    asm volatile (
        ".set noreorder\n\t"
        "addiu $sp, $sp, -24\n\t"
        "sw $t0, 16($sp)\n\t"
        "li $v0, %[clone_syscall]\n\t"
        "syscall\n\t"
        "bne $a3, $0, 1f\n\t"
        "nop\n\t"
        "bne $v0, $0, 1f\n\t"
        "nop\n\t"

        /* child: */
        "lw $a0, 0($sp)\n\t"
        "lw $t9, 4($sp)\n\t"
        "addiu $sp, $sp, 8\n\t"
        "jalr $t9\n\t"
        "nop\n\t"
        "move $a0, $2\n\t"
        "li $v0, %[exit_syscall]\n\t"
        "syscall\n\t"

        /* parent: */
        "1:\n\t"
        "addiu $sp, $sp, 24\n\t"
        "move %0, $a3\n\t"
        "move %1, $v0\n\t"
        ".set reorder\n\t"
        : "=r" (status),
          "=r" (retval)
        : "r" (a0),
          "r" (a1),
          "r" (a2),
          "r" (a3),
          "r" (a4),
          [clone_syscall] "i" (__NR_clone),
          [exit_syscall] "i" (__NR_exit)
        : "$1", "$2", "$3",
          "$10", "$11", "$12", "$13", "$14", "$15",
          "$24", "$25",
          "hi", "lo",
          "memory"
    );

    if (status == 0)
    {
      result = retval;
    }
    else
    {
      result = -1;
      errno = retval;
    }
  }
#endif

  return result;
}

static gssize
gum_libc_read (gint fd,
               gpointer buf,
               gsize count)
{
  return gum_libc_syscall_3 (__NR_read, fd, GPOINTER_TO_SIZE (buf), count);
}

static gssize
gum_libc_write (gint fd,
                gconstpointer buf,
                gsize count)
{
  return gum_libc_syscall_3 (__NR_write, fd, GPOINTER_TO_SIZE (buf), count);
}

static pid_t
gum_libc_waitpid (pid_t pid,
                  int * status,
                  int options)
{
#ifdef __NR_waitpid
  return gum_libc_syscall_3 (__NR_waitpid, pid, GPOINTER_TO_SIZE (status),
      options);
#else
  return gum_libc_syscall_4 (__NR_wait4, pid, GPOINTER_TO_SIZE (status),
      options, 0);
#endif
}

static gssize
gum_libc_ptrace (gsize request,
                 pid_t pid,
                 gpointer address,
                 gpointer data)
{
  return gum_libc_syscall_4 (__NR_ptrace, request, pid,
      GPOINTER_TO_SIZE (address), GPOINTER_TO_SIZE (data));
}

static gssize
gum_libc_syscall_4 (gsize n,
                    gsize a,
                    gsize b,
                    gsize c,
                    gsize d)
{
  gssize result;

#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
  {
    register gsize ebx asm ("ebx") = a;
    register gsize ecx asm ("ecx") = b;
    register gsize edx asm ("edx") = c;
    register gsize esi asm ("esi") = d;

    asm volatile (
        "int $0x80\n\t"
        : "=a" (result)
        : "0" (n),
          "r" (ebx),
          "r" (ecx),
          "r" (edx),
          "r" (esi)
        : "cc", "memory"
    );
  }
#elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
  {
    register gsize rdi asm ("rdi") = a;
    register gsize rsi asm ("rsi") = b;
    register gsize rdx asm ("rdx") = c;
    register gsize r10 asm ("r10") = d;

    asm volatile (
        "syscall\n\t"
        : "=a" (result)
        : "0" (n),
          "r" (rdi),
          "r" (rsi),
          "r" (rdx),
          "r" (r10)
        : "rcx", "r11", "cc", "memory"
    );
  }
#elif defined (HAVE_ARM) && defined (__ARM_EABI__)
  {
    register gssize r6 asm ("r6") = n;
    register  gsize r0 asm ("r0") = a;
    register  gsize r1 asm ("r1") = b;
    register  gsize r2 asm ("r2") = c;
    register  gsize r3 asm ("r3") = d;

    asm volatile (
        "push {r7}\n\t"
        "mov r7, r6\n\t"
        "swi 0x0\n\t"
        "pop {r7}\n\t"
        : "+r" (r0)
        : "r" (r1),
          "r" (r2),
          "r" (r3),
          "r" (r6)
        : "memory"
    );

    result = r0;
  }
#elif defined (HAVE_ARM)
  {
    register gssize r0 asm ("r0") = n;
    register  gsize r1 asm ("r1") = a;
    register  gsize r2 asm ("r2") = b;
    register  gsize r3 asm ("r3") = c;
    register  gsize r4 asm ("r4") = d;

    asm volatile (
        "swi %[syscall]\n\t"
        : "+r" (r0)
        : "r" (r1),
          "r" (r2),
          "r" (r3),
          "r" (r4),
          [syscall] "i" (__NR_syscall)
        : "memory"
    );

    result = r0;
  }
#elif defined (HAVE_ARM64)
  {
    register gssize x8 asm ("x8") = n;
    register  gsize x0 asm ("x0") = a;
    register  gsize x1 asm ("x1") = b;
    register  gsize x2 asm ("x2") = c;
    register  gsize x3 asm ("x3") = d;

    asm volatile (
        "svc 0x0\n\t"
        : "+r" (x0)
        : "r" (x1),
          "r" (x2),
          "r" (x3),
          "r" (x8)
        : "memory"
    );

    result = x0;
  }
#elif defined (HAVE_MIPS)
  {
    register gssize v0 asm ("$16") = n;
    register  gsize a0 asm ("$4") = a;
    register  gsize a1 asm ("$5") = b;
    register  gsize a2 asm ("$6") = c;
    register  gsize a3 asm ("$7") = d;
    int status;
    gssize retval;

    asm volatile (
        ".set noreorder\n\t"
        "move $2, %1\n\t"
        "syscall\n\t"
        "move %0, $7\n\t"
        "move %1, $2\n\t"
        ".set reorder\n\t"
        : "=r" (status),
          "=r" (retval)
        : "r" (v0),
          "r" (a0),
          "r" (a1),
          "r" (a2),
          "r" (a3)
        : "$1", "$2", "$3",
          "$10", "$11", "$12", "$13", "$14", "$15",
          "$24", "$25",
          "hi", "lo",
          "memory"
    );

    if (status == 0)
    {
      result = retval;
    }
    else
    {
      result = -1;
      errno = retval;
    }
  }
#endif

  return result;
}
