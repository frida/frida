/*
 * Copyright (C) 2009-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2023 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2024 Håvard Sørbø <havard@hsorbo.no>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumprocess-priv.h"

#include "gummodule-windows.h"

#include <intrin.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <winternl.h>

#ifndef _MSC_VER
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Warray-bounds"
#endif

typedef enum {
  GUM_THREAD_QUERY_BASIC_INFORMATION,
  GUM_THREAD_QUERY_SET_WIN32_START_ADDRESS = 9,
} GumThreadInfoClass;

typedef NTSTATUS (WINAPI * GumQueryInformationThreadFunc) (HANDLE thread,
    GumThreadInfoClass klass, PVOID thread_information,
    ULONG thread_information_length, PULONG return_length);
typedef struct _GumThreadBasicInformation GumThreadBasicInformation;
typedef struct _GumThreadEnvironmentBlock GumThreadEnvironmentBlock;
typedef struct _GumSetHardwareBreakpointContext GumSetHardwareBreakpointContext;
typedef struct _GumSetHardwareWatchpointContext GumSetHardwareWatchpointContext;
typedef void (* GumModifyDebugRegistersFunc) (CONTEXT * ctx,
    gpointer user_data);
typedef HRESULT (WINAPI * GumGetThreadDescriptionFunc) (
    HANDLE thread, WCHAR ** description);
typedef void (WINAPI * GumGetCurrentThreadStackLimitsFunc) (
    PULONG_PTR low_limit, PULONG_PTR high_limit);
typedef BOOL (WINAPI * GumIsWow64ProcessFunc) (HANDLE process, BOOL * is_wow64);
typedef BOOL (WINAPI * GumGetProcessInformationFunc) (HANDLE process,
    PROCESS_INFORMATION_CLASS process_information_class,
    void * process_information, DWORD process_information_size);

struct _GumThreadBasicInformation
{
  NTSTATUS exit_status;
  GumThreadEnvironmentBlock * teb;
  CLIENT_ID client_id;
  ULONG_PTR affinity_mask;
  KPRIORITY priority;
  LONG base_priority;
};

struct _GumThreadEnvironmentBlock
{
  gpointer current_seh_frame;
  ULONG_PTR stack_high;
  ULONG_PTR stack_low;
#if GLIB_SIZEOF_VOID_P == 4
  gpointer padding[896];
#else
  gpointer padding[652];
#endif
  ULONG_PTR deallocation_stack;
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

static void gum_deinit_libc_module (void);
static gboolean gum_windows_query_thread_details (DWORD thread_id,
    GumThreadFlags flags, GumThreadDetails * details, gpointer * storage);
static gboolean gum_process_enumerate_heap_ranges (HANDLE heap,
    GumFoundMallocRangeFunc func, gpointer user_data);
static void gum_do_set_hardware_breakpoint (CONTEXT * ctx, gpointer user_data);
static void gum_do_unset_hardware_breakpoint (CONTEXT * ctx,
    gpointer user_data);
static void gum_do_set_hardware_watchpoint (CONTEXT * ctx, gpointer user_data);
static void gum_do_unset_hardware_watchpoint (CONTEXT * ctx,
    gpointer user_data);
static gboolean gum_modify_debug_registers (GumThreadId thread_id,
    GumModifyDebugRegistersFunc func, gpointer user_data, GError ** error);

static GumQueryInformationThreadFunc gum_get_query_information_thread (void);

static GumModule * gum_libc_module;

GumModule *
gum_process_get_libc_module (void)
{
  static gsize modules_value = 0;

  if (g_once_init_enter (&modules_value))
  {
    gum_libc_module = gum_process_find_module_by_name ("msvcrt.dll");

    _gum_register_destructor (gum_deinit_libc_module);

    g_once_init_leave (&modules_value, GPOINTER_TO_SIZE (gum_libc_module) + 1);
  }

  return GSIZE_TO_POINTER (modules_value - 1);
}

static void
gum_deinit_libc_module (void)
{
  g_clear_object (&gum_libc_module);
}

GumModule *
gum_process_find_module_by_name (const gchar * name)
{
  gunichar2 * wide_name;
  BOOL found;
  HMODULE handle;

  wide_name = g_utf8_to_utf16 (name, -1, NULL, NULL, NULL);
  handle = GetModuleHandleW ((LPCWSTR) wide_name);
  g_free (wide_name);
  if (handle == NULL)
    return NULL;

  return GUM_MODULE (_gum_native_module_make (handle));
}

GumModule *
gum_process_find_module_by_address (GumAddress address)
{
  HMODULE handle;

  if (!GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        GSIZE_TO_POINTER (address), &handle))
  {
    return NULL;
  }

  return GUM_MODULE (_gum_native_module_make (handle));
}

gboolean
gum_process_is_debugger_attached (void)
{
  return IsDebuggerPresent ();
}

GumProcessId
gum_process_get_id (void)
{
  return GetCurrentProcessId ();
}

#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4

GumThreadId
gum_process_get_current_thread_id (void)
{
  return __readfsdword (0x24);
}

#elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8

GumThreadId
gum_process_get_current_thread_id (void)
{
  return __readgsdword (0x48);
}

#else

GumThreadId
gum_process_get_current_thread_id (void)
{
  return GetCurrentThreadId ();
}

#endif

gboolean
gum_process_has_thread (GumThreadId thread_id)
{
  gboolean found = FALSE;
  HANDLE thread;

  thread = OpenThread (SYNCHRONIZE, FALSE, thread_id);
  if (thread != NULL)
  {
    found = WaitForSingleObject (thread, 0) == WAIT_TIMEOUT;

    CloseHandle (thread);
  }

  return found;
}

GumThreadDetails *
gum_process_find_thread_by_id (GumThreadId thread_id,
                               GumThreadFlags flags)
{
  GumThreadDetails * result = NULL;
  GumThreadDetails thread;
  gpointer storage;

  if (gum_windows_query_thread_details (thread_id, flags, &thread, &storage))
  {
    result = gum_thread_details_copy (&thread);
    g_free (storage);
  }

  return result;
}

gboolean
gum_process_modify_thread (GumThreadId thread_id,
                           GumModifyThreadFunc func,
                           gpointer user_data,
                           GumModifyThreadFlags flags)
{
  gboolean success = FALSE;
  HANDLE thread;
#ifdef _MSC_VER
  __declspec (align (64))
#endif
      CONTEXT context
#ifndef _MSC_VER
        __attribute__ ((aligned (64)))
#endif
        = { 0, };
  GumCpuContext cpu_context;

  thread = OpenThread (THREAD_GET_CONTEXT | THREAD_SET_CONTEXT |
      THREAD_SUSPEND_RESUME, FALSE, thread_id);
  if (thread == NULL)
    goto beach;

  if (SuspendThread (thread) == (DWORD) -1)
    goto beach;

  context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER |
      CONTEXT_FLOATING_POINT;
  if (!GetThreadContext (thread, &context))
    goto beach;

  gum_windows_parse_context (&context, &cpu_context);
#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
  cpu_context.xmm = (GumX86VectorReg *) &context.Xmm0;
#endif
  func (thread_id, &cpu_context, user_data);
  gum_windows_unparse_context (&cpu_context, &context);

  if (!SetThreadContext (thread, &context))
  {
    ResumeThread (thread);
    goto beach;
  }

  success = ResumeThread (thread) != (DWORD) -1;

beach:
  if (thread != NULL)
    CloseHandle (thread);

  return success;
}

void
_gum_process_enumerate_threads (GumFoundThreadFunc func,
                                gpointer user_data,
                                GumThreadFlags flags)
{
  DWORD this_process_id;
  HANDLE snapshot;
  THREADENTRY32 entry;
  gboolean carry_on;

  this_process_id = GetCurrentProcessId ();

  snapshot = CreateToolhelp32Snapshot (TH32CS_SNAPTHREAD, 0);
  if (snapshot == INVALID_HANDLE_VALUE)
    goto beach;

  entry.dwSize = sizeof (entry);
  if (!Thread32First (snapshot, &entry))
    goto beach;

  carry_on = TRUE;
  do
  {
    if (RTL_CONTAINS_FIELD (&entry, entry.dwSize, th32OwnerProcessID) &&
        entry.th32OwnerProcessID == this_process_id)
    {
      GumThreadDetails thread;
      gpointer storage;

      if (gum_windows_query_thread_details (entry.th32ThreadID, flags, &thread,
            &storage))
      {
        carry_on = func (&thread, user_data);
        g_free (storage);
      }
    }

    entry.dwSize = sizeof (entry);
  }
  while (carry_on && Thread32Next (snapshot, &entry));

beach:
  if (snapshot != INVALID_HANDLE_VALUE)
    CloseHandle (snapshot);
}

static gboolean
gum_windows_query_thread_details (DWORD thread_id,
                                  GumThreadFlags flags,
                                  GumThreadDetails * thread,
                                  gpointer * storage)
{
  gboolean success = FALSE;
  HANDLE handle = NULL;
  gchar * name = NULL;
  gboolean resume_still_pending = FALSE;
#ifdef _MSC_VER
  __declspec (align (64))
#endif
      CONTEXT context
#ifndef _MSC_VER
        __attribute__ ((aligned (64)))
#endif
        = { 0, };

  memset (thread, 0, sizeof (GumThreadDetails));
  *storage = NULL;

  thread->id = thread_id;

  handle = OpenThread (THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT |
      THREAD_SUSPEND_RESUME, FALSE, thread_id);
  if (handle == NULL)
    goto beach;

  if ((flags & GUM_THREAD_FLAGS_NAME) != 0)
  {
    name = gum_windows_query_thread_name (handle);
    if (name != NULL)
    {
      thread->name = name;
      thread->flags |= GUM_THREAD_FLAGS_NAME;

      *storage = g_steal_pointer (&name);
    }
  }

  if ((flags & (GUM_THREAD_FLAGS_STATE | GUM_THREAD_FLAGS_CPU_CONTEXT)) != 0)
  {
    if (thread_id == GetCurrentThreadId ())
    {
      if ((flags & GUM_THREAD_FLAGS_STATE) != 0)
      {
        thread->state = GUM_THREAD_RUNNING;
        thread->flags |= GUM_THREAD_FLAGS_STATE;
      }

      if ((flags & GUM_THREAD_FLAGS_CPU_CONTEXT) != 0)
      {
        RtlCaptureContext (&context);
        gum_windows_parse_context (&context, &thread->cpu_context);
        thread->flags |= GUM_THREAD_FLAGS_CPU_CONTEXT;
      }
    }
    else
    {
      DWORD previous_suspend_count;

      previous_suspend_count = SuspendThread (handle);
      if (previous_suspend_count == (DWORD) -1)
        goto beach;
      resume_still_pending = TRUE;

      if ((flags & GUM_THREAD_FLAGS_STATE) != 0)
      {
        if (previous_suspend_count == 0)
          thread->state = GUM_THREAD_RUNNING;
        else
          thread->state = GUM_THREAD_STOPPED;
        thread->flags |= GUM_THREAD_FLAGS_STATE;
      }

      if ((flags & GUM_THREAD_FLAGS_CPU_CONTEXT) != 0)
      {
        context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
        if (!GetThreadContext (handle, &context))
          goto beach;
        gum_windows_parse_context (&context, &thread->cpu_context);
        thread->flags |= GUM_THREAD_FLAGS_CPU_CONTEXT;
      }

      ResumeThread (handle);
      resume_still_pending = FALSE;
    }
  }

  if ((flags & GUM_THREAD_FLAGS_ENTRYPOINT_ROUTINE) != 0)
  {
    thread->entrypoint.routine =
        gum_windows_query_thread_entrypoint_routine (handle);
    thread->flags |= GUM_THREAD_FLAGS_ENTRYPOINT_ROUTINE;
  }

  success = TRUE;

beach:
  g_free (name);

  if (resume_still_pending)
    ResumeThread (handle);

  if (handle != NULL)
    CloseHandle (handle);

  if (!success)
    g_free ((gpointer) thread->name);

  return success;
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
_gum_process_enumerate_ranges (GumPageProtection prot,
                               GumFoundRangeFunc func,
                               gpointer user_data)
{
  guint8 * cur_base_address;

  cur_base_address = NULL;

  while (TRUE)
  {
    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T ret;

    ret = VirtualQuery (cur_base_address, &mbi, sizeof (mbi));
    if (ret == 0)
      break;

    if (mbi.Protect != 0 && (mbi.Protect & PAGE_GUARD) == 0)
    {
      GumPageProtection cur_prot;

      cur_prot = gum_page_protection_from_windows (mbi.Protect);

      if ((cur_prot & prot) == prot)
      {
        GumMemoryRange range;
        GumRangeDetails details;

        range.base_address = GUM_ADDRESS (cur_base_address);
        range.size = mbi.RegionSize;

        details.range = &range;
        details.protection = cur_prot;
        details.file = NULL; /* TODO */

        if (!func (&details, user_data))
          return;
      }
    }

    cur_base_address += mbi.RegionSize;
  }
}

void
gum_process_enumerate_malloc_ranges (GumFoundMallocRangeFunc func,
                                     gpointer user_data)
{
  HANDLE process_heap;
  DWORD num_heaps;
  HANDLE * heaps;
  DWORD num_heaps_after;
  DWORD i;

  process_heap = GetProcessHeap ();
  if (!gum_process_enumerate_heap_ranges (process_heap, func, user_data))
    return;

  num_heaps = GetProcessHeaps (0, NULL);
  if (num_heaps == 0)
    return;
  heaps = HeapAlloc (process_heap, 0, num_heaps * sizeof (HANDLE));
  if (heaps == NULL)
    return;
  num_heaps_after = GetProcessHeaps (num_heaps, heaps);

  num_heaps = MIN (num_heaps_after, num_heaps);
  for (i = 0; i != num_heaps; i++)
  {
    if (heaps[i] != process_heap)
    {
      if (!gum_process_enumerate_heap_ranges (process_heap, func, user_data))
        break;
    }
  }

  HeapFree (process_heap, 0, heaps);
}

static gboolean
gum_process_enumerate_heap_ranges (HANDLE heap,
                                   GumFoundMallocRangeFunc func,
                                   gpointer user_data)
{
  gboolean carry_on;
  gboolean locked_heap;
  GumMemoryRange range;
  GumMallocRangeDetails details;
  PROCESS_HEAP_ENTRY entry;

  /* HeapLock may fail but it doesn't seem to have any real consequences... */
  locked_heap = HeapLock (heap);

  details.range = &range;
  carry_on = TRUE;
  entry.lpData = NULL;
  while (carry_on && HeapWalk (heap, &entry))
  {
    if ((entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) != 0)
    {
      range.base_address = GUM_ADDRESS (entry.lpData);
      range.size = entry.cbData;
      carry_on = func (&details, user_data);
    }
  }

  if (locked_heap)
    HeapUnlock (heap);

  return carry_on;
}

guint
gum_thread_try_get_ranges (GumMemoryRange * ranges,
                           guint max_length)
{
  static gsize initialized = FALSE;
  static GumGetCurrentThreadStackLimitsFunc get_stack_limits = NULL;
  ULONG_PTR low, high;
  GumMemoryRange * range;

  if (g_once_init_enter (&initialized))
  {
    get_stack_limits = (GumGetCurrentThreadStackLimitsFunc) GetProcAddress (
        GetModuleHandleW (L"kernel32.dll"),
        "GetCurrentThreadStackLimits");

    g_once_init_leave (&initialized, TRUE);
  }

  if (get_stack_limits != NULL)
  {
    get_stack_limits (&low, &high);
  }
  else
  {
    GumThreadBasicInformation tbi;

    (gum_get_query_information_thread ()) (GetCurrentThread (),
        GUM_THREAD_QUERY_BASIC_INFORMATION, &tbi, sizeof (tbi), NULL);

    low = tbi.teb->deallocation_stack;
    high = tbi.teb->stack_high;
  }

  range = &ranges[0];
  range->base_address = low;
  range->size = high - low;

  return 1;
}

#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4

gint
gum_thread_get_system_error (void)
{
  gint32 * teb = (gint32 *) __readfsdword (0x18);
  return teb[13];
}

void
gum_thread_set_system_error (gint value)
{
  gint32 * teb = (gint32 *) __readfsdword (0x18);
  if (teb[13] != value)
    teb[13] = value;
}

#elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8

gint
gum_thread_get_system_error (void)
{
  gint32 * teb = (gint32 *) __readgsqword (0x30);
  return teb[26];
}

void
gum_thread_set_system_error (gint value)
{
  gint32 * teb = (gint32 *) __readgsqword (0x30);
  if (teb[26] != value)
    teb[26] = value;
}

#else

gint
gum_thread_get_system_error (void)
{
  return (gint) GetLastError ();
}

void
gum_thread_set_system_error (gint value)
{
  SetLastError ((DWORD) value);
}

#endif

gboolean
gum_thread_suspend (GumThreadId thread_id,
                    GError ** error)
{
  gboolean success = FALSE;
  HANDLE thread;

  thread = OpenThread (THREAD_SUSPEND_RESUME, FALSE, thread_id);
  if (thread == NULL)
    goto failure;

  if (SuspendThread (thread) == (DWORD) -1)
    goto failure;

  success = TRUE;
  goto beach;

failure:
  {
    g_set_error (error,
        GUM_ERROR,
        GUM_ERROR_FAILED,
        "Unable to suspend thread: 0x%08lx", GetLastError ());
    goto beach;
  }
beach:
  {
    if (thread != NULL)
      CloseHandle (thread);

    return success;
  }
}

gboolean
gum_thread_resume (GumThreadId thread_id,
                   GError ** error)
{
  gboolean success = FALSE;
  HANDLE thread;

  thread = OpenThread (THREAD_SUSPEND_RESUME, FALSE, thread_id);
  if (thread == NULL)
    goto failure;

  if (ResumeThread (thread) == (DWORD) -1)
    goto failure;

  success = TRUE;
  goto beach;

failure:
  {
    g_set_error (error,
        GUM_ERROR,
        GUM_ERROR_FAILED,
        "Unable to resume thread: 0x%08lx", GetLastError ());
    goto beach;
  }
beach:
  {
    if (thread != NULL)
      CloseHandle (thread);

    return success;
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

  return gum_modify_debug_registers (thread_id, gum_do_set_hardware_breakpoint,
      &bpc, error);
}

static void
gum_do_set_hardware_breakpoint (CONTEXT * ctx,
                                gpointer user_data)
{
  GumSetHardwareBreakpointContext * bpc = user_data;

#ifdef HAVE_ARM64
  _gum_arm64_set_breakpoint (ctx->Bcr, ctx->Bvr, bpc->breakpoint_id,
      bpc->address);
#else
  _gum_x86_set_breakpoint (&ctx->Dr7, &ctx->Dr0, bpc->breakpoint_id,
      bpc->address);
#endif
}

gboolean
gum_thread_unset_hardware_breakpoint (GumThreadId thread_id,
                                      guint breakpoint_id,
                                      GError ** error)
{
  return gum_modify_debug_registers (thread_id,
      gum_do_unset_hardware_breakpoint, GUINT_TO_POINTER (breakpoint_id),
      error);
}

static void
gum_do_unset_hardware_breakpoint (CONTEXT * ctx,
                                  gpointer user_data)
{
  guint breakpoint_id = GPOINTER_TO_UINT (user_data);

#ifdef HAVE_ARM64
  _gum_arm64_unset_breakpoint (ctx->Bcr, ctx->Bvr, breakpoint_id);
#else
  _gum_x86_unset_breakpoint (&ctx->Dr7, &ctx->Dr0, breakpoint_id);
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

  return gum_modify_debug_registers (thread_id, gum_do_set_hardware_watchpoint,
      &wpc, error);
}

static void
gum_do_set_hardware_watchpoint (CONTEXT * ctx,
                                gpointer user_data)
{
  GumSetHardwareWatchpointContext * wpc = user_data;

#ifdef HAVE_ARM64
  _gum_arm64_set_watchpoint (ctx->Wcr, ctx->Wvr, wpc->watchpoint_id,
      wpc->address, wpc->size, wpc->conditions);
#else
  _gum_x86_set_watchpoint (&ctx->Dr7, &ctx->Dr0, wpc->watchpoint_id,
      wpc->address, wpc->size, wpc->conditions);
#endif
}

gboolean
gum_thread_unset_hardware_watchpoint (GumThreadId thread_id,
                                      guint watchpoint_id,
                                      GError ** error)
{
  return gum_modify_debug_registers (thread_id,
      gum_do_unset_hardware_watchpoint, GUINT_TO_POINTER (watchpoint_id),
      error);
}

static void
gum_do_unset_hardware_watchpoint (CONTEXT * ctx,
                                  gpointer user_data)
{
  guint watchpoint_id = GPOINTER_TO_UINT (user_data);

#ifdef HAVE_ARM64
  _gum_arm64_unset_watchpoint (ctx->Wcr, ctx->Wvr, watchpoint_id);
#else
  _gum_x86_unset_watchpoint (&ctx->Dr7, &ctx->Dr0, watchpoint_id);
#endif
}

static gboolean
gum_modify_debug_registers (GumThreadId thread_id,
                            GumModifyDebugRegistersFunc func,
                            gpointer user_data,
                            GError ** error)
{
  gboolean success = FALSE;
  HANDLE thread = NULL;
  CONTEXT * active_ctx;

  if (thread_id == gum_process_get_current_thread_id () &&
      (active_ctx = gum_windows_get_active_exceptor_context ()) != NULL)
  {
    func (active_ctx, user_data);
  }
  else
  {
    CONTEXT ctx = { 0, };

    thread = OpenThread (
        THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
        FALSE,
        thread_id);
    if (thread == NULL)
      goto failure;

    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;

    if (!GetThreadContext (thread, &ctx))
      goto failure;

    func (&ctx, user_data);

    if (!SetThreadContext (thread, &ctx))
      goto failure;
  }

  success = TRUE;
  goto beach;

failure:
  {
    g_set_error (error,
        GUM_ERROR,
        GUM_ERROR_FAILED,
        "Unable to modify debug registers: 0x%08lx", GetLastError ());
    goto beach;
  }
beach:
  {
    if (thread != NULL)
      CloseHandle (thread);

    return success;
  }
}

GumCpuType
gum_windows_query_native_cpu_type (void)
{
  static gsize initialized = FALSE;
  static GumCpuType type;

  if (g_once_init_enter (&initialized))
  {
    SYSTEM_INFO si;

    GetNativeSystemInfo (&si);

    switch (si.wProcessorArchitecture)
    {
      case PROCESSOR_ARCHITECTURE_INTEL:
        type = GUM_CPU_IA32;
        break;
      case PROCESSOR_ARCHITECTURE_AMD64:
        type = GUM_CPU_AMD64;
        break;
      case PROCESSOR_ARCHITECTURE_ARM64:
        type = GUM_CPU_ARM64;
        break;
      default:
        g_assert_not_reached ();
    }

    g_once_init_leave (&initialized, TRUE);
  }

  return type;
}

GumCpuType
gum_windows_cpu_type_from_pid (guint pid,
                               GError ** error)
{
  GumCpuType result = -1;
  HANDLE process;
  static gsize initialized = FALSE;
  static GumIsWow64ProcessFunc is_wow64_process;
  static GumGetProcessInformationFunc get_process_information;

  process = OpenProcess (PROCESS_QUERY_INFORMATION, FALSE, pid);
  if (process == NULL)
    goto propagate_api_error;

  if (g_once_init_enter (&initialized))
  {
    HMODULE kernel32;

    kernel32 = GetModuleHandleW (L"kernel32.dll");

    is_wow64_process = (GumIsWow64ProcessFunc)
        GetProcAddress (kernel32, "IsWow64Process");
    get_process_information = (GumGetProcessInformationFunc)
        GetProcAddress (kernel32, "GetProcessInformation");

    if (get_process_information != NULL)
    {
      NTSTATUS (WINAPI * rtl_get_version) (PRTL_OSVERSIONINFOW info);
      RTL_OSVERSIONINFOW info = { 0, };
      gboolean win11_or_newer;

      rtl_get_version = (NTSTATUS (WINAPI *) (PRTL_OSVERSIONINFOW))
          GetProcAddress (GetModuleHandleW (L"ntdll.dll"), "RtlGetVersion");

      info.dwOSVersionInfoSize = sizeof (info);
      rtl_get_version (&info);

      win11_or_newer =
          info.dwMajorVersion >= 11 ||
          (info.dwMajorVersion == 10 &&
           (info.dwMinorVersion > 0 || info.dwBuildNumber >= 22000));
      if (!win11_or_newer)
        get_process_information = NULL;
    }

    g_once_init_leave (&initialized, TRUE);
  }

  if (get_process_information != NULL)
  {
    PROCESS_MACHINE_INFORMATION info;

    if (!get_process_information (process, ProcessMachineTypeInfo, &info,
          sizeof (info)))
    {
      goto propagate_api_error;
    }

    switch (info.ProcessMachine)
    {
      case IMAGE_FILE_MACHINE_I386:
        result = GUM_CPU_IA32;
        break;
      case IMAGE_FILE_MACHINE_AMD64:
        result = GUM_CPU_AMD64;
        break;
      case IMAGE_FILE_MACHINE_ARM64:
        result = GUM_CPU_ARM64;
        break;
      default:
        g_assert_not_reached ();
    }
  }
  else if (is_wow64_process != NULL)
  {
    BOOL is_wow64;

    if (!is_wow64_process (process, &is_wow64))
      goto propagate_api_error;

    result = is_wow64 ? GUM_CPU_IA32 : gum_windows_query_native_cpu_type ();
  }
  else
  {
    result = gum_windows_query_native_cpu_type ();
  }

  goto beach;

propagate_api_error:
  {
    DWORD code = GetLastError ();

    switch (code)
    {
      case ERROR_INVALID_PARAMETER:
        g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_FOUND,
            "Process not found");
        break;
      case ERROR_ACCESS_DENIED:
        g_set_error (error, GUM_ERROR, GUM_ERROR_PERMISSION_DENIED,
            "Permission denied");
        break;
      default:
        g_set_error (error, GUM_ERROR, GUM_ERROR_FAILED,
            "Unexpectedly failed with error code: 0x%08x", code);
        break;
    }

    goto beach;
  }
beach:
  {
    if (process != NULL)
      CloseHandle (process);

    return result;
  }
}

gboolean
_gum_windows_is_win7_wow64 (void)
{
  static gsize cached_result = 0;

  if (g_once_init_enter (&cached_result))
  {
    gboolean is_win7_wow64 = FALSE;
    GumIsWow64ProcessFunc is_wow64_process;
    HMODULE kernel32 = GetModuleHandleW (L"kernel32.dll");

    is_wow64_process = (GumIsWow64ProcessFunc)
        GetProcAddress (kernel32, "IsWow64Process");
    if (is_wow64_process != NULL)
    {
      BOOL is_wow64 = FALSE;

      if (is_wow64_process (GetCurrentProcess (), &is_wow64) && is_wow64)
      {
        NTSTATUS (WINAPI * rtl_get_version) (PRTL_OSVERSIONINFOW info);
        RTL_OSVERSIONINFOW info = { 0, };

        rtl_get_version = (NTSTATUS (WINAPI *) (PRTL_OSVERSIONINFOW))
            GetProcAddress (GetModuleHandleW (L"ntdll.dll"), "RtlGetVersion");

        info.dwOSVersionInfoSize = sizeof (info);

        /* Win7 / Server 2008 R2 = NT 6.1 */
        if (rtl_get_version != NULL &&
            rtl_get_version (&info) == 0 &&
            info.dwMajorVersion == 6 &&
            info.dwMinorVersion == 1)
        {
          is_win7_wow64 = TRUE;
        }
      }
    }

    g_once_init_leave (&cached_result, is_win7_wow64 + 1);
  }

  return cached_result - 1;
}

gchar *
gum_windows_query_thread_name (HANDLE thread)
{
  gchar * name = NULL;
  static gsize initialized = FALSE;
  static GumGetThreadDescriptionFunc get_thread_description;
  WCHAR * name_utf16 = NULL;

  if (g_once_init_enter (&initialized))
  {
    get_thread_description = (GumGetThreadDescriptionFunc) GetProcAddress (
        GetModuleHandleW (L"kernel32.dll"),
        "GetThreadDescription");

    g_once_init_leave (&initialized, TRUE);
  }

  if (get_thread_description == NULL)
    goto beach;

  if (!SUCCEEDED (get_thread_description (thread, &name_utf16)))
    goto beach;

  if (name_utf16[0] == L'\0')
    goto beach;

  name = g_utf16_to_utf8 ((const gunichar2 *) name_utf16, -1, NULL, NULL, NULL);

beach:
  if (name_utf16 != NULL)
    LocalFree (name_utf16);

  return name;
}

GumAddress
gum_windows_query_thread_entrypoint_routine (HANDLE thread)
{
  GumAddress routine = 0;

  (gum_get_query_information_thread ()) (thread,
      GUM_THREAD_QUERY_SET_WIN32_START_ADDRESS, &routine, sizeof (routine),
      NULL);

  return routine;
}

static GumQueryInformationThreadFunc
gum_get_query_information_thread (void)
{
  static gsize initialized = FALSE;
  static GumQueryInformationThreadFunc query_information_thread = NULL;

  if (g_once_init_enter (&initialized))
  {
    query_information_thread = (GumQueryInformationThreadFunc) GetProcAddress (
        GetModuleHandleW (L"ntdll.dll"),
        "NtQueryInformationThread");

    g_once_init_leave (&initialized, TRUE);
  }

  return query_information_thread;
}

void
gum_windows_parse_context (const CONTEXT * context,
                           GumCpuContext * cpu_context)
{
#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
  cpu_context->eip = context->Eip;

  cpu_context->edi = context->Edi;
  cpu_context->esi = context->Esi;
  cpu_context->ebp = context->Ebp;
  cpu_context->esp = context->Esp;
  cpu_context->ebx = context->Ebx;
  cpu_context->edx = context->Edx;
  cpu_context->ecx = context->Ecx;
  cpu_context->eax = context->Eax;

  cpu_context->xmm = NULL;
#elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
  cpu_context->rip = context->Rip;

  cpu_context->r15 = context->R15;
  cpu_context->r14 = context->R14;
  cpu_context->r13 = context->R13;
  cpu_context->r12 = context->R12;
  cpu_context->r11 = context->R11;
  cpu_context->r10 = context->R10;
  cpu_context->r9 = context->R9;
  cpu_context->r8 = context->R8;

  cpu_context->rdi = context->Rdi;
  cpu_context->rsi = context->Rsi;
  cpu_context->rbp = context->Rbp;
  cpu_context->rsp = context->Rsp;
  cpu_context->rbx = context->Rbx;
  cpu_context->rdx = context->Rdx;
  cpu_context->rcx = context->Rcx;
  cpu_context->rax = context->Rax;

  cpu_context->xmm = NULL;
#else
  guint i;

  cpu_context->pc = context->Pc;
  cpu_context->sp = context->Sp;
  cpu_context->nzcv = context->Cpsr;

  cpu_context->x[0] = context->X0;
  cpu_context->x[1] = context->X1;
  cpu_context->x[2] = context->X2;
  cpu_context->x[3] = context->X3;
  cpu_context->x[4] = context->X4;
  cpu_context->x[5] = context->X5;
  cpu_context->x[6] = context->X6;
  cpu_context->x[7] = context->X7;
  cpu_context->x[8] = context->X8;
  cpu_context->x[9] = context->X9;
  cpu_context->x[10] = context->X10;
  cpu_context->x[11] = context->X11;
  cpu_context->x[12] = context->X12;
  cpu_context->x[13] = context->X13;
  cpu_context->x[14] = context->X14;
  cpu_context->x[15] = context->X15;
  cpu_context->x[16] = context->X16;
  cpu_context->x[17] = context->X17;
  cpu_context->x[18] = context->X18;
  cpu_context->x[19] = context->X19;
  cpu_context->x[20] = context->X20;
  cpu_context->x[21] = context->X21;
  cpu_context->x[22] = context->X22;
  cpu_context->x[23] = context->X23;
  cpu_context->x[24] = context->X24;
  cpu_context->x[25] = context->X25;
  cpu_context->x[26] = context->X26;
  cpu_context->x[27] = context->X27;
  cpu_context->x[28] = context->X28;
  cpu_context->fp = context->Fp;
  cpu_context->lr = context->Lr;

  for (i = 0; i != G_N_ELEMENTS (cpu_context->v); i++)
    memcpy (cpu_context->v[i].q, context->V[i].B, 16);
#endif
}

void
gum_windows_unparse_context (const GumCpuContext * cpu_context,
                             CONTEXT * context)
{
#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
  context->Eip = cpu_context->eip;

  context->Edi = cpu_context->edi;
  context->Esi = cpu_context->esi;
  context->Ebp = cpu_context->ebp;
  context->Esp = cpu_context->esp;
  context->Ebx = cpu_context->ebx;
  context->Edx = cpu_context->edx;
  context->Ecx = cpu_context->ecx;
  context->Eax = cpu_context->eax;
#elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
  context->Rip = cpu_context->rip;

  context->R15 = cpu_context->r15;
  context->R14 = cpu_context->r14;
  context->R13 = cpu_context->r13;
  context->R12 = cpu_context->r12;
  context->R11 = cpu_context->r11;
  context->R10 = cpu_context->r10;
  context->R9 = cpu_context->r9;
  context->R8 = cpu_context->r8;

  context->Rdi = cpu_context->rdi;
  context->Rsi = cpu_context->rsi;
  context->Rbp = cpu_context->rbp;
  context->Rsp = cpu_context->rsp;
  context->Rbx = cpu_context->rbx;
  context->Rdx = cpu_context->rdx;
  context->Rcx = cpu_context->rcx;
  context->Rax = cpu_context->rax;
#else
  guint i;

  context->Pc = cpu_context->pc;
  context->Sp = cpu_context->sp;
  context->Cpsr = cpu_context->nzcv;

  context->X0 = cpu_context->x[0];
  context->X1 = cpu_context->x[1];
  context->X2 = cpu_context->x[2];
  context->X3 = cpu_context->x[3];
  context->X4 = cpu_context->x[4];
  context->X5 = cpu_context->x[5];
  context->X6 = cpu_context->x[6];
  context->X7 = cpu_context->x[7];
  context->X8 = cpu_context->x[8];
  context->X9 = cpu_context->x[9];
  context->X10 = cpu_context->x[10];
  context->X11 = cpu_context->x[11];
  context->X12 = cpu_context->x[12];
  context->X13 = cpu_context->x[13];
  context->X14 = cpu_context->x[14];
  context->X15 = cpu_context->x[15];
  context->X16 = cpu_context->x[16];
  context->X17 = cpu_context->x[17];
  context->X18 = cpu_context->x[18];
  context->X19 = cpu_context->x[19];
  context->X20 = cpu_context->x[20];
  context->X21 = cpu_context->x[21];
  context->X22 = cpu_context->x[22];
  context->X23 = cpu_context->x[23];
  context->X24 = cpu_context->x[24];
  context->X25 = cpu_context->x[25];
  context->X26 = cpu_context->x[26];
  context->X27 = cpu_context->x[27];
  context->X28 = cpu_context->x[28];
  context->Fp = cpu_context->fp;
  context->Lr = cpu_context->lr;

  for (i = 0; i != G_N_ELEMENTS (cpu_context->v); i++)
    memcpy (context->V[i].B, cpu_context->v[i].q, 16);
#endif
}

#ifndef _MSC_VER
# pragma GCC diagnostic pop
#endif
