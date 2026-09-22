// The half of the NT agent that runs in ring 3, inside a process the host attached to. It reaches
// nothing of the kernel's, thus it has primitives of its own, which frida_winnt_user_main selects
// before it does anything else.

use alloc::collections::BTreeMap;
use alloc::string::String;
use alloc::vec::Vec;
use core::ffi::c_void;
use core::sync::atomic::{AtomicUsize, Ordering};

use crate::winnt::{
    BLOCK_PROCESS_ID, BLOCK_THREAD_ID, CURRENT_PROCESS, MEM_COMMIT, MEM_RELEASE, MEM_RESERVE,
    COPY_LEFT, OBSERVED_PID, OBSERVED_THREAD, PAGE_EXECUTE_READWRITE, PAGE_READWRITE, Primitives,
    REGISTER_PROCESS, REGISTER_THREAD, STOP_REQUEST,
    BOOTSTRAP_CLIENT_ID, BOOTSTRAP_CONTEXT, BOOTSTRAP_CREATE_THREAD, BOOTSTRAP_HANDLE,
    BOOTSTRAP_INITIAL_TEB, BOOTSTRAP_TERMINATE_THREAD, TARGET_WAKE_HANDLE,
    USER_SHARED_DATA, AGENT_WAKE_HANDLE, ENTRY_DLL_BASE_OFFSET, ENTRY_FULL_NAME_OFFSET,
    ENTRY_SIZE_OF_IMAGE_OFFSET, LDR_IN_LOAD_ORDER_OFFSET, LOADER_LIBRARY, PEB_LDR_OFFSET,
    CONTEXT_ALIGNMENT, CONTEXT_CONTROL, CONTEXT_FLAGS, CONTEXT_FULL, CONTEXT_PC, CONTEXT_SIZE,
    PEB_PARAMETERS_OFFSET,
    POINTER_SIZE, export, module_base, read_pointer, read_u32,
    select_user, take_frame_from_host, windows_fn,
};

// No code in this image calls the ring 3 entry point, thus tell the linker to keep it.
#[used]
static USER_ENTRY: extern "C" fn(usize) = frida_winnt_user_main;

// This is the half that runs in a process. It has its own copy of the image, thus its data is
// its own. The ring selects which primitives it uses.
// The thread that descends has no block of its own, thus it can only make syscalls: ntdll's
// stubs need none. It asks for a real thread, which the kernel gives a block, and then retires.
#[used]
static BOOTSTRAP_ENTRY: extern "C" fn(usize) -> ! = frida_winnt_user_bootstrap;

#[unsafe(no_mangle)]
pub extern "C" fn frida_winnt_user_bootstrap(arena: usize) -> ! {
    unsafe {
        let create: windows_fn!(
            *mut *mut c_void, u32, *mut c_void, *mut c_void, *mut c_void, *mut u8, *mut c_void,
            u8, => i32) = core::mem::transmute(read_pointer(arena + BOOTSTRAP_CREATE_THREAD as usize));
        let terminate: windows_fn!(*mut c_void, i32 => i32) =
            core::mem::transmute(read_pointer(arena + BOOTSTRAP_TERMINATE_THREAD as usize));

        let status = create(
            (arena + BOOTSTRAP_HANDLE as usize) as *mut *mut c_void,
            THREAD_ALL_ACCESS,
            core::ptr::null_mut(),
            CURRENT_PROCESS,
            (arena + BOOTSTRAP_CLIENT_ID as usize) as *mut c_void,
            read_pointer(arena + BOOTSTRAP_CONTEXT as usize) as *mut u8,
            read_pointer(arena + BOOTSTRAP_INITIAL_TEB as usize) as *mut c_void,
            0,
        );

        let _ = status;

        terminate(CURRENT_THREAD, 0);
    }

    loop {}
}

const THREAD_ALL_ACCESS: u32 = 0x1f_03ff;
const CURRENT_THREAD: *mut c_void = -2isize as *mut c_void;

#[unsafe(no_mangle)]
pub extern "C" fn frida_winnt_user_main(arena: usize) {
    select_user();

    unsafe {
        ARENA = arena as u64;
    }
    resolve_user_api();

    unsafe {
        crate::set_own_range(
            ((arena + crate::winnt::IMAGE_BASE as usize) as *const u64).read_volatile(),
            ((arena + crate::winnt::IMAGE_SIZE as usize) as *const u64).read_volatile())
    };

    crate::winnt::be_told_page_size(
        unsafe { ((arena + crate::winnt::PAGE_SIZE_IN_USE as usize) as *const u64).read_volatile() }
            as usize);

    crate::gthread::use_thread_slots(&SLOTS);

    unsafe { crate::run_constructors() };
    unsafe { crate::init_gum_without_exceptor() };
    hear_about_faults();
    let context = unsafe { crate::adopt_js_context() };

    unsafe {
        ((arena + OBSERVED_PID as usize) as *mut u32).write_volatile(current_client_id(BLOCK_PROCESS_ID));
        ((arena + OBSERVED_THREAD as usize) as *mut u32)
            .write_volatile(current_client_id(BLOCK_THREAD_ID));

        crate::route_frames_through(arena as u64);
    }

    crate::gum_windows::watch_the_loader();

    crate::glib::own_the_loop();
    crate::watch_for_work(context, copy_has_work, serve_the_copy);
    crate::watch_a_descriptor(context, target_wake_handle() as isize as i32, poll_handles);

    while unsafe { ((arena + STOP_REQUEST as usize) as *const u32).read_volatile() } == 0 {
        unsafe { crate::dispatch_pending_work(context) };
    }

    crate::gum_windows::forget_the_loader();
    stop_hearing_about_faults();

    unsafe { ((arena + COPY_LEFT as usize) as *mut u32).write_volatile(1) };

    unsafe { (user_api().exit_thread)(0) };
}

// The group that the ring 3 half runs on. The order is the order in Primitives.
pub static USER: Primitives = Primitives {
    log,
    alloc,
    free,
    alloc_code,
    free_code,
    protect,
    protection_at,
    enumerate_ranges,
    enumerate_threads,
    find_thread,
    modify_thread,
    wait,
    wake,
    poke_loop_wakeup,
    yield_now,
    current_process_id,
    current_thread_id,
    shared_data,
};

pub(crate) fn flush_code(start: *const u8, end: *const u8) -> bool {
    let Some(api) = (unsafe { (*core::ptr::addr_of!(USER_API)).as_ref() }) else {
        return false;
    };

    unsafe { (api.flush_code)(CURRENT_PROCESS, start as *mut u8, end as usize - start as usize) };

    true
}

pub(crate) fn yield_entry_point() -> usize {
    unsafe { user_api().yield_execution as usize }
}

fn resolve_user_api() {
    let ntdll = loader_library();

    unsafe {
        USER_API = Some(UserApi {
            allocate_heap: core::mem::transmute(export(ntdll, b"RtlAllocateHeap")),
            free_heap: core::mem::transmute(export(ntdll, b"RtlFreeHeap")),
            allocate_memory: core::mem::transmute(export(ntdll, b"NtAllocateVirtualMemory")),
            free_memory: core::mem::transmute(export(ntdll, b"NtFreeVirtualMemory")),
            protect_memory: core::mem::transmute(export(ntdll, b"NtProtectVirtualMemory")),
            query_memory: core::mem::transmute(export(ntdll, b"NtQueryVirtualMemory")),
            yield_execution: core::mem::transmute(export(ntdll, b"NtYieldExecution")),
            wait_for_object: core::mem::transmute(export(ntdll, b"NtWaitForSingleObject")),
            wait_for_objects: core::mem::transmute(export(ntdll, b"NtWaitForMultipleObjects")),
            set_event: core::mem::transmute(export(ntdll, b"NtSetEvent")),
            reset_event: core::mem::transmute(export(ntdll, b"NtResetEvent")),
            create_event: core::mem::transmute(export(ntdll, b"NtCreateEvent")),
            close: core::mem::transmute(export(ntdll, b"NtClose")),
            exit_thread: core::mem::transmute(export(ntdll, b"RtlExitUserThread")),
            flush_code: core::mem::transmute(export(ntdll, b"NtFlushInstructionCache")),
            create_heap: core::mem::transmute(export(ntdll, b"RtlCreateHeap")),
            add_fault_handler: core::mem::transmute(
                export(ntdll, b"RtlAddVectoredExceptionHandler")),
            remove_fault_handler: core::mem::transmute(
                export(ntdll, b"RtlRemoveVectoredExceptionHandler")),
        });

        SPAWN_API = Some(SpawnApi {
            query_process: core::mem::transmute(export(ntdll, b"NtQueryInformationProcess")),
            read_memory: core::mem::transmute(export(ntdll, b"NtReadVirtualMemory")),
            write_memory: core::mem::transmute(export(ntdll, b"NtWriteVirtualMemory")),
            protect_memory: core::mem::transmute(export(ntdll, b"NtProtectVirtualMemory")),
            suspend_thread: core::mem::transmute(export(ntdll, b"NtSuspendThread")),
            delay: core::mem::transmute(export(ntdll, b"NtDelayExecution")),
            resume_thread: core::mem::transmute(export(ntdll, b"NtResumeThread")),
            get_context: core::mem::transmute(export(ntdll, b"NtGetContextThread")),
            close: core::mem::transmute(export(ntdll, b"NtClose")),
        });
    }
}

pub fn serve_the_session_server(arena: usize) {
    let waiting = unsafe { ((arena + REGISTER_THREAD as usize) as *const u32).read_volatile() };
    if waiting == 0 {
        return;
    }
    let process = unsafe { ((arena + REGISTER_PROCESS as usize) as *const u32).read_volatile() };
    unsafe { ((arena + REGISTER_THREAD as usize) as *mut u32).write_volatile(0) };

    let Some(server) = session_server_routines() else {
        return;
    };

    let mut record: *mut c_void = core::ptr::null_mut();
    if unsafe { (server.lock)(process as usize as *mut c_void, &mut record) } < 0 {
        return;
    }

    let handle = unsafe { (server.open_thread)(THREAD_ALL_ACCESS, 0, waiting) };
    let id = [process as usize, waiting as usize];
    unsafe { (server.make_record)(record, handle, id.as_ptr() as *mut c_void, 0) };

    unsafe { (server.unlock)(record) };
}

fn session_server_routines() -> Option<SessionServer> {
    let own = peb();
    let library = module_base(own, b"csrsrv.dll");
    if library == 0 {
        return None;
    }

    Some(SessionServer {
        lock: unsafe { core::mem::transmute(export(library, b"CsrLockProcessByClientId")) },
        unlock: unsafe { core::mem::transmute(export(library, b"CsrUnlockProcess")) },
        make_record: unsafe { core::mem::transmute(export(library, b"CsrCreateThread")) },
        open_thread: unsafe {
            core::mem::transmute(export(module_base(own, b"kernel32.dll"), b"OpenThread"))
        },
    })
}

struct SessionServer {
    lock: windows_fn!(*mut c_void, *mut *mut c_void => i32),
    unlock: windows_fn!(*mut c_void => i32),
    make_record: windows_fn!(*mut c_void, *mut c_void, *mut c_void, i32 => i32),
    open_thread: windows_fn!(u32, i32, u32 => *mut c_void),
}

// The loader has one way in and one way out for a library, thus a stand-in on each says what
// the process has mapped now.
pub fn loader_entry_points() -> Option<LoaderEntryPoints> {
    let ntdll = loader_library();

    Some(LoaderEntryPoints {
        load: export(ntdll, b"LdrLoadDll"),
        load_with_flags: 0,
        unload: export(ntdll, b"LdrUnloadDll"),
    })
}

pub fn enumerate_shortcuts(found: &mut dyn FnMut(&str, &str, &str, &str)) {
    let api = menu_api();
    let files = crate::start_menu::Api {
        find_first: api.find_first as usize,
        find_next: api.find_next as usize,
        find_close: api.find_close as usize,
        create_file: api.create_file as usize,
        read_file: api.read_file as usize,
        set_file_pointer: api.set_file_pointer as usize,
        close_handle: api.close as usize,
    };

    for profile in [b"ALLUSERSPROFILE".as_slice(), b"USERPROFILE".as_slice()] {
        let mut menu = alloc::vec::Vec::new();
        menu.extend_from_slice(&folder_named(profile));
        if menu.is_empty() {
            continue;
        }
        menu.extend_from_slice(b"\\Start Menu");

        crate::start_menu::enumerate(&files, &menu, found);
    }
}

fn folder_named(name: &[u8]) -> alloc::vec::Vec<u8> {
    let api = menu_api();

    let mut asciiz = alloc::vec::Vec::new();
    asciiz.extend_from_slice(name);
    asciiz.push(0);

    let mut folder = [0u8; MAX_FOLDER];
    let length = unsafe {
        (api.environment_variable)(asciiz.as_ptr(), folder.as_mut_ptr(), folder.len() as u32)
    } as usize;
    if length == 0 || length >= folder.len() {
        return alloc::vec::Vec::new();
    }

    folder[..length].to_vec()
}

fn menu_api() -> &'static MenuApi {
    unsafe {
        if (*core::ptr::addr_of!(MENU_API)).is_none() {
            let library = module_base(peb(), b"kernel32.dll");
            MENU_API = Some(MenuApi {
                find_first: core::mem::transmute(export(library, b"FindFirstFileA")),
                find_next: core::mem::transmute(export(library, b"FindNextFileA")),
                find_close: core::mem::transmute(export(library, b"FindClose")),
                create_file: core::mem::transmute(export(library, b"CreateFileA")),
                read_file: core::mem::transmute(export(library, b"ReadFile")),
                set_file_pointer: core::mem::transmute(export(library, b"SetFilePointer")),
                environment_variable: core::mem::transmute(
                    export(library, b"GetEnvironmentVariableA")),
                close: core::mem::transmute(export(library, b"CloseHandle")),
            });
        }
        (*core::ptr::addr_of!(MENU_API)).as_ref().unwrap()
    }
}

struct MenuApi {
    find_first: windows_fn!(*const u8, *mut u8 => u32),
    find_next: windows_fn!(u32, *mut u8 => u32),
    find_close: windows_fn!(u32 => u32),
    create_file: windows_fn!(*const u8, u32, u32, u32, u32, u32, u32 => u32),
    read_file: windows_fn!(u32, *mut u8, u32, *mut u32, u32 => u32),
    set_file_pointer: windows_fn!(u32, u32, u32, u32 => u32),
    environment_variable: windows_fn!(*const u8, *mut u8, u32 => u32),
    close: windows_fn!(u32 => u32),
}

static mut MENU_API: Option<MenuApi> = None;

const MAX_FOLDER: usize = 260;

pub fn thread_entry_points() -> Option<ThreadEntryPoints> {
    Some(ThreadEntryPoints {
        start: thread_starter()?,
        exit: export(module_base(peb(), b"kernel32.dll"), b"ExitThread"),
    })
}

pub struct ThreadEntryPoints {
    pub start: usize,
    pub exit: usize,
}

pub fn thread_start_slot() -> *mut *mut c_void {
    &raw mut THREAD_START as *mut *mut c_void
}

pub fn thread_exit_slot() -> *mut *mut c_void {
    &raw mut THREAD_EXIT as *mut *mut c_void
}

static mut THREAD_START: *mut c_void = core::ptr::null_mut();
static mut THREAD_EXIT: *mut c_void = core::ptr::null_mut();

#[cfg(target_arch = "x86")]
pub unsafe extern "stdcall" fn on_thread_start(routine: usize, parameter: usize) -> ! {
    unsafe { start_thread(routine, parameter) }
}

#[cfg(target_arch = "x86_64")]
pub unsafe extern "win64" fn on_thread_start(routine: usize, parameter: usize) -> ! {
    unsafe { start_thread(routine, parameter) }
}

#[cfg(target_arch = "aarch64")]
pub unsafe extern "C" fn on_thread_start(routine: usize, parameter: usize) -> ! {
    unsafe { start_thread(routine, parameter) }
}

unsafe fn start_thread(routine: usize, parameter: usize) -> ! {
    crate::gum_windows::thread_appeared_at(current_thread_id() as u32, routine, parameter);

    let original: windows_fn!(usize, usize => !) =
        unsafe { core::mem::transmute(THREAD_START) };
    unsafe { original(routine, parameter) }
}

#[cfg(target_arch = "x86")]
pub unsafe extern "stdcall" fn on_thread_exit(status: u32) -> ! {
    unsafe { exit_thread(status) }
}

#[cfg(target_arch = "x86_64")]
pub unsafe extern "win64" fn on_thread_exit(status: u32) -> ! {
    unsafe { exit_thread(status) }
}

#[cfg(target_arch = "aarch64")]
pub unsafe extern "C" fn on_thread_exit(status: u32) -> ! {
    unsafe { exit_thread(status) }
}

unsafe fn exit_thread(status: u32) -> ! {
    crate::gum_windows::thread_vanished(current_thread_id() as u32);

    let original: windows_fn!(u32 => !) =
        unsafe { core::mem::transmute(THREAD_EXIT) };
    unsafe { original(status) }
}

// The thread starter of this system is not exported, thus it is found through the thunk that
// leads to it: xor ebp, ebp; push ebx; push eax; push 0; jmp <starter>.
#[cfg(target_arch = "x86")]
fn thread_starter() -> Option<usize> {
    let library = module_base(peb(), b"kernel32.dll");
    let text = unsafe {
        core::slice::from_raw_parts(library as *const u8, image_size(library))
    };

    let jump = text.windows(THREAD_THUNK.len())
        .position(|window| window == THREAD_THUNK)
        .map(|offset| library + offset + THREAD_THUNK.len() - 1)?;

    let displacement = unsafe { ((jump + 1) as *const i32).read_unaligned() };

    Some((jump as isize + 5 + displacement as isize) as usize)
}

const THREAD_THUNK: [u8; 7] = [0x33, 0xed, 0x53, 0x50, 0x6a, 0x00, 0xe9];

#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
fn thread_starter() -> Option<usize> {
    Some(export(module_base(peb(), b"kernel32.dll"), b"BaseThreadStart"))
}

fn image_size(base: usize) -> usize {
    unsafe {
        let headers = base + ((base + PE_SIGNATURE_OFFSET) as *const u32).read() as usize;
        ((headers + PE_IMAGE_SIZE_OFFSET) as *const u32).read() as usize
    }
}

const PE_IMAGE_SIZE_OFFSET: usize = 0x50;

pub struct LoaderEntryPoints {
    pub load: usize,
    pub load_with_flags: usize,
    pub unload: usize,
}

pub unsafe extern "C" fn on_module_load_with_flags() {}

#[cfg(target_arch = "x86")]
pub unsafe extern "stdcall" fn on_module_load(search_path: *const u16, flags: *mut u32,
        name: *mut u8, handle: *mut usize) -> i32 {
    unsafe { load_module(search_path, flags, name, handle) }
}

#[cfg(target_arch = "x86_64")]
pub unsafe extern "win64" fn on_module_load(search_path: *const u16, flags: *mut u32,
        name: *mut u8, handle: *mut usize) -> i32 {
    unsafe { load_module(search_path, flags, name, handle) }
}

#[cfg(target_arch = "aarch64")]
pub unsafe extern "C" fn on_module_load(search_path: *const u16, flags: *mut u32,
        name: *mut u8, handle: *mut usize) -> i32 {
    unsafe { load_module(search_path, flags, name, handle) }
}

unsafe fn load_module(search_path: *const u16, flags: *mut u32, name: *mut u8,
        handle: *mut usize) -> i32 {
    let original: windows_fn!(*const u16, *mut u32, *mut u8, *mut usize => i32) =
        unsafe { core::mem::transmute(crate::gum_windows::loader_load()) };

    let status = unsafe { original(search_path, flags, name, handle) };
    if status >= 0 {
        crate::gum_windows::module_arrived(unsafe { handle.read() } as u64);
    }

    status
}

#[cfg(target_arch = "x86")]
pub unsafe extern "stdcall" fn on_module_unload(handle: usize) -> i32 {
    unsafe { unload_module(handle) }
}

#[cfg(target_arch = "x86_64")]
pub unsafe extern "win64" fn on_module_unload(handle: usize) -> i32 {
    unsafe { unload_module(handle) }
}

#[cfg(target_arch = "aarch64")]
pub unsafe extern "C" fn on_module_unload(handle: usize) -> i32 {
    unsafe { unload_module(handle) }
}

unsafe fn unload_module(handle: usize) -> i32 {
    let original: windows_fn!(usize => i32) =
        unsafe { core::mem::transmute(crate::gum_windows::loader_unload()) };

    let status = unsafe { original(handle) };
    if status >= 0 {
        crate::gum_windows::module_left(handle as u64);
    }

    status
}

// The loader's list says everything about a module that just arrived.
pub fn describe_module(base: u64) -> Option<LoadedModule> {
    enumerate_modules().into_iter().find(|module| module.base == base)
}

// The loader keeps a list of what this process has mapped, and the copy can read it directly.
pub fn enumerate_modules() -> Vec<LoadedModule> {
    let mut modules = Vec::new();

    unsafe {
        let head = read_pointer(peb() + PEB_LDR_OFFSET) + LDR_IN_LOAD_ORDER_OFFSET;
        let mut entry = read_pointer(head);
        while entry != head {
            let base = read_pointer(entry + ENTRY_DLL_BASE_OFFSET);
            if base != 0 {
                modules.push(LoadedModule {
                    path: wide_text_at(entry + ENTRY_FULL_NAME_OFFSET),
                    base: base as u64,
                    size: read_u32(entry + ENTRY_SIZE_OF_IMAGE_OFFSET) as u64,
                });
            }
            entry = read_pointer(entry);
        }
    }

    modules
}

fn wide_text_at(record: usize) -> String {
    unsafe {
        let characters = (read_pointer(record + UNICODE_STRING_BUFFER) / 1) as *const u16;
        let length = (record as *const u16).read() as usize / 2;

        let mut text = String::new();
        for index in 0..length {
            text.push(char::from_u32(characters.add(index).read() as u32).unwrap_or('?'));
        }

        text
    }
}

pub struct LoadedModule {
    pub path: String,
    pub base: u64,
    pub size: u64,
}

// Making a process is the work of the loader's own entry points. They ask for wide strings and
// they leave the first thread of the new process held, which is what a caller wants.
pub fn spawn_process(command_line: &str) -> u32 {
    let Some(made) = make_process(command_line) else {
        return 0;
    };

    if hold_at_entry_point(made.process, made.thread).is_none() {
        return 0;
    }

    unsafe {
        held().insert(made.pid, HeldProcess {
            process: made.process,
            thread: made.thread,
        })
    };

    made.pid
}

fn hold_at_entry_point(process: *mut c_void, thread: *mut c_void) -> Option<()> {
    let api = spawn_api();
    let entry_point = entry_point_of(process)?;
    let mut prologue = [0u8; HOLD_SIZE];
    read_process_memory(process, entry_point, &mut prologue)?;
    write_process_memory(process, entry_point, &HOLD_INSTRUCTION)?;
    unsafe { (api.resume_thread)(thread, core::ptr::null_mut()) };
    let arrived = wait_for_entry_point(thread, entry_point);

    write_process_memory(process, entry_point, &prologue)?;

    arrived
}

fn wait_for_entry_point(thread: *mut c_void, entry_point: usize) -> Option<()> {
    let api = spawn_api();

    for _ in 0..HOLD_ATTEMPTS {
        if program_counter_of(thread) == Some(entry_point) {
            unsafe { (api.suspend_thread)(thread, core::ptr::null_mut()) };
            if program_counter_of(thread) == Some(entry_point) {
                return Some(());
            }
            unsafe { (api.resume_thread)(thread, core::ptr::null_mut()) };
        }

        let slice = -(HOLD_SLICE_US * 10);
        unsafe { (api.delay)(0, &slice) };
    }

    None
}

fn program_counter_of(thread: *mut c_void) -> Option<usize> {
    let api = spawn_api();

    let mut context = [0u8; CONTEXT_SIZE + CONTEXT_ALIGNMENT];
    let base = unsafe {
        context.as_mut_ptr().add(context.as_ptr().align_offset(CONTEXT_ALIGNMENT))
    };
    unsafe { (base.add(CONTEXT_FLAGS) as *mut u32).write(CONTEXT_CONTROL) };

    let asked = unsafe { (api.get_context)(thread, base) };

    (asked >= 0).then(|| unsafe { (base.add(CONTEXT_PC as usize) as *const usize).read() })
}

fn entry_point_of(process: *mut c_void) -> Option<usize> {
    let api = spawn_api();

    let mut information = [0u8; PROCESS_BASIC_INFORMATION_SIZE];
    let asked = unsafe {
        (api.query_process)(process, PROCESS_BASIC_INFORMATION, information.as_mut_ptr(),
            information.len() as u32, core::ptr::null_mut())
    };
    if asked < 0 {
        return None;
    }
    let peb = read_word(&information, PROCESS_BASIC_INFORMATION_PEB);

    let mut base = [0u8; POINTER_SIZE];
    read_process_memory(process, peb + PEB_IMAGE_BASE_OFFSET, &mut base)?;
    let image = read_word(&base, 0);

    let mut headers = [0u8; 4];
    read_process_memory(process, image + PE_SIGNATURE_OFFSET, &mut headers)?;
    let signature = image + u32::from_le_bytes(headers) as usize;

    let mut rva = [0u8; 4];
    read_process_memory(process, signature + PE_ENTRY_POINT_OFFSET, &mut rva)?;

    Some(image + u32::from_le_bytes(rva) as usize)
}

fn read_word(bytes: &[u8], offset: usize) -> usize {
    let mut word = [0u8; POINTER_SIZE];
    word.copy_from_slice(&bytes[offset..offset + POINTER_SIZE]);

    usize::from_le_bytes(word)
}

fn read_process_memory(process: *mut c_void, address: usize, into: &mut [u8]) -> Option<()> {
    let read = unsafe {
        (spawn_api().read_memory)(process, address, into.as_mut_ptr(), into.len(),
            core::ptr::null_mut())
    };

    (read >= 0).then_some(())
}

fn write_process_memory(process: *mut c_void, address: usize, from: &[u8]) -> Option<()> {
    let api = spawn_api();

    let mut base = address;
    let mut region = from.len();
    let mut previous = 0u32;
    let opened = unsafe {
        (api.protect_memory)(process, &mut base, &mut region, PAGE_EXECUTE_READWRITE,
            &mut previous)
    };
    if opened < 0 {
        return None;
    }

    let written = unsafe {
        (api.write_memory)(process, address, from.as_ptr(), from.len(), core::ptr::null_mut())
    };

    let mut base = address;
    let mut region = from.len();
    let mut ignored = 0u32;
    unsafe { (api.protect_memory)(process, &mut base, &mut region, previous, &mut ignored) };

    (written >= 0).then_some(())
}

fn make_process(command_line: &str) -> Option<MadeProcess> {
    let create: windows_fn!(*const u16, *mut u16, *mut c_void, *mut c_void, u32, u32, *mut c_void,
        *const u16, *mut u8, *mut u8 => u32) = unsafe {
        core::mem::transmute(export(module_base(peb(), b"kernel32.dll"), b"CreateProcessW"))
    };

    let mut line: Vec<u16> = command_line.encode_utf16().collect();
    line.push(0);

    let mut startup = [0u8; STARTUP_INFO_SIZE];
    unsafe { (startup.as_mut_ptr() as *mut u32).write(STARTUP_INFO_SIZE as u32) };

    let mut information = [0u8; PROCESS_INFORMATION_SIZE];
    let made = unsafe {
        create(core::ptr::null(), line.as_mut_ptr(), core::ptr::null_mut(),
            core::ptr::null_mut(), 0, CREATE_SUSPENDED, core::ptr::null_mut(),
            core::ptr::null(), startup.as_mut_ptr(), information.as_mut_ptr())
    };
    if made == 0 {
        return None;
    }

    Some(MadeProcess {
        process: read_handle(&information, PROCESS_INFORMATION_PROCESS),
        thread: read_handle(&information, PROCESS_INFORMATION_THREAD),
        pid: unsafe { read_u32(information.as_ptr() as usize + PROCESS_INFORMATION_PID) },
        tid: unsafe { read_u32(information.as_ptr() as usize + PROCESS_INFORMATION_TID) },
    })
}

struct MadeProcess {
    process: *mut c_void,
    thread: *mut c_void,
    pid: u32,
    tid: u32,
}

pub fn resume_process(pid: u32) -> bool {
    let Some(held) = (unsafe { held().remove(&pid) }) else {
        return false;
    };
    let api = spawn_api();

    unsafe {
        let resumed = (api.resume_thread)(held.thread, core::ptr::null_mut()) >= 0;
        (api.close)(held.thread);
        (api.close)(held.process);

        resumed
    }
}

fn read_handle(information: &[u8; PROCESS_INFORMATION_SIZE], offset: usize) -> *mut c_void {
    let mut bytes = [0u8; POINTER_SIZE];
    bytes.copy_from_slice(&information[offset..offset + POINTER_SIZE]);

    usize::from_le_bytes(bytes) as *mut c_void
}

fn held() -> &'static mut BTreeMap<u32, HeldProcess> {
    unsafe { (&raw mut HELD_PROCESSES).as_mut().unwrap() }
}

// A wide string and the record that names it travel together, the record pointing into it.
struct SpawnApi {
    query_process: windows_fn!(*mut c_void, u32, *mut u8, u32, *mut u32 => i32),
    read_memory: windows_fn!(*mut c_void, usize, *mut u8, usize, *mut usize => i32),
    write_memory: windows_fn!(*mut c_void, usize, *const u8, usize, *mut usize => i32),
    protect_memory: windows_fn!(*mut c_void, *mut usize, *mut usize, u32, *mut u32 => i32),
    suspend_thread: windows_fn!(*mut c_void, *mut u32 => i32),
    delay: windows_fn!(u8, *const i64 => i32),
    resume_thread: windows_fn!(*mut c_void, *mut u32 => i32),
    get_context: windows_fn!(*mut c_void, *mut u8 => i32),
    close: windows_fn!(*mut c_void => i32),
}

struct HeldProcess {
    process: *mut c_void,
    thread: *mut c_void,
}

static mut SPAWN_API: Option<SpawnApi> = None;
static mut HELD_PROCESSES: BTreeMap<u32, HeldProcess> = BTreeMap::new();

const CREATE_SUSPENDED: u32 = 0x4;
const PROCESS_BASIC_INFORMATION: u32 = 0;
const PE_SIGNATURE_OFFSET: usize = 0x3c;
const PE_ENTRY_POINT_OFFSET: usize = 0x28;
const HOLD_SIZE: usize = 2;
const HOLD_INSTRUCTION: [u8; HOLD_SIZE] = [0xeb, 0xfe];
const HOLD_ATTEMPTS: u32 = 5000;
const HOLD_SLICE_US: i64 = 1000;

#[cfg(target_pointer_width = "32")]
const PROCESS_BASIC_INFORMATION_SIZE: usize = 24;
#[cfg(target_pointer_width = "32")]
const PROCESS_BASIC_INFORMATION_PEB: usize = 0x04;
#[cfg(target_pointer_width = "32")]
const PEB_IMAGE_BASE_OFFSET: usize = 0x08;

#[cfg(target_pointer_width = "64")]
const PROCESS_BASIC_INFORMATION_SIZE: usize = 48;
#[cfg(target_pointer_width = "64")]
const PROCESS_BASIC_INFORMATION_PEB: usize = 0x08;
#[cfg(target_pointer_width = "64")]
const PEB_IMAGE_BASE_OFFSET: usize = 0x10;

#[cfg(target_arch = "x86")]
const UNICODE_STRING_SIZE: usize = 8;
#[cfg(target_pointer_width = "32")]
const UNICODE_STRING_BUFFER: usize = 4;
#[cfg(target_arch = "x86")]
const PARAMETERS_DESKTOP_OFFSET: usize = 0x78;
#[cfg(target_pointer_width = "32")]
const PROCESS_INFORMATION_SIZE: usize = 128;
#[cfg(target_pointer_width = "32")]
const PROCESS_INFORMATION_PROCESS: usize = 0x00;
#[cfg(target_pointer_width = "32")]
const PROCESS_INFORMATION_THREAD: usize = 0x04;
#[cfg(target_pointer_width = "32")]
const PROCESS_INFORMATION_PID: usize = 0x08;
#[cfg(target_pointer_width = "32")]
const PROCESS_INFORMATION_TID: usize = 0x0c;
#[cfg(target_pointer_width = "32")]
const STARTUP_INFO_SIZE: usize = 0x44;

#[cfg(target_arch = "x86_64")]
const UNICODE_STRING_SIZE: usize = 16;
#[cfg(target_pointer_width = "64")]
const UNICODE_STRING_BUFFER: usize = 8;
#[cfg(target_arch = "x86_64")]
const PARAMETERS_DESKTOP_OFFSET: usize = 0xc0;
#[cfg(target_pointer_width = "64")]
const PROCESS_INFORMATION_SIZE: usize = 192;
#[cfg(target_pointer_width = "64")]
const PROCESS_INFORMATION_PROCESS: usize = 0x00;
#[cfg(target_pointer_width = "64")]
const PROCESS_INFORMATION_THREAD: usize = 0x08;
#[cfg(target_pointer_width = "64")]
const PROCESS_INFORMATION_PID: usize = 0x10;
#[cfg(target_pointer_width = "64")]
const STARTUP_INFO_SIZE: usize = 0x68;
#[cfg(target_pointer_width = "64")]
const PROCESS_INFORMATION_TID: usize = 0x20;

fn spawn_api() -> &'static SpawnApi {
    unsafe { (*core::ptr::addr_of!(SPAWN_API)).as_ref().unwrap() }
}

fn user_api() -> &'static UserApi {
    unsafe { (*core::ptr::addr_of!(USER_API)).as_ref().unwrap() }
}

struct UserApi {
    allocate_heap: windows_fn!(usize, u32, usize => *mut u8),
    free_heap: windows_fn!(usize, u32, *mut u8 => u8),
    allocate_memory: windows_fn!(*mut c_void, *mut *mut u8, usize, *mut usize, u32, u32 => i32),
    free_memory: windows_fn!(*mut c_void, *mut *mut u8, *mut usize, u32 => i32),
    protect_memory: windows_fn!(*mut c_void, *mut *mut u8, *mut usize, u32, *mut u32 => i32),
    query_memory: windows_fn!(*mut c_void, *mut u8, u32, *mut u8, usize, *mut usize => i32),
    yield_execution: windows_fn!( => i32),
    wait_for_object: windows_fn!(*mut c_void, u8, *const i64 => i32),
    wait_for_objects: windows_fn!(u32, *const *mut c_void, u32, u8, *const i64 => i32),
    flush_code: windows_fn!(*mut c_void, *mut u8, usize => i32),
    set_event: windows_fn!(*mut c_void, *mut u32 => i32),
    reset_event: windows_fn!(*mut c_void, *mut u32 => i32),
    create_event: windows_fn!(*mut *mut c_void, u32, *mut c_void, u32, u8 => i32),
    close: windows_fn!(*mut c_void => i32),
    exit_thread: windows_fn!(u32 => !),
    add_fault_handler: windows_fn!(u32, *const c_void => *mut c_void),
    remove_fault_handler: windows_fn!(*mut c_void => u32),
    create_heap: windows_fn!(u32, *mut c_void, usize, usize, *mut c_void, *mut c_void => usize),
}

static mut USER_API: Option<UserApi> = None;

// A process that is still held has neither a list of its libraries nor a heap of its own: the
// loader makes both when it runs, and it has not run yet. The kernel half says where the loader
// library is, and a heap is ours to make.
fn loader_library() -> usize {
    let own = peb();
    if unsafe { read_pointer(own + PEB_LDR_OFFSET) } != 0 {
        return module_base(own, b"ntdll.dll");
    }

    unsafe { ((ARENA + LOADER_LIBRARY) as *const u64).read() as usize }
}

fn process_heap() -> usize {
    let own = unsafe { read_pointer(peb() + PEB_PROCESS_HEAP_OFFSET) };
    if own != 0 {
        return own;
    }

    let made = unsafe { MADE_HEAP };
    if made != 0 {
        return made;
    }

    let heap = unsafe {
        (user_api().create_heap)(HEAP_GROWABLE, core::ptr::null_mut(), 0, 0,
            core::ptr::null_mut(), core::ptr::null_mut())
    };
    unsafe { MADE_HEAP = heap };

    heap
}

static mut MADE_HEAP: usize = 0;
const HEAP_GROWABLE: u32 = 0x2;

static SLOTS: crate::gthread::ThreadSlots = crate::gthread::ThreadSlots {
    take: take_thread_slot,
    get: read_thread_slot,
    set: write_thread_slot,
};

fn take_thread_slot() -> u32 {
    unsafe { (thread_slot_api().take)() }
}

fn read_thread_slot(slot: u32) -> *mut c_void {
    unsafe { (thread_slot_api().read)(slot) }
}

fn write_thread_slot(slot: u32, value: *mut c_void) {
    unsafe { (thread_slot_api().write)(slot, value) };
}

fn thread_slot_api() -> &'static ThreadSlotApi {
    unsafe {
        if (*core::ptr::addr_of!(THREAD_SLOT_API)).is_none() {
            let library = module_base(peb(), b"kernel32.dll");
            THREAD_SLOT_API = Some(ThreadSlotApi {
                take: core::mem::transmute(export(library, b"TlsAlloc")),
                read: core::mem::transmute(export(library, b"TlsGetValue")),
                write: core::mem::transmute(export(library, b"TlsSetValue")),
            });
        }
        (*core::ptr::addr_of!(THREAD_SLOT_API)).as_ref().unwrap()
    }
}

struct ThreadSlotApi {
    take: windows_fn!(=> u32),
    read: windows_fn!(u32 => *mut c_void),
    write: windows_fn!(u32, *mut c_void => i32),
}

static mut THREAD_SLOT_API: Option<ThreadSlotApi> = None;

fn hear_about_faults() {
    unsafe { FAULT_HANDLER = (user_api().add_fault_handler)(1, on_fault as *const c_void) };
}

fn stop_hearing_about_faults() {
    unsafe { (user_api().remove_fault_handler)(FAULT_HANDLER) };
}

#[cfg(target_arch = "x86")]
unsafe extern "stdcall" fn on_fault(pointers: *const usize) -> i32 {
    unsafe { look_at_the_fault(pointers) }
}

#[cfg(target_arch = "x86_64")]
unsafe extern "win64" fn on_fault(pointers: *const usize) -> i32 {
    unsafe { look_at_the_fault(pointers) }
}

#[cfg(target_arch = "aarch64")]
unsafe extern "C" fn on_fault(pointers: *const usize) -> i32 {
    unsafe { look_at_the_fault(pointers) }
}

unsafe fn look_at_the_fault(pointers: *const usize) -> i32 {
    let record = unsafe { pointers.read() } as *const usize;
    let context = unsafe { pointers.add(1).read() } as *mut u8;

    let code = unsafe { (record as *const u32).read() };
    let at = unsafe { record.byte_add(RECORD_ADDRESS).read() };
    let accessed = unsafe { record.byte_add(RECORD_ACCESSED).read() };

    let state = unsafe { crate::winnt::cpu_state_of(context) };
    let mut cpu_context = crate::gum_windows::cpu_context_from(&state);

    let handled = unsafe {
        crate::bindings::gum_barebone_handle_exception(crate::gum_windows::fault_type_of(code),
            at as *mut c_void, accessed as *mut c_void, &mut cpu_context)
    };
    if handled == 0 {
        return SEARCH_ON;
    }

    let state = crate::gum_windows::cpu_state_from(&cpu_context);
    unsafe { crate::winnt::write_cpu_state(context, &state) };

    CARRY_ON
}

static mut FAULT_HANDLER: *mut c_void = core::ptr::null_mut();

const CARRY_ON: i32 = -1;
const SEARCH_ON: i32 = 0;

#[cfg(target_pointer_width = "32")]
const RECORD_ADDRESS: usize = 0x0c;
#[cfg(target_pointer_width = "32")]
const RECORD_ACCESSED: usize = 0x18;
#[cfg(target_pointer_width = "64")]
const RECORD_ADDRESS: usize = 0x10;
#[cfg(target_pointer_width = "64")]
const RECORD_ACCESSED: usize = 0x28;

fn copy_has_work() -> bool {
    let arena = unsafe { ARENA };

    unsafe {
        crate::winnt::holds_a_frame_from_host(arena)
            || ((arena + REGISTER_THREAD) as *const u32).read_volatile() != 0
            || ((arena + STOP_REQUEST) as *const u32).read_volatile() != 0
    }
}

fn serve_the_copy() {
    let arena = unsafe { ARENA };

    serve_the_session_server(arena as usize);

    while let Some(frame) = take_frame_from_host(arena) {
        crate::on_frame_from_host(&frame);
    }
}

unsafe extern "C" fn poll_handles(ufds: *mut crate::GPollFd, nfds: u32, timeout: i32) -> i32 {
    const MAX_HANDLES: usize = 16;
    let fds = unsafe { core::slice::from_raw_parts_mut(ufds, nfds as usize) };

    let mut handles: [*mut c_void; MAX_HANDLES] = [core::ptr::null_mut(); MAX_HANDLES];
    let mut origin = [0usize; MAX_HANDLES];
    let mut count = 0usize;
    let mut ready = 0i32;

    for (index, fd) in fds.iter_mut().enumerate() {
        fd.revents = 0;

        if fd.fd == G_WAIT_WAKEUP_HANDLE {
            if !fd.user_data.is_null() {
                if unsafe { (fd.user_data as *const i32).read_volatile() } != 0 {
                    fd.revents = POLL_IN;
                    ready += 1;
                }
                let token = (fd.user_data as usize + GWAKEUP_TOKEN) as *mut *mut c_void;
                unsafe { token.write(fd.user_data) };
            }
        } else if fd.fd != -1 && count < MAX_HANDLES {
            handles[count] = fd.fd as isize as *mut c_void;
            origin[count] = index;
            count += 1;
        }
    }

    if ready == 0 && count != 0 {
        let due_time = if timeout < 0 {
            None
        } else {
            Some(-((timeout as i64) * 10_000))
        };
        let status = unsafe {
            (user_api().wait_for_objects)(count as u32, handles.as_ptr(), WAIT_ANY_OBJECT, 0,
                due_time.as_ref().map_or(core::ptr::null(), |t| t))
        };
        if status >= 0 && (status as usize) < count {
            let slot = status as usize;
            fds[origin[slot]].revents = POLL_IN;
            ready += 1;
            if core::ptr::eq(handles[slot], target_wake_handle()) {
                unsafe { (user_api().reset_event)(handles[slot], core::ptr::null_mut()) };
            }
        }
    }

    for fd in fds.iter() {
        if fd.fd == G_WAIT_WAKEUP_HANDLE && !fd.user_data.is_null() {
            let token = (fd.user_data as usize + GWAKEUP_TOKEN) as *mut *mut c_void;
            unsafe { token.write(core::ptr::null_mut()) };
        }
    }

    ready
}

pub(crate) fn poke_loop_wakeup() {
    let event = target_wake_handle();
    if event.is_null() {
        return;
    }
    unsafe { (user_api().set_event)(event, core::ptr::null_mut()) };
}

const G_WAIT_WAKEUP_HANDLE: i32 = -43;
const GWAKEUP_TOKEN: usize = 8;
const POLL_IN: u16 = 1;
const WAIT_ANY_OBJECT: u32 = 1;

fn target_wake_handle() -> *mut c_void {
    unsafe { ((ARENA + TARGET_WAKE_HANDLE) as *const u64).read() as *mut c_void }
}

fn agent_wake_handle() -> *mut c_void {
    unsafe { ((ARENA + AGENT_WAKE_HANDLE) as *const u64).read() as *mut c_void }
}

static mut ARENA: u64 = 0;

#[cfg(target_arch = "x86")]
fn current_client_id(offset: u32) -> u32 {
    let id: u32;
    unsafe {
        core::arch::asm!("mov {0:e}, fs:[{1:e}]", out(reg) id, in(reg) offset,
            options(nomem, nostack, preserves_flags));
    }
    id
}

#[cfg(target_arch = "x86_64")]
fn current_client_id(offset: u32) -> u32 {
    let id: u64;
    unsafe {
        core::arch::asm!("mov {0}, gs:[{1:e}]", out(reg) id, in(reg) offset,
            options(nomem, nostack, preserves_flags));
    }
    id as u32
}

#[cfg(target_arch = "x86")]
fn peb() -> usize {
    let peb: usize;
    unsafe {
        core::arch::asm!("mov {0:e}, fs:[0x30]", out(reg) peb,
            options(nomem, nostack, preserves_flags));
    }
    peb
}

#[cfg(target_arch = "x86_64")]
fn peb() -> usize {
    let peb: usize;
    unsafe {
        core::arch::asm!("mov {0}, gs:[0x60]", out(reg) peb,
            options(nomem, nostack, preserves_flags));
    }
    peb
}

#[cfg(target_arch = "aarch64")]
fn current_client_id(offset: u32) -> u32 {
    unsafe { ((thread_block() + offset as usize) as *const u64).read() as u32 }
}

#[cfg(target_arch = "aarch64")]
fn peb() -> usize {
    unsafe { ((thread_block() + BLOCK_PEB_OFFSET) as *const usize).read() }
}

#[cfg(target_arch = "aarch64")]
fn thread_block() -> usize {
    let block: usize;
    unsafe {
        core::arch::asm!("mov {0}, x18", out(reg) block,
            options(nomem, nostack, preserves_flags));
    }
    block
}

#[cfg(target_arch = "aarch64")]
const BLOCK_PEB_OFFSET: usize = 0x60;

#[cfg(target_pointer_width = "32")]
const PEB_PROCESS_HEAP_OFFSET: usize = 0x18;

#[cfg(target_pointer_width = "64")]
const PEB_PROCESS_HEAP_OFFSET: usize = 0x30;

const MEMORY_BASIC_INFORMATION: u32 = 0;
const MEMORY_BASE: usize = 0x00;
const MEMORY_REGION_SIZE: usize = if core::mem::size_of::<usize>() == 8 { 0x18 } else { 0x0c };
const MEMORY_STATE: usize = if core::mem::size_of::<usize>() == 8 { 0x20 } else { 0x10 };
const MEMORY_PROTECT: usize = if core::mem::size_of::<usize>() == 8 { 0x24 } else { 0x14 };
const MEMORY_INFORMATION_SIZE: usize = if core::mem::size_of::<usize>() == 8 { 0x30 } else { 0x1c };
const PAGE_PROTECTION_MASK: u32 = 0xff;
const PAGE_WRITECOPY: u32 = 0x08;
const PAGE_EXECUTE: u32 = 0x10;
const PAGE_EXECUTE_WRITECOPY: u32 = 0x80;
const PAGE_READONLY: u32 = 0x02;
const PAGE_EXECUTE_READ: u32 = 0x20;
const GUM_PAGE_READ: u32 = 0x1;
const GUM_PAGE_WRITE: u32 = 0x2;
const GUM_PAGE_EXECUTE: u32 = 0x4;

// Only ring 0 can write to a port.
fn log(_msg: &str) {}

fn alloc(size: usize) -> *mut u8 {
    unsafe { (user_api().allocate_heap)(process_heap(), 0, size) }
}

fn free(ptr: *mut u8, _size: usize) {
    unsafe { (user_api().free_heap)(process_heap(), 0, ptr) };
}

fn alloc_code(size: usize) -> *mut u8 {
    let mut address: *mut u8 = core::ptr::null_mut();
    let mut region = size;

    unsafe {
        (user_api().allocate_memory)(CURRENT_PROCESS, &mut address, 0, &mut region,
            MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    }

    address
}

fn free_code(ptr: *mut u8, _size: usize) {
    let mut address = ptr;
    let mut region = 0usize;

    unsafe { (user_api().free_memory)(CURRENT_PROCESS, &mut address, &mut region, MEM_RELEASE) };
}

fn protect(address: u64, size: usize, gum_prot: u32) -> bool {
    let mut base = address as *mut u8;
    let mut region = size;
    let mut previous = 0u32;

    unsafe {
        (user_api().protect_memory)(CURRENT_PROCESS, &mut base, &mut region,
            page_protection_of(gum_prot), &mut previous) >= 0
    }
}

fn page_protection_of(gum_prot: u32) -> u32 {
    match (gum_prot & GUM_PAGE_WRITE != 0, gum_prot & GUM_PAGE_EXECUTE != 0) {
        (true, true) => PAGE_EXECUTE_READWRITE,
        (true, false) => PAGE_READWRITE,
        (false, true) => PAGE_EXECUTE_READ,
        (false, false) => PAGE_READONLY,
    }
}

// Only the kernel half can read the page tables. The copy asks the memory manager.
fn protection_at(address: usize) -> u32 {
    let mut region = [0u8; MEMORY_INFORMATION_SIZE];
    if !query_region(address as u64, &mut region) {
        return 0;
    }

    gum_protection_of(&region)
}

fn enumerate_ranges(found: &mut dyn FnMut(u64, u64, u32)) {
    let mut address = 0u64;
    let mut region = [0u8; MEMORY_INFORMATION_SIZE];

    while query_region(address, &mut region) {
        let size = read_field(&region, MEMORY_REGION_SIZE);
        if size == 0 {
            return;
        }

        let protection = gum_protection_of(&region);
        if protection != 0 {
            found(read_field(&region, MEMORY_BASE), size, protection);
        }

        address = address.wrapping_add(size);
    }
}

fn enumerate_threads(found: &mut dyn FnMut(crate::kernel::ThreadInfo), with_registers: bool) {
    for_each_thread(&mut |id| {
        found(crate::kernel::ThreadInfo {
            id,
            cpu_state: with_registers.then(|| registers_of(id)).flatten(),
        });
        true
    });
}

// A thread cannot ask the system about itself: what comes back describes the asking, not the
// thread as a script would want to see it.
fn find_thread(id: u32, with_registers: bool) -> Option<crate::kernel::ThreadInfo> {
    let mut found = false;
    for_each_thread(&mut |seen| {
        found = seen == id;
        !found
    });

    found.then(|| crate::kernel::ThreadInfo {
        id,
        cpu_state: with_registers.then(|| registers_of(id)).flatten(),
    })
}

fn for_each_thread(visit: &mut dyn FnMut(u32) -> bool) {
    let api = thread_list_api();
    let mut thread: *mut c_void = core::ptr::null_mut();

    unsafe {
        for _ in 0..MAX_THREADS_PER_PROCESS {
            let mut next: *mut c_void = core::ptr::null_mut();
            let status = (api.next_thread)(CURRENT_PROCESS, thread,
                THREAD_QUERY_LIMITED_INFORMATION, 0, 0, &mut next);
            if !thread.is_null() {
                (api.close)(thread);
            }
            if status < 0 {
                return;
            }
            if next.is_null() {
                return;
            }
            thread = next;

            if let Some(id) = id_of_thread(api, thread) {
                if !visit(id) {
                    (api.close)(thread);
                    return;
                }
            }
        }

        if !thread.is_null() {
            (api.close)(thread);
        }
    }
}

const MAX_THREADS_PER_PROCESS: usize = 4096;

unsafe fn id_of_thread(api: &ThreadListApi, thread: *mut c_void) -> Option<u32> {
    let mut basic = [0u8; THREAD_BASIC_INFORMATION_SIZE];

    unsafe {
        if (api.query_thread)(thread, THREAD_BASIC_INFORMATION, basic.as_mut_ptr(),
                basic.len() as u32, core::ptr::null_mut()) < 0 {
            return None;
        }

        Some(basic.as_ptr().add(THREAD_BASIC_ID_OFFSET).cast::<usize>().read_unaligned() as u32)
    }
}

fn modify_thread(id: u32, change: &mut dyn FnMut(&mut crate::kernel::CpuState)) -> bool {
    let api = thread_list_api();
    unsafe {
        let thread = (api.open)(
            THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME, 0, id);
        if thread.is_null() {
            return false;
        }

        let mut changed = false;
        if (api.suspend)(thread, core::ptr::null_mut()) >= 0 {
            let mut context = [0u8; CONTEXT_SIZE + CONTEXT_ALIGNMENT];
            let base = context.as_mut_ptr().add(context.as_ptr().align_offset(CONTEXT_ALIGNMENT));
            (base.add(CONTEXT_FLAGS) as *mut u32).write(CONTEXT_FULL);

            if (api.get_context)(thread, base) >= 0 {
                let mut state = crate::winnt::cpu_state_of(base);
                change(&mut state);
                crate::winnt::write_cpu_state(base, &state);
                (base.add(CONTEXT_FLAGS) as *mut u32).write(CONTEXT_FULL);

                changed = (api.set_context)(thread, base) >= 0;
            }

            (api.resume)(thread, core::ptr::null_mut());
        }

        (api.close)(thread);

        changed
    }
}

fn registers_of(id: u32) -> Option<crate::kernel::CpuState> {
    if id == current_thread_id() as u32 {
        return None;
    }

    let api = thread_list_api();
    unsafe {
        let thread = (api.open)(THREAD_GET_CONTEXT, 0, id);
        if thread.is_null() {
            return None;
        }

        let mut context = [0u8; CONTEXT_SIZE + CONTEXT_ALIGNMENT];
        let base = context.as_mut_ptr().add(context.as_ptr().align_offset(CONTEXT_ALIGNMENT));
        (base.add(CONTEXT_FLAGS) as *mut u32).write(CONTEXT_FULL);

        let asked = (api.get_context)(thread, base);
        (api.close)(thread);

        (asked >= 0).then(|| crate::winnt::cpu_state_of(base))
    }
}

const THREAD_GET_CONTEXT: u32 = 0x0008;
const THREAD_SET_CONTEXT: u32 = 0x0010;
const THREAD_SUSPEND_RESUME: u32 = 0x0002;

fn thread_list_api() -> &'static ThreadListApi {
    unsafe {
        if (*core::ptr::addr_of!(THREAD_LIST_API)).is_none() {
            let library = module_base(peb(), b"kernel32.dll");
            THREAD_LIST_API = Some(ThreadListApi {
                next_thread: core::mem::transmute(
                    export(loader_library(), b"NtGetNextThread")),
                query_thread: core::mem::transmute(
                    export(loader_library(), b"NtQueryInformationThread")),
                open: core::mem::transmute(export(library, b"OpenThread")),
                get_context: core::mem::transmute(
                    export(loader_library(), b"NtGetContextThread")),
                set_context: core::mem::transmute(
                    export(loader_library(), b"NtSetContextThread")),
                suspend: core::mem::transmute(export(loader_library(), b"NtSuspendThread")),
                resume: core::mem::transmute(export(loader_library(), b"NtResumeThread")),
                close: core::mem::transmute(export(library, b"CloseHandle")),
            });
        }
        (*core::ptr::addr_of!(THREAD_LIST_API)).as_ref().unwrap()
    }
}

struct ThreadListApi {
    next_thread: windows_fn!(*mut c_void, *mut c_void, u32, u32, u32, *mut *mut c_void => i32),
    query_thread: windows_fn!(*mut c_void, u32, *mut u8, u32, *mut u32 => i32),
    open: windows_fn!(u32, i32, u32 => *mut c_void),
    get_context: windows_fn!(*mut c_void, *mut u8 => i32),
    set_context: windows_fn!(*mut c_void, *const u8 => i32),
    suspend: windows_fn!(*mut c_void, *mut u32 => i32),
    resume: windows_fn!(*mut c_void, *mut u32 => i32),
    close: windows_fn!(*mut c_void => i32),
}

static mut THREAD_LIST_API: Option<ThreadListApi> = None;

const THREAD_QUERY_LIMITED_INFORMATION: u32 = 0x0800;
const THREAD_BASIC_INFORMATION: u32 = 0;
#[cfg(target_pointer_width = "64")]
const THREAD_BASIC_INFORMATION_SIZE: usize = 0x30;
#[cfg(target_pointer_width = "64")]
const THREAD_BASIC_ID_OFFSET: usize = 0x18;
#[cfg(target_pointer_width = "32")]
const THREAD_BASIC_INFORMATION_SIZE: usize = 0x1c;
#[cfg(target_pointer_width = "32")]
const THREAD_BASIC_ID_OFFSET: usize = 0x0c;

fn wait(token: *const u8, timeout_us: Option<u64>, check: &mut dyn FnMut() -> bool) {
    let slot = slot_for_token(token);
    SLEEPERS[slot].fetch_add(1, Ordering::AcqRel);
    let event = event_in(slot);
    if check() {
        release_slot(slot, token);
        return;
    }

    let due_time = timeout_us.map(|us| -((us as i64) * 10));
    unsafe {
        (user_api().wait_for_object)(event, 0,
            due_time.as_ref().map_or(core::ptr::null(), |t| t))
    };
    if core::ptr::eq(event, target_wake_handle()) {
        unsafe { (user_api().reset_event)(event, core::ptr::null_mut()) };
    }
    release_slot(slot, token);
}

// A token names one wait of one thread, and the next wait has a token of its own. Thus a slot
// belongs to a token only while somebody waits on it: a table that never let go filled after a
// few dozen waits, and every token after that shared the first slot, where a wake meant for one
// thread released another.
fn release_slot(slot: usize, token: *const u8) {
    if SLEEPERS[slot].fetch_sub(1, Ordering::AcqRel) != 1 {
        return;
    }

    let _ = OWNERS[slot].compare_exchange(token as usize, 0, Ordering::AcqRel, Ordering::Acquire);
}

fn wake(token: *const u8) {
    let Some(slot) = slot_owned_by(token) else {
        return;
    };
    let event = event_in(slot);

    let mut left = SLEEPERS[slot].load(Ordering::Acquire).max(1);
    while left != 0 {
        unsafe { (user_api().set_event)(event, core::ptr::null_mut()) };
        left -= 1;
    }
}

fn slot_for_token(token: *const u8) -> usize {
    let start = (token as usize / core::mem::align_of::<usize>()) % NUM_EVENTS;
    for step in 0..NUM_EVENTS {
        let slot = (start + step) % NUM_EVENTS;

        let owner = OWNERS[slot].load(Ordering::Acquire);
        if owner == token as usize {
            return slot;
        }
        if owner == 0
                && OWNERS[slot].compare_exchange(0, token as usize, Ordering::AcqRel,
                    Ordering::Acquire).is_ok() {
            return slot;
        }
    }

    0
}

// The kernel half sets the one event a copy has a handle to, thus the loop keeps waiting on
// that one and every other token gets an event of its own.
fn slot_owned_by(token: *const u8) -> Option<usize> {
    let start = (token as usize / core::mem::align_of::<usize>()) % NUM_EVENTS;
    for step in 0..NUM_EVENTS {
        let slot = (start + step) % NUM_EVENTS;
        if OWNERS[slot].load(Ordering::Acquire) == token as usize {
            return Some(slot);
        }
    }

    None
}

fn event_in(slot: usize) -> *mut c_void {
    if OWNERS[slot].load(Ordering::Acquire) == crate::glib::wakeup_token() as usize {
        return target_wake_handle();
    }

    loop {
        let existing = EVENTS[slot].load(Ordering::Acquire);
        if existing != 0 {
            return existing as *mut c_void;
        }

        let mut made: *mut c_void = core::ptr::null_mut();
        unsafe {
            (user_api().create_event)(&mut made, EVENT_ALL_ACCESS, core::ptr::null_mut(),
                SYNCHRONIZATION_EVENT, 0)
        };
        if EVENTS[slot].compare_exchange(0, made as usize, Ordering::AcqRel, Ordering::Acquire)
                .is_err() {
            unsafe { (user_api().close)(made) };
        }
    }
}

const NUM_EVENTS: usize = 32;
const EVENT_ALL_ACCESS: u32 = 0x1f_0003;
const SYNCHRONIZATION_EVENT: u32 = 1;
static OWNERS: [AtomicUsize; NUM_EVENTS] = [const { AtomicUsize::new(0) }; NUM_EVENTS];
static EVENTS: [AtomicUsize; NUM_EVENTS] = [const { AtomicUsize::new(0) }; NUM_EVENTS];
static SLEEPERS: [AtomicUsize; NUM_EVENTS] = [const { AtomicUsize::new(0) }; NUM_EVENTS];

fn yield_now() {
    unsafe { (user_api().yield_execution)() };
}

fn current_process_id() -> u32 {
    current_client_id(BLOCK_PROCESS_ID)
}

fn current_thread_id() -> u64 {
    current_client_id(BLOCK_THREAD_ID) as u64
}

fn shared_data() -> usize {
    USER_SHARED_DATA
}

fn query_region(address: u64, region: &mut [u8; MEMORY_INFORMATION_SIZE]) -> bool {
    let mut written = 0usize;

    unsafe {
        (user_api().query_memory)(CURRENT_PROCESS, address as *mut u8, MEMORY_BASIC_INFORMATION,
            region.as_mut_ptr(), MEMORY_INFORMATION_SIZE, &mut written) >= 0
    }
}

fn gum_protection_of(region: &[u8; MEMORY_INFORMATION_SIZE]) -> u32 {
    if read_field(region, MEMORY_STATE) as u32 != MEM_COMMIT {
        return 0;
    }

    match (read_field(region, MEMORY_PROTECT) as u32) & PAGE_PROTECTION_MASK {
        PAGE_READONLY => GUM_PAGE_READ,
        PAGE_READWRITE | PAGE_WRITECOPY => GUM_PAGE_READ | GUM_PAGE_WRITE,
        PAGE_EXECUTE => GUM_PAGE_EXECUTE,
        PAGE_EXECUTE_READ => GUM_PAGE_READ | GUM_PAGE_EXECUTE,
        PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY =>
            GUM_PAGE_READ | GUM_PAGE_WRITE | GUM_PAGE_EXECUTE,
        _ => 0,
    }
}

fn read_field(region: &[u8; MEMORY_INFORMATION_SIZE], offset: usize) -> u64 {
    let mut value = [0u8; 8];
    value.copy_from_slice(&region[offset..offset + 8]);
    u64::from_le_bytes(value)
}

// The kernel half waits for an event, thus set the event after you write the frame.
pub(crate) fn signal_kernel_half() {
    unsafe { (user_api().set_event)(agent_wake_handle(), core::ptr::null_mut()) };
}
