use core::ptr;
use core::sync::atomic::{AtomicUsize, Ordering};

use crate::bindings::{g_wait_is_set, gint64, gpointer};
use crate::kernel;

const G_WAIT_INFINITE: gint64 = -1;

pub static mut WAKEUP_TOKEN: u64 = 0;

pub fn wakeup_token() -> *const u8 {
    ptr::addr_of!(WAKEUP_TOKEN) as *const u8
}

#[unsafe(no_mangle)]
pub extern "C" fn g_get_monotonic_time() -> gint64 {
    kernel::monotonic_micros()
}

#[unsafe(no_mangle)]
pub extern "C" fn g_wait_sleep(token: gpointer, timeout_us: gint64) {
    let timeout = if timeout_us == G_WAIT_INFINITE {
        None
    } else {
        Some(timeout_us as u64)
    };

    let ours = is_loop_thread();
    let event = if ours {
        LOOP_TOKEN.store(token as usize, Ordering::Release);
        ptr::addr_of_mut!(WAKEUP_TOKEN) as *const u8
    } else {
        token as *const u8
    };

    kernel::wait(event, timeout, &mut || unsafe { g_wait_is_set(token) != 0 });

    if ours {
        LOOP_TOKEN.store(0, Ordering::Release);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn g_wait_wake(token: gpointer) {
    kernel::poke_loop_wakeup();
    kernel::wake(ptr::addr_of_mut!(WAKEUP_TOKEN) as *const u8);
    kernel::wake(token as *const u8);
}

pub fn is_loop_thread() -> bool {
    kernel::current_thread_id() == LOOP_THREAD.load(Ordering::Acquire) as u64
}

pub fn own_the_loop() {
    LOOP_THREAD.store(kernel::current_thread_id() as usize, Ordering::Release);
}

static LOOP_THREAD: AtomicUsize = AtomicUsize::new(0);
static LOOP_TOKEN: AtomicUsize = AtomicUsize::new(0);

#[unsafe(no_mangle)]
pub extern "C" fn g_io_channel_unix_new(_fd: i32) -> gpointer {
    ptr::null_mut()
}

#[unsafe(no_mangle)]
pub extern "C" fn g_spawn_async_with_pipes() -> i32 {
    0
}
