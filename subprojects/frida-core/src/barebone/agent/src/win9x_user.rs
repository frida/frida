// The half of the Win9x agent that runs in ring 3, inside a process the host attached to. It
// reaches nothing of the kernel's, thus it has primitives of its own, which frida_win9x_user_main
// selects before it does anything else.

use alloc::collections::BTreeMap;
use alloc::string::String;
use alloc::vec::Vec;
use core::ffi::c_void;
use core::sync::atomic::{AtomicBool, Ordering};

use crate::kernel::ThreadEntry;
use crate::win9x::*;

// No code in this image calls the ring 3 entry point, thus tell the linker to keep it.
#[used]
static USER_ENTRY: extern "C" fn(u32) = frida_win9x_user_main;

// This is the ring 3 half. It has its own copy of the image, thus its data is its own. Only
// the primitives that need the kernel are different.
#[unsafe(no_mangle)]
pub extern "C" fn frida_win9x_user_main(arena: u32) {
    select_user();
    resolve_user_api();
    crate::gthread::use_thread_slots(&SLOTS);
    unsafe {
        crate::set_own_range(((arena + IMAGE_BASE) as *const u32).read_volatile() as u64,
            ((arena + IMAGE_SIZE) as *const u32).read_volatile() as u64)
    };
    unsafe { crate::run_constructors() };
    unsafe { crate::init_gum_without_exceptor() };
    hear_about_faults();

    let sleep: unsafe extern "stdcall" fn(u32) =
        unsafe { core::mem::transmute(user_api().sleep as usize) };

    // This thread waits for the host. Do not use yield_now() here, because that rate makes the
    // guest slow.
    unsafe { ((arena + RUNNING_FLAG) as *mut u32).write_volatile(1) };
    while unsafe { ((arena + GO_FLAG) as *const u32).read_volatile() } == 0 {
        unsafe { sleep(PARKED_SLEEP_MS) };
    }

    spawn_thread(user_worker, arena as *mut c_void);

    while unsafe { ((arena + STOP_REQUEST) as *const u32).read_volatile() } == 0 {
        unsafe { sleep(PARKED_SLEEP_MS) };
    }

    stop_hearing_about_faults();
    unsafe { ((arena + MAIN_STOPPED) as *mut u32).write_volatile(1) };
    loop {
        unsafe { sleep(PARKED_SLEEP_MS) };
    }
}

// The group that the user-mode half runs on. The order is the order in Primitives.
pub static USER: Primitives = Primitives {
    alloc,
    free,
    alloc_code,
    free_code,
    spawn_thread,
    wait,
    wake,
    yield_now,
    monotonic_micros,
    current_process_id,
    current_thread_id,
    protect,
    protection_at,
    enumerate_ranges,
    enumerate_threads,
    find_thread,
    modify_thread,
};

static SLOTS: crate::gthread::ThreadSlots = crate::gthread::ThreadSlots {
    take: take_thread_slot,
    get: read_thread_slot,
    set: write_thread_slot,
};

fn take_thread_slot() -> u32 {
    let take: unsafe extern "stdcall" fn() -> u32 =
        unsafe { core::mem::transmute(user_api().tls_alloc as usize) };

    unsafe { take() }
}

fn read_thread_slot(slot: u32) -> *mut c_void {
    let read: unsafe extern "stdcall" fn(u32) -> *mut c_void =
        unsafe { core::mem::transmute(user_api().tls_get_value as usize) };

    unsafe { read(slot) }
}

fn write_thread_slot(slot: u32, value: *mut c_void) {
    let write: unsafe extern "stdcall" fn(u32, *mut c_void) -> u32 =
        unsafe { core::mem::transmute(user_api().tls_set_value as usize) };

    unsafe { write(slot, value) };
}

fn hear_about_faults() {
    let install: unsafe extern "stdcall" fn(u32) -> u32 =
        unsafe { core::mem::transmute(user_api().set_unhandled_exception_filter as usize) };

    unsafe { PREVIOUS_FILTER = install(on_fault as u32) };
}

fn stop_hearing_about_faults() {
    let install: unsafe extern "stdcall" fn(u32) -> u32 =
        unsafe { core::mem::transmute(user_api().set_unhandled_exception_filter as usize) };

    unsafe { install(PREVIOUS_FILTER) };
}

unsafe extern "stdcall" fn on_fault(pointers: *const u32) -> i32 {
    let record = unsafe { pointers.read() } as *const u32;
    let context = unsafe { pointers.add(1).read() } as *mut u8;

    let code = unsafe { record.read() };
    let at = unsafe { record.add(RECORD_ADDRESS).read() };
    let accessed = unsafe { record.add(RECORD_ACCESSED).read() };

    let state = unsafe { crate::win9x::cpu_state_of(context) };
    let mut cpu_context = crate::gum_windows::cpu_context_from(&state);

    let handled = unsafe {
        crate::bindings::gum_barebone_handle_exception(crate::gum_windows::fault_type_of(code),
            at as *mut c_void, accessed as *mut c_void, &mut cpu_context)
    };
    if handled == 0 {
        let previous = unsafe { PREVIOUS_FILTER };
        if previous == 0 {
            return SEARCH_ON;
        }

        let chain: unsafe extern "stdcall" fn(*const u32) -> i32 =
            unsafe { core::mem::transmute(previous as usize) };

        return unsafe { chain(pointers) };
    }

    let state = crate::gum_windows::cpu_state_from(&cpu_context);
    unsafe { crate::win9x::write_cpu_state(context, &state) };

    CARRY_ON
}

static mut PREVIOUS_FILTER: u32 = 0;

const CARRY_ON: i32 = -1;
const SEARCH_ON: i32 = 0;
const RECORD_ADDRESS: usize = 3;
const RECORD_ACCESSED: usize = 6;

fn copy_has_work() -> bool {
    let arena = unsafe { ARENA };

    unsafe {
        a_thread_is_gone(arena)
            || crate::win9x::holds_a_frame_from_host(arena as u64)
            || (((arena + GATING) as *const u32).read_volatile() != 0) != GATING_HERE
            || ((arena + STOP_REQUEST) as *const u32).read_volatile() != 0
    }
}

fn serve_the_copy() {
    let arena = unsafe { ARENA };

    while let Some(id) = take_a_departed_thread(arena) {
        crate::gum_windows::thread_vanished(id);
    }

    while let Some(frame) = take_frame_from_host(arena as u64) {
        crate::on_frame_from_host(&frame);
    }

    let wanted = unsafe { ((arena + GATING) as *const u32).read_volatile() } != 0;
    if wanted != unsafe { GATING_HERE } {
        unsafe { GATING_HERE = wanted };
        gate_spawns(wanted);
    }
}

fn a_thread_is_gone(arena: u32) -> bool {
    unsafe {
        ((arena + GONE_HEAD) as *const u32).read_volatile()
            != ((arena + GONE_TAIL) as *const u32).read_volatile()
    }
}

fn take_a_departed_thread(arena: u32) -> Option<u32> {
    unsafe {
        let head = ((arena + GONE_HEAD) as *const u32).read_volatile();
        let tail = ((arena + GONE_TAIL) as *const u32).read_volatile();
        if head == tail {
            return None;
        }

        let id = ((arena + GONE_SLOTS + (tail % GONE_COUNT) * 4) as *const u32).read_volatile();
        ((arena + GONE_TAIL) as *mut u32).write_volatile(tail.wrapping_add(1));

        Some(id)
    }
}

static mut ARENA: u32 = 0;

unsafe extern "C" fn user_worker(parameter: *mut c_void, _wait_result: i32) {
    let arena = parameter as u32;
    unsafe { ARENA = arena };
    let context = unsafe { crate::adopt_js_context() };

    unsafe { ((arena + OBSERVED_PID) as *mut u32).write_volatile(current_process_id()) };

    unsafe { crate::route_frames_through(arena as u64) };

    unsafe {
        ((arena + LOOP_THREAD) as *mut u32).write_volatile(crate::win9x::get_cur_thread_handle())
    };

    crate::gum_windows::watch_the_loader();
    crate::glib::own_the_loop();
    crate::watch_for_work(context, copy_has_work, serve_the_copy);

    while unsafe { ((arena + STOP_REQUEST) as *const u32).read_volatile() } == 0 {
        unsafe { crate::dispatch_pending_work(context) };
    }

    unsafe { ((arena + WORKER_STOPPED) as *mut u32).write_volatile(1) };
}

unsafe extern "stdcall" fn frida_win9x_user_thread(start: *mut c_void) -> u32 {
    let start = start as *mut UserThreadStart;
    let UserThreadStart { entry, parameter } = unsafe { start.read() };
    free(start as *mut u8, core::mem::size_of::<UserThreadStart>());

    unsafe { entry(parameter, 0) };

    0
}

struct UserThreadStart {
    entry: ThreadEntry,
    parameter: *mut c_void,
}

fn enumerate_ranges(found: &mut dyn FnMut(u64, u64, u32)) {
    let mut address = 0usize;
    loop {
        let Some(region) = describe_region(address) else {
            return;
        };

        if region.protection != 0 {
            found(region.base as u64, region.size as u64, region.protection);
        }

        address = region.base + region.size;
    }
}

fn enumerate_threads(found: &mut dyn FnMut(crate::kernel::ThreadInfo), with_registers: bool) {
    crate::win9x::enumerate_threads_of(current_process_id(), found, with_registers)
}

fn find_thread(id: u32, with_registers: bool) -> Option<crate::kernel::ThreadInfo> {
    let thread = crate::win9x::thread_handle_of(current_process_id(), id)?;

    Some(crate::kernel::ThreadInfo {
        id,
        cpu_state: with_registers.then(|| crate::win9x::thread_cpu_state(thread)).flatten(),
    })
}

fn modify_thread(id: u32, change: &mut dyn FnMut(&mut crate::kernel::CpuState)) -> bool {
    let Some(thread) = crate::win9x::thread_handle_of(current_process_id(), id) else {
        return false;
    };

    crate::win9x::modify_thread_at(thread, change)
}

fn protection_at(address: usize) -> u32 {
    match describe_region(address) {
        Some(region) => region.protection,
        None => 0,
    }
}

fn describe_region(address: usize) -> Option<Region> {
    let mut info = [0u32; REGION_WORDS];
    let query: extern "stdcall" fn(usize, *mut u32, u32) -> u32 =
        unsafe { core::mem::transmute(user_api().virtual_query) };
    if query(address, info.as_mut_ptr(), (REGION_WORDS * 4) as u32) == 0 {
        return None;
    }

    Some(Region {
        base: info[REGION_BASE] as usize,
        size: info[REGION_SIZE] as usize,
        protection: if info[REGION_STATE] == MEM_COMMIT {
            gum_protection_of(info[REGION_PROTECTION])
        } else {
            0
        },
    })
}

fn gum_protection_of(protection: u32) -> u32 {
    match protection & 0xff {
        PAGE_READONLY | PAGE_EXECUTE_READ => GUM_PAGE_READ | GUM_PAGE_EXECUTE,
        PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY =>
            GUM_PAGE_READ | GUM_PAGE_WRITE | GUM_PAGE_EXECUTE,
        PAGE_EXECUTE => GUM_PAGE_READ | GUM_PAGE_EXECUTE,
        _ => 0,
    }
}

struct Region {
    base: usize,
    size: usize,
    protection: u32,
}

const REGION_WORDS: usize = 7;
const REGION_BASE: usize = 0;
const REGION_SIZE: usize = 3;
const REGION_STATE: usize = 4;
const REGION_PROTECTION: usize = 5;
const MEM_COMMIT: u32 = 0x1000;
const PAGE_READONLY: u32 = 0x02;
const PAGE_READWRITE: u32 = 0x04;
const PAGE_WRITECOPY: u32 = 0x08;
const PAGE_EXECUTE: u32 = 0x10;
const PAGE_EXECUTE_READ: u32 = 0x20;
const PAGE_EXECUTE_READWRITE: u32 = 0x40;
const PAGE_EXECUTE_WRITECOPY: u32 = 0x80;
const GUM_PAGE_READ: u32 = 0x1;
const GUM_PAGE_WRITE: u32 = 0x2;
const GUM_PAGE_EXECUTE: u32 = 0x4;

pub unsafe extern "stdcall" fn on_module_load(name: *const u8) -> u32 {
    unsafe {
        let n = (ARENA + 0xb0) as *mut u32;
        n.write_volatile(n.read_volatile() + 1);
    }
    let original: extern "stdcall" fn(*const u8) -> u32 =
        unsafe { core::mem::transmute(crate::gum_windows::loader_load()) };

    let handle = original(name);
    note_module(handle, name);
    unsafe {
        let n = (ARENA + 0xb4) as *mut u32;
        n.write_volatile(n.read_volatile() + 1);
    }

    handle
}

pub unsafe extern "stdcall" fn on_module_load_with_flags(name: *const u8, file: u32,
        flags: u32) -> u32 {
    let original: extern "stdcall" fn(*const u8, u32, u32) -> u32 =
        unsafe { core::mem::transmute(crate::gum_windows::loader_load_with_flags()) };

    let handle = original(name, file, flags);
    note_module(handle, name);

    handle
}

pub unsafe extern "stdcall" fn on_module_unload(handle: u32) -> u32 {
    let original: extern "stdcall" fn(u32) -> u32 =
        unsafe { core::mem::transmute(crate::gum_windows::loader_unload()) };

    let base = handle;
    let left = original(handle);
    if left != 0 {
        crate::gum_windows::module_left(base as u64);
    }

    left
}

pub fn thread_entry_points() -> Option<ThreadEntryPoints> {
    Some(ThreadEntryPoints {
        start: thread_starter()?,
        exit: kernel32_export(b"ExitThread") as usize,
    })
}

pub struct ThreadEntryPoints {
    pub start: usize,
    pub exit: usize,
}

pub fn thread_start() -> usize {
    unsafe { THREAD_START as usize }
}

pub fn thread_start_slot() -> *mut *mut c_void {
    &raw mut THREAD_START as *mut *mut c_void
}

pub fn thread_exit_slot() -> *mut *mut c_void {
    &raw mut THREAD_EXIT as *mut *mut c_void
}

pub unsafe extern "stdcall" fn on_thread_exit(status: u32) -> ! {
    crate::gum_windows::thread_vanished(current_thread_id() as u32);

    let original: unsafe extern "stdcall" fn(u32) -> ! =
        unsafe { core::mem::transmute(THREAD_EXIT) };
    unsafe { original(status) }
}

fn thread_starter() -> Option<usize> {
    let api = user_api();
    let create_thread: unsafe extern "stdcall" fn(u32, u32, *mut u8, u32, u32, *mut u32) -> u32 =
        unsafe { core::mem::transmute(api.create_thread as usize) };
    let get_thread_context: unsafe extern "stdcall" fn(u32, *mut u32) -> u32 =
        unsafe { core::mem::transmute(api.get_thread_context as usize) };
    let resume_thread: unsafe extern "stdcall" fn(u32) -> u32 =
        unsafe { core::mem::transmute(api.resume_thread as usize) };
    let close_handle: unsafe extern "stdcall" fn(u32) -> u32 =
        unsafe { core::mem::transmute(api.close_handle as usize) };

    let body = alloc_code(RETURN_AT_ONCE.len());
    unsafe { body.copy_from_nonoverlapping(RETURN_AT_ONCE.as_ptr(), RETURN_AT_ONCE.len()) };

    let mut id = 0u32;
    let mut context = [0u32; CONTEXT_WORDS];
    context[0] = CONTEXT_CONTROL;

    let start = unsafe {
        let thread = create_thread(0, 0, body, 0, CREATE_SUSPENDED, &mut id);
        if thread == 0 {
            return None;
        }

        get_thread_context(thread, context.as_mut_ptr());

        // A new thread waits in a stub of KERNEL32 until the one that made it lets it go. That
        // stub first restores a register frame from the thread's own stack, thus the word above
        // the frame is where the thread goes when it is let go.
        let frame = context[CONTEXT_ESP / 4] + SAVED_REGISTERS;
        let start = (frame as *const u32).read_volatile() as usize;

        resume_thread(thread);
        close_handle(thread);

        start
    };

    Some(start)
}

const RETURN_AT_ONCE: [u8; 5] = [0x33, 0xc0, 0xc2, 0x04, 0x00];
const SAVED_REGISTERS: u32 = 52;

static mut THREAD_START: *mut c_void = core::ptr::null_mut();
static mut THREAD_EXIT: *mut c_void = core::ptr::null_mut();

pub fn loader_entry_points() -> Option<LoaderEntryPoints> {
    let load = kernel32_export(b"LoadLibraryA") as usize;
    let unload = kernel32_export(b"FreeLibrary") as usize;
    if load == 0 || unload == 0 {
        return None;
    }

    Some(LoaderEntryPoints {
        load,
        load_with_flags: kernel32_export(b"LoadLibraryExA") as usize,
        unload,
    })
}

pub struct LoaderEntryPoints {
    pub load: usize,
    pub load_with_flags: usize,
    pub unload: usize,
}

fn note_module(handle: u32, name: *const u8) {
    if handle == 0 {
        return;
    }

    crate::gum_windows::module_arrived(handle as u64, &text_at(name));
}

fn text_at(name: *const u8) -> alloc::string::String {
    let mut text = alloc::string::String::new();
    let mut cursor = name;
    loop {
        let byte = unsafe { cursor.read() };
        if byte == 0 {
            return text;
        }
        text.push(byte as char);
        cursor = unsafe { cursor.add(1) };
    }
}

fn resolve_user_api() {
    unsafe {
        USER_API = UserApi {
            sleep: kernel32_export(b"Sleep"),
            get_tick_count: kernel32_export(b"GetTickCount"),
            get_current_thread_id: kernel32_export(b"GetCurrentThreadId"),
            get_current_process_id: kernel32_export(b"GetCurrentProcessId"),
            get_process_heap: kernel32_export(b"GetProcessHeap"),
            heap_alloc: kernel32_export(b"HeapAlloc"),
            heap_free: kernel32_export(b"HeapFree"),
            virtual_alloc: kernel32_export(b"VirtualAlloc"),
            virtual_free: kernel32_export(b"VirtualFree"),
            virtual_protect: kernel32_export(b"VirtualProtect"),
            virtual_query: kernel32_export(b"VirtualQuery"),
            create_thread: kernel32_export(b"CreateThread"),
            create_process: kernel32_export(b"CreateProcessA"),
            get_thread_context: kernel32_export(b"GetThreadContext"),
            suspend_thread: kernel32_export(b"SuspendThread"),
            resume_thread: kernel32_export(b"ResumeThread"),
            create_event: kernel32_export(b"CreateEventA"),
            set_event: kernel32_export(b"SetEvent"),
            wait_for_single_object: kernel32_export(b"WaitForSingleObject"),
            wait_for_single_object_ex: kernel32_export(b"WaitForSingleObjectEx"),
            exit_thread: kernel32_export(b"ExitThread"),
            close_handle: kernel32_export(b"CloseHandle"),
            set_file_pointer: kernel32_export(b"SetFilePointer"),
            create_file: kernel32_export(b"CreateFileA"),
            read_file: kernel32_export(b"ReadFile"),
            find_first_file: kernel32_export(b"FindFirstFileA"),
            find_next_file: kernel32_export(b"FindNextFileA"),
            find_close: kernel32_export(b"FindClose"),
            set_unhandled_exception_filter: kernel32_export(b"SetUnhandledExceptionFilter"),
            tls_alloc: kernel32_export(b"TlsAlloc"),
            tls_get_value: kernel32_export(b"TlsGetValue"),
            tls_set_value: kernel32_export(b"TlsSetValue"),
        };
    }
}

fn user_api() -> &'static UserApi {
    unsafe { &*core::ptr::addr_of!(USER_API) }
}

struct UserApi {
    sleep: u32,
    get_tick_count: u32,
    get_current_thread_id: u32,
    get_current_process_id: u32,
    get_process_heap: u32,
    heap_alloc: u32,
    heap_free: u32,
    virtual_alloc: u32,
    virtual_free: u32,
    virtual_protect: u32,
    virtual_query: u32,
    create_thread: u32,
    create_process: u32,
    get_thread_context: u32,
    suspend_thread: u32,
    resume_thread: u32,
    create_event: u32,
    set_event: u32,
    wait_for_single_object: u32,
    wait_for_single_object_ex: u32,
    exit_thread: u32,
    close_handle: u32,
    set_file_pointer: u32,
    create_file: u32,
    read_file: u32,
    find_first_file: u32,
    find_next_file: u32,
    find_close: u32,
    set_unhandled_exception_filter: u32,
    tls_alloc: u32,
    tls_get_value: u32,
    tls_set_value: u32,
}

static mut USER_API: UserApi = UserApi {
    sleep: 0,
    get_tick_count: 0,
    get_current_thread_id: 0,
    get_current_process_id: 0,
    get_process_heap: 0,
    heap_alloc: 0,
    heap_free: 0,
    virtual_alloc: 0,
    virtual_free: 0,
    virtual_protect: 0,
    virtual_query: 0,
    create_thread: 0,
    create_process: 0,
    get_thread_context: 0,
    suspend_thread: 0,
    resume_thread: 0,
    create_event: 0,
    set_event: 0,
    wait_for_single_object: 0,
    wait_for_single_object_ex: 0,
    exit_thread: 0,
    close_handle: 0,
    set_file_pointer: 0,
    create_file: 0,
    read_file: 0,
    find_first_file: 0,
    find_next_file: 0,
    find_close: 0,
    set_unhandled_exception_filter: 0,
    tls_alloc: 0,
    tls_get_value: 0,
    tls_set_value: 0,
};

fn slot_for_token(token: *const u8) -> usize {
    let start = (token as usize / core::mem::align_of::<usize>()) % EVENTS.len();
    for step in 0..EVENTS.len() {
        let slot = (start + step) % EVENTS.len();

        let owner = EVENT_OWNERS[slot].load(Ordering::Acquire);
        if owner == token as usize {
            return slot;
        }
        if owner == 0
                && EVENT_OWNERS[slot].compare_exchange(0, token as usize, Ordering::AcqRel,
                    Ordering::Acquire).is_ok() {
            return slot;
        }
    }

    0
}

fn slot_owned_by(token: *const u8) -> Option<usize> {
    let start = (token as usize / core::mem::align_of::<usize>()) % EVENTS.len();
    for step in 0..EVENTS.len() {
        let slot = (start + step) % EVENTS.len();
        if EVENT_OWNERS[slot].load(Ordering::Acquire) == token as usize {
            return Some(slot);
        }
    }

    None
}

fn event_in(slot: usize) -> u32 {
    let existing = EVENTS[slot].load(Ordering::Acquire);
    if existing != 0 {
        return existing;
    }

    let create_event: unsafe extern "stdcall" fn(u32, u32, u32, u32) -> u32 =
        unsafe { core::mem::transmute(user_api().create_event as usize) };
    let created = unsafe { create_event(0, 0, 0, 0) };
    match EVENTS[slot].compare_exchange(0, created, Ordering::AcqRel, Ordering::Acquire) {
        Ok(_) => created,
        Err(raced) => raced,
    }
}

// What a process has mapped is written down where only ring 0 can read it. Thus the kernel half
// hands the list over, and this side only reads it.
pub fn enumerate_modules() -> Vec<LoadedModule> {
    let mut modules = Vec::new();

    let list = unsafe { ((crate::routed_arena() as u32 + MODULE_LIST) as *const u32).read() };
    if list == 0 {
        return modules;
    }

    let mut cursor = list + 4;
    let count = read_word(list);
    for _ in 0..count {
        let base = read_word(cursor);
        let size = read_word(cursor + 4);
        let length = read_word(cursor + 8) as usize;
        let path = unsafe {
            core::slice::from_raw_parts((cursor + 12) as *const u8, length)
        };
        modules.push(LoadedModule {
            path: String::from_utf8_lossy(path).into_owned(),
            base: base as u64,
            size: size as u64,
        });
        cursor += 12 + length as u32;
    }

    modules
}

fn read_word(address: u32) -> u32 {
    unsafe { (address as *const u32).read() }
}

pub struct LoadedModule {
    pub path: String,
    pub base: u64,
    pub size: u64,
}

pub fn spawn_process(command_line: &str) -> u32 {
    let create_process: unsafe extern "stdcall" fn(u32, *mut u8, u32, u32, u32, u32, u32, u32,
        *mut u8, *mut u32) -> u32 = unsafe {
        core::mem::transmute(user_api().create_process as usize)
    };

    let line = alloc(command_line.len() + 1);
    unsafe {
        core::ptr::copy_nonoverlapping(command_line.as_ptr(), line, command_line.len());
        line.add(command_line.len()).write(0);
    }

    let mut startup = [0u32; STARTUP_INFO_WORDS];
    startup[0] = (STARTUP_INFO_WORDS * 4) as u32;
    let mut created = [0u32; PROCESS_INFORMATION_WORDS];

    let ok = unsafe {
        create_process(0, line, 0, 0, 0, CREATE_SUSPENDED, 0, 0, startup.as_mut_ptr() as *mut u8,
            created.as_mut_ptr())
    };
    free(line, command_line.len() + 1);
    if ok == 0 {
        return 0;
    }

    let process = created[PROCESS_INFORMATION_PROCESS];
    let thread = created[PROCESS_INFORMATION_THREAD];
    let pid = created[PROCESS_INFORMATION_PID];
    if hold_at_entry_point(pid, thread).is_none() {
        return 0;
    }

    unsafe { held().insert(pid, HeldProcess { process, thread }) };

    pid
}

fn gate_spawns(on: bool) {
    let create_process = user_api().create_process as *mut c_void;
    if create_process.is_null() {
        return;
    }

    unsafe {
        let interceptor = crate::bindings::gum_interceptor_obtain();
        crate::bindings::gum_interceptor_begin_transaction(interceptor);
        if on {
            crate::bindings::gum_interceptor_replace(interceptor, create_process,
                on_create_process as *mut c_void, &raw mut ORIGINAL_CREATE_PROCESS,
                core::ptr::null());
        } else {
            crate::bindings::gum_interceptor_revert(interceptor, create_process);
        }
        crate::bindings::gum_interceptor_end_transaction(interceptor);
    }
}

unsafe extern "stdcall" fn on_create_process(application: u32, command_line: *const u8,
        process_attributes: u32, thread_attributes: u32, inherit: u32, flags: u32,
        environment: u32, directory: u32, startup: *mut u8, information: *mut u32) -> u32 {
    let original: unsafe extern "stdcall" fn(u32, *const u8, u32, u32, u32, u32, u32, u32,
        *mut u8, *mut u32) -> u32 = unsafe {
        core::mem::transmute((&raw const ORIGINAL_CREATE_PROCESS).read())
    };

    let ok = unsafe {
        original(application, command_line, process_attributes, thread_attributes, inherit,
            flags | CREATE_SUSPENDED, environment, directory, startup, information)
    };
    if ok == 0 {
        return ok;
    }

    let process = unsafe { information.add(PROCESS_INFORMATION_PROCESS).read() };
    let thread = unsafe { information.add(PROCESS_INFORMATION_THREAD).read() };
    let pid = unsafe { information.add(PROCESS_INFORMATION_PID).read() };

    let held_ok = hold_at_entry_point(pid, thread).is_some();
    unsafe {
        let arena = crate::routed_arena() as u32;
        ((arena + HOLD_RESULT) as *mut u32).write_volatile(if held_ok { 1 } else { 2 });
    }
    unsafe { held().insert(pid, HeldProcess { process, thread }) };

    crate::tell_the_host_of_a_spawn(pid, command_line);

    ok
}

static mut ORIGINAL_CREATE_PROCESS: *mut c_void = core::ptr::null_mut();
static mut GATING_HERE: bool = false;

pub fn enumerate_shortcuts(found: &mut dyn FnMut(&str, &str, &str, &str)) {
    crate::start_menu::enumerate(&files(), b"C:\\WINDOWS\\Start Menu", found);
}

fn files() -> crate::start_menu::Api {
    let api = user_api();

    crate::start_menu::Api {
        find_first: api.find_first_file as usize,
        find_next: api.find_next_file as usize,
        find_close: api.find_close as usize,
        create_file: api.create_file as usize,
        read_file: api.read_file as usize,
        set_file_pointer: api.set_file_pointer as usize,
        close_handle: api.close_handle as usize,
    }
}

fn hold_at_entry_point(pid: u32, thread: u32) -> Option<()> {
    let (entry_point, prologue) = patch(pid, 0, HOLD_INSTRUCTION)?;
    if entry_point == NOTHING_TO_HOLD {
        return Some(());
    }
    if entry_point == 0 {
        return None;
    }

    resume_thread(thread);
    let arrived = wait_for_entry_point(thread, entry_point);

    patch(pid, entry_point, prologue)?;

    arrived
}

pub fn take_writes_on(first_page: *mut u8, pages: u32) -> *mut u8 {
    let span = pages as usize * PAGE_SIZE as usize;
    if !in_shared_library(first_page as u32) {
        protect(first_page as u64, span, GUM_PAGE_READ | GUM_PAGE_WRITE | GUM_PAGE_EXECUTE);
        return first_page;
    }

    let scratch = alloc_code(span);
    if scratch.is_null() {
        return scratch;
    }
    unsafe { core::ptr::copy_nonoverlapping(first_page, scratch, span) };

    let mut before = Vec::with_capacity(span);
    before.extend_from_slice(unsafe { core::slice::from_raw_parts(first_page, span) });
    unsafe { (&raw mut REMAPPED).as_mut().unwrap() }.push(Remapped {
        scratch: scratch as u32,
        first_page: first_page as u32,
        before,
    });

    scratch
}

pub fn make_the_writes(writable: *mut u8, pages: u32) {
    let remapped = unsafe { (&raw mut REMAPPED).as_mut().unwrap() };
    let Some(index) = remapped.iter().position(|known| known.scratch == writable as u32) else {
        return;
    };
    let taken = remapped.remove(index);

    let span = pages as usize * PAGE_SIZE as usize;
    let written = unsafe { core::slice::from_raw_parts(taken.scratch as *const u8, span) };

    let mut offset = 0;
    while offset != span {
        if written[offset] == taken.before[offset] {
            offset += 1;
            continue;
        }

        let start = offset;
        while offset != span && written[offset] != taken.before[offset] {
            offset += 1;
        }

        ask_for_a_guard(taken.first_page + start as u32, &written[start..offset]);
    }

    free_code(taken.scratch as *mut u8, span);
}

struct Remapped {
    scratch: u32,
    first_page: u32,
    before: Vec<u8>,
}

static mut REMAPPED: Vec<Remapped> = Vec::new();

fn in_shared_library(address: u32) -> bool {
    let address = address as u64;

    enumerate_modules().iter().any(|module| {
        module.base >= SHARED_ARENA_BASE
            && address >= module.base
            && address < module.base + module.size
    })
}

fn ask_for_a_guard(address: u32, bytes: &[u8]) {
    let sleep: unsafe extern "stdcall" fn(u32) =
        unsafe { core::mem::transmute(user_api().sleep as usize) };
    let arena = crate::routed_arena() as u32;

    if bytes.len() > PATCH_CODE_MAX {
        return;
    }

    take_the_slot();

    unsafe {
        core::ptr::copy_nonoverlapping(bytes.as_ptr(), (arena + PATCH_CODE) as *mut u8,
            bytes.len());
        ((arena + PATCH_ADDRESS) as *mut u32).write_volatile(address);
        ((arena + PATCH_LENGTH) as *mut u32).write_volatile(bytes.len() as u32);
        ((arena + PATCH_REQUEST) as *mut u32).write_volatile(HOOK_ASKED);
    }

    wait_for_the_answer(arena);
    unsafe { ((arena + PATCH_REQUEST) as *mut u32).write_volatile(0) };
    give_the_slot_back();
}

fn wait_for_the_answer(arena: u32) {
    unsafe {
        ((arena + PATCH_THREAD) as *mut u32)
            .write_volatile(crate::win9x::get_cur_thread_handle())
    };
    crate::win9x::signal_kernel_half(arena);

    let answered = |_: ()| unsafe {
        ((arena + PATCH_REQUEST) as *const u32).read_volatile() == PATCH_ANSWERED
    };
    while !answered(()) {
        crate::kernel::wait(crate::glib::wakeup_token(), None, &mut || answered(()));
    }
}

const SHARED_ARENA_BASE: u64 = 0x8000_0000;

fn take_the_slot() {
    let sleep: unsafe extern "stdcall" fn(u32) =
        unsafe { core::mem::transmute(user_api().sleep as usize) };

    while ASKING.compare_exchange(false, true, Ordering::AcqRel, Ordering::Acquire).is_err() {
        unsafe { sleep(1) };
    }
}

fn give_the_slot_back() {
    ASKING.store(false, Ordering::Release);
}

static ASKING: AtomicBool = AtomicBool::new(false);

fn patch(pid: u32, address: u32, code: u32) -> Option<(u32, u32)> {
    let sleep: unsafe extern "stdcall" fn(u32) =
        unsafe { core::mem::transmute(user_api().sleep as usize) };
    let arena = crate::routed_arena() as u32;

    take_the_slot();

    unsafe {
        ((arena + PATCH_PROCESS) as *mut u32).write_volatile(pid);
        ((arena + PATCH_ADDRESS) as *mut u32).write_volatile(address);
        ((arena + PATCH_BYTES) as *mut u32).write_volatile(code);
        ((arena + PATCH_REQUEST) as *mut u32).write_volatile(PATCH_ASKED);
    }

    wait_for_the_answer(arena);

    let at = unsafe { ((arena + PATCH_ADDRESS) as *const u32).read_volatile() };
    let previous = unsafe { ((arena + PATCH_BYTES) as *const u32).read_volatile() };
    unsafe { ((arena + PATCH_REQUEST) as *mut u32).write_volatile(0) };
    give_the_slot_back();

    Some((at, previous))
}

fn wait_for_entry_point(thread: u32, entry_point: u32) -> Option<()> {
    let sleep: unsafe extern "stdcall" fn(u32) =
        unsafe { core::mem::transmute(user_api().sleep as usize) };

    for _ in 0..HOLD_ATTEMPTS {
        if register_of(thread, CONTEXT_CONTROL, CONTEXT_EIP) == Some(entry_point) {
            suspend_thread(thread);
            if register_of(thread, CONTEXT_CONTROL, CONTEXT_EIP) == Some(entry_point) {
                return Some(());
            }
            resume_thread(thread);
        }

        unsafe { sleep(HOLD_SLICE_MS) };
    }

    None
}

fn register_of(thread: u32, flags: u32, offset: usize) -> Option<u32> {
    let get_thread_context: unsafe extern "stdcall" fn(u32, *mut u8) -> u32 =
        unsafe { core::mem::transmute(user_api().get_thread_context as usize) };

    let mut context = [0u32; CONTEXT_WORDS];
    context[0] = flags;
    let asked = unsafe { get_thread_context(thread, context.as_mut_ptr() as *mut u8) };

    (asked != 0).then(|| context[offset / 4])
}

fn suspend_thread(thread: u32) {
    let suspend: unsafe extern "stdcall" fn(u32) -> u32 =
        unsafe { core::mem::transmute(user_api().suspend_thread as usize) };

    unsafe { suspend(thread) };
}

fn resume_thread(thread: u32) {
    let resume: unsafe extern "stdcall" fn(u32) -> u32 =
        unsafe { core::mem::transmute(user_api().resume_thread as usize) };

    unsafe { resume(thread) };
}

pub fn resume_process(pid: u32) -> bool {
    let Some(held) = (unsafe { held().remove(&pid) }) else {
        return false;
    };

    let close_handle: unsafe extern "stdcall" fn(u32) -> u32 =
        unsafe { core::mem::transmute(user_api().close_handle as usize) };

    resume_thread(held.thread);
    unsafe {
        close_handle(held.thread);
        close_handle(held.process);
    }

    true
}

fn held() -> &'static mut BTreeMap<u32, HeldProcess> {
    unsafe { (&raw mut HELD_PROCESSES).as_mut().unwrap() }
}

struct HeldProcess {
    process: u32,
    thread: u32,
}

static mut HELD_PROCESSES: BTreeMap<u32, HeldProcess> = BTreeMap::new();

const CREATE_SUSPENDED: u32 = 0x4;
const CONTEXT_WORDS: usize = 179;
const CONTEXT_CONTROL: u32 = 0x0001_0001;
const CONTEXT_INTEGER: u32 = 0x0001_0002;
const CONTEXT_EAX: usize = 0xb0;
const CONTEXT_EIP: usize = 0xb8;
const CONTEXT_ESP: usize = 0xc4;
const HOLD_INSTRUCTION: u32 = 0xfeeb;
const PATCH_ATTEMPTS: u32 = 5000;
const HOLD_ATTEMPTS: u32 = 5000;
const HOLD_SLICE_MS: u32 = 1;
const STARTUP_INFO_WORDS: usize = 17;
const PROCESS_INFORMATION_WORDS: usize = 4;
const PROCESS_INFORMATION_PROCESS: usize = 0;
const PROCESS_INFORMATION_THREAD: usize = 1;
const PROCESS_INFORMATION_PID: usize = 2;

pub fn alloc(size: usize) -> *mut u8 {
    let get_process_heap: unsafe extern "stdcall" fn() -> u32 =
        unsafe { core::mem::transmute(user_api().get_process_heap as usize) };
    let heap_alloc: unsafe extern "stdcall" fn(u32, u32, u32) -> *mut u8 =
        unsafe { core::mem::transmute(user_api().heap_alloc as usize) };
    unsafe { heap_alloc(get_process_heap(), 0, size as u32) }
}

pub fn free(ptr: *mut u8, _size: usize) {
    let get_process_heap: unsafe extern "stdcall" fn() -> u32 =
        unsafe { core::mem::transmute(user_api().get_process_heap as usize) };
    let heap_free: unsafe extern "stdcall" fn(u32, u32, *mut u8) -> u32 =
        unsafe { core::mem::transmute(user_api().heap_free as usize) };
    unsafe { heap_free(get_process_heap(), 0, ptr) };
}

pub fn alloc_code(size: usize) -> *mut u8 {
    let virtual_alloc: unsafe extern "stdcall" fn(u32, u32, u32, u32) -> *mut u8 =
        unsafe { core::mem::transmute(user_api().virtual_alloc as usize) };
    return unsafe {
        virtual_alloc(0, size as u32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE)
    };
}

pub fn free_code(ptr: *mut u8, _size: usize) {
    let virtual_free: unsafe extern "stdcall" fn(*mut u8, u32, u32) -> u32 =
        unsafe { core::mem::transmute(user_api().virtual_free as usize) };
    unsafe { virtual_free(ptr, 0, MEM_RELEASE) };
}

pub fn spawn_thread(entry: ThreadEntry, parameter: *mut c_void) -> isize {
    let create_thread: unsafe extern "stdcall" fn(u32, u32, u32, *mut c_void, u32, *mut u32) -> u32 =
        unsafe { core::mem::transmute(user_api().create_thread as usize) };

    let start = alloc(core::mem::size_of::<UserThreadStart>()) as *mut UserThreadStart;
    unsafe { start.write(UserThreadStart { entry, parameter }) };

    let thread = unsafe {
        create_thread(0, USER_THREAD_STACK_SIZE, frida_win9x_user_thread as usize as u32,
            start as *mut c_void, 0, core::ptr::null_mut())
    };

    let close_handle: unsafe extern "stdcall" fn(u32) -> u32 =
        unsafe { core::mem::transmute(user_api().close_handle as usize) };
    unsafe { close_handle(thread) };

    thread as isize
}

pub fn wait(token: *const u8, timeout_us: Option<u64>, check: &mut dyn FnMut() -> bool) {
    let slot = slot_for_token(token);
    EVENT_SLEEPERS[slot].fetch_add(1, Ordering::AcqRel);
    let event = event_in(slot);
    if check() {
        release_slot(slot, token);
        return;
    }

    let wait_for_single_object_ex: unsafe extern "stdcall" fn(u32, u32, u32) -> u32 =
        unsafe { core::mem::transmute(user_api().wait_for_single_object_ex as usize) };
    let timeout = match timeout_us {
        None => INFINITE,
        Some(us) => (us / 1000).max(1) as u32,
    };
    unsafe { wait_for_single_object_ex(event, timeout, 1) };
    release_slot(slot, token);
}

// A token names one wait of one thread, and the next wait has a token of its own. Thus a slot
// belongs to a token only while somebody waits on it: a table that never let go filled after a
// few dozen waits, and every token after that shared the first slot, where a wake meant for one
// thread released another.
fn release_slot(slot: usize, token: *const u8) {
    if EVENT_SLEEPERS[slot].fetch_sub(1, Ordering::AcqRel) != 1 {
        return;
    }
    if core::ptr::eq(token, crate::glib::wakeup_token()) {
        return;
    }

    let _ = EVENT_OWNERS[slot].compare_exchange(token as usize, 0, Ordering::AcqRel,
        Ordering::Acquire);
}

pub fn wake(token: *const u8) {
    let Some(slot) = slot_owned_by(token) else {
        return;
    };

    let set_event: unsafe extern "stdcall" fn(u32) -> u32 =
        unsafe { core::mem::transmute(user_api().set_event as usize) };

    let event = event_in(slot);

    let mut sleepers = EVENT_SLEEPERS[slot].load(Ordering::Acquire).max(1);
    while sleepers != 0 {
        unsafe { set_event(event) };
        sleepers -= 1;
    }
}

pub fn yield_now() {
    let sleep: unsafe extern "stdcall" fn(u32) =
        unsafe { core::mem::transmute(user_api().sleep as usize) };
    unsafe { sleep(10) };
}

pub fn monotonic_micros() -> i64 {
    let get_tick_count: unsafe extern "stdcall" fn() -> u32 =
        unsafe { core::mem::transmute(user_api().get_tick_count as usize) };
    (unsafe { get_tick_count() }) as i64 * 1000
}

pub fn current_process_id() -> u32 {
    let get_current_process_id: unsafe extern "stdcall" fn() -> u32 =
        unsafe { core::mem::transmute(user_api().get_current_process_id as usize) };
    unsafe { get_current_process_id() }
}

pub fn current_thread_id() -> u64 {
    let get_current_thread_id: unsafe extern "stdcall" fn() -> u32 =
        unsafe { core::mem::transmute(user_api().get_current_thread_id as usize) };
    (unsafe { get_current_thread_id() }) as u64
}

pub fn protect(address: u64, size: usize, gum_prot: u32) -> bool {
    let virtual_protect: unsafe extern "stdcall" fn(u32, u32, u32, *mut u32) -> u32 =
        unsafe { core::mem::transmute(user_api().virtual_protect as usize) };
    let wanted = if (gum_prot & GUM_PAGE_WRITE) != 0 {
        PAGE_EXECUTE_READWRITE
    } else {
        PAGE_EXECUTE_READ
    };
    let mut previous = 0;
    return unsafe {
        virtual_protect(address as u32, size as u32, wanted, &mut previous) != 0
    };
}
