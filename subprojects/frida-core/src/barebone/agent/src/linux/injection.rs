use core::ffi::{c_int, c_void};
use core::ptr;

use alloc::collections::BTreeMap;
use alloc::string::String;

use super::layout::{field_offset, struct_size};
use super::STACK_SPAN;
use super::native;
use super::processes::task_with_id;
use super::arena::{Arena, HOME, REPORTED, WOKEN};
use super::user::entry_offset;

#[cfg(target_arch = "arm")]
use _arm_copy_to_user as copy_to_user;
#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
use __copy_to_user as copy_to_user;
#[cfg(not(any(target_arch = "arm", target_arch = "x86", target_arch = "x86_64")))]
use ___arch_copy_to_user as copy_to_user;

pub fn inject_into_process(id: u32) -> u32 {
    if let Some(placed) = unsafe { placements() }.get_mut(&id) {
        let home = pid_reported_by(placed);
        if home != 0 && placed.says.is_null() {
            take_what_the_copy_opened(placed);
        }

        return home;
    }

    let Some(task) = task_with_id(id) else {
        return 0;
    };
    let Some(home) = give_the_copy_a_home(task, id) else {
        return 0;
    };

    let placed = Placement {
        task,
        copy_pid: home.copy_pid,
        base: home.base,
        arena: home.arena,
        seen_by_the_copy: home.seen_by_the_copy,
        stack: home.stack,
        says: ptr::null_mut(),
        hears: ptr::null_mut(),
    };
    unsafe { placements() }.insert(id, placed);
    watch_the_threads();

    0
}

fn take_what_the_copy_opened(placed: &mut Placement) {
    let arena = Arena::at(placed.arena);
    let says_fd = arena.says_through();
    let hears_fd = arena.hears_through();
    let copy = COPY_TASK.load(core::sync::atomic::Ordering::Acquire) as *mut c_void;
    placed.says = native::take_the_file_of(copy, says_fd);
    placed.hears = native::take_the_file_of(copy, hears_fd);
    listen_to_the_copy(placed.says);
}

fn listen_to_the_copy(says: *mut c_void) {
    native::spawn_thread(carry_what_the_copy_says, says);
}

unsafe extern "C" fn carry_what_the_copy_says(argument: *mut c_void, _reason: i32) {
    while native::wait_for_a_word(argument) {
        native::wake(argument as *const u8);
    }
}

struct Placement {
    task: usize,
    copy_pid: u32,
    base: usize,
    arena: usize,
    seen_by_the_copy: usize,
    stack: usize,
    says: *mut c_void,
    hears: *mut c_void,
}

fn pid_reported_by(placed: &Placement) -> u32 {
    unsafe { ((placed.arena + REPORTED) as *const u32).read_volatile() }
}

// The copy is given a task of its own that shares the address space it was mapped into, so
// nothing of the target is borrowed and a target that never runs is no obstacle.
struct Home {
    copy_pid: u32,
    base: usize,
    arena: usize,
    seen_by_the_copy: usize,
    stack: usize,
}

fn give_the_copy_a_home(task: usize, id: u32) -> Option<Home> {
    let memory = unsafe { _get_task_mm(task as *mut c_void) };
    if memory.is_null() {
        return None;
    }

    unsafe { _kthread_use_mm(memory) };
    let placed = map_and_start(id);
    unsafe { _kthread_unuse_mm(memory) };

    unsafe { _mmput(memory) };

    placed
}

fn map_and_start(id: u32) -> Option<Home> {
    let base = map_a_copy()?;
    let stack = map_a_stack()?;

    let where_the_copy_sees_it = base + crate::own_range().1;
    give(
        where_the_copy_sees_it + HOME,
        &id as *const u32 as usize,
        size_of::<u32>(),
    )?;
    let page = native::page_size() as u32;
    give(
        where_the_copy_sees_it + super::arena::PAGE_SIZE,
        &page as *const u32 as usize,
        size_of::<u32>(),
    )?;

    let arena = view_of(where_the_copy_sees_it)?;

    let copy_pid = start(base, where_the_copy_sees_it, stack);

    Some(Home {
        copy_pid,
        base,
        arena,
        seen_by_the_copy: where_the_copy_sees_it,
        stack,
    })
}

// The kernel half reaches the same pages by an address of its own, so what the two leave for
// each other needs neither the address space borrowed nor a copy made.
fn borrowed_memory() -> Option<usize> {
    let at = field_offset("task_struct", "mm")?;

    Some(unsafe { ((native::current_task() as usize + at) as *const usize).read() })
}

fn view_of(seen_by_the_copy: usize) -> Option<usize> {
    let count = ARENA_SIZE / PAGE_SIZE;
    let pages = native::alloc(count * size_of::<usize>()) as *mut c_void;

    let held = unsafe { _get_user_pages_unlocked(seen_by_the_copy, count, pages, FOLL_WRITE) };
    if held != count as isize {
        native::free(pages as *mut u8, count * size_of::<usize>());
        return None;
    }

    let view = native::map_pages(pages, count);
    native::free(pages as *mut u8, count * size_of::<usize>());

    if view.is_null() {
        return None;
    }

    Some(view as usize)
}

fn map_a_stack() -> Option<usize> {
    let base = unsafe {
        _vm_mmap(
            ptr::null_mut(),
            0,
            STACK_SIZE,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            0,
        )
    };
    if base >= FIRST_ERROR_ADDRESS {
        return None;
    }

    Some(base + STACK_SIZE - STACK_HEADROOM)
}

fn map_a_copy() -> Option<usize> {
    let (own, size) = crate::own_range();
    let shared = crate::writable_half_start() - own;
    let private = crate::writable_half_size();

    let executable_up_front = if cfg!(target_arch = "aarch64") { 0 } else { PROT_EXEC };
    let base = unsafe {
        _vm_mmap(
            ptr::null_mut(),
            0,
            size + ARENA_SIZE,
            PROT_READ | PROT_WRITE | executable_up_front,
            MAP_PRIVATE | MAP_ANONYMOUS,
            0,
        )
    };
    if base >= FIRST_ERROR_ADDRESS {
        return None;
    }

    give(base, own, shared)?;

    let rebased = native::alloc(private);
    unsafe { crate::install_writable_half(base, rebased as usize) };
    let handed_over = give(base + shared, rebased as usize, private);
    native::free(rebased, private);
    handed_over?;

    #[cfg(target_arch = "aarch64")]
    make_the_code_executable(base, shared)?;

    Some(base)
}

#[cfg(target_arch = "aarch64")]
fn make_the_code_executable(base: usize, size: usize) -> Option<()> {
    let memory = borrowed_memory()? as *mut c_void;
    unsafe {
        let apply = _apply_to_existing_page_range?;
        let clean_to_unification = _caches_clean_inval_pou?;
        if apply(memory, base, size, frida_pte_exec_trampoline, ptr::null_mut()) != 0 {
            return None;
        }
        clean_to_unification(base, base + size);
        flush_user_translations();
    }
    Some(())
}

#[cfg(target_arch = "aarch64")]
core::arch::global_asm!(
    ".pushsection .text,\"ax\",%progbits",
    ".balign 4",
    ".word 0xa8320d42",
    ".globl frida_pte_exec_trampoline",
    ".type frida_pte_exec_trampoline,%function",
    "frida_pte_exec_trampoline:",
    "bti c",
    "b {inner}",
    ".popsection",
    inner = sym grant_user_execute,
);

#[cfg(target_arch = "aarch64")]
unsafe extern "C" {
    fn frida_pte_exec_trampoline(pte: *mut c_void, address: usize, data: *mut c_void) -> c_int;
}

#[cfg(target_arch = "aarch64")]
unsafe extern "C" fn grant_user_execute(entry: *mut c_void, _address: usize, _data: *mut c_void) -> c_int {
    let entry = entry as *mut u64;
    let revised = (unsafe { entry.read_volatile() } | PTE_READ_ONLY) & !PTE_USER_EXECUTE_NEVER;
    unsafe { entry.write_volatile(revised) };
    0
}

#[cfg(target_arch = "aarch64")]
unsafe fn flush_user_translations() {
    unsafe {
        core::arch::asm!(
            "dsb ishst",
            "tlbi vmalle1is",
            "dsb ish",
            "isb",
            options(nostack, preserves_flags),
        );
    }
}

#[cfg(target_arch = "aarch64")]
const PTE_READ_ONLY: u64 = 1 << 7;
#[cfg(target_arch = "aarch64")]
const PTE_USER_EXECUTE_NEVER: u64 = 1 << 54;

fn give(destination: usize, source: usize, size: usize) -> Option<()> {
    let opened = unsafe { open_user_access() };
    let left =
        unsafe { copy_to_user(destination as *mut c_void, source as *const c_void, size) };
    unsafe { close_user_access(opened) };
    if left != 0 {
        if left == size {
        } else if left < size {
        } else {
        }
        return None;
    }

    Some(())
}

#[cfg(target_arch = "arm")]
unsafe fn open_user_access() -> usize {
    let domains: usize;
    unsafe {
        core::arch::asm!("mrc p15, 0, {}, c3, c0, 0", out(reg) domains, options(nostack));
        let opened = (domains & !USER_DOMAIN_MASK) | USER_DOMAIN_CLIENT;
        core::arch::asm!("mcr p15, 0, {}, c3, c0, 0", "isb", in(reg) opened, options(nostack));
    }

    domains
}

#[cfg(target_arch = "arm")]
unsafe fn close_user_access(domains: usize) {
    unsafe {
        core::arch::asm!("mcr p15, 0, {}, c3, c0, 0", "isb", in(reg) domains, options(nostack));
    }
}

#[cfg(target_arch = "arm")]
const USER_DOMAIN_MASK: usize = 0b11 << 2;

#[cfg(target_arch = "arm")]
const USER_DOMAIN_CLIENT: usize = 0b01 << 2;

#[cfg(not(target_arch = "arm"))]
unsafe fn open_user_access() -> usize {
    0
}

#[cfg(not(target_arch = "arm"))]
unsafe fn close_user_access(_domains: usize) {}

fn start(base: usize, arena: usize, stack: usize) -> u32 {
    let entry = native::alloc(size_of::<Entry>()) as *mut Entry;
    unsafe {
        entry.write(Entry {
            arena,
            bootstrap: base + entry_offset(),
            stack,
        });

        _user_mode_thread(USER_ENTRY, entry as *mut c_void, CLONE_VM | CLONE_FS | CLONE_FILES) as u32
    }
}

#[repr(C)]
struct Entry {
    arena: usize,
    bootstrap: usize,
    stack: usize,
}

// Runs on the new task before the kernel lets it out, which is where what it starts with in
// userland is written down.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn frida_cb_user(argument: *mut c_void) -> c_int {
    let entry = argument as *mut Entry;
    let (arena, bootstrap, stack) = unsafe { ((*entry).arena, (*entry).bootstrap, (*entry).stack) };
    native::free(entry as *mut u8, size_of::<Entry>());

    COPY_TASK.store(native::current_task() as usize, core::sync::atomic::Ordering::Release);

    let Some(places) = describe_registers() else {
        return 0;
    };
    let registers = registers_of_this_task(&places);

    let stack = hand_over_argument(stack, arena);

    unsafe {
        ((registers + places.pc) as *mut usize).write(bootstrap & !THUMB_BIT);
        ((registers + places.stack) as *mut usize).write(stack);
        ((registers + places.flags) as *mut usize).write(
            USER_EXECUTION | (bootstrap & THUMB_BIT) * THUMB_STATE);
        ((registers + places.first) as *mut usize).write(arena);
        write_selectors(registers, &places);
    }

    0
}

// What a task starts with in userland is at the top of the kernel stack it was given.
#[cfg(target_arch = "aarch64")]
fn registers_of_this_task(places: &Places) -> usize {
    let stack = unsafe {
        ((native::current_task() as usize + places.stack_of_task) as *const usize).read()
    };

    stack + STACK_SPAN - places.size
}

#[cfg(target_arch = "x86_64")]
fn registers_of_this_task(places: &Places) -> usize {
    native::top_of_stack() - places.size
}

#[cfg(not(any(target_arch = "aarch64", target_arch = "x86_64")))]
fn registers_of_this_task(places: &Places) -> usize {
    let here = &places as *const _ as usize;
    let top = (here & !(STACK_SPAN - 1)) + STACK_SPAN;

    top - TOP_OF_STACK_PADDING - places.size
}

#[cfg(any(target_arch = "x86", target_arch = "arm"))]
const TOP_OF_STACK_PADDING: usize = 8;

#[cfg(not(any(target_arch = "x86", target_arch = "arm")))]
const TOP_OF_STACK_PADDING: usize = 0;

#[cfg(target_arch = "x86_64")]
unsafe fn write_selectors(registers: usize, _places: &Places) {
    unsafe {
        ((registers + 136) as *mut usize).write(0x33);
        ((registers + 160) as *mut usize).write(0x2b);
    }
}

#[cfg(target_arch = "x86")]
unsafe fn write_selectors(registers: usize, _places: &Places) {
    unsafe {
        ((registers + 52) as *mut usize).write(0x73);
        ((registers + 64) as *mut usize).write(0x7b);
        ((registers + 28) as *mut usize).write(0x7b);
        ((registers + 32) as *mut usize).write(0x7b);
    }
}

#[cfg(not(any(target_arch = "x86", target_arch = "x86_64")))]
unsafe fn write_selectors(_registers: usize, _places: &Places) {}

#[cfg(target_arch = "x86")]
fn hand_over_argument(stack: usize, arena: usize) -> usize {
    let frame = [0usize, arena];
    let sp = stack - size_of::<[usize; 2]>();

    match give(sp, frame.as_ptr() as usize, size_of::<[usize; 2]>()) {
        Some(()) => sp,
        None => stack,
    }
}

#[cfg(not(target_arch = "x86"))]
fn hand_over_argument(stack: usize, _arena: usize) -> usize {
    stack
}

struct Places {
    size: usize,
    first: usize,
    stack: usize,
    pc: usize,
    flags: usize,
    stack_of_task: usize,
}

#[cfg(target_arch = "aarch64")]
fn describe_registers() -> Option<Places> {
    Some(Places {
        size: struct_size("pt_regs")?,
        first: field_offset("user_pt_regs", "regs")?,
        stack: field_offset("user_pt_regs", "sp")?,
        pc: field_offset("user_pt_regs", "pc")?,
        flags: field_offset("user_pt_regs", "pstate")?,
        stack_of_task: field_offset("task_struct", "stack")?,
    })
}

#[cfg(target_arch = "arm")]
fn describe_registers() -> Option<Places> {
    Some(Places {
        size: 72,
        first: 0,
        stack: 13 * 4,
        pc: 15 * 4,
        flags: 16 * 4,
        stack_of_task: 0,
    })
}

#[cfg(target_arch = "x86")]
fn describe_registers() -> Option<Places> {
    Some(Places {
        size: 68,
        first: 24,
        stack: 60,
        pc: 48,
        flags: 56,
        stack_of_task: 0,
    })
}

#[cfg(target_arch = "x86_64")]
fn describe_registers() -> Option<Places> {
    Some(Places {
        size: 168,
        first: 112,
        stack: 152,
        pc: 128,
        flags: 144,
        stack_of_task: 0,
    })
}

pub fn arena_for_pid(id: u32) -> Option<u64> {
    unsafe { placements() }.get(&id).map(|placed| placed.arena as u64)
}

pub fn arenas() -> alloc::vec::Vec<u64> {
    unsafe { placements() }
        .values()
        .map(|placed| placed.arena as u64)
        .collect()
}

pub fn tell_the_copy_at(arena: u64) {
    let Some(id) = (unsafe { placements() })
        .iter()
        .find(|(_, placed)| placed.arena as u64 == arena)
        .map(|(id, _)| *id)
    else {
        return;
    };

    tell_the_copy(id);
}

pub fn a_copy_has_something_to_say() -> bool {
    unsafe { placements() }.values().any(|placed| what_it_says(placed).is_some())
}

#[cfg(target_arch = "aarch64")]
pub fn a_copy_wants_executable() -> bool {
    unsafe { placements() }
        .values()
        .any(|placed| Arena::at(placed.arena).wants_executable().is_some())
}

#[cfg(target_arch = "aarch64")]
pub fn serve_executable_requests() {
    for placed in unsafe { placements() }.values() {
        let arena = Arena::at(placed.arena);
        let Some((address, size)) = arena.wants_executable() else {
            continue;
        };
        let granted = make_a_range_executable(placed.task, address as usize, size as usize);
        arena.grant_executable(granted);
    }
}

#[cfg(target_arch = "aarch64")]
fn make_a_range_executable(task: usize, base: usize, size: usize) -> bool {
    let memory = unsafe { _get_task_mm(task as *mut c_void) };
    if memory.is_null() {
        return false;
    }
    unsafe { _kthread_use_mm(memory) };
    let granted = make_the_code_executable(base, size).is_some();
    unsafe { _kthread_unuse_mm(memory) };
    unsafe { _mmput(memory) };
    granted
}

pub fn report_what_the_copies_hit() {
    for placed in unsafe { placements() }.values() {
        let Some(hit) = what_it_says(placed) else {
            continue;
        };
        bump_to(placed.arena + super::arena::PROGRESS, NOTHING_MORE);

        let arena = Arena::at(placed.arena);
        let said = arena.said();
        if hit == super::user::SPOKE {
            native::log(&alloc::format!("copy in {}: {}\n", pid_reported_by(placed), said));
        } else if hit == super::user::PANICKED {
            native::log(&alloc::format!("copy in {} gave up: {}\n", pid_reported_by(placed), said));
        } else {
            let kind = read_address(placed.arena + super::arena::FAULT_KIND);
            native::log(&alloc::format!(
                "copy in {} died on signal {} ({}) at {:#x}, pc {}, lr {}, image at {:#x}, {}\n",
                pid_reported_by(placed),
                kind as u32,
                (kind >> 32) as u32,
                read_address(placed.arena + super::arena::FAULT_ADDRESS),
                relative_to_image(read_address(placed.arena + super::arena::FAULT_PC), placed),
                relative_to_image(read_address(placed.arena + super::arena::FAULT_LR), placed),
                placed.base,
                said
            ));
        }
    }
}

// A copy notes where it is on its way up as well, and those steps are not for reporting.
fn what_it_says(placed: &Placement) -> Option<u32> {
    let said = read_word(placed.arena + super::arena::PROGRESS);
    if said != super::user::FAULTED && said != super::user::PANICKED && said != super::user::SPOKE {
        return None;
    }

    Some(said)
}

fn bump_to(address: usize, value: u32) {
    unsafe { (address as *mut u32).write_volatile(value) };
}

fn relative_to_image(address: u64, placed: &Placement) -> String {
    let base = placed.base as u64;
    let size = crate::own_range().1 as u64;
    if address >= base && address - base < size {
        alloc::format!("image+{:#x}", address - base)
    } else {
        alloc::format!("{:#x}", address)
    }
}

const NOTHING_MORE: u32 = 0;

fn watch_the_threads() {
    return;
    #[allow(unreachable_code)]
    if WATCHING.swap(true, core::sync::atomic::Ordering::AcqRel) {
        return;
    }

    unsafe {
        _tracepoint_probe_register(___tracepoint_sched_process_fork,
            THREAD_APPEARED_PROBE as *mut c_void, ptr::null_mut());
        _tracepoint_probe_register(___tracepoint_sched_process_exit,
            THREAD_LEFT_PROBE as *mut c_void, ptr::null_mut());
    }
}

pub fn hold_what_is_spawned() {
    return;
    #[allow(unreachable_code)]
    if HOLDING.swap(true, core::sync::atomic::Ordering::AcqRel) {
        return;
    }

    unsafe {
        _tracepoint_probe_register(___tracepoint_sched_process_exec,
            EXEC_PROBE as *mut c_void, ptr::null_mut());
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn frida_cb_exec(_data: *mut c_void, task: *mut c_void, _was: c_int,
        program: *mut c_void) {
    if !super::spawn::holds_this_one(task as usize) {
        return;
    }

    native::send_signal(STOP, task as usize);

    let Some(id) = id_of(task as usize) else {
        return;
    };
    let Some(at) = field_offset("linux_binprm", "filename") else {
        return;
    };
    super::spawn::note_a_held_spawn(id, unsafe {
        ((program as usize + at) as *const *const u8).read()
    });

    native::wake(task as *const u8);
}

static HOLDING: core::sync::atomic::AtomicBool = core::sync::atomic::AtomicBool::new(false);

const STOP: c_int = 19;

#[unsafe(no_mangle)]
pub unsafe extern "C" fn frida_cb_thread_appeared(_data: *mut c_void, _parent: *mut c_void,
        child: *mut c_void) {
    note_a_thread(child as usize, false);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn frida_cb_thread_left(_data: *mut c_void, task: *mut c_void, _group_dead: bool) {
    note_a_thread(task as usize, true);
}

fn note_a_thread(task: usize, is_gone: bool) {
    let Some(home) = group_of(task) else {
        return;
    };
    let Some(thread) = id_of(task) else {
        return;
    };

    let Some(placed) = (unsafe { placements() }).get(&home) else {
        return;
    };

    Arena::at(placed.arena).note_a_thread(thread, is_gone);
    wake_the_copy(placed);
}

fn group_of(task: usize) -> Option<u32> {
    let at = field_offset("task_struct", "tgid")?;

    Some(unsafe { ((task + at) as *const u32).read() })
}

pub fn id_of(task: usize) -> Option<u32> {
    let at = field_offset("task_struct", "pid")?;

    Some(unsafe { ((task + at) as *const u32).read() })
}

static COPY_TASK: core::sync::atomic::AtomicUsize = core::sync::atomic::AtomicUsize::new(0);

static WATCHING: core::sync::atomic::AtomicBool = core::sync::atomic::AtomicBool::new(false);

pub fn detach_from_process(id: u32) -> bool {
    let Some(placed) = (unsafe { placements() }.remove(&id)) else {
        return false;
    };

    let memory = unsafe { _get_task_mm(placed.task as *mut c_void) };
    ask_it_to_leave(&placed);
    take_back_what_it_was_given(&placed, memory);

    super::relay::forget(placed.arena as u64);

    true
}

pub fn stop_copies() {
    let homes: alloc::vec::Vec<u32> = unsafe { placements() }.keys().copied().collect();
    for home in homes {
        detach_from_process(home);
    }

    if !WATCHING.swap(false, core::sync::atomic::Ordering::AcqRel) {
        return;
    }

    unsafe {
        _tracepoint_probe_unregister(___tracepoint_sched_process_fork,
            THREAD_APPEARED_PROBE as *mut c_void, ptr::null_mut());
        _tracepoint_probe_unregister(___tracepoint_sched_process_exit,
            THREAD_LEFT_PROBE as *mut c_void, ptr::null_mut());
    }
}

fn ask_it_to_leave(placed: &Placement) {
    let arena = Arena::at(placed.arena);
    arena.tell_it_to_go();
    wake_the_copy(placed);

    let waited_on = &placed.arena as *const usize as *const u8;
    while !arena.has_gone() {
        native::wait(waited_on, Some(LEAVING_GRACE_US), &mut || arena.has_gone());
    }
}

fn take_back_what_it_was_given(placed: &Placement, memory: *mut c_void) {
    if !placed.says.is_null() {
        native::let_the_file_go(placed.says);
        native::let_the_file_go(placed.hears);
    }

    unsafe { _vunmap(placed.arena as *mut c_void) };

    if memory.is_null() {
        return;
    }

    let (_, image) = crate::own_range();
    unsafe {
        _kthread_use_mm(memory);
        _vm_munmap(placed.base, image + ARENA_SIZE);
        _vm_munmap(placed.stack + STACK_HEADROOM - STACK_SIZE, STACK_SIZE);
        _kthread_unuse_mm(memory);

        _mmput(memory);
    }
}

const LEAVING_GRACE_US: u64 = 100_000;

pub fn tell_the_copy(id: u32) {
    let Some(placed) = (unsafe { placements() }).get(&id) else {
        return;
    };

    bump(placed.arena + WOKEN);
    wake_the_copy(placed);
}

fn wake_the_copy(placed: &Placement) {
    if !placed.hears.is_null() {
        native::leave_a_word(placed.hears);
    }
}

fn read_address(address: usize) -> u64 {
    unsafe { (address as *const u64).read_volatile() }
}

fn read_word(address: usize) -> u32 {
    unsafe { (address as *const u32).read_volatile() }
}

fn bump(address: usize) {
    unsafe { (address as *mut u32).write_volatile(read_word(address).wrapping_add(1)) };
}

unsafe fn placements() -> &'static mut BTreeMap<u32, Placement> {
    unsafe { (&raw mut PLACEMENTS).as_mut().unwrap() }
}

static mut PLACEMENTS: BTreeMap<u32, Placement> = BTreeMap::new();

pub(crate) const ARENA_SIZE: usize = 2 * 1024 * 1024;

#[cfg(target_arch = "arm")]
const THUMB_BIT: usize = 1;

#[cfg(not(target_arch = "arm"))]
const THUMB_BIT: usize = 0;

#[cfg(target_arch = "arm")]
const THUMB_STATE: usize = 0x20;

#[cfg(not(target_arch = "arm"))]
const THUMB_STATE: usize = 0;

#[cfg(target_arch = "aarch64")]
const USER_EXECUTION: usize = 0;

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
const USER_EXECUTION: usize = 0x202;

#[cfg(target_arch = "arm")]
const USER_EXECUTION: usize = 0x10;

const PROT_READ: usize = 1;
const PROT_WRITE: usize = 2;
const PROT_EXEC: usize = 4;
const MAP_PRIVATE: usize = 2;
const MAP_ANONYMOUS: usize = 0x20;
const FIRST_ERROR_ADDRESS: usize = usize::MAX - 4095;
const PAGE_SIZE: usize = 4096;
const FOLL_WRITE: c_int = 1;
const FUTEX_WAIT: c_int = 128;
const FUTEX_WAKE: c_int = 129;
const WAKE_EVERY_WAITER: u32 = i32::MAX as u32;

const CLONE_VM: usize = 0x100;
const CLONE_FS: usize = 0x200;
const CLONE_FILES: usize = 0x400;
pub const STACK_SIZE: usize = 1024 * 1024;
const STACK_HEADROOM: usize = 16;

unsafe extern "C" {
    #[cfg(not(target_arch = "x86"))]
    static _get_task_mm: unsafe extern "C" fn(*mut c_void) -> *mut c_void;
    #[cfg(not(target_arch = "x86"))]
    static _kthread_use_mm: unsafe extern "C" fn(*mut c_void);
    #[cfg(not(target_arch = "x86"))]
    static _kthread_unuse_mm: unsafe extern "C" fn(*mut c_void);
    #[cfg(not(target_arch = "x86"))]
    static _mmput: unsafe extern "C" fn(*mut c_void);
    #[cfg(not(target_arch = "x86"))]
    static _vm_mmap: unsafe extern "C" fn(*mut c_void, usize, usize, usize, usize, usize) -> usize;
    #[cfg(not(target_arch = "x86"))]
    static _vm_munmap: unsafe extern "C" fn(usize, usize) -> c_int;
    #[cfg(not(target_arch = "x86"))]
    static _tracepoint_probe_register:
        unsafe extern "C" fn(*mut c_void, *mut c_void, *mut c_void) -> c_int;
    #[cfg(not(target_arch = "x86"))]
    static _tracepoint_probe_unregister:
        unsafe extern "C" fn(*mut c_void, *mut c_void, *mut c_void) -> c_int;
    static ___tracepoint_sched_process_fork: *mut c_void;
    static ___tracepoint_sched_process_exit: *mut c_void;
    static ___tracepoint_sched_process_exec: *mut c_void;
    #[cfg(not(target_arch = "x86"))]
    static _vunmap: unsafe extern "C" fn(*mut c_void);
    #[cfg(not(any(target_arch = "arm", target_arch = "x86", target_arch = "x86_64")))]
    static ___arch_copy_to_user: unsafe extern "C" fn(*mut c_void, *const c_void, usize) -> usize;
    #[cfg(target_arch = "x86_64")]
    static __copy_to_user: unsafe extern "C" fn(*mut c_void, *const c_void, usize) -> usize;
    #[cfg(target_arch = "arm")]
    static _arm_copy_to_user: unsafe extern "C" fn(*mut c_void, *const c_void, usize) -> usize;
    #[cfg(not(target_arch = "x86"))]
    static _down_read: unsafe extern "C" fn(*mut c_void);
    #[cfg(not(target_arch = "x86"))]
    static _up_read: unsafe extern "C" fn(*mut c_void);
    #[cfg(not(target_arch = "x86"))]
    static _get_user_pages_unlocked:
        unsafe extern "C" fn(usize, usize, *mut c_void, c_int) -> isize;
    #[cfg(not(target_arch = "x86"))]
    static _get_user_pages_remote: unsafe extern "C" fn(
        *mut c_void,
        usize,
        usize,
        c_int,
        *mut c_void,
        *mut c_void,
    ) -> isize;
    static ___arch_copy_from_user:
        unsafe extern "C" fn(*mut c_void, *const c_void, usize) -> usize;
    #[cfg(not(target_arch = "x86"))]
    static _do_futex: unsafe extern "C" fn(usize, c_int, u32, usize, usize, u32, u32) -> isize;
    #[cfg(not(target_arch = "x86"))]
    static _user_mode_thread:
        unsafe extern "C" fn(unsafe extern "C" fn(*mut c_void) -> c_int, *mut c_void, usize) -> c_int;
    #[cfg(target_arch = "aarch64")]
    static _apply_to_existing_page_range: Option<
        unsafe extern "C" fn(
            *mut c_void,
            usize,
            usize,
            unsafe extern "C" fn(*mut c_void, usize, *mut c_void) -> c_int,
            *mut c_void,
        ) -> c_int,
    >;
    #[cfg(target_arch = "aarch64")]
    static _caches_clean_inval_pou: Option<unsafe extern "C" fn(usize, usize)>;
}

#[cfg(target_arch = "x86")]
unsafe extern "C" {
    #[link_name = "frida_k_get_task_mm"]
    fn _get_task_mm(a0: *mut c_void) -> *mut c_void;
    #[link_name = "frida_k_kthread_use_mm"]
    fn _kthread_use_mm(a0: *mut c_void);
    #[link_name = "frida_k_kthread_unuse_mm"]
    fn _kthread_unuse_mm(a0: *mut c_void);
    #[link_name = "frida_k_mmput"]
    fn _mmput(a0: *mut c_void);
    #[link_name = "frida_k_vm_mmap"]
    fn _vm_mmap(a0: *mut c_void, a1: usize, a2: usize, a3: usize, a4: usize, a5: usize) -> usize;
    #[link_name = "frida_k_vm_munmap"]
    fn _vm_munmap(a0: usize, a1: usize) -> c_int;
    #[link_name = "frida_k_vunmap"]
    fn _vunmap(a0: *mut c_void);
    #[link_name = "frida_k__copy_to_user"]
    fn __copy_to_user(a0: *mut c_void, a1: *const c_void, a2: usize) -> usize;
    #[link_name = "frida_k_down_read"]
    fn _down_read(a0: *mut c_void);
    #[link_name = "frida_k_up_read"]
    fn _up_read(a0: *mut c_void);
    #[link_name = "frida_k_get_user_pages_remote"]
    fn _get_user_pages_remote(a0: *mut c_void, a1: usize, a2: usize, a3: c_int, a4: *mut c_void, a5: *mut c_void) -> isize;
    #[link_name = "frida_k_do_futex"]
    fn _do_futex(a0: usize, a1: c_int, a2: u32, a3: usize, a4: usize, a5: u32, a6: u32) -> isize;
}

#[cfg(target_arch = "x86")]
unsafe extern "C" {
    #[link_name = "frida_k_get_user_pages_unlocked"]
    fn _get_user_pages_unlocked(a0: usize, a1: usize, a2: *mut c_void, a3: c_int) -> isize;
    #[link_name = "frida_k_tracepoint_probe_register"]
    fn _tracepoint_probe_register(a0: *mut c_void, a1: *mut c_void, a2: *mut c_void) -> c_int;
    #[link_name = "frida_k_tracepoint_probe_unregister"]
    fn _tracepoint_probe_unregister(a0: *mut c_void, a1: *mut c_void, a2: *mut c_void) -> c_int;
    #[link_name = "frida_k_user_mode_thread"]
    fn _user_mode_thread(a0: unsafe extern "C" fn(*mut c_void) -> c_int, a1: *mut c_void,
        a2: usize) -> c_int;
    fn frida_kcb_user(argument: *mut c_void) -> c_int;
    fn frida_kcb_exec(data: *mut c_void, task: *mut c_void, was: c_int, program: *mut c_void);
    fn frida_kcb_thread_appeared(data: *mut c_void, parent: *mut c_void, child: *mut c_void);
    fn frida_kcb_thread_left(data: *mut c_void, task: *mut c_void, group_dead: bool);
}

#[cfg(target_arch = "x86")]
const USER_ENTRY: unsafe extern "C" fn(*mut c_void) -> c_int = frida_kcb_user;
#[cfg(not(target_arch = "x86"))]
const USER_ENTRY: unsafe extern "C" fn(*mut c_void) -> c_int = frida_cb_user;

#[cfg(target_arch = "x86")]
const EXEC_PROBE: unsafe extern "C" fn(*mut c_void, *mut c_void, c_int, *mut c_void) =
    frida_kcb_exec;
#[cfg(not(target_arch = "x86"))]
const EXEC_PROBE: unsafe extern "C" fn(*mut c_void, *mut c_void, c_int, *mut c_void) =
    frida_cb_exec;

#[cfg(target_arch = "x86")]
const THREAD_APPEARED_PROBE: unsafe extern "C" fn(*mut c_void, *mut c_void, *mut c_void) =
    frida_kcb_thread_appeared;
#[cfg(not(target_arch = "x86"))]
const THREAD_APPEARED_PROBE: unsafe extern "C" fn(*mut c_void, *mut c_void, *mut c_void) =
    frida_cb_thread_appeared;

#[cfg(target_arch = "x86")]
const THREAD_LEFT_PROBE: unsafe extern "C" fn(*mut c_void, *mut c_void, bool) =
    frida_kcb_thread_left;
#[cfg(not(target_arch = "x86"))]
const THREAD_LEFT_PROBE: unsafe extern "C" fn(*mut c_void, *mut c_void, bool) =
    frida_cb_thread_left;
