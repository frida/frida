// The agent as a kernel module: every primitive is an ordinary call into
// linux/frida-kmod.c, compiled by kbuild against the target kernel's headers,
// which is what keeps us out of the business of guessing struct layouts,
// config-dependent macros and per-version symbol names.

use core::ffi::{CStr, c_char, c_int, c_void};

use alloc::string::String;
use alloc::vec::Vec;

use super::{LoadedModule, ModuleEvent};
use crate::kernel::ThreadEntry;

pub fn log(msg: &str) {
    unsafe { frida_kmod_log(msg.as_ptr() as *const c_char) };
}

pub fn panic(msg: &str) -> ! {
    unsafe { frida_kmod_panic(msg.as_ptr() as *const c_char) }
}

pub fn run_when_ready(action: fn()) {
    action();
}

pub fn spawn_thread(entry: ThreadEntry, parameter: *mut c_void) -> isize {
    unsafe { frida_kmod_spawn_thread(entry, parameter) as isize }
}

pub fn install_hooks() {
    unsafe { frida_kmod_install_hooks() };
}

pub fn alloc(size: usize) -> *mut u8 {
    unsafe { frida_kmod_alloc(size) }
}

pub fn free(ptr: *mut u8, size: usize) {
    unsafe { frida_kmod_free(ptr, size) };
}

pub fn alloc_code(size: usize) -> *mut u8 {
    unsafe { frida_kmod_alloc_code(size) }
}

pub fn free_code(ptr: *mut u8, size: usize) {
    unsafe { frida_kmod_free_code(ptr, size) };
}

pub fn page_size() -> usize {
    crate::gum::page_size_the_kernel_runs_with()
}

pub fn alloc_heap(size: usize) -> *mut u8 {
    alloc(size)
}

pub fn alloc_dma(size: usize) -> *mut u8 {
    alloc_code(size)
}

pub fn free_dma(ptr: *mut u8, size: usize) {
    free_code(ptr, size);
}

pub fn own_range() -> (u64, u64) {
    let mut base: u64 = 0;
    let mut size: u64 = 0;
    unsafe { frida_kmod_own_range(&mut base, &mut size) };
    (base, size)
}

// The sequence number is taken before `check` runs, so a wakeup racing with the
// condition we are about to observe still makes the commit return immediately.
pub fn wait(_token: *const u8, timeout_us: Option<u64>, check: &mut dyn FnMut() -> bool) {
    let seq = unsafe { frida_kmod_wait_prepare() };

    if check() {
        return;
    }

    unsafe { frida_kmod_wait_commit(seq, timeout_us.map_or(-1, |us| us as i64)) };
}

pub fn poke_loop_wakeup() {}

pub fn wake(_token: *const u8) {
    unsafe { frida_kmod_wake() };
}

/// Gives the scheduler a chance to run something else. A kernel thread that loops
/// without doing this is a hard lockup waiting to happen.
pub fn yield_now() {
    unsafe { frida_kmod_yield() };
}

pub fn monotonic_micros() -> i64 {
    unsafe { frida_kmod_monotonic_micros() }
}

pub fn wall_clock_micros() -> (u32, u32) {
    let mut secs: u32 = 0;
    let mut micros: u32 = 0;
    unsafe { frida_kmod_wall_clock_micros(&mut secs, &mut micros) };
    (secs, micros)
}

pub fn current_process_id() -> u32 {
    KERNEL_PROCESS
}

pub fn current_thread_id() -> u64 {
    unsafe { frida_kmod_current_thread_id() }
}

pub fn protect(address: u64, size: usize, gum_prot: u32) -> bool {
    unsafe { frida_kmod_protect(address, size, gum_prot) != 0 }
}

pub fn remap_writable(first_page: u64, n_pages: u32) -> *mut c_void {
    unsafe { frida_kmod_remap_writable(first_page, n_pages) }
}

/// Tears down an alias handed out by [`remap_writable`] and flushes the icache
/// over the range it aliased; the shim remembers which range that was.
pub fn unmap_writable(mapping: *mut c_void) {
    unsafe { frida_kmod_unmap_writable(mapping) };
}

pub fn get_kernel_base() -> u64 {
    unsafe { frida_kmod_kernel_base() }
}

pub fn get_kernel_size() -> u64 {
    unsafe { frida_kmod_kernel_size() }
}

pub fn enumerate_modules() -> Vec<LoadedModule> {
    unsafe extern "C" fn on_module(
        name: *const c_char,
        version: *const c_char,
        base: u64,
        size: u64,
        user_data: *mut c_void,
    ) -> c_int {
        unsafe {
            let modules = &mut *(user_data as *mut Vec<LoadedModule>);
            modules.push(LoadedModule {
                name: String::from(CStr::from_ptr(name).to_str().unwrap_or("?")),
                version: String::from(CStr::from_ptr(version).to_str().unwrap_or("")),
                base,
                size,
            });
            1
        }
    }

    let mut modules: Vec<LoadedModule> = Vec::new();
    unsafe {
        frida_kmod_enumerate_modules(on_module, &mut modules as *mut _ as *mut c_void);
    }
    modules
}

pub fn watch_modules(on_event: fn(ModuleEvent, LoadedModule)) {
    unsafe extern "C" fn on_module_event(
        loaded: c_int,
        name: *const c_char,
        version: *const c_char,
        base: u64,
        size: u64,
        user_data: *mut c_void,
    ) {
        unsafe {
            let on_event: fn(ModuleEvent, LoadedModule) = core::mem::transmute(user_data);
            on_event(
                if loaded != 0 {
                    ModuleEvent::Loaded
                } else {
                    ModuleEvent::Unloaded
                },
                LoadedModule {
                    name: String::from(CStr::from_ptr(name).to_str().unwrap_or("?")),
                    version: String::from(CStr::from_ptr(version).to_str().unwrap_or("")),
                    base,
                    size,
                },
            );
        }
    }

    unsafe {
        frida_kmod_watch_modules(on_module_event, on_event as *mut c_void);
    }
}

pub fn unwatch_modules() {
    unsafe { frida_kmod_unwatch_modules() };
}

pub fn enumerate_module_symbols(
    base: u64,
    on_symbol: &mut dyn FnMut(*const c_char, u64, u64, u8, bool) -> bool,
) -> bool {
    unsafe extern "C" fn on_module_symbol(
        name: *const c_char,
        address: u64,
        size: u64,
        kind: u8,
        is_global: c_int,
        user_data: *mut c_void,
    ) -> c_int {
        unsafe {
            let callback =
                &mut *(user_data as *mut &mut dyn FnMut(*const c_char, u64, u64, u8, bool) -> bool);
            if callback(name, address, size, kind, is_global != 0) { 1 } else { 0 }
        }
    }

    let mut boxed: &mut dyn FnMut(*const c_char, u64, u64, u8, bool) -> bool = on_symbol;
    unsafe {
        frida_kmod_enumerate_module_symbols(base, on_module_symbol, &mut boxed as *mut _ as *mut c_void) != 0
    }
}

pub fn enumerate_module_exports(
    base: u64,
    on_export: &mut dyn FnMut(*const c_char, u64) -> bool,
) -> bool {
    unsafe extern "C" fn on_module_export(
        name: *const c_char,
        address: u64,
        user_data: *mut c_void,
    ) -> c_int {
        unsafe {
            let callback = &mut *(user_data as *mut &mut dyn FnMut(*const c_char, u64) -> bool);
            if callback(name, address) { 1 } else { 0 }
        }
    }

    let mut boxed: &mut dyn FnMut(*const c_char, u64) -> bool = on_export;
    unsafe {
        frida_kmod_enumerate_module_exports(base, on_module_export, &mut boxed as *mut _ as *mut c_void) != 0
    }
}

pub fn find_symbol(name: &CStr) -> u64 {
    unsafe { frida_kmod_find_symbol(name.as_ptr()) }
}

pub fn find_function(name: &CStr) -> u64 {
    unsafe { frida_kmod_find_function(name.as_ptr()) }
}

/// Resolves `address` to `(name, symbol_address)`, or `None` when it falls
/// outside every known symbol.
pub fn symbol_from_address(address: u64) -> Option<(String, u64)> {
    const NAME_CAPACITY: usize = 256;

    let mut buffer = [0u8; NAME_CAPACITY];
    let mut symbol_address: u64 = 0;
    let found = unsafe {
        frida_kmod_symbol_name_from_address(
            address,
            buffer.as_mut_ptr() as *mut c_char,
            NAME_CAPACITY,
            &mut symbol_address,
        )
    };
    if found == 0 {
        return None;
    }

    let name = CStr::from_bytes_until_nul(&buffer).unwrap();
    Some((String::from(name.to_str().unwrap()), symbol_address))
}

/// Visits every symbol the running kernel knows about. Returns false when
/// kallsyms is unavailable, which is the case on kernels built without
/// CONFIG_KALLSYMS or when the shim could not resolve its entry points.
pub fn enumerate_symbols(callback: &mut dyn FnMut(&CStr, u64) -> bool) -> bool {
    unsafe extern "C" fn on_symbol(
        name: *const c_char,
        address: u64,
        user_data: *mut c_void,
    ) -> c_int {
        unsafe {
            let callback = &mut *(user_data as *mut &mut dyn FnMut(&CStr, u64) -> bool);
            if callback(CStr::from_ptr(name), address) { 1 } else { 0 }
        }
    }

    let mut boxed: &mut dyn FnMut(&CStr, u64) -> bool = callback;
    unsafe { frida_kmod_enumerate_symbols(on_symbol, &mut boxed as *mut _ as *mut c_void) != 0 }
}

const KERNEL_PROCESS: u32 = 0;

type FoundSymbolFunc =
    unsafe extern "C" fn(name: *const c_char, address: u64, user_data: *mut c_void) -> c_int;
type ModuleEventFunc = unsafe extern "C" fn(c_int, *const c_char, *const c_char, u64, u64, *mut c_void);
type FoundModuleSymbolFunc =
    unsafe extern "C" fn(*const c_char, u64, u64, u8, c_int, *mut c_void) -> c_int;
type FoundModuleExportFunc = unsafe extern "C" fn(*const c_char, u64, *mut c_void) -> c_int;
type FoundModuleFunc = unsafe extern "C" fn(
    name: *const c_char,
    version: *const c_char,
    base: u64,
    size: u64,
    user_data: *mut c_void,
) -> c_int;

unsafe extern "C" {
    fn frida_kmod_log(message: *const c_char);
    fn frida_kmod_panic(message: *const c_char) -> !;
    fn frida_kmod_spawn_thread(entry: ThreadEntry, parameter: *mut c_void) -> c_int;
    fn frida_kmod_install_hooks();
    fn frida_kmod_alloc(size: usize) -> *mut u8;
    fn frida_kmod_free(ptr: *mut u8, size: usize);
    fn frida_kmod_alloc_code(size: usize) -> *mut u8;
    fn frida_kmod_free_code(ptr: *mut u8, size: usize);
    fn frida_kmod_own_range(base: *mut u64, size: *mut u64);
    fn frida_kmod_wait_prepare() -> u32;
    fn frida_kmod_wait_commit(seq: u32, timeout_us: i64);
    fn frida_kmod_wake();
    fn frida_kmod_yield();
    fn frida_kmod_monotonic_micros() -> i64;
    fn frida_kmod_wall_clock_micros(secs: *mut u32, micros: *mut u32);
    fn frida_kmod_current_thread_id() -> u64;
    fn frida_kmod_protect(address: u64, size: usize, gum_prot: u32) -> c_int;
    fn frida_kmod_remap_writable(first_page: u64, n_pages: u32) -> *mut c_void;
    fn frida_kmod_unmap_writable(mapping: *mut c_void);
    fn frida_kmod_kernel_base() -> u64;
    fn frida_kmod_kernel_size() -> u64;
    fn frida_kmod_enumerate_modules(func: FoundModuleFunc, user_data: *mut c_void);
    fn frida_kmod_watch_modules(func: ModuleEventFunc, user_data: *mut c_void);
    fn frida_kmod_unwatch_modules();
    fn frida_kmod_enumerate_module_symbols(
        base: u64,
        func: FoundModuleSymbolFunc,
        user_data: *mut c_void,
    ) -> c_int;
    fn frida_kmod_enumerate_module_exports(
        base: u64,
        func: FoundModuleExportFunc,
        user_data: *mut c_void,
    ) -> c_int;
    fn frida_kmod_find_symbol(name: *const c_char) -> u64;
    fn frida_kmod_find_function(name: *const c_char) -> u64;
    fn frida_kmod_symbol_name_from_address(
        address: u64,
        buffer: *mut c_char,
        size: usize,
        symbol_address: *mut u64,
    ) -> c_int;
    fn frida_kmod_enumerate_symbols(func: FoundSymbolFunc, user_data: *mut c_void) -> c_int;
}
