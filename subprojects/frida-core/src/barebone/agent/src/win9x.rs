// The VMM side of the blob flavor. Reached the way XNU is: the host fills in a
// slot per service before calling _start, so nothing here is a link-time import.
//
// The signatures follow the VxD service documentation; none of them has been
// exercised against a running kernel yet.

extern crate alloc;

use alloc::collections::{BTreeMap, VecDeque};
use core::ffi::c_void;
use core::sync::atomic::{AtomicU32, AtomicUsize, Ordering};

use crate::bindings::{
    GumAddress, GumX86Writer, cs_insn, gconstpointer, gpointer, gum_x86_writer_flush,
    gum_x86_writer_new, gum_x86_writer_put_jmp_address, gum_x86_writer_unref,
};
use crate::kernel::{CpuState, ThreadEntry, ThreadInfo};
use crate::ring::{Ring, Taken};

// A process is made in ring 3, thus a copy of the agent does this work.
pub use crate::win9x_user::{
    LoadedModule, LoaderEntryPoints, ThreadEntryPoints, enumerate_modules, enumerate_shortcuts,
    loader_entry_points,
    on_module_load, on_module_load_with_flags, on_module_unload, on_thread_exit, resume_process,
    spawn_process, thread_entry_points, thread_exit_slot, thread_start, thread_start_slot,
};

pub use frida_win9x_thread_start_thunk as on_thread_start;

pub const MODULE_DIRECTORY: &str = "/WINDOWS/SYSTEM/VMM32/";

pub(crate) const PAGE_SIZE: u32 = 4096;

const PG_SYS: u32 = 1;
const PAGE_FLAGS_CODE: u32 = PAGE_FIXED | PAGE_LOCKED | PAGE_ZERO_INIT;
const PAGE_FIXED: u32 = 0x8;
const PAGE_LOCKED: u32 = 0x80;
const PAGE_ZERO_INIT: u32 = 0x1;

const PAGE_PRESENT: u32 = 0x1;
const PAGE_WRITEABLE: u32 = 0x2;
pub(crate) const GUM_PAGE_WRITE: u32 = 0x2;

const BLOCK_SVC_INTS: u32 = 1 << 0;

const DEBUG_CONSOLE_PORT: u16 = 0xe9;

pub fn log(msg: &str) {
    for byte in msg.bytes() {
        if byte == 0 {
            break;
        }
        write_debug_byte(byte);
    }
}

pub fn is_win16_task(pdb: u32) -> bool {
    unsafe { (pdb as *const u32).byte_add(PDB_FLAGS_OFFSET).read() & WIN16_TASK != 0 }
}

fn program_of(line: *const u8) -> *const u8 {
    if line.is_null() {
        return line;
    }

    let program = unsafe { (&raw mut PROGRAM).as_mut().unwrap() };
    let quoted = unsafe { line.read() } == b'"';
    let mut read = quoted as usize;
    let mut written = 0;
    while written < program.len() - 1 {
        let byte = unsafe { line.add(read).read() };
        if byte == 0 || (quoted && byte == b'"') || (!quoted && byte == b' ') {
            break;
        }
        program[written] = byte;
        read += 1;
        written += 1;
    }
    program[written] = 0;

    program.as_ptr()
}

static mut PROGRAM: [u8; 260] = [0; 260];

fn log_hex(value: u32) {
    let mut shift = 32;
    while shift != 0 {
        shift -= 4;
        let digit = ((value >> shift) & 0xf) as u8;
        write_debug_byte(if digit < 10 { b'0' + digit } else { b'a' + digit - 10 });
    }
    write_debug_byte(b'\n');
}

fn write_debug_byte(byte: u8) {
    unsafe {
        core::arch::asm!("out dx, al", in("dx") DEBUG_CONSOLE_PORT, in("al") byte,
            options(nomem, nostack, preserves_flags));
    }
}

pub fn panic(msg: &str) {
    log(msg);
    unsafe {
        fatal_error_handler(msg.as_ptr(), 0);
    }
}

pub fn alloc(size: usize) -> *mut u8 {
    (primitives().alloc)(size)
}

pub fn free(ptr: *mut u8, size: usize) {
    (primitives().free)(ptr, size)
}

pub fn alloc_code(size: usize) -> *mut u8 {
    (primitives().alloc_code)(size)
}

pub fn free_code(ptr: *mut u8, size: usize) {
    (primitives().free_code)(ptr, size)
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

pub fn spawn_thread(entry: ThreadEntry, parameter: *mut c_void) -> isize {
    (primitives().spawn_thread)(entry, parameter)
}

// VMM refuses to build a thread from the borrowed context the host enters us on, and only
// says so by never returning, so hand the work to a point where VMM is between jobs.
pub fn run_when_ready(action: fn()) {
    unsafe {
        READY_ACTION = Some(action);
        schedule_global_event(frida_win9x_event_thunk);
    }
}

#[unsafe(no_mangle)]
extern "C" fn frida_win9x_on_event() {
    if let Some(action) = unsafe { READY_ACTION } {
        action();
    }
}

static mut READY_ACTION: Option<fn()> = None;

#[unsafe(no_mangle)]
extern "C" fn frida_win9x_thread_start() {
    unsafe {
        if let Some(entry) = THREAD_ENTRY {
            entry(THREAD_PARAMETER, 0);
        }
    }
}

static mut THREAD_ENTRY: Option<ThreadEntry> = None;
static mut THREAD_PARAMETER: *mut c_void = core::ptr::null_mut();

pub(crate) const THREAD_STACK_SIZE: usize = 64 * 1024;

pub fn wait(token: *const u8, timeout_us: Option<u64>, check: &mut dyn FnMut() -> bool) {
    (primitives().wait)(token, timeout_us, check)
}

fn block_until_signalled_or_timed_out(semaphore: u32, timeout_us: u64) {
    let timeout_ms = (timeout_us / 1000).max(1) as u32;
    let timeout = unsafe { set_global_time_out(timeout_ms, semaphore) };
    unsafe {
        wait_semaphore(semaphore, BLOCK_SVC_INTS);
    }
    if timeout != 0 {
        unsafe {
            cancel_time_out(timeout);
        }
    }
}

pub fn poke_loop_wakeup() {}

pub fn wake(token: *const u8) {
    (primitives().wake)(token)
}

pub fn yield_now() {
    (primitives().yield_now)()
}

pub fn allocate_shared(size: usize) -> u32 {
    alloc_shared(size) as u32
}

// The payload is a call into this image, not a copy of it. The pages of the agent are
// available in ring 3, thus the new thread runs the same code as the kernel half.
pub fn inject_agent(pid: u32) -> u32 {
    let process = process_for_pid(pid);
    if process == 0 {
        return 0;
    }

    let (image_base, entry) = place_shared_agent();

    let mut payload = [
        0xff, 0x74, 0x24, 0x04,
        0xba, 0, 0, 0, 0,
        0xff, 0xd2,
        0xeb, 0xfe,
    ];
    payload[5..9].copy_from_slice(&entry.to_le_bytes());

    let injection = inject(process, &payload);
    if injection.thread == 0 {
        return 0;
    }

    unsafe {
        ((injection.arena + IMAGE_BASE) as *mut u32).write_volatile(image_base);
        ((injection.arena + IMAGE_SIZE) as *mut u32)
            .write_volatile((*core::ptr::addr_of!(crate::OWN_RANGE)).size as u32);
        ((injection.arena + MODULE_LIST) as *mut u32)
            .write_volatile(publish_module_list(process))
    };
    if !await_flag(injection.arena + OBSERVED_PID) {
        return 0;
    }

    let observed = unsafe { ((injection.arena + OBSERVED_PID) as *const u32).read_volatile() };
    unsafe {
        targets().insert(observed, Target {
            arena: injection.arena,
            stack: injection.stack,
            image_base,
        })
    };

    observed
}

pub fn hook_shared_code(pid: u32, target: u32, patch: &[u8]) -> bool {
    let Some(index) = shared_hook_for(target) else {
        return false;
    };
    let hooks = unsafe { (&raw mut SHARED_HOOKS).as_mut().unwrap() };

    if patch == &hooks[index].original[..patch.len()] {
        forget_guard(&mut hooks[index], pid);
        return true;
    }

    let destination = destination_of(patch, target);
    let Some(destination) = destination else {
        return false;
    };

    add_guard(&mut hooks[index], pid, destination)
}

pub fn release_shared_hooks() {
    let hooks = unsafe { (&raw mut SHARED_HOOKS).as_mut().unwrap() };

    for hook in hooks.iter_mut() {
        if hook.entered {
            write_original(hook);
        }
    }

    hooks.clear();
}

fn shared_hook_for(target: u32) -> Option<usize> {
    let hooks = unsafe { (&raw mut SHARED_HOOKS).as_mut().unwrap() };
    if let Some(index) = hooks.iter().position(|hook| hook.target == target) {
        return Some(index);
    }

    let page = alloc_shared(PAGE_SIZE as usize) as u32;
    if page == 0 {
        return None;
    }

    let resume = page;
    let moved = move_code(target, resume, JUMP_SIZE)?;

    let mut original = alloc::vec::Vec::new();
    for offset in 0..moved {
        original.push(unsafe { ((target + offset as u32) as *const u8).read() });
    }

    hooks.push(SharedHook {
        target,
        original,
        head: resume,
        free: page + GUARDS_OFFSET,
        entered: false,
        guards: alloc::vec::Vec::new(),
    });

    Some(hooks.len() - 1)
}

fn add_guard(hook: &mut SharedHook, pid: u32, destination: u32) -> bool {
    if let Some((_, guard)) = hook.guards.iter().find(|(known, _)| *known == pid).copied() {
        write_guard(guard, pid, destination, guard_next(guard));
        return true;
    }

    if let Some(guard) = unclaimed_guard(hook) {
        hook.guards.push((pid, guard));
        write_guard(guard, pid, destination, guard_next(guard));
        lead_to_the_chain(hook);
        return true;
    }

    let guard = hook.free;
    if (guard + GUARD_SIZE) % PAGE_SIZE < GUARD_SIZE {
        return false;
    }
    hook.free += GUARD_SIZE;

    write_guard(guard, pid, destination, hook.head);
    hook.head = guard;
    hook.guards.push((pid, guard));

    lead_to_the_chain(hook);

    true
}

fn lead_to_the_chain(hook: &mut SharedHook) {
    jump_to(hook.target, hook.head, hook.original.len());
    hook.entered = true;
}

fn forget_guard(hook: &mut SharedHook, pid: u32) {
    let Some((_, guard)) = hook.guards.iter().find(|(known, _)| *known == pid).copied() else {
        return;
    };

    unsafe { ((guard + GUARD_PROCESS) as *mut u32).write_unaligned(0) };
    hook.guards.retain(|(known, _)| *known != pid);

    if hook.guards.is_empty() {
        write_original(hook);
    }
}

fn write_original(hook: &mut SharedHook) {
    unsafe {
        protect(
            hook.target as u64,
            hook.original.len(),
            GUM_PAGE_READ | GUM_PAGE_WRITE | GUM_PAGE_EXECUTE,
        );
        for (offset, byte) in hook.original.iter().enumerate() {
            ((hook.target + offset as u32) as *mut u8).write(*byte);
        }
    }

    hook.entered = false;
}

fn unclaimed_guard(hook: &SharedHook) -> Option<u32> {
    let mut guard = hook.head;
    while guard >= hook.guards_start() && guard < hook.free {
        if unsafe { ((guard + GUARD_PROCESS) as *const u32).read_unaligned() } == 0 {
            return Some(guard);
        }
        guard = guard_next(guard);
    }

    None
}

pub fn forget_guards_of(pid: u32) {
    let hooks = unsafe { (&raw mut SHARED_HOOKS).as_mut().unwrap() };
    for hook in hooks.iter_mut() {
        forget_guard(hook, pid);
    }
}

fn write_guard(guard: u32, pid: u32, destination: u32, next: u32) {
    let get_pid = kernel32_export(b"GetCurrentProcessId");

    let mut code = [
        0x60,
        0xb8, 0, 0, 0, 0,
        0xff, 0xd0,
        0x3d, 0, 0, 0, 0,
        0x75, 0x06,
        0x61,
        0xe9, 0, 0, 0, 0,
        0x61,
        0xe9, 0, 0, 0, 0,
    ];
    code[2..6].copy_from_slice(&get_pid.to_le_bytes());
    code[9..13].copy_from_slice(&pid.to_le_bytes());
    code[17..21].copy_from_slice(&destination.wrapping_sub(guard + 21).to_le_bytes());
    code[23..27].copy_from_slice(&next.wrapping_sub(guard + 27).to_le_bytes());

    unsafe { core::ptr::copy_nonoverlapping(code.as_ptr(), guard as *mut u8, code.len()) };
}

fn guard_next(guard: u32) -> u32 {
    let displacement = unsafe { ((guard + GUARD_NEXT) as *const u32).read_unaligned() };

    guard.wrapping_add(GUARD_END).wrapping_add(displacement)
}

fn move_code(from: u32, to: u32, least: usize) -> Option<usize> {
    unsafe {
        let writer = gum_x86_writer_new(to as gpointer);
        let relocator = gum_x86_relocator_new(from as gconstpointer, writer);

        let mut moved = 0;
        while moved < least as u32 {
            let read = gum_x86_relocator_read_one(relocator, core::ptr::null_mut());
            if read == 0 {
                break;
            }
            moved = read;
        }
        gum_x86_relocator_write_all(relocator);

        let landed = moved >= least as u32
            && gum_x86_writer_put_jmp_address(writer, (from + moved) as GumAddress) != 0
            && gum_x86_writer_flush(writer) != 0;

        gum_x86_relocator_unref(relocator);
        gum_x86_writer_unref(writer);

        landed.then_some(moved as usize)
    }
}

unsafe extern "C" {
    fn gum_x86_relocator_new(input_code: gconstpointer, output: *mut GumX86Writer)
        -> *mut c_void;
    fn gum_x86_relocator_unref(relocator: *mut c_void);
    fn gum_x86_relocator_read_one(relocator: *mut c_void, instruction: *mut *const cs_insn)
        -> u32;
    fn gum_x86_relocator_write_all(relocator: *mut c_void);
}

fn destination_of(patch: &[u8], at: u32) -> Option<u32> {
    match patch {
        [0xe9, ..] if patch.len() >= JUMP_SIZE => {
            let mut displacement = [0u8; 4];
            displacement.copy_from_slice(&patch[1..5]);

            Some(at.wrapping_add(5).wrapping_add(u32::from_le_bytes(displacement)))
        }
        [0xeb, step, ..] => Some(at.wrapping_add(2).wrapping_add(*step as i8 as i32 as u32)),
        _ => None,
    }
}

fn jump_to(target: u32, destination: u32, span: usize) {
    unsafe {
        protect(target as u64, span, GUM_PAGE_READ | GUM_PAGE_WRITE | GUM_PAGE_EXECUTE);
        (target as *mut u8).write(0xe9);
        ((target + 1) as *mut u32).write_unaligned(destination.wrapping_sub(target + 5));
        for filler in JUMP_SIZE..span {
            ((target + filler as u32) as *mut u8).write(0x90);
        }
    }
}

impl SharedHook {
    fn guards_start(&self) -> u32 {
        (self.free & !(PAGE_SIZE - 1)) + GUARDS_OFFSET
    }
}

struct SharedHook {
    target: u32,
    original: alloc::vec::Vec<u8>,
    head: u32,
    free: u32,
    entered: bool,
    guards: alloc::vec::Vec<(u32, u32)>,
}

static mut SHARED_HOOKS: alloc::vec::Vec<SharedHook> = alloc::vec::Vec::new();

const GUARDS_OFFSET: u32 = 0x100;
const GUARD_SIZE: u32 = 0x20;
const GUARD_PROCESS: u32 = 0x09;
const GUARD_NEXT: u32 = 0x17;
const GUARD_END: u32 = 0x1b;
const JUMP_SIZE: usize = 5;

// Only the copy knows when it is safe to stop. It runs on the stack and in the image that
// this function releases, thus the copy reports when it leaves both.
pub fn release_interrupt() {
    let handle = unsafe { IRQ_HANDLE };
    if handle == 0 {
        return;
    }

    unsafe {
        IRQ_HANDLE = 0;
        vpicd_force_default_behavior(handle);
    }
}

pub fn stop_copies() {
    let pids: alloc::vec::Vec<u32> = unsafe { targets() }.keys().copied().collect();
    for pid in pids {
        detach_from_process(pid);
    }
}

pub fn detach_from_process(pid: u32) -> bool {
    let Some(target) = (unsafe { targets().remove(&pid) }) else {
        return false;
    };
    let arena = target.arena;

    unsafe { ((arena + STOP_REQUEST) as *mut u32).write_volatile(1) };
    if !await_flag(arena + WORKER_STOPPED) || !await_flag(arena + MAIN_STOPPED) {
        return false;
    }

    // A thread sets the flag before it leaves. Thus wait for the time it needs to run its last
    // instructions and return into KERNEL32.
    wait(core::ptr::addr_of!(TEARDOWN_TOKEN), Some(TEARDOWN_GRACE_US), &mut || false);

    forget_hold(arena);

    unsafe {
        free_shared(((arena as u64 + TO_TARGET.buffer) as *const u32).read_volatile());
        free_shared(((arena as u64 + FROM_TARGET.buffer) as *const u32).read_volatile());
    }
    free_shared(target.stack);
    free_shared(target.image_base);
    free_shared(arena);

    forget_guards_of(pid);

    true
}

struct Target {
    arena: u32,
    stack: u32,
    image_base: u32,
}

static mut TEARDOWN_TOKEN: u8 = 0;

pub fn arena_for_pid(pid: u32) -> Option<u64> {
    unsafe { targets().get(&pid).map(|t| t.arena as u64) }
}

pub fn gate_spawns(on: bool) {
    unsafe { GATING_WANTED = on };

    for target in unsafe { targets() }.values() {
        unsafe { ((target.arena + GATING) as *mut u32).write_volatile(on as u32) };
        wake_copy(target.arena);
    }
}

pub fn spawns_are_gated() -> bool {
    unsafe { GATING_WANTED }
}

static mut GATING_WANTED: bool = false;

pub fn serve_patch_requests() {
    let asking: alloc::vec::Vec<(u32, u32)> = unsafe { targets() }
        .iter()
        .map(|(pid, target)| (*pid, target.arena))
        .collect();

    for (owner, arena) in asking {
        let request = unsafe { ((arena + PATCH_REQUEST) as *const u32).read_volatile() };

        match request {
            PATCH_ASKED => {
                let pid = unsafe { ((arena + PATCH_PROCESS) as *const u32).read_volatile() };
                let mut address =
                    unsafe { ((arena + PATCH_ADDRESS) as *const u32).read_volatile() };
                let bytes = unsafe { ((arena + PATCH_BYTES) as *const u32).read_volatile() };

                let previous = patch_in_process(pid, &mut address, bytes);

                unsafe {
                    ((arena + PATCH_ADDRESS) as *mut u32).write_volatile(address);
                    ((arena + PATCH_BYTES) as *mut u32).write_volatile(previous);
                }
            }
            HOOK_ASKED => {
                let address = unsafe { ((arena + PATCH_ADDRESS) as *const u32).read_volatile() };
                let length =
                    unsafe { ((arena + PATCH_LENGTH) as *const u32).read_volatile() } as usize;
                let patch = unsafe {
                    core::slice::from_raw_parts((arena + PATCH_CODE) as *const u8, length)
                };

                let hooked = hook_shared_code(owner, address, patch);

                unsafe { ((arena + PATCH_BYTES) as *mut u32).write_volatile(hooked as u32) };
            }
            _ => continue,
        }

        unsafe { ((arena + PATCH_REQUEST) as *mut u32).write_volatile(PATCH_ANSWERED) };
        wake_asker(arena);
    }
}

pub fn a_patch_is_wanted() -> bool {
    unsafe { targets() }.values().any(|target| {
        let request = unsafe { ((target.arena + PATCH_REQUEST) as *const u32).read_volatile() };

        request == PATCH_ASKED || request == HOOK_ASKED
    })
}

fn patch_in_process(pid: u32, address: &mut u32, bytes: u32) -> u32 {
    let pdb = process_for_pid(pid);
    if pdb == 0 {
        return 0;
    }

    let context = unsafe { (pdb as *const u32).byte_add(PDB_MEMORY_CONTEXT).read() };
    let saved = unsafe { __GetCurrentContext() };
    unsafe { __ContextSwitch(context) };

    if *address == 0 {
        *address = entry_point_of(pdb);
    }

    let mut previous = 0;
    if *address != 0 && *address != NOTHING_TO_HOLD {
        previous = unsafe { (*address as *const u16).read_unaligned() } as u32;
        kernel::protect(*address as u64, PATCH_SIZE, GUM_PAGE_WRITE);
        unsafe { (*address as *mut u16).write_unaligned(bytes as u16) };
    }

    unsafe { __ContextSwitch(saved) };

    previous
}

fn entry_point_of(pdb: u32) -> u32 {
    // A 16 bit program runs in the shared machine, and the only module its process record names
    // is KERNEL32. There is no first instruction of its own to hold it at.
    if is_win16_task(pdb) {
        return NOTHING_TO_HOLD;
    }

    let table = module_table();
    let first = unsafe { (pdb as *const u32).byte_add(PDB_MODREF_OFFSET).read() };
    if table == 0 || first < ARENA_FLOOR {
        return 0;
    }

    let mut modules = alloc::vec::Vec::new();
    collect_modules(table, first, MODREF_NEXT_OFFSET, &mut modules);
    let previous = unsafe { (first as *const u32).byte_add(MODREF_PREVIOUS_OFFSET).read() };
    collect_modules(table, previous, MODREF_PREVIOUS_OFFSET, &mut modules);

    let Some(image) = modules.iter().map(|m| m.base).min() else {
        return 0;
    };

    let headers = image + unsafe { (image as *const u32).byte_add(PE_SIGNATURE_OFFSET).read() };

    image + unsafe { (headers as *const u32).byte_add(PE_ENTRY_POINT_OFFSET).read() }
}

pub fn injected_arenas() -> alloc::vec::Vec<u64> {
    unsafe { targets().values().map(|t| t.arena as u64).collect() }
}

unsafe fn targets() -> &'static mut BTreeMap<u32, Target> {
    unsafe { core::ptr::addr_of_mut!(TARGETS).as_mut().unwrap() }
}

static mut TARGETS: BTreeMap<u32, Target> = BTreeMap::new();

fn process_for_pid(pid: u32) -> u32 {
    let slot = unsafe { (THREAD_BLOCK_SLOT as *const u32).read() };
    let vm = unsafe { get_sys_vm_handle() };
    let first = unsafe { get_initial_thread_handle(vm) };

    let mut thread = first;
    while thread != 0 {
        if unsafe { (thread as *const u32).add(0x2c / 4).read() } == WIN32_THREAD {
            let block = unsafe { (thread as *const u32).byte_add(slot as usize).read() };
            let pdb = unsafe { (block as *const u32).add(1).read() };
            if process_id(pdb) == pid {
                return pdb;
            }
        }

        let next = unsafe { get_next_thread_handle(thread) };
        thread = if next == first { 0 } else { next };
    }

    0
}

// The kernel half is already holding the code, and a page can be shown at a second address.
// Thus a process gets the same code pages, and only the half that gets written to is new memory.
fn place_shared_agent() -> (u32, u32) {
    let own = unsafe { &*core::ptr::addr_of!(crate::OWN_RANGE) };
    let private_offset = crate::writable_half_start() - own.base_address as usize;
    let pages = (own.size as usize).div_ceil(PAGE_SIZE as usize) as u32;
    let base = unsafe { __PageReserve(PR_SHARED, pages, 0) };
    let first = base / PAGE_SIZE;

    let shared_pages = (private_offset / PAGE_SIZE as usize) as u32;
    let own_first = own.base_address as u32 / PAGE_SIZE;
    for page in 0..shared_pages {
        let mut entry = 0;
        unsafe {
            __CopyPageTable(own_first + page, 1, &mut entry, 0);
            // A second address for a page that the memory manager does not own takes the user
            // bit only. The service refuses the flags that new pages take.
            __PageCommitPhys(first + page, 1, entry / PAGE_SIZE, PC_USER);
        }
    }

    unsafe {
        __PageCommit(first + shared_pages, pages - shared_pages, PD_FIXEDZERO, 0,
            PC_FIXED | PC_PRESENT | PC_USER | PC_WRITEABLE);
        crate::install_writable_half(base as usize, (base + private_offset as u32) as usize);
    }

    let entry = base + (crate::win9x_user::frida_win9x_user_main as usize
        - own.base_address as usize) as u32;

    (base, entry)
}

// Ring 3 cannot write to a different process. Thus KERNEL32 makes the thread in the target,
// with the stack and the payload in the shared arena, which all processes can read.
pub fn inject(process: u32, payload: &[u8]) -> Injection {
    hear_from_vmm();

    let arena = alloc_shared(INJECTION_ARENA_SIZE) as u32;
    let entry = arena + PAYLOAD_OFFSET;
    unsafe {
        core::ptr::write_bytes(arena as *mut u8, 0, INJECTION_ARENA_SIZE);
        core::ptr::copy_nonoverlapping(payload.as_ptr(), entry as *mut u8, payload.len());
    }

    unsafe {
        ((arena as u64 + TO_TARGET.buffer) as *mut u32)
            .write_volatile(alloc_shared(FRAME_BUFFER_SIZE) as u32);
        ((arena as u64 + FROM_TARGET.buffer) as *mut u32)
            .write_volatile(alloc_shared(FRAME_BUFFER_SIZE) as u32);
        ((arena + KERNEL_SEMAPHORE) as *mut u32).write_volatile(loop_semaphore());
        ((arena + WAKE_STUB_OFFSET) as *mut u8).write_volatile(0xc2);
        ((arena + WAKE_STUB_OFFSET + 1) as *mut u16).write_volatile(0x0004);
    };

    let stack = alloc_shared(INJECTION_STACK_SIZE) as u32;
    write_trampoline(arena + TRAMPOLINE_OFFSET, entry, stack + INJECTION_STACK_SIZE as u32 - 0x40);

    create_suspended_thread(arena + STUB_OFFSET, arena + TRAMPOLINE_OFFSET, arena, process);
    if !await_flag(arena + THREAD_DATABASE) {
        return Injection { arena, thread: 0, stack };
    }

    let database = unsafe { ((arena + THREAD_DATABASE) as *const u32).read_volatile() };
    let thread = unsafe { (database as *const u32).byte_add(TDB_CONTROL_BLOCK).read() };
    redirect_to_trampoline(arena, thread);

    resume_thread(arena + RESUME_STUB_OFFSET, arena);
    if !await_flag(arena + RUNNING_FLAG) {
        return Injection { arena, thread: 0, stack };
    }
    unsafe {
        ((arena + GATING) as *mut u32).write_volatile(spawns_are_gated() as u32);
        ((arena + GO_FLAG) as *mut u32).write_volatile(1);
    }

    Injection { arena, thread, stack }
}

// The two halves use the same protocol, thus the arena holds complete frames. The copy
// answers as it answers the host, and the kernel half sends these bytes without a change.
pub fn forward_frame(arena: u64, frame: &[u8]) -> bool {
    let arena = arena as u32;
    unsafe { waiting() }.entry(arena).or_default().push_back(frame.to_vec());
    send_what_waits(arena);

    true
}

pub fn serve_waiting_frames() {
    let arenas: alloc::vec::Vec<u32> = unsafe { waiting() }.keys().copied().collect();
    for arena in arenas {
        send_what_waits(arena);
    }
}

pub fn publish_frame_to_host(arena: u64, frame: &[u8]) -> bool {
    write_frame(&FROM_TARGET, arena as u32, frame, signal_kernel_half)
}

pub fn take_frame_from_target(arena: u64) -> Option<alloc::vec::Vec<u8>> {
    read_frame(&FROM_TARGET, arena as u32, wake_copy)
}

pub fn take_frame_from_host(arena: u64) -> Option<alloc::vec::Vec<u8>> {
    read_frame(&TO_TARGET, arena as u32, signal_kernel_half)
}

pub fn holds_a_frame_from_target(arena: u64) -> bool {
    FROM_TARGET.holds_anything(arena)
}

pub fn holds_a_frame_from_host(arena: u64) -> bool {
    TO_TARGET.holds_anything(arena)
}

pub fn a_frame_waits_for_room() -> bool {
    unsafe { waiting() }
        .iter()
        .any(|(arena, frames)| !frames.is_empty() && TO_TARGET.has_room(*arena as u64))
}

fn send_what_waits(arena: u32) {
    let frames = unsafe { waiting() }.get_mut(&arena).unwrap();

    while let Some(frame) = frames.front_mut() {
        TO_TARGET.take_lock(arena as u64, yield_now);
        let piece = TO_TARGET.write(arena as u64, buffer_of(&TO_TARGET, arena), frame, 0);
        if piece.is_none() {
            TO_TARGET.ask_for_room(arena as u64);
        }
        TO_TARGET.let_lock_go(arena as u64);

        let Some(written) = piece else {
            return;
        };
        wake_copy(arena);

        if written == frame.len() {
            frames.pop_front();
        } else {
            frame.drain(..written);
        }
    }
}

fn write_frame(ring: &Ring, arena: u32, frame: &[u8], tell: fn(u32)) -> bool {
    ring.take_lock(arena as u64, yield_now);

    let mut written = 0;
    while written != frame.len() {
        match ring.write(arena as u64, buffer_of(ring, arena), frame, written) {
            Some(now) => {
                written = now;
                tell(arena);
            }
            None => {
                ring.ask_for_room(arena as u64);
                wait(crate::glib::wakeup_token(), None, &mut || ring.has_room(arena as u64));
            }
        }
    }

    ring.let_lock_go(arena as u64);

    true
}

fn read_frame(ring: &Ring, arena: u32, tell: fn(u32)) -> Option<alloc::vec::Vec<u8>> {
    let hold = unsafe { holds() }.entry((arena, ring.head)).or_default();

    loop {
        match ring.read(arena as u64, buffer_of(ring, arena), hold) {
            Taken::Nothing => return None,
            Taken::Piece => {
                if ring.room_is_wanted(arena as u64) {
                    tell(arena);
                }
            }
            Taken::Frame => {
                let frame = core::mem::take(hold);
                if ring.room_is_wanted(arena as u64) {
                    tell(arena);
                }
                return Some(frame);
            }
        }
    }
}

fn wake_asker(arena: u32) {
    let thread = unsafe { ((arena + PATCH_THREAD) as *const u32).read_volatile() };
    if thread == 0 {
        return;
    }

    unsafe { __VWIN32_QueueUserApc(arena + WAKE_STUB_OFFSET, 0, thread) };
}


fn wake_copy(arena: u32) {
    let thread = unsafe { ((arena + LOOP_THREAD) as *const u32).read_volatile() };
    if thread == 0 {
        return;
    }

    unsafe { __VWIN32_QueueUserApc(arena + WAKE_STUB_OFFSET, 0, thread) };
}

pub(crate) fn signal_kernel_half(arena: u32) {
    let semaphore = unsafe { ((arena + KERNEL_SEMAPHORE) as *const u32).read_volatile() };
    if semaphore != 0 {
        unsafe { signal_semaphore(semaphore) };
    }
}

fn buffer_of(ring: &Ring, arena: u32) -> u64 {
    unsafe { ((arena as u64 + ring.buffer) as *const u32).read_volatile() as u64 }
}

fn forget_hold(arena: u32) {
    unsafe { holds() }.retain(|(of, _), _| *of != arena);
    unsafe { waiting() }.remove(&arena);
}

unsafe fn waiting() -> &'static mut BTreeMap<u32, VecDeque<alloc::vec::Vec<u8>>> {
    unsafe { (&raw mut WAITING).as_mut().unwrap() }
}

static mut WAITING: BTreeMap<u32, VecDeque<alloc::vec::Vec<u8>>> = BTreeMap::new();

unsafe fn holds() -> &'static mut BTreeMap<(u32, u64), alloc::vec::Vec<u8>> {
    unsafe { (&raw mut HOLDS).as_mut().unwrap() }
}

static mut HOLDS: BTreeMap<(u32, u64), alloc::vec::Vec<u8>> = BTreeMap::new();

const TO_TARGET: Ring = Ring {
    buffer: 0x1c,
    head: 0x20,
    tail: 0x24,
    lock: 0x28,
    room: 0x84,
    size: FRAME_BUFFER_SIZE,
};

const FROM_TARGET: Ring = Ring {
    buffer: 0x2c,
    head: 0x30,
    tail: 0x34,
    lock: 0x38,
    room: 0x88,
    size: FRAME_BUFFER_SIZE,
};

pub struct Injection {
    pub arena: u32,
    pub thread: u32,
    pub stack: u32,
}

// KERNEL32 makes threads from its own worker in ring 3. Thus an APC to thread -1 runs our
// code, and the target does nothing. CreateThread would use the process of that worker, not
// the target, thus the code calls the internal function, which receives the process. The
// thread starts suspended, which gives time to make the stack available.
fn create_suspended_thread(stub: u32, entry: u32, parameter: u32, process: u32) {
    let mut code = [
        0x6a, 0x68,
        0x68, 0, 0, 0, 0,
        0x68, 0, 0, 0, 0,
        0x68, 0x00, 0x00, 0x01, 0x00,
        0x68, 0, 0, 0, 0,
        0xe8, 0, 0, 0, 0,
        0xa3, 0, 0, 0, 0,
        0xc2, 0x04, 0x00,
    ];
    code[3..7].copy_from_slice(&parameter.to_le_bytes());
    code[8..12].copy_from_slice(&entry.to_le_bytes());
    code[18..22].copy_from_slice(&process.to_le_bytes());
    code[23..27].copy_from_slice(&(thread_creator() - (stub + 27)).to_le_bytes());
    code[28..32].copy_from_slice(&(parameter + THREAD_DATABASE).to_le_bytes());

    unsafe {
        core::ptr::copy_nonoverlapping(code.as_ptr(), stub as *mut u8, code.len());
        __VWIN32_QueueUserApc(stub, 0, ANY_THREAD);
    }
}

fn resume_thread(stub: u32, arena: u32) {
    let mut code = [
        0xff, 0x35, 0, 0, 0, 0,
        0xe8, 0, 0, 0, 0,
        0xc7, 0x05, 0, 0, 0, 0, 0x01, 0x00, 0x00, 0x00,
        0xc2, 0x04, 0x00,
    ];
    code[2..6].copy_from_slice(&(arena + THREAD_DATABASE).to_le_bytes());
    code[7..11].copy_from_slice(&(thread_resumer() - (stub + 11)).to_le_bytes());
    code[13..17].copy_from_slice(&(arena + RESUMED_FLAG).to_le_bytes());

    unsafe {
        core::ptr::copy_nonoverlapping(code.as_ptr(), stub as *mut u8, code.len());
        __VWIN32_QueueUserApc(stub, 0, ANY_THREAD);
    }
}

fn write_trampoline(trampoline: u32, entry: u32, stack_top: u32) {
    let mut code = [
        0x8b, 0x44, 0x24, 0x04,
        0xbc, 0, 0, 0, 0,
        0x50,
        0xba, 0, 0, 0, 0,
        0xff, 0xd2,
        0x6a, 0x00,
        0xba, 0, 0, 0, 0,
        0xff, 0xd2,
    ];
    code[5..9].copy_from_slice(&stack_top.to_le_bytes());
    code[11..15].copy_from_slice(&entry.to_le_bytes());
    code[20..24].copy_from_slice(&kernel32_export(b"ExitThread").to_le_bytes());

    unsafe { core::ptr::copy_nonoverlapping(code.as_ptr(), trampoline as *mut u8, code.len()) };
}

fn redirect_to_trampoline(arena: u32, thread: u32) {
    let mut context = [0u8; CONTEXT_SIZE];
    let base = context.as_mut_ptr();
    unsafe {
        base.add(CONTEXT_FLAGS).cast::<u32>().write_unaligned(CONTEXT_FULL);
        __VWIN32_Get_Thread_Context(thread, base);
    }
    let esp = unsafe { base.add(CONTEXT_ESP).cast::<u32>().read_unaligned() };
    let selector = |slot: u32| unsafe { ((esp + slot) as *const u32).read_volatile() };

    let stack = alloc_shared(PAGE_SIZE as usize) as u32;
    let top = stack + PAGE_SIZE - 0x40;
    unsafe { ((top + 4) as *mut u32).write_volatile(arena) };

    unsafe {
        base.add(CONTEXT_FLAGS).cast::<u32>().write_unaligned(CONTEXT_FULL);
        base.add(CONTEXT_DS).cast::<u32>().write_unaligned(selector(0));
        base.add(CONTEXT_ES).cast::<u32>().write_unaligned(selector(4));
        base.add(CONTEXT_FS).cast::<u32>().write_unaligned(selector(8));
        base.add(CONTEXT_GS).cast::<u32>().write_unaligned(selector(12));
        base.add(CONTEXT_EIP).cast::<u32>().write_unaligned(arena + TRAMPOLINE_OFFSET);
        base.add(CONTEXT_ESP).cast::<u32>().write_unaligned(top);
        __VWIN32_Set_Thread_Context(thread, base);
    }
}

fn await_flag(address: u32) -> bool {
    static mut TOKEN: u8 = 0;
    for _ in 0..INJECTION_ATTEMPTS {
        wait(core::ptr::addr_of!(TOKEN), Some(INJECTION_POLL_US), &mut || false);
        if unsafe { (address as *const u32).read_volatile() } != 0 {
            return true;
        }
    }

    false
}

fn free_shared(address: u32) {
    unsafe { __PageFree(address as *mut u8, 0) };
}

fn alloc_shared(size: usize) -> *mut u8 {
    let pages = size.div_ceil(PAGE_SIZE as usize) as u32;
    let address = unsafe { __PageReserve(PR_SHARED, pages, 0) };
    let committed = unsafe {
        __PageCommit(address / PAGE_SIZE, pages, PD_FIXEDZERO, 0,
            PC_FIXED | PC_PRESENT | PC_USER | PC_WRITEABLE)
    };
    let _ = committed;

    address as *mut u8
}

// KERNEL32 exports neither the creator that receives a process nor the code behind
// ResumeThread. Thus find them by the shape of the code that calls them. The ring 3 worker
// The primitives that differ between the two halves. The kernel half runs on KERNEL, and
// the copy in a process selects USER before it does anything else. The order here is the
// order of the functions in each group.
pub struct Primitives {
    pub alloc: fn(usize) -> *mut u8,
    pub free: fn(*mut u8, usize),
    pub alloc_code: fn(usize) -> *mut u8,
    pub free_code: fn(*mut u8, usize),
    pub spawn_thread: fn(ThreadEntry, *mut c_void) -> isize,
    pub wait: fn(*const u8, Option<u64>, &mut dyn FnMut() -> bool),
    pub wake: fn(*const u8),
    pub yield_now: fn(),
    pub monotonic_micros: fn() -> i64,
    pub current_process_id: fn() -> u32,
    pub current_thread_id: fn() -> u64,
    pub protect: fn(u64, usize, u32) -> bool,
    pub protection_at: fn(usize) -> u32,
    pub enumerate_ranges: fn(&mut dyn FnMut(u64, u64, u32)),
    pub enumerate_threads: fn(&mut dyn FnMut(ThreadInfo), bool),
    pub find_thread: fn(u32, bool) -> Option<ThreadInfo>,
    pub modify_thread: fn(u32, &mut dyn FnMut(&mut CpuState)) -> bool,
}

pub fn select_user() {
    unsafe { ACTIVE = &crate::win9x_user::USER };
}

pub fn in_copy() -> bool {
    core::ptr::eq(primitives(), &crate::win9x_user::USER)
}

fn primitives() -> &'static Primitives {
    unsafe { ACTIVE }
}

static mut ACTIVE: &Primitives = &KERNEL;

static KERNEL: Primitives = Primitives {
    alloc: kernel::alloc,
    free: kernel::free,
    alloc_code: kernel::alloc_code,
    free_code: kernel::free_code,
    spawn_thread: kernel::spawn_thread,
    wait: kernel::wait,
    wake: kernel::wake,
    yield_now: kernel::yield_now,
    monotonic_micros: kernel::monotonic_micros,
    current_process_id: kernel::current_process_id,
    current_thread_id: kernel::current_thread_id,
    protect: kernel::protect,
    protection_at: kernel::protection_at,
    enumerate_ranges: kernel::enumerate_ranges,
    enumerate_threads: kernel::enumerate_threads,
    find_thread: kernel::find_thread,
    modify_thread: kernel::modify_thread,
};

mod kernel {
    use super::*;

    pub fn alloc(size: usize) -> *mut u8 {
        unsafe { __HeapAllocate(size as u32, 0) }
    }

    pub fn free(ptr: *mut u8, _size: usize) {
        unsafe {
            __HeapFree(ptr, 0);
        }
    }

    pub fn alloc_code(size: usize) -> *mut u8 {
        let pages = size.div_ceil(PAGE_SIZE as usize) as u32;
        unsafe { __PageAllocate(pages, PG_SYS, 0, 0, 0, 0, core::ptr::null_mut(), PAGE_FLAGS_CODE) }
    }

    pub fn free_code(ptr: *mut u8, _size: usize) {
        unsafe {
            __PageFree(ptr, 0);
        }
    }

    pub fn spawn_thread(entry: ThreadEntry, parameter: *mut c_void) -> isize {
        unsafe {
            THREAD_ENTRY = Some(entry);
            THREAD_PARAMETER = parameter;

            vwin32_create_ring0_thread(
                THREAD_STACK_SIZE as u32,
                0,
                frida_win9x_thread_thunk as u32,
                0,
            ) as isize
        }
    }

    pub fn wait(token: *const u8, timeout_us: Option<u64>, check: &mut dyn FnMut() -> bool) {
        let slot = slot_for_token(token);

        let semaphore = semaphore_in(slot);

        if WOKEN_EARLY[slot].swap(0, Ordering::AcqRel) != 0 {
            return;
        }
        if check() {
            return;
        }

        SLEEPERS[slot].fetch_add(1, Ordering::AcqRel);
        match timeout_us {
            None => unsafe { wait_semaphore(semaphore, BLOCK_SVC_INTS) },
            Some(us) => block_until_signalled_or_timed_out(semaphore, us),
        }
        SLEEPERS[slot].fetch_sub(1, Ordering::AcqRel);
    }

    pub fn wake(token: *const u8) {
        if unsafe { super::IN_INTERRUPT } {
            super::WAKE_WANTED.store(token as usize, Ordering::Release);
            unsafe { schedule_global_event(frida_win9x_wake_event_thunk) };

            return;
        }

        let slot = slot_for_token(token);

        let semaphore = SEMAPHORES[slot].load(Ordering::Acquire);
        if semaphore == 0 {
            WOKEN_EARLY[slot].store(1, Ordering::Release);
            return;
        }

        let mut sleepers = SLEEPERS[slot].load(Ordering::Acquire).max(1);
        while sleepers != 0 {
            unsafe { signal_semaphore(semaphore) };
            sleepers -= 1;
        }
    }

    pub fn yield_now() {
        unsafe {
            _Release_Time_Slice();
        }
    }

    pub fn monotonic_micros() -> i64 {
        unsafe { _Get_System_Time() as i64 * 1000 }
    }

    pub fn current_process_id() -> u32 {
        0
    }

    pub fn current_thread_id() -> u64 {
        unsafe { get_cur_thread_handle() as u64 }
    }

    pub fn enumerate_threads(found: &mut dyn FnMut(ThreadInfo), with_registers: bool) {
        super::enumerate_ring_zero_threads(found, with_registers)
    }

    pub fn find_thread(id: u32, with_registers: bool) -> Option<ThreadInfo> {
        let thread = super::ring_zero_thread(id)?;

        Some(ThreadInfo {
            id,
            cpu_state: with_registers.then(|| super::thread_cpu_state(thread)).flatten(),
        })
    }

    pub fn modify_thread(id: u32, change: &mut dyn FnMut(&mut CpuState)) -> bool {
        let Some(thread) = super::ring_zero_thread(id) else {
            return false;
        };

        super::modify_thread_at(thread, change)
    }

    pub fn enumerate_ranges(found: &mut dyn FnMut(u64, u64, u32)) {
        let mut base = 0usize;
        let mut size = 0usize;
        let mut protection = 0u32;

        let mut address = ARENA_START;
        while address != ARENA_END {
            let here = protection_at(address);

            if here != protection || base + size != address {
                if protection != 0 {
                    found(base as u64, size as u64, protection);
                }
                base = address;
                size = 0;
                protection = here;
            }
            size += PAGE_SIZE as usize;

            address += PAGE_SIZE as usize;
        }

        if protection != 0 {
            found(base as u64, size as u64, protection);
        }
    }

    pub fn protection_at(address: usize) -> u32 {
        let mut entry: u32 = 0;
        let copied = unsafe {
            __CopyPageTable((address / PAGE_SIZE as usize) as u32, 1, &mut entry, 0)
        };
        if copied == 0 {
            return 0;
        }

        protection_of(entry)
    }

    pub fn protect(address: u64, size: usize, gum_prot: u32) -> bool {
        let first_page = address / PAGE_SIZE as u64;
        let pages = size.div_ceil(PAGE_SIZE as usize) as u32;

        let mut set = PAGE_PRESENT;
        if (gum_prot & GUM_PAGE_WRITE) != 0 {
            set |= PAGE_WRITEABLE;
        }
        let clear = PAGE_WRITEABLE & !set;

        unsafe { __PageModifyPermissions(first_page as u32, pages, !clear, set) != 0xffff_ffff }
    }
}

// for _VWIN32_CreateRing0Thread calls the creator with the same five arguments.
fn thread_creator() -> u32 {
    let cached = unsafe { core::ptr::addr_of!(THREAD_CREATOR).read() };
    if cached != 0 {
        return cached;
    }

    let base = unsafe { core::ptr::addr_of!(_KERNEL32_Base).read() };
    let last = base + image_size(base) - CREATE_CALL_SITE.len() as u32;
    let mut address = base;
    while address != last {
        if read_u8(address) == 0x6a && read_u8(address + 1) == 0x28 && matches_call_site(address) {
            let found = (address + CREATE_CALL_SITE.len() as u32)
                .wrapping_add(read_u32(address + 21));
            unsafe { THREAD_CREATOR = found };
            return found;
        }
        address += 1;
    }

    0
}

fn matches_call_site(address: u32) -> bool {
    for (offset, expected) in CREATE_CALL_SITE.iter().enumerate() {
        if *expected != 0 && read_u8(address + offset as u32) != *expected {
            return false;
        }
    }

    true
}

// ResumeThread gives the thread database to it and examines the result for -1.
fn thread_resumer() -> u32 {
    let cached = unsafe { core::ptr::addr_of!(THREAD_RESUMER).read() };
    if cached != 0 {
        return cached;
    }

    let entry = kernel32_export(b"ResumeThread");
    let mut address = entry;
    while address != entry + RESUME_SEARCH_RANGE {
        if read_u8(address) == 0x57
            && read_u8(address + 1) == 0xe8
            && read_u8(address + 6) == 0x83
            && read_u8(address + 7) == 0xf8
            && read_u8(address + 8) == 0xff
        {
            let found = (address + 6).wrapping_add(read_u32(address + 2));
            unsafe { THREAD_RESUMER = found };
            return found;
        }
        address += 1;
    }

    0
}

pub(crate) fn image_size(base: u32) -> u32 {
    read_u32(base + read_u32(base + DOS_HEADERS_OFFSET) + IMAGE_SIZE_OFFSET)
}

static mut THREAD_CREATOR: u32 = 0;
static mut THREAD_RESUMER: u32 = 0;

// push 0x28; push esi; push imm32; push [esi+8]; push [imm32]; mov edi,[esi+0x10]; call rel32
const CREATE_CALL_SITE: [u8; 25] = [
    0x6a, 0x28, 0x56, 0x68, 0, 0, 0, 0, 0xff, 0x76, 0x08, 0xff, 0x35, 0, 0, 0, 0, 0x8b, 0x7e,
    0x10, 0xe8, 0, 0, 0, 0,
];
const RESUME_SEARCH_RANGE: u32 = 0x60;
const DOS_HEADERS_OFFSET: u32 = 0x3c;
const IMAGE_SIZE_OFFSET: u32 = 0x50;

// The host cannot resolve these, because parts of the export names of KERNEL32 are out of
// memory. Only the guest can get these pages again.
// A module says what it exports in its own headers, thus a copy can read them for the modules
// of the process it runs in.
pub(crate) fn enumerate_exports(base: u32, found: &mut dyn FnMut(*const u8, u64) -> bool) {
    let headers = base + read_u32(base + DOS_HEADERS_OFFSET);
    let directory_rva = read_u32(headers + EXPORT_DIRECTORY_OFFSET);
    if directory_rva == 0 {
        return;
    }

    let directory = base + directory_rva;
    let count = read_u32(directory + 0x18);
    let functions = base + read_u32(directory + 0x1c);
    let names = base + read_u32(directory + 0x20);
    let ordinals = base + read_u32(directory + 0x24);

    for index in 0..count {
        let ordinal = read_u16(ordinals + index * 2) as u32;
        let address = base + read_u32(functions + ordinal * 4);
        if !found((base + read_u32(names + index * 4)) as *const u8, address as u64) {
            break;
        }
    }
}

pub fn kernel32_export(wanted: &[u8]) -> u32 {
    let base = unsafe { core::ptr::addr_of!(_KERNEL32_Base).read() };
    let headers = base + read_u32(base + DOS_HEADERS_OFFSET);
    let directory = base + read_u32(headers + EXPORT_DIRECTORY_OFFSET);
    let count = read_u32(directory + 0x18);
    let functions = base + read_u32(directory + 0x1c);
    let names = base + read_u32(directory + 0x20);
    let ordinals = base + read_u32(directory + 0x24);

    let mut lowest = 0;
    let mut highest = count;
    while lowest < highest {
        let middle = (lowest + highest) / 2;
        match compare_export(base + read_u32(names + middle * 4), wanted) {
            core::cmp::Ordering::Equal => {
                let ordinal = read_u16(ordinals + middle * 2) as u32;
                return base + read_u32(functions + ordinal * 4);
            }
            core::cmp::Ordering::Less => lowest = middle + 1,
            core::cmp::Ordering::Greater => highest = middle,
        }
    }

    0
}

fn compare_export(name: u32, wanted: &[u8]) -> core::cmp::Ordering {
    for (index, expected) in wanted.iter().enumerate() {
        let actual = read_u8(name + index as u32);
        if actual != *expected {
            return actual.cmp(expected);
        }
    }

    read_u8(name + wanted.len() as u32).cmp(&0)
}

fn read_u32(address: u32) -> u32 {
    unsafe { (address as *const u32).read_unaligned() }
}

fn read_u16(address: u32) -> u16 {
    unsafe { (address as *const u16).read_unaligned() }
}

fn read_u8(address: u32) -> u8 {
    unsafe { (address as *const u8).read() }
}

const EXPORT_DIRECTORY_OFFSET: u32 = 0x78;

pub(crate) const MEM_COMMIT: u32 = 0x0000_1000;
pub(crate) const MEM_RESERVE: u32 = 0x0000_2000;
pub(crate) const MEM_RELEASE: u32 = 0x0000_8000;
pub(crate) const PAGE_EXECUTE_READ: u32 = 0x20;
pub(crate) const PAGE_EXECUTE_READWRITE: u32 = 0x40;
pub(crate) const INFINITE: u32 = 0xffff_ffff;
pub(crate) const USER_THREAD_STACK_SIZE: u32 = 0x10000;

const INJECTION_ARENA_SIZE: usize = 0x1000;
const HANDSHAKE_SIZE: usize = 0x40;
pub(crate) const RUNNING_FLAG: u32 = 0x00;
pub(crate) const GO_FLAG: u32 = 0x04;
pub(crate) const OBSERVED_PID: u32 = 0x08;
const THREAD_DATABASE: u32 = 0x14;
const RESUMED_FLAG: u32 = 0x18;
pub(crate) const STOP_REQUEST: u32 = 0x0c;
pub(crate) const MAIN_STOPPED: u32 = 0x10;
pub(crate) const WORKER_STOPPED: u32 = 0x3c;
pub(crate) const MODULE_LIST: u32 = 0x40;
pub(crate) const PATCH_PROCESS: u32 = 0x4c;
pub(crate) const PATCH_ADDRESS: u32 = 0x50;
pub(crate) const PATCH_BYTES: u32 = 0x54;
pub(crate) const PATCH_REQUEST: u32 = 0x58;
pub(crate) const GATING: u32 = 0x9c;
pub(crate) const HOLD_RESULT: u32 = 0xa0;
pub(crate) const LOOP_THREAD: u32 = 0xa4;
pub(crate) const KERNEL_SEMAPHORE: u32 = 0xa8;
pub(crate) const PATCH_THREAD: u32 = 0xac;
pub(crate) const GONE_HEAD: u32 = 0xb0;
pub(crate) const GONE_TAIL: u32 = 0xb4;
pub(crate) const GONE_SLOTS: u32 = 0xb8;
pub(crate) const IMAGE_BASE: u32 = 0xd8;
pub(crate) const IMAGE_SIZE: u32 = 0xdc;
pub(crate) const GONE_COUNT: u32 = 8;
pub(crate) const PATCH_ASKED: u32 = 1;
pub(crate) const HOOK_ASKED: u32 = 3;
pub(crate) const PATCH_CODE: u32 = 0x64;
pub(crate) const PATCH_CODE_MAX: usize = 32;
pub(crate) const PATCH_LENGTH: u32 = 0x60;
pub(crate) const PATCH_ANSWERED: u32 = 2;
const PATCH_SIZE: usize = 2;
const PE_SIGNATURE_OFFSET: usize = 0x3c;
const PE_ENTRY_POINT_OFFSET: usize = 0x28;
const FRAME_BUFFER_SIZE: usize = 0x4000;
const TEARDOWN_GRACE_US: u64 = 500_000;
const TDB_CONTROL_BLOCK: usize = 0x5c;
const IDLE_SLEEP_MS: u32 = 1000;
pub(crate) const PARKED_SLEEP_MS: u32 = 200;
const STUB_OFFSET: u32 = 0x100;
const PAYLOAD_OFFSET: u32 = 0x200;
const INJECTION_ATTEMPTS: u32 = 100;
const INJECTION_POLL_US: u64 = 100_000;
const ANY_THREAD: u32 = 0xffff_ffff;
const RESUME_STUB_OFFSET: u32 = 0x140;
const WAKE_STUB_OFFSET: u32 = 0x1c0;
const TRAMPOLINE_OFFSET: u32 = 0x180;

const INJECTION_STACK_SIZE: usize = 0x4000;
const THREAD_BLOCK_SLOT: u32 = 0xc002_11cc;
const PDB_MEMORY_CONTEXT: usize = 0x1c;
const PR_SHARED: u32 = 0x8006_0000;
const PD_FIXEDZERO: u32 = 0x3;
const PC_FIXED: u32 = 0x0000_0008;
const PC_WRITEABLE: u32 = 0x0002_0000;
const PC_USER: u32 = 0x0004_0000;
const PC_PRESENT: u32 = 0x8000_0000;

pub fn monotonic_micros() -> i64 {
    (primitives().monotonic_micros)()
}

pub fn wall_clock_micros() -> (u32, u32) {
    let micros = monotonic_micros() as u64;
    ((micros / 1_000_000) as u32, (micros % 1_000_000) as u32)
}

// Ring 0 is in no process, thus only the injected copy has an answer.
pub fn current_process_id() -> u32 {
    (primitives().current_process_id)()
}

pub fn current_thread_id() -> u64 {
    (primitives().current_thread_id)()
}

pub fn protect(address: u64, size: usize, gum_prot: u32) -> bool {
    (primitives().protect)(address, size, gum_prot)
}

// This kernel has no self-map, but VMM copies a run of page table entries into a buffer,
// which gives the same data. Report only the arena, because the memory below it belongs to
// the VM that is current.
pub fn enumerate_ranges(found: &mut dyn FnMut(u64, u64, u32)) {
    (primitives().enumerate_ranges)(found)
}

pub fn protection_at(address: usize) -> u32 {
    (primitives().protection_at)(address)
}

// These processors have no execute permission, thus all present pages are executable.
fn protection_of(entry: u32) -> u32 {
    if (entry & PAGE_PRESENT) == 0 {
        return 0;
    }

    let mut prot = GUM_PAGE_READ | GUM_PAGE_EXECUTE;
    if (entry & PAGE_WRITEABLE) != 0 {
        prot |= GUM_PAGE_WRITE;
    }
    prot
}

const ARENA_START: usize = 0xc000_0000;
const ARENA_END: usize = 0xc400_0000;
const GUM_PAGE_READ: u32 = 0x1;
const GUM_PAGE_EXECUTE: u32 = 0x4;

pub fn map_io(phys_addr: u64, size: u64) -> *mut c_void {
    let pages = (size as usize).div_ceil(PAGE_SIZE as usize) as u32;
    unsafe { __MapPhysToLinear(phys_addr as u32, pages * PAGE_SIZE, 0) as *mut c_void }
}

pub fn virt_to_phys(vaddr: u64) -> u64 {
    let mut entry: u32 = 0;
    let page = (vaddr / PAGE_SIZE as u64) as u32;
    unsafe {
        __CopyPageTable(page, 1, &mut entry, 0);
    }
    ((entry & !(PAGE_SIZE - 1)) as u64) | (vaddr & (PAGE_SIZE - 1) as u64)
}

pub fn install_interrupt_handler(
    irq: u32,
    target: *mut c_void,
    handler: InterruptHandler,
    refcon: *mut c_void,
) -> i32 {
    unsafe {
        HW_INT_HANDLER = Some(handler);
        HW_INT_TARGET = target;
        HW_INT_REFCON = refcon;
    }

    unsafe {
        IRQ_DESCRIPTOR = VpicdIrqDescriptor {
            irq_number: irq as u16,
            options: VPICD_OPT_REF_DATA | VPICD_OPT_CAN_SHARE,
            hw_int_proc: frida_win9x_hw_int_thunk,
            virt_int_proc: 0,
            eoi_proc: 0,
            mask_change_proc: 0,
            iret_proc: 0,
            iret_time_out: VPICD_DEFAULT_IRET_TIME_OUT,
            hw_int_ref: 0,
        };
    }

    let handle = unsafe { vpicd_virtualize_irq(core::ptr::addr_of_mut!(IRQ_DESCRIPTOR)) };
    if handle != 0 {
        unsafe {
            IRQ_HANDLE = handle;
            vpicd_physically_unmask(handle);
        }
    }
    if handle == 0 { -1 } else { 0 }
}

pub fn enumerate_threads(found: &mut dyn FnMut(ThreadInfo), with_registers: bool) {
    (primitives().enumerate_threads)(found, with_registers)
}

pub fn find_thread(id: u32, with_registers: bool) -> Option<ThreadInfo> {
    (primitives().find_thread)(id, with_registers)
}

pub fn modify_thread(id: u32, change: &mut dyn FnMut(&mut CpuState)) -> bool {
    (primitives().modify_thread)(id, change)
}

pub fn modify_thread_at(thread: u32, change: &mut dyn FnMut(&mut CpuState)) -> bool {
    let mut context = [0u8; CONTEXT_SIZE];
    let base = context.as_mut_ptr();

    unsafe {
        base.add(CONTEXT_FLAGS).cast::<u32>().write_unaligned(CONTEXT_FULL);

        if __VWIN32_Get_Thread_Context(thread, base) == 0 {
            return false;
        }

        let mut state = cpu_state_of(base);
        change(&mut state);
        write_cpu_state(base, &state);

        base.add(CONTEXT_FLAGS).cast::<u32>().write_unaligned(CONTEXT_FULL);

        __VWIN32_Set_Thread_Context(thread, base) != 0
    }
}

pub fn thread_handle_of(pid: u32, id: u32) -> Option<u32> {
    let mut found = None;
    enumerate_thread_handles_of(pid, &mut |thread, thread_id| {
        if thread_id == id {
            found = Some(thread);
        }
    });

    found
}

// The threads of one process, as the copy in it sees them. Ring 3 of this system reaches both
// the services and the memory the kernel keeps, thus the copy walks the same list the kernel
// half does and keeps what belongs to it, registers and all.
pub fn enumerate_threads_of(pid: u32, found: &mut dyn FnMut(ThreadInfo), with_registers: bool) {
    enumerate_thread_handles_of(pid, &mut |thread, id| {
        found(ThreadInfo {
            id,
            cpu_state: with_registers.then(|| thread_cpu_state(thread)).flatten(),
        });
    });
}

fn enumerate_thread_handles_of(pid: u32, found: &mut dyn FnMut(u32, u32)) {
    let slot = unsafe { (THREAD_BLOCK_SLOT as *const u32).read() };
    let vm = unsafe { get_sys_vm_handle() };
    let first = unsafe { get_initial_thread_handle(vm) };

    let mut thread = first;
    while thread != 0 {
        if unsafe { (thread as *const u32).add(0x2c / 4).read() } == WIN32_THREAD {
            let block = unsafe { (thread as *const u32).byte_add(slot as usize).read() };
            let database = unsafe { (block as *const u32).read() };
            let process = unsafe { (block as *const u32).add(1).read() };
            if process_id(process) == pid {
                found(thread, process_id(database));
            }
        }

        let next = unsafe { get_next_thread_handle(thread) };
        thread = if next == first { 0 } else { next };
    }
}

fn ring_zero_thread(id: u32) -> Option<u32> {
    let vm = unsafe { get_sys_vm_handle() };
    let first = unsafe { get_initial_thread_handle(vm) };

    let mut thread = first;
    while thread != 0 {
        if thread == id {
            return Some(thread);
        }

        let next = unsafe { get_next_thread_handle(thread) };
        thread = if next == first { 0 } else { next };
    }

    None
}

fn enumerate_ring_zero_threads(found: &mut dyn FnMut(ThreadInfo), with_registers: bool) {
    let vm = unsafe { get_sys_vm_handle() };
    let first = unsafe { get_initial_thread_handle(vm) };

    let mut thread = first;
    while thread != 0 {
        found(ThreadInfo {
            id: thread,
            cpu_state: with_registers.then(|| thread_cpu_state(thread)).flatten(),
        });

        let next = unsafe { get_next_thread_handle(thread) };
        thread = if next == first { 0 } else { next };
    }
}

// VWIN32 gives the registers of Win32 threads only, and it fails for the others.
pub fn thread_cpu_state(thread: u32) -> Option<CpuState> {
    let mut context = [0u8; CONTEXT_SIZE];
    let base = context.as_mut_ptr();

    unsafe {
        base.add(CONTEXT_FLAGS).cast::<u32>().write_unaligned(CONTEXT_FULL);

        if __VWIN32_Get_Thread_Context(thread, base) == 0 {
            return None;
        }

        Some(cpu_state_of(base))
    }
}

pub(crate) unsafe fn cpu_state_of(base: *const u8) -> CpuState {
    let field = |offset: usize| unsafe { base.add(offset).cast::<u32>().read_unaligned() };

    CpuState {
        eip: field(CONTEXT_EIP),
        edi: field(CONTEXT_EDI),
        esi: field(CONTEXT_ESI),
        ebp: field(CONTEXT_EBP),
        esp: field(CONTEXT_ESP),
        ebx: field(CONTEXT_EBX),
        edx: field(CONTEXT_EDX),
        ecx: field(CONTEXT_ECX),
        eax: field(CONTEXT_EAX),
    }
}

pub(crate) unsafe fn write_cpu_state(base: *mut u8, state: &CpuState) {
    let mut field = |offset: usize, value: u32| unsafe {
        base.add(offset).cast::<u32>().write_unaligned(value)
    };

    field(CONTEXT_EIP, state.eip);
    field(CONTEXT_EDI, state.edi);
    field(CONTEXT_ESI, state.esi);
    field(CONTEXT_EBP, state.ebp);
    field(CONTEXT_ESP, state.esp);
    field(CONTEXT_EBX, state.ebx);
    field(CONTEXT_EDX, state.edx);
    field(CONTEXT_ECX, state.ecx);
    field(CONTEXT_EAX, state.eax);
}

const CONTEXT_SIZE: usize = 0xcc;
const CONTEXT_FULL: u32 = 0x0001_0007;
const CONTEXT_FLAGS: usize = 0x00;
const CONTEXT_GS: usize = 0x8c;
const CONTEXT_FS: usize = 0x90;
const CONTEXT_ES: usize = 0x94;
const CONTEXT_DS: usize = 0x98;
const CONTEXT_EDI: usize = 0x9c;
const CONTEXT_ESI: usize = 0xa0;
const CONTEXT_EBX: usize = 0xa4;
const CONTEXT_EDX: usize = 0xa8;
const CONTEXT_ECX: usize = 0xac;
const CONTEXT_EAX: usize = 0xb0;
const CONTEXT_EBP: usize = 0xb4;
const CONTEXT_EIP: usize = 0xb8;
const CONTEXT_ESP: usize = 0xc4;

pub struct ProcessInfo {
    pub id: u32,
    pub path: *const u8,
    pub command_line: *const u8,
}

// Win32 threads carry the process they belong to in VWIN32's per-thread block, so the
// process list is the deduplicated set of those, and the image path is where the command
// line starts.
pub fn enumerate_processes(found: &mut dyn FnMut(ProcessInfo)) {
    let slot = unsafe { (THREAD_BLOCK_SLOT as *const u32).read() };
    let vm = unsafe { get_sys_vm_handle() };
    let first = unsafe { get_initial_thread_handle(vm) };

    let mut seen: [u32; 64] = [0; 64];
    let mut count = 0usize;
    let mut thread = first;
    while thread != 0 && count < seen.len() {
        if unsafe { (thread as *const u32).add(0x2c / 4).read() } == WIN32_THREAD {
            let block = unsafe { (thread as *const u32).byte_add(slot as usize).read() };
            let pdb = unsafe { (block as *const u32).add(1).read() };
            if !seen[..count].contains(&pdb) {
                seen[count] = pdb;
                count += 1;
                found(ProcessInfo {
                    id: process_id(pdb),
                    path: image_path(pdb),
                    command_line: command_line(pdb),
                });
            }
        }

        let next = unsafe { get_next_thread_handle(thread) };
        thread = if next == first { 0 } else { next };
    }
}

// KERNEL32 hands out its process database pointer XOR a per-boot value, and so must we.
pub(crate) fn process_database(pid: u32) -> u32 {
    process_id(pid)
}

fn process_id(pdb: u32) -> u32 {
    let slot = unsafe { core::ptr::addr_of!(_KERNEL32_ProcessIdObfuscator).read() };
    if slot == 0 {
        return pdb;
    }

    pdb ^ unsafe { (slot as *const u32).read() }
}

// What a process has mapped is written down in memory that only ring 0 can read. Thus the copy
// gets the list in memory that its own ring can read.
fn publish_module_list(pdb: u32) -> u32 {
    let table = module_table();
    if table == 0 {
        return 0;
    }

    let first = unsafe { (pdb as *const u32).byte_add(PDB_MODREF_OFFSET).read() };
    if first < ARENA_FLOOR {
        return 0;
    }

    // The list runs both ways from the entry that the process keeps, thus follow both. An image
    // of the process is mapped in the space of that process alone, thus read it from there.
    let mut modules = alloc::vec::Vec::new();
    let context = unsafe { (pdb as *const u32).byte_add(PDB_MEMORY_CONTEXT).read() };
    let saved = unsafe { __GetCurrentContext() };
    unsafe { __ContextSwitch(context) };
    collect_modules(table, first, MODREF_NEXT_OFFSET, &mut modules);
    let previous = unsafe { (first as *const u32).byte_add(MODREF_PREVIOUS_OFFSET).read() };
    collect_modules(table, previous, MODREF_PREVIOUS_OFFSET, &mut modules);

    let mut list = alloc::vec::Vec::new();
    list.extend_from_slice(&(modules.len() as u32).to_le_bytes());
    for module in modules.iter() {
        list.extend_from_slice(&module.base.to_le_bytes());
        list.extend_from_slice(&module.size.to_le_bytes());
        write_text(&mut list, module.path);
    }
    unsafe { __ContextSwitch(saved) };

    let published = alloc_shared(list.len());
    unsafe { core::ptr::copy_nonoverlapping(list.as_ptr(), published, list.len()) };

    published as u32
}

fn collect_modules(table: u32, first: u32, step: usize, modules: &mut alloc::vec::Vec<ListedModule>) {
    let mut entry = first;
    while entry >= ARENA_FLOOR {
        let index = unsafe { (entry as *const u16).byte_add(MODREF_MTE_INDEX_OFFSET).read() };
        let row = unsafe { (table as *const u32).add(index as usize).read() };
        if row >= ARENA_FLOOR {
            let base = unsafe { (row as *const u32).byte_add(IMTE_BASE_OFFSET).read() };
            let path = unsafe { (row as *const u32).byte_add(IMTE_FILE_NAME_OFFSET).read() };
            if base >= ARENA_FLOOR && path >= ARENA_FLOOR {
                modules.push(ListedModule {
                    base,
                    size: image_size(base),
                    path: path as *const u8,
                });
            }
        }

        entry = unsafe { (entry as *const u32).byte_add(step).read() };
    }
}

fn write_text(list: &mut alloc::vec::Vec<u8>, text: *const u8) {
    let mut cursor = text;
    let start = list.len();
    list.extend_from_slice(&0u32.to_le_bytes());
    loop {
        let byte = unsafe { cursor.read() };
        if byte == 0 {
            break;
        }
        list.push(byte);
        cursor = unsafe { cursor.add(1) };
    }

    let length = (list.len() - start - 4) as u32;
    list[start..start + 4].copy_from_slice(&length.to_le_bytes());
}

struct ListedModule {
    base: u32,
    size: u32,
    path: *const u8,
}

pub(crate) fn module_table() -> u32 {
    let slot = unsafe { core::ptr::addr_of!(_KERNEL32_ModuleTable).read() };
    if slot == 0 {
        return 0;
    }

    let table = unsafe { (slot as *const u32).read() };
    if table < ARENA_FLOOR {
        return 0;
    }

    table
}

fn command_line(pdb: u32) -> *const u8 {
    let env_db = unsafe { (pdb as *const u32).byte_add(PDB_ENVIRONMENT_OFFSET).read() };
    if env_db < ARENA_FLOOR {
        return core::ptr::null();
    }

    let text = unsafe { (env_db as *const u32).byte_add(ENVIRONMENT_COMMAND_LINE_OFFSET).read() };
    if text < ARENA_FLOOR {
        return core::ptr::null();
    }

    text as *const u8
}

// The command line would do for most processes, but it is empty for the ones Windows starts
// itself and can carry arguments besides. KERNEL32 names a module the way GetModuleFileName
// does: the process's own MODREF, indexed into the module table.
fn image_path(pdb: u32) -> *const u8 {
    if is_win16_task(pdb) {
        let program = program_of(command_line(pdb));
        if !program.is_null() && unsafe { program.read() } != 0 {
            return program;
        }
    }

    let table = module_table();
    if table == 0 {
        return core::ptr::null();
    }

    let modref = unsafe { (pdb as *const u32).byte_add(PDB_MODREF_OFFSET).read() };
    if modref < ARENA_FLOOR {
        return core::ptr::null();
    }

    let index = unsafe { (modref as *const u16).byte_add(MODREF_MTE_INDEX_OFFSET).read() };
    let entry = unsafe { (table as *const u32).add(index as usize).read() };
    if entry < ARENA_FLOOR {
        return core::ptr::null();
    }

    let path = unsafe { (entry as *const u32).byte_add(IMTE_FILE_NAME_OFFSET).read() };
    if path < ARENA_FLOOR {
        return core::ptr::null();
    }

    path as *const u8
}

const WIN32_THREAD: u32 = 0x2a;
const PDB_FLAGS_OFFSET: usize = 0x20;
const WIN16_TASK: u32 = 0x08;
pub(crate) const NOTHING_TO_HOLD: u32 = 0xffff_ffff;
pub(crate) const ARENA_FLOOR: u32 = 0x10000;
pub(crate) const PDB_MODREF_OFFSET: usize = 0x94;
const PDB_ENVIRONMENT_OFFSET: usize = 0x40;
const ENVIRONMENT_COMMAND_LINE_OFFSET: usize = 0x08;
pub(crate) const MODREF_MTE_INDEX_OFFSET: usize = 0x10;
pub(crate) const MODREF_NEXT_OFFSET: usize = 0x00;
pub(crate) const MODREF_PREVIOUS_OFFSET: usize = 0x04;
pub(crate) const IMTE_BASE_OFFSET: usize = 0x24;
pub(crate) const IMTE_FILE_NAME_OFFSET: usize = 0x0c;

pub fn identify_image(path: *const u8) -> *const u8 {
    let identity = unsafe { (&raw mut IDENTITY).as_mut().unwrap() };
    identity[0] = 0;

    let Some(file) = File::open(path) else {
        return identity.as_ptr();
    };

    let written = crate::icons::identify(&file, &mut identity[..MAX_IDENTITY]);
    identity[written] = 0;

    identity.as_ptr()
}

static mut IDENTITY: [u8; MAX_IDENTITY + 1] = [0; MAX_IDENTITY + 1];
const MAX_IDENTITY: usize = 128;

pub fn describe_process(process: &ProcessInfo) -> *const u8 {
    if process.path.is_null() {
        return c"".as_ptr() as *const u8;
    }

    describe_image(process.path)
}

pub fn describe_image(path: *const u8) -> *const u8 {
    let description = unsafe { (&raw mut DESCRIPTION).as_mut().unwrap() };
    description[0] = 0;

    let Some(file) = File::open(path) else {
        return description.as_ptr();
    };

    let written = crate::icons::describe(&file, &mut description[..MAX_DESCRIPTION]);
    description[written] = 0;

    description.as_ptr()
}

static mut DESCRIPTION: [u8; MAX_DESCRIPTION + 1] = [0; MAX_DESCRIPTION + 1];
const MAX_DESCRIPTION: usize = 128;

pub fn enumerate_applications(found: &mut dyn FnMut(&[u8], &[u8])) {
    let mut apps = 0;
    if unsafe { __RegOpenKey(HKEY_LOCAL_MACHINE, APP_PATHS.as_ptr(), &raw mut apps) } != 0 {
        return;
    }

    let mut index = 0;
    loop {
        let mut identifier = [0u8; MAX_KEY_NAME];
        let taken = unsafe {
            __RegEnumKey(apps, index, identifier.as_mut_ptr(), identifier.len() as u32)
        };
        if taken != 0 {
            break;
        }
        index += 1;

        let mut app = 0;
        if unsafe { __RegOpenKey(apps, identifier.as_ptr(), &raw mut app) } != 0 {
            continue;
        }

        let mut path = [0u8; MAX_PATH];
        let mut length = path.len() as u32;
        let read = unsafe {
            __RegQueryValueEx(app, core::ptr::null(), core::ptr::null_mut(),
                core::ptr::null_mut(), path.as_mut_ptr(), &raw mut length)
        };
        unsafe { __RegCloseKey(app) };

        if read == 0 {
            found(text_of(&identifier), text_of(&path));
        }
    }

    unsafe { __RegCloseKey(apps) };
}

fn text_of(bytes: &[u8]) -> &[u8] {
    let end = bytes.iter().position(|b| *b == 0).unwrap_or(bytes.len());

    &bytes[..end]
}

const HKEY_LOCAL_MACHINE: u32 = 0x8000_0002;
const APP_PATHS: &[u8] = b"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\0";
const MAX_KEY_NAME: usize = 64;
const MAX_PATH: usize = 260;

pub fn enumerate_icons(path: *const u8, found: &mut dyn FnMut(&[u8])) {
    let Some(file) = File::open(path) else {
        return;
    };

    crate::icons::enumerate(&file, found);
}

struct File {
    handle: u32,
}

impl File {
    fn open(path: *const u8) -> Option<File> {
        let handle = unsafe {
            ifsmgr_ring0_file_io(
                R0_OPENCREATFILE,
                0,
                0,
                ACTION_OPENEXISTING,
                path as u32,
                ACCESS_READONLY | SHARE_DENYNONE,
            )
        };
        if handle == 0 {
            return None;
        }

        Some(File { handle })
    }
}

impl crate::icons::Image for File {
    fn read_at(&self, position: u32, buffer: &mut [u8]) -> u32 {
        unsafe {
            ifsmgr_ring0_file_io(
                R0_READFILE,
                self.handle,
                buffer.len() as u32,
                position,
                buffer.as_mut_ptr() as u32,
                0,
            )
        }
    }
}

impl Drop for File {
    fn drop(&mut self) {
        unsafe { ifsmgr_ring0_file_io(R0_CLOSEFILE, self.handle, 0, 0, 0, 0) };
    }
}

const R0_OPENCREATFILE: u32 = 0xd500;
const R0_READFILE: u32 = 0xd600;
const R0_CLOSEFILE: u32 = 0xd700;
const ACCESS_READONLY: u32 = 0x0000;
const SHARE_DENYNONE: u32 = 0x0040;
const ACTION_OPENEXISTING: u32 = 0x01;

pub fn watches_threads() -> bool {
    !in_copy()
}

pub fn watch_threads(appeared: fn(u32), vanished: fn(u32)) {
    unsafe {
        THREAD_APPEARED = Some(appeared);
        THREAD_VANISHED = Some(vanished);
    }

    hear_from_vmm();
}

pub fn forget_threads() {
    unsafe {
        THREAD_APPEARED = None;
        THREAD_VANISHED = None;
    }
}

pub fn hear_from_vmm() {
    unsafe {
        if HEARING {
            return;
        }
        HEARING = true;

        let ddb = (&raw mut DEVICE).as_mut().unwrap();
        ddb[DDB_SDK_VERSION / 4] = SDK_VERSION;
        ddb[DDB_NAME / 4] = u32::from_le_bytes(*b"FRID");
        ddb[DDB_NAME / 4 + 1] = u32::from_le_bytes(*b"A   ");
        ddb[DDB_CONTROL_PROC / 4] = frida_win9x_control_thunk as u32;

        vmm_add_ddb(ddb.as_mut_ptr() as u32);
    }
}

pub fn stop_hearing_from_vmm() {
    unsafe {
        if !HEARING {
            return;
        }
        HEARING = false;

        let ddb = (&raw mut DEVICE).as_mut().unwrap();
        vmm_remove_ddb(ddb.as_mut_ptr() as u32);
    }
}

static mut HEARING: bool = false;


// What a new thread is to run, and the value to hand it, wait on its own stack: the stub returns
// to the starter of KERNEL32, which reads them from there. The thunk gives this the frame it made,
// which is the eight registers, the flags, the room it keeps for the original, and the place the
// replacement of the Interceptor returns to.
#[unsafe(no_mangle)]
extern "C" fn frida_win9x_on_thread_start(frame: *const u32) -> usize {
    let routine = unsafe { frame.add(START_ROUTINE).read() } as usize;
    let parameter = unsafe { frame.add(START_PARAMETER).read() } as usize;

    crate::gum_windows::thread_appeared_at(current_thread_id() as u32, routine, parameter);

    thread_start()
}

const START_ROUTINE: usize = 11;
const START_PARAMETER: usize = 12;

#[unsafe(no_mangle)]
extern "C" fn frida_win9x_on_control(message: u32, thread: u32) {
    if message == TERMINATE_THREAD {
        tell_the_copy_of_a_departure(thread);
    }

    let told = match message {
        THREAD_INIT => unsafe { THREAD_APPEARED },
        DESTROY_THREAD => unsafe { THREAD_VANISHED },
        _ => return,
    };

    if let Some(told) = told {
        told(thread);
    }
}

fn tell_the_copy_of_a_departure(thread: u32) {
    let slot = unsafe { (THREAD_BLOCK_SLOT as *const u32).read() };
    if unsafe { (thread as *const u32).add(0x2c / 4).read() } != WIN32_THREAD {
        return;
    }

    let block = unsafe { (thread as *const u32).byte_add(slot as usize).read() };
    if block == 0 {
        return;
    }
    let database = unsafe { (block as *const u32).read() };
    let process = unsafe { (block as *const u32).add(1).read() };

    let Some(arena) = arena_for_pid(process_id(process)) else {
        return;
    };
    let arena = arena as u32;

    unsafe {
        let head = ((arena + GONE_HEAD) as *const u32).read_volatile();
        let tail = ((arena + GONE_TAIL) as *const u32).read_volatile();
        if head.wrapping_sub(tail) == GONE_COUNT {
            return;
        }

        ((arena + GONE_SLOTS + (head % GONE_COUNT) * 4) as *mut u32)
            .write_volatile(process_id(database));
        ((arena + GONE_HEAD) as *mut u32).write_volatile(head.wrapping_add(1));
    }

    wake_copy(arena);
}

static mut THREAD_APPEARED: Option<fn(u32)> = None;
static mut THREAD_VANISHED: Option<fn(u32)> = None;
static mut DEVICE: [u32; DDB_WORDS] = [0; DDB_WORDS];

const DDB_WORDS: usize = 0x14;
const DDB_SDK_VERSION: usize = 0x04;
const DDB_NAME: usize = 0x0c;
const DDB_CONTROL_PROC: usize = 0x18;
const SDK_VERSION: u32 = 0x0400;
const THREAD_INIT: u32 = 0x1e;
const TERMINATE_THREAD: u32 = 0x1f;
const DESTROY_THREAD: u32 = 0x21;

pub fn install_fault_reporter() {
    unsafe {
        for (fault, thunk) in [
            (INVALID_OPCODE, frida_win9x_fault_thunk_ud as unsafe extern "C" fn()),
            (GENERAL_PROTECTION, frida_win9x_fault_thunk_gp as unsafe extern "C" fn()),
            (PAGE_FAULT, frida_win9x_fault_thunk_pf as unsafe extern "C" fn()),
        ] {
            FAULT_CHAIN[fault as usize] = fault_handler(fault);
            (thunk as *mut u32).sub(2).write_volatile(thunk as u32);
            hook_vmm_fault(fault, thunk);
        }
    }
}

fn fault_table() -> u32 {
    unsafe {
        let hook = (core::ptr::addr_of!(_Hook_VMM_Fault) as *const u32).read();

        ((hook + 1) as *const u32).read_unaligned()
    }
}

fn fault_handler(fault: u32) -> u32 {
    unsafe { ((fault_table() + fault * 4) as *const u32).read_volatile() }
}

fn set_fault_handler(fault: u32, handler: u32) {
    unsafe { ((fault_table() + fault * 4) as *mut u32).write_volatile(handler) };
}

pub fn release_fault_reporter() {
    unsafe {
        for (fault, thunk) in [
            (INVALID_OPCODE, frida_win9x_fault_thunk_ud as unsafe extern "C" fn()),
            (GENERAL_PROTECTION, frida_win9x_fault_thunk_gp as unsafe extern "C" fn()),
            (PAGE_FAULT, frida_win9x_fault_thunk_pf as unsafe extern "C" fn()),
        ] {
            let previous = FAULT_CHAIN[fault as usize];
            if previous == 0 {
                continue;
            }
            FAULT_CHAIN[fault as usize] = 0;

            let slot = thunk as *mut u32;
            core::arch::asm!("cli");
            slot.write_volatile(previous);
            unhook_vmm_fault(fault, thunk as u32);
            if slot.read_volatile() != 0 {
                slot.write_volatile(SLOT_JUMPS_OVER_ITSELF);
                set_fault_handler(fault, previous);
            }
            core::arch::asm!("sti");
        }
    }
}

const SLOT_JUMPS_OVER_ITSELF: u32 = 0x9090_02eb;

#[unsafe(no_mangle)]
extern "C" fn frida_win9x_on_fault(fault: u32, frame: *mut u32) -> u32 {
    let Some(registers) = our_fault_frame(frame) else {
        return unsafe { FAULT_CHAIN[fault as usize] };
    };

    let mut cpu_context = unsafe {
        crate::bindings::_GumIA32CpuContext {
            eip: registers.byte_add(FAULT_FRAME_EIP).read(),
            edi: registers.read(),
            esi: registers.add(1).read(),
            ebp: registers.add(2).read(),
            esp: recovery_stack(frame),
            ebx: registers.add(4).read(),
            edx: registers.add(5).read(),
            ecx: registers.add(6).read(),
            eax: registers.add(7).read(),
            xmm: core::ptr::null_mut(),
        }
    };

    let handled = unsafe {
        crate::bindings::gum_barebone_handle_exception(
            exception_type_for(fault),
            cpu_context.eip as *mut c_void,
            faulting_address() as *mut c_void,
            &mut cpu_context,
        )
    };
    if handled == 0 {
        report_unhandled_fault(fault, cpu_context.eip);
        cpu_context.eip = frida_win9x_park as u32;
    }

    unsafe {
        frida_win9x_resume = [
            cpu_context.eip,
            cpu_context.edi,
            cpu_context.esi,
            cpu_context.ebp,
            cpu_context.esp,
            cpu_context.ebx,
            cpu_context.edx,
            cpu_context.ecx,
            cpu_context.eax,
        ];
    }

    0
}

// VMM enters a fault hook with EBP at the frame that the processor and its dispatcher made:
// the registers from pushad, then the error code, EIP, CS and the flags. Use that frame. The
// frame of the thunk holds the address that VMM returns to.
fn our_fault_frame(frame: *mut u32) -> Option<*const u32> {
    let registers = unsafe { frame.add(THUNK_FRAME_EBP).read() } as *const u32;
    let eip = unsafe { registers.byte_add(FAULT_FRAME_EIP).read() };
    let cs = unsafe { registers.byte_add(FAULT_FRAME_CS).read() };

    (cs & 3 == 0 && crate::own_range_contains(eip)).then_some(registers)
}

fn recovery_stack(frame: *mut u32) -> u32 {
    unsafe { frame.add(THUNK_FRAME_ESP).read() }
}

fn report_unhandled_fault(fault: u32, eip: u32) {
    let address = faulting_address();

    crate::note_unhandled_fault(fault as u64, eip as u64, address as u64);

    log("frida: unhandled fault in the agent\n");
    log("frida:   vector ");
    log_hex(fault);
    log("frida:   eip ");
    log_hex(eip);
    log("frida:   address ");
    log_hex(address);
}

// Nothing can recover from this, thus the thread stops here.
extern "C" fn frida_win9x_park() -> ! {
    loop {
        wait(core::ptr::addr_of!(PARK_TOKEN), Some(PARK_SLICE_US), &mut || false);
    }
}

static mut PARK_TOKEN: u8 = 0;

#[unsafe(no_mangle)]
static mut frida_win9x_resume: [u32; 9] = [0; 9];

fn exception_type_for(fault: u32) -> crate::bindings::GumExceptionType {
    use crate::bindings::*;

    match fault {
        DIVIDE_ERROR | OVERFLOW | BOUND_RANGE_EXCEEDED | X87_FLOATING_POINT
        | SIMD_FLOATING_POINT => _GumExceptionType_GUM_EXCEPTION_ARITHMETIC,
        DEBUG => _GumExceptionType_GUM_EXCEPTION_SINGLE_STEP,
        BREAKPOINT => _GumExceptionType_GUM_EXCEPTION_BREAKPOINT,
        INVALID_OPCODE => _GumExceptionType_GUM_EXCEPTION_ILLEGAL_INSTRUCTION,
        STACK_SEGMENT_FAULT => _GumExceptionType_GUM_EXCEPTION_STACK_OVERFLOW,
        GENERAL_PROTECTION | PAGE_FAULT => _GumExceptionType_GUM_EXCEPTION_ACCESS_VIOLATION,
        _ => _GumExceptionType_GUM_EXCEPTION_SYSTEM,
    }
}

const DIVIDE_ERROR: u32 = 0;
const DEBUG: u32 = 1;
const BREAKPOINT: u32 = 3;
const OVERFLOW: u32 = 4;
const BOUND_RANGE_EXCEEDED: u32 = 5;
const STACK_SEGMENT_FAULT: u32 = 12;
const X87_FLOATING_POINT: u32 = 16;
const SIMD_FLOATING_POINT: u32 = 19;

const THUNK_FRAME_EBP: usize = 2;
const THUNK_FRAME_ESP: usize = 3;
const FAULT_FRAME_EIP: usize = 0x24;
const FAULT_FRAME_CS: usize = 0x28;
const PARK_SLICE_US: u64 = 1_000_000;

fn faulting_address() -> u32 {
    let address: u32;
    unsafe { core::arch::asm!("mov {0:e}, cr2", out(reg) address, options(nomem, nostack, preserves_flags)) };
    address
}

static mut FAULT_CHAIN: [u32; 32] = [0; 32];

const INVALID_OPCODE: u32 = 6;
const GENERAL_PROTECTION: u32 = 13;
const PAGE_FAULT: u32 = 14;

#[unsafe(no_mangle)]
extern "C" fn frida_win9x_on_hw_int(_ref_data: *mut c_void) {
    unsafe {
        IN_INTERRUPT = true;
        if let Some(handler) = HW_INT_HANDLER {
            handler(HW_INT_TARGET, HW_INT_REFCON, core::ptr::null_mut(), 0);
        }
        IN_INTERRUPT = false;
        vpicd_phys_eoi(IRQ_HANDLE);
    }
}

#[unsafe(no_mangle)]
extern "C" fn frida_win9x_on_wake_event() {
    let token = WAKE_WANTED.swap(0, Ordering::AcqRel);
    if token != 0 {
        crate::nudge_the_loop(token as *const u8);
    }
}

static mut IN_INTERRUPT: bool = false;
static WAKE_WANTED: AtomicUsize = AtomicUsize::new(0);

static mut IRQ_HANDLE: u32 = 0;

static mut IRQ_DESCRIPTOR: VpicdIrqDescriptor = VpicdIrqDescriptor {
    irq_number: 0,
    options: 0,
    hw_int_proc: frida_win9x_hw_int_thunk,
    virt_int_proc: 0,
    eoi_proc: 0,
    mask_change_proc: 0,
    iret_proc: 0,
    iret_time_out: 0,
    hw_int_ref: 0,
};

static mut HW_INT_HANDLER: Option<InterruptHandler> = None;
static mut HW_INT_TARGET: *mut c_void = core::ptr::null_mut();
static mut HW_INT_REFCON: *mut c_void = core::ptr::null_mut();

#[repr(C)]
struct VpicdIrqDescriptor {
    irq_number: u16,
    options: u16,
    hw_int_proc: unsafe extern "C" fn(),
    virt_int_proc: u32,
    eoi_proc: u32,
    mask_change_proc: u32,
    iret_proc: u32,
    iret_time_out: u32,
    hw_int_ref: u32,
}

const VPICD_OPT_CAN_SHARE: u16 = 0x02;
const VPICD_OPT_REF_DATA: u16 = 0x04;
const VPICD_DEFAULT_IRET_TIME_OUT: u32 = 500;

pub type InterruptHandler =
    unsafe extern "C" fn(target: *mut c_void, refcon: *mut c_void, nub: *mut c_void, source: i32);

pub fn get_kernel_base() -> u64 {
    KERNEL_BASE.load(Ordering::Relaxed) as u64
}

pub fn set_kernel_base(base: u64) {
    KERNEL_BASE.store(base as u32, Ordering::Relaxed);
}

static KERNEL_BASE: AtomicU32 = AtomicU32::new(0);

// One semaphore per waited-on address, created on first use. VMM has no
// futex-alike, so the token has to be mapped onto something it does have.
fn slot_for_token(token: *const u8) -> usize {
    let start = (token as usize / core::mem::align_of::<usize>()) % NUM_SEMAPHORES;
    for step in 0..NUM_SEMAPHORES {
        let slot = (start + step) % NUM_SEMAPHORES;

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

fn loop_semaphore() -> u32 {
    semaphore_in(slot_for_token(crate::glib::wakeup_token()))
}

fn release_slot(slot: usize, token: *const u8) {
    if SLEEPERS[slot].fetch_sub(1, Ordering::AcqRel) != 1 {
        return;
    }
    if core::ptr::eq(token, crate::glib::wakeup_token()) {
        return;
    }

    let _ = OWNERS[slot].compare_exchange(token as usize, 0, Ordering::AcqRel, Ordering::Acquire);
}

fn slot_owned_by(token: *const u8) -> Option<usize> {
    let start = (token as usize / core::mem::align_of::<usize>()) % NUM_SEMAPHORES;
    for step in 0..NUM_SEMAPHORES {
        let slot = (start + step) % NUM_SEMAPHORES;
        if OWNERS[slot].load(Ordering::Acquire) == token as usize {
            return Some(slot);
        }
    }

    None
}

fn semaphore_in(slot: usize) -> u32 {
    let existing = SEMAPHORES[slot].load(Ordering::Acquire);
    if existing != 0 {
        return existing;
    }

    let created = unsafe { create_semaphore(0) };
    match SEMAPHORES[slot].compare_exchange(0, created, Ordering::AcqRel, Ordering::Acquire) {
        Ok(_) => created,
        Err(raced) => raced,
    }
}

const NUM_SEMAPHORES: usize = 64;
static SEMAPHORES: [AtomicU32; NUM_SEMAPHORES] = [const { AtomicU32::new(0) }; NUM_SEMAPHORES];
static OWNERS: [AtomicUsize; NUM_SEMAPHORES] = [const { AtomicUsize::new(0) }; NUM_SEMAPHORES];
static SLEEPERS: [AtomicU32; NUM_SEMAPHORES] = [const { AtomicU32::new(0) }; NUM_SEMAPHORES];
static WOKEN_EARLY: [AtomicU32; NUM_SEMAPHORES] = [const { AtomicU32::new(0) }; NUM_SEMAPHORES];
pub(crate) static EVENTS: [AtomicU32; NUM_SEMAPHORES] = [const { AtomicU32::new(0) }; NUM_SEMAPHORES];
pub(crate) static EVENT_OWNERS: [AtomicUsize; NUM_SEMAPHORES] =
    [const { AtomicUsize::new(0) }; NUM_SEMAPHORES];
pub(crate) static EVENT_SLEEPERS: [AtomicU32; NUM_SEMAPHORES] =
    [const { AtomicU32::new(0) }; NUM_SEMAPHORES];

unsafe extern "C" {
    fn wait_semaphore(semaphore: u32, flags: u32);
    fn signal_semaphore(semaphore: u32);
    fn create_semaphore(token_count: u32) -> u32;
    pub fn get_cur_thread_handle() -> u32;
    fn fatal_error_handler(message: *const u8, flags: u32);
    fn vpicd_virtualize_irq(descriptor: *mut VpicdIrqDescriptor) -> u32;
    fn vpicd_physically_unmask(handle: u32);
    fn vpicd_phys_eoi(handle: u32);
    fn vpicd_force_default_behavior(handle: u32) -> u32;
    fn set_global_time_out(milliseconds: u32, semaphore: u32) -> u32;
    fn cancel_time_out(timeout: u32);
    fn hook_vmm_fault(fault: u32, handler: unsafe extern "C" fn()) -> u32;
    fn vmm_add_ddb(ddb: u32) -> u32;
    fn vmm_remove_ddb(ddb: u32);
    fn frida_win9x_control_thunk();
    pub fn frida_win9x_thread_start_thunk();
    fn unhook_vmm_fault(fault: u32, handler: u32);
    fn frida_win9x_fault_thunk_ud();
    fn frida_win9x_fault_thunk_gp();
    fn frida_win9x_fault_thunk_pf();
    fn frida_win9x_time_out_thunk();
    fn get_cur_vm_handle() -> u32;
    fn get_sys_vm_handle() -> u32;
    fn schedule_global_event(callback: unsafe extern "C" fn()) -> u32;
    fn frida_win9x_event_thunk();
    fn frida_win9x_wake_event_thunk();
    fn get_next_vm_handle(vm: u32) -> u32;
    fn get_initial_thread_handle(vm: u32) -> u32;
    fn get_next_thread_handle(thread: u32) -> u32;
    fn ifsmgr_ring0_file_io(function: u32, ebx: u32, ecx: u32, edx: u32, esi: u32, edi: u32) -> u32;
    fn vwin32_create_ring0_thread(stack_size: u32, parameter: u32, entry: u32, event: u32) -> u32;
    fn frida_win9x_hw_int_thunk();
    fn frida_win9x_thread_thunk();
}

core::arch::global_asm!(
    r#"
.intel_syntax noprefix

// A position-independent image cannot name its data in an instruction. Thus the code reads the
// program counter, from which the offset to the data is known, and adds the offset. The frame
// pointer holds the result, because no service reads that register.
frida_win9x_get_pc_ebp:
    mov ebp, [esp]
    ret

frida_win9x_get_pc_ebx:
    mov ebx, [esp]
    ret

.macro CALL_SERVICE slot
    push ebp
    call frida_win9x_get_pc_ebp
    add ebp, offset _GLOBAL_OFFSET_TABLE_
    call dword ptr [ebp + \slot@GOTOFF]
    pop ebp
.endm

.global wait_semaphore
wait_semaphore:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    mov eax, [ebp + 8]
    mov ecx, [ebp + 12]
    CALL_SERVICE _Wait_Semaphore
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

.global signal_semaphore
signal_semaphore:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    mov eax, [ebp + 8]
    CALL_SERVICE _Signal_Semaphore
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

.global create_semaphore
create_semaphore:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    mov ecx, [ebp + 8]
    CALL_SERVICE _Create_Semaphore
    cmc
    sbb ecx, ecx
    and eax, ecx
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

.global get_cur_thread_handle
get_cur_thread_handle:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    CALL_SERVICE _Get_Cur_Thread_Handle
    mov eax, edi
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

.global fatal_error_handler
fatal_error_handler:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    mov esi, [ebp + 8]
    mov eax, [ebp + 12]
    CALL_SERVICE _Fatal_Error_Handler
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

.global schedule_global_event
schedule_global_event:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    mov esi, [ebp + 8]
    CALL_SERVICE _Schedule_Global_Event
    mov eax, ebx
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

.global frida_win9x_event_thunk
frida_win9x_event_thunk:
    pushad
    call frida_win9x_on_event
    popad
    ret

.global frida_win9x_wake_event_thunk
frida_win9x_wake_event_thunk:
    pushad
    call frida_win9x_on_wake_event
    popad
    ret

.global get_sys_vm_handle
get_sys_vm_handle:
    push ebx
    CALL_SERVICE _Get_Sys_VM_Handle
    mov eax, ebx
    pop ebx
    ret

.global get_next_vm_handle
get_next_vm_handle:
    push ebp
    mov ebp, esp
    push ebx
    mov ebx, [ebp + 8]
    CALL_SERVICE _Get_Next_VM_Handle
    mov eax, ebx
    pop ebx
    pop ebp
    ret

.global get_initial_thread_handle
get_initial_thread_handle:
    push ebp
    mov ebp, esp
    push ebx
    push edi
    mov ebx, [ebp + 8]
    CALL_SERVICE _Get_Initial_Thread_Handle
    mov eax, edi
    pop edi
    pop ebx
    pop ebp
    ret

.global get_next_thread_handle
get_next_thread_handle:
    push ebp
    mov ebp, esp
    push ebx
    push edi
    mov edi, [ebp + 8]
    CALL_SERVICE _Get_Next_Thread_Handle
    mov eax, edi
    pop edi
    pop ebx
    pop ebp
    ret

.global vwin32_create_ring0_thread
vwin32_create_ring0_thread:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    mov ecx, [ebp + 8]
    mov edx, [ebp + 12]
    mov ebx, [ebp + 16]
    mov esi, [ebp + 20]
    CALL_SERVICE __VWIN32_CreateRing0Thread
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

.global ifsmgr_ring0_file_io
ifsmgr_ring0_file_io:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    mov eax, [ebp + 8]
    mov ebx, [ebp + 12]
    mov ecx, [ebp + 16]
    mov edx, [ebp + 20]
    mov esi, [ebp + 24]
    mov edi, [ebp + 28]
    CALL_SERVICE _IFSMgr_Ring0_FileIO
    jnc 1f
    xor eax, eax
1:
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

.global get_cur_vm_handle
get_cur_vm_handle:
    push ebx
    CALL_SERVICE _Get_Cur_VM_Handle
    mov eax, ebx
    pop ebx
    ret

.global vmm_add_ddb
vmm_add_ddb:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    mov edi, [ebp + 8]
    CALL_SERVICE _VMM_Add_DDB
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

.global vmm_remove_ddb
vmm_remove_ddb:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    mov edi, [ebp + 8]
    CALL_SERVICE _VMM_Remove_DDB
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

.global frida_win9x_thread_start_thunk
frida_win9x_thread_start_thunk:
    push eax
    pushfd
    pushad
    push esp
    call frida_win9x_on_thread_start
    add esp, 4
    mov [esp + 36], eax
    popad
    popfd
    ret

.global frida_win9x_control_thunk
frida_win9x_control_thunk:
    pushad
    push edi
    push eax
    call frida_win9x_on_control
    add esp, 8
    popad
    clc
    ret

.global hook_vmm_fault
.global unhook_vmm_fault
unhook_vmm_fault:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    mov eax, [ebp + 8]
    mov esi, [ebp + 12]
    CALL_SERVICE _Unhook_VMM_Fault
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

.global hook_vmm_fault
hook_vmm_fault:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    mov eax, [ebp + 8]
    mov esi, [ebp + 12]
    CALL_SERVICE _Hook_VMM_Fault
    mov eax, esi
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

/*
 * A handler the system can take out again carries a head of twelve bytes and a word of its own:
 * the head is how Unhook_VMM_Fault knows one of its own, and the word is where Hook_VMM_Fault
 * writes the handler that was there before. The system enters the handler at that word, thus the
 * word is a jump over itself while the handler is in the table, and becomes the way back again
 * for as long as the unhooking takes.
 */
.macro FAULT_THUNK name, fault
.align 4
    .byte 0xeb, 0x0a
    .byte 0xff, 0x25
    .long 0
    .long 0
.global \name
\name:
    .byte 0xeb, 0x02, 0x90, 0x90
    push \fault
    jmp frida_win9x_fault_common
.endm

FAULT_THUNK frida_win9x_fault_thunk_ud, 6
FAULT_THUNK frida_win9x_fault_thunk_gp, 13
FAULT_THUNK frida_win9x_fault_thunk_pf, 14

frida_win9x_fault_common:
    pushad
    mov eax, [esp + 32]
    push esp
    push eax
    call frida_win9x_on_fault
    add esp, 8
    test eax, eax
    jz frida_win9x_fault_resume
    mov [esp + 32], eax
    popad
    ret
frida_win9x_fault_resume:
    call frida_win9x_get_pc_ebx
    add ebx, offset _GLOBAL_OFFSET_TABLE_
    lea ebx, [ebx + frida_win9x_resume@GOTOFF]
    mov esp, [ebx + 16]
    push dword ptr [ebx]
    push dword ptr [ebx + 32]
    push dword ptr [ebx + 20]
    mov edi, [ebx + 4]
    mov esi, [ebx + 8]
    mov ebp, [ebx + 12]
    mov edx, [ebx + 24]
    mov ecx, [ebx + 28]
    pop ebx
    pop eax
    ret

.global set_global_time_out
set_global_time_out:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    mov eax, [ebp + 8]
    mov edx, [ebp + 12]
    call frida_win9x_get_pc_ebp
    add ebp, offset _GLOBAL_OFFSET_TABLE_
    lea esi, [ebp + frida_win9x_time_out_thunk@GOTOFF]
    call dword ptr [ebp + _Set_Global_Time_Out@GOTOFF]
    mov eax, esi
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

.global cancel_time_out
cancel_time_out:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    mov esi, [ebp + 8]
    CALL_SERVICE _Cancel_Time_Out
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

.global frida_win9x_time_out_thunk
frida_win9x_time_out_thunk:
    pushad
    mov eax, edx
    CALL_SERVICE _Signal_Semaphore
    popad
    ret

.global vpicd_phys_eoi
vpicd_force_default_behavior:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    mov eax, [ebp + 8]
    CALL_SERVICE _VPICD_Force_Default_Behavior
    cmc
    sbb eax, eax
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

.global vpicd_phys_eoi
vpicd_phys_eoi:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    mov eax, [ebp + 8]
    CALL_SERVICE _VPICD_Phys_EOI
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

.global vpicd_physically_unmask
vpicd_physically_unmask:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    mov eax, [ebp + 8]
    CALL_SERVICE _VPICD_Physically_Unmask
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

.global vpicd_virtualize_irq
vpicd_virtualize_irq:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    mov edi, [ebp + 8]
    CALL_SERVICE _Get_Cur_VM_Handle
    CALL_SERVICE _VPICD_Virtualize_IRQ
    cmc
    sbb ecx, ecx
    and eax, ecx
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

.global frida_win9x_thread_thunk
frida_win9x_thread_thunk:
    call frida_win9x_thread_start
    ret

.global frida_win9x_hw_int_thunk
frida_win9x_hw_int_thunk:
    pushad
    push edx
    call frida_win9x_on_hw_int
    add esp, 4
    popad
    clc
    ret
"#
);

unsafe extern "C" {
    static _Get_Cur_VM_Handle: unsafe extern "C" fn();
    static _Get_Sys_VM_Handle: unsafe extern "C" fn();
    static _Schedule_Global_Event: unsafe extern "C" fn();
    static _Create_Semaphore: unsafe extern "C" fn();
    static _Wait_Semaphore: unsafe extern "C" fn();
    static _Signal_Semaphore: unsafe extern "C" fn();
    static _Get_Next_VM_Handle: unsafe extern "C" fn();
    static _Release_Time_Slice: unsafe extern "C" fn();
    static _Set_Global_Time_Out: unsafe extern "C" fn();
    static _Cancel_Time_Out: unsafe extern "C" fn();
    static _Get_System_Time: unsafe extern "C" fn() -> u32;
    static __HeapAllocate: unsafe extern "C" fn(u32, u32) -> *mut u8;
    static __HeapFree: unsafe extern "C" fn(*mut u8, u32);
    static __PageAllocate:
        unsafe extern "C" fn(u32, u32, u32, u32, u32, u32, *mut c_void, u32) -> *mut u8;
    static __PageFree: unsafe extern "C" fn(*mut u8, u32);
    static __CopyPageTable: unsafe extern "C" fn(u32, u32, *mut u32, u32) -> u32;
    static __MapPhysToLinear: unsafe extern "C" fn(u32, u32, u32) -> u32;
    static __RegOpenKey: unsafe extern "C" fn(u32, *const u8, *mut u32) -> u32;
    static __RegCloseKey: unsafe extern "C" fn(u32) -> u32;
    static __RegEnumKey: unsafe extern "C" fn(u32, u32, *mut u8, u32) -> u32;
    static __RegQueryValueEx: unsafe extern "C" fn(u32, *const u8, *mut u32, *mut u32, *mut u8,
        *mut u32) -> u32;
    static __PageReserve: unsafe extern "C" fn(u32, u32, u32) -> u32;
    static __PageCommit: unsafe extern "C" fn(u32, u32, u32, u32, u32) -> u32;
    static __PageCommitPhys: unsafe extern "C" fn(u32, u32, u32, u32) -> u32;
    static __VWIN32_QueueUserApc: unsafe extern "C" fn(u32, u32, u32) -> u32;
    static _Hook_VMM_Fault: unsafe extern "C" fn();
    static _VMM_Add_DDB: unsafe extern "C" fn();
    static _VMM_Remove_DDB: unsafe extern "C" fn();
    static _Unhook_VMM_Fault: unsafe extern "C" fn();
    static _Fatal_Error_Handler: unsafe extern "C" fn();
    static _Get_Cur_Thread_Handle: unsafe extern "C" fn();
    static _Get_Initial_Thread_Handle: unsafe extern "C" fn();
    static _Get_Next_Thread_Handle: unsafe extern "C" fn();
    static __Debug_Printf_Service: unsafe extern "C" fn(*const u8, ...);
    static __ContextSwitch: unsafe extern "C" fn(u32);
    static __PageModifyPermissions: unsafe extern "C" fn(u32, u32, u32, u32) -> u32;
    static __GetCurrentContext: unsafe extern "C" fn() -> u32;
    static _VPICD_Virtualize_IRQ: unsafe extern "C" fn();
    static _VPICD_Phys_EOI: unsafe extern "C" fn();
    static _VPICD_Physically_Unmask: unsafe extern "C" fn();
    static _VPICD_Force_Default_Behavior: unsafe extern "C" fn();
    static __VWIN32_Get_Thread_Context: unsafe extern "C" fn(u32, *mut u8) -> u32;
    static __VWIN32_Set_Thread_Context: unsafe extern "C" fn(u32, *const u8) -> u32;
    static __VWIN32_CreateRing0Thread: unsafe extern "C" fn();
    static _IFSMgr_Ring0_FileIO: unsafe extern "C" fn();
    static _KERNEL32_Base: u32;
    static _KERNEL32_ProcessIdObfuscator: u32;
    static _KERNEL32_ModuleTable: u32;
}
