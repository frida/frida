use core::sync::atomic::{AtomicBool, Ordering};

// The copy's own memory, which is not the C library's: that one asks for memory of its own
// while it is taking its lock, and a lock that allocates through the allocator it guards never
// finishes the first time it is asked.
pub fn take(size: usize) -> *mut u8 {
    let wanted = wanted_for(size);
    let class = class_of(wanted);

    held(|| {
        if let Some(piece) = pop(class) {
            return piece;
        }

        carve(class)
    })
}

pub fn give_back(piece: *mut u8) {
    if piece.is_null() {
        return;
    }

    let class = unsafe { *(piece.sub(HEADER) as *const usize) };

    held(|| push(class, piece));
}

fn pages_for(size: usize) -> *mut u8 {
    #[cfg(feature = "linux-injected")]
    {
        crate::linux::user::map_writable(size)
    }
    #[cfg(feature = "xnu-core")]
    {
        crate::xnu_user_calls::map_writable(size)
    }
}

fn pop(class: usize) -> Option<*mut u8> {
    let free = unsafe { (&raw mut FREE).as_mut().unwrap() };

    let piece = free[class];
    if piece.is_null() {
        return None;
    }
    free[class] = unsafe { *(piece as *const *mut u8) };

    Some(piece)
}

fn push(class: usize, piece: *mut u8) {
    let free = unsafe { (&raw mut FREE).as_mut().unwrap() };

    unsafe { *(piece as *mut *mut u8) = free[class] };
    free[class] = piece;
}

fn carve(class: usize) -> *mut u8 {
    let wanted = HEADER + (SMALLEST << class);

    let mut edge = unsafe { EDGE };
    let mut left = unsafe { LEFT };
    if left < wanted {
        let asked = if wanted > CHUNK { wanted } else { CHUNK };
        edge = pages_for(asked) as usize;
        if edge == 0 {
            return core::ptr::null_mut();
        }
        left = asked;
    }

    unsafe {
        EDGE = edge + wanted;
        LEFT = left - wanted;

        *(edge as *mut usize) = class;
    }

    (edge + HEADER) as *mut u8
}

fn held<T>(work: impl FnOnce() -> T) -> T {
    while TAKEN.swap(true, Ordering::Acquire) {
        core::hint::spin_loop();
    }

    let answer = work();

    TAKEN.store(false, Ordering::Release);

    answer
}

fn wanted_for(size: usize) -> usize {
    if size < SMALLEST { SMALLEST } else { size }
}

fn class_of(size: usize) -> usize {
    let mut class = 0;
    while (SMALLEST << class) < size {
        class += 1;
    }

    class
}

static mut FREE: [*mut u8; CLASSES] = [core::ptr::null_mut(); CLASSES];
static mut EDGE: usize = 0;
static mut LEFT: usize = 0;
static TAKEN: AtomicBool = AtomicBool::new(false);

const HEADER: usize = 16;
const SMALLEST: usize = 16;
const CLASSES: usize = 32;
const CHUNK: usize = 4 * 1024 * 1024;
