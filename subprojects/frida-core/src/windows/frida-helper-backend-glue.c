#include "frida-helper-backend.h"

#include <gio/gio.h>
#include <gum/gum.h>
#include <gum/arch-arm64/gumarm64writer.h>
#include <gum/arch-x86/gumx86writer.h>

#include <windows.h>
#include <tlhelp32.h>
#include <strsafe.h>

#define CHECK_OS_RESULT(n1, cmp, n2, op) \
  if (!((n1) cmp (n2))) \
  { \
    failed_operation = op; \
    goto os_failure; \
  }

#define CHECK_NT_RESULT(n1, cmp, n2, op) \
  if (!((n1) cmp (n2))) \
  { \
    failed_operation = op; \
    goto nt_failure; \
  }

typedef struct _FridaInjectInstance FridaInjectInstance;
typedef struct _FridaInjectionDetails FridaInjectionDetails;
typedef struct _FridaRemoteWorkerContext FridaRemoteWorkerContext;

struct _FridaInjectInstance
{
  HANDLE process_handle;
  gpointer free_address;
  gpointer stay_resident_address;
};

struct _FridaInjectionDetails
{
  HANDLE process_handle;
  const WCHAR * dll_path;
  const gchar * entrypoint_name;
  const gchar * entrypoint_data;
};

struct _FridaRemoteWorkerContext
{
  gboolean stay_resident;

  gpointer load_library_impl;
  gpointer get_proc_address_impl;
  gpointer free_library_impl;
  gpointer virtual_free_impl;
  gpointer get_last_error_impl;

  WCHAR dll_path[MAX_PATH + 1];
  gchar entrypoint_name[256];
  gchar entrypoint_data[MAX_PATH + 1];

  gpointer entrypoint;
  gpointer argument;
};

typedef struct _RtlClientId RtlClientId;

struct _RtlClientId
{
  SIZE_T unique_process;
  SIZE_T unique_thread;
};

typedef NTSTATUS (WINAPI * RtlCreateUserThreadFunc) (HANDLE process, SECURITY_DESCRIPTOR * sec,
    BOOLEAN create_suspended, ULONG stack_zero_bits, SIZE_T * stack_reserved, SIZE_T * stack_commit,
    LPTHREAD_START_ROUTINE start_address, LPVOID parameter, HANDLE * thread_handle, RtlClientId * result);

static void frida_propagate_open_process_error (guint32 pid, DWORD os_error, GError ** error);
static gboolean frida_enable_debug_privilege (void);

static gboolean frida_remote_worker_context_init (FridaRemoteWorkerContext * rwc, FridaInjectionDetails * details, GError ** error);
static gsize frida_remote_worker_context_emit_payload (FridaRemoteWorkerContext * rwc, gpointer code);
static void frida_remote_worker_context_destroy (FridaRemoteWorkerContext * rwc, FridaInjectionDetails * details);

static gboolean frida_remote_worker_context_has_resolved_all_kernel32_functions (const FridaRemoteWorkerContext * rwc);
static gboolean frida_remote_worker_context_collect_kernel32_export (const GumExportDetails * details, gpointer user_data);

static gboolean frida_file_exists_and_is_readable (const WCHAR * filename);

void
_frida_windows_helper_backend_inject_library_file (guint32 pid, const gchar * path, const gchar * entrypoint, const gchar * data,
    void ** inject_instance, void ** waitable_thread_handle, GError ** error)
{
  gboolean success = FALSE;
  const gchar * failed_operation;
  NTSTATUS nt_status;
  FridaInjectionDetails details;
  DWORD desired_access;
  HANDLE thread_handle = NULL;
  gboolean rwc_initialized = FALSE;
  FridaRemoteWorkerContext rwc;
  FridaInjectInstance * instance;

  details.dll_path = (WCHAR *) g_utf8_to_utf16 (path, -1, NULL, NULL, NULL);
  details.entrypoint_name = entrypoint;
  details.entrypoint_data = data;
  details.process_handle = NULL;

  if (!frida_file_exists_and_is_readable (details.dll_path))
    goto invalid_path;

  frida_enable_debug_privilege ();

  desired_access =
      PROCESS_DUP_HANDLE    | /* duplicatable handle                  */
      PROCESS_VM_OPERATION  | /* for VirtualProtectEx and mem access  */
      PROCESS_VM_READ       | /*   ReadProcessMemory                  */
      PROCESS_VM_WRITE      | /*   WriteProcessMemory                 */
      PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION;

  details.process_handle = OpenProcess (desired_access, FALSE, pid);
  CHECK_OS_RESULT (details.process_handle, !=, NULL, "OpenProcess");

  if (!frida_remote_worker_context_init (&rwc, &details, error))
    goto beach;
  rwc_initialized = TRUE;

  thread_handle = CreateRemoteThread (details.process_handle, NULL, 0, GUM_POINTER_TO_FUNCPTR (LPTHREAD_START_ROUTINE, rwc.entrypoint),
      rwc.argument, 0, NULL);
  if (thread_handle == NULL)
  {
    RtlCreateUserThreadFunc rtl_create_user_thread;
    RtlClientId client_id;

    rtl_create_user_thread = (RtlCreateUserThreadFunc) GetProcAddress (GetModuleHandleW (L"ntdll.dll"), "RtlCreateUserThread");
    nt_status = rtl_create_user_thread (details.process_handle, NULL, FALSE, 0, NULL, NULL,
        GUM_POINTER_TO_FUNCPTR (LPTHREAD_START_ROUTINE, rwc.entrypoint), rwc.argument, &thread_handle, &client_id);
    CHECK_NT_RESULT (nt_status, == , 0, "RtlCreateUserThread");
  }

  instance = g_slice_new (FridaInjectInstance);
  instance->process_handle = details.process_handle;
  details.process_handle = NULL;
  instance->free_address = rwc.entrypoint;
  instance->stay_resident_address = (guint8 *) rwc.argument + G_STRUCT_OFFSET (FridaRemoteWorkerContext, stay_resident);
  *inject_instance = instance;

  *waitable_thread_handle = thread_handle;
  thread_handle = NULL;

  success = TRUE;

  goto beach;

  /* ERRORS */
invalid_path:
  {
    g_set_error (error,
        FRIDA_ERROR,
        FRIDA_ERROR_INVALID_ARGUMENT,
        "Unable to find DLL at '%s'",
        path);
    goto beach;
  }
os_failure:
  {
    DWORD os_error;

    os_error = GetLastError ();

    if (details.process_handle == NULL)
    {
      frida_propagate_open_process_error (pid, os_error, error);
    }
    else
    {
      g_set_error (error,
          FRIDA_ERROR,
          (os_error == ERROR_ACCESS_DENIED) ? FRIDA_ERROR_PERMISSION_DENIED : FRIDA_ERROR_NOT_SUPPORTED,
          "Unexpected error while attaching to process with pid %u (%s returned 0x%08lx)",
          pid, failed_operation, os_error);
    }

    goto beach;
  }
nt_failure:
  {
    gint code;

    if (nt_status == 0xC0000022) /* STATUS_ACCESS_DENIED */
      code = FRIDA_ERROR_PERMISSION_DENIED;
    else
      code = FRIDA_ERROR_NOT_SUPPORTED;

    g_set_error (error,
        FRIDA_ERROR,
        code,
        "Unexpected error while attaching to process with pid %u (%s returned 0x%08lx)",
        pid, failed_operation, nt_status);
    goto beach;
  }

beach:
  {
    if (!success && rwc_initialized)
      frida_remote_worker_context_destroy (&rwc, &details);

    if (thread_handle != NULL)
      CloseHandle (thread_handle);

    if (details.process_handle != NULL)
      CloseHandle (details.process_handle);

    g_free ((gpointer) details.dll_path);
  }
}

void
_frida_windows_helper_backend_free_inject_instance (void * inject_instance, gboolean * is_resident)
{
  FridaInjectInstance * instance = inject_instance;
  gboolean stay_resident;
  SIZE_T n_bytes_read;

  if (ReadProcessMemory (instance->process_handle, instance->stay_resident_address, &stay_resident, sizeof (stay_resident),
      &n_bytes_read) && n_bytes_read == sizeof (stay_resident))
  {
    *is_resident = stay_resident;
  }
  else
  {
    *is_resident = FALSE;
  }

  VirtualFreeEx (instance->process_handle, instance->free_address, 0, MEM_RELEASE);

  CloseHandle (instance->process_handle);

  g_slice_free (FridaInjectInstance, instance);
}

static void
frida_propagate_open_process_error (guint32 pid, DWORD os_error, GError ** error)
{
  if (os_error == ERROR_INVALID_PARAMETER)
  {
    g_set_error (error,
        FRIDA_ERROR,
        FRIDA_ERROR_PROCESS_NOT_FOUND,
        "Unable to find process with pid %u",
        pid);
  }
  else if (os_error == ERROR_ACCESS_DENIED)
  {
    g_set_error (error,
        FRIDA_ERROR,
        FRIDA_ERROR_PERMISSION_DENIED,
        "Unable to access process with pid %u from the current user account",
        pid);
  }
  else
  {
    g_set_error (error,
        FRIDA_ERROR,
        FRIDA_ERROR_NOT_SUPPORTED,
        "Unable to access process with pid %u due to an unexpected error (OpenProcess returned 0x%08lx)",
        pid, os_error);
  }
}

static gboolean
frida_enable_debug_privilege (void)
{
  static gboolean enabled = FALSE;
  gboolean success = FALSE;
  HANDLE token = NULL;
  TOKEN_PRIVILEGES privileges;
  LUID_AND_ATTRIBUTES * p = &privileges.Privileges[0];

  if (enabled)
    return TRUE;

  if (!OpenProcessToken (GetCurrentProcess (), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &token))
    goto beach;

  privileges.PrivilegeCount = 1;
  if (!LookupPrivilegeValueW (NULL, L"SeDebugPrivilege", &p->Luid))
    goto beach;
  p->Attributes = SE_PRIVILEGE_ENABLED;

  if (!AdjustTokenPrivileges (token, FALSE, &privileges, 0, NULL, NULL))
    goto beach;

  if (GetLastError () == ERROR_NOT_ALL_ASSIGNED)
    goto beach;

  enabled = TRUE;
  success = TRUE;

beach:
  if (token != NULL)
    CloseHandle (token);

  return success;
}

static gboolean
frida_remote_worker_context_init (FridaRemoteWorkerContext * rwc, FridaInjectionDetails * details, GError ** error)
{
  gpointer code;
  guint code_size;
  GumModule * kernel32;
  SIZE_T page_size, alloc_size;
  DWORD old_protect;

  gum_init ();

  page_size = gum_query_page_size ();

  /* Executable so debugger can be used to inspect code */
  code = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RWX);
  code_size = frida_remote_worker_context_emit_payload (rwc, code);

  memset (rwc, 0, sizeof (FridaRemoteWorkerContext));

  kernel32 = gum_process_find_module_by_name ("kernel32.dll");
  gum_module_enumerate_exports (kernel32, frida_remote_worker_context_collect_kernel32_export, rwc);
  g_object_unref (kernel32);
  if (!frida_remote_worker_context_has_resolved_all_kernel32_functions (rwc))
    goto failed_to_resolve_kernel32_functions;

  StringCbCopyW (rwc->dll_path, sizeof (rwc->dll_path), details->dll_path);
  StringCbCopyA (rwc->entrypoint_name, sizeof (rwc->entrypoint_name), details->entrypoint_name);
  StringCbCopyA (rwc->entrypoint_data, sizeof (rwc->entrypoint_data), details->entrypoint_data);

  g_assert (code_size <= page_size);

  alloc_size = page_size + sizeof (FridaRemoteWorkerContext);
  rwc->entrypoint = VirtualAllocEx (details->process_handle, NULL, alloc_size, MEM_COMMIT, PAGE_READWRITE);
  if (rwc->entrypoint == NULL)
    goto virtual_alloc_ex_failed;

  if (!WriteProcessMemory (details->process_handle, rwc->entrypoint, code, code_size, NULL))
    goto write_process_memory_failed;

  rwc->argument = GSIZE_TO_POINTER (GPOINTER_TO_SIZE (rwc->entrypoint) + page_size);
  if (!WriteProcessMemory (details->process_handle, rwc->argument, rwc, sizeof (FridaRemoteWorkerContext), NULL))
    goto write_process_memory_failed;

  if (!VirtualProtectEx (details->process_handle, rwc->entrypoint, page_size, PAGE_EXECUTE_READ, &old_protect))
    goto virtual_protect_ex_failed;

  gum_memory_free (code, page_size);

  return TRUE;

  /* ERRORS */
failed_to_resolve_kernel32_functions:
  {
    g_set_error (error,
        FRIDA_ERROR,
        FRIDA_ERROR_NOT_SUPPORTED,
        "Unexpected error while resolving kernel32 functions");
    goto error_common;
  }
virtual_alloc_ex_failed:
  {
    g_set_error (error,
        FRIDA_ERROR,
        FRIDA_ERROR_NOT_SUPPORTED,
        "Unexpected error allocating memory in target process (VirtualAllocEx returned 0x%08lx)",
        GetLastError ());
    goto error_common;
  }
write_process_memory_failed:
  {
    g_set_error (error,
        FRIDA_ERROR,
        FRIDA_ERROR_NOT_SUPPORTED,
        "Unexpected error writing to memory in target process (WriteProcessMemory returned 0x%08lx)",
        GetLastError ());
    goto error_common;
  }
virtual_protect_ex_failed:
  {
    g_set_error (error,
        FRIDA_ERROR,
        FRIDA_ERROR_NOT_SUPPORTED,
        "Unexpected error changing memory permission in target process (VirtualProtectEx returned 0x%08lx)",
        GetLastError ());
    goto error_common;
  }
error_common:
  {
    frida_remote_worker_context_destroy (rwc, details);
    gum_memory_free (code, page_size);

    return FALSE;
  }
}

#define EMIT_ARM64_LOAD(reg, field) \
    gum_arm64_writer_put_ldr_reg_reg_offset (&cw, ARM64_REG_##reg, ARM64_REG_X20, G_STRUCT_OFFSET (FridaRemoteWorkerContext, field))
#define EMIT_ARM64_LOAD_ADDRESS_OF(reg, field) \
    gum_arm64_writer_put_add_reg_reg_imm (&cw, ARM64_REG_##reg, ARM64_REG_X20, G_STRUCT_OFFSET (FridaRemoteWorkerContext, field))
#define EMIT_ARM64_MOVE(dstreg, srcreg) \
    gum_arm64_writer_put_mov_reg_reg (&cw, ARM64_REG_##dstreg, ARM64_REG_##srcreg)
#define EMIT_ARM64_CALL(reg) \
    gum_arm64_writer_put_blr_reg_no_auth (&cw, ARM64_REG_##reg)

static gsize
frida_remote_worker_context_emit_payload (FridaRemoteWorkerContext * rwc, gpointer code)
{
  gsize code_size;
  const gchar * loadlibrary_failed = "loadlibrary_failed";
  const gchar * skip_unload = "skip_unload";
  const gchar * return_result = "return_result";
#ifdef HAVE_ARM64
  GumArm64Writer cw;

  gum_arm64_writer_init (&cw, code);

  gum_arm64_writer_put_push_reg_reg (&cw, ARM64_REG_FP, ARM64_REG_LR);
  gum_arm64_writer_put_mov_reg_reg (&cw, ARM64_REG_FP, ARM64_REG_SP);
  gum_arm64_writer_put_push_reg_reg (&cw, ARM64_REG_X19, ARM64_REG_X20);

  /* x20 = (FridaRemoteWorkerContext *) lpParameter */
  EMIT_ARM64_MOVE (X20, X0);

  /* x19 = LoadLibrary (x20->dll_path) */
  EMIT_ARM64_LOAD_ADDRESS_OF (X0, dll_path);
  EMIT_ARM64_LOAD (X8, load_library_impl);
  EMIT_ARM64_CALL (X8);
  gum_arm64_writer_put_cbz_reg_label (&cw, ARM64_REG_X0, loadlibrary_failed);
  EMIT_ARM64_MOVE (X19, X0);

  /* x8 = GetProcAddress (x19, x20->entrypoint_name) */
  EMIT_ARM64_MOVE (X0, X19);
  EMIT_ARM64_LOAD_ADDRESS_OF (X1, entrypoint_name);
  EMIT_ARM64_LOAD (X8, get_proc_address_impl);
  EMIT_ARM64_CALL (X8);
  EMIT_ARM64_MOVE (X8, X0);

  /* x8 (x20->entrypoint_data, &x20->stay_resident, NULL) */
  EMIT_ARM64_LOAD_ADDRESS_OF (X0, entrypoint_data);
  EMIT_ARM64_LOAD_ADDRESS_OF (X1, stay_resident);
  EMIT_ARM64_MOVE (X2, XZR);
  EMIT_ARM64_CALL (X8);

  /* if (!x20->stay_resident) { */
  EMIT_ARM64_LOAD (X0, stay_resident);
  gum_arm64_writer_put_cbnz_reg_label (&cw, ARM64_REG_X0, skip_unload);

  /* FreeLibrary (xsi) */
  EMIT_ARM64_MOVE (X0, X19);
  EMIT_ARM64_LOAD (X8, free_library_impl);
  EMIT_ARM64_CALL (X8);

  /* } */
  gum_arm64_writer_put_label (&cw, skip_unload);

  /* result = ERROR_SUCCESS */
  EMIT_ARM64_MOVE (X0, XZR);
  gum_arm64_writer_put_b_label (&cw, return_result);

  gum_arm64_writer_put_label (&cw, loadlibrary_failed);
  /* result = GetLastError() */
  EMIT_ARM64_LOAD (X8, get_last_error_impl);
  EMIT_ARM64_CALL (X8);

  gum_arm64_writer_put_label (&cw, return_result);
  gum_arm64_writer_put_pop_reg_reg (&cw, ARM64_REG_X19, ARM64_REG_X20);
  gum_arm64_writer_put_pop_reg_reg (&cw, ARM64_REG_FP, ARM64_REG_LR);
  gum_arm64_writer_put_ret (&cw);

  gum_arm64_writer_flush (&cw);
  code_size = gum_arm64_writer_offset (&cw);
  gum_arm64_writer_clear (&cw);
#else
  GumX86Writer cw;

  gum_x86_writer_init (&cw, code);

  /* Will clobber these */
  gum_x86_writer_put_push_reg (&cw, GUM_X86_XBX);
  gum_x86_writer_put_push_reg (&cw, GUM_X86_XSI);
  gum_x86_writer_put_push_reg (&cw, GUM_X86_XDI); /* Alignment padding */

  /* xbx = (FridaRemoteWorkerContext *) lpParameter */
#if GLIB_SIZEOF_VOID_P == 4
  gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_EBX, GUM_X86_ESP, (3 + 1) * sizeof (gpointer));
#else
  gum_x86_writer_put_mov_reg_reg (&cw, GUM_X86_RBX, GUM_X86_RCX);
#endif

  /* xsi = LoadLibrary (xbx->dll_path) */
  gum_x86_writer_put_lea_reg_reg_offset (&cw, GUM_X86_XCX,
      GUM_X86_XBX, G_STRUCT_OFFSET (FridaRemoteWorkerContext, dll_path));
  gum_x86_writer_put_call_reg_offset_ptr_with_arguments (&cw, GUM_CALL_SYSAPI,
      GUM_X86_XBX, G_STRUCT_OFFSET (FridaRemoteWorkerContext, load_library_impl),
      1,
      GUM_ARG_REGISTER, GUM_X86_XCX);
  gum_x86_writer_put_test_reg_reg (&cw, GUM_X86_XAX, GUM_X86_XAX);
  gum_x86_writer_put_jcc_near_label (&cw, X86_INS_JE, loadlibrary_failed, GUM_UNLIKELY);
  gum_x86_writer_put_mov_reg_reg (&cw, GUM_X86_XSI, GUM_X86_XAX);

  /* xax = GetProcAddress (xsi, xbx->entrypoint_name) */
  gum_x86_writer_put_lea_reg_reg_offset (&cw, GUM_X86_XDX,
      GUM_X86_XBX, G_STRUCT_OFFSET (FridaRemoteWorkerContext, entrypoint_name));
  gum_x86_writer_put_call_reg_offset_ptr_with_arguments (&cw, GUM_CALL_SYSAPI,
      GUM_X86_XBX, G_STRUCT_OFFSET (FridaRemoteWorkerContext, get_proc_address_impl),
      2,
      GUM_ARG_REGISTER, GUM_X86_XSI,
      GUM_ARG_REGISTER, GUM_X86_XDX);

  /* xax (xbx->entrypoint_data, &xbx->stay_resident, NULL) */
  gum_x86_writer_put_lea_reg_reg_offset (&cw, GUM_X86_XCX,
      GUM_X86_XBX, G_STRUCT_OFFSET (FridaRemoteWorkerContext, entrypoint_data));
  gum_x86_writer_put_lea_reg_reg_offset (&cw, GUM_X86_XDX,
      GUM_X86_XBX, G_STRUCT_OFFSET (FridaRemoteWorkerContext, stay_resident));
  gum_x86_writer_put_call_reg_with_arguments (&cw, GUM_CALL_CAPI, GUM_X86_XAX,
      3,
      GUM_ARG_REGISTER, GUM_X86_XCX,
      GUM_ARG_REGISTER, GUM_X86_XDX,
      GUM_ARG_ADDRESS, GUM_ADDRESS (0));

  /* if (!xbx->stay_resident) { */
  gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_EAX,
      GUM_X86_XBX, G_STRUCT_OFFSET (FridaRemoteWorkerContext, stay_resident));
  gum_x86_writer_put_test_reg_reg (&cw, GUM_X86_EAX, GUM_X86_EAX);
  gum_x86_writer_put_jcc_short_label (&cw, X86_INS_JNE, skip_unload, GUM_NO_HINT);

  /* FreeLibrary (xsi) */
  gum_x86_writer_put_call_reg_offset_ptr_with_arguments (&cw, GUM_CALL_SYSAPI,
      GUM_X86_XBX, G_STRUCT_OFFSET (FridaRemoteWorkerContext, free_library_impl),
      1,
      GUM_ARG_REGISTER, GUM_X86_XSI);

  /* } */
  gum_x86_writer_put_label (&cw, skip_unload);

  /* result = ERROR_SUCCESS */
  gum_x86_writer_put_xor_reg_reg (&cw, GUM_X86_EAX, GUM_X86_EAX);
  gum_x86_writer_put_jmp_short_label (&cw, return_result);

  gum_x86_writer_put_label (&cw, loadlibrary_failed);
  /* result = GetLastError() */
  gum_x86_writer_put_call_reg_offset_ptr_with_arguments (&cw, GUM_CALL_SYSAPI,
      GUM_X86_XBX, G_STRUCT_OFFSET (FridaRemoteWorkerContext, get_last_error_impl),
      0);

  gum_x86_writer_put_label (&cw, return_result);
  gum_x86_writer_put_pop_reg (&cw, GUM_X86_XDI);
  gum_x86_writer_put_pop_reg (&cw, GUM_X86_XSI);
  gum_x86_writer_put_pop_reg (&cw, GUM_X86_XBX);
  gum_x86_writer_put_ret (&cw);

  gum_x86_writer_flush (&cw);
  code_size = gum_x86_writer_offset (&cw);
  gum_x86_writer_clear (&cw);
#endif

  return code_size;
}

static void
frida_remote_worker_context_destroy (FridaRemoteWorkerContext * rwc, FridaInjectionDetails * details)
{
  if (rwc->entrypoint != NULL)
  {
    VirtualFreeEx (details->process_handle, rwc->entrypoint, 0, MEM_RELEASE);
    rwc->entrypoint = NULL;
  }
}

static gboolean
frida_remote_worker_context_has_resolved_all_kernel32_functions (const FridaRemoteWorkerContext * rwc)
{
  return (rwc->load_library_impl != NULL) && (rwc->get_proc_address_impl != NULL) &&
      (rwc->free_library_impl != NULL) && (rwc->virtual_free_impl != NULL);
}

static gboolean
frida_remote_worker_context_collect_kernel32_export (const GumExportDetails * details, gpointer user_data)
{
  FridaRemoteWorkerContext * rwc = user_data;

  if (details->type != GUM_EXPORT_FUNCTION)
    return TRUE;

  if (strcmp (details->name, "LoadLibraryW") == 0)
    rwc->load_library_impl = GSIZE_TO_POINTER (details->address);
  else if (strcmp (details->name, "GetProcAddress") == 0)
    rwc->get_proc_address_impl = GSIZE_TO_POINTER (details->address);
  else if (strcmp (details->name, "FreeLibrary") == 0)
    rwc->free_library_impl = GSIZE_TO_POINTER (details->address);
  else if (strcmp (details->name, "VirtualFree") == 0)
    rwc->virtual_free_impl = GSIZE_TO_POINTER (details->address);
  else if (strcmp (details->name, "GetLastError") == 0)
    rwc->get_last_error_impl = GSIZE_TO_POINTER (details->address);

  return TRUE;
}

static gboolean
frida_file_exists_and_is_readable (const WCHAR * filename)
{
  HANDLE file;

  file = CreateFileW (filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
    NULL, OPEN_EXISTING, 0, NULL);
  if (file == INVALID_HANDLE_VALUE)
    return FALSE;
  CloseHandle (file);

  return TRUE;
}
