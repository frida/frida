
pub fn leave_the_list() {
    let Some(list) = the_list() else {
        return;
    };
    let me = unsafe { (list.which_one_is_this)() };

    unsafe { (list.take_the_lock)(list.lock, THE_WAY_THE_SYSTEM_TAKES_IT) };

    let (after, before) = (read(me + AFTER_IT), read(me + WHAT_POINTS_AT_IT));
    if after != 0 {
        write(after + WHAT_POINTS_AT_IT, before);
    } else {
        write(list.head + LAST_OF_THEM, before);
    }
    write(before, after);

    unsafe { (list.let_the_lock_go)(list.lock) };
}

pub fn join_the_list_again() {
    let Some(list) = the_list() else {
        return;
    };
    let me = unsafe { (list.which_one_is_this)() };

    unsafe { (list.take_the_lock)(list.lock, THE_WAY_THE_SYSTEM_TAKES_IT) };

    let last = read(list.head + LAST_OF_THEM);
    write(me + AFTER_IT, 0);
    write(me + WHAT_POINTS_AT_IT, last);
    write(last, me);
    write(list.head + LAST_OF_THEM, me + AFTER_IT);

    unsafe { (list.let_the_lock_go)(list.lock) };
}

fn the_list() -> Option<&'static List> {
    let held = unsafe { (&raw const LIST).as_ref().unwrap() };
    if held.is_some() {
        return held.as_ref();
    }

    let walks_it = crate::xnu_libsystem::function_named(b"/libsystem_pthread.dylib",
        b"_pthread_from_mach_thread_np")?;

    let mut page = 0;
    let mut lock = 0;
    let mut head = 0;
    let mut calls = [0u64; 2];
    let mut called = 0;
    for step in 0..HOW_FAR_IN_TO_LOOK {
        let at = walks_it + (step * 4);
        let word = unsafe { (at as *const u32).read_volatile() };

        if word & ADRP_SHAPE == ADRP {
            page = (at & !0xfff).wrapping_add(adrp_reach(word));
        } else if word & ADD_SHAPE == ADD && page != 0 && lock == 0 {
            lock = page + ((word >> 10) & 0xfff) as u64;
        } else if word & LDR_SHAPE == LDR && page != 0 && head == 0 {
            head = page + (((word >> 10) & 0xfff) as u64 * 8);
        } else if word & BL_SHAPE == BL && called != calls.len() {
            calls[called] = at.wrapping_add(bl_reach(word));
            called += 1;
        }
    }
    if lock == 0 || head == 0 || called != calls.len() {
        return None;
    }

    let found = List {
        head,
        lock,
        which_one_is_this: unsafe { core::mem::transmute(crate::xnu_libsystem::function_named(
            b"/libsystem_pthread.dylib", b"_pthread_self")?) },
        take_the_lock: unsafe { core::mem::transmute(calls[0]) },
        let_the_lock_go: unsafe { core::mem::transmute(calls[1]) },
    };

    unsafe { LIST = Some(found) };
    unsafe { (&raw const LIST).as_ref().unwrap().as_ref() }
}

struct List {
    head: u64,
    lock: u64,
    which_one_is_this: unsafe extern "C" fn() -> u64,
    take_the_lock: unsafe extern "C" fn(u64, u32),
    let_the_lock_go: unsafe extern "C" fn(u64),
}

static mut LIST: Option<List> = None;

fn read(at: u64) -> u64 {
    unsafe { (at as *const u64).read_volatile() }
}

fn write(at: u64, word: u64) {
    unsafe { (at as *mut u64).write_volatile(word) };
}

fn adrp_reach(word: u32) -> u64 {
    let said = (((word >> 5) & 0x7ffff) << 2) | ((word >> 29) & 3);
    (((said as i32) << 11) >> 11) as i64 as u64 * 0x1000
}

fn bl_reach(word: u32) -> u64 {
    ((((word & 0x3ff_ffff) as i32) << 6) >> 6) as i64 as u64 * 4
}

const HOW_FAR_IN_TO_LOOK: u64 = 24;

const ADRP_SHAPE: u32 = 0x9f00_0000;
const ADRP: u32 = 0x9000_0000;
const ADD_SHAPE: u32 = 0xffc0_0000;
const ADD: u32 = 0x9100_0000;
const LDR_SHAPE: u32 = 0xffc0_0000;
const LDR: u32 = 0xf940_0000;
const BL_SHAPE: u32 = 0xfc00_0000;
const BL: u32 = 0x9400_0000;

const AFTER_IT: u64 = 0x10;
const WHAT_POINTS_AT_IT: u64 = 0x18;
const LAST_OF_THEM: u64 = 8;

const THE_WAY_THE_SYSTEM_TAKES_IT: u32 = 0x50000;
