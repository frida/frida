
use core::ffi::{c_int, c_void};

pub fn hang_the_bell() {
    if unsafe { HUNG } {
        return;
    }
    unsafe { HUNG = true };

    let calls = unsafe { _sysent };
    if calls == 0 {
        return;
    }

    let signed = unsafe {
        ((calls + ENDING_A_WAIT * AN_ENTRY) as *const u64).read_volatile()
    };
    let ending_a_wait = unsafe { crate::pac::ptrauth_strip_data(signed as *const u8) };
    unsafe { WHERE_IT_HANGS = ending_a_wait as *mut c_void };
    unsafe {
        let interceptor = crate::bindings::gum_interceptor_obtain();
        crate::bindings::gum_interceptor_begin_transaction(interceptor);
        crate::bindings::gum_interceptor_replace(interceptor, ending_a_wait as *mut c_void,
            answer_the_bell as *mut c_void, (&raw mut ENDS_A_WAIT) as *mut *mut c_void,
            core::ptr::null_mut());
        crate::bindings::gum_interceptor_end_transaction(interceptor);
    }
}

unsafe extern "C" fn answer_the_bell(process: *mut c_void, asked: *mut c_void,
    answer: *mut i32) -> c_int
{
    if unsafe { (*(asked as *const Asked)).value } == WHAT_ONLY_A_COPY_SAYS {
        RUNG.fetch_add(1, core::sync::atomic::Ordering::Relaxed);

        if let Some(this_thread) = unsafe { _current_thread } {
            crate::xnu_hiding::a_thread_of_ours(unsafe { this_thread() });
        }


        crate::kernel::wake(crate::glib::wakeup_token());
        return 0;
    }

    match unsafe { ENDS_A_WAIT } {
        Some(end_it) => unsafe { end_it(process, asked, answer) },
        None => 0,
    }
}

pub fn take_the_bell_down() {
    if !unsafe { HUNG } {
        return;
    }
    unsafe { HUNG = false };

    let hangs_on = unsafe { WHERE_IT_HANGS };
    if hangs_on.is_null() {
        return;
    }

    unsafe {
        let interceptor = crate::bindings::gum_interceptor_obtain();
        crate::bindings::gum_interceptor_begin_transaction(interceptor);
        crate::bindings::gum_interceptor_revert(interceptor, hangs_on);
        crate::bindings::gum_interceptor_end_transaction(interceptor);
        ENDS_A_WAIT = None;
        WHERE_IT_HANGS = core::ptr::null_mut();
    }
}

pub fn ring_the_bell() {
    let arena = crate::xnu_user::arena();
    if arena == 0 {
        return;
    }

    unsafe {
        crate::xnu_user_calls::ask(ENDING_A_WAIT as i64, [(WAITING_ON_AN_ADDRESS | EVERY_WAITER)
            as u64, arena + crate::xnu_relay::BELL_WORD, WHAT_ONLY_A_COPY_SAYS, 0])
    };
}

pub fn wake_the_copy(process: *mut c_void, ours: u64, theirs: u64) {
    let Some(end_the_wait) = (unsafe { _ulock_wake }) else {
        return;
    };

    unsafe {
        let word = (ours + crate::xnu_relay::WAKE_WORD) as *mut u32;
        word.write_volatile(word.read_volatile().wrapping_add(1));
    }

    let mut asked = Asked { how: WAITING_ON_AN_ADDRESS | EVERY_WAITER, _padding: 0,
        address: theirs + crate::xnu_relay::WAKE_WORD, value: 0 };
    let mut answer = 0;
    unsafe { end_the_wait(process, &mut asked as *mut Asked as *mut c_void, &mut answer) };
}

#[repr(C)]
struct Asked {
    how: u32,
    _padding: u32,
    address: u64,
    value: u64,
}

type EndsAWait = unsafe extern "C" fn(*mut c_void, *mut c_void, *mut i32) -> c_int;

static mut HUNG: bool = false;
static RUNG: core::sync::atomic::AtomicU32 = core::sync::atomic::AtomicU32::new(0);
static mut ENDS_A_WAIT: Option<EndsAWait> = None;
static mut WHERE_IT_HANGS: *mut c_void = core::ptr::null_mut();

const ENDING_A_WAIT: usize = 516;
const AN_ENTRY: usize = 24;

const WHAT_ONLY_A_COPY_SAYS: u64 = 0x6672_6964_6100_0002;

const WAITING_ON_AN_ADDRESS: u32 = 1;
const EVERY_WAITER: u32 = 0x100;

unsafe extern "C" {
    static _current_thread: Option<unsafe extern "C" fn() -> *mut c_void>;
    pub static _sysent: usize;
    static _ulock_wake: Option<unsafe extern "C" fn(*mut c_void, *mut c_void, *mut i32) -> c_int>;
}
