use alloc::format;
use crate::bindings::{
    GumMemoryRange, GumModuleRegistry, g_object_unref, gpointer, gsize,
    gum_barebone_register_module, gum_barebone_unregister_module,
};
use crate::gum::{self, FoundExportCallback, FoundSymbolCallback};
use crate::kernel;
use crate::linux::{LoadedModule, ModuleEvent};

pub fn publish(registry: *mut GumModuleRegistry) {
    unsafe { REGISTRY = registry };

    let base = kernel::get_kernel_base();
    if base != 0 {
        register(registry, KERNEL_PATH, "", base, kernel::get_kernel_size());
    }

    for m in kernel::enumerate_modules() {
        register(registry, &path_of(&m.name), &m.version, m.base, m.size);
    }

    kernel::watch_modules(on_module_event);
}

pub fn enumerate_symbols_in_range(
    start_address: u64,
    end_address: u64,
    callback: &mut FoundSymbolCallback<'_>,
) {
    #[cfg(feature = "linux-injected")]
    if kernel::in_copy() {
        kernel::enumerate_symbols_in_range(start_address, end_address, callback);
        return;
    }

    kernel::enumerate_module_symbols(start_address, &mut |name, address, size, kind, global| {
        if address < start_address || address >= end_address {
            return true;
        }
        callback(name as *const _, address, size, kind, global)
    });
}

pub fn enumerate_exports_in_module(
    start_address: u64,
    callback: &mut FoundExportCallback<'_>,
) -> bool {
    kernel::enumerate_module_exports(start_address, &mut |name, address| {
        callback(name as *const _, address)
    })
}

pub fn unpublish() {
    kernel::unwatch_modules();

    unsafe { REGISTRY = core::ptr::null_mut() };
}

fn on_module_event(kind: ModuleEvent, m: LoadedModule) {
    let registry = unsafe { REGISTRY };
    if registry.is_null() {
        return;
    }

    match kind {
        ModuleEvent::Loaded => register(registry, &path_of(&m.name), &m.version, m.base, m.size),
        ModuleEvent::Unloaded => unsafe { gum_barebone_unregister_module(registry, m.base) },
    }
}

fn register(
    registry: *mut GumModuleRegistry,
    path: &str,
    version: &str,
    base: u64,
    size: u64,
) {
    unsafe {
        let range = GumMemoryRange {
            base_address: base,
            size: size as gsize,
        };

        let module = gum::gum_native_module_new(path, version, &range);
        gum_barebone_register_module(registry, module);
        g_object_unref(module as gpointer);
    }
}

fn path_of(name: &str) -> alloc::string::String {
    format!("{}{}{}", MODULE_DIRECTORY, name, MODULE_SUFFIX)
}

static mut REGISTRY: *mut GumModuleRegistry = core::ptr::null_mut();

const KERNEL_PATH: &str = "/boot/vmlinux";
const MODULE_DIRECTORY: &str = "/lib/modules/";
const MODULE_SUFFIX: &str = ".ko";
