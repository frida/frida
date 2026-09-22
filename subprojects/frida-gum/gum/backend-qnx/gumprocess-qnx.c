/*
 * Copyright (C) 2015-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2023 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumprocess-priv.h"

#include "gum-init.h"
#include "gum/gumqnx.h"
#include "gumqnx-priv.h"

#include <devctl.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/elf.h>
#include <sys/neutrino.h>
#include <sys/procfs.h>
#include <ucontext.h>

#define GUM_PSR_THUMB 0x20

static void gum_deinit_libc_module (void);

static void gum_store_cpu_context (GumThreadId thread_id,
    GumCpuContext * cpu_context, gpointer user_data);
static void gum_enumerate_ranges_of (const gchar * device_path,
    GumPageProtection prot, GumFoundRangeFunc func, gpointer user_data);

static void gum_cpu_context_from_qnx (const debug_greg_t * gregs,
    GumCpuContext * ctx);
static void gum_cpu_context_to_qnx (const GumCpuContext * ctx,
    debug_greg_t * gregs);

static gboolean gum_collect_thread_details (GumThreadDetails * details,
    const debug_thread_t * thread, GumThreadFlags flags, gchar * name_buffer);
static GumThreadState gum_thread_state_from_system_thread_state (int state);

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
    g_assert (addr_in_libc != NULL);

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
  gint fd, res G_GNUC_UNUSED;
  procfs_status status;

  fd = open ("/proc/self", O_RDONLY);
  g_assert (fd != -1);

  status.tid = gettid ();
  res = devctl (fd, DCMD_PROC_TIDSTATUS, &status, sizeof (status), NULL);
  g_assert (res == 0);

  close (fd);

  return status.flags != 0;
}

GumProcessId
gum_process_get_id (void)
{
  return getpid ();
}

GumThreadId
gum_process_get_current_thread_id (void)
{
  return gettid ();
}

gboolean
gum_process_has_thread (GumThreadId thread_id)
{
  gboolean found = FALSE;
  gint fd;
  procfs_status status;

  fd = open ("/proc/self", O_RDONLY);
  g_assert (fd != -1);

  status.tid = thread_id;
  if (devctl (fd, DCMD_PROC_TIDSTATUS, &status, sizeof (status), NULL) != EOK)
    goto beach;

  found = status.tid == thread_id;

beach:
  close (fd);

  return found;
}

GumThreadDetails *
gum_process_find_thread_by_id (GumThreadId thread_id,
                               GumThreadFlags flags)
{
  GumThreadDetails * result = NULL;
  gint fd;
  debug_thread_t thread = { 0, };
  GumThreadDetails details = { 0, };
  gchar thread_name[_NTO_THREAD_NAME_MAX];

  fd = open ("/proc/self/as", O_RDONLY);
  g_assert (fd != -1);

  thread.tid = thread_id;
  if (devctl (fd, DCMD_PROC_TIDSTATUS, &thread, sizeof (thread), NULL) == 0 &&
      thread.tid == thread_id && thread.state != STATE_DEAD)
  {
    if (gum_collect_thread_details (&details, &thread, flags, thread_name))
      result = gum_thread_details_copy (&details);
  }

  close (fd);

  return result;
}

gboolean
gum_process_modify_thread (GumThreadId thread_id,
                           GumModifyThreadFunc func,
                           gpointer user_data,
                           GumModifyThreadFlags flags)
{
  gboolean success = FALSE;
  gboolean holding = FALSE;
  pid_t child;
  int status;

  if (ThreadCtl (_NTO_TCTL_ONE_THREAD_HOLD, GSIZE_TO_POINTER (thread_id)) == -1)
    goto beach;
  holding = TRUE;

  child = vfork ();
  if (child == -1)
    goto beach;

  if (child == 0)
  {
    gchar as_path[PATH_MAX];
    int fd, res G_GNUC_UNUSED;
    procfs_greg gregs;
    GumCpuContext cpu_context;

    sprintf (as_path, "/proc/%d/as", getppid ());

    fd = open (as_path, O_RDWR);
    g_assert (fd != -1);

    res = devctl (fd, DCMD_PROC_CURTHREAD, &thread_id, sizeof (thread_id),
        NULL);
    g_assert (res == 0);

    res = devctl (fd, DCMD_PROC_GETGREG, &gregs, sizeof (gregs), NULL);
    g_assert (res == 0);

    gum_cpu_context_from_qnx (&gregs, &cpu_context);
    func (thread_id, &cpu_context, user_data);
    gum_cpu_context_to_qnx (&cpu_context, &gregs);

    res = devctl (fd, DCMD_PROC_SETGREG, &gregs, sizeof (gregs), NULL);
    g_assert (res == 0);

    close (fd);
    _exit (0);
  }

  waitpid (child, &status, 0);

  success = TRUE;

beach:
  if (holding)
    ThreadCtl (_NTO_TCTL_ONE_THREAD_CONT, GSIZE_TO_POINTER (thread_id));

  return success;
}

void
_gum_process_enumerate_threads (GumFoundThreadFunc func,
                                gpointer user_data,
                                GumThreadFlags flags)
{
  gint fd, res G_GNUC_UNUSED;
  debug_process_t info;
  debug_thread_t thread;
  gboolean carry_on;

  fd = open ("/proc/self/as", O_RDONLY);
  g_assert (fd != -1);

  res = devctl (fd, DCMD_PROC_INFO, &info, sizeof (info), NULL);
  g_assert (res == 0);

  for (thread.tid = 1, carry_on = TRUE;
      carry_on && devctl (fd, DCMD_PROC_TIDSTATUS, &thread, sizeof (thread),
        NULL) == 0;
      thread.tid++)
  {
    GumThreadDetails details = { 0, };
    gchar thread_name[_NTO_THREAD_NAME_MAX];

    if (thread.state == STATE_DEAD)
      continue;

    if (gum_collect_thread_details (&details, &thread, flags, thread_name))
      carry_on = func (&details, user_data);
  }

  close (fd);
}

static gboolean
gum_collect_thread_details (GumThreadDetails * details,
                            const debug_thread_t * thread,
                            GumThreadFlags flags,
                            gchar * name_buffer)
{
  details->id = thread->tid;

  if ((flags & GUM_THREAD_FLAGS_NAME) != 0)
  {
    if (pthread_getname_np (thread->tid, name_buffer,
          _NTO_THREAD_NAME_MAX) == 0 && name_buffer[0] != '\0')
    {
      details->name = name_buffer;
      details->flags |= GUM_THREAD_FLAGS_NAME;
    }
  }

  if ((flags & GUM_THREAD_FLAGS_STATE) != 0)
  {
    details->state = gum_thread_state_from_system_thread_state (thread->state);
    details->flags |= GUM_THREAD_FLAGS_STATE;
  }

  if ((flags & GUM_THREAD_FLAGS_CPU_CONTEXT) != 0)
  {
    if (!gum_process_modify_thread (details->id, gum_store_cpu_context,
          &details->cpu_context, GUM_MODIFY_THREAD_FLAGS_ABORT_SAFELY))
      return FALSE;
    details->flags |= GUM_THREAD_FLAGS_CPU_CONTEXT;
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

gboolean
_gum_process_collect_main_module (GumModule * module,
                                  gpointer user_data)
{
  GumModule ** out = user_data;

  *out = g_object_ref (module);

  return FALSE;
}

void
gum_qnx_enumerate_ranges (pid_t pid,
                          GumPageProtection prot,
                          GumFoundRangeFunc func,
                          gpointer user_data)
{
  gchar * as_path = g_strdup_printf ("/proc/%d/as", pid);
  gum_enumerate_ranges_of (as_path, prot, func, user_data);
  g_free (as_path);
}

void
_gum_process_enumerate_ranges (GumPageProtection prot,
                               GumFoundRangeFunc func,
                               gpointer user_data)
{
  gum_enumerate_ranges_of ("/proc/self/as", prot, func, user_data);
}

static void
gum_enumerate_ranges_of (const gchar * device_path,
                         GumPageProtection prot,
                         GumFoundRangeFunc func,
                         gpointer user_data)
{
  gint fd, res G_GNUC_UNUSED;
  gboolean carry_on = TRUE;
  gint mapinfo_count;
  procfs_mapinfo * mapinfo_entries;
  gsize mapinfo_size;
  procfs_debuginfo * debuginfo;
  const gsize debuginfo_size = sizeof (procfs_debuginfo) + 0x100;
  gint i;

  fd = open (device_path, O_RDONLY);
  if (fd == -1)
    return;

  res = devctl (fd, DCMD_PROC_PAGEDATA, 0, 0, &mapinfo_count);
  g_assert (res == 0);
  mapinfo_size = mapinfo_count * sizeof (procfs_mapinfo);
  mapinfo_entries = g_malloc (mapinfo_size);

  debuginfo = g_malloc (debuginfo_size);

  res = devctl (fd, DCMD_PROC_PAGEDATA, mapinfo_entries, mapinfo_size,
      &mapinfo_count);
  g_assert (res == 0);

  for (i = 0; carry_on && i != mapinfo_count; i++)
  {
    const procfs_mapinfo * mapinfo = &mapinfo_entries[i];
    GumRangeDetails details;
    GumMemoryRange range;
    GumFileMapping file;
    gchar * path = NULL;

    details.range = &range;
    details.protection = _gum_page_protection_from_posix (mapinfo->flags);

    range.base_address = mapinfo->vaddr;
    range.size = mapinfo->size;

    debuginfo->vaddr = mapinfo->vaddr;
    res = devctl (fd, DCMD_PROC_MAPDEBUG, debuginfo, debuginfo_size, NULL);
    g_assert (res == 0);
    if (strcmp (debuginfo->path, "/dev/zero") != 0)
    {
      if (debuginfo->path[0] != '/')
      {
        path = g_strconcat ("/", debuginfo->path, NULL);
        file.path = path;
      }
      else
      {
        file.path = debuginfo->path;
      }

      file.offset = mapinfo->offset;
      file.size = mapinfo->size;

      details.file = &file;
    }
    else
    {
      details.file = NULL;
    }

    if ((details.protection & prot) == prot)
    {
      carry_on = func (&details, user_data);
    }

    g_free (path);
  }

  g_free (debuginfo);
  g_free (mapinfo_entries);

  close (fd);
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
  /* Not implemented */
  return 0;
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
  if (ThreadCtl (_NTO_TCTL_ONE_THREAD_HOLD, GSIZE_TO_POINTER (thread_id)) == -1)
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
  if (ThreadCtl (_NTO_TCTL_ONE_THREAD_CONT, GSIZE_TO_POINTER (thread_id)) == -1)
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

GumCpuType
gum_qnx_cpu_type_from_file (const gchar * path,
                            GError ** error)
{
  GumCpuType result = -1;
  FILE * file;
  guint8 ei_data;
  guint16 e_machine;

  file = fopen (path, "rb");
  if (file == NULL)
    goto beach;

  if (fseek (file, EI_DATA, SEEK_SET) != 0)
    goto beach;
  if (fread (&ei_data, sizeof (ei_data), 1, file) != 1)
    goto beach;

  if (fseek (file, 0x12, SEEK_SET) != 0)
    goto beach;
  if (fread (&e_machine, sizeof (e_machine), 1, file) != 1)
    goto beach;

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
gum_qnx_cpu_type_from_pid (pid_t pid,
                           GError ** error)
{
  GumCpuType result = -1;
  gchar * auxv_path;
  guint8 * auxv;
  gsize auxv_size, i;

  auxv_path = g_strdup_printf ("/proc/%d/auxv", pid);

  auxv = NULL;
  if (!g_file_get_contents (auxv_path, (gchar **) &auxv, &auxv_size, NULL))
    goto not_found;

#ifdef HAVE_I386
  result = GUM_CPU_AMD64;
#else
  result = GUM_CPU_ARM64;
#endif

  for (i = 0; i < auxv_size; i += 16)
  {
    if (auxv[4] != 0 || auxv[5] != 0 ||
        auxv[6] != 0 || auxv[7] != 0)
    {
#ifdef HAVE_I386
      result = GUM_CPU_IA32;
#else
      result = GUM_CPU_ARM;
#endif
      break;
    }
  }

  goto beach;

not_found:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_FOUND, "Process not found");
    goto beach;
  }
beach:
  {
    g_free (auxv_path);

    return result;
  }
}

gchar *
gum_qnx_query_program_path_for_self (GError ** error)
{
  gchar * program_path = NULL;
  int fd;
  struct
  {
    procfs_debuginfo info;
    char buffer[PATH_MAX];
  } name;

  fd = open ("/proc/self/as", O_RDONLY);
  if (fd == -1)
    goto failure;

  if (devctl (fd, DCMD_PROC_MAPDEBUG_BASE, &name, sizeof (name), 0) != EOK)
    goto failure;

  if (g_path_is_absolute (name.info.path))
  {
    program_path = g_strdup (name.info.path);
  }
  else
  {
    gchar * cwd = g_get_current_dir ();
    program_path = g_canonicalize_filename (name.info.path, cwd);
    g_free (cwd);
  }

  goto beach;

failure:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_FAILED, "%s", g_strerror (errno));
    goto beach;
  }
beach:
  {
    if (fd != -1)
      close (fd);

    return program_path;
  }
}

static void
gum_cpu_context_from_qnx (const debug_greg_t * gregs,
                          GumCpuContext * ctx)
{
#if defined (HAVE_I386)
  const X86_CPU_REGISTERS * regs = &gregs->x86;

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
#elif defined (HAVE_ARM)
  const ARM_CPU_REGISTERS * regs = &gregs->arm;

  ctx->pc = regs->gpr[ARM_REG_R15];
  ctx->sp = regs->gpr[ARM_REG_R13];
  ctx->cpsr = regs->spsr;

  ctx->r8 = regs->gpr[ARM_REG_R8];
  ctx->r9 = regs->gpr[ARM_REG_R9];
  ctx->r10 = regs->gpr[ARM_REG_R10];
  ctx->r11 = regs->gpr[ARM_REG_R11];
  ctx->r12 = regs->gpr[ARM_REG_R12];

  memset (ctx->v, 0, sizeof (ctx->v));

  memcpy (ctx->r, regs->gpr, sizeof (ctx->r));
  ctx->lr = regs->gpr[ARM_REG_R14];
#else
# error Fix this for other architectures
#endif
}

static void
gum_cpu_context_to_qnx (const GumCpuContext * ctx,
                        debug_greg_t * gregs)
{
#if defined (HAVE_I386)
  X86_CPU_REGISTERS * regs = &gregs->x86;

  regs->eip = ctx->eip;

  regs->edi = ctx->edi;
  regs->esi = ctx->esi;
  regs->ebp = ctx->ebp;
  regs->esp = ctx->esp;
  regs->ebx = ctx->ebx;
  regs->edx = ctx->edx;
  regs->ecx = ctx->ecx;
  regs->eax = ctx->eax;
#elif defined (HAVE_ARM)
  ARM_CPU_REGISTERS * regs = &gregs->arm;

  regs->gpr[ARM_REG_R15] = ctx->pc;
  regs->gpr[ARM_REG_R13] = ctx->sp;
  regs->spsr = ctx->cpsr;

  regs->gpr[ARM_REG_R8] = ctx->r8;
  regs->gpr[ARM_REG_R9] = ctx->r9;
  regs->gpr[ARM_REG_R10] = ctx->r10;
  regs->gpr[ARM_REG_R11] = ctx->r11;
  regs->gpr[ARM_REG_R12] = ctx->r12;

  memcpy (regs->gpr, ctx->r, sizeof (ctx->r));
  regs->gpr[ARM_REG_R14] = ctx->lr;
#else
# error Fix this for other architectures
#endif
}

static GumThreadState
gum_thread_state_from_system_thread_state (gint state)
{
  switch (state)
  {
    case STATE_RUNNING:
      return GUM_THREAD_RUNNING;
    case STATE_CONDVAR:
    case STATE_INTR:
    case STATE_JOIN:
    case STATE_MUTEX:
    case STATE_NET_REPLY:
    case STATE_NET_SEND:
    case STATE_READY:
    case STATE_RECEIVE:
    case STATE_REPLY:
    case STATE_NANOSLEEP:
    case STATE_SEM:
    case STATE_SEND:
    case STATE_SIGSUSPEND:
    case STATE_SIGWAITINFO:
    case STATE_STACK:
    case STATE_WAITCTX:
    case STATE_WAITPAGE:
    case STATE_WAITTHREAD:
      return GUM_THREAD_WAITING;
    case STATE_STOPPED:
      return GUM_THREAD_STOPPED;
    case STATE_DEAD:
      return GUM_THREAD_HALTED;
    default:
      g_assert_not_reached ();
      break;
  }
}

void
gum_qnx_parse_ucontext (const ucontext_t * uc,
                        GumCpuContext * ctx)
{
#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
  const X86_CPU_REGISTERS * cpu = &uc->uc_mcontext.cpu;

  ctx->eip = cpu->eip;

  ctx->edi = cpu->edi;
  ctx->esi = cpu->esi;
  ctx->ebp = cpu->ebp;
  ctx->esp = cpu->esp;
  ctx->ebx = cpu->ebx;
  ctx->edx = cpu->edx;
  ctx->ecx = cpu->ecx;
  ctx->eax = cpu->eax;

  ctx->xmm = NULL;
#elif defined (HAVE_ARM)
  const ARM_CPU_REGISTERS * cpu = &uc->uc_mcontext.cpu;
  guint i;

  ctx->pc = cpu->gpr[ARM_REG_PC];
  ctx->sp = cpu->gpr[ARM_REG_SP];
  ctx->cpsr = cpu->spsr;

  ctx->r8 = cpu->gpr[ARM_REG_R8];
  ctx->r9 = cpu->gpr[ARM_REG_R9];
  ctx->r10 = cpu->gpr[ARM_REG_R10];
  ctx->r11 = cpu->gpr[ARM_REG_R11];
  ctx->r12 = cpu->gpr[ARM_REG_R12];

  memset (ctx->v, 0, sizeof (ctx->v));

  for (i = 0; i != G_N_ELEMENTS (ctx->r); i++)
    ctx->r[i] = cpu->gpr[i];
  ctx->lr = cpu->gpr[ARM_REG_LR];
#else
# error FIXME
#endif
}

void
gum_qnx_unparse_ucontext (const GumCpuContext * ctx,
                          ucontext_t * uc)
{
#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
  X86_CPU_REGISTERS * cpu = &uc->uc_mcontext.cpu;

  cpu->eip = ctx->eip;

  cpu->edi = ctx->edi;
  cpu->esi = ctx->esi;
  cpu->ebp = ctx->ebp;
  cpu->esp = ctx->esp;
  cpu->ebx = ctx->ebx;
  cpu->edx = ctx->edx;
  cpu->ecx = ctx->ecx;
  cpu->eax = ctx->eax;
#elif defined (HAVE_ARM)
  ARM_CPU_REGISTERS * cpu = &uc->uc_mcontext.cpu;
  guint i;

  cpu->gpr[ARM_REG_PC] = ctx->pc;
  cpu->gpr[ARM_REG_SP] = ctx->sp;
  cpu->spsr = ctx->cpsr;
  if (ctx->pc & 1)
    cpu->spsr |= GUM_PSR_THUMB;
  else
    cpu->spsr &= ~GUM_PSR_THUMB;

  cpu->gpr[ARM_REG_R8] = ctx->r8;
  cpu->gpr[ARM_REG_R9] = ctx->r9;
  cpu->gpr[ARM_REG_R10] = ctx->r10;
  cpu->gpr[ARM_REG_R11] = ctx->r11;
  cpu->gpr[ARM_REG_R12] = ctx->r12;

  for (i = 0; i != G_N_ELEMENTS (ctx->r); i++)
    cpu->gpr[i] = ctx->r[i];
  cpu->gpr[ARM_REG_LR] = ctx->lr;
#else
# error FIXME
#endif
}

