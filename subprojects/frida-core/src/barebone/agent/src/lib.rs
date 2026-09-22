#![no_std]

#[cfg(not(any(
    feature = "xnu-core",
    feature = "win9x",
    feature = "winnt",
    feature = "linux",
    feature = "linux-injected"
)))]
compile_error!(
    "pick a flavor: --features xnu, --features win9x, --features winnt, --features linux or --features linux-injected"
);

#[cfg(all(feature = "blob", feature = "linux"))]
compile_error!("pick one flavour, not both");

use alloc::boxed::Box;
use alloc::collections::BTreeMap;
use alloc::format;
use alloc::string::String;
use alloc::vec::Vec;
use core::alloc::{GlobalAlloc, Layout};
use core::ffi::{CStr, c_void};
use core::ptr;
use core::sync::atomic::{AtomicU32, Ordering};

use bindings::{
    GAsyncResult, GBytes, GError, GMainContext, GObject, GSource, GSourceFunc, GSourceFuncs,
    GVariant, GumMemoryRange, gboolean, g_main_context_acquire, g_main_context_check,
    g_main_context_dispatch, g_main_context_prepare, g_main_context_query, g_main_context_release,
    g_main_context_wakeup,
    g_source_attach, g_source_new, g_source_unref,
    GumScript, GumScriptBackend, g_error_free, g_free, g_main_context_iteration,
    g_main_context_push_thread_default, g_memdup2, g_object_unref, g_variant_check_format_string,
    g_variant_get, g_variant_get_boolean, g_variant_get_child_value, g_variant_get_data,
    g_variant_get_size, g_variant_get_string, g_variant_get_strv,
    g_variant_get_uint32, g_variant_new, g_variant_new_from_data, g_variant_new_string,
    g_variant_new_tuple, g_variant_new_uint32, g_variant_type_free, g_variant_type_new,
    g_variant_builder_add, g_variant_builder_add_value, g_variant_builder_close,
    g_variant_builder_end, g_variant_builder_new, g_variant_builder_open,
    g_variant_new_fixed_array, g_variant_get_fixed_array, g_bytes_new, g_bytes_get_data,
    g_bytes_unref, GVariantType,
    g_variant_unref, gchar, gpointer, gsize, gum_script_backend_create,
    gum_script_backend_create_finish, gum_script_backend_get_scheduler,
    gum_script_backend_obtain_qjs, gum_script_get_stalker, gum_script_load,
    gum_script_load_finish, gum_script_post, gum_script_scheduler_disable_background_thread,
    gum_script_scheduler_get_js_context, gum_script_set_message_handler, gum_script_unload,
    gum_script_unload_finish, gum_stalker_exclude,
};

mod ffi;
mod glib;
mod gthread;
mod gum;
mod libc;
mod ring;

pub mod kernel;

#[cfg(feature = "linux")]
mod gum_linux;
#[cfg(feature = "linux-injected")]
mod gum_btf;
#[cfg(any(feature = "linux", feature = "linux-injected"))]
mod gum_modules;
#[cfg(any(feature = "linux", feature = "xnu-kext"))]
mod hostlink_chardev;
#[cfg(any(feature = "linux-injected", feature = "xnu-core"))]
mod heap;
#[cfg(any(feature = "linux", feature = "linux-injected"))]
mod linux;

#[cfg(any(feature = "win9x", feature = "winnt"))]
mod gum_windows;
#[cfg(any(feature = "win9x", feature = "winnt"))]
mod icons;
#[cfg(any(feature = "win9x", feature = "winnt"))]
mod start_menu;
#[cfg(feature = "win9x")]
mod win9x;
#[cfg(feature = "win9x")]
mod win9x_user;
#[cfg(feature = "winnt")]
mod winnt;
#[cfg(feature = "winnt")]
mod winnt_paging;
#[cfg(feature = "winnt")]
mod winnt_user;

#[cfg(any(feature = "xnu-core", feature = "linux-injected"))]
mod gum_injected;
#[cfg(feature = "blob")]
mod hostlink_virtio;
#[cfg(feature = "winnt")]
mod hostlink_serial;
#[cfg(feature = "xnu-core")]
mod hostlink_vsock;
#[cfg(feature = "xnu-core")]
mod pac;
#[cfg(any(feature = "blob", feature = "xnu-kext"))]
mod symbols;
#[cfg(feature = "xnu-core")]
mod xnu;
#[cfg(feature = "xnu-core")]
mod xnu_applications;
#[cfg(feature = "xnu-core")]
mod xnu_bell;
#[cfg(feature = "xnu-core")]
mod xnu_hiding;
#[cfg(feature = "xnu-core")]
mod xnu_injection;
#[cfg(feature = "xnu-core")]
mod xnu_libsystem;
#[cfg(feature = "xnu-core")]
mod xnu_unlisted;
#[cfg(feature = "xnu-core")]
mod xnu_mapped;
#[cfg(feature = "xnu-core")]
mod xnu_processes;
#[cfg(feature = "xnu-core")]
mod xnu_ranges;
#[cfg(feature = "xnu-core")]
mod xnu_relay;
#[cfg(feature = "xnu-core")]
mod xnu_spawn;
#[cfg(feature = "xnu-core")]
mod xnu_user;
#[cfg(feature = "xnu-core")]
mod xnu_user_calls;

mod bindings {
    #![allow(
        dead_code,
        improper_ctypes,
        non_camel_case_types,
        non_snake_case,
        non_upper_case_globals,
        unused_imports
    )]
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}

#[repr(u8)]
#[derive(PartialEq, Eq, Clone, Copy, Debug)]
pub enum FridaCommand {
    CreateScript = 1,
    LoadScript = 2,
    DestroyScript = 3,
    PostScriptMessage = 4,
    RemapWritablePages = 5,
    MemoryProtect = 6,
    PatchCode = 7,
    EnumerateProcesses = 8,
    InjectIntoProcess = 9,
    AllocateShared = 10,
    DetachFromProcess = 11,
    PlaceAgentInProcess = 12,
    StartAgentInProcess = 13,
    SpawnProcess = 14,
    ResumeProcess = 15,
    Stop = 16,
    GateSpawns = 17,
    EnumerateApplications = 18,
    EnumerateShortcuts = 19,

    Reply = 128,
    ScriptMessage = 129,
    SpawnAdded = 130,
    Ready = 131,
}

impl core::fmt::Display for FridaCommand {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            FridaCommand::GateSpawns => write!(f, "GateSpawns"),
            FridaCommand::EnumerateApplications => write!(f, "EnumerateApplications"),
            FridaCommand::EnumerateShortcuts => write!(f, "EnumerateShortcuts"),
            FridaCommand::SpawnAdded => write!(f, "SpawnAdded"),
            FridaCommand::Ready => write!(f, "Ready"),
            FridaCommand::CreateScript => write!(f, "CreateScript"),
            FridaCommand::LoadScript => write!(f, "LoadScript"),
            FridaCommand::DestroyScript => write!(f, "DestroyScript"),
            FridaCommand::PostScriptMessage => write!(f, "PostScriptMessage"),
            FridaCommand::RemapWritablePages => write!(f, "RemapWritablePages"),
            FridaCommand::MemoryProtect => write!(f, "MemoryProtect"),
            FridaCommand::PatchCode => write!(f, "PatchCode"),
            FridaCommand::EnumerateProcesses => write!(f, "EnumerateProcesses"),
            FridaCommand::InjectIntoProcess => write!(f, "InjectIntoProcess"),
            FridaCommand::AllocateShared => write!(f, "AllocateShared"),
            FridaCommand::PlaceAgentInProcess => write!(f, "PlaceAgentInProcess"),
            FridaCommand::StartAgentInProcess => write!(f, "StartAgentInProcess"),
            FridaCommand::SpawnProcess => write!(f, "SpawnProcess"),
            FridaCommand::ResumeProcess => write!(f, "ResumeProcess"),
            FridaCommand::Stop => write!(f, "Stop"),
            FridaCommand::DetachFromProcess => write!(f, "DetachFromProcess"),
            FridaCommand::Reply => write!(f, "Reply"),
            FridaCommand::ScriptMessage => write!(f, "ScriptMessage"),
        }
    }
}

#[derive(Debug)]
struct HandlerResponse {
    variant: *mut GVariant,
}

impl HandlerResponse {
    fn success(variant: *mut GVariant) -> Self {
        Self {
            variant: unsafe { g_variant_new(c"(bv)".as_ptr(), 1, variant) },
        }
    }

    fn success_empty() -> Self {
        Self::success(unsafe { g_variant_new_tuple(ptr::null(), 0) })
    }

    fn error(message: &str) -> Self {
        let mut c_message = String::from(message);
        c_message.push('\0');

        Self {
            variant: unsafe {
                g_variant_new(
                    c"(bv)".as_ptr(),
                    0,
                    g_variant_new_string(c_message.as_ptr() as *const core::ffi::c_char),
                )
            },
        }
    }
}

pub static mut MODULE_INFO: Vec<ModuleInfo> = Vec::new();
#[cfg(any(feature = "blob", feature = "xnu-kext"))]
pub static mut SYMBOL_TABLE: crate::symbols::SymbolTable = crate::symbols::SymbolTable::empty();

#[derive(Debug, Clone)]
pub struct ModuleInfo {
    pub name: String,
    pub version: String,
    pub offset: u64,
    pub size: u64,
    pub start_func_offset: u64,
    pub stop_func_offset: u64,
}

#[cfg(feature = "blob")]
mod entrypoint_blob {
    use super::*;
    use crate::symbols::SymbolTable;

    static mut CONFIG_DATA: &'static [u8] = &[];

    #[cfg(all(feature = "winnt", target_arch = "x86_64"))]
    #[unsafe(no_mangle)]
    pub unsafe extern "win64" fn _start(config_data: *const u8, config_size: usize) {
        unsafe { enter(config_data, config_size) }
    }

    #[cfg(not(all(feature = "winnt", target_arch = "x86_64")))]
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn _start(config_data: *const u8, config_size: usize) {
        unsafe { enter(config_data, config_size) }
    }

    unsafe fn enter(config_data: *const u8, config_size: usize) {
        unsafe {
            #[cfg(any(feature = "win9x", feature = "winnt", feature = "linux-injected", feature = "xnu-core"))]
            crate::preserve_writable_half();

            CONFIG_DATA = core::slice::from_raw_parts(config_data, config_size);

            kernel::run_when_ready(start_worker);
        }
    }

    // KNOWN ISSUE (vphone research kernel): the worker panics during gum_init_embedded with
    // "debug exceptions enabled in kernel mode". The worker itself is healthy — it runs with
    // PSTATE.D=1 (debug masked) throughout. The panic is the research kernel's synchronous-
    // exception handler reacting to ANY exception the agent takes in kernel mode while the VZ
    // debug stub is active. Two confirmed sources: (1) the host's breakpoint-based Callbacks
    // patch BRK at gum_try_mprotect/remap, and the BRK is a debug exception; (2) a stray jump
    // into the agent's data region during GObject init (instruction abort) — looks like a
    // misapplied relocation for a function pointer stored in .data/.rodata.
    fn start_worker() {
        kernel::spawn_thread(worker, 12345usize as *mut c_void);
    }

    unsafe extern "C" fn worker(_parameter: *mut c_void, _wait_result: i32) {
        unsafe {
            crate::run_constructors();
            init_gum();

            let (transport_config, kernel_base, module_info, symbol_table, own_range) =
                parse_config(core::ptr::addr_of!(CONFIG_DATA).read());
            kernel::set_kernel_base(kernel_base);
            MODULE_INFO = module_info;
            SYMBOL_TABLE = symbol_table;
            OWN_RANGE = own_range;

            kernel::install_fault_reporter();
            #[cfg(feature = "winnt")]
            #[cfg(target_arch = "aarch64")]
            kernel::disarm_patchguard();

            #[cfg(feature = "linux-injected")]
            crate::gum_btf::publish();

            let wake_token = ptr::addr_of_mut!(glib::WAKEUP_TOKEN) as *const u8;
            // Install this before the loop waits the first time, thus a copy placed in a process later
            // can receive a handle to the same event.
            #[cfg(feature = "winnt")]
            kernel::install_shareable_wake_event(wake_token);
            let transport = match transport_config {
                TransportConfig::Virtio { mmio, irq } => Transport::Virtio(
                    hostlink_virtio::Hostlink::init(
                        mmio,
                        irq,
                        Some(on_frame_from_host),
                        wake_token,
                    )
                    .unwrap(),
                ),
                #[cfg(any(feature = "win9x", feature = "winnt", feature = "linux-injected"))]
                TransportConfig::VirtioPci { ecam } => Transport::Virtio(
                    hostlink_virtio::Hostlink::init_pci(ecam, Some(on_frame_from_host), wake_token)
                        .unwrap(),
                ),
                #[cfg(feature = "linux-injected")]
                TransportConfig::PipeVsock { path } => Transport::PipeVsock(
                    linux::hostlink_pipe_vsock::Hostlink::init(&path, Some(on_frame_from_host), wake_token)
                        .unwrap(),
                ),
                #[cfg(feature = "xnu-core")]
                TransportConfig::Vsock { host_port } => Transport::Vsock(
                    hostlink_vsock::Hostlink::init(host_port, Some(on_frame_from_host), wake_token)
                        .unwrap(),
                ),
                #[cfg(feature = "winnt")]
                TransportConfig::Serial => Transport::Serial(
                    hostlink_serial::Hostlink::init(Some(on_frame_from_host), wake_token).unwrap(),
                ),
            };
            transport_set(transport);

            tell_the_host_we_are_listening();

            let context = adopt_js_context();
            run_main_loop(context);

            destroy_all_scripts(context);

            #[cfg(any(feature = "win9x", feature = "winnt", feature = "linux-injected",
                feature = "xnu-core"))]
            {
                kernel::stop_copies();
                #[cfg(feature = "win9x")]
                kernel::release_shared_hooks();
                #[cfg(feature = "win9x")]
                kernel::stop_hearing_from_vmm();
            }

            transport_get_unchecked().shutdown();

            #[cfg(feature = "linux-injected")]
            crate::gum_modules::unpublish();

            #[cfg(any(feature = "win9x", feature = "winnt", feature = "linux-injected"))]
            {
                kernel::release_interrupt();
                kernel::release_fault_reporter();
            }

            (&raw mut frida_agent_left).write_volatile(1);

        }
    }

    unsafe fn parse_config(
        config: &[u8],
    ) -> (
        TransportConfig,
        u64,
        Vec<ModuleInfo>,
        SymbolTable,
        GumMemoryRange,
    ) {
        use crate::bindings::{
            GVariantIter, g_variant_get_byte, g_variant_get_child_value, g_variant_get_uint64,
            g_variant_get_variant, g_variant_iter_init, g_variant_iter_next,
        };

        unsafe {
            let type_string = c"((tt)yvta(sstttt)aya(st)a(ttu))".as_ptr() as *const gchar;
            let variant_type = g_variant_type_new(type_string);

            let root_variant = g_variant_new_from_data(
                variant_type,
                config.as_ptr() as *const c_void,
                config.len() as gsize,
                1,
                None,
                ptr::null_mut(),
            );

            let transport_kind_variant = g_variant_get_child_value(root_variant, 1);
            let transport_kind = g_variant_get_byte(transport_kind_variant);

            let transport_cfg_outer = g_variant_get_child_value(root_variant, 2);
            let transport_cfg_inner = g_variant_get_variant(transport_cfg_outer);
            let transport_config = match transport_kind {
                0 => {
                    let mmio_variant = g_variant_get_child_value(transport_cfg_inner, 0);
                    let mmio = g_variant_get_uint64(mmio_variant);
                    let irq_variant = g_variant_get_child_value(transport_cfg_inner, 1);
                    let irq = g_variant_get_uint32(irq_variant);
                    g_variant_unref(irq_variant);
                    g_variant_unref(mmio_variant);
                    TransportConfig::Virtio { mmio, irq }
                }
                1 => {
                    let host_port = g_variant_get_uint32(transport_cfg_inner);
                    #[cfg(feature = "xnu-core")]
                    { TransportConfig::Vsock { host_port } }
                    #[cfg(not(feature = "xnu-core"))]
                    { let _ = host_port; panic!("vsock is XNU's") }
                }
                2 => {
                    let ecam = where_configuration_space_is_mapped(transport_cfg_inner);
                    #[cfg(any(feature = "win9x", feature = "winnt", feature = "linux-injected"))]
                    { TransportConfig::VirtioPci { ecam } }
                    #[cfg(not(any(feature = "win9x", feature = "winnt", feature = "linux-injected")))]
                    { let _ = ecam; panic!("virtio-pci is not this kernel's") }
                }
                3 => {
                    #[cfg(feature = "winnt")]
                    { TransportConfig::Serial }
                    #[cfg(not(feature = "winnt"))]
                    { panic!("serial is NT's") }
                }
                4 => {
                    #[cfg(feature = "linux-injected")]
                    {
                        let raw = g_variant_get_string(transport_cfg_inner, ptr::null_mut());
                        let path = core::ffi::CStr::from_ptr(raw as *const core::ffi::c_char)
                            .to_str()
                            .unwrap_or("")
                            .into();
                        TransportConfig::PipeVsock { path }
                    }
                    #[cfg(not(feature = "linux-injected"))]
                    { panic!("pipe-vsock is linux-injected's") }
                }
                _ => panic!("Unsupported transport kind: {}", transport_kind),
            };
            g_variant_unref(transport_cfg_inner);
            g_variant_unref(transport_cfg_outer);
            g_variant_unref(transport_kind_variant);

            let kernel_base_variant = g_variant_get_child_value(root_variant, 3);
            let kernel_base = g_variant_get_uint64(kernel_base_variant);
            kernel::set_kernel_base(kernel_base);

            let module_info_variant = g_variant_get_child_value(root_variant, 4);
            let mut iter: GVariantIter = core::mem::zeroed();
            g_variant_iter_init(&mut iter as *mut GVariantIter, module_info_variant);

            let mut module_info: Vec<ModuleInfo> = Vec::new();
            let mut raw_name: *mut gchar = ptr::null_mut();
            let mut raw_version: *mut gchar = ptr::null_mut();
            let mut offset: u64 = 0;
            let mut size: u64 = 0;
            let mut start_func_offset: u64 = 0;
            let mut stop_func_offset: u64 = 0;
            while g_variant_iter_next(
                &mut iter as *mut GVariantIter,
                c"(&s&stttt)".as_ptr(),
                &mut raw_name,
                &mut raw_version,
                &mut offset,
                &mut size,
                &mut start_func_offset,
                &mut stop_func_offset,
            ) != 0
            {
                module_info.push(ModuleInfo {
                    name: String::from(CStr::from_ptr(raw_name).to_str().unwrap()),
                    version: String::from(CStr::from_ptr(raw_version).to_str().unwrap()),
                    offset,
                    size,
                    start_func_offset,
                    stop_func_offset,
                });
            }

            let symbol_array_variant = g_variant_get_child_value(root_variant, 5);
            let symbol_data_ptr = g_variant_get_data(symbol_array_variant) as *const u8;
            let symbol_data_size = g_variant_get_size(symbol_array_variant) as usize;
            let symbol_table = SymbolTable::new(core::slice::from_raw_parts(
                symbol_data_ptr,
                symbol_data_size,
            ));

            let told_variant = g_variant_get_child_value(root_variant, 6);
            let mut told: GVariantIter = core::mem::zeroed();
            g_variant_iter_init(&mut told as *mut GVariantIter, told_variant);
            let mut what: *mut gchar = ptr::null_mut();
            let mut number: u64 = 0;
            while g_variant_iter_next(
                &mut told as *mut GVariantIter,
                c"(&st)".as_ptr(),
                &mut what,
                &mut number,
            ) != 0
            {
                kernel::take_note_of(core::ffi::CStr::from_ptr(what).to_str().unwrap_or(""), number);
            }

            let mapped_variant = g_variant_get_child_value(root_variant, 7);
            let mut mapped: GVariantIter = core::mem::zeroed();
            g_variant_iter_init(&mut mapped as *mut GVariantIter, mapped_variant);
            let mut address: u64 = 0;
            let mut size: u64 = 0;
            let mut protection: u32 = 0;
            while g_variant_iter_next(
                &mut mapped as *mut GVariantIter,
                c"(ttu)".as_ptr(),
                &mut address,
                &mut size,
                &mut protection,
            ) != 0
            {
                kernel::take_note_of_mapping(address, size as usize, protection);
            }
            g_variant_unref(mapped_variant);

            let own_range_variant = g_variant_get_child_value(root_variant, 0);
            let mut own_base: u64 = 0;
            let mut own_size: u64 = 0;
            g_variant_get(
                own_range_variant,
                c"(tt)".as_ptr(),
                &mut own_base,
                &mut own_size,
            );
            let own_range = GumMemoryRange {
                base_address: own_base,
                size: own_size as gsize,
            };

            g_variant_unref(own_range_variant);
            g_variant_unref(told_variant);
            g_variant_unref(symbol_array_variant);
            g_variant_unref(module_info_variant);
            g_variant_unref(kernel_base_variant);
            g_variant_unref(root_variant);
            g_variant_type_free(variant_type);

            (
                transport_config,
                kernel_base,
                module_info,
                symbol_table,
                own_range,
            )
        }
    }
}

#[cfg(feature = "xnu-kext")]
mod entrypoint_xnu_kext {
    use super::*;
    use core::ffi::c_int;
    use core::sync::atomic::AtomicBool;

    static WORKER_EXITED: AtomicBool = AtomicBool::new(false);

    #[unsafe(no_mangle)]
    pub extern "C" fn frida_agent_start(own_base: u64, own_size: u64) -> c_int {
        kernel::log("frida: agent starting\n\0");

        unsafe {
            OWN_RANGE = GumMemoryRange {
                base_address: own_base,
                size: own_size as gsize,
            };
        }

        if kernel::spawn_thread(worker, ptr::null_mut()) != 0 {
            kernel::log("frida: failed to spawn worker\n\0");
            return -1;
        }

        0
    }

    #[unsafe(no_mangle)]
    pub extern "C" fn frida_agent_stop() {
        kernel::log("frida: agent stopping\n\0");

        STOP_REQUESTED.store(true, Ordering::Release);

        while !WORKER_EXITED.load(Ordering::Acquire) {
            kernel::wake(ptr::addr_of!(glib::WAKEUP_TOKEN) as *const u8);
            kernel::wait(
                ptr::addr_of!(glib::WAKEUP_TOKEN) as *const u8,
                Some(10_000),
                &mut || WORKER_EXITED.load(Ordering::Acquire),
            );
        }

        kernel::log("frida: agent stopped\n\0");
    }

    #[unsafe(no_mangle)]
    pub extern "C" fn frida_agent_wake() {
        let context = LOOP_CONTEXT.load(Ordering::Acquire) as *mut GMainContext;
        if context.is_null() {
            return;
        }

        unsafe { g_main_context_wakeup(context) };
    }

    static LOOP_CONTEXT: core::sync::atomic::AtomicUsize =
        core::sync::atomic::AtomicUsize::new(0);

    unsafe extern "C" fn worker(_parameter: *mut c_void, _wait_result: i32) {
        unsafe {
            crate::run_constructors();
            init_gum();

            match hostlink_chardev::Hostlink::init(Some(on_frame_from_host)) {
                Ok(hostlink) => transport_set(Transport::CharDevice(hostlink)),
                Err(_) => {
                    kernel::log("frida: no one to answer\n\0");
                    WORKER_EXITED.store(true, Ordering::Release);
                    return;
                }
            }

            let main_context = adopt_js_context();
            LOOP_CONTEXT.store(main_context as usize, Ordering::Release);
            run_main_loop(main_context);
            LOOP_CONTEXT.store(0, Ordering::Release);

            destroy_all_scripts(main_context);
            kernel::stop_copies();
            transport_teardown();

            WORKER_EXITED.store(true, Ordering::Release);
            kernel::wake(ptr::addr_of!(glib::WAKEUP_TOKEN) as *const u8);
        }
    }
}

#[cfg(feature = "linux")]
mod entrypoint_linux {
    use super::*;
    use core::ffi::c_int;
    use core::sync::atomic::AtomicBool;

    static STOP_REQUESTED: AtomicBool = AtomicBool::new(false);
    static WORKER_EXITED: AtomicBool = AtomicBool::new(false);

    /// Called from the module's init path. Everything interesting happens on our
    /// own kernel thread: `insmod` runs with the module mutex held, and bringing
    /// up Gum plus connecting to the host both block.
    #[unsafe(no_mangle)]
    pub extern "C" fn frida_agent_start(own_base: u64, own_size: u64) -> c_int {
        kernel::log("frida: agent starting\n\0");

        unsafe {
            OWN_RANGE = GumMemoryRange {
                base_address: own_base,
                size: own_size as gsize,
            };
        }

        if kernel::spawn_thread(worker, ptr::null_mut()) != 0 {
            kernel::log("frida: failed to spawn worker\n\0");
            return -1;
        }

        0
    }

    /// Called from the module's exit path, which must not return until the worker
    /// is off our text — the module's pages go away right after.
    #[unsafe(no_mangle)]
    pub extern "C" fn frida_agent_stop() {
        kernel::log("frida: agent stopping\n\0");

        STOP_REQUESTED.store(true, Ordering::Release);

        while !WORKER_EXITED.load(Ordering::Acquire) {
            kernel::wake(ptr::addr_of!(glib::WAKEUP_TOKEN) as *const u8);
            kernel::wait(
                ptr::addr_of!(glib::WAKEUP_TOKEN) as *const u8,
                Some(10_000),
                &mut || WORKER_EXITED.load(Ordering::Acquire),
            );
        }

        kernel::log("frida: agent stopped\n\0");
    }

    pub fn stop_requested() -> bool {
        STOP_REQUESTED.load(Ordering::Acquire)
    }

    fn start_worker() {
        kernel::spawn_thread(worker, 12345usize as *mut c_void);
    }

    unsafe extern "C" fn worker(_parameter: *mut c_void, _wait_result: i32) {
        unsafe {
            kernel::log("frida: worker entry\n\0");

            crate::run_constructors();
            init_gum();

            kernel::install_hooks();

            match hostlink_chardev::Hostlink::init(Some(on_frame_from_host)) {
                Ok(hostlink) => transport_set(Transport::CharDevice(hostlink)),
                Err(_) => {
                    kernel::log("frida: failed to connect to peer\n\0");
                    WORKER_EXITED.store(true, Ordering::Release);
                    return;
                }
            }

            let main_context = adopt_js_context();

            run_main_loop(main_context);

            destroy_all_scripts(main_context);
            transport_teardown();

            WORKER_EXITED.store(true, Ordering::Release);
            kernel::wake(ptr::addr_of!(glib::WAKEUP_TOKEN) as *const u8);
        }
    }
}

#[cfg(feature = "linux")]
pub use entrypoint_linux::{frida_agent_start, frida_agent_stop};
#[cfg(feature = "xnu-kext")]
pub use entrypoint_xnu_kext::{frida_agent_start, frida_agent_stop, frida_agent_wake};
#[cfg(feature = "blob")]
pub use entrypoint_blob::_start;

pub enum Transport {
    #[cfg(feature = "blob")]
    Virtio(hostlink_virtio::Hostlink),
    #[cfg(feature = "winnt")]
    Serial(hostlink_serial::Hostlink),
    #[cfg(feature = "xnu-core")]
    Vsock(hostlink_vsock::Hostlink),
    #[cfg(feature = "linux-injected")]
    PipeVsock(linux::hostlink_pipe_vsock::Hostlink),
    #[cfg(any(feature = "linux", feature = "xnu-kext"))]
    CharDevice(hostlink_chardev::Hostlink),
}

impl Transport {
    pub fn send(&self, payload: &[u8]) {
        match self {
            #[cfg(feature = "blob")]
            Transport::Virtio(h) => h.send(payload),
            #[cfg(feature = "winnt")]
            Transport::Serial(h) => h.send(payload),
            #[cfg(feature = "xnu-core")]
            Transport::Vsock(h) => h.send(payload),
            #[cfg(feature = "linux-injected")]
            Transport::PipeVsock(h) => h.send(payload),
            #[cfg(any(feature = "linux", feature = "xnu-kext"))]
            Transport::CharDevice(h) => h.send(payload),
        }
    }

    pub fn shutdown(&self) {
        match self {
            #[cfg(feature = "blob")]
            Transport::Virtio(h) => h.shutdown(),
            #[cfg(feature = "winnt")]
            Transport::Serial(h) => h.shutdown(),
            #[cfg(feature = "xnu-core")]
            Transport::Vsock(_) => {}
            #[cfg(feature = "linux-injected")]
            Transport::PipeVsock(h) => h.shutdown(),
            #[cfg(any(feature = "linux", feature = "xnu-kext"))]
            Transport::CharDevice(_) => {}
        }
    }

    pub fn process(&self) {
        match self {
            #[cfg(feature = "blob")]
            Transport::Virtio(h) => h.process(),
            #[cfg(feature = "winnt")]
            Transport::Serial(h) => h.process(),
            #[cfg(feature = "xnu-core")]
            Transport::Vsock(h) => h.process(),
            #[cfg(feature = "linux-injected")]
            Transport::PipeVsock(h) => h.process(),
            #[cfg(any(feature = "linux", feature = "xnu-kext"))]
            Transport::CharDevice(h) => h.process(),
        }
    }
}

// Configuration space is reached through I/O ports where there are any, and only a machine
// without them is told where it is mapped instead.
#[cfg(any(target_arch = "aarch64", target_arch = "arm"))]
unsafe fn where_configuration_space_is_mapped(transport: *mut GVariant) -> u64 {
    use crate::bindings::{g_variant_get_child_value, g_variant_get_uint64, g_variant_unref};

    unsafe {
        let mapped = g_variant_get_child_value(transport, 0);
        let ecam = g_variant_get_uint64(mapped);
        g_variant_unref(mapped);

        ecam
    }
}

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
unsafe fn where_configuration_space_is_mapped(_transport: *mut GVariant) -> u64 {
    0
}

#[cfg(feature = "blob")]
pub enum TransportConfig {
    Virtio { mmio: u64, irq: u32 },
    #[cfg(feature = "winnt")]
    Serial,
    #[cfg(any(feature = "win9x", feature = "winnt", feature = "linux-injected"))]
    VirtioPci { ecam: u64 },
    #[cfg(feature = "xnu-core")]
    Vsock { host_port: u32 },
    #[cfg(feature = "linux-injected")]
    PipeVsock { path: alloc::string::String },
}

static mut TRANSPORT_DRIVER: *mut Transport = core::ptr::null_mut();

#[inline(always)]
fn transport_set(driver: Transport) {
    unsafe {
        let boxed = Box::into_raw(Box::new(driver));
        TRANSPORT_DRIVER = boxed;
    }
}

fn tell_the_host_we_are_listening() {
    unsafe {
        let message = g_variant_new(
            c"(yquv)".as_ptr(),
            FridaCommand::Ready as u8 as u32,
            0u32,
            0u32,
            g_variant_new(c"()".as_ptr()),
        );

        if let Some(serialized) = serialize_message(message) {
            send_frame(&serialized);
        }

        g_variant_unref(message);
    }
}

#[inline(always)]
fn send_frame(frame: &[u8]) {
    #[cfg(any(feature = "win9x", feature = "winnt", feature = "linux-injected", feature = "xnu-core"))]
    if unsafe { ROUTED_ARENA } != 0 {
        kernel::publish_frame_to_host(unsafe { ROUTED_ARENA }, frame);
        return;
    }

    transport_get_unchecked().send(frame);
}

#[cfg(any(feature = "win9x", feature = "winnt", feature = "linux-injected", feature = "xnu-core"))]
pub(crate) unsafe fn route_frames_through(arena: u64) {
    unsafe { ROUTED_ARENA = arena };
}

#[cfg(any(feature = "win9x", feature = "winnt", feature = "linux-injected", feature = "xnu-core"))]
static mut ROUTED_ARENA: u64 = 0;

fn transport_get_unchecked() -> &'static Transport {
    unsafe {
        debug_assert!(!TRANSPORT_DRIVER.is_null());
        &*TRANSPORT_DRIVER
    }
}

// Only the XNU backend has to cope with Gum asking for memory work before the
// hostlink that performs it exists.
#[cfg(any(feature = "xnu-core", feature = "linux-injected"))]
#[inline(always)]
fn transport_is_up() -> bool {
    unsafe { !TRANSPORT_DRIVER.is_null() }
}

static mut SCRIPTS: BTreeMap<u32, *mut GumScript> = BTreeMap::new();
static NEXT_SCRIPT_ID: AtomicU32 = AtomicU32::new(1);
// A copy of the agent runs the same code, but it cannot share what gets written to. Thus keep
// the writable half as it is before anything writes to it.
#[cfg(any(feature = "win9x", feature = "winnt", feature = "linux-injected", feature = "xnu-core"))]
pub(crate) unsafe fn preserve_writable_half() {
    let size = writable_half_size();
    let pristine = kernel::alloc(size);
    // A few-hundred-KiB kmalloc needs an order-9 contiguous block, which a fragmented guest can
    // refuse; the pristine copy only feeds process-copies, so proceed without it rather than
    // faulting into a NULL destination.
    if pristine.is_null() {
        return;
    }
    unsafe {
        ptr::copy_nonoverlapping(writable_half_start() as *const u8, pristine, size);
        PRISTINE_WRITABLE_HALF = pristine;
    }
}

// Give a copy the half it writes to, and move every address in it to where the copy runs. The
// address that the copy runs at and the address that takes the bytes are two different ones.
#[cfg(any(feature = "win9x", feature = "winnt", feature = "linux-injected", feature = "xnu-core"))]
pub(crate) unsafe fn install_writable_half(seen_by_copy: usize, writable_from_here: usize) {
    let own = unsafe { ptr::addr_of!(OWN_RANGE).read() }.base_address as usize;
    let distance = seen_by_copy.wrapping_sub(own);
    let private_offset = writable_half_start() - own;

    unsafe {
        ptr::copy_nonoverlapping(PRISTINE_WRITABLE_HALF, writable_from_here as *mut u8,
            writable_half_size());

        #[cfg(feature = "xnu-kext")]
        let (mut entry, end) = (frida_agent_relocs_start, frida_agent_relocs_end);
        #[cfg(not(feature = "xnu-kext"))]
        let (mut entry, end) = (&raw const _agent_relocs_start as usize,
            &raw const _agent_relocs_end as usize);
        while entry != end {
            let slot = (writable_from_here + (entry as *const usize).read() - private_offset)
                as *mut usize;
            slot.write(slot.read().wrapping_add(distance));
            entry += RELOCATION_SIZE;
        }
    }
}

#[cfg(feature = "xnu-kext")]
pub(crate) unsafe fn run_constructors() {
    unsafe {
        let mut entry = frida_agent_init_start;
        while entry != frida_agent_init_end {
            let signed = (entry as *const usize).read() as *const u8;
            let start: extern "C" fn() =
                core::mem::transmute(crate::pac::ptrauth_strip_data(signed));
            start();
            entry += core::mem::size_of::<usize>();
        }
    }
}

#[cfg(not(feature = "xnu-kext"))]
pub(crate) unsafe fn run_constructors() {
    unsafe {
        let mut entry = &raw const _agent_init_start as usize;
        let end = &raw const _agent_init_end as usize;
        while entry != end {
            let start: extern "C" fn() = core::mem::transmute((entry as *const usize).read());
            start();
            entry += core::mem::size_of::<usize>();
        }
    }
}

#[cfg(feature = "xnu-kext")]
pub(crate) fn writable_half_start() -> usize {
    unsafe { frida_agent_private_start }
}


#[cfg(all(any(feature = "win9x", feature = "winnt", feature = "linux-injected",
    feature = "xnu-core"), not(feature = "xnu-kext")))]
pub(crate) fn writable_half_start() -> usize {
    &raw const _agent_private_start as usize
}

#[cfg(feature = "xnu-kext")]
fn writable_half_size() -> usize {
    unsafe { frida_agent_heap_start - writable_half_start() }
}

#[cfg(all(any(feature = "win9x", feature = "winnt", feature = "linux-injected",
    feature = "xnu-core"), not(feature = "xnu-kext")))]
fn writable_half_size() -> usize {
    (&raw const _heap_start as usize) - writable_half_start()
}

// A relocation names the slot first, and the rest of it says the same thing as the value that
// the slot already holds.
#[cfg(any(target_arch = "x86", target_arch = "arm"))]
const RELOCATION_SIZE: usize = 8;
#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
const RELOCATION_SIZE: usize = 24;

#[cfg(any(feature = "win9x", feature = "winnt", feature = "linux-injected", feature = "xnu-core"))]
static mut PRISTINE_WRITABLE_HALF: *mut u8 = ptr::null_mut();

#[cfg(not(feature = "xnu-kext"))]
unsafe extern "C" {
    static _agent_init_start: u8;
    static _agent_init_end: u8;
}

#[cfg(all(any(feature = "win9x", feature = "winnt", feature = "linux-injected",
    feature = "xnu-core"), not(feature = "xnu-kext")))]
unsafe extern "C" {
    static _agent_private_start: u8;
    static _heap_start: u8;
    static _agent_relocs_start: u8;
    static _agent_relocs_end: u8;
}

#[cfg(feature = "xnu-kext")]
unsafe extern "C" {
    static frida_agent_init_start: usize;
    static frida_agent_init_end: usize;
    static frida_agent_private_start: usize;
    static frida_agent_heap_start: usize;
    static frida_agent_relocs_start: usize;
    static frida_agent_relocs_end: usize;
}

// A copy runs at a base of its own, thus the half that placed it there says where. The
// Stalker keeps out of this range: it must not instrument the runtime it runs on.
pub(crate) unsafe fn set_own_range(base: u64, size: u64) {
    unsafe {
        OWN_RANGE = GumMemoryRange {
            base_address: base,
            size: size as gsize,
        };
    }
}

#[cfg(feature = "winnt")]
pub(crate) fn own_code() -> (usize, usize) {
    let start = entrypoint_blob::_start as usize;

    (start, (&raw const _agent_private_start as usize) - start)
}

pub(crate) fn own_range() -> (usize, usize) {
    let range = unsafe { ptr::addr_of!(OWN_RANGE).read() };

    (range.base_address as usize, range.size as usize)
}

pub(crate) fn own_range_contains(address: u32) -> bool {
    let range = unsafe { ptr::addr_of!(OWN_RANGE).read() };
    let base = range.base_address as u32;

    address >= base && address - base < range.size as u32
}

static mut OWN_RANGE: GumMemoryRange = GumMemoryRange {
    base_address: 0,
    size: 0,
};

static NEXT_REQUEST_ID: AtomicU32 = AtomicU32::new(1);
static mut PENDING_REPLIES: BTreeMap<u16, *mut GVariant> = BTreeMap::new();
static mut PENDING_WAITERS: BTreeMap<u16, usize> = BTreeMap::new();

unsafe fn init_gum() {
    unsafe { init_gum_with_exceptor(true) };
}

// The Exceptor installs fault handling for the full machine, which only the kernel half may
// do. A copy in a process would take it from the half that uses it.
pub(crate) unsafe fn init_gum_without_exceptor() {
    unsafe { init_gum_with_exceptor(false) };
}

unsafe fn init_gum_with_exceptor(exceptor: bool) {
    unsafe {
        bindings::g_set_panic_handler(Some(signed_to_be_called_back(frida_panic_handler, 0)), ptr::null_mut());
        bindings::gum_init_embedded();
        if exceptor {
            bindings::gum_exceptor_obtain();
        }
        bindings::g_log_set_default_handler(Some(signed_to_be_called_back(frida_log_handler, 0)), ptr::null_mut());

        gum_script_scheduler_disable_background_thread(gum_script_backend_get_scheduler());
    }
}

pub(crate) fn on_frame_from_host(frame: &[u8]) {
    let Some(variant) = deserialize_message(frame) else {
        return;
    };

    // A frame for a process that the host attached to belongs to the copy in that process. The
    // copy also runs this code, but it has no targets, thus it continues.
    #[cfg(any(feature = "win9x", feature = "winnt", feature = "linux-injected", feature = "xnu-core"))]
    {
        let destination = destination_of(variant);
        if let Some(arena) = kernel::arena_for_pid(destination) {
            unsafe { g_variant_unref(variant) };
            kernel::forward_frame(arena, frame);
            return;
        }
    }

    process_incoming_message(variant);
    unsafe { g_variant_unref(variant) };
}

#[cfg(any(feature = "win9x", feature = "winnt", feature = "linux-injected", feature = "xnu-core"))]
fn destination_of(variant: *mut GVariant) -> u32 {
    let mut command: u8 = 0;
    let mut request_id: u16 = 0;
    let mut destination: u32 = 0;
    let mut payload: *mut GVariant = ptr::null_mut();
    unsafe {
        g_variant_get(
            variant,
            c"(yquv)".as_ptr(),
            &mut command,
            &mut request_id,
            &mut destination,
            &mut payload,
        );
        g_variant_unref(payload);
    }

    destination
}

pub(crate) unsafe fn adopt_js_context() -> *mut GMainContext {
    unsafe {
        JS_THREAD_ID = kernel::current_thread_id();
        let context = gum_script_scheduler_get_js_context(gum_script_backend_get_scheduler());

        // Acquires the context as well, which is what lets this thread run the jobs the
        // asynchronous script calls queue on it. Being the thread default is what makes
        // a script created here deliver its messages here too.
        g_main_context_push_thread_default(context);

        context
    }
}

static mut JS_THREAD_ID: u64 = 0;

pub(crate) fn on_js_thread() -> bool {
    let id = unsafe { JS_THREAD_ID };
    id != 0 && kernel::current_thread_id() == id
}

pub(crate) static STOP_REQUESTED: core::sync::atomic::AtomicBool =
    core::sync::atomic::AtomicBool::new(false);

#[unsafe(no_mangle)]
pub static mut frida_agent_left: u32 = 0;

pub(crate) fn note_unhandled_fault(vector: u64, pc: u64, address: u64) {
    unsafe {
        let fault = &raw mut frida_agent_fault;
        (*fault).count += 1;
        (*fault).vector = vector;
        (*fault).pc = pc;
        (*fault).address = address;
    }
}

#[unsafe(no_mangle)]
pub static mut frida_agent_fault: AgentFault = AgentFault {
    count: 0,
    vector: 0,
    pc: 0,
    address: 0,
};

#[repr(C)]
pub struct AgentFault {
    pub count: u64,
    pub vector: u64,
    pub pc: u64,
    pub address: u64,
}

pub(crate) fn stop_requested() -> bool {
    STOP_REQUESTED.load(Ordering::Acquire)
}

fn run_main_loop(main_context: *mut GMainContext) {
    SERVICE_CONTEXT.store(main_context as usize, Ordering::Release);

    glib::own_the_loop();

    #[cfg(any(feature = "blob", feature = "xnu-core"))]
    watch_for_work(main_context, kernel_half_has_work, serve_the_kernel_half);

    unsafe {
        loop {
            #[cfg(not(any(feature = "blob", feature = "xnu-core")))]
            transport_get_unchecked().process();

            #[cfg(feature = "linux")]
            if entrypoint_linux::stop_requested() {
                return;
            }

            if stop_requested() {
                return;
            }

            dispatch_pending_work(main_context);
        }
    }
}

#[cfg(any(feature = "win9x", feature = "winnt", feature = "linux-injected", feature = "xnu-core"))]
fn kernel_half_has_work() -> bool {
    #[cfg(not(feature = "xnu-core"))]
    if hostlink_virtio::a_turn_is_wanted() {
        return true;
    }

    #[cfg(feature = "winnt")]
    if hostlink_serial::a_turn_is_wanted() {
        return true;
    }

    #[cfg(feature = "linux-injected")]
    if linux::hostlink_pipe_vsock::a_turn_is_wanted() {
        return true;
    }

    #[cfg(feature = "xnu-kext")]
    if hostlink_chardev::a_turn_is_wanted() {
        return true;
    }

    #[cfg(feature = "xnu-core")]
    if hostlink_vsock::a_turn_is_wanted() || kernel::a_spawn_is_held()
        || kernel::a_copy_is_asking_for_something()
        || kernel::a_program_is_wanted() || kernel::applications_are_asked_for()
    {
        return true;
    }

    #[cfg(feature = "win9x")]
    if deferred_work_is_waiting() || kernel::a_patch_is_wanted() {
        return true;
    }

    if kernel::a_frame_waits_for_room() {
        return true;
    }

    #[cfg(feature = "linux-injected")]
    if kernel::a_copy_has_something_to_say() || kernel::a_spawn_is_held() {
        return true;
    }

    #[cfg(all(feature = "linux-injected", target_arch = "aarch64"))]
    if kernel::a_copy_wants_executable() {
        return true;
    }

    kernel::injected_arenas()
        .iter()
        .any(|arena| kernel::holds_a_frame_from_target(*arena))
}

#[cfg(any(feature = "win9x", feature = "winnt", feature = "linux-injected", feature = "xnu-core"))]
fn serve_the_kernel_half() {
    #[cfg(feature = "xnu-core")]
    relay_frames_from_targets();

    #[cfg(feature = "linux-injected")]
    kernel::tell_of_held_spawns(&mut |id, program| {
        let said = alloc::ffi::CString::new(program).unwrap();
        tell_the_host_of_a_spawn(id, said.as_ptr() as *const u8);
    });

    unsafe { transport_get_unchecked().process() };


    #[cfg(feature = "win9x")]
    serve_deferred_work();

    #[cfg(all(feature = "linux-injected", target_arch = "aarch64"))]
    kernel::serve_executable_requests();

    #[cfg(feature = "linux-injected")]
    kernel::report_what_the_copies_hit();

    relay_frames_from_targets();
}

pub(crate) fn watch_for_work(main_context: *mut GMainContext, ready: fn() -> bool, serve: fn()) {
    unsafe {
        WORK_READY = Some(ready);
        WORK_SERVE = Some(serve);

        #[cfg(feature = "xnu-kext")]
        sign_what_the_loop_calls_back();

        let source = g_source_new(&raw mut WORK_FUNCS, core::mem::size_of::<GSource>() as u32);
        WORK_SOURCE = source;
        g_source_attach(source, main_context);
        g_source_unref(source);
    }
}

static mut WORK_SOURCE: *mut GSource = ptr::null_mut();

#[repr(C)]
pub struct GPollFd {
    pub fd: i32,
    pub events: u16,
    pub revents: u16,
    pub user_data: *mut c_void,
}

pub type GPollFunc = unsafe extern "C" fn(*mut GPollFd, u32, i32) -> i32;

unsafe extern "C" {
    fn g_source_add_poll(source: *mut GSource, fd: *mut GPollFd);
    fn g_main_context_set_poll_func(context: *mut GMainContext, func: GPollFunc);
}

static mut WAKEUP_POLLFD: GPollFd = GPollFd { fd: -1, events: 0, revents: 0, user_data: ptr::null_mut() };

pub(crate) fn watch_a_descriptor(context: *mut GMainContext, fd: i32, poll: GPollFunc) {
    const G_IO_IN: u16 = 1;
    unsafe {
        WAKEUP_POLLFD = GPollFd { fd, events: G_IO_IN, revents: 0, user_data: ptr::null_mut() };
        g_source_add_poll(WORK_SOURCE, ptr::addr_of_mut!(WAKEUP_POLLFD));
        g_main_context_set_poll_func(context, poll);
    }
}

#[cfg(feature = "xnu-kext")]
pub(crate) unsafe fn signed_to_be_called_back<T>(callback: T, discriminator: core::ffi::c_uint) -> T {
    unsafe {
        core::mem::transmute_copy(&crate::pac::ptrauth_sign(
            core::mem::transmute_copy::<T, *const u8>(&callback), discriminator as usize))
    }
}

#[cfg(not(feature = "xnu-kext"))]
pub(crate) unsafe fn signed_to_be_called_back<T>(callback: T, _discriminator: core::ffi::c_uint) -> T {
    callback
}

#[cfg(feature = "xnu-kext")]
unsafe fn sign_what_the_loop_calls_back() {
    unsafe {
        let funcs = &raw mut WORK_FUNCS;
        (*funcs).prepare = Some(signed_to_be_called_back(work_prepare, 0));
        (*funcs).check = Some(signed_to_be_called_back(work_check, 0));
        (*funcs).dispatch = Some(signed_to_be_called_back(work_dispatch, 0));
    }
}

static mut WORK_READY: Option<fn() -> bool> = None;
static mut WORK_SERVE: Option<fn()> = None;

unsafe extern "C" fn work_prepare(source: *mut GSource, timeout: *mut i32) -> gboolean {
    unsafe {
        *timeout = -1;

        work_check(source)
    }
}

unsafe extern "C" fn work_check(_source: *mut GSource) -> gboolean {
    let Some(ready) = (unsafe { ptr::addr_of!(WORK_READY).read() }) else {
        return 0;
    };

    ready() as gboolean
}

unsafe extern "C" fn work_dispatch(_source: *mut GSource, _callback: GSourceFunc,
        _data: gpointer) -> gboolean {
    if let Some(serve) = unsafe { ptr::addr_of!(WORK_SERVE).read() } {
        serve();
    }

    1
}

static mut WORK_FUNCS: GSourceFuncs = GSourceFuncs {
    prepare: Some(work_prepare),
    check: Some(work_check),
    dispatch: Some(work_dispatch),
    finalize: None,
    closure_callback: None,
    closure_marshal: None,
};

// This call blocks. GLib sleeps until one of its timeouts is due, or until something wakes
// the loop.
pub(crate) unsafe fn dispatch_pending_work(main_context: *mut GMainContext) {
    unsafe {
        g_main_context_iteration(main_context, 1);
    }
}

static SERVICE_CONTEXT: core::sync::atomic::AtomicUsize =
    core::sync::atomic::AtomicUsize::new(0);

pub(crate) fn nudge_the_loop(token: *const u8) {
    let context = SERVICE_CONTEXT.load(Ordering::Acquire) as *mut GMainContext;
    if !context.is_null() {
        unsafe { g_main_context_wakeup(context) };
    }
    kernel::wake(token);
}

pub(crate) fn destroy_all_scripts(main_context: *mut GMainContext) {
    unsafe {
        let scripts = core::mem::take(core::ptr::addr_of_mut!(SCRIPTS).as_mut().unwrap());
        for (_, script) in scripts {
            UNLOADS_IN_FLIGHT.fetch_add(1, Ordering::Relaxed);
            gum_script_unload(script, ptr::null_mut(),
                Some(signed_to_be_called_back(on_script_unloaded, 0)),
                ptr::null_mut());
        }

        while UNLOADS_IN_FLIGHT.load(Ordering::Relaxed) != 0 {
            g_main_context_iteration(main_context, 0);
            kernel::yield_now();
        }
    }
}

unsafe extern "C" fn on_script_unloaded(
    source_object: *mut GObject,
    result: *mut GAsyncResult,
    _user_data: gpointer,
) {
    unsafe {
        let script = source_object as *mut GumScript;

        gum_script_unload_finish(script, result);
        g_object_unref(script as *mut c_void);

        UNLOADS_IN_FLIGHT.fetch_sub(1, Ordering::Relaxed);
    }
}

static UNLOADS_IN_FLIGHT: AtomicU32 = AtomicU32::new(0);

#[cfg(any(feature = "linux", feature = "xnu-kext"))]
fn transport_teardown() {
    unsafe {
        let driver = TRANSPORT_DRIVER;
        TRANSPORT_DRIVER = ptr::null_mut();
        drop(Box::from_raw(driver));
    }
}

unsafe fn serialize_message(variant: *mut GVariant) -> Option<Vec<u8>> {
    unsafe {
        let size = g_variant_get_size(variant) as usize;
        if size == 0 {
            return None;
        }

        let data_ptr = g_variant_get_data(variant) as *const u8;
        let mut result = Vec::with_capacity(size);
        result.resize(size, 0);
        core::ptr::copy_nonoverlapping(data_ptr, result.as_mut_ptr(), size);

        Some(result)
    }
}

fn deserialize_message(data: &[u8]) -> Option<*mut GVariant> {
    unsafe {
        if data.is_empty() {
            return None;
        }

        let variant_type = g_variant_type_new(c"(yquv)".as_ptr());
        let data_copy = g_memdup2(data.as_ptr() as *const c_void, data.len() as gsize);
        let variant = g_variant_new_from_data(
            variant_type,
            data_copy,
            data.len() as gsize,
            0,
            Some(signed_to_be_called_back(g_free, 0)),
            data_copy,
        );
        g_variant_type_free(variant_type);

        if variant.is_null() {
            None
        } else {
            Some(variant)
        }
    }
}

fn process_incoming_message(variant: *mut GVariant) {
    {
        let mut cmd_value: u8 = 0;
        let mut request_id: u16 = 0;
        let mut payload_variant: *mut GVariant = ptr::null_mut();

        let mut destination: u32 = 0;
        let cmd = unsafe {
            g_variant_get(
                variant,
                c"(yquv)".as_ptr(),
                &mut cmd_value,
                &mut request_id,
                &mut destination,
                &mut payload_variant,
            );

            core::mem::transmute::<u8, FridaCommand>(cmd_value)
        };

        if cmd == FridaCommand::Reply {
            unsafe {
                core::ptr::addr_of_mut!(PENDING_REPLIES)
                    .as_mut()
                    .unwrap()
                    .insert(request_id, payload_variant);
                if let Some(token) = waiter_of(request_id) {
                    kernel::wake(token);
                }
            }
            return;
        }

        let response = match cmd {
            FridaCommand::CreateScript => handle_create_script(payload_variant, request_id),
            FridaCommand::LoadScript => handle_load_script(payload_variant, request_id),
            FridaCommand::DestroyScript => handle_destroy_script(payload_variant, request_id),
            FridaCommand::PostScriptMessage => Some(handle_post_script_message(payload_variant)),
            #[cfg(any(feature = "win9x", feature = "linux-injected", feature = "xnu-core"))]
            FridaCommand::GateSpawns => Some(handle_gate_spawns(payload_variant)),
            #[cfg(any(feature = "win9x", feature = "winnt"))]
            FridaCommand::EnumerateApplications => Some(handle_enumerate_applications(payload_variant)),
            #[cfg(feature = "xnu-core")]
            FridaCommand::EnumerateApplications => {
                kernel::list_applications_when_the_loop_can(request_id);
                None
            }
            #[cfg(any(feature = "win9x", feature = "winnt"))]
            FridaCommand::EnumerateShortcuts => Some(handle_enumerate_shortcuts()),
            #[cfg(any(
                feature = "win9x",
                feature = "winnt",
                feature = "linux-injected",
                feature = "xnu-core"
            ))]
            FridaCommand::EnumerateProcesses => Some(handle_enumerate_processes(payload_variant)),
            #[cfg(feature = "win9x")]
            FridaCommand::InjectIntoProcess => handle_inject_into_process(payload_variant, request_id),
            #[cfg(any(feature = "linux-injected", feature = "xnu-core"))]
            FridaCommand::InjectIntoProcess => Some(handle_inject_into_process(payload_variant)),
            #[cfg(feature = "win9x")]
            FridaCommand::AllocateShared => handle_allocate_shared(payload_variant, request_id),
            #[cfg(feature = "winnt")]
            FridaCommand::PlaceAgentInProcess => Some(handle_place_agent_in_process(payload_variant)),
            #[cfg(feature = "winnt")]
            FridaCommand::StartAgentInProcess => Some(handle_start_agent_in_process(payload_variant)),
            #[cfg(any(feature = "win9x", feature = "winnt", feature = "linux-injected"))]
            FridaCommand::SpawnProcess => Some(handle_spawn_process(payload_variant)),
            #[cfg(feature = "xnu-core")]
            FridaCommand::SpawnProcess => {
                kernel::spawn_when_the_loop_can(request_id, &words_run_together(payload_variant));
                None
            }
            #[cfg(any(feature = "win9x", feature = "winnt", feature = "linux-injected",
                feature = "xnu-core"))]
            FridaCommand::ResumeProcess => Some(handle_resume_process(payload_variant)),
            #[cfg(any(
                feature = "win9x",
                feature = "winnt",
                feature = "linux-injected",
                feature = "xnu-core"
            ))]
            FridaCommand::Stop => Some(handle_stop()),
            #[cfg(any(feature = "winnt", feature = "linux-injected", feature = "xnu-core"))]
            FridaCommand::DetachFromProcess => {
                Some(handle_detach_from_process(payload_variant))
            }
            #[cfg(feature = "win9x")]
            FridaCommand::DetachFromProcess => {
                unsafe {
                    PENDING_DETACH = (request_id, g_variant_get_uint32(payload_variant));
                    DETACH_PENDING = true;
                }
                None
            }
            _ => Some(HandlerResponse::error("Unknown command")),
        };

        if let Some(response) = response {
            send_command_reply(request_id, response);
        }

        unsafe { g_variant_unref(payload_variant) };
    }
}

// You can queue this APC only when VMM is between jobs, not during a host command. Thus the
// work is deferred and the answer comes later.
#[cfg(feature = "win9x")]
fn handle_inject_into_process(payload: *mut GVariant, request_id: u16) -> Option<HandlerResponse> {
    unsafe {
        PENDING_INJECTION = (request_id, g_variant_get_uint32(payload));
        INJECTION_PENDING = true;
    }

    None
}

#[cfg(any(feature = "linux-injected", feature = "xnu-core"))]
fn handle_inject_into_process(payload: *mut GVariant) -> HandlerResponse {
    unsafe {
        let reached = kernel::inject_into_process(g_variant_get_uint32(payload));

        HandlerResponse::success(g_variant_new_uint32(reached))
    }
}

#[cfg(feature = "winnt")]
fn handle_place_agent_in_process(payload: *mut GVariant) -> HandlerResponse {
    unsafe {
        let placed = kernel::place_agent_in_process(g_variant_get_uint32(payload));

        HandlerResponse::success(g_variant_new_uint32(placed as u32))
    }
}

#[cfg(feature = "winnt")]
fn handle_start_agent_in_process(payload: *mut GVariant) -> HandlerResponse {
    unsafe {
        let reached = kernel::start_agent_in_process(g_variant_get_uint32(payload));
        HandlerResponse::success(g_variant_new_uint32(reached))
    }
}

// All processes can read the shared arena, and ring 3 can execute from it.
#[cfg(feature = "win9x")]
fn handle_allocate_shared(payload: *mut GVariant, request_id: u16) -> Option<HandlerResponse> {
    unsafe {
        PENDING_ALLOCATION = (request_id, g_variant_get_uint32(payload));
        ALLOCATION_PENDING = true;
    }

    None
}

// Commands arrive in the interrupt callback of the transport, where VMM makes no threads and
// gives no heap. Thus that callback only sets a flag, and the loop does the work.
#[cfg(feature = "win9x")]
fn deferred_work_is_waiting() -> bool {
    unsafe {
        ptr::addr_of!(ALLOCATION_PENDING).read()
            || ptr::addr_of!(INJECTION_PENDING).read()
            || ptr::addr_of!(DETACH_PENDING).read()
    }
}

#[cfg(feature = "win9x")]
fn serve_deferred_work() {
    serve_pending_allocation();
    serve_pending_injection();
    serve_pending_detach();
}

#[cfg(feature = "win9x")]
fn serve_pending_allocation() {
    if !unsafe { ptr::addr_of!(ALLOCATION_PENDING).read() } {
        return;
    }
    unsafe { ALLOCATION_PENDING = false };

    let (request_id, size) = unsafe { PENDING_ALLOCATION };
    let address = kernel::allocate_shared(size as usize);
    let response = if address == 0 {
        HandlerResponse::error("Unable to allocate shared memory")
    } else {
        HandlerResponse::success(unsafe { g_variant_new_uint32(address) })
    };
    send_command_reply(request_id, response);
}

#[cfg(feature = "win9x")]
fn serve_pending_injection() {
    if !unsafe { ptr::addr_of!(INJECTION_PENDING).read() } {
        return;
    }
    unsafe { INJECTION_PENDING = false };

    let (request_id, pid) = unsafe { PENDING_INJECTION };
    let observed = kernel::inject_agent(pid);
    let response = if observed == 0 {
        HandlerResponse::error("Unable to inject into process")
    } else {
        HandlerResponse::success(unsafe { g_variant_new_uint32(observed) })
    };
    send_command_reply(request_id, response);
}

#[cfg(feature = "win9x")]
fn serve_pending_detach() {
    if !unsafe { ptr::addr_of!(DETACH_PENDING).read() } {
        return;
    }
    unsafe { DETACH_PENDING = false };

    let (request_id, pid) = unsafe { PENDING_DETACH };
    let response = if kernel::detach_from_process(pid) {
        HandlerResponse::success(unsafe { g_variant_new_uint32(pid) })
    } else {
        HandlerResponse::error("Unable to detach from process")
    };
    send_command_reply(request_id, response);
}


#[cfg(feature = "xnu-core")]
pub static mut ASKED_THE_HOST: u32 = 0;

// The copy sends complete frames, thus the half with the hostlink sends the bytes without a
// change.
#[cfg(any(feature = "win9x", feature = "winnt", feature = "linux-injected", feature = "xnu-core"))]
fn relay_frames_from_targets() {
    #[cfg(feature = "xnu-core")]
    {
        kernel::serve_what_the_copies_ask();
        kernel::look_for_new_processes();
        kernel::tell_of_held_spawns(&mut |id, program| {
            let said = alloc::ffi::CString::new(program).unwrap();
            tell_the_host_of_a_spawn(id, said.as_ptr() as *const u8);
        });
        if let Some(request_id) = kernel::asked_for_applications() {
            send_command_reply(request_id, what_is_installed());
        }
        kernel::start_what_was_asked_for(&mut |request_id, id| {
            let response = if id != 0 {
                HandlerResponse::success(unsafe { g_variant_new_uint32(id) })
            } else {
                HandlerResponse::error("Unable to spawn")
            };
            send_command_reply(request_id, response);
        });
    }

    #[cfg(feature = "win9x")]
    kernel::serve_patch_requests();

    kernel::serve_waiting_frames();

    for arena in kernel::injected_arenas() {
        while let Some(frame) = kernel::take_frame_from_target(arena) {
            send_frame(&frame);
        }
    }
}

#[cfg(feature = "win9x")]
static mut ALLOCATION_PENDING: bool = false;

#[cfg(feature = "win9x")]
static mut PENDING_ALLOCATION: (u16, u32) = (0, 0);

#[cfg(feature = "win9x")]
static mut DETACH_PENDING: bool = false;

#[cfg(feature = "win9x")]
static mut PENDING_DETACH: (u16, u32) = (0, 0);

#[cfg(feature = "win9x")]
static mut INJECTION_PENDING: bool = false;

#[cfg(feature = "win9x")]
static mut PENDING_INJECTION: (u16, u32) = (0, 0);

#[cfg(any(feature = "win9x", feature = "winnt"))]
fn describe(path: *const u8) -> *const gchar {
    if path.is_null() {
        return c"".as_ptr();
    }

    kernel::describe_image(path) as *const gchar
}

#[cfg(feature = "xnu-core")]
fn what_is_installed() -> HandlerResponse {
    unsafe {
        let list_type = g_variant_type_new(c"a(sss)".as_ptr() as *const gchar);
        let application_type = g_variant_type_new(c"(sss)".as_ptr() as *const gchar);
        let builder = g_variant_builder_new(list_type);

        kernel::each_application(&mut |identifier, program, shown| {
            let said = |bytes: &[u8]| alloc::ffi::CString::new(bytes).unwrap();
            let (identifier, program, shown) = (said(identifier), said(program), said(shown));

            g_variant_builder_open(builder, application_type);
            g_variant_builder_add(builder, c"s".as_ptr(), identifier.as_ptr());
            g_variant_builder_add(builder, c"s".as_ptr(), program.as_ptr());
            g_variant_builder_add(builder, c"s".as_ptr(), shown.as_ptr());
            g_variant_builder_close(builder);
        });

        let list = g_variant_builder_end(builder);

        g_variant_type_free(list_type);
        g_variant_type_free(application_type);

        HandlerResponse::success(list)
    }
}

#[cfg(any(feature = "win9x", feature = "winnt"))]
fn handle_enumerate_applications(payload: *mut GVariant) -> HandlerResponse {
    unsafe {
        let wanted = SelectedIdentifiers::from(payload);

        let list_type = g_variant_type_new(c"a(sss)".as_ptr() as *const gchar);
        let application_type = g_variant_type_new(c"(sss)".as_ptr() as *const gchar);
        let builder = g_variant_builder_new(list_type);

        kernel::enumerate_applications(&mut |identifier, path| {
            let mut identifier_text = [0u8; 64];
            let mut path_text = [0u8; 260];
            let identifier = as_text(identifier, &mut identifier_text);
            let path = as_text(path, &mut path_text);

            let identity = kernel::identify_image(path as *const u8);
            let named = if unsafe { identity.read() } != 0 { identity as *const gchar } else {
                identifier
            };

            if !wanted.contains(named) {
                return;
            }

            g_variant_builder_open(builder, application_type);
            g_variant_builder_add(builder, c"s".as_ptr(), named);
            g_variant_builder_add(builder, c"s".as_ptr(), path);
            g_variant_builder_add(builder, c"s".as_ptr(), describe(path as *const u8));
            g_variant_builder_close(builder);
        });

        let list = g_variant_builder_end(builder);

        g_variant_type_free(list_type);
        g_variant_type_free(application_type);

        HandlerResponse::success(list)
    }
}

struct SelectedIdentifiers {
    names: *mut *const gchar,
    count: usize,
}

impl SelectedIdentifiers {
    unsafe fn from(variant: *mut GVariant) -> Self {
        let mut count: gsize = 0;
        let names = unsafe { g_variant_get_strv(variant, &mut count) };
        SelectedIdentifiers { names, count: count as usize }
    }

    unsafe fn contains(&self, name: *const gchar) -> bool {
        if self.count == 0 {
            return true;
        }
        let selected = unsafe { core::slice::from_raw_parts(self.names, self.count) };
        selected.iter().any(|&candidate| unsafe { c_string_equal(candidate, name) })
    }
}

impl Drop for SelectedIdentifiers {
    fn drop(&mut self) {
        unsafe { g_free(self.names as gpointer) };
    }
}

unsafe fn c_string_equal(a: *const gchar, b: *const gchar) -> bool {
    unsafe {
        let mut offset = 0;
        loop {
            let left = a.add(offset).read();
            let right = b.add(offset).read();
            if left != right {
                return false;
            }
            if left == 0 {
                return true;
            }
            offset += 1;
        }
    }
}

#[cfg(any(feature = "win9x", feature = "winnt"))]
fn handle_enumerate_shortcuts() -> HandlerResponse {
    unsafe {
        let list_type = g_variant_type_new(c"a(ssss)".as_ptr() as *const gchar);
        let shortcut_type = g_variant_type_new(c"(ssss)".as_ptr() as *const gchar);
        let builder = g_variant_builder_new(list_type);

        kernel::enumerate_shortcuts(&mut |identity, target, shown, description| {
            let mut identity_text = [0u8; 128];
            let mut target_text = [0u8; 260];
            let mut shown_text = [0u8; 128];
            let mut description_text = [0u8; 128];

            g_variant_builder_open(builder, shortcut_type);
            g_variant_builder_add(builder, c"s".as_ptr(),
                as_text(identity.as_bytes(), &mut identity_text));
            g_variant_builder_add(builder, c"s".as_ptr(),
                as_text(target.as_bytes(), &mut target_text));
            g_variant_builder_add(builder, c"s".as_ptr(),
                as_text(shown.as_bytes(), &mut shown_text));
            g_variant_builder_add(builder, c"s".as_ptr(),
                as_text(description.as_bytes(), &mut description_text));
            g_variant_builder_close(builder);
        });

        let list = g_variant_builder_end(builder);

        g_variant_type_free(shortcut_type);
        g_variant_type_free(list_type);

        HandlerResponse::success(list)
    }
}

#[cfg(any(feature = "win9x", feature = "winnt"))]
fn as_text<'a>(bytes: &[u8], into: &'a mut [u8]) -> *const gchar {
    let taken = core::cmp::min(bytes.len(), into.len() - 1);
    into[..taken].copy_from_slice(&bytes[..taken]);
    into[taken] = 0;

    into.as_ptr() as *const gchar
}

#[cfg(any(feature = "win9x", feature = "linux-injected", feature = "xnu-core"))]
fn handle_gate_spawns(payload: *mut GVariant) -> HandlerResponse {
    kernel::gate_spawns(unsafe { g_variant_get_boolean(payload) } != 0);



    HandlerResponse::success(unsafe { g_variant_new_uint32(0) })
}

#[cfg(any(feature = "win9x", feature = "linux-injected", feature = "xnu-core"))]
pub(crate) fn tell_the_host_of_a_spawn(pid: u32, command_line: *const u8) {
    unsafe {
        let message = g_variant_new(
            c"(yquv)".as_ptr(),
            FridaCommand::SpawnAdded as u8 as u32,
            0u32,
            source_process_id(),
            g_variant_new(c"(us)".as_ptr(), pid, command_line as *const gchar),
        );

        if let Some(serialized) = serialize_message(message) {
            send_frame(&serialized);
        }

        g_variant_unref(message);
    }
}

#[cfg(any(
    feature = "win9x",
    feature = "winnt",
    feature = "linux-injected",
    feature = "xnu-core"
))]
fn handle_enumerate_processes(payload: *mut GVariant) -> HandlerResponse {
    unsafe {
        let flags = g_variant_get_child_value(payload, 0);
        let include_icons = g_variant_get_boolean(flags) != 0;
        g_variant_unref(flags);

        let selection = g_variant_get_child_value(payload, 1);
        let wanted = SelectedPids::from(selection);

        let list_type = g_variant_type_new(c"a(usssaay)".as_ptr() as *const gchar);
        let process_type = g_variant_type_new(c"(usssaay)".as_ptr() as *const gchar);
        let icons_type = g_variant_type_new(c"aay".as_ptr() as *const gchar);
        let byte_type = g_variant_type_new(c"y".as_ptr() as *const gchar);
        let builder = g_variant_builder_new(list_type);

        if wanted.contains(KERNEL_PID) {
            g_variant_builder_open(builder, process_type);
            g_variant_builder_add(builder, c"u".as_ptr(), KERNEL_PID);
            g_variant_builder_add(builder, c"s".as_ptr(), c"".as_ptr());
            g_variant_builder_add(builder, c"s".as_ptr(), c"".as_ptr());
            g_variant_builder_add(builder, c"s".as_ptr(), kernel_name());
            g_variant_builder_open(builder, icons_type);
            g_variant_builder_close(builder);
            g_variant_builder_close(builder);
        }

        kernel::enumerate_processes(&mut |process| {
            if !wanted.contains(process.id) {
                return;
            }

            let path = text_or_empty(process.path);

            g_variant_builder_open(builder, process_type);
            g_variant_builder_add(builder, c"u".as_ptr(), process.id);
            g_variant_builder_add(builder, c"s".as_ptr(), path);
            g_variant_builder_add(builder, c"s".as_ptr(), text_or_empty(process.command_line));
            g_variant_builder_add(
                builder,
                c"s".as_ptr(),
                text_or_empty(kernel::describe_process(&process)),
            );

            g_variant_builder_open(builder, icons_type);
            if include_icons && !process.path.is_null() {
                kernel::enumerate_icons(process.path, &mut |bytes| {
                    let icon = g_variant_new_fixed_array(
                        byte_type,
                        bytes.as_ptr() as *const c_void,
                        bytes.len() as gsize,
                        1,
                    );
                    g_variant_builder_add_value(builder, icon);
                });
            }
            g_variant_builder_close(builder);

            g_variant_builder_close(builder);
        });

        let processes = g_variant_builder_end(builder);

        g_variant_type_free(byte_type);
        g_variant_type_free(icons_type);
        g_variant_type_free(process_type);
        g_variant_type_free(list_type);
        g_variant_unref(selection);

        HandlerResponse::success(processes)
    }
}

struct SelectedPids {
    pids: *const u32,
    count: usize,
}

impl SelectedPids {
    unsafe fn from(variant: *mut GVariant) -> Self {
        let mut count: gsize = 0;
        let pids = unsafe { g_variant_get_fixed_array(variant, &mut count, 4) } as *const u32;
        SelectedPids { pids, count: count as usize }
    }

    fn contains(&self, pid: u32) -> bool {
        if self.count == 0 {
            return true;
        }
        let selected = unsafe { core::slice::from_raw_parts(self.pids, self.count) };
        selected.contains(&pid)
    }
}

#[cfg(feature = "win9x")]
fn kernel_name() -> *const gchar {
    c"VMM".as_ptr() as *const gchar
}

#[cfg(feature = "winnt")]
fn kernel_name() -> *const gchar {
    c"ntoskrnl.exe".as_ptr() as *const gchar
}

#[cfg(any(feature = "linux", feature = "linux-injected"))]
fn kernel_name() -> *const gchar {
    c"kernel".as_ptr() as *const gchar
}

#[cfg(feature = "xnu-core")]
fn kernel_name() -> *const gchar {
    c"kernel".as_ptr() as *const gchar
}

const KERNEL_PID: u32 = 0;

#[cfg(any(
    feature = "win9x",
    feature = "winnt",
    feature = "linux-injected",
    feature = "xnu-core"
))]
fn text_or_empty(text: *const u8) -> *const core::ffi::c_char {
    if text.is_null() {
        c"".as_ptr()
    } else {
        text as *const core::ffi::c_char
    }
}

fn send_command_reply(request_id: u16, response: HandlerResponse) {

    unsafe {
        let message = g_variant_new(
            c"(yquv)".as_ptr(),
            FridaCommand::Reply as u8 as u32,
            request_id as u32,
            0u32,
            response.variant,
        );

        if let Some(serialized) = serialize_message(message) {
            send_frame(&serialized);
        }

        g_variant_unref(message);
    }
}

pub fn host_rpc(command: FridaCommand, payload: *mut GVariant) -> *mut GVariant {
    unsafe {
        let request_id = NEXT_REQUEST_ID.fetch_add(1, Ordering::Relaxed) as u16;
        let message = g_variant_new(
            c"(yquv)".as_ptr(),
            command as u8 as u32,
            request_id as u32,
            0u32,
            payload,
        );
        let transport = transport_get_unchecked();
        transport.send(&serialize_message(message).unwrap());
        g_variant_unref(message);

        let on_loop = glib::is_loop_thread();
        let wait_event = if on_loop {
            glib::wakeup_token()
        } else {
            ptr::addr_of!(request_id) as *const u8
        };
        if !on_loop {
            note_waiter(request_id, wait_event);
        }

        #[cfg(feature = "xnu-core")]
        let reply = loop {
            if on_loop {
                transport.process();
            }
            if let Some(reply) = take_pending_reply(request_id) {
                break reply;
            }

            kernel::wait(wait_event, None, &mut || {
                hostlink_vsock::a_turn_is_wanted() || pending_reply_arrived(request_id)
            });
        };

        #[cfg(not(feature = "xnu-core"))]
        let reply = loop {
            let mut reply: Option<*mut GVariant> = None;
            kernel::wait(wait_event, None, &mut || {
                if on_loop {
                    transport.process();
                }
                reply = take_pending_reply(request_id);
                reply.is_some()
            });
            if let Some(reply) = reply {
                break reply;
            }
        };

        if !on_loop {
            forget_waiter(request_id);
        }

        reply
    }
}

fn note_waiter(request_id: u16, token: *const u8) {
    unsafe {
        core::ptr::addr_of_mut!(PENDING_WAITERS)
            .as_mut()
            .unwrap()
            .insert(request_id, token as usize);
    }
}

fn waiter_of(request_id: u16) -> Option<*const u8> {
    unsafe {
        core::ptr::addr_of!(PENDING_WAITERS)
            .as_ref()
            .unwrap()
            .get(&request_id)
            .map(|token| *token as *const u8)
    }
}

fn forget_waiter(request_id: u16) {
    unsafe {
        core::ptr::addr_of_mut!(PENDING_WAITERS)
            .as_mut()
            .unwrap()
            .remove(&request_id);
    }
}

#[cfg(feature = "xnu-core")]
fn pending_reply_arrived(request_id: u16) -> bool {
    unsafe {
        core::ptr::addr_of!(PENDING_REPLIES)
            .as_ref()
            .unwrap()
            .contains_key(&request_id)
    }
}

fn take_pending_reply(request_id: u16) -> Option<*mut GVariant> {
    unsafe {
        core::ptr::addr_of_mut!(PENDING_REPLIES)
            .as_mut()
            .unwrap()
            .remove(&request_id)
    }
}

#[cfg(feature = "xnu-core")]
fn words_run_together(payload: *mut GVariant) -> alloc::vec::Vec<u8> {
    let mut said = alloc::vec::Vec::new();
    unsafe {
        for index in 0..crate::bindings::g_variant_n_children(payload) {
            let word = crate::bindings::g_variant_get_child_value(payload, index);
            said.extend_from_slice(
                core::ffi::CStr::from_ptr(g_variant_get_string(word, ptr::null_mut())).to_bytes());
            said.push(0);
            g_variant_unref(word);
        }
    }
    said.push(0);

    said
}

#[cfg(feature = "linux-injected")]
fn handle_spawn_process(payload: *mut GVariant) -> HandlerResponse {
    unsafe {
        let mut words: Vec<&str> = Vec::new();
        for index in 0..crate::bindings::g_variant_n_children(payload) {
            let word = crate::bindings::g_variant_get_child_value(payload, index);
            let said = core::ffi::CStr::from_ptr(g_variant_get_string(word, ptr::null_mut()));
            words.push(said.to_str().unwrap_or(""));
            g_variant_unref(word);
        }

        let pid = kernel::spawn_process(&words);
        if pid == 0 {
            return HandlerResponse::error("Unable to spawn");
        }

        HandlerResponse::success(g_variant_new_uint32(pid))
    }
}

#[cfg(any(feature = "win9x", feature = "winnt"))]
fn handle_spawn_process(payload: *mut GVariant) -> HandlerResponse {
    unsafe {
        let command_line = core::ffi::CStr::from_ptr(g_variant_get_string(payload,
            core::ptr::null_mut()));
        let pid = kernel::spawn_process(command_line.to_str().unwrap_or(""));
        if pid == 0 {
            return HandlerResponse::error("Unable to spawn");
        }

        HandlerResponse::success(g_variant_new_uint32(pid))
    }
}

#[cfg(any(feature = "win9x", feature = "winnt", feature = "linux-injected", feature = "xnu-core"))]
fn handle_stop() -> HandlerResponse {
    #[cfg(feature = "xnu")]
    {
        kernel::give_the_word_back();
        kernel::take_the_bell_down();
        kernel::show_our_threads_again();
    }

    #[cfg(feature = "blob")]
    STOP_REQUESTED.store(true, Ordering::Release);

    HandlerResponse::success(unsafe { g_variant_new_uint32(0) })
}

#[cfg(any(feature = "winnt", feature = "linux-injected", feature = "xnu-core"))]
fn handle_detach_from_process(payload: *mut GVariant) -> HandlerResponse {
    let left = unsafe { kernel::detach_from_process(g_variant_get_uint32(payload)) };
    if !left {
        return HandlerResponse::error("Unable to detach from the process");
    }

    HandlerResponse::success(unsafe { g_variant_new_uint32(0) })
}

#[cfg(any(feature = "win9x", feature = "winnt", feature = "linux-injected", feature = "xnu-core"))]
fn handle_resume_process(payload: *mut GVariant) -> HandlerResponse {
    let resumed = unsafe { kernel::resume_process(g_variant_get_uint32(payload)) };
    if !resumed {
        return HandlerResponse::error("Process is not held");
    }

    HandlerResponse::success(unsafe { g_variant_new_uint32(0) })
}

fn handle_create_script(payload_variant: *mut GVariant, request_id: u16) -> Option<HandlerResponse> {
    unsafe {
        if g_variant_check_format_string(payload_variant, c"s".as_ptr(), 0) == 0 {
            return Some(HandlerResponse::error("Invalid payload format: expected string"));
        }
        let source = g_variant_get_string(payload_variant, core::ptr::null_mut());
        gum_script_backend_create(
            gum_script_backend_obtain_qjs(),
            c"agent.js".as_ptr(),
            source,
            ptr::null_mut(),
            ptr::null_mut(),
            Some(signed_to_be_called_back(on_script_created, 0)),
            Box::into_raw(Box::new(request_id)) as *mut c_void,
        );
        None
    }
}

unsafe extern "C" fn on_script_created(
    source_object: *mut GObject,
    result: *mut GAsyncResult,
    user_data: gpointer,
) {
    unsafe {
        let request_id = *Box::from_raw(user_data as *mut u16);

        let mut error: *mut GError = ptr::null_mut();
        let script = gum_script_backend_create_finish(
            source_object as *mut GumScriptBackend,
            result,
            &mut error,
        );

        script_is_ready(script, error, request_id);
    }
}

unsafe fn script_is_ready(script: *mut GumScript, error: *mut GError, request_id: u16) {
    unsafe {
        if !error.is_null() {
            let message = String::from(CStr::from_ptr((*error).message).to_str().unwrap());
            g_error_free(error);
            send_command_reply(request_id, HandlerResponse::error(&message));
            return;
        }

        exclude_own_range_from_stalker(script);

        let script_id = NEXT_SCRIPT_ID.fetch_add(1, Ordering::Relaxed);

        gum_script_set_message_handler(
            script,
            Some(signed_to_be_called_back(frida_message_handler, 0)),
            Box::into_raw(Box::new(script_id)) as *mut c_void,
            None,
        );

        core::ptr::addr_of_mut!(SCRIPTS)
            .as_mut()
            .unwrap()
            .insert(script_id, script);

        send_command_reply(request_id, HandlerResponse::success(g_variant_new_uint32(script_id)));
    }
}

// Stalker must not instrument our own runtime: when it follows the current
// thread into the hostlink transport it would deadlock on the locks that the
// RPC it depends on is holding. On XNU the host injected the image, so it tells
// us our range as part of the config; on Linux we ask the module loader.
unsafe fn exclude_own_range_from_stalker(script: *mut GumScript) {
    unsafe {
        gum_stalker_exclude(gum_script_get_stalker(script), core::ptr::addr_of!(OWN_RANGE));
    }
}

const BYTE_TYPE: *const GVariantType = c"y".as_ptr() as *const GVariantType;

unsafe extern "C" fn frida_message_handler(
    message: *const gchar,
    data: *mut GBytes,
    user_data: gpointer,
) {
    unsafe {
        let script_id = *(user_data as *const u32);

        let mut size: gsize = 0;
        let bytes = if data.is_null() {
            ptr::null()
        } else {
            g_bytes_get_data(data, &mut size)
        };

        let message_variant = g_variant_new(
            c"(yquv)".as_ptr(),
            FridaCommand::ScriptMessage as u8 as u32,
            0u32,
            source_process_id(),
            g_variant_new(
                c"(us@ay)".as_ptr(),
                script_id,
                message,
                g_variant_new_fixed_array(BYTE_TYPE, bytes as *const c_void, size, 1),
            ),
        );

        if let Some(serialized) = serialize_message(message_variant) {
            send_frame(&serialized);
        }

        g_variant_unref(message_variant);
    }
}

#[cfg(any(feature = "win9x", feature = "winnt"))]
pub(crate) fn routed_arena() -> u64 {
    unsafe { ROUTED_ARENA }
}

#[cfg(any(feature = "win9x", feature = "winnt"))]
pub(crate) fn running_in_a_process() -> bool {
    unsafe { ROUTED_ARENA != 0 }
}

// Each copy numbers its scripts from one, thus a message says which process it comes from and
// the host has no two scripts of the same name.
// Which session a frame belongs to is the process the copy was placed in, which on a kernel
// that gives the copy a task of its own is not the same as the process it is.
pub(crate) fn source_process_id() -> u32 {
    #[cfg(any(feature = "win9x", feature = "winnt"))]
    if running_in_a_process() {
        return kernel::current_process_id();
    }

    #[cfg(feature = "linux-injected")]
    if kernel::in_copy() {
        return kernel::home_process_id();
    }

    #[cfg(feature = "xnu-core")]
    if crate::xnu::in_copy() {
        return kernel::current_process_id();
    }

    0
}

fn handle_load_script(payload_variant: *mut GVariant, request_id: u16) -> Option<HandlerResponse> {
    unsafe {
        if g_variant_check_format_string(payload_variant, c"u".as_ptr(), 0) == 0 {
            return Some(HandlerResponse::error("Invalid payload format: expected uint32"));
        }
        let script_id = g_variant_get_uint32(payload_variant);

        let Some(script) = get_script_by_id(script_id) else {
            return Some(HandlerResponse::error(&format!("Script with ID {} not found", script_id)));
        };

        gum_script_load(
            script,
            ptr::null_mut(),
            Some(signed_to_be_called_back(on_script_loaded, 0)),
            Box::into_raw(Box::new(request_id)) as *mut c_void,
        );
        None
    }
}

unsafe extern "C" fn on_script_loaded(
    source_object: *mut GObject,
    result: *mut GAsyncResult,
    user_data: gpointer,
) {
    unsafe {
        let request_id = *Box::from_raw(user_data as *mut u16);

        gum_script_load_finish(source_object as *mut GumScript, result);

        send_command_reply(request_id, HandlerResponse::success_empty());
    }
}

fn handle_destroy_script(payload_variant: *mut GVariant, request_id: u16) -> Option<HandlerResponse> {
    unsafe {
        if g_variant_check_format_string(payload_variant, c"u".as_ptr(), 0) == 0 {
            return Some(HandlerResponse::error("Invalid payload format: expected uint32"));
        }
        let script_id = g_variant_get_uint32(payload_variant);

        let scripts = core::ptr::addr_of_mut!(SCRIPTS).as_mut().unwrap();
        let Some(script) = scripts.remove(&script_id) else {
            return Some(HandlerResponse::error(&format!("Script with ID {} not found", script_id)));
        };

        gum_script_unload(
            script,
            ptr::null_mut(),
            Some(signed_to_be_called_back(on_script_destroyed, 0)),
            Box::into_raw(Box::new(request_id)) as *mut c_void,
        );
        None
    }
}

unsafe extern "C" fn on_script_destroyed(
    source_object: *mut GObject,
    result: *mut GAsyncResult,
    user_data: gpointer,
) {
    unsafe {
        let request_id = *Box::from_raw(user_data as *mut u16);
        let script = source_object as *mut GumScript;

        gum_script_unload_finish(script, result);
        g_object_unref(script as *mut c_void);

        send_command_reply(request_id, HandlerResponse::success_empty());
    }
}

fn handle_post_script_message(payload_variant: *mut GVariant) -> HandlerResponse {
    unsafe {
        if g_variant_check_format_string(payload_variant, c"(usay)".as_ptr(), 0) == 0 {
            return HandlerResponse::error(
                "Invalid payload format: expected (uint32, string, byte array)");
        }
        let mut script_id: u32 = 0;
        let mut message: *const gchar = ptr::null();
        let mut blob: *mut GVariant = ptr::null_mut();
        g_variant_get(
            payload_variant,
            c"(u&s@ay)".as_ptr(),
            &mut script_id,
            &mut message,
            &mut blob,
        );

        let Some(script) = get_script_by_id(script_id) else {
            g_variant_unref(blob);
            return HandlerResponse::error(&format!("Script with ID {} not found", script_id));
        };

        let mut size: gsize = 0;
        let bytes = g_variant_get_fixed_array(blob, &mut size, 1) as *const u8;
        let data = if size != 0 {
            g_bytes_new(bytes as *const c_void, size)
        } else {
            ptr::null_mut()
        };

        gum_script_post(script, message, data);

        if !data.is_null() {
            g_bytes_unref(data);
        }
        g_variant_unref(blob);

        HandlerResponse::success_empty()
    }
}

unsafe fn get_script_by_id(script_id: u32) -> Option<*mut GumScript> {
    unsafe {
        let scripts = core::ptr::addr_of_mut!(SCRIPTS).as_mut().unwrap();
        scripts.get(&script_id).copied()
    }
}

pub(crate) unsafe extern "C" fn frida_panic_handler(
    message: *const core::ffi::c_char,
    _user_data: *mut c_void,
) {
    let msg = unsafe { CStr::from_ptr(message).to_str().unwrap() };
    panic!("[Frida] {}", msg);
}

pub(crate) unsafe extern "C" fn frida_log_handler(
    _log_domain: *const core::ffi::c_char,
    _log_level: i32,
    message: *const core::ffi::c_char,
    _user_data: *mut c_void,
) {
    let msg = unsafe { CStr::from_ptr(message).to_str().unwrap() };
    kprintln!("[Frida] {}", msg);
}

#[panic_handler]
fn panic(info: &core::panic::PanicInfo<'_>) -> ! {
    let mut s = format!("{}", info);
    s.push('\0');
    kernel::panic(s.as_str());
    #[allow(unreachable_code)]
    loop {}
}

pub struct KernelAllocator;

unsafe impl GlobalAlloc for KernelAllocator {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        let mut memory: *mut c_void = ptr::null_mut();
        let alignment = layout.align().max(SMALLEST_ALIGNMENT);
        if unsafe { posix_memalign(&mut memory, alignment, layout.size()) } != 0 {
            return ptr::null_mut();
        }

        memory as *mut u8
    }

    unsafe fn dealloc(&self, ptr: *mut u8, _layout: Layout) {
        unsafe { free(ptr as *mut c_void) };
    }
}

const SMALLEST_ALIGNMENT: usize = 2 * core::mem::size_of::<*const u8>();

unsafe extern "C" {
    fn posix_memalign(memory: *mut *mut c_void, alignment: usize, size: usize) -> i32;
    fn free(memory: *mut c_void);
}

#[global_allocator]
static GLOBAL: KernelAllocator = KernelAllocator;
extern crate alloc;

#[macro_export]
macro_rules! kprintln {
    ($($arg:tt)*) => {{
        use core::fmt::Write;
        let mut buf = alloc::string::String::new();
        write!(&mut buf, $($arg)*).unwrap();
        buf.push('\n');
        buf.push('\0');
        $crate::kernel::log(&buf)
    }};
}
