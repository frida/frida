
pub fn function_named(library: &[u8], wanted: &[u8]) -> Option<u64> {
    let image = image_named(library)?;

    function_in(image, wanted)
}

pub fn making_threads() -> Option<&'static MakingThreads> {
    let held = unsafe { (&raw const MAKING).as_ref().unwrap() };
    if held.is_some() {
        return held.as_ref();
    }

    let pthread = image_named(b"/libsystem_pthread.dylib")?;
    let found = MakingThreads {
        out_of_a_bare_one: function_in(pthread, b"_pthread_create_from_mach_thread")
            .map(|at| unsafe { core::mem::transmute(at) }),
        the_usual_way: unsafe { core::mem::transmute(function_in(pthread, b"_pthread_create")?) },
    };

    unsafe { MAKING = Some(found) };
    unsafe { (&raw const MAKING).as_ref().unwrap().as_ref() }
}

pub type StartAThread = unsafe extern "C" fn(*mut core::ffi::c_void) -> *mut core::ffi::c_void;
pub type MakeAThread = unsafe extern "C" fn(*mut u64, *const u8, StartAThread,
    *mut core::ffi::c_void) -> i32;

pub struct MakingThreads {
    pub out_of_a_bare_one: Option<MakeAThread>,
    pub the_usual_way: MakeAThread,
}

static mut MAKING: Option<MakingThreads> = None;

pub fn signed_to_begin_at(start: StartAThread) -> StartAThread {
    unsafe { core::mem::transmute(crate::pac::ptrauth_sign(start as *const u8, NOTHING_TO_TIE_IT_TO)) }
}

const NOTHING_TO_TIE_IT_TO: usize = 0;

pub fn asking_about_threads() -> Option<&'static AskingAboutThreads> {
    let held = unsafe { (&raw const ASKING).as_ref().unwrap() };
    if held.is_some() {
        return held.as_ref();
    }

    let kernel = image_named(b"/libsystem_kernel.dylib")?;
    let found = AskingAboutThreads {
        this_task: function_in(kernel, b"_mach_task_self_")?,
        which_ones: unsafe { core::mem::transmute(function_in(kernel, b"_task_threads")?) },
        what_it_is_doing: unsafe { core::mem::transmute(function_in(kernel, b"_thread_get_state")?) },
        set_what_it_does: unsafe { core::mem::transmute(function_in(kernel, b"_thread_set_state")?) },
        hold_it_still: unsafe { core::mem::transmute(function_in(kernel, b"_thread_suspend")?) },
        let_it_go: unsafe { core::mem::transmute(function_in(kernel, b"_thread_resume")?) },
        give_back: unsafe { core::mem::transmute(function_in(kernel, b"_vm_deallocate")?) },
        which_one_is_this: unsafe { core::mem::transmute(function_in(kernel, b"_mach_thread_self")?) },
    };

    unsafe { ASKING = Some(found) };
    unsafe { (&raw const ASKING).as_ref().unwrap().as_ref() }
}

pub struct AskingAboutThreads {
    pub this_task: u64,
    pub which_ones: unsafe extern "C" fn(u32, *mut *mut u32, *mut u32) -> i32,
    pub what_it_is_doing: unsafe extern "C" fn(u32, i32, *mut u32, *mut u32) -> i32,
    pub set_what_it_does: unsafe extern "C" fn(u32, i32, *const u32, u32) -> i32,
    pub hold_it_still: unsafe extern "C" fn(u32) -> i32,
    pub let_it_go: unsafe extern "C" fn(u32) -> i32,
    pub give_back: unsafe extern "C" fn(u32, u64, u64) -> i32,
    pub which_one_is_this: unsafe extern "C" fn() -> u32,
}

static mut ASKING: Option<AskingAboutThreads> = None;

pub fn image_named(wanted: &[u8]) -> Option<u64> {
    let cache = crate::xnu_user_calls::where_the_shared_code_is()?;
    let read_word = |at: u64| unsafe { ((cache + at) as *const u32).read_volatile() };
    let read_long = |at: u64| unsafe { ((cache + at) as *const u64).read_volatile() };

    let moved_by = cache.wrapping_sub(read_long(read_word(SPANS_ARE_AT) as u64));

    let images_at = read_word(IMAGES_ARE_AT) as u64;
    let images = read_word(HOW_MANY_IMAGES);
    for image in 0..images as u64 {
        let at = images_at + (image * AN_IMAGE);
        let name_at = read_word(at + WHAT_IT_IS_CALLED) as u64;
        if !name_here(cache + name_at, wanted) {
            continue;
        }

        return Some(read_long(at).wrapping_add(moved_by));
    }

    None
}

pub fn function_in(image: u64, wanted: &[u8]) -> Option<u64> {
    let (trie, size) = what_the_image_offers(image)?;

    let mut at = trie;
    let mut spent = 0;
    for _ in 0..wanted.len() + 1 {
        if at >= trie + size {
            return None;
        }

        let (said, after) = a_number_at(at);
        if said != 0 && spent == wanted.len() {
            let (_, past_flags) = a_number_at(after);
            let (offset, _) = a_number_at(past_flags);
            return Some(image + offset);
        }

        let children = after + said;
        let (count, mut step) = (byte_at(children), children + 1);
        let mut went = None;
        for _ in 0..count {
            let began = step;
            let mut length = 0;
            while length < LONGEST_NAME as u64 && byte_at(began + length) != 0 {
                length += 1;
            }

            let matches = spent + length as usize <= wanted.len()
                && (0..length).all(|index| byte_at(began + index) == wanted[spent + index as usize]);

            let (next, after_next) = a_number_at(began + length + 1);
            if matches && went.is_none() {
                went = Some((trie + next, spent + length as usize));
            }
            step = after_next;
        }

        let (there, now_spent) = went?;
        at = there;
        spent = now_spent;
    }

    None
}

fn what_the_image_offers(image: u64) -> Option<(u64, u64)> {
    let commands = read_word(image + HOW_MANY_COMMANDS);
    let mut at = image + PAST_THE_HEADER;
    let mut linkedit = None;
    let mut offers = None;

    for _ in 0..commands {
        let kind = read_word(at);
        let length = read_word(at + 4) as u64;

        if kind == A_SEGMENT && name_here(at + 8, b"__LINKEDIT") {
            linkedit = Some((read_long(at + 24), read_long(at + 40)));
        }
        if kind == WHAT_IT_OFFERS {
            offers = Some((read_word(at + 8) as u64, read_word(at + 12) as u64));
        }

        at += length;
    }

    let ((where_it_wants_to_be, where_it_is_in_the_file), (offered_at, size)) =
        (linkedit?, offers?);

    Some((where_it_wants_to_be + slide_of() + (offered_at - where_it_is_in_the_file), size))
}

fn slide_of() -> u64 {
    let cache = crate::xnu_user_calls::where_the_shared_code_is().unwrap_or(0);

    cache.wrapping_sub(read_long(cache + read_word(cache + SPANS_ARE_AT) as u64))
}

fn read_word(at: u64) -> u32 {
    unsafe { (at as *const u32).read_volatile() }
}

fn read_long(at: u64) -> u64 {
    unsafe { (at as *const u64).read_volatile() }
}

fn byte_at(at: u64) -> u8 {
    unsafe { (at as *const u8).read_volatile() }
}

fn a_number_at(at: u64) -> (u64, u64) {
    let mut said = 0u64;
    let mut step = at;
    for shift in (0..MOST_BYTES_TO_A_NUMBER * 7).step_by(7) {
        let byte = byte_at(step);
        step += 1;
        said |= ((byte & 0x7f) as u64) << shift;
        if (byte & 0x80) == 0 {
            break;
        }
    }

    (said, step)
}

const MOST_BYTES_TO_A_NUMBER: usize = 10;

const HOW_MANY_COMMANDS: u64 = 16;
const PAST_THE_HEADER: u64 = 32;
const A_SEGMENT: u32 = 0x19;
const WHAT_IT_OFFERS: u32 = 0x8000_0033;

fn name_here(at: u64, wanted: &[u8]) -> bool {
    let mut length = 0;
    while length < LONGEST_NAME {
        if unsafe { ((at as *const u8).add(length)).read_volatile() } == 0 {
            break;
        }
        length += 1;
    }
    if length < wanted.len() {
        return false;
    }

    let from = length - wanted.len();
    for (step, byte) in wanted.iter().enumerate() {
        if unsafe { ((at as *const u8).add(from + step)).read_volatile() } != *byte {
            return false;
        }
    }

    true
}

const SPANS_ARE_AT: u64 = 0x10;
const IMAGES_ARE_AT: u64 = 0x1c0;
const HOW_MANY_IMAGES: u64 = 0x1c4;
const AN_IMAGE: u64 = 32;
const WHAT_IT_IS_CALLED: u64 = 24;
const LONGEST_NAME: usize = 256;
