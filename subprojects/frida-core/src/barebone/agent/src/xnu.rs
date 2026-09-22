use core::ffi::c_void;
use core::sync::atomic::{AtomicU64, Ordering};

use crate::kernel::ThreadEntry;

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
    pub wall_clock_micros: fn() -> (u32, u32),
    pub current_process_id: fn() -> u32,
    pub current_thread_id: fn() -> u64,
    pub protect: fn(u64, usize, u32) -> bool,
    pub protection_at: fn(u64) -> u32,
    pub page_size: fn() -> usize,
    pub cache_shape: fn() -> u64,
    pub enumerate_ranges: fn(&mut dyn FnMut(u64, usize, u32)),
    pub enumerate_threads: fn(&mut dyn FnMut(crate::kernel::ThreadInfo)),
    pub find_thread: fn(u32) -> Option<crate::kernel::ThreadInfo>,
    pub modify_thread: fn(u32, &mut dyn FnMut(&mut crate::kernel::CpuState)) -> bool,
}

pub fn select_user() {
    unsafe { ACTIVE = &crate::xnu_user_calls::USER };
}

pub fn in_copy() -> bool {
    core::ptr::eq(primitives(), &crate::xnu_user_calls::USER)
}

fn primitives() -> &'static Primitives {
    unsafe { ACTIVE }
}

static mut ACTIVE: &Primitives = &KERNEL;

static KERNEL: Primitives = Primitives {
    alloc: kernel_alloc,
    free: kernel_free,
    alloc_code: kernel_alloc_code,
    free_code: kernel_free_code,
    spawn_thread: kernel_spawn_thread,
    wait: kernel_wait,
    wake: kernel_wake,
    yield_now: kernel_yield_now,
    monotonic_micros: kernel_monotonic_micros,
    wall_clock_micros: kernel_wall_clock_micros,
    current_process_id: kernel_current_process_id,
    current_thread_id: kernel_current_thread_id,
    protect: kernel_protect,
    protection_at: crate::xnu_ranges::protection_at,
    page_size: kernel_page_size,
    cache_shape: kernel_cache_shape,
    enumerate_ranges: crate::xnu_ranges::enumerate_ranges,
    enumerate_threads: kernel_enumerate_threads,
    find_thread: kernel_find_thread,
    modify_thread: kernel_modify_thread,
};

pub fn alloc(size: usize) -> *mut u8 {
    (primitives().alloc)(size)
}

pub fn free(ptr: *mut u8, size: usize) {
    (primitives().free)(ptr, size);
}

pub fn alloc_code(size: usize) -> *mut u8 {
    (primitives().alloc_code)(size)
}

pub fn free_code(ptr: *mut u8, size: usize) {
    (primitives().free_code)(ptr, size);
}

pub fn spawn_thread(entry: ThreadEntry, parameter: *mut c_void) -> isize {
    (primitives().spawn_thread)(entry, parameter)
}

pub fn wait(token: *const u8, timeout_us: Option<u64>, check: &mut dyn FnMut() -> bool) {
    (primitives().wait)(token, timeout_us, check);
}

pub fn poke_loop_wakeup() {}

pub fn wake(token: *const u8) {
    (primitives().wake)(token);
}

pub fn yield_now() {
    (primitives().yield_now)();
}

pub fn monotonic_micros() -> i64 {
    (primitives().monotonic_micros)()
}

pub fn current_process_id() -> u32 {
    (primitives().current_process_id)()
}

pub fn current_thread_id() -> u64 {
    (primitives().current_thread_id)()
}

pub fn protect(address: u64, size: usize, may: u32) -> bool {
    (primitives().protect)(address, size, may)
}

pub fn protection_at(address: u64) -> u32 {
    (primitives().protection_at)(address)
}

pub fn enumerate_ranges(found: &mut dyn FnMut(u64, usize, u32)) {
    (primitives().enumerate_ranges)(found);
}

pub fn enumerate_threads(found: &mut dyn FnMut(crate::kernel::ThreadInfo)) {
    (primitives().enumerate_threads)(found);
}

pub fn find_thread(id: u32) -> Option<crate::kernel::ThreadInfo> {
    (primitives().find_thread)(id)
}

pub fn modify_thread(id: u32, change: &mut dyn FnMut(&mut crate::kernel::CpuState)) -> bool {
    (primitives().modify_thread)(id, change)
}

fn kernel_current_process_id() -> u32 {
    0
}

fn kernel_protect(_address: u64, _size: usize, _may: u32) -> bool {
    false
}

fn kernel_enumerate_threads(_found: &mut dyn FnMut(crate::kernel::ThreadInfo)) {}

fn kernel_find_thread(_id: u32) -> Option<crate::kernel::ThreadInfo> {
    None
}

fn kernel_modify_thread(_id: u32, _change: &mut dyn FnMut(&mut crate::kernel::CpuState)) -> bool {
    false
}

pub fn log(msg: &str) {
    let mut line = [0u8; 256];
    let length = msg.len().min(line.len() - 1);
    line[..length].copy_from_slice(&msg.as_bytes()[..length]);

    unsafe { _IOLog(line.as_ptr()) };
}

pub fn panic(msg: &str) {
    unsafe { _panic(msg.as_ptr()) };
}

pub fn run_when_ready(action: fn()) {
    action();
}

// The exception vectors of this kernel are not hooked yet. Thus a fault in the agent goes to
// the kernel as it is.
pub fn install_fault_reporter() {}

fn kernel_spawn_thread(entry: ThreadEntry, parameter: *mut c_void) -> isize {
    kernel_thread_start(entry, parameter)
}

fn kernel_alloc(size: usize) -> *mut u8 {
    kalloc(size)
}

#[cfg(feature = "xnu-kext")]
pub fn kernel_alloc_code_above(lowest: u64, size: usize) -> *mut u8 {
    let page = page_size();
    let wanted = ((size + page - 1) & !(page - 1)) as u64;

    let map = the_kernel_map();
    let at = take_pages_from_the_kernel_map_above(map, wanted, lowest & !(page as u64 - 1));
    if at == 0 {
        return core::ptr::null_mut();
    }

    dress_pages_for_code(map, at, wanted)
}

#[cfg(feature = "xnu-kext")]
fn kernel_alloc_code(size: usize) -> *mut u8 {
    let page = page_size();
    let wanted = ((size + page - 1) & !(page - 1)) as u64;

    let map = the_kernel_map();
    let at = take_pages_from_the_kernel_map(map, wanted);
    if at == 0 {
        return core::ptr::null_mut();
    }

    dress_pages_for_code(map, at, wanted)
}

#[cfg(feature = "xnu-kext")]
fn dress_pages_for_code(map: *mut c_void, at: u64, wanted: u64) -> *mut u8 {
    if !protect_in_kernel_map(at, wanted as usize, VM_PROT_READ | VM_PROT_EXECUTE)
        || !keep_pages_in_memory(map, at, wanted, VM_PROT_READ | VM_PROT_EXECUTE)
        || !give_each_page_a_second_name(at, wanted as usize)
    {
        unsafe { _mach_vm_deallocate.unwrap()(map, at, wanted) };
        return core::ptr::null_mut();
    }

    at as *mut u8
}

#[cfg(feature = "xnu-kext")]
pub fn kernel_alloc_pages(size: usize) -> *mut u8 {
    let page = page_size();
    let wanted = ((size + page - 1) & !(page - 1)) as u64;

    let map = the_kernel_map();
    let at = take_pages_from_the_kernel_map(map, wanted);
    if at == 0 {
        return core::ptr::null_mut();
    }

    if !keep_pages_in_memory(map, at, wanted, VM_PROT_READ | VM_PROT_WRITE) {
        unsafe { _mach_vm_deallocate.unwrap()(map, at, wanted) };
        return core::ptr::null_mut();
    }

    at as *mut u8
}

#[cfg(feature = "xnu-kext")]
fn keep_pages_in_memory(map: *mut c_void, at: u64, size: u64, reached_by: core::ffi::c_int)
    -> bool
{
    unsafe { _vm_map_wire_external.unwrap()(map, at, at + size, reached_by, 0) == KERN_SUCCESS }
}

#[cfg(feature = "xnu-kext")]
fn let_every_page_be_present(address: u64, size: u64) {
    let page = page_size() as u64;
    let mut at = address;
    while at < address + size {
        unsafe { (at as *const u8).read_volatile() };
        at += page;
    }
}

#[cfg(feature = "xnu-kext")]
fn take_pages_from_the_kernel_map(map: *mut c_void, wanted: u64) -> u64 {
    take_pages_from_the_kernel_map_above(map, wanted, 0)
}

#[cfg(feature = "xnu-kext")]
fn take_pages_from_the_kernel_map_above(map: *mut c_void, wanted: u64, lowest: u64) -> u64 {
    if map.is_null() {
        return 0;
    }

    let mut at = lowest;
    if unsafe { _mach_vm_allocate.unwrap()(map, &mut at, wanted, VM_FLAGS_ANYWHERE) }
        != KERN_SUCCESS
    {
        return 0;
    }

    at
}

#[cfg(feature = "xnu-kext")]
fn kernel_free_code(ptr: *mut u8, size: usize) {
    let page = page_size();
    let wanted = ((size + page - 1) & !(page - 1)) as u64;

    let map = the_kernel_map();
    if map.is_null() {
        return;
    }

    if let Some((alias, held)) = forget_the_second_name(ptr as u64, size) {
        unsafe { _mach_vm_deallocate.unwrap()(map, alias, held as u64) };
    }

    unsafe { _mach_vm_deallocate.unwrap()(map, ptr as u64, wanted) };
}

#[cfg(feature = "xnu-kext")]
fn give_each_page_a_second_name(address: u64, size: usize) -> bool {
    let page = page_size();

    let_every_page_be_present(address, size as u64);

    let alias = a_writable_name_for(address, size as u64);
    if alias == 0 {
        return false;
    }

    hold_the_books();
    let mut at = address;
    while at < address + size as u64 {
        unsafe { views() }.insert(at, alias + (at - address));
        at += page as u64;
    }
    let_the_books_go();

    true
}

#[cfg(feature = "xnu-kext")]
fn a_writable_name_for(address: u64, size: u64) -> u64 {
    let map = the_kernel_map();
    if map.is_null() {
        return 0;
    }

    let mut at = 0u64;
    let mut may_now: core::ffi::c_int = 0;
    let mut may_ever: core::ffi::c_int = 0;
    if unsafe {
        _mach_vm_remap.unwrap()(map, &mut at, size, 0, VM_FLAGS_ANYWHERE, map, address, 0,
            &mut may_now, &mut may_ever, VM_INHERIT_NONE)
    } != KERN_SUCCESS
    {
        return 0;
    }

    if !protect_in_kernel_map(at, size as usize, VM_PROT_READ | VM_PROT_WRITE) {
        unsafe { _mach_vm_deallocate.unwrap()(map, at, size) };
        return 0;
    }

    let_every_page_be_present(at, size);

    at
}

#[cfg(feature = "xnu-kext")]
pub fn where_a_page_lives(address: u64) -> u64 {
    let worth = unsafe { NUMBERED_BY };
    let worth = if worth != 0 { worth } else { what_a_page_number_is_worth() };
    if worth == 0 {
        return 0;
    }

    let pmap = the_kernel_pmap();
    if pmap.is_null() {
        return 0;
    }

    let numbered = unsafe { _pmap_find_phys.unwrap()(pmap, address) } as u64;
    if numbered == 0 {
        return 0;
    }

    numbered << worth
}

#[cfg(feature = "xnu-kext")]
fn what_a_page_number_is_worth() -> u32 {
    let pmap = the_kernel_pmap();
    if pmap.is_null() {
        return 0;
    }

    let known = (crate::own_range().0 & !(page_size() - 1)) as u64;
    let address = virt_to_phys(known);
    let numbered = unsafe { _pmap_find_phys.unwrap()(pmap, known) } as u64;
    if address == 0 || numbered == 0 || address % numbered != 0 {
        return 0;
    }

    let worth = (address / numbered).trailing_zeros();
    unsafe { NUMBERED_BY = worth };

    worth
}

#[cfg(feature = "xnu-kext")]
static mut NUMBERED_BY: u32 = 0;

#[cfg(feature = "xnu-kext")]
pub fn let_the_kernel_map_write(address: u64, size: usize) -> bool {
    protect_in_kernel_map(address, size, VM_PROT_READ | VM_PROT_WRITE)
}

#[cfg(feature = "xnu-kext")]
fn protect_in_kernel_map(address: u64, size: usize, may: core::ffi::c_int) -> bool {
    let map = the_kernel_map();
    if map.is_null() {
        return false;
    }

    let page = page_size();
    let base = address & !(page as u64 - 1);
    let span = (((address - base) as usize + size + page - 1) & !(page - 1)) as u64;

    unsafe { _mach_vm_protect.unwrap()(map, base, span, 0, may) == KERN_SUCCESS }
}

#[cfg(feature = "xnu-kext")]
fn the_kernel_map() -> *mut c_void {
    let held = unsafe { _kernel_map };
    if held.is_null() {
        return core::ptr::null_mut();
    }

    unsafe { (held as *const *mut c_void).read() }
}

#[cfg(feature = "xnu-kext")]
fn the_kernel_pmap() -> *mut c_void {
    let held = unsafe { _kernel_pmap };
    if held.is_null() {
        return core::ptr::null_mut();
    }

    unsafe { (held as *const *mut c_void).read() }
}

#[cfg(feature = "xnu-kext")]
pub fn remember_what_we_took(base: u64, size: usize, may_run: bool) {
    hold_the_books();
    unsafe { ours() }.insert(base, (size, may_run));
    let_the_books_go();
}

#[cfg(feature = "xnu-kext")]
fn forget_the_second_name(base: u64, size: usize) -> Option<(u64, usize)> {
    let page = page_size();

    hold_the_books();
    let alias = unsafe { views() }.remove(&base);
    let mut at = base + page as u64;
    while at < base + size as u64 {
        unsafe { views() }.remove(&at);
        at += page as u64;
    }
    let_the_books_go();

    alias.map(|found| (found, size))
}

#[cfg(feature = "xnu-kext")]
pub fn forget_what_we_took(base: u64) {
    hold_the_books();
    unsafe { ours() }.remove(&base);
    let_the_books_go();
}

#[cfg(feature = "xnu-kext")]
pub fn we_took(address: u64) -> Option<bool> {
    hold_the_books();
    let found = unsafe { ours() }
        .iter()
        .find(|(base, (size, _))| address >= **base && address < **base + *size as u64)
        .map(|(_, (_, may_run))| *may_run);
    let_the_books_go();

    found
}

#[cfg(feature = "xnu-kext")]
unsafe fn ours() -> &'static mut alloc::collections::BTreeMap<u64, (usize, bool)> {
    unsafe { (&raw mut OURS).as_mut().unwrap() }
}

#[cfg(feature = "xnu-kext")]
static mut OURS: alloc::collections::BTreeMap<u64, (usize, bool)> =
    alloc::collections::BTreeMap::new();

#[cfg(feature = "xnu-kext")]
pub fn the_page_behind(alias: u64) -> Option<u64> {
    let page = page_size();
    let base = alias & !(page as u64 - 1);

    hold_the_books();
    let found = unsafe { views() }
        .iter()
        .find(|(_, named)| **named == base)
        .map(|(page, _)| *page);
    let_the_books_go();

    found
}

#[cfg(feature = "xnu-kext")]
pub fn the_writable_view_of(address: u64) -> u64 {
    let page = page_size();
    let base = address & !(page as u64 - 1);

    hold_the_books();
    let found = unsafe { views() }
        .get(&base)
        .map(|alias| alias + (address - base))
        .unwrap_or(0);
    let_the_books_go();

    found
}

#[cfg(feature = "xnu-kext")]
pub fn is_a_writable_view(address: u64) -> bool {
    let page = page_size();
    hold_the_books();
    let found = unsafe { views() }
        .values()
        .any(|alias| address >= *alias && address < *alias + page as u64);
    let_the_books_go();

    found
}

#[cfg(feature = "xnu-kext")]
unsafe fn views() -> &'static mut alloc::collections::BTreeMap<u64, u64> {
    unsafe { (&raw mut VIEWS).as_mut().unwrap() }
}

#[cfg(feature = "xnu-kext")]
static mut VIEWS: alloc::collections::BTreeMap<u64, u64> = alloc::collections::BTreeMap::new();

#[cfg(feature = "xnu-kext")]
unsafe fn device_views() -> &'static mut alloc::collections::BTreeMap<u64, u64> {
    unsafe { (&raw mut DEVICE_VIEWS).as_mut().unwrap() }
}

#[cfg(feature = "xnu-kext")]
static mut DEVICE_VIEWS: alloc::collections::BTreeMap<u64, u64> =
    alloc::collections::BTreeMap::new();

#[cfg(feature = "xnu-kext")]
const VM_FLAGS_ANYWHERE: core::ffi::c_int = 1;
#[cfg(feature = "xnu-kext")]
const VM_PROT_WRITE: core::ffi::c_int = 2;

#[cfg(feature = "xnu-kext")]
unsafe extern "C" {
    static _kernel_map: *mut c_void;
    static _kernel_pmap: *mut c_void;
    static _pmap_find_phys: Option<unsafe extern "C" fn(*mut c_void, u64) -> u32>;
    static _mach_vm_remap: Option<
        unsafe extern "C" fn(*mut c_void, *mut u64, u64, u64, core::ffi::c_int, *mut c_void, u64,
            core::ffi::c_int, *mut core::ffi::c_int, *mut core::ffi::c_int, core::ffi::c_int)
            -> core::ffi::c_int,
    >;
    static _ml_static_ptovirt: Option<unsafe extern "C" fn(u64) -> u64>;
}

#[cfg(feature = "xnu-kext")]
const KERN_SUCCESS: core::ffi::c_int = 0;
#[cfg(feature = "xnu-kext")]
const VM_INHERIT_NONE: core::ffi::c_int = 2;

#[cfg(feature = "xnu-kext")]
const VM_PROT_READ: core::ffi::c_int = 1;
#[cfg(feature = "xnu-kext")]
const VM_PROT_EXECUTE: core::ffi::c_int = 4;

#[cfg(feature = "xnu-kext")]
unsafe extern "C" {
    static _mach_vm_allocate:
        Option<unsafe extern "C" fn(*mut c_void, *mut u64, u64, core::ffi::c_int) -> core::ffi::c_int>;
    static _mach_vm_deallocate:
        Option<unsafe extern "C" fn(*mut c_void, u64, u64) -> core::ffi::c_int>;
    static _vm_map_wire_external: Option<
        unsafe extern "C" fn(*mut c_void, u64, u64, core::ffi::c_int, core::ffi::c_int)
            -> core::ffi::c_int,
    >;
    static _mach_vm_protect: Option<
        unsafe extern "C" fn(*mut c_void, u64, u64, core::ffi::c_int, core::ffi::c_int)
            -> core::ffi::c_int,
    >;
}

#[cfg(not(feature = "xnu-kext"))]
fn kernel_alloc_code(size: usize) -> *mut u8 {
    kalloc(size)
}

#[cfg(not(feature = "xnu-kext"))]
fn kernel_free_code(ptr: *mut u8, size: usize) {
    kernel_free(ptr, size);
}

pub fn make_the_machine_agree() {
    unsafe {
        core::arch::asm!(
            "tlbi vmalle1is",
            "dsb ish",
            "isb",
            options(nostack),
        );
    }
}

pub fn page_size() -> usize {
    (primitives().page_size)()
}

fn kernel_page_size() -> usize {
    crate::gum::page_size_the_kernel_runs_with()
}

pub fn cache_shape() -> u64 {
    (primitives().cache_shape)()
}

pub fn kernel_cache_shape() -> u64 {
    let told: u64;
    unsafe { core::arch::asm!("mrs {}, ctr_el0", out(reg) told, options(nomem, nostack)) };

    told
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

// Arms the wait, gives `check` a chance to observe the condition, and only then
// commits to sleeping — the three-phase Mach protocol, which is race-free
// because a wakeup landing after assert_wait() cancels the pending block.
fn kernel_wait(token: *const u8, timeout_us: Option<u64>, check: &mut dyn FnMut() -> bool) {
    let wait_result = match timeout_us {
        Some(waiting_for) => {
            assert_wait_timeout(token, THREAD_INTERRUPTIBLE, (waiting_for * 1000) as u32, 1)
        }
        None => assert_wait(token, THREAD_INTERRUPTIBLE),
    };
    if wait_result != THREAD_WAITING {
        return;
    }

    if check() {
        thread_wakeup(token);
        return;
    }

    thread_block(None);
}

fn kernel_wake(token: *const u8) {
    thread_wakeup(token);
}

// XNU reschedules a kernel thread that stays runnable, so there is nothing this has
// to do here. It exists because Linux does not forgive a loop that never yields.
fn kernel_yield_now() {}

fn kernel_monotonic_micros() -> i64 {
    (absolutetime_to_nanoseconds(mach_absolute_time()) / 1000) as i64
}

pub fn wall_clock_micros() -> (u32, u32) {
    (primitives().wall_clock_micros)()
}

fn kernel_wall_clock_micros() -> (u32, u32) {
    clock_get_calendar_microtime()
}

// The thread pointer is exposed to JavaScript as a GumThreadId, so it must fit
// in a double without losing precision; the low bits keep it unique per thread.
const JS_SAFE_THREAD_ID_MASK: u64 = (1 << 48) - 1;

fn kernel_current_thread_id() -> u64 {
    let thread_ptr: u64;
    unsafe {
        core::arch::asm!("mrs {}, tpidr_el1", out(reg) thread_ptr, options(nomem, nostack));
    }
    thread_ptr & JS_SAFE_THREAD_ID_MASK
}

pub fn get_kernel_base() -> u64 {
    KERNEL_BASE.load(Ordering::Relaxed)
}

pub fn set_kernel_base(base: u64) {
    KERNEL_BASE.store(base, Ordering::Relaxed);
}

static KERNEL_BASE: AtomicU64 = AtomicU64::new(0);

// Z_WAITOK (modern XNU): block until allocation succeeds.
const Z_WAITOK: u32 = 0x0200;

pub fn kalloc(size: usize) -> *mut u8 {
    unsafe {
        if let Some(f) = _kalloc_data {
            f(size, Z_WAITOK)
        } else if let Some(f) = _kalloc {
            f(size)
        } else {
            core::ptr::null_mut()
        }
    }
}

fn kernel_free(ptr: *mut u8, size: usize) {
    unsafe {
        if let Some(f) = _kfree_data {
            f(ptr, size);
        } else if let Some(f) = _kfree {
            f(ptr, size);
        }
    }
}

#[cfg(feature = "xnu-kext")]
pub fn what_a_proc_callout_is_signed_with() -> usize {
    unsafe { frida_agent_disc_proc_callout as usize }
}

#[cfg(not(feature = "xnu-kext"))]
pub fn what_a_proc_callout_is_signed_with() -> usize {
    0
}

#[cfg(feature = "xnu-kext")]
fn what_a_thread_entry_is_signed_with() -> usize {
    unsafe { frida_agent_disc_thread_continue as usize }
}

#[cfg(not(feature = "xnu-kext"))]
fn what_a_thread_entry_is_signed_with() -> usize {
    0xd507
}

#[cfg(feature = "blob")]
fn what_a_handler_is_signed_with() -> usize {
    0xd36
}

#[cfg(feature = "xnu-kext")]
unsafe extern "C" {
    static frida_agent_disc_thread_continue: u32;
    static frida_agent_disc_proc_callout: u32;
}

pub fn kernel_thread_start(continuation: ContinuationFn, thread_parameter: *mut c_void) -> isize {
    let mut new_thread: *mut c_void = core::ptr::null_mut();
    return unsafe {
        let ptr = crate::pac::ptrauth_sign(continuation as *const u8,
            what_a_thread_entry_is_signed_with());
        _kernel_thread_start(
            core::mem::transmute(ptr),
            thread_parameter,
            &mut new_thread as *mut *mut c_void,
        )
        // TODO: Call thread_deallocate()
    };
}

const THREAD_INTERRUPTIBLE: u32 = 1;
const THREAD_WAITING: i32 = -1;
const THREAD_AWAKENED: i32 = 0;

pub fn assert_wait(event: *const u8, interruptible: u32) -> i32 {
    unsafe { _assert_wait(event, interruptible) }
}

pub fn assert_wait_timeout(
    event: *const u8,
    interruptible: u32,
    interval: u32,
    scale_factor: u32,
) -> i32 {
    unsafe { _assert_wait_timeout(event, interruptible, interval, scale_factor) }
}

pub fn thread_block(continuation: Option<ContinuationFn>) -> i32 {
    unsafe { _thread_block(continuation) }
}

pub fn thread_wakeup(event: *const u8) -> i32 {
    unsafe {
        if let Some(f) = _thread_wakeup {
            f(event)
        } else if let Some(f) = _thread_wakeup_prim {
            f(event, 0, THREAD_AWAKENED)
        } else if let Some(f) = _wakeup {
            f(event);
            0
        } else {
            0
        }
    }
}

pub fn mach_absolute_time() -> u64 {
    unsafe { _mach_absolute_time() }
}

pub fn absolutetime_to_nanoseconds(abstime: u64) -> u64 {
    let mut nanoseconds: u64 = 0;
    unsafe {
        _absolutetime_to_nanoseconds(abstime, &mut nanoseconds);
    }
    nanoseconds
}

pub fn clock_get_calendar_microtime() -> (u32, u32) {
    let mut secs: u32 = 0;
    let mut microsecs: u32 = 0;
    unsafe {
        _clock_get_calendar_microtime(&mut secs, &mut microsecs);
    }
    (secs, microsecs)
}

pub fn map_io(phys_addr: u64, size: u64) -> *mut c_void {
    unsafe { _ml_io_map(phys_addr, size) }
}

#[cfg(feature = "xnu-kext")]
pub fn write_through_a_writable_alias(address: u64, data: *const u8, len: usize) -> bool {
    let page = page_size();
    let mut written = 0;

    while written < len {
        let at = address + written as u64;
        let base = at & !(page as u64 - 1);
        let offset = (at - base) as usize;
        let chunk = core::cmp::min(len - written, page - offset);
        if chunk == 0 {
            return false;
        }

        hold_the_books();
        let known_alias = unsafe { views() }
            .get(&base)
            .or_else(|| unsafe { device_views() }.get(&base))
            .copied();
        let_the_books_go();

        let alias = match known_alias {
            Some(known) => known,
            None => {
                let lives = where_a_page_lives(base);
                if lives == 0 {
                    return false;
                }
                let made = map_io(lives, page as u64);
                if made.is_null() {
                    return false;
                }
                hold_the_books();
                unsafe { device_views() }.insert(base, made as u64);
                let_the_books_go();
                made as u64
            }
        };

        forget_what_the_cache_holds(at, chunk);

        let to = (alias + offset as u64) as *mut u8;
        let from = unsafe { data.add(written) };
        if !copy_the_words_that_differ(from, to as *const u8, to, chunk) {
            return false;
        }

        forget_what_the_cache_holds(at, chunk);

        written += chunk;
    }

    true
}

#[cfg(feature = "xnu-kext")]
fn alias_of(physical: u64, page: usize) -> *mut c_void {
    let held = unsafe { aliases() };
    if let Some(known) = held.get(&physical) {
        return *known as *mut c_void;
    }

    let made = map_io(physical, page as u64);
    if !made.is_null() {
        held.insert(physical, made as u64);
    }

    made
}

#[cfg(feature = "xnu-kext")]
unsafe fn aliases() -> &'static mut alloc::collections::BTreeMap<u64, u64> {
    unsafe { (&raw mut ALIASES).as_mut().unwrap() }
}

#[cfg(feature = "xnu-kext")]
static mut ALIASES: alloc::collections::BTreeMap<u64, u64> =
    alloc::collections::BTreeMap::new();

#[cfg(feature = "xnu-kext")]
fn hold_the_books() {
    while BOOKS
        .compare_exchange(0, 1, Ordering::Acquire, Ordering::Relaxed)
        .is_err()
    {
        core::hint::spin_loop();
    }
}

#[cfg(feature = "xnu-kext")]
fn let_the_books_go() {
    BOOKS.store(0, Ordering::Release);
}

#[cfg(feature = "xnu-kext")]
static BOOKS: core::sync::atomic::AtomicU32 = core::sync::atomic::AtomicU32::new(0);

#[cfg(feature = "xnu-kext")]
pub fn forget_what_the_cache_holds(address: u64, len: usize) {
    let line = 64u64;
    let mut at = address & !(line - 1);
    while at < address + len as u64 {
        unsafe { core::arch::asm!("dc civac, {}", in(reg) at, options(nostack)) };
        at += line;
    }
    unsafe { core::arch::asm!("dsb sy", "isb", options(nomem, nostack)) };
}

#[cfg(feature = "xnu-kext")]
fn copy_the_words_that_differ(from: *const u8, have: *const u8, to: *mut u8, len: usize) -> bool {
    if (to as usize) % 4 != 0 || len % 4 != 0 {
        return false;
    }

    for step in (0..len).step_by(4) {
        let wanted = unsafe { (from.add(step) as *const u32).read_unaligned() };
        let already = unsafe { (have.add(step) as *const u32).read_volatile() };
        if wanted != already {
            unsafe { (to.add(step) as *mut u32).write_volatile(wanted) };
        }
    }

    true
}

#[cfg(feature = "xnu-kext")]
fn copy_as_words(from: *const u8, to: *mut u8, len: usize) -> bool {
    if (to as usize) % 4 != 0 || len % 4 != 0 {
        return false;
    }

    for step in (0..len).step_by(4) {
        let word = unsafe { (from.add(step) as *const u32).read_unaligned() };
        unsafe { (to.add(step) as *mut u32).write_volatile(word) };
    }

    true
}

pub fn virt_to_phys(vaddr: u64) -> u64 {
    unsafe {
        if let Some(ask) = _ml_vtophys {
            return ask(vaddr);
        }
        _ml_static_vtop.unwrap()(vaddr)
    }
}

#[cfg(feature = "blob")]
pub type IOInterruptHandler =
    extern "C" fn(target: *mut c_void, refcon: *mut c_void, nub: *mut c_void, source: i32);

#[cfg(feature = "blob")]
pub fn pci_interrupt(_bus: u8, _devfn: u8) -> Option<u32> {
    None
}

#[cfg(feature = "blob")]
pub fn install_interrupt_handler(
    irq: u32,
    target: *mut c_void,
    handler: IOInterruptHandler,
    refcon: *mut c_void,
) -> i32 {
    let pe = unsafe { __ZN9IOService11getPlatformEv() };

    let name =
        unsafe { __ZN8OSSymbol17withCStringNoCopyEPKc(c"IOInterruptController0000001A".as_ptr()) };

    let lookup: extern "C" fn(*mut IOPlatformExpert, *mut OSSymbol) -> *mut IOInterruptController =
        vf(pe as _, VT_LOOKUP_IC);
    let ic = lookup(pe as *mut IOPlatformExpert, name as *mut OSSymbol);
    if ic.is_null() {
        panic!("Failed to lookup IOInterruptController");
    }

    let nub = kalloc(0x88);
    unsafe {
        core::ptr::write_bytes(nub, 0, 0x88);
        __ZN9IOServiceC2Ev(nub as *mut core::ffi::c_void);
    }

    type IOServiceInitFn = unsafe extern "C" fn(*mut c_void, *mut c_void) -> bool;
    let init_fn: IOServiceInitFn = vf(nub as *mut c_void, 21);
    unsafe { init_fn(nub as *mut c_void, core::ptr::null_mut()) };

    let interrupt_sources =
        kalloc(core::mem::size_of::<IOInterruptSource>()) as *mut IOInterruptSource;

    let source_bytes = irq.to_ne_bytes();
    let vector_data =
        unsafe { __ZN6OSData9withBytesEPKvj(source_bytes.as_ptr() as *const core::ffi::c_void, 4) };

    unsafe {
        (*interrupt_sources).interrupt_controller = ic;
        (*interrupt_sources).vector_data = vector_data;
    }

    unsafe {
        let interrupt_sources_ptr = (nub as *mut u8).offset(0x80) as *mut *mut IOInterruptSource;
        *interrupt_sources_ptr = interrupt_sources;
    }

    let reg: extern "C" fn(
        *mut _,
        *mut c_void,
        i32,
        *mut c_void,
        IOInterruptHandler,
        *mut c_void,
    ) -> i32 = vf(ic as _, VT_REGISTER_INT);

    let signed_handler = unsafe {
        let handler_ptr = crate::pac::ptrauth_sign(handler as *const u8,
            what_a_handler_is_signed_with());
        core::mem::transmute::<*const u8, IOInterruptHandler>(handler_ptr)
    };

    let kr = reg(ic, nub as *mut c_void, 0, target, signed_handler, refcon);
    if kr != 0 {
        return kr;
    }

    let en: extern "C" fn(*mut _, *mut c_void, i32) -> i32 = vf(ic as _, VT_ENABLE_INT);
    en(ic, nub as *mut c_void, 0)
}

#[cfg(feature = "blob")]
#[repr(C)]
struct IOPlatformExpert {
    _p: [u8; 0],
}

#[cfg(feature = "blob")]
#[repr(C)]
struct IOInterruptController {
    _p: [u8; 0],
}

#[cfg(feature = "blob")]
#[repr(C)]
struct OSSymbol {
    _p: [u8; 0],
}

#[cfg(feature = "blob")]
#[repr(C)]
struct OSData {
    _p: [u8; 0],
}

#[cfg(feature = "blob")]
#[repr(C)]
struct IOInterruptSource {
    interrupt_controller: *mut IOInterruptController,
    vector_data: *mut OSData,
}

#[inline(always)]
#[cfg(feature = "blob")]
fn vf<T>(obj: *mut c_void, slot: isize) -> T
where
    T: Copy,
{
    let vtable_ptr = unsafe { *(obj as *const *const usize) };
    let vtable = unsafe { crate::pac::ptrauth_strip_data(vtable_ptr as *const u8) as *const usize };
    let entry = unsafe { *vtable.offset(slot) };
    let entry_ptr = unsafe { crate::pac::ptrauth_strip_data(entry as *const u8) };
    unsafe { core::mem::transmute_copy::<*const u8, T>(&entry_ptr) }
}

#[cfg(feature = "blob")]
const IO_SERVICE_VTABLE_LENGTH: isize = 168;

#[cfg(feature = "blob")]
const VT_LOOKUP_IC: isize = IO_SERVICE_VTABLE_LENGTH + 25; // IOPlatformExpert
#[cfg(feature = "blob")]
const VT_REGISTER_INT: isize = IO_SERVICE_VTABLE_LENGTH + 0; // IOInterruptController
#[cfg(feature = "blob")]
const VT_ENABLE_INT: isize = IO_SERVICE_VTABLE_LENGTH + 3; // IOInterruptController

type ContinuationFn = ThreadEntry;

unsafe extern "C" {
    static _panic: unsafe extern "C" fn(*const u8);
    static _IOLog: unsafe extern "C" fn(*const u8, ...);
    // QEMU XNU exports plain kalloc/kfree; iOS XNU exports kalloc_data/kfree_data
    // (modern data-typed allocator KPIs). The host fills in whichever pair the
    // running kernel provides; the other slot stays null.
    static _kalloc: Option<unsafe extern "C" fn(usize) -> *mut u8>;
    static _kfree: Option<unsafe extern "C" fn(*mut u8, usize) -> *mut u8>;
    static _kalloc_data: Option<unsafe extern "C" fn(usize, u32) -> *mut u8>;
    static _kfree_data: Option<unsafe extern "C" fn(*mut u8, usize)>;
    static _kernel_thread_start:
        unsafe extern "C" fn(*const (), *mut c_void, *mut *mut c_void) -> isize;
    static _assert_wait: unsafe extern "C" fn(*const u8, u32) -> i32;
    static _assert_wait_timeout: unsafe extern "C" fn(*const u8, u32, u32, u32) -> i32;
    static _thread_block: unsafe extern "C" fn(Option<ContinuationFn>) -> i32;
    // QEMU XNU exports thread_wakeup; iOS XNU exports the BSD-style wakeup wrapper.
    static _thread_wakeup: Option<unsafe extern "C" fn(*const u8) -> i32>;
    static _thread_wakeup_prim: Option<unsafe extern "C" fn(*const u8, i32, i32) -> i32>;
    static _wakeup: Option<unsafe extern "C" fn(*const u8)>;
    static _mach_absolute_time: unsafe extern "C" fn() -> u64;
    static _absolutetime_to_nanoseconds: unsafe extern "C" fn(u64, *mut u64);
    static _clock_get_calendar_microtime: unsafe extern "C" fn(*mut u32, *mut u32);
    static _ml_io_map: unsafe extern "C" fn(u64, u64) -> *mut c_void;
    static _ml_vtophys: Option<unsafe extern "C" fn(u64) -> u64>;
    static _ml_static_vtop: Option<unsafe extern "C" fn(u64) -> u64>;
}

#[cfg(feature = "blob")]
unsafe extern "C" {
    static __ZN9IOService11getPlatformEv: unsafe extern "C" fn() -> *mut c_void;
    static __ZN9IOServiceC2Ev: unsafe extern "C" fn(*mut core::ffi::c_void);
    static __ZN8OSSymbol17withCStringNoCopyEPKc:
        unsafe extern "C" fn(*const core::ffi::c_char) -> *const OSSymbol;
    static __ZN6OSData9withBytesEPKvj:
        unsafe extern "C" fn(*const core::ffi::c_void, u32) -> *mut OSData;
}
