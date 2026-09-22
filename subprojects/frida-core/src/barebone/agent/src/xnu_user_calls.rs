
use core::arch::asm;
use core::ffi::c_void;
use core::ptr::null_mut;
use core::sync::atomic::{AtomicBool, Ordering};

use crate::kernel::{CpuState, ThreadEntry, ThreadInfo};
use crate::xnu::Primitives;

pub static SLOTS: crate::gthread::ThreadSlots = crate::gthread::ThreadSlots {
    take: take_thread_slot,
    get: read_thread_slot,
    set: write_thread_slot,
};

fn take_thread_slot() -> u32 {
    unsafe { TAKEN += 1; TAKEN }
}

fn read_thread_slot(slot: u32) -> *mut c_void {
    if slot as usize >= SLOTS_PER_THREAD {
        return null_mut();
    }

    held(|| unsafe { row_for(thread_id()) }.map(|row| row[slot as usize]).unwrap_or(null_mut()))
}

fn write_thread_slot(slot: u32, value: *mut c_void) {
    if slot as usize >= SLOTS_PER_THREAD {
        return;
    }

    held(|| {
        let id = thread_id();
        let rows = unsafe { (&raw mut ROWS).as_mut().unwrap() };
        if unsafe { row_for(id) }.is_none() {
            let free = rows.iter().position(|row| row.0 == 0);
            match free {
                Some(at) => rows[at].0 = id,
                None => return,
            }
        }
        if let Some(row) = unsafe { row_for(id) } {
            row[slot as usize] = value;
        }
    });
}

unsafe fn row_for(id: u64) -> Option<&'static mut [*mut c_void; SLOTS_PER_THREAD]> {
    let rows = unsafe { (&raw mut ROWS).as_mut().unwrap() };

    rows.iter_mut().find(|row| row.0 == id).map(|row| &mut row.1)
}

fn held<T>(what: impl FnOnce() -> T) -> T {
    while LOCK.swap(true, Ordering::Acquire) {
        core::hint::spin_loop();
    }
    let answer = what();
    LOCK.store(false, Ordering::Release);

    answer
}

static LOCK: AtomicBool = AtomicBool::new(false);
static mut TAKEN: u32 = 0;
static mut ROWS: [(u64, [*mut c_void; SLOTS_PER_THREAD]); MOST_THREADS] =
    [(0, [null_mut(); SLOTS_PER_THREAD]); MOST_THREADS];

const SLOTS_PER_THREAD: usize = 256;
const MOST_THREADS: usize = 128;

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
    wall_clock_micros,
    current_process_id,
    current_thread_id,
    protect,
    page_size,
    cache_shape,
    protection_at,
    enumerate_ranges,
    enumerate_threads,
    find_thread,
    modify_thread,
};

fn alloc(size: usize) -> *mut u8 {
    crate::heap::take(size)
}

fn free(ptr: *mut u8, _size: usize) {
    crate::heap::give_back(ptr);
}

fn page_size() -> usize {
    unsafe { PAGE_SIZE }
}

fn cache_shape() -> u64 {
    unsafe { CACHE_SHAPE }
}

pub fn told_the_cache_shape(shape: u64) {
    unsafe { CACHE_SHAPE = shape };
}

static mut CACHE_SHAPE: u64 = 0;

pub fn told_the_page_size(size: usize) {
    unsafe { PAGE_SIZE = size };
}

static mut PAGE_SIZE: usize = 16384;

pub fn map_writable(size: usize) -> *mut u8 {
    take_memory(size).unwrap_or(0) as *mut u8
}



fn alloc_code(size: usize) -> *mut u8 {
    take_memory(size).unwrap_or(0) as *mut u8
}

fn free_code(ptr: *mut u8, size: usize) {
    give_memory_back(ptr as u64, size);
}

fn protect(address: u64, size: usize, may: u32) -> bool {
    crate::xnu_user::ask_for_protection(address, size, may)
}

fn wait(token: *const u8, timeout_us: Option<u64>, check: &mut dyn FnMut() -> bool) {
    if check() {
        return;
    }

    let token = where_the_other_half_can_reach(token);
    let value = unsafe { (token as *const u32).read_volatile() };
    unsafe {
        ask(ULOCK_WAIT, [COMPARE_AND_WAIT, token as u64, value as u64,
            timeout_us.unwrap_or(UNTIL_IT_IS_WOKEN)])
    };
}

const UNTIL_IT_IS_WOKEN: u64 = 0;

fn wake(token: *const u8) {
    let token = where_the_other_half_can_reach(token);

    unsafe { ask(ULOCK_WAKE, [COMPARE_AND_WAIT | WAKE_ALL, token as u64, 0, 0]) };
}

fn where_the_other_half_can_reach(token: *const u8) -> *const u8 {
    let arena = crate::xnu_user::arena();
    if arena == 0 || token != crate::glib::wakeup_token() {
        return token;
    }

    (arena + crate::xnu_relay::WAKE_WORD) as *const u8
}

pub fn this_thread_is() -> u64 {
    unsafe { ask(WHICH_THREAD_THIS_IS, [0, 0, 0, 0]) as u64 }
}

fn yield_now() {
    give_up_the_processor();
}

fn monotonic_micros() -> i64 {
    micros() as i64
}

fn wall_clock_micros() -> (u32, u32) {
    let mut when = WhatTimeItIs { seconds: 0, micros: 0 };
    unsafe { ask(GET_TIME_OF_DAY, [&mut when as *mut WhatTimeItIs as u64, 0, 0, 0]) };

    (when.seconds as u32, when.micros as u32)
}

#[repr(C)]
struct WhatTimeItIs {
    seconds: i64,
    micros: i64,
}

fn current_process_id() -> u32 {
    process_id()
}

fn current_thread_id() -> u64 {
    match crate::xnu_libsystem::asking_about_threads() {
        Some(asking) => unsafe { (asking.which_one_is_this)() as u64 },
        None => thread_id(),
    }
}

fn enumerate_ranges(found: &mut dyn FnMut(u64, usize, u32)) {
    let mut at = 0u64;
    while let Some(region) = region_from(at) {
        found(region.address, region.size as usize, region.protection);
        at = region.address + region.size;
    }
}

fn protection_at(address: u64) -> u32 {
    match region_from(address) {
        Some(region) if region.address <= address => region.protection,
        _ => 0,
    }
}

fn region_from(address: u64) -> Option<Region> {
    let mut region = Region::default();
    let told = unsafe {
        svc7(PROC_INFO, [ABOUT_A_PROCESS, process_id() as u64, ABOUT_A_REGION, address,
            &mut region as *mut Region as u64, core::mem::size_of::<Region>() as u64, 0])
    };

    (told == core::mem::size_of::<Region>() as i64).then_some(region)
}

#[repr(C)]
#[derive(Default)]
struct Region {
    protection: u32,
    most_it_may_be: u32,
    inheritance: u32,
    flags: u32,
    offset: u64,
    behavior: u32,
    wired: u32,
    tag: u32,
    resident: u32,
    now_private: u32,
    swapped_out: u32,
    dirtied: u32,
    references: u32,
    shadow_depth: u32,
    share_mode: u32,
    private_resident: u32,
    shared_resident: u32,
    object: u32,
    depth: u32,
    address: u64,
    size: u64,
}

fn enumerate_threads(found: &mut dyn FnMut(ThreadInfo)) {
    let Some(asking) = crate::xnu_libsystem::asking_about_threads() else {
        return;
    };

    let mut threads: *mut u32 = core::ptr::null_mut();
    let mut counted: u32 = 0;
    let task = unsafe { (asking.this_task as *const u32).read_volatile() };
    let told = unsafe { (asking.which_ones)(task, &mut threads, &mut counted) };

    if told != WELL_ENOUGH {
        return;
    }

    for step in 0..counted as usize {
        let thread = unsafe { threads.add(step).read() };
        found(ThreadInfo { id: thread, cpu_state: state_of(thread) });
    }

    unsafe { (asking.give_back)(task, threads as u64, (counted as u64) * 4) };
}

fn find_thread(id: u32) -> Option<ThreadInfo> {
    let mut found = None;
    enumerate_threads(&mut |thread| {
        if thread.id == id {
            found = Some(thread);
        }
    });

    found
}

fn modify_thread(id: u32, change: &mut dyn FnMut(&mut CpuState)) -> bool {
    let Some(asking) = crate::xnu_libsystem::asking_about_threads() else {
        return false;
    };
    if unsafe { (asking.hold_it_still)(id) } != WELL_ENOUGH {
        return false;
    }

    let mut state = [0u32; STATE_WORDS];
    let changed = read_state(id, &mut state) && {
        let mut cpu = as_cpu_state(&state);
        change(&mut cpu);
        write_cpu_state(&cpu, &mut state);
        unsafe { (asking.set_what_it_does)(id, ARM_THREAD_STATE64, state.as_ptr(),
            STATE_WORDS as u32) == WELL_ENOUGH }
    };

    unsafe { (asking.let_it_go)(id) };

    changed
}

fn state_of(thread: u32) -> Option<CpuState> {
    let mut state = [0u32; STATE_WORDS];
    read_state(thread, &mut state).then(|| as_cpu_state(&state))
}

fn read_state(thread: u32, state: &mut [u32; STATE_WORDS]) -> bool {
    let Some(asking) = crate::xnu_libsystem::asking_about_threads() else {
        return false;
    };

    let mut counted = STATE_WORDS as u32;
    unsafe {
        (asking.what_it_is_doing)(thread, ARM_THREAD_STATE64, state.as_mut_ptr(), &mut counted)
            == WELL_ENOUGH
    }
}

fn as_cpu_state(state: &[u32; STATE_WORDS]) -> CpuState {
    let mut cpu = CpuState { pc: word(state, PC), sp: word(state, SP), nzcv: state[FLAGS] as u64,
        x: [0; 29], fp: word(state, FP), lr: word(state, LR) };
    for (index, register) in cpu.x.iter_mut().enumerate() {
        *register = word(state, index);
    }

    cpu
}

fn write_cpu_state(cpu: &CpuState, state: &mut [u32; STATE_WORDS]) {
    for (index, register) in cpu.x.iter().enumerate() {
        put(state, index, *register);
    }
    put(state, FP, cpu.fp);
    put(state, LR, cpu.lr);
    put(state, SP, cpu.sp);
    put(state, PC, cpu.pc);
    state[FLAGS] = cpu.nzcv as u32;
}

fn word(state: &[u32; STATE_WORDS], index: usize) -> u64 {
    (state[index * 2] as u64) | ((state[index * 2 + 1] as u64) << 32)
}

fn put(state: &mut [u32; STATE_WORDS], index: usize, value: u64) {
    state[index * 2] = value as u32;
    state[index * 2 + 1] = (value >> 32) as u32;
}

const WELL_ENOUGH: i32 = 0;
const ARM_THREAD_STATE64: i32 = 6;
const STATE_WORDS: usize = 68;
const FP: usize = 29;
const LR: usize = 30;
const SP: usize = 31;
const PC: usize = 32;
const FLAGS: usize = 66;

fn spawn_thread(entry: ThreadEntry, parameter: *mut c_void) -> isize {
    let Some(making) = crate::xnu_libsystem::making_threads() else {
        return 0;
    };

    let errand = alloc::boxed::Box::into_raw(alloc::boxed::Box::new(Errand { entry, parameter }));

    let mut made = 0u64;
    let told = unsafe {
        (making.the_usual_way)(&mut made, core::ptr::null(),
            crate::xnu_libsystem::signed_to_begin_at(say_which_one_first),
            errand as *mut c_void)
    };
    if told != 0 {
        drop(unsafe { alloc::boxed::Box::from_raw(errand) });
    }

    (told == 0) as isize
}

struct Errand {
    entry: ThreadEntry,
    parameter: *mut c_void,
}

unsafe extern "C" fn say_which_one_first(errand: *mut c_void) -> *mut c_void {
    let errand = unsafe { alloc::boxed::Box::from_raw(errand as *mut Errand) };

    crate::xnu_user::say_which_thread_this_is(crate::xnu_user::arena());
    crate::xnu_unlisted::leave_the_list();
    unsafe { (errand.entry)(errand.parameter, 0) };
    crate::xnu_unlisted::join_the_list_again();

    core::ptr::null_mut()
}

const STACK: usize = 1024 * 1024;
const STACK_HEADROOM: u64 = 0x100;
const PLAIN_ADDRESSES: u32 = 1;



pub fn process_id() -> u32 {
    unsafe { ask(GET_PID, [0; 4]) as u32 }
}

pub fn thread_id() -> u64 {
    unsafe { ask(THREAD_SELF_ID, [0; 4]) as u64 }
}

pub fn give_up_the_processor() {
    unsafe { ask(SCHED_YIELD, [0; 4]) };
}

pub fn micros() -> u64 {
    let ticks: u64;
    let rate: u64;
    unsafe {
        asm!("isb", "mrs {0}, cntvct_el0", "mrs {1}, cntfrq_el0", out(reg) ticks, out(reg) rate);
    }

    ticks / (rate / 1_000_000)
}

pub fn where_the_shared_code_is() -> Option<u64> {
    let mut base = 0u64;
    let told = unsafe { ask(WHERE_IS_THE_SHARED_CODE, [&mut base as *mut u64 as u64, 0, 0, 0]) };

    (told == 0 && base != 0).then_some(base)
}

const WHERE_IS_THE_SHARED_CODE: i64 = 294;

pub fn code_memory_near(wanted: u64, size: usize) -> *mut u8 {
    let mut address = wanted;
    let told = unsafe {
        trap(MACH_VM_ALLOCATE, [own_task_map(), &mut address as *mut u64 as u64, size as u64,
            ANYWHERE])
    };
    if told == KERN_SUCCESS {
        write_down_what_we_took(address, size);
        return address as *mut u8;
    }

    take_memory(size).unwrap_or(0) as *mut u8
}

fn own_task_map() -> u64 {
    own_task() as u64
}

pub fn take_memory(size: usize) -> Option<u64> {
    let mut address: u64 = 0;
    let told = unsafe {
        trap(MACH_VM_ALLOCATE, [task(), &mut address as *mut u64 as u64, size as u64, ANYWHERE])
    };

    if told != KERN_SUCCESS {
        return None;
    }
    write_down_what_we_took(address, size);

    Some(address)
}

fn write_down_what_we_took(address: u64, size: usize) {
    let Some(taken) = what_we_took() else {
        hold_it_until_there_is_somewhere_to_say_it(address, size);
        return;
    };

    for (at, size) in unsafe { held_until_there_was() } {
        if *at != 0 {
            say_it(taken, *at, *size);
            *at = 0;
        }
    }

    say_it(taken, address, size as u64);
}

fn hold_it_until_there_is_somewhere_to_say_it(address: u64, size: usize) {
    for (at, room) in unsafe { held_until_there_was() } {
        if *at == 0 {
            *room = size as u64;
            *at = address;
            return;
        }
    }
}

unsafe fn held_until_there_was() -> &'static mut [(u64, u64); BEFORE_WE_KNEW] {
    unsafe { (&raw mut HELD).as_mut().unwrap() }
}

static mut HELD: [(u64, u64); BEFORE_WE_KNEW] = [(0, 0); BEFORE_WE_KNEW];

const BEFORE_WE_KNEW: usize = 32;

fn say_it(taken: *mut u64, address: u64, size: u64) {

    for step in 0..crate::xnu_relay::MOST_WE_TAKE {
        let at = unsafe { taken.add(1 + step * 2) };
        if unsafe { at.read_volatile() } != 0 {
            continue;
        }
        unsafe {
            at.add(1).write_volatile(size);
            at.write_volatile(address);
            if taken.read_volatile() < (step + 1) as u64 {
                taken.write_volatile((step + 1) as u64);
            }
        }
        return;
    }
}

fn cross_out_what_we_gave_back(address: u64) {
    for (at, _) in unsafe { held_until_there_was() } {
        if *at == address {
            *at = 0;
            return;
        }
    }

    let Some(taken) = what_we_took() else {
        return;
    };

    let how_many = unsafe { taken.read_volatile() } as usize;
    for step in 0..how_many.min(crate::xnu_relay::MOST_WE_TAKE) {
        let at = unsafe { taken.add(1 + step * 2) };
        if unsafe { at.read_volatile() } == address {
            unsafe { at.write_volatile(0) };
            return;
        }
    }
}

fn what_we_took() -> Option<*mut u64> {
    let arena = crate::xnu_user::arena();

    (arena != 0).then(|| (arena + crate::xnu_relay::WHAT_WE_TOOK) as *mut u64)
}

pub fn give_memory_back(address: u64, size: usize) {
    cross_out_what_we_gave_back(address);
    unsafe { trap(MACH_VM_DEALLOCATE, [task(), address, size as u64, 0]) };
}

fn set_protection(address: u64, size: usize, may: u64) -> bool {
    unsafe { trap(MACH_VM_PROTECT, [task(), address, size as u64, may]) == KERN_SUCCESS }
}

unsafe fn task() -> u64 {
    unsafe { trap(TASK_SELF, [0; 4]) as u64 }
}

unsafe fn trap(number: i64, args: [u64; 4]) -> i64 {
    unsafe { svc(number, args).0 }
}

pub(crate) unsafe fn ask(number: i64, args: [u64; 4]) -> i64 {
    let (answer, went_wrong) = unsafe { svc(number, args) };
    if went_wrong {
        return -answer;
    }

    answer
}

pub(crate) unsafe fn svc7(number: i64, args: [u64; 7]) -> i64 {
    unsafe { ask7(number, args).0 }
}

pub(crate) unsafe fn ask7(number: i64, args: [u64; 7]) -> (i64, bool) {
    let answer: i64;
    let went_wrong: u64;
    unsafe {
        asm!(
            "svc #0x80",
            "cset x7, cs",
            inlateout("x16") number => _,
            inlateout("x0") args[0] => answer,
            inlateout("x1") args[1] => _,
            inlateout("x2") args[2] => _,
            inlateout("x3") args[3] => _,
            inlateout("x4") args[4] => _,
            inlateout("x5") args[5] => _,
            inlateout("x6") args[6] => _,
            lateout("x7") went_wrong,
            clobber_abi("C"),
        );
    }

    (answer, went_wrong != 0)
}

pub(crate) fn own_task() -> u32 {
    unsafe { trap(TASK_SELF, [0; 4]) as u32 }
}

pub fn own_thread() -> u32 {
    unsafe { trap(THREAD_SELF, [0; 4]) as u32 }
}

pub(crate) fn a_port_to_answer_on() -> u32 {
    unsafe { trap(REPLY_PORT, [0; 4]) as u32 }
}

unsafe fn svc(number: i64, args: [u64; 4]) -> (i64, bool) {
    let answer: i64;
    let went_wrong: u64;
    unsafe {
        asm!(
            "svc #0x80",
            "cset x4, cs",
            inlateout("x16") number => _,
            inlateout("x0") args[0] => answer,
            inlateout("x1") args[1] => _,
            inlateout("x2") args[2] => _,
            inlateout("x3") args[3] => _,
            lateout("x4") went_wrong,
            clobber_abi("C"),
        );
    }

    (answer, went_wrong != 0)
}
const KERN_SUCCESS: i64 = 0;
const ANYWHERE: u64 = 1;

const MACH_VM_ALLOCATE: i64 = -10;
const MACH_VM_DEALLOCATE: i64 = -12;
const MACH_VM_PROTECT: i64 = -14;
const TASK_SELF: i64 = -28;
const REPLY_PORT: i64 = -26;
const THREAD_SELF: i64 = -27;
pub(crate) const MACH_MSG: i64 = -31;

const COMPARE_AND_WAIT: u64 = 1;
const WAKE_ALL: u64 = 0x100;

const GET_PID: i64 = 20;
const GET_TIME_OF_DAY: i64 = 116;
const PROC_INFO: i64 = 336;

const ABOUT_A_PROCESS: u64 = 2;
const ABOUT_A_REGION: u64 = 7;
const WHICH_THREAD_THIS_IS: i64 = 372;
const ULOCK_WAIT: i64 = 515;
const ULOCK_WAKE: i64 = 516;
const SCHED_YIELD: i64 = 331;
const THREAD_SELF_ID: i64 = 372;
