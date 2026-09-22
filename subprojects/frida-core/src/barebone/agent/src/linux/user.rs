use alloc::string::String;
use alloc::vec::Vec;
use core::ffi::c_void;

use crate::kernel::ThreadEntry;

use super::arena::{Arena, FAULT_ADDRESS, FAULT_KIND, FAULT_LR, FAULT_PC, WOKEN};
use super::facade::Primitives;

pub fn entry_offset() -> usize {
    (frida_linux_user_entry as usize) - crate::own_range().0
}

// The copy's first word in the address space it was placed in: it says it is up, and from then
// on it sleeps until the kernel half has something for it. Nothing here looks at a clock.
pub extern "C" fn frida_linux_user_entry(begins: usize) -> ! {
    let arena = Arena::at(begins);

    super::facade::select_user();
    unsafe { ARENA = begins };
    conceal_thread(current_thread_id() as u32);
    let (image_base, image_size) = crate::own_range();
    conceal_range(image_base as u64, (image_size + super::injection::ARENA_SIZE) as u64);
    stand_on_a_thread_pointer();
    install_fault_reporter();

    unsafe { crate::init_gum_without_exceptor() };
    let context = unsafe { crate::adopt_js_context() };
    unsafe { CONTEXT = context };
    unsafe { crate::route_frames_through(begins as u64) };

    open_the_channels(arena);

    arena.report_home();
    arena.tell_the_kernel_half();

    crate::watch_for_work(context, a_frame_is_waiting, serve_the_copy);
    crate::watch_a_descriptor(context, unsafe { LOOP_WAKEUP_POLL_FD }, frida_poll);

    loop {
        unsafe { crate::dispatch_pending_work(context) };
    }
}

fn a_frame_is_waiting() -> bool {
    let arena = Arena::at(unsafe { ARENA });
    arena.was_told_to_go()
        || arena.holds_a_thread()
        || super::relay::holds_a_frame_from_host(unsafe { ARENA } as u64)
}

const CLOAK_MAX: usize = 32;
static mut CLOAK_RANGES: [(u64, u64); CLOAK_MAX] = [(0, 0); CLOAK_MAX];
static mut CLOAK_THREADS: [u32; CLOAK_MAX] = [0; CLOAK_MAX];
static mut CLOAK_FDS: [u32; CLOAK_MAX] = [0; CLOAK_MAX];

pub fn conceal_range(base: u64, size: u64) {
    if base == 0 || size == 0 {
        return;
    }
    let ranges = unsafe { (&raw mut CLOAK_RANGES).as_mut().unwrap() };
    for slot in ranges.iter_mut() {
        if slot.1 == 0 {
            *slot = (base, size);
            return;
        }
    }
}

pub fn conceal_thread(id: u32) {
    remember(unsafe { (&raw mut CLOAK_THREADS).as_mut().unwrap() }, id);
}

pub fn conceal_fd(fd: u32) {
    remember(unsafe { (&raw mut CLOAK_FDS).as_mut().unwrap() }, fd);
}

fn remember(kept: &mut [u32; CLOAK_MAX], value: u32) {
    if value == 0 {
        return;
    }
    for slot in kept.iter_mut() {
        if *slot == 0 {
            *slot = value;
            return;
        }
    }
}

pub fn range_is_ours(address: u64) -> bool {
    unsafe { CLOAK_RANGES }
        .iter()
        .any(|&(base, size)| size != 0 && address >= base && address < base + size)
}

pub fn thread_is_ours(id: u32) -> bool {
    id != 0 && unsafe { CLOAK_THREADS }.contains(&id)
}

pub fn fd_is_ours(fd: u32) -> bool {
    fd != 0 && unsafe { CLOAK_FDS }.contains(&fd)
}

fn serve_the_copy() {
    drain_loop_wakeup();
    let arena = Arena::at(unsafe { ARENA });

    while let Some((thread, is_gone)) = arena.take_a_thread() {
        if is_gone {
            crate::gum_injected::thread_vanished(thread);
        } else {
            crate::gum_injected::thread_appeared(thread);
        }
    }

    while let Some(frame) = super::relay::take_frame_from_host(unsafe { ARENA } as u64) {
        crate::on_frame_from_host(&frame);
    }

    if arena.was_told_to_go() {
        leave();
    }
}

fn leave() -> ! {
    unsafe { crate::destroy_all_scripts(CONTEXT) };

    for descriptor in unsafe { [SAYING, SAYS_THROUGH, LOOP_WAKEUP_POLL_FD as u32, LOOP_WAKEUP_WRITE_FD as u32] } {
        syscall(CLOSE, descriptor as usize, 0, 0, 0, 0, 0);
    }

    Arena::at(unsafe { ARENA }).gone();
    syscall(EXIT_GROUP, 0, 0, 0, 0, 0, 0);

    loop {}
}

pub static USER: Primitives = Primitives {
    log,
    panic,
    alloc,
    free,
    alloc_heap,
    alloc_code,
    free_code,
    page_size,
    protect,
    spawn_thread,
    wait,
    wake,
    yield_now,
    monotonic_micros,
    wall_clock_micros,
    home_process_id,
    current_process_id,
    current_thread_id,
    install_fault_reporter,
    poke_loop_wakeup,
};

// The copy has no file to write to -- what it was given came from a thread of the kernel -- so
// what it has to say goes where the kernel half can read it.
fn log(msg: &str) {
    let arena = Arena::at(unsafe { ARENA });
    arena.say(msg);
    arena.note(SPOKE);
    arena.tell_the_kernel_half();
}

fn panic(msg: &str) -> ! {
    let arena = Arena::at(unsafe { ARENA });
    arena.say(msg);
    arena.note(PANICKED);
    arena.tell_the_kernel_half();

    syscall(EXIT_GROUP, 1, 0, 0, 0, 0, 0);

    loop {}
}

fn alloc(size: usize) -> *mut u8 {
    crate::heap::take(size)
}

fn free(ptr: *mut u8, _size: usize) {
    crate::heap::give_back(ptr);
}

// What the C library hands out is served from this, so it cannot come from the C library.
fn alloc_heap(size: usize) -> *mut u8 {
    map_writable(size)
}

fn alloc_code(size: usize) -> *mut u8 {
    map(size, READABLE | WRITABLE | EXECUTABLE)
}

fn free_code(ptr: *mut u8, size: usize) {
    syscall(MUNMAP, ptr as usize, size, 0, 0, 0, 0);
}

fn protect(address: u64, size: usize, protection: u32) -> bool {
    #[cfg(target_arch = "aarch64")]
    if (protection & GUM_PAGE_EXECUTE) != 0 {
        return ask_the_kernel_half_to_make_executable(address, size);
    }

    let wanted = ((protection & GUM_PAGE_READ) != 0) as usize * READABLE
        | ((protection & GUM_PAGE_WRITE) != 0) as usize * WRITABLE
        | ((protection & GUM_PAGE_EXECUTE) != 0) as usize * EXECUTABLE;

    let page = page_size();
    let first = (address as usize) & !(page - 1);
    let span = ((address as usize) + size + page - 1 & !(page - 1)) - first;

    syscall(MPROTECT, first, span, wanted, 0, 0, 0) == 0
}

#[cfg(target_arch = "aarch64")]
fn ask_the_kernel_half_to_make_executable(address: u64, size: usize) -> bool {
    let arena = Arena::at(unsafe { ARENA });
    let page = page_size();
    let first = (address as usize) & !(page - 1);
    let span = (((address as usize) + size + page - 1) & !(page - 1)) - first;
    let seq = arena.request_executable(first as u64, span as u64);
    arena.tell_the_kernel_half();
    while !arena.executable_settled(seq) {
        yield_now();
    }
    arena.executable_was_granted()
}

pub fn map_writable(size: usize) -> *mut u8 {
    map(size, READABLE | WRITABLE)
}

fn map(size: usize, protection: usize) -> *mut u8 {
    let mapped = syscall(
        MMAP,
        0,
        size,
        protection,
        PRIVATE | ANONYMOUS,
        NO_FILE,
        0,
    );
    if is_error(mapped) {
        return core::ptr::null_mut();
    }

    mapped as *mut u8
}

fn is_error(answer: isize) -> bool {
    (answer as usize) >= LAST_ERROR_ANSWER
}

const LAST_ERROR_ANSWER: usize = usize::MAX - 4095;

fn spawn_thread(entry: ThreadEntry, parameter: *mut c_void) -> isize {
    let stack = map_writable(THREAD_STACK_SIZE);
    if stack.is_null() {
        return -1;
    }
    conceal_range(stack as u64, THREAD_STACK_SIZE as u64);

    let carried = alloc(size_of::<Carried>()) as *mut Carried;
    unsafe { carried.write(Carried { entry, parameter }) };

    let top = unsafe { stack.add(THREAD_STACK_SIZE) } as usize;

    let id = unsafe { start_thread(top, carried as usize) };
    if id > 0 {
        conceal_thread(id as u32);
    }
    id
}

// A task the kernel half makes has no thread pointer, and code of the process reads what it
// keeps there: musl's errno lives below it, and reading that at zero is what kills the copy the
// moment it calls into the C library it was placed beside.
#[cfg(target_arch = "arm")]
fn stand_on_a_thread_pointer() {
    let block = map_writable(THREAD_POINTER_BLOCK);
    if block.is_null() {
        return;
    }

    let pointer = unsafe { block.add(THREAD_POINTER_BLOCK / 2) } as usize;
    unsafe { core::arch::asm!("mcr p15, 0, {}, c13, c0, 2", in(reg) pointer, options(nomem, nostack)) };
}

#[cfg(target_arch = "x86")]
fn stand_on_a_thread_pointer() {
    let block = map_writable(THREAD_POINTER_BLOCK);
    if block.is_null() {
        return;
    }

    let pointer = unsafe { block.add(THREAD_POINTER_BLOCK / 2) } as usize;
    unsafe { (pointer as *mut usize).write(pointer) };

    let mut area = UserDesc {
        entry_number: u32::MAX,
        base_address: pointer as u32,
        limit: 0xfffff,
        flags: SEGMENT_FLAGS,
    };
    if syscall(SET_THREAD_AREA, &raw mut area as usize, 0, 0, 0, 0, 0) != 0 {
        return;
    }

    let selector = (area.entry_number << 3) | 3;
    unsafe { core::arch::asm!("mov gs, {:e}", in(reg) selector, options(nomem, nostack)) };
}

#[repr(C)]
struct UserDesc {
    entry_number: u32,
    base_address: u32,
    limit: u32,
    flags: u32,
}

#[cfg(target_arch = "x86")]
const SEGMENT_FLAGS: u32 = 0x51;

#[cfg(target_arch = "x86_64")]
fn stand_on_a_thread_pointer() {
    let block = map_writable(THREAD_POINTER_BLOCK);
    if block.is_null() {
        return;
    }

    let pointer = unsafe { block.add(THREAD_POINTER_BLOCK / 2) } as usize;
    unsafe { (pointer as *mut usize).write(pointer) };
    syscall(ARCH_PRCTL, ARCH_SET_FS, pointer, 0, 0, 0, 0);
}

#[cfg(target_arch = "aarch64")]
fn stand_on_a_thread_pointer() {
    let block = map_writable(THREAD_POINTER_BLOCK);
    if block.is_null() {
        return;
    }

    let pointer = unsafe { block.add(THREAD_POINTER_BLOCK / 2) } as usize;
    unsafe { core::arch::asm!("msr tpidr_el0, {}", in(reg) pointer, options(nomem, nostack)) };
}

const THREAD_POINTER_BLOCK: usize = 8192;

#[cfg(target_arch = "x86")]
const SET_THREAD_AREA: usize = 243;

#[cfg(target_arch = "x86_64")]
const ARCH_PRCTL: usize = 158;
#[cfg(target_arch = "x86_64")]
const ARCH_SET_FS: usize = 0x1002;

struct Carried {
    entry: ThreadEntry,
    parameter: *mut c_void,
}

unsafe extern "C" fn enter_thread(carried: usize) -> ! {
    stand_on_a_thread_pointer();

    let carried = carried as *mut Carried;
    unsafe {
        let Carried { entry, parameter } = carried.read();
        free(carried as *mut u8, size_of::<Carried>());

        entry(parameter, 0);
    }

    syscall(EXIT, 0, 0, 0, 0, 0, 0);

    loop {}
}

// The thread the kernel makes here starts on a stack of its own with nothing on it, so where it
// goes and what it is given are put in the registers the child keeps across the call.
#[cfg(target_arch = "arm")]
unsafe fn start_thread(stack: usize, carried: usize) -> isize {
    let spawned: isize;

    unsafe {
        core::arch::asm!(
            "push {{r7}}",
            "mov r7, {number}",
            "svc #0",
            "pop {{r7}}",
            "cmp r0, #0",
            "bne 2f",
            "mov r0, r5",
            "blx r4",
            "2:",
            number = const CLONE,
            inlateout("r0") THREAD_FLAGS => spawned,
            inlateout("r1") stack => _,
            inlateout("r2") 0 => _,
            inlateout("r3") 0 => _,
            in("r4") enter_thread,
            in("r5") carried,
            lateout("r12") _,
            lateout("lr") _,
        );
    }

    spawned
}

#[cfg(target_arch = "x86")]
core::arch::global_asm!(
    ".globl frida_start_thread",
    ".hidden frida_start_thread",
    "frida_start_thread:",
    "push ebp",
    "push ebx",
    "push esi",
    "push edi",
    "mov eax, [esp + 20]",
    "mov ebx, [esp + 24]",
    "mov ecx, [esp + 28]",
    "xor edx, edx",
    "xor esi, esi",
    "xor edi, edi",
    "int 0x80",
    "test eax, eax",
    "jnz 2f",
    "pop ecx",
    "call ecx",
    "ud2",
    "2:",
    "pop edi",
    "pop esi",
    "pop ebx",
    "pop ebp",
    "ret",
);

#[cfg(target_arch = "x86")]
unsafe extern "C" {
    fn frida_start_thread(number: usize, flags: usize, launch: usize) -> isize;
}

#[cfg(target_arch = "x86")]
unsafe fn start_thread(stack: usize, carried: usize) -> isize {
    let launch = stack - 8;
    unsafe {
        (launch as *mut usize).write(enter_thread as usize);
        ((launch + 4) as *mut usize).write(carried);

        frida_start_thread(CLONE, THREAD_FLAGS, launch)
    }
}

#[cfg(target_arch = "x86_64")]
unsafe fn start_thread(stack: usize, carried: usize) -> isize {
    let spawned: isize;

    unsafe {
        core::arch::asm!(
            "syscall",
            "test rax, rax",
            "jnz 2f",
            "mov rdi, r13",
            "call r12",
            "2:",
            inlateout("rax") CLONE => spawned,
            in("rdi") THREAD_FLAGS,
            in("rsi") stack,
            in("rdx") 0,
            in("r10") 0,
            in("r8") 0,
            in("r12") enter_thread,
            in("r13") carried,
            lateout("rcx") _,
            lateout("r11") _,
        );
    }

    spawned
}

#[cfg(target_arch = "aarch64")]
unsafe fn start_thread(stack: usize, carried: usize) -> isize {
    let spawned: isize;

    unsafe {
        core::arch::asm!(
            "svc #0",
            "cbnz x0, 2f",
            "mov x0, x22",
            "blr x21",
            "2:",
            in("x8") CLONE,
            inlateout("x0") THREAD_FLAGS => spawned,
            in("x1") stack,
            in("x2") 0,
            in("x3") 0,
            in("x4") 0,
            in("x21") enter_thread,
            in("x22") carried,
            clobber_abi("C"),
        );
    }

    spawned
}

fn open_the_channels(arena: Arena) {
    let mut says = [0u32; 2];
    syscall(PIPE, says.as_mut_ptr() as usize, 0, 0, 0, 0, 0);

    let (poll_fd, write_fd) = make_loop_wakeup();

    unsafe {
        SAYING = says[1];
        SAYS_THROUGH = says[0];
        LOOP_WAKEUP_POLL_FD = poll_fd;
        LOOP_WAKEUP_WRITE_FD = write_fd;
    }
    for fd in [says[0], says[1], poll_fd as u32, write_fd as u32] {
        conceal_fd(fd);
    }

    arena.reachable_at(says[0], write_fd as u32);
}

pub fn say_something() {
    let byte = 0u8;
    syscall(WRITE, unsafe { SAYING } as usize, &byte as *const u8 as usize, 1, 0, 0, 0);
}

static mut SAYING: u32 = 0;
static mut SAYS_THROUGH: u32 = 0;
static mut LOOP_WAKEUP_POLL_FD: i32 = -1;
static mut LOOP_WAKEUP_WRITE_FD: i32 = -1;

fn make_loop_wakeup() -> (i32, i32) {
    let mut ends = [0u32; 2];
    syscall(PIPE, ends.as_mut_ptr() as usize, 0, 0, 0, 0, 0);
    syscall(FCNTL, ends[0] as usize, F_SETFL, O_NONBLOCK, 0, 0, 0);
    (ends[0] as i32, ends[1] as i32)
}

fn poke_loop_wakeup() {
    let fd = unsafe { LOOP_WAKEUP_WRITE_FD };
    if fd < 0 {
        return;
    }
    let one: u64 = 1;
    syscall(WRITE, fd as usize, &one as *const u64 as usize, 8, 0, 0, 0);
}

fn drain_loop_wakeup() {
    let fd = unsafe { LOOP_WAKEUP_POLL_FD };
    if fd < 0 {
        return;
    }
    let mut sink = [0u8; 64];
    loop {
        let read = syscall(READ, fd as usize, sink.as_mut_ptr() as usize, sink.len(), 0, 0, 0);
        if read < sink.len() as isize {
            break;
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
struct PollFd {
    fd: i32,
    events: i16,
    revents: i16,
}

#[repr(C)]
struct TimeSpec {
    seconds: i64,
    nanoseconds: i64,
}

unsafe extern "C" fn frida_poll(ufds: *mut crate::GPollFd, nfds: u32, timeout: i32) -> i32 {
    const MAX_FDS: usize = 16;
    let fds = unsafe { core::slice::from_raw_parts_mut(ufds, nfds as usize) };

    let mut real = [PollFd { fd: 0, events: 0, revents: 0 }; MAX_FDS];
    let mut origin = [0usize; MAX_FDS];
    let mut count = 0usize;
    let mut ready = 0i32;

    for (index, fd) in fds.iter_mut().enumerate() {
        fd.revents = 0;

        if fd.fd == G_WAIT_WAKEUP_HANDLE {
            if !fd.user_data.is_null() {
                let signalled = unsafe { (fd.user_data as *const i32).read_volatile() };
                if signalled != 0 {
                    fd.revents = POLL_IN as u16;
                    ready += 1;
                }
                let token = (fd.user_data as usize + GWAKEUP_TOKEN) as *mut *mut c_void;
                unsafe { token.write(fd.user_data) };
            }
        } else if fd.fd >= 0 && count < MAX_FDS {
            real[count] = PollFd { fd: fd.fd, events: fd.events as i16, revents: 0 };
            origin[count] = index;
            count += 1;
        }
    }

    let waiting = if ready > 0 { 0 } else { timeout };
    let span = TimeSpec {
        seconds: (waiting as i64) / 1000,
        nanoseconds: ((waiting as i64) % 1000) * 1_000_000,
    };
    let deadline = if waiting < 0 {
        core::ptr::null::<TimeSpec>()
    } else {
        &span as *const TimeSpec
    };

    syscall(PPOLL, real.as_mut_ptr() as usize, count, deadline as usize, 0, 0, 0);

    for slot in 0..count {
        if real[slot].revents != 0 {
            fds[origin[slot]].revents = real[slot].revents as u16;
            ready += 1;
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

const G_WAIT_WAKEUP_HANDLE: i32 = -43;
const GWAKEUP_TOKEN: usize = 8;
const F_SETFL: usize = 4;
const O_NONBLOCK: usize = 0x800;

fn wait(_token: *const u8, timeout_us: Option<u64>, check: &mut dyn FnMut() -> bool) {
    let unchanged = unchanged();
    if check() {
        return;
    }

    match timeout_us {
        Some(us) => {
            let until = [(us / 1_000_000) as i64, ((us % 1_000_000) * 1000) as i64];
            syscall(NANOSLEEP, until.as_ptr() as usize, 0, 0, 0, 0, 0);
        }
        None => wait_on(waited_on(), unchanged, None),
    }
}

fn wake(_token: *const u8) {
    bump_what_is_waited_on();
    wake_up(waited_on());
}

fn yield_now() {
    syscall(SCHED_YIELD, 0, 0, 0, 0, 0, 0);
}

fn monotonic_micros() -> i64 {
    micros_of(MONOTONIC)
}

fn wall_clock_micros() -> (u32, u32) {
    let now = read_clock(REALTIME);

    (now[0] as u32, (now[1] / 1000) as u32)
}

fn micros_of(clock: usize) -> i64 {
    let now = read_clock(clock);

    (now[0] * 1_000_000) + (now[1] / 1000)
}

fn read_clock(clock: usize) -> [i64; 2] {
    let mut now = [0i64; 2];
    syscall(CLOCK_GETTIME, clock, now.as_mut_ptr() as usize, 0, 0, 0, 0);

    now
}

// The copy has a task of its own, so which process it belongs to is what it was told.
fn home_process_id() -> u32 {
    Arena::at(unsafe { ARENA }).home()
}

fn current_process_id() -> u32 {
    syscall(GETPID, 0, 0, 0, 0, 0, 0) as u32
}

fn current_thread_id() -> u64 {
    syscall(GETTID, 0, 0, 0, 0, 0, 0) as u64
}

// A fault in the copy is nobody else's to report: the kernel kills the task and says nothing,
// so what it was is written down where the kernel half can read it.
fn install_fault_reporter() {
    for signal in [ILLEGAL, ABORTED, BUS, SEGMENT, ARITHMETIC, TRAPPED, BAD_CALL] {
        let action = Action {
            handler: report_fault as usize,
            flags: SIGINFO | RESTORER_FLAG,
            restorer: restorer(),
            blocked: 0,
        };
        let mut previous = Action { handler: 0, flags: 0, restorer: 0, blocked: 0 };

        syscall(
            RT_SIGACTION,
            signal,
            &action as *const Action as usize,
            &mut previous as *mut Action as usize,
            size_of::<u64>(),
            0,
            0,
        );

        unsafe { PREVIOUS[signal] = previous };
    }
}

#[derive(Clone, Copy)]
#[repr(C)]
struct Action {
    handler: usize,
    flags: usize,
    restorer: usize,
    blocked: u64,
}

static mut PREVIOUS: [Action; 32] = [Action { handler: 0, flags: 0, restorer: 0, blocked: 0 }; 32];

unsafe extern "C" fn report_fault(signal: usize, about: usize, running: usize) {
    if unsafe { recovered(signal, about, running) } {
        return;
    }

    if unsafe { chained_to_previous(signal, about, running) } {
        return;
    }

    let arena = Arena::at(unsafe { ARENA });

    arena.note(FAULTED);
    unsafe {
        arena.leave(
            FAULT_KIND,
            (signal as u64) | ((((about + ABOUT_CODE) as *const u32).read() as u64) << 32),
        );
        arena.leave(FAULT_ADDRESS, ((about + ABOUT_ADDRESS) as *const usize).read() as u64);
        arena.leave(
            FAULT_PC,
            ((running + RUNNING_STATE + STATE_PC) as *const usize).read() as u64,
        );
        arena.leave(
            FAULT_LR,
            ((running + RUNNING_STATE + STATE_LR) as *const usize).read() as u64,
        );
    }
    arena.tell_the_kernel_half();

    syscall(EXIT_GROUP, 1, 0, 0, 0, 0, 0);
}

#[cfg(any(
    target_arch = "aarch64",
    target_arch = "x86_64",
    target_arch = "x86",
    target_arch = "arm"
))]
unsafe fn recovered(signal: usize, about: usize, running: usize) -> bool {
    let mut context = unsafe { cpu_context_at(running) };
    let faulted_at = unsafe { ((running + RUNNING_STATE + STATE_PC) as *const usize).read() };
    let accessed = unsafe { ((about + ABOUT_ADDRESS) as *const usize).read() };

    let handled = unsafe {
        crate::bindings::gum_barebone_handle_exception(
            fault_type_of(signal),
            faulted_at as *mut c_void,
            accessed as *mut c_void,
            &mut context,
        )
    };
    if handled == 0 {
        return false;
    }

    unsafe { restore_cpu_context(running, &context) };
    true
}

#[cfg(not(any(
    target_arch = "aarch64",
    target_arch = "x86_64",
    target_arch = "x86",
    target_arch = "arm"
)))]
unsafe fn recovered(_signal: usize, _about: usize, _running: usize) -> bool {
    false
}

unsafe fn chained_to_previous(signal: usize, about: usize, running: usize) -> bool {
    let previous = unsafe { PREVIOUS[signal] };
    if previous.handler <= SIG_IGN {
        return false;
    }

    let deliver: unsafe extern "C" fn(usize, usize, usize) =
        unsafe { core::mem::transmute(previous.handler) };
    unsafe { deliver(signal, about, running) };
    true
}

const SIG_IGN: usize = 1;

fn fault_type_of(signal: usize) -> crate::bindings::GumExceptionType {
    use crate::bindings::{
        _GumExceptionType_GUM_EXCEPTION_ABORT, _GumExceptionType_GUM_EXCEPTION_ACCESS_VIOLATION,
        _GumExceptionType_GUM_EXCEPTION_ARITHMETIC, _GumExceptionType_GUM_EXCEPTION_BREAKPOINT,
        _GumExceptionType_GUM_EXCEPTION_ILLEGAL_INSTRUCTION, _GumExceptionType_GUM_EXCEPTION_SYSTEM,
    };

    match signal {
        SEGMENT | BUS => _GumExceptionType_GUM_EXCEPTION_ACCESS_VIOLATION,
        ILLEGAL | BAD_CALL => _GumExceptionType_GUM_EXCEPTION_ILLEGAL_INSTRUCTION,
        ARITHMETIC => _GumExceptionType_GUM_EXCEPTION_ARITHMETIC,
        TRAPPED => _GumExceptionType_GUM_EXCEPTION_BREAKPOINT,
        ABORTED => _GumExceptionType_GUM_EXCEPTION_ABORT,
        _ => _GumExceptionType_GUM_EXCEPTION_SYSTEM,
    }
}

#[cfg(target_arch = "aarch64")]
unsafe fn cpu_context_at(running: usize) -> crate::bindings::GumCpuContext {
    let regs = (running + RUNNING_STATE + REGS) as *const u64;
    let mut x = [0u64; 29];
    for slot in 0..29 {
        x[slot] = unsafe { regs.add(slot).read() };
    }
    crate::bindings::GumCpuContext {
        pc: unsafe { ((running + RUNNING_STATE + STATE_PC) as *const u64).read() },
        sp: unsafe { ((running + RUNNING_STATE + STATE_SP) as *const u64).read() },
        nzcv: unsafe { ((running + RUNNING_STATE + STATE_PSTATE) as *const u64).read() },
        x,
        fp: unsafe { regs.add(29).read() },
        lr: unsafe { regs.add(30).read() },
    }
}

#[cfg(target_arch = "aarch64")]
unsafe fn restore_cpu_context(running: usize, context: &crate::bindings::GumCpuContext) {
    let regs = (running + RUNNING_STATE + REGS) as *mut u64;
    for slot in 0..29 {
        unsafe { regs.add(slot).write(context.x[slot]) };
    }
    unsafe {
        regs.add(29).write(context.fp);
        regs.add(30).write(context.lr);
        ((running + RUNNING_STATE + STATE_SP) as *mut u64).write(context.sp);
        ((running + RUNNING_STATE + STATE_PC) as *mut u64).write(context.pc);
        ((running + RUNNING_STATE + STATE_PSTATE) as *mut u64).write(context.nzcv);
    }
}

#[cfg(target_arch = "aarch64")]
const REGS: usize = 8;
#[cfg(target_arch = "aarch64")]
const STATE_SP: usize = 256;
#[cfg(target_arch = "aarch64")]
const STATE_PSTATE: usize = 272;

#[cfg(target_arch = "x86_64")]
unsafe fn cpu_context_at(running: usize) -> crate::bindings::GumCpuContext {
    let gregs = (running + RUNNING_STATE) as *const u64;
    let mut context: crate::bindings::GumCpuContext = unsafe { core::mem::zeroed() };
    unsafe {
        context.rip = gregs.add(RIP).read();
        context.r15 = gregs.add(R15).read();
        context.r14 = gregs.add(R14).read();
        context.r13 = gregs.add(R13).read();
        context.r12 = gregs.add(R12).read();
        context.r11 = gregs.add(R11).read();
        context.r10 = gregs.add(R10).read();
        context.r9 = gregs.add(R9).read();
        context.r8 = gregs.add(R8).read();
        context.rdi = gregs.add(RDI).read();
        context.rsi = gregs.add(RSI).read();
        context.rbp = gregs.add(RBP).read();
        context.rsp = gregs.add(RSP).read();
        context.rbx = gregs.add(RBX).read();
        context.rdx = gregs.add(RDX).read();
        context.rcx = gregs.add(RCX).read();
        context.rax = gregs.add(RAX).read();
    }
    context
}

#[cfg(target_arch = "x86_64")]
unsafe fn restore_cpu_context(running: usize, context: &crate::bindings::GumCpuContext) {
    let gregs = (running + RUNNING_STATE) as *mut u64;
    unsafe {
        gregs.add(RIP).write(context.rip);
        gregs.add(R15).write(context.r15);
        gregs.add(R14).write(context.r14);
        gregs.add(R13).write(context.r13);
        gregs.add(R12).write(context.r12);
        gregs.add(R11).write(context.r11);
        gregs.add(R10).write(context.r10);
        gregs.add(R9).write(context.r9);
        gregs.add(R8).write(context.r8);
        gregs.add(RDI).write(context.rdi);
        gregs.add(RSI).write(context.rsi);
        gregs.add(RBP).write(context.rbp);
        gregs.add(RSP).write(context.rsp);
        gregs.add(RBX).write(context.rbx);
        gregs.add(RDX).write(context.rdx);
        gregs.add(RCX).write(context.rcx);
        gregs.add(RAX).write(context.rax);
    }
}

#[cfg(target_arch = "x86_64")]
const R8: usize = 0;
#[cfg(target_arch = "x86_64")]
const R9: usize = 1;
#[cfg(target_arch = "x86_64")]
const R10: usize = 2;
#[cfg(target_arch = "x86_64")]
const R11: usize = 3;
#[cfg(target_arch = "x86_64")]
const R12: usize = 4;
#[cfg(target_arch = "x86_64")]
const R13: usize = 5;
#[cfg(target_arch = "x86_64")]
const R14: usize = 6;
#[cfg(target_arch = "x86_64")]
const R15: usize = 7;
#[cfg(target_arch = "x86_64")]
const RDI: usize = 8;
#[cfg(target_arch = "x86_64")]
const RSI: usize = 9;
#[cfg(target_arch = "x86_64")]
const RBP: usize = 10;
#[cfg(target_arch = "x86_64")]
const RBX: usize = 11;
#[cfg(target_arch = "x86_64")]
const RDX: usize = 12;
#[cfg(target_arch = "x86_64")]
const RAX: usize = 13;
#[cfg(target_arch = "x86_64")]
const RCX: usize = 14;
#[cfg(target_arch = "x86_64")]
const RSP: usize = 15;
#[cfg(target_arch = "x86_64")]
const RIP: usize = 16;

#[cfg(target_arch = "x86")]
unsafe fn cpu_context_at(running: usize) -> crate::bindings::GumCpuContext {
    let gregs = (running + RUNNING_STATE) as *const u32;
    let mut context: crate::bindings::GumCpuContext = unsafe { core::mem::zeroed() };
    unsafe {
        context.eip = gregs.add(EIP).read();
        context.edi = gregs.add(EDI).read();
        context.esi = gregs.add(ESI).read();
        context.ebp = gregs.add(EBP).read();
        context.esp = gregs.add(ESP).read();
        context.ebx = gregs.add(EBX).read();
        context.edx = gregs.add(EDX).read();
        context.ecx = gregs.add(ECX).read();
        context.eax = gregs.add(EAX).read();
    }
    context
}

#[cfg(target_arch = "x86")]
unsafe fn restore_cpu_context(running: usize, context: &crate::bindings::GumCpuContext) {
    let gregs = (running + RUNNING_STATE) as *mut u32;
    unsafe {
        gregs.add(EIP).write(context.eip);
        gregs.add(EDI).write(context.edi);
        gregs.add(ESI).write(context.esi);
        gregs.add(EBP).write(context.ebp);
        gregs.add(ESP).write(context.esp);
        gregs.add(EBX).write(context.ebx);
        gregs.add(EDX).write(context.edx);
        gregs.add(ECX).write(context.ecx);
        gregs.add(EAX).write(context.eax);
    }
}

#[cfg(target_arch = "x86")]
const EDI: usize = 4;
#[cfg(target_arch = "x86")]
const ESI: usize = 5;
#[cfg(target_arch = "x86")]
const EBP: usize = 6;
#[cfg(target_arch = "x86")]
const ESP: usize = 7;
#[cfg(target_arch = "x86")]
const EBX: usize = 8;
#[cfg(target_arch = "x86")]
const EDX: usize = 9;
#[cfg(target_arch = "x86")]
const ECX: usize = 10;
#[cfg(target_arch = "x86")]
const EAX: usize = 11;
#[cfg(target_arch = "x86")]
const EIP: usize = 14;

#[cfg(target_arch = "arm")]
unsafe fn cpu_context_at(running: usize) -> crate::bindings::GumCpuContext {
    let regs = (running + RUNNING_STATE + ARM_R0) as *const u32;
    let mut context: crate::bindings::GumCpuContext = unsafe { core::mem::zeroed() };
    unsafe {
        for slot in 0..8 {
            context.r[slot] = regs.add(slot).read();
        }
        context.r8 = regs.add(8).read();
        context.r9 = regs.add(9).read();
        context.r10 = regs.add(10).read();
        context.r11 = regs.add(11).read();
        context.r12 = regs.add(12).read();
        context.sp = regs.add(13).read();
        context.lr = regs.add(14).read();
        context.pc = regs.add(15).read();
        context.cpsr = regs.add(16).read();
    }
    context
}

#[cfg(target_arch = "arm")]
unsafe fn restore_cpu_context(running: usize, context: &crate::bindings::GumCpuContext) {
    let regs = (running + RUNNING_STATE + ARM_R0) as *mut u32;
    unsafe {
        for slot in 0..8 {
            regs.add(slot).write(context.r[slot]);
        }
        regs.add(8).write(context.r8);
        regs.add(9).write(context.r9);
        regs.add(10).write(context.r10);
        regs.add(11).write(context.r11);
        regs.add(12).write(context.r12);
        regs.add(13).write(context.sp);
        regs.add(14).write(context.lr);
        regs.add(15).write(context.pc);
        regs.add(16).write(context.cpsr);
    }
}

#[cfg(target_arch = "arm")]
const ARM_R0: usize = 12;

#[cfg(any(
    target_arch = "aarch64",
    target_arch = "x86_64",
    target_arch = "x86",
    target_arch = "arm"
))]
const RESTORER_FLAG: usize = 0x0400_0000;
#[cfg(not(any(
    target_arch = "aarch64",
    target_arch = "x86_64",
    target_arch = "x86",
    target_arch = "arm"
)))]
const RESTORER_FLAG: usize = 0;

#[cfg(any(
    target_arch = "aarch64",
    target_arch = "x86_64",
    target_arch = "x86",
    target_arch = "arm"
))]
fn restorer() -> usize {
    sigreturn_trampoline as usize
}
#[cfg(not(any(
    target_arch = "aarch64",
    target_arch = "x86_64",
    target_arch = "x86",
    target_arch = "arm"
)))]
fn restorer() -> usize {
    0
}

#[cfg(target_arch = "aarch64")]
core::arch::global_asm!(
    ".globl sigreturn_trampoline",
    ".hidden sigreturn_trampoline",
    "sigreturn_trampoline:",
    "mov x8, {sysno}",
    "svc #0",
    sysno = const RT_SIGRETURN,
);
#[cfg(target_arch = "aarch64")]
const RT_SIGRETURN: usize = 139;

#[cfg(target_arch = "x86_64")]
core::arch::global_asm!(
    ".globl sigreturn_trampoline",
    ".hidden sigreturn_trampoline",
    "sigreturn_trampoline:",
    "mov rax, {sysno}",
    "syscall",
    sysno = const RT_SIGRETURN,
);
#[cfg(target_arch = "x86_64")]
const RT_SIGRETURN: usize = 15;

#[cfg(target_arch = "x86")]
core::arch::global_asm!(
    ".globl sigreturn_trampoline",
    ".hidden sigreturn_trampoline",
    "sigreturn_trampoline:",
    "mov eax, {sysno}",
    "int 0x80",
    sysno = const RT_SIGRETURN,
);
#[cfg(target_arch = "x86")]
const RT_SIGRETURN: usize = 173;

#[cfg(target_arch = "arm")]
core::arch::global_asm!(
    ".globl sigreturn_trampoline",
    ".hidden sigreturn_trampoline",
    "sigreturn_trampoline:",
    "mov r7, {sysno}",
    "svc #0",
    sysno = const RT_SIGRETURN,
);
#[cfg(target_arch = "arm")]
const RT_SIGRETURN: usize = 173;

#[cfg(any(
    target_arch = "aarch64",
    target_arch = "x86_64",
    target_arch = "x86",
    target_arch = "arm"
))]
unsafe extern "C" {
    fn sigreturn_trampoline();
}

// What the kernel was set up with is left in the arena, since asking the register is the
// kernel's own business.
fn page_size() -> usize {
    Arena::at(unsafe { ARENA }).page_size() as usize
}

pub fn names_in(directory: &core::ffi::CStr) -> Vec<String> {
    let listed = syscall(OPENAT, WORKING_DIRECTORY, directory.as_ptr() as usize, READ_ONLY, 0, 0, 0);
    if listed < 0 {
        return Vec::new();
    }

    let mut names = Vec::new();
    let mut chunk = [0u8; 4096];
    loop {
        let read = syscall(GETDENTS, listed as usize, chunk.as_mut_ptr() as usize, chunk.len(),
            0, 0, 0);
        if read <= 0 {
            break;
        }

        let mut at = 0;
        while at < read as usize {
            let entry = unsafe { chunk.as_ptr().add(at) };
            let length = unsafe { entry.add(16).cast::<u16>().read_unaligned() } as usize;
            let name = unsafe { core::ffi::CStr::from_ptr(entry.add(19).cast()) };
            names.push(String::from_utf8_lossy(name.to_bytes()).into_owned());
            at += length;
        }
    }
    syscall(CLOSE, listed as usize, 0, 0, 0, 0, 0);

    names
}

pub fn contents_of(path: &core::ffi::CStr) -> Vec<u8> {
    let file = syscall(OPENAT, WORKING_DIRECTORY, path.as_ptr() as usize, READ_ONLY, 0, 0, 0);
    if file < 0 {
        return Vec::new();
    }

    let mut said = Vec::new();
    let mut chunk = [0u8; 4096];
    loop {
        let read = syscall(READ, file as usize, chunk.as_mut_ptr() as usize, chunk.len(), 0, 0, 0);
        if read <= 0 {
            break;
        }
        said.extend_from_slice(&chunk[..read as usize]);
    }
    syscall(CLOSE, file as usize, 0, 0, 0, 0, 0);

    said
}

fn waited_on() -> usize {
    unsafe { ARENA + WOKEN }
}

fn unchanged() -> u32 {
    unsafe { ((ARENA + WOKEN) as *const u32).read_volatile() }
}

fn bump_what_is_waited_on() {
    unsafe {
        let word = (ARENA + WOKEN) as *mut u32;
        word.write_volatile(word.read_volatile().wrapping_add(1));
    }
}

fn wait_on(word: usize, until_it_changes: u32, micros: Option<u64>) {
    let until = micros.map(|us| [(us / 1_000_000) as i64, ((us % 1_000_000) * 1000) as i64]);
    let deadline = match &until {
        Some(when) => when.as_ptr() as usize,
        None => 0,
    };

    syscall(
        FUTEX,
        word,
        FUTEX_WAIT_PRIVATE,
        until_it_changes as usize,
        deadline,
        0,
        0,
    );
}

pub fn wake_up(word: usize) {
    syscall(FUTEX, word, FUTEX_WAKE_PRIVATE, WAKE_EVERY_WAITER, 0, 0, 0);
}

#[cfg(target_arch = "arm")]
fn syscall(number: usize, a: usize, b: usize, c: usize, d: usize, e: usize, f: usize) -> isize {
    let answer: isize;

    unsafe {
        core::arch::asm!(
            "push {{r7}}",
            "mov r7, {number}",
            "svc #0",
            "pop {{r7}}",
            number = in(reg) number,
            inlateout("r0") a => answer,
            in("r1") b,
            in("r2") c,
            in("r3") d,
            in("r4") e,
            in("r5") f,
            options(nostack)
        );
    }

    answer
}

#[cfg(target_arch = "x86")]
core::arch::global_asm!(
    ".globl frida_syscall",
    ".hidden frida_syscall",
    "frida_syscall:",
    "push ebp",
    "push ebx",
    "push esi",
    "push edi",
    "mov eax, [esp + 20]",
    "mov ebx, [esp + 24]",
    "mov ecx, [esp + 28]",
    "mov edx, [esp + 32]",
    "mov esi, [esp + 36]",
    "mov edi, [esp + 40]",
    "mov ebp, [esp + 44]",
    "int 0x80",
    "pop edi",
    "pop esi",
    "pop ebx",
    "pop ebp",
    "ret",
);

#[cfg(target_arch = "x86")]
unsafe extern "C" {
    fn frida_syscall(number: usize, a: usize, b: usize, c: usize, d: usize, e: usize, f: usize) -> isize;
}

#[cfg(target_arch = "x86")]
fn syscall(number: usize, a: usize, b: usize, c: usize, d: usize, e: usize, f: usize) -> isize {
    unsafe { frida_syscall(number, a, b, c, d, e, f) }
}

#[cfg(target_arch = "x86_64")]
fn syscall(number: usize, a: usize, b: usize, c: usize, d: usize, e: usize, f: usize) -> isize {
    let answer: isize;

    unsafe {
        core::arch::asm!(
            "syscall",
            inlateout("rax") number => answer,
            in("rdi") a,
            in("rsi") b,
            in("rdx") c,
            in("r10") d,
            in("r8") e,
            in("r9") f,
            lateout("rcx") _,
            lateout("r11") _,
            options(nostack)
        );
    }

    answer
}

#[cfg(target_arch = "aarch64")]
fn syscall(number: usize, a: usize, b: usize, c: usize, d: usize, e: usize, f: usize) -> isize {
    let answer: isize;

    unsafe {
        core::arch::asm!(
            "svc #0",
            in("x8") number,
            inlateout("x0") a => answer,
            in("x1") b,
            in("x2") c,
            in("x3") d,
            in("x4") e,
            in("x5") f,
            options(nostack)
        );
    }

    answer
}

static mut ARENA: usize = 0;
static mut CONTEXT: *mut crate::bindings::GMainContext = core::ptr::null_mut();

pub const PANICKED: u32 = 9;
pub const SPOKE: u32 = 10;
pub const FAULTED: u32 = 8;

#[cfg(target_arch = "arm")]
const RT_SIGACTION: usize = 174;
#[cfg(target_arch = "x86")]
const RT_SIGACTION: usize = 174;
#[cfg(target_arch = "x86_64")]
const RT_SIGACTION: usize = 13;
#[cfg(target_arch = "aarch64")]
const RT_SIGACTION: usize = 134;
const ILLEGAL: usize = 4;
const TRAPPED: usize = 5;
const ABORTED: usize = 6;
const ARITHMETIC: usize = 8;
const BAD_CALL: usize = 31;
const BUS: usize = 7;
const SEGMENT: usize = 11;
const SIGINFO: usize = 4;
const ABOUT_CODE: usize = 8;
const ABOUT_ADDRESS: usize = 16;
#[cfg(target_arch = "x86")]
const RUNNING_STATE: usize = 20;
#[cfg(target_arch = "x86_64")]
const RUNNING_STATE: usize = 40;
#[cfg(target_arch = "aarch64")]
const RUNNING_STATE: usize = 176;
#[cfg(target_arch = "arm")]
const RUNNING_STATE: usize = 20;
#[cfg(target_arch = "x86")]
const STATE_PC: usize = 56;
#[cfg(target_arch = "x86_64")]
const STATE_PC: usize = 128;
#[cfg(target_arch = "aarch64")]
const STATE_PC: usize = 264;
#[cfg(target_arch = "arm")]
const STATE_PC: usize = 72;
#[cfg(target_arch = "x86")]
const STATE_LR: usize = 28;
#[cfg(target_arch = "x86_64")]
const STATE_LR: usize = 120;
#[cfg(target_arch = "aarch64")]
const STATE_LR: usize = 248;
#[cfg(target_arch = "arm")]
const STATE_LR: usize = 68;

#[cfg(target_arch = "arm")]
const OPENAT: usize = 322;
#[cfg(target_arch = "x86")]
const OPENAT: usize = 295;
#[cfg(target_arch = "x86_64")]
const OPENAT: usize = 257;
#[cfg(target_arch = "aarch64")]
const OPENAT: usize = 56;
#[cfg(target_arch = "arm")]
const CLOSE: usize = 6;
#[cfg(target_arch = "x86")]
const CLOSE: usize = 6;
#[cfg(target_arch = "x86_64")]
const CLOSE: usize = 3;
#[cfg(target_arch = "aarch64")]
const CLOSE: usize = 57;
#[cfg(target_arch = "arm")]
const NANOSLEEP: usize = 162;
#[cfg(target_arch = "x86")]
const NANOSLEEP: usize = 162;
#[cfg(target_arch = "x86_64")]
const NANOSLEEP: usize = 35;
#[cfg(target_arch = "aarch64")]
const NANOSLEEP: usize = 101;
#[cfg(target_arch = "arm")]
const PIPE: usize = 42;
#[cfg(target_arch = "x86")]
const PIPE: usize = 42;
#[cfg(target_arch = "x86_64")]
const PIPE: usize = 22;
#[cfg(target_arch = "aarch64")]
const PIPE: usize = 59;

#[cfg(target_arch = "arm")]
const PPOLL: usize = 336;
#[cfg(target_arch = "x86")]
const PPOLL: usize = 309;
#[cfg(target_arch = "x86_64")]
const PPOLL: usize = 271;
#[cfg(target_arch = "aarch64")]
const PPOLL: usize = 73;
#[cfg(target_arch = "arm")]
const FCNTL: usize = 55;
#[cfg(target_arch = "x86")]
const FCNTL: usize = 55;
#[cfg(target_arch = "x86_64")]
const FCNTL: usize = 72;
#[cfg(target_arch = "aarch64")]
const FCNTL: usize = 25;
#[cfg(target_arch = "arm")]
const GETDENTS: usize = 217;
#[cfg(target_arch = "x86")]
const GETDENTS: usize = 220;
#[cfg(target_arch = "x86_64")]
const GETDENTS: usize = 217;
#[cfg(target_arch = "aarch64")]
const GETDENTS: usize = 61;
#[cfg(target_arch = "x86")]
const POLL: usize = 168;
#[cfg(target_arch = "x86_64")]
const POLL: usize = 7;
#[cfg(target_arch = "aarch64")]
const POLL: usize = 73;
#[cfg(target_arch = "arm")]
const READ: usize = 3;
#[cfg(target_arch = "x86")]
const READ: usize = 3;
#[cfg(target_arch = "x86_64")]
const READ: usize = 0;
#[cfg(target_arch = "aarch64")]
const READ: usize = 63;
#[cfg(target_arch = "arm")]
const WRITE: usize = 4;
#[cfg(target_arch = "x86")]
const WRITE: usize = 4;
#[cfg(target_arch = "x86_64")]
const WRITE: usize = 1;
#[cfg(target_arch = "aarch64")]
const WRITE: usize = 64;
#[cfg(target_arch = "arm")]
const EXIT: usize = 1;
#[cfg(target_arch = "x86")]
const EXIT: usize = 1;
#[cfg(target_arch = "x86_64")]
const EXIT: usize = 60;
#[cfg(target_arch = "aarch64")]
const EXIT: usize = 93;
#[cfg(target_arch = "arm")]
const EXIT_GROUP: usize = 248;
#[cfg(target_arch = "x86")]
const EXIT_GROUP: usize = 252;
#[cfg(target_arch = "x86_64")]
const EXIT_GROUP: usize = 231;
#[cfg(target_arch = "aarch64")]
const EXIT_GROUP: usize = 94;
#[cfg(target_arch = "arm")]
const FUTEX: usize = 240;
#[cfg(target_arch = "x86")]
const FUTEX: usize = 240;
#[cfg(target_arch = "x86_64")]
const FUTEX: usize = 202;
#[cfg(target_arch = "aarch64")]
const FUTEX: usize = 98;
#[cfg(target_arch = "arm")]
const CLOCK_GETTIME: usize = 263;
#[cfg(target_arch = "x86")]
const CLOCK_GETTIME: usize = 265;
#[cfg(target_arch = "x86_64")]
const CLOCK_GETTIME: usize = 228;
#[cfg(target_arch = "aarch64")]
const CLOCK_GETTIME: usize = 113;
#[cfg(target_arch = "arm")]
const SCHED_YIELD: usize = 158;
#[cfg(target_arch = "x86")]
const SCHED_YIELD: usize = 158;
#[cfg(target_arch = "x86_64")]
const SCHED_YIELD: usize = 24;
#[cfg(target_arch = "aarch64")]
const SCHED_YIELD: usize = 124;
#[cfg(target_arch = "arm")]
const GETPID: usize = 20;
#[cfg(target_arch = "x86")]
const GETPID: usize = 20;
#[cfg(target_arch = "x86_64")]
const GETPID: usize = 39;
#[cfg(target_arch = "aarch64")]
const GETPID: usize = 172;
#[cfg(target_arch = "arm")]
const GETTID: usize = 224;
#[cfg(target_arch = "x86")]
const GETTID: usize = 224;
#[cfg(target_arch = "x86_64")]
const GETTID: usize = 186;
#[cfg(target_arch = "aarch64")]
const GETTID: usize = 178;
#[cfg(target_arch = "arm")]
const MUNMAP: usize = 91;
#[cfg(target_arch = "x86")]
const MUNMAP: usize = 91;
#[cfg(target_arch = "x86_64")]
const MUNMAP: usize = 11;
#[cfg(target_arch = "aarch64")]
const MUNMAP: usize = 215;
#[cfg(target_arch = "arm")]
const CLONE: usize = 120;
#[cfg(target_arch = "x86")]
const CLONE: usize = 120;
#[cfg(target_arch = "x86_64")]
const CLONE: usize = 56;
#[cfg(target_arch = "aarch64")]
const CLONE: usize = 220;
#[cfg(target_arch = "arm")]
const MMAP: usize = 192;
#[cfg(target_arch = "x86")]
const MMAP: usize = 192;
#[cfg(target_arch = "x86_64")]
const MMAP: usize = 9;
#[cfg(target_arch = "aarch64")]
const MMAP: usize = 222;
#[cfg(target_arch = "arm")]
const MPROTECT: usize = 125;
#[cfg(target_arch = "x86")]
const MPROTECT: usize = 125;
#[cfg(target_arch = "x86_64")]
const MPROTECT: usize = 10;
#[cfg(target_arch = "aarch64")]
const MPROTECT: usize = 226;

const STANDARD_ERROR: usize = 2;
const WORKING_DIRECTORY: usize = -100isize as usize;
const READ_ONLY: usize = 0;
const POLL_IN: i32 = 1;
pub const READABLE: usize = 1;
pub const WRITABLE: usize = 2;
pub const EXECUTABLE: usize = 4;
const PRIVATE: usize = 2;
const ANONYMOUS: usize = 0x20;
const NO_FILE: usize = usize::MAX;

const MONOTONIC: usize = 1;
const REALTIME: usize = 0;

const THREAD_STACK_SIZE: usize = 1024 * 1024;
const THREAD_FLAGS: usize = 0x0000_0100 | 0x0000_0200 | 0x0000_0400 | 0x0000_0800 | 0x0001_0000;

const FUTEX_WAIT_PRIVATE: usize = 128;
const FUTEX_WAKE_PRIVATE: usize = 129;
const WAKE_EVERY_WAITER: usize = i32::MAX as usize;

const GUM_PAGE_READ: u32 = 1;
const GUM_PAGE_WRITE: u32 = 2;
const GUM_PAGE_EXECUTE: u32 = 4;


