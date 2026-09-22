
use core::ffi::{c_char, c_int, c_void};

pub fn spawn_when_the_loop_can(request_id: u16, words: &[u8]) {
    let asked = each_word_asked_for();
    if words.len() > asked.len() {
        return;
    }
    asked.fill(0);
    asked[..words.len()].copy_from_slice(words);

    unsafe {
        ASKED_BY = request_id;
        ASKED_FOR = true;
    }
}

pub fn a_program_is_wanted() -> bool {
    unsafe { ASKED_FOR }
}

pub fn start_what_was_asked_for(say: &mut dyn FnMut(u16, u32)) {
    if !unsafe { ASKED_FOR } {
        return;
    }
    unsafe { ASKED_FOR = false };

    say(unsafe { ASKED_BY }, start_a_program());
}

fn start_a_program() -> u32 {
    gate_spawns(true);
    look_for_new_processes();

    let words = each_word_asked_for();
    let asked_for_an_application = !words[..words.iter().position(|byte| *byte == 0).unwrap_or(0)]
        .contains(&b'/');
    let helper = if asked_for_an_application {
        crate::xnu_injection::a_copy_that_can_ask_the_system()
    } else {
        crate::xnu_injection::a_copy_that_can_start_a_program()
    };

    let Some(arena) = helper else {
        return 0;
    };

    let since = unsafe { HOW_MANY_HELD };

    for (at, byte) in words.iter().enumerate() {
        unsafe { ((arena + crate::xnu_relay::SPAWN_WORDS + at as u64) as *mut u8)
            .write_volatile(*byte) };
    }

    let put = |at: u64, value: u64| unsafe {
        ((arena + at) as *mut u64).write_volatile(value)
    };
    put(crate::xnu_relay::SPAWN_ANSWER, crate::xnu_relay::NOTHING_WANTED);
    put(crate::xnu_relay::SPAWN_WANTED, 1);
    crate::xnu_injection::wake_the_copy_at(arena);

    let answered = &mut || unsafe {
        ((arena + crate::xnu_relay::SPAWN_ANSWER) as *const u64).read_volatile()
    } != crate::xnu_relay::NOTHING_WANTED;
    let began = crate::kernel::monotonic_micros();
    loop {
        let said = unsafe {
            ((arena + crate::xnu_relay::SPAWN_ANSWER) as *const u64).read_volatile()
        };
        if said == crate::xnu_relay::NOTHING_WANTED {
            let waited = (crate::kernel::monotonic_micros() - began) as u64;
            if waited >= LONG_ENOUGH_TO_START {
                break;
            }
            crate::kernel::wait(crate::glib::wakeup_token(),
                Some(LONG_ENOUGH_TO_START - waited), answered);
            continue;
        }

        put(crate::xnu_relay::SPAWN_WANTED, crate::xnu_relay::NOTHING_WANTED);
        if said == crate::xnu_relay::THE_SYSTEM_IS_STARTING_IT {
            return the_one_about_to_run(arena, since);
        }
        if said != crate::xnu_relay::DONE {
            return 0;
        }
        return unsafe {
            ((arena + crate::xnu_relay::SPAWN_ID) as *const u64).read_volatile()
        } as u32;
    }

    0
}

fn the_one_about_to_run(arena: u64, since: u64) -> u32 {
    let mut called = [0u8; NAME_ROOM];
    for (step, byte) in called.iter_mut().enumerate() {
        *byte = unsafe {
            ((arena + crate::xnu_relay::SPAWN_WORDS + step as u64) as *const u8).read_volatile()
        };
    }
    let called = &called[..called.iter().position(|byte| *byte == 0).unwrap_or(0)];
    if called.is_empty() {
        return 0;
    }

    let began = crate::kernel::monotonic_micros();
    loop {
        let newest = each_held().iter()
            .filter(|held| held.waiting && held.seen_at > since && held.is_called(called))
            .max_by_key(|held| held.seen_at);
        if let Some(held) = newest {
            return held.id;
        }

        let waited = (crate::kernel::monotonic_micros() - began) as u64;
        if waited >= LONG_ENOUGH_TO_COME_UP {
            return 0;
        }
        crate::kernel::wait(crate::glib::wakeup_token(), Some(LONG_ENOUGH_TO_COME_UP - waited),
            &mut || false);
    }
}

const LONG_ENOUGH_TO_COME_UP: u64 = 8_000_000;

fn each_word_asked_for() -> &'static mut [u8; WORD_ROOM] {
    unsafe { (&raw mut ASKED_WORDS).as_mut().unwrap() }
}

static mut ASKED_FOR: bool = false;
static mut ASKED_BY: u16 = 0;
static mut ASKED_WORDS: [u8; WORD_ROOM] = [0; WORD_ROOM];

const WORD_ROOM: usize = crate::xnu_relay::SPAWN_WORDS_ROOM as usize;
const LONG_ENOUGH_TO_START: u64 = 8_000_000;

pub fn gate_spawns(on: bool) {
    unsafe { GATING = on };
}

pub fn look_for_new_processes() {
    if unsafe { GATING } {
        take_over_the_first_word_a_process_says();
    }
}

pub fn is_held(id: u32) -> bool {
    each_held().iter().any(|held| held.waiting && held.id == id)
}

pub fn a_spawn_is_held() -> bool {
    each_held().iter().any(|held| held.waiting && !held.told_of)
}

pub fn tell_of_held_spawns(say: &mut dyn FnMut(u32, &[u8])) {
    for held in each_held().iter_mut() {
        if !held.waiting || held.told_of {
            continue;
        }
        held.told_of = true;
        let length = held.name.iter().position(|byte| *byte == 0).unwrap_or(held.name.len());
        say(held.id, &held.name[..length]);
    }
}

pub fn resume_process(id: u32) -> bool {
    for held in each_held().iter_mut().filter(|held| held.waiting && held.id == id) {
        held.waiting = false;
        crate::kernel::wake(held as *const Held as *const u8);
    }

    true
}

fn take_over_the_first_word_a_process_says() {
    if unsafe { TAKEN_OVER } {
        return;
    }
    unsafe { TAKEN_OVER = true };

    let (Some(word), Some(work)) = (the_call_that_says_it(SAYING_HOW_THREADS_ARE_MADE),
        unsafe { _task_restartable_ranges_register })
    else {
        return;
    };
    let work = work as u64;

    unsafe {
        WHERE_IT_IS_SAID = word as *mut c_void;
        WHERE_IT_IS_ASKED = work as *mut c_void;
        let interceptor = crate::bindings::gum_interceptor_obtain();
        crate::bindings::gum_interceptor_begin_transaction(interceptor);
        crate::bindings::gum_interceptor_replace(interceptor, word as *mut c_void,
            hold_it_early as *mut c_void, (&raw mut THE_WORD) as *mut *mut c_void,
            core::ptr::null_mut());
        crate::bindings::gum_interceptor_replace(interceptor, work as *mut c_void,
            hold_it_there as *mut c_void, (&raw mut THE_CLAIM) as *mut *mut c_void,
            core::ptr::null_mut());
        crate::bindings::gum_interceptor_end_transaction(interceptor);
    }
}

pub fn give_the_word_back() {
    if !unsafe { TAKEN_OVER } {
        return;
    }
    unsafe { TAKEN_OVER = false };

    let where_it_is_said = unsafe { WHERE_IT_IS_SAID };
    if where_it_is_said.is_null() {
        return;
    }

    unsafe {
        let interceptor = crate::bindings::gum_interceptor_obtain();
        crate::bindings::gum_interceptor_begin_transaction(interceptor);
        crate::bindings::gum_interceptor_revert(interceptor, where_it_is_said);
        if !WHERE_IT_IS_ASKED.is_null() {
            crate::bindings::gum_interceptor_revert(interceptor, WHERE_IT_IS_ASKED);
        }
        crate::bindings::gum_interceptor_end_transaction(interceptor);
        THE_WORD = None;
        THE_CLAIM = None;
        WHERE_IT_IS_SAID = core::ptr::null_mut();
        WHERE_IT_IS_ASKED = core::ptr::null_mut();
    }
}

fn the_call_that_says_it(which: usize) -> Option<u64> {
    let calls = unsafe { crate::xnu_bell::_sysent };
    if calls == 0 {
        return None;
    }

    let signed = unsafe { ((calls + which * AN_ENTRY) as *const u64).read_volatile() };

    Some(unsafe { crate::pac::ptrauth_strip_data(signed as *const u8) } as u64)
}

unsafe extern "C" fn hold_it_early(process: *mut c_void, asked: *mut c_void, answer: *mut i32)
    -> c_int
{
    let went = match unsafe { THE_WORD } {
        Some(say_it) => unsafe { say_it(process, asked, answer) },
        None => 0,
    };

    hold_it(process);

    went
}

unsafe extern "C" fn hold_it_there(task: *mut c_void, ranges: *mut c_void, how_many: u32)
    -> c_int
{
    let kept = crate::xnu_injection::count_only_what_the_process_made(task);

    let went = match unsafe { THE_CLAIM } {
        Some(claim_it) => unsafe { claim_it(task, ranges, how_many) },
        None => 0,
    };

    if let Some(kept) = kept {
        crate::xnu_injection::count_them_all_again(task, kept);
    }

    went
}

fn hold_it(process: *mut c_void) {
    if !unsafe { GATING } {
        return;
    }

    let Some(held) = note_it(process) else {
        return;
    };

    let waiting = &mut || held.waiting;
    while held.waiting {
        crate::kernel::wait(held as *const Held as *const u8, None, waiting);
    }
}

fn note_it(process: *mut c_void) -> Option<&'static mut Held> {
    let (Some(number_of), Some(name_of)) = (unsafe { _proc_pid }, unsafe { _proc_best_name })
    else {
        return None;
    };

    let held = each_held().iter_mut().find(|held| !held.waiting)?;

    held.id = unsafe { number_of(process) } as u32;
    held.told_of = false;
    held.name = [0; NAME_ROOM];

    let said = unsafe { name_of(process) } as *const u8;
    for (at, letter) in held.name.iter_mut().enumerate().take(NAME_ROOM - 1) {
        let byte = unsafe { said.add(at).read() };
        if byte == 0 {
            break;
        }
        *letter = byte;
    }

    held.waiting = true;
    unsafe { HOW_MANY_HELD += 1 };
    held.seen_at = unsafe { HOW_MANY_HELD };

    crate::kernel::wake(crate::glib::wakeup_token());

    Some(held)
}

fn each_held() -> &'static mut [Held; HOW_MANY_AT_ONCE] {
    unsafe { (&raw mut HELD).as_mut().unwrap() }
}

impl Held {
    fn is_called(&self, called: &[u8]) -> bool {
        let length = self.name.iter().position(|byte| *byte == 0).unwrap_or(self.name.len());

        self.name[..length] == called[..length.min(called.len())]
    }
}

#[derive(Clone, Copy)]
struct Held {
    id: u32,
    seen_at: u64,
    name: [u8; NAME_ROOM],
    waiting: bool,
    told_of: bool,
}

static mut GATING: bool = false;
static mut TAKEN_OVER: bool = false;
type ACall = unsafe extern "C" fn(*mut c_void, *mut c_void, *mut i32) -> c_int;

static mut THE_WORD: Option<ACall> = None;
static mut THE_CLAIM: Option<unsafe extern "C" fn(*mut c_void, *mut c_void, u32) -> c_int> = None;
static mut WHERE_IT_IS_SAID: *mut c_void = core::ptr::null_mut();
static mut WHERE_IT_IS_ASKED: *mut c_void = core::ptr::null_mut();
static mut HOW_MANY_HELD: u64 = 0;
static mut HELD: [Held; HOW_MANY_AT_ONCE] = [Held {
    id: 0,
    seen_at: 0,
    name: [0; NAME_ROOM],
    waiting: false,
    told_of: false,
}; HOW_MANY_AT_ONCE];

const HOW_MANY_AT_ONCE: usize = 64;
const NAME_ROOM: usize = 64;
const SAYING_HOW_THREADS_ARE_MADE: usize = 366;
const AN_ENTRY: usize = 24;

unsafe extern "C" {
    static _task_restartable_ranges_register:
        Option<unsafe extern "C" fn(*mut c_void, *mut c_void, u32) -> c_int>;
    pub static _get_bsdtask_info: Option<unsafe extern "C" fn(*mut c_void) -> *mut c_void>;
    pub static _proc_pid: Option<unsafe extern "C" fn(*mut c_void) -> c_int>;
    static _proc_best_name: Option<unsafe extern "C" fn(*mut c_void) -> *const c_char>;
}
