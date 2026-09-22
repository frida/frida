use alloc::ffi::CString;
use alloc::vec::Vec;
use core::ffi::{c_int, c_void};
use core::mem::size_of;
use core::ptr;

use super::layout::field_offset;
use super::native;
use super::processes::task_with_id;

pub fn spawn_process(words: &[&str]) -> u32 {
    let Some(words) = said_as(words) else {
        return 0;
    };

    super::injection::hold_what_is_spawned();

    let held = native::alloc(size_of::<Held>()) as *mut Held;
    unsafe {
        held.write(Held {
            id: 0,
            words,
            environment: environment(),
        })
    };

    let info = unsafe {
        _call_usermodehelper_setup(
            (*held).words.first_word(),
            (*held).words.as_argv(),
            (*held).environment.as_argv(),
            GFP_KERNEL,
            HOLD_SPAWN,
            RELEASE_WORDS,
            held as *mut c_void,
        )
    };
    if info.is_null() {
        return 0;
    }

    if unsafe { _call_usermodehelper_exec(info, UMH_WAIT_EXEC) } != 0 {
        return 0;
    }

    unsafe { (*held).id }
}

pub fn resume_process(id: u32) -> bool {
    let Some(task) = task_with_id(id) else {
        return false;
    };

    native::send_signal(CONTINUE, task);

    true
}

pub fn gate_spawns(on: bool) {
    GATING.store(on, core::sync::atomic::Ordering::Release);
    if on {
        super::injection::hold_what_is_spawned();
    }
}

pub fn holds_this_one(task: usize) -> bool {
    if GATING.load(core::sync::atomic::Ordering::Acquire) {
        return true;
    }

    let Some(id) = super::injection::id_of(task) else {
        return false;
    };

    let held = unsafe { (&raw const HELD).read() };

    held != 0 && held == id
}

// A spawn is held in the kernel's own exec path, where telling the host is not this thread's
// to do; the loop says it once it is back where sending a frame belongs.
pub fn note_a_held_spawn(id: u32, program: *const u8) {
    let said = unsafe { core::ffi::CStr::from_ptr(program.cast()) };
    unsafe { held_spawns() }.push((id, said.to_bytes().to_vec()));
}

pub fn a_spawn_is_held() -> bool {
    !unsafe { held_spawns() }.is_empty()
}

pub fn tell_of_held_spawns(say: &mut dyn FnMut(u32, &[u8])) {
    for (id, program) in core::mem::take(unsafe { held_spawns() }) {
        say(id, &program);
    }
}

unsafe fn held_spawns() -> &'static mut Vec<(u32, Vec<u8>)> {
    unsafe { (&raw mut HELD_SPAWNS).as_mut().unwrap() }
}

static mut HELD_SPAWNS: Vec<(u32, Vec<u8>)> = Vec::new();
static GATING: core::sync::atomic::AtomicBool = core::sync::atomic::AtomicBool::new(false);

#[unsafe(no_mangle)]
pub unsafe extern "C" fn frida_cb_hold_spawn(info: *mut c_void, _credentials: *mut c_void) -> c_int {
    let Some(at) = field_offset("subprocess_info", "data") else {
        return 0;
    };
    let held = unsafe { ((info as usize + at) as *const usize).read() } as *mut Held;

    let id = super::injection::id_of(native::current_task() as usize).unwrap_or(0);
    unsafe {
        (*held).id = id;
        (&raw mut HELD).write(id);
    }

    0
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn frida_cb_release_words(info: *mut c_void) {
    let Some(at) = field_offset("subprocess_info", "data") else {
        return;
    };
    let held = unsafe { ((info as usize + at) as *const usize).read() } as *mut Held;

    unsafe { ptr::drop_in_place(held) };
    native::free(held as *mut u8, size_of::<Held>());
}

fn said_as(words: &[&str]) -> Option<Words> {
    let said: Vec<CString> = words.iter().map(|word| CString::new(*word).unwrap()).collect();
    if said.is_empty() {
        return None;
    }

    let mut pointers: Vec<*const u8> = said.iter().map(|word| word.as_ptr() as *const u8).collect();
    pointers.push(ptr::null());

    Some(Words { said, pointers })
}

fn environment() -> Words {
    said_as(&["HOME=/", "PATH=/sbin:/usr/sbin:/bin:/usr/bin", "TERM=linux"]).unwrap()
}

struct Held {
    id: u32,
    words: Words,
    environment: Words,
}

struct Words {
    said: Vec<CString>,
    pointers: Vec<*const u8>,
}

impl Words {
    fn first_word(&self) -> *const u8 {
        self.said[0].as_ptr() as *const u8
    }

    fn as_argv(&self) -> *const *const u8 {
        self.pointers.as_ptr()
    }
}

static mut HELD: u32 = 0;

const GFP_KERNEL: u32 = 0xcc0;
const UMH_WAIT_EXEC: c_int = 1;
const CONTINUE: c_int = 18;

unsafe extern "C" {
    #[cfg(not(target_arch = "x86"))]
    static _call_usermodehelper_setup: unsafe extern "C" fn(
        *const u8,
        *const *const u8,
        *const *const u8,
        u32,
        unsafe extern "C" fn(*mut c_void, *mut c_void) -> c_int,
        unsafe extern "C" fn(*mut c_void),
        *mut c_void,
    ) -> *mut c_void;
    #[cfg(not(target_arch = "x86"))]
    static _call_usermodehelper_exec: unsafe extern "C" fn(*mut c_void, c_int) -> c_int;
}

#[cfg(target_arch = "x86")]
unsafe extern "C" {
    #[link_name = "frida_k_call_usermodehelper_exec"]
    fn _call_usermodehelper_exec(a0: *mut c_void, a1: c_int) -> c_int;
}

#[cfg(target_arch = "x86")]
unsafe extern "C" {
    #[link_name = "frida_k_call_usermodehelper_setup"]
    fn _call_usermodehelper_setup(
        a0: *const u8,
        a1: *const *const u8,
        a2: *const *const u8,
        a3: u32,
        a4: unsafe extern "C" fn(*mut c_void, *mut c_void) -> c_int,
        a5: unsafe extern "C" fn(*mut c_void),
        a6: *mut c_void,
    ) -> *mut c_void;
    fn frida_kcb_hold_spawn(info: *mut c_void, credentials: *mut c_void) -> c_int;
    fn frida_kcb_release_words(info: *mut c_void);
}

#[cfg(target_arch = "x86")]
const HOLD_SPAWN: unsafe extern "C" fn(*mut c_void, *mut c_void) -> c_int = frida_kcb_hold_spawn;
#[cfg(not(target_arch = "x86"))]
const HOLD_SPAWN: unsafe extern "C" fn(*mut c_void, *mut c_void) -> c_int = frida_cb_hold_spawn;

#[cfg(target_arch = "x86")]
const RELEASE_WORDS: unsafe extern "C" fn(*mut c_void) = frida_kcb_release_words;
#[cfg(not(target_arch = "x86"))]
const RELEASE_WORDS: unsafe extern "C" fn(*mut c_void) = frida_cb_release_words;
