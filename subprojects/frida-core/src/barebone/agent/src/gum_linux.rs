// Linux half of Gum's platform backend. Everything is done locally: we are the
// kernel, so page permissions go through set_memory_*(), writable aliases of
// write-protected text come from vmap(), and symbols come from kallsyms.

use crate::{
    bindings::{
        _GumPageProtection_GUM_PAGE_EXECUTE, _GumPageProtection_GUM_PAGE_READ,
        _GumPageProtection_GUM_PAGE_WRITE, _GumRwxSupport_GUM_RWX_NONE, GArray,
        GumDebugSymbolDetails, GumMemoryRange, GumModuleRegistry, GumPageProtection, GumRwxSupport,
        g_array_append_vals, g_array_new, g_object_unref, g_pattern_spec_free, g_pattern_spec_match_string,
        g_pattern_spec_new, g_strdup, gboolean, gchar, gconstpointer, gpointer, gsize, guint,
        gum_barebone_register_module, gum_mprotect,
    },
    gum::{self, FoundExportCallback},
    kernel,
};
use alloc::format;
use core::ffi::CStr;
use core::ptr;

#[unsafe(no_mangle)]
pub extern "C" fn gum_barebone_query_platform() -> *const crate::bindings::gchar {
    c"linux".as_ptr() as *const crate::bindings::gchar
}

#[unsafe(no_mangle)]
pub extern "C" fn gum_barebone_query_stack_size() -> gsize {
    crate::linux::stack_headroom() as gsize
}

#[unsafe(no_mangle)]
pub extern "C" fn gum_query_rwx_support() -> GumRwxSupport {
    _GumRwxSupport_GUM_RWX_NONE
}

#[unsafe(no_mangle)]
pub extern "C" fn gum_memory_can_remap_writable() -> gboolean {
    1
}

#[unsafe(no_mangle)]
pub extern "C" fn gum_memory_try_remap_writable_pages(
    first_page: gpointer,
    n_pages: guint,
) -> gpointer {
    kernel::remap_writable(first_page as u64, n_pages) as gpointer
}

#[unsafe(no_mangle)]
pub extern "C" fn gum_memory_dispose_writable_pages(writable: gpointer, _n_pages: guint) {
    kernel::unmap_writable(writable as *mut core::ffi::c_void);
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
    // vfree() wants the range writable again: the pages go back to the vmalloc
    // allocator, and leaving them RX would fault the next consumer.
    if gum::is_agent_slab(address as u64) {
        unsafe {
            gum_mprotect(
                address,
                size,
                (_GumPageProtection_GUM_PAGE_READ | _GumPageProtection_GUM_PAGE_WRITE)
                    as GumPageProtection,
            );
        }
        gum::unregister_slab(address as u64);
    }
    kernel::free_code(address as *mut u8, size as usize);
    1
}

#[unsafe(no_mangle)]
pub extern "C" fn gum_barebone_on_registry_activating(registry: *mut GumModuleRegistry) {
    crate::gum_modules::publish(registry);
}

#[unsafe(no_mangle)]
pub extern "C" fn gum_barebone_on_registry_deactivating(_registry: *mut GumModuleRegistry) {
    crate::gum_modules::unpublish();
}

pub(crate) unsafe fn enumerate_exports_in_range(
    start_address: u64,
    end_address: u64,
    callback: &mut FoundExportCallback<'_>,
) {
    if crate::gum_modules::enumerate_exports_in_module(start_address, callback) {
        return;
    }

    kernel::enumerate_symbols(&mut |name, address| {
        if address < start_address || address >= end_address {
            return true;
        }
        callback(name.as_ptr() as *const gchar, address)
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn gum_symbol_details_from_address(
    address: gpointer,
    details: *mut GumDebugSymbolDetails,
) -> gboolean {
    let Some((name, symbol_address)) = kernel::symbol_from_address(address as u64) else {
        return 0;
    };

    unsafe {
        let name_bytes = name.as_bytes();
        let copy_len = core::cmp::min(name_bytes.len(), (*details).symbol_name.len() - 1);
        core::ptr::copy_nonoverlapping(
            name_bytes.as_ptr(),
            (*details).symbol_name.as_mut_ptr() as *mut u8,
            copy_len,
        );
        (*details).symbol_name[copy_len] = 0;

        (*details).address = symbol_address;

        (*details).module_name[0] = 0;
        (*details).file_name[0] = 0;
        (*details).line_number = 0;
        (*details).column = 0;
    }

    1
}

#[unsafe(no_mangle)]
pub extern "C" fn gum_symbol_name_from_address(address: gpointer) -> *mut gchar {
    let Some((name, _)) = kernel::symbol_from_address(address as u64) else {
        return ptr::null_mut();
    };

    let mut name = name;
    name.push('\0');
    unsafe { g_strdup(name.as_ptr() as *const gchar) }
}

#[unsafe(no_mangle)]
pub extern "C" fn gum_find_function(name: *const gchar) -> gpointer {
    let address = kernel::find_function(unsafe { CStr::from_ptr(name) });
    address as gpointer
}

#[unsafe(no_mangle)]
pub extern "C" fn gum_find_functions_named(name: *const gchar) -> *mut GArray {
    let wanted = unsafe { CStr::from_ptr(name) };

    collect_addresses(&mut |candidate, _address| candidate == wanted)
}

#[unsafe(no_mangle)]
pub extern "C" fn gum_find_functions_matching(pattern: *const gchar) -> *mut GArray {
    unsafe {
        let spec = g_pattern_spec_new(pattern);
        let array = collect_addresses(&mut |candidate, _address| {
            g_pattern_spec_match_string(spec, candidate.as_ptr() as *const gchar) != 0
        });
        g_pattern_spec_free(spec);
        array
    }
}

fn collect_addresses(predicate: &mut dyn FnMut(&CStr, u64) -> bool) -> *mut GArray {
    unsafe {
        let array = g_array_new(0, 0, core::mem::size_of::<gpointer>() as guint);

        kernel::enumerate_symbols(&mut |name, address| {
            if predicate(name, address) {
                let addr = address as gpointer;
                g_array_append_vals(array, &addr as *const gpointer as gconstpointer, 1);
            }
            true
        });

        array
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn gum_load_symbols(_path: *const gchar) -> gboolean {
    0
}
