// The Windows part of Gum's platform backend, for Win9x and NT. Kernel code is writable on
// both, thus this backend needs no alias. Only the position of the modules is different.

use crate::{
    bindings::{
        _GumPageProtection_GUM_PAGE_EXECUTE, _GumRwxSupport_GUM_RWX_FULL, GumMemoryRange,
        GumModuleRegistry, GumPageProtection, GumRwxSupport, g_object_unref, gboolean, gpointer,
        gsize, guint, gum_barebone_register_module, gum_mprotect, GumFoundRangeFunc,
        GumFoundThreadFunc, GumModifyThreadFlags, GumModifyThreadFunc, GumRangeDetails,
        GumThreadDetails, GumThreadFlags, GumThreadId, GumThreadRegistry,
        gum_thread_details_copy,
    },
    gum::{self, FoundExportCallback},
    kernel,
};
use crate::bindings::{GumCpuContext, GumThreadFlags_GUM_THREAD_FLAGS_CPU_CONTEXT};
use alloc::format;
use core::ptr;

#[unsafe(no_mangle)]
pub extern "C" fn gum_barebone_query_platform() -> *const crate::bindings::gchar {
    c"windows".as_ptr() as *const crate::bindings::gchar
}

#[unsafe(no_mangle)]
pub extern "C" fn gum_barebone_query_stack_size() -> crate::bindings::gsize {
    if crate::on_js_thread() {
        kernel::THREAD_STACK_SIZE as crate::bindings::gsize
    } else {
        0
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn gum_query_rwx_support() -> GumRwxSupport {
    _GumRwxSupport_GUM_RWX_FULL
}

#[unsafe(no_mangle)]
pub extern "C" fn gum_memory_can_remap_writable() -> gboolean {
    #[cfg(feature = "win9x")]
    {
        crate::win9x::in_copy() as gboolean
    }
    #[cfg(not(feature = "win9x"))]
    {
        0
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn gum_memory_try_remap_writable_pages(
    first_page: gpointer,
    _n_pages: guint,
) -> gpointer {
    #[cfg(feature = "win9x")]
    if crate::win9x::in_copy() {
        return crate::win9x_user::take_writes_on(first_page as *mut u8, _n_pages) as gpointer;
    }

    first_page
}

#[unsafe(no_mangle)]
pub extern "C" fn gum_memory_dispose_writable_pages(_writable: gpointer, _n_pages: guint) {
    #[cfg(feature = "win9x")]
    if crate::win9x::in_copy() {
        crate::win9x_user::make_the_writes(_writable as *mut u8, _n_pages);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn gum_memory_query_protection(
    address: gpointer,
    prot: *mut GumPageProtection,
) -> gboolean {
    let protection = kernel::protection_at(address as usize);
    if protection == 0 {
        return 0;
    }

    unsafe { *prot = protection as GumPageProtection };
    1
}

#[unsafe(no_mangle)]
pub extern "C" fn gum_try_mprotect(
    address: gpointer,
    size: gsize,
    prot: GumPageProtection,
) -> gboolean {
    if kernel::protect(address as u64, size as usize, prot as u32) {
        1
    } else {
        0
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn gum_memory_allocate(
    _address: gpointer,
    size: gsize,
    _alignment: gsize,
    prot: GumPageProtection,
) -> gpointer {
    let ptr = kernel::alloc_code(size as usize);
    if ptr.is_null() {
        return ptr::null_mut();
    }

    unsafe {
        core::ptr::write_bytes(ptr, 0, size as usize);
        if (prot & _GumPageProtection_GUM_PAGE_EXECUTE) != 0 {
            gum::register_slab(ptr as u64, size as usize);
            gum_mprotect(ptr as gpointer, size, prot);
        }
    }

    ptr as gpointer
}

#[unsafe(no_mangle)]
pub extern "C" fn gum_memory_free(address: gpointer, size: gsize) -> gboolean {
    if gum::is_agent_slab(address as u64) {
        gum::unregister_slab(address as u64);
    }
    kernel::free_code(address as *mut u8, size as usize);
    1
}

#[unsafe(no_mangle)]
pub extern "C" fn gum_barebone_on_registry_activating(registry: *mut GumModuleRegistry) {
    #[cfg(any(feature = "win9x", feature = "winnt"))]
    if crate::running_in_a_process() {
        register_the_process_modules(registry);
        return;
    }

    unsafe {
        let modules = &*core::ptr::addr_of!(crate::MODULE_INFO);

        for module_info in modules.iter() {
            let path = format!("{}{}", kernel::MODULE_DIRECTORY, module_info.name);
            let range = GumMemoryRange {
                base_address: module_info.offset as u64,
                size: module_info.size as gsize,
            };

            let module = gum::gum_native_module_new(&path, &module_info.version, &range);
            gum_barebone_register_module(registry, module);
            g_object_unref(module as gpointer);
        }
    }
}

// A script can hook a library the moment it arrives, thus stand in front of the loader and say
// what came and what went.
#[cfg(any(feature = "win9x", feature = "winnt"))]
pub(crate) fn watch_the_loader() {
    let Some(entries) = kernel::loader_entry_points() else {
        return;
    };

    unsafe {
        let interceptor = crate::bindings::gum_interceptor_obtain();
        crate::bindings::gum_interceptor_begin_transaction(interceptor);
        crate::bindings::gum_interceptor_replace(interceptor, entries.load as gpointer,
            kernel::on_module_load as gpointer, &raw mut LOADER_LOAD, ptr::null());
        if entries.load_with_flags != 0 {
            crate::bindings::gum_interceptor_replace(interceptor,
                entries.load_with_flags as gpointer, kernel::on_module_load_with_flags as gpointer,
                &raw mut LOADER_LOAD_WITH_FLAGS, ptr::null());
        }
        crate::bindings::gum_interceptor_replace(interceptor, entries.unload as gpointer,
            kernel::on_module_unload as gpointer, &raw mut LOADER_UNLOAD, ptr::null());

        crate::bindings::gum_interceptor_end_transaction(interceptor);
    }
}

#[cfg(any(feature = "win9x", feature = "winnt"))]
pub(crate) fn forget_the_loader() {
    let Some(entries) = kernel::loader_entry_points() else {
        return;
    };

    unsafe {
        let interceptor = crate::bindings::gum_interceptor_obtain();
        crate::bindings::gum_interceptor_begin_transaction(interceptor);
        crate::bindings::gum_interceptor_revert(interceptor, entries.load as gpointer);
        if entries.load_with_flags != 0 {
            crate::bindings::gum_interceptor_revert(interceptor,
                entries.load_with_flags as gpointer);
        }
        crate::bindings::gum_interceptor_revert(interceptor, entries.unload as gpointer);
        crate::bindings::gum_interceptor_end_transaction(interceptor);
    }
}

#[cfg(any(feature = "win9x", feature = "winnt"))]
pub(crate) fn loader_load() -> gpointer {
    unsafe { LOADER_LOAD }
}

#[cfg(any(feature = "win9x", feature = "winnt"))]
pub(crate) fn loader_load_with_flags() -> gpointer {
    unsafe { LOADER_LOAD_WITH_FLAGS }
}

#[cfg(any(feature = "win9x", feature = "winnt"))]
pub(crate) fn loader_unload() -> gpointer {
    unsafe { LOADER_UNLOAD }
}

#[cfg(feature = "win9x")]
pub(crate) fn module_arrived(base: u64, path: &str) {
    if !known_mut().insert(base) {
        return;
    }

    let range = GumMemoryRange {
        base_address: base,
        size: kernel::image_size(base as u32) as gsize,
    };

    unsafe {
        let registry = crate::bindings::gum_module_registry_obtain();
        crate::bindings::gum_module_registry_lock(registry);

        let native = gum::gum_native_module_new(path, "", &range);
        gum_barebone_register_module(registry, native);
        g_object_unref(native as gpointer);

        crate::bindings::gum_module_registry_unlock(registry);
    }
}

// A module arrived, thus tell the registry where it is.
#[cfg(feature = "winnt")]
pub(crate) fn module_arrived(base: u64) {
    if !known_mut().insert(base) {
        return;
    }

    let Some(module) = kernel::describe_module(base) else {
        known_mut().remove(&base);
        return;
    };

    let range = GumMemoryRange {
        base_address: module.base,
        size: module.size as gsize,
    };

    unsafe {
        let registry = crate::bindings::gum_module_registry_obtain();
        crate::bindings::gum_module_registry_lock(registry);

        let native = gum::gum_native_module_new(&module.path, "", &range);
        gum_barebone_register_module(registry, native);
        g_object_unref(native as gpointer);

        crate::bindings::gum_module_registry_unlock(registry);
    }
}

// A module went away, thus take it out of the registry while its name is still known.
#[cfg(any(feature = "win9x", feature = "winnt"))]
pub(crate) fn module_left(base: u64) {
    if !known_mut().remove(&base) {
        return;
    }

    unsafe {
        let registry = crate::bindings::gum_module_registry_obtain();
        crate::bindings::gum_module_registry_lock(registry);

        crate::bindings::gum_barebone_unregister_module(registry, base);

        crate::bindings::gum_module_registry_unlock(registry);
    }
}

#[cfg(any(feature = "win9x", feature = "winnt"))]
fn known_mut() -> &'static mut alloc::collections::BTreeSet<u64> {
    unsafe { (&raw mut KNOWN).as_mut().unwrap() }
}

#[cfg(any(feature = "win9x", feature = "winnt"))]
static mut KNOWN: alloc::collections::BTreeSet<u64> = alloc::collections::BTreeSet::new();

#[cfg(any(feature = "win9x", feature = "winnt"))]
static mut LOADER_LOAD: gpointer = ptr::null_mut();
#[cfg(any(feature = "win9x", feature = "winnt"))]
static mut LOADER_LOAD_WITH_FLAGS: gpointer = ptr::null_mut();
#[cfg(any(feature = "win9x", feature = "winnt"))]
static mut LOADER_UNLOAD: gpointer = ptr::null_mut();

fn register_the_process_modules(registry: *mut GumModuleRegistry) {
    for module in kernel::enumerate_modules() {
        #[cfg(any(feature = "win9x", feature = "winnt"))]
        known_mut().insert(module.base);

        let range = GumMemoryRange {
            base_address: module.base,
            size: module.size as gsize,
        };

        unsafe {
            let native = gum::gum_native_module_new(&module.path, "", &range);
            gum_barebone_register_module(registry, native);
            g_object_unref(native as gpointer);
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn _gum_process_enumerate_ranges(
    prot: GumPageProtection,
    func: GumFoundRangeFunc,
    user_data: gpointer,
) {
    let Some(emit) = func else {
        return;
    };

    kernel::enumerate_ranges(&mut |base, size, protection| {
        if (protection & prot as u32) != prot as u32 {
            return;
        }

        let range = GumMemoryRange {
            base_address: base,
            size: size as gsize,
        };
        let details = GumRangeDetails {
            range: &range,
            protection: protection as GumPageProtection,
            file: ptr::null(),
        };

        unsafe { emit(&details, user_data) };
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn gum_barebone_on_thread_registry_activating(registry: *mut GumThreadRegistry) {
    unsafe { THREAD_REGISTRY = registry };

    kernel::enumerate_threads(&mut |thread| announce_thread(thread.id), false);

    if kernel::watches_threads() {
        kernel::watch_threads(announce_thread, announce_thread_is_gone);
    } else {
        watch_the_threads();
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn gum_barebone_on_thread_registry_deactivating(_registry: *mut GumThreadRegistry) {

    if kernel::watches_threads() {
        kernel::forget_threads();
    } else {
        forget_the_threads();
    }

    unsafe { THREAD_REGISTRY = ptr::null_mut() };
}

fn watch_the_threads() {
    let Some(entries) = kernel::thread_entry_points() else {
        return;
    };

    unsafe {
        let interceptor = crate::bindings::gum_interceptor_obtain();
        crate::bindings::gum_interceptor_begin_transaction(interceptor);
        crate::bindings::gum_interceptor_replace(interceptor, entries.start as gpointer,
            kernel::on_thread_start as gpointer, kernel::thread_start_slot(), ptr::null());
        crate::bindings::gum_interceptor_replace(interceptor, entries.exit as gpointer,
            kernel::on_thread_exit as gpointer, kernel::thread_exit_slot(), ptr::null());
        crate::bindings::gum_interceptor_end_transaction(interceptor);
    }
}

fn forget_the_threads() {
    let Some(entries) = kernel::thread_entry_points() else {
        return;
    };

    unsafe {
        let interceptor = crate::bindings::gum_interceptor_obtain();
        crate::bindings::gum_interceptor_begin_transaction(interceptor);
        crate::bindings::gum_interceptor_revert(interceptor, entries.start as gpointer);
        crate::bindings::gum_interceptor_revert(interceptor, entries.exit as gpointer);
        crate::bindings::gum_interceptor_end_transaction(interceptor);
    }
}

pub(crate) fn thread_appeared(id: u32) {
    announce_thread(id);
}

pub(crate) fn thread_appeared_at(id: u32, routine: usize, parameter: usize) {
    let registry = unsafe { THREAD_REGISTRY };
    if registry.is_null() {
        return;
    }

    let mut details: GumThreadDetails = unsafe { core::mem::zeroed() };
    details.id = id as GumThreadId;
    details.entrypoint.routine = routine as u64;
    details.entrypoint.parameter = parameter as u64;
    details.flags = crate::bindings::GumThreadFlags_GUM_THREAD_FLAGS_ENTRYPOINT_ROUTINE
        | crate::bindings::GumThreadFlags_GUM_THREAD_FLAGS_ENTRYPOINT_PARAMETER;

    unsafe { crate::bindings::gum_barebone_register_thread(registry, &details) };
}

pub(crate) fn thread_vanished(id: u32) {
    announce_thread_is_gone(id);
}

fn announce_thread(id: u32) {
    let registry = unsafe { THREAD_REGISTRY };
    if registry.is_null() {
        return;
    }

    let mut details: GumThreadDetails = unsafe { core::mem::zeroed() };
    details.id = id as GumThreadId;

    unsafe { crate::bindings::gum_barebone_register_thread(registry, &details) };
}

fn announce_thread_is_gone(id: u32) {
    let registry = unsafe { THREAD_REGISTRY };
    if registry.is_null() {
        return;
    }

    unsafe { crate::bindings::gum_barebone_unregister_thread(registry, id as GumThreadId) };
}

static mut THREAD_REGISTRY: *mut GumThreadRegistry = ptr::null_mut();

#[unsafe(no_mangle)]
pub extern "C" fn gum_barebone_enumerate_threads(func: GumFoundThreadFunc, user_data: gpointer,
        flags: GumThreadFlags) {
    let Some(emit) = func else {
        return;
    };

    let wanted = (flags & GumThreadFlags_GUM_THREAD_FLAGS_CPU_CONTEXT) != 0;
    kernel::enumerate_threads(&mut |thread| {
        let mut details: GumThreadDetails = unsafe { core::mem::zeroed() };
        details.flags = 0;
        details.id = thread.id as GumThreadId;

        if let Some(state) = thread.cpu_state {
            details.flags = GumThreadFlags_GUM_THREAD_FLAGS_CPU_CONTEXT;
            details.cpu_context = cpu_context_from(&state);
        }

        unsafe { emit(&details, user_data) };
    }, wanted);
}

#[unsafe(no_mangle)]
pub extern "C" fn gum_barebone_find_thread_by_id(thread_id: GumThreadId, flags: GumThreadFlags)
        -> *mut GumThreadDetails {
    let wanted = (flags & GumThreadFlags_GUM_THREAD_FLAGS_CPU_CONTEXT) != 0;
    let Some(thread) = kernel::find_thread(thread_id as u32, wanted) else {
        return ptr::null_mut();
    };

    let mut details: GumThreadDetails = unsafe { core::mem::zeroed() };
    details.id = thread.id as GumThreadId;

    if let Some(state) = thread.cpu_state {
        details.flags = GumThreadFlags_GUM_THREAD_FLAGS_CPU_CONTEXT;
        details.cpu_context = cpu_context_from(&state);
    }

    unsafe { gum_thread_details_copy(&details) }
}

#[unsafe(no_mangle)]
pub extern "C" fn gum_barebone_modify_thread(thread_id: GumThreadId, func: GumModifyThreadFunc,
        user_data: gpointer, _flags: GumModifyThreadFlags) -> gboolean {
    let Some(modify) = func else {
        return 0;
    };

    let changed = kernel::modify_thread(thread_id as u32, &mut |state| {
        let mut context = cpu_context_from(state);
        unsafe { modify(thread_id, &mut context, user_data) };
        *state = cpu_state_from(&context);
    });

    changed as gboolean
}

#[cfg(target_arch = "x86")]
pub(crate) fn cpu_context_from(state: &kernel::CpuState) -> GumCpuContext {
    GumCpuContext {
        eip: state.eip,
        edi: state.edi,
        esi: state.esi,
        ebp: state.ebp,
        esp: state.esp,
        ebx: state.ebx,
        edx: state.edx,
        ecx: state.ecx,
        eax: state.eax,
        xmm: ptr::null_mut(),
    }
}

#[cfg(target_arch = "x86")]
pub(crate) fn cpu_state_from(context: &GumCpuContext) -> kernel::CpuState {
    kernel::CpuState {
        eip: context.eip,
        edi: context.edi,
        esi: context.esi,
        ebp: context.ebp,
        esp: context.esp,
        ebx: context.ebx,
        edx: context.edx,
        ecx: context.ecx,
        eax: context.eax,
    }
}

#[cfg(target_arch = "x86_64")]
pub(crate) fn cpu_context_from(state: &kernel::CpuState) -> GumCpuContext {
    GumCpuContext {
        rip: state.rip,
        r15: state.r15,
        r14: state.r14,
        r13: state.r13,
        r12: state.r12,
        r11: state.r11,
        r10: state.r10,
        r9: state.r9,
        r8: state.r8,
        rdi: state.rdi,
        rsi: state.rsi,
        rbp: state.rbp,
        rsp: state.rsp,
        rbx: state.rbx,
        rdx: state.rdx,
        rcx: state.rcx,
        rax: state.rax,
        xmm: ptr::null_mut(),
    }
}

#[cfg(target_arch = "x86_64")]
pub(crate) fn cpu_state_from(context: &GumCpuContext) -> kernel::CpuState {
    kernel::CpuState {
        rip: context.rip,
        r15: context.r15,
        r14: context.r14,
        r13: context.r13,
        r12: context.r12,
        r11: context.r11,
        r10: context.r10,
        r9: context.r9,
        r8: context.r8,
        rdi: context.rdi,
        rsi: context.rsi,
        rbp: context.rbp,
        rsp: context.rsp,
        rbx: context.rbx,
        rdx: context.rdx,
        rcx: context.rcx,
        rax: context.rax,
    }
}

#[cfg(target_arch = "aarch64")]
pub(crate) fn cpu_context_from(state: &kernel::CpuState) -> GumCpuContext {
    GumCpuContext {
        pc: state.pc,
        sp: state.sp,
        nzcv: state.nzcv,
        x: state.x,
        fp: state.fp,
        lr: state.lr,
    }
}

#[cfg(target_arch = "aarch64")]
pub(crate) fn cpu_state_from(context: &GumCpuContext) -> kernel::CpuState {
    kernel::CpuState {
        pc: context.pc,
        sp: context.sp,
        nzcv: context.nzcv,
        x: context.x,
        fp: context.fp,
        lr: context.lr,
    }
}

pub(crate) fn fault_type_of(code: u32) -> crate::bindings::GumExceptionType {
    use crate::bindings::*;

    match code {
        ACCESS_VIOLATION | MISALIGNED_DATA | ARRAY_BOUNDS_EXCEEDED =>
            _GumExceptionType_GUM_EXCEPTION_ACCESS_VIOLATION,
        GUARD_PAGE => _GumExceptionType_GUM_EXCEPTION_GUARD_PAGE,
        ILLEGAL_INSTRUCTION | PRIVILEGED_INSTRUCTION =>
            _GumExceptionType_GUM_EXCEPTION_ILLEGAL_INSTRUCTION,
        STACK_OVERFLOW => _GumExceptionType_GUM_EXCEPTION_STACK_OVERFLOW,
        FLOAT_DENORMAL_OPERAND | FLOAT_DIVIDE_BY_ZERO | FLOAT_INEXACT_RESULT
        | FLOAT_INVALID_OPERATION | FLOAT_OVERFLOW | FLOAT_STACK_CHECK | FLOAT_UNDERFLOW
        | INTEGER_DIVIDE_BY_ZERO | INTEGER_OVERFLOW =>
            _GumExceptionType_GUM_EXCEPTION_ARITHMETIC,
        BREAKPOINT => _GumExceptionType_GUM_EXCEPTION_BREAKPOINT,
        SINGLE_STEP => _GumExceptionType_GUM_EXCEPTION_SINGLE_STEP,
        _ => _GumExceptionType_GUM_EXCEPTION_SYSTEM,
    }
}

const ACCESS_VIOLATION: u32 = 0xc000_0005;
const MISALIGNED_DATA: u32 = 0x8000_0002;
const ARRAY_BOUNDS_EXCEEDED: u32 = 0xc000_008c;
const GUARD_PAGE: u32 = 0x8000_0001;
const ILLEGAL_INSTRUCTION: u32 = 0xc000_001d;
const PRIVILEGED_INSTRUCTION: u32 = 0xc000_0096;
const STACK_OVERFLOW: u32 = 0xc000_00fd;
const FLOAT_DENORMAL_OPERAND: u32 = 0xc000_008d;
const FLOAT_DIVIDE_BY_ZERO: u32 = 0xc000_008e;
const FLOAT_INEXACT_RESULT: u32 = 0xc000_008f;
const FLOAT_INVALID_OPERATION: u32 = 0xc000_0090;
const FLOAT_OVERFLOW: u32 = 0xc000_0091;
const FLOAT_STACK_CHECK: u32 = 0xc000_0092;
const FLOAT_UNDERFLOW: u32 = 0xc000_0093;
const INTEGER_DIVIDE_BY_ZERO: u32 = 0xc000_0094;
const INTEGER_OVERFLOW: u32 = 0xc000_0095;
const BREAKPOINT: u32 = 0x8000_0003;
const SINGLE_STEP: u32 = 0x8000_0004;

pub(crate) unsafe fn enumerate_exports_in_range(
    start_address: u64,
    end_address: u64,
    callback: &mut FoundExportCallback,
) {
    #[cfg(any(feature = "win9x", feature = "winnt"))]
    if crate::running_in_a_process() {
        kernel::enumerate_exports(start_address as _, &mut |name, address| {
            callback(name as *const crate::bindings::gchar, address)
        });
        return;
    }

    unsafe {
        let symbol_table = &*core::ptr::addr_of!(crate::SYMBOL_TABLE);

        for symbol_ref in symbol_table.iter_symbols_in_range(start_address, end_address) {
            if !callback(symbol_ref.name_ptr(), symbol_ref.address()) {
                break;
            }
        }
    }
}
