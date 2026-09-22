use core::ffi::c_void;

use crate::bindings::{gpointer, gsize, gum_try_mprotect, GumPageProtection};
use crate::{kernel, libc};

const GUM_PAGE_READ: GumPageProtection = 1;
const GUM_PAGE_EXECUTE: GumPageProtection = 4;

// libffi (FFI_TRAMP_EMBEDDER) asks us to back a trampoline table: a copy of its
// static trampoline `text` made executable, followed by a writable parameter
// table `map_size` bytes later (the trampolines reach their parameters via that
// fixed offset). The two halves are whole pages, so flipping the first to RX
// leaves the parameter table writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ffi_tramp_embedder_map(
    text: *const c_void,
    map_size: usize,
    code_table: *mut *mut c_void,
    parm_table: *mut *mut c_void,
) -> i32 {
    unsafe {
        let base = kernel::alloc_code(map_size * 2);
        if base.is_null() {
            return 0;
        }

        core::ptr::copy_nonoverlapping(text as *const u8, base, map_size);
        libc::__clear_cache(base, base.add(map_size));

        let made_executable =
            gum_try_mprotect(base as gpointer, map_size as gsize, GUM_PAGE_READ | GUM_PAGE_EXECUTE);
        if made_executable == 0 {
            kernel::free_code(base, map_size * 2);
            return 0;
        }

        *code_table = base as *mut c_void;
        *parm_table = base.add(map_size) as *mut c_void;
        1
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ffi_tramp_embedder_unmap(
    code_table: *mut c_void,
    _parm_table: *mut c_void,
    map_size: usize,
) {
    kernel::free_code(code_table as *mut u8, map_size * 2);
}
