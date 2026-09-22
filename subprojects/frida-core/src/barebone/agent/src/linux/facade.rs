use core::ffi::c_void;

use crate::kernel::ThreadEntry;

pub fn log(msg: &str) {
    (primitives().log)(msg)
}

pub fn panic(msg: &str) -> ! {
    (primitives().panic)(msg)
}

pub fn alloc(size: usize) -> *mut u8 {
    (primitives().alloc)(size)
}

pub fn free(ptr: *mut u8, size: usize) {
    (primitives().free)(ptr, size)
}

pub fn alloc_heap(size: usize) -> *mut u8 {
    (primitives().alloc_heap)(size)
}

pub fn alloc_code(size: usize) -> *mut u8 {
    (primitives().alloc_code)(size)
}

pub fn free_code(ptr: *mut u8, size: usize) {
    (primitives().free_code)(ptr, size)
}

pub fn page_size() -> usize {
    (primitives().page_size)()
}

pub fn protect(address: u64, size: usize, protection: u32) -> bool {
    (primitives().protect)(address, size, protection)
}

pub fn spawn_thread(entry: ThreadEntry, parameter: *mut c_void) -> isize {
    (primitives().spawn_thread)(entry, parameter)
}

pub fn wait(token: *const u8, timeout_us: Option<u64>, check: &mut dyn FnMut() -> bool) {
    (primitives().wait)(token, timeout_us, check)
}

pub fn wake(token: *const u8) {
    (primitives().wake)(token)
}

pub fn yield_now() {
    (primitives().yield_now)()
}

pub fn monotonic_micros() -> i64 {
    (primitives().monotonic_micros)()
}

pub fn wall_clock_micros() -> (u32, u32) {
    (primitives().wall_clock_micros)()
}

pub fn home_process_id() -> u32 {
    (primitives().home_process_id)()
}

pub fn current_process_id() -> u32 {
    (primitives().current_process_id)()
}

pub fn current_thread_id() -> u64 {
    (primitives().current_thread_id)()
}

pub fn install_fault_reporter() {
    (primitives().install_fault_reporter)()
}

pub fn poke_loop_wakeup() {
    (primitives().poke_loop_wakeup)()
}

pub fn select_user() {
    unsafe { ACTIVE = &super::user::USER };
}

pub fn in_copy() -> bool {
    core::ptr::eq(primitives(), &super::user::USER)
}

fn primitives() -> &'static Primitives {
    unsafe { ACTIVE }
}

static mut ACTIVE: &Primitives = &KERNEL;

static KERNEL: Primitives = Primitives {
    log: super::native::log,
    panic: super::native::panic,
    alloc: super::native::alloc,
    free: super::native::free,
    alloc_heap: super::native::alloc,
    alloc_code: super::native::alloc_code,
    free_code: super::native::free_code,
    page_size: super::native::page_size,
    protect: super::native::protect,
    spawn_thread: super::native::spawn_thread,
    wait: super::native::wait,
    wake: super::native::wake,
    yield_now: super::native::yield_now,
    monotonic_micros: super::native::monotonic_micros,
    wall_clock_micros: super::native::wall_clock_micros,
    home_process_id: super::native::current_process_id,
    current_process_id: super::native::current_process_id,
    current_thread_id: super::native::current_thread_id,
    install_fault_reporter: super::native::install_fault_reporter,
    poke_loop_wakeup: super::native::poke_loop_wakeup,
};

pub struct Primitives {
    pub log: fn(&str),
    pub panic: fn(&str) -> !,
    pub alloc: fn(usize) -> *mut u8,
    pub free: fn(*mut u8, usize),
    pub alloc_heap: fn(usize) -> *mut u8,
    pub alloc_code: fn(usize) -> *mut u8,
    pub free_code: fn(*mut u8, usize),
    pub page_size: fn() -> usize,
    pub protect: fn(u64, usize, u32) -> bool,
    pub spawn_thread: fn(ThreadEntry, *mut c_void) -> isize,
    pub wait: fn(*const u8, Option<u64>, &mut dyn FnMut() -> bool),
    pub wake: fn(*const u8),
    pub yield_now: fn(),
    pub monotonic_micros: fn() -> i64,
    pub wall_clock_micros: fn() -> (u32, u32),
    pub home_process_id: fn() -> u32,
    pub current_process_id: fn() -> u32,
    pub current_thread_id: fn() -> u64,
    pub install_fault_reporter: fn(),
    pub poke_loop_wakeup: fn(),
}
