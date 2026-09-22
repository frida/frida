
pub fn entry_offset() -> usize {
    (frida_xnu_user_entry as usize) - crate::own_range().0
}

pub extern "C" fn frida_xnu_user_entry(arena: usize) -> ! {
    let arena = arena as u64;
    unsafe { ARENA = arena };

    crate::xnu_user_calls::told_the_cache_shape(word_at(arena + crate::xnu_relay::CACHE_SHAPE));
    crate::xnu_user_calls::told_the_page_size(
        word_at(arena + crate::xnu_relay::PAGE_SIZE) as usize);
    crate::xnu::select_user();
    crate::gthread::use_thread_slots(&crate::xnu_user_calls::SLOTS);

    unsafe {
        crate::set_own_range(word_at(arena + crate::xnu_relay::IMAGE_BASE),
            word_at(arena + crate::xnu_relay::IMAGE_SIZE));
        crate::run_constructors();

        crate::init_gum_without_exceptor();
    }

    let has_run = unsafe { ((arena + crate::xnu_relay::HAS_RUN) as *const u32).read_volatile() };
    say_which_thread_this_is(arena);
    let served = has_run != 0 && become_a_thread_the_system_knows(arena);
    unsafe { ON_A_THREAD_THE_SYSTEM_KNOWS = served };

    unsafe { ((arena + crate::xnu_relay::AWAKE_AT) as *mut u64).write_volatile(AWAKE) };
    crate::xnu_bell::ring_the_bell();

    if !served {
        unsafe { user_worker(arena as *mut core::ffi::c_void, 0) };
    }

    wait_for_something_that_never_comes();
}

fn wait_for_something_that_never_comes() -> ! {
    if let Some(wait) = crate::xnu_libsystem::function_named(b"/libsystem_kernel.dylib",
        b"_mach_msg")
    {
        type Wait = unsafe extern "C" fn(*mut u8, i32, u32, u32, u32, u32, u32) -> i32;
        let wait: Wait = unsafe { core::mem::transmute(wait) };

        let listening_on = crate::xnu_user_calls::a_port_to_answer_on();
        let mut nothing = [0u8; 1024];
        loop {
            unsafe {
                wait(nothing.as_mut_ptr(), ONLY_LISTEN, 0, nothing.len() as u32, listening_on,
                    NO_TIMEOUT, NO_ONE_TO_TELL)
            };
        }
    }

    loop {
        crate::kernel::yield_now();
    }
}

const ONLY_LISTEN: i32 = 2;
const NO_TIMEOUT: u32 = 0;
const NO_ONE_TO_TELL: u32 = 0;

fn become_a_thread_the_system_knows(arena: u64) -> bool {
    let Some(making) = crate::xnu_libsystem::making_threads() else {
        return false;
    };
    let Some(out_of_a_bare_one) = making.out_of_a_bare_one else {
        return false;
    };

    let start = crate::xnu_libsystem::signed_to_begin_at(serve_from_a_proper_thread);
    let mut made = 0u64;

    unsafe {
        out_of_a_bare_one(&mut made, core::ptr::null(), start, arena as *mut core::ffi::c_void) == 0
    }
}

unsafe extern "C" fn serve_from_a_proper_thread(arena: *mut core::ffi::c_void)
    -> *mut core::ffi::c_void
{
    crate::xnu_unlisted::leave_the_list();
    unsafe { user_worker(arena, 0) };
    crate::xnu_unlisted::join_the_list_again();

    core::ptr::null_mut()
}

pub fn on_a_thread_the_system_knows() -> bool {
    unsafe { ON_A_THREAD_THE_SYSTEM_KNOWS }
}

static mut ON_A_THREAD_THE_SYSTEM_KNOWS: bool = false;

pub fn say_which_thread_this_is(arena: u64) {
    let number = crate::xnu_user_calls::this_thread_is();
    if number == 0 {
        return;
    }

    let said = (arena + crate::xnu_relay::OUR_THREADS) as *mut u64;
    for step in 0..crate::xnu_hiding::MOST_OF_OURS_IN_ONE {
        let at = unsafe { said.add(step) };
        let held = unsafe { at.read_volatile() };
        if held == number {
            return;
        }
        if held == 0 {
            unsafe { at.write_volatile(number) };
            crate::xnu_bell::ring_the_bell();
            return;
        }
    }
}

unsafe extern "C" fn user_worker(parameter: *mut core::ffi::c_void, _wait_result: i32) {
    let arena = parameter as u64;
    unsafe { ARENA = arena };
    say_which_thread_this_is(arena);

    let context = unsafe { crate::adopt_js_context() };
    unsafe { crate::route_frames_through(arena) };
    crate::glib::own_the_loop();
    crate::watch_for_work(context, copy_has_work, serve_the_copy);

    while word_at(arena + crate::xnu_relay::STOP_REQUEST) == 0 {
        unsafe { crate::dispatch_pending_work(context) };

    }

    unsafe { ((arena + crate::xnu_relay::WORKER_STOPPED) as *mut u32).write_volatile(1) };
}

fn copy_has_work() -> bool {
    let arena = unsafe { ARENA };

    crate::xnu_relay::holds_a_frame_from_host(arena)
        || word_at(arena + crate::xnu_relay::SPAWN_WANTED) != crate::xnu_relay::NOTHING_WANTED
        || word_at(arena + crate::xnu_relay::APPS_WANTED) != crate::xnu_relay::NOTHING_WANTED

        || word_at(arena + crate::xnu_relay::STOP_REQUEST) != 0
}

fn serve_the_copy() {
    let arena = unsafe { ARENA };

    while let Some(frame) = crate::xnu_relay::take_frame_from_host(arena) {
        crate::on_frame_from_host(&frame);
    }

    if word_at(arena + crate::xnu_relay::SPAWN_WANTED) != crate::xnu_relay::NOTHING_WANTED {
        start_what_the_other_half_asked_for(arena);
        crate::xnu_bell::ring_the_bell();
    }

    if word_at(arena + crate::xnu_relay::APPS_WANTED) != crate::xnu_relay::NOTHING_WANTED {
        crate::xnu_applications::say_what_is_installed(arena);
        crate::xnu_bell::ring_the_bell();
    }
}

fn start_what_the_other_half_asked_for(arena: u64) {
    let mut said = [core::ptr::null(); MOST_WORDS];
    let mut how_many = 0;
    let mut at = arena + crate::xnu_relay::SPAWN_WORDS;
    let past = at + crate::xnu_relay::SPAWN_WORDS_ROOM;
    while at < past && unsafe { (at as *const u8).read_volatile() } != 0
        && how_many < MOST_WORDS - 1
    {
        said[how_many] = at as *const u8;
        how_many += 1;
        while at < past && unsafe { (at as *const u8).read_volatile() } != 0 {
            at += 1;
        }
        at += 1;
    }

    if how_many == 0 {
        answer_what_was_started(arena, 0);
        return;
    }

    let asked_for = unsafe {
        core::slice::from_raw_parts(said[0], length_of(said[0]))
    };
    if !asked_for.contains(&b'/') {
        started_by_the_system(arena, asked_for);
        return;
    }

    answer_what_was_started(arena, start_a_program(said[0], &said));
}

fn started_by_the_system(arena: u64, identifier: &[u8]) {
    let mut runs = [0u8; MOST_A_NAME_IS];
    if !crate::xnu_applications::start_the_one_called(identifier, &mut runs) {
        answer_what_was_started(arena, 0);
        return;
    }

    let at = arena + crate::xnu_relay::SPAWN_WORDS;
    for (step, byte) in runs.iter().enumerate() {
        unsafe { ((at + step as u64) as *mut u8).write_volatile(*byte) };
    }

    unsafe {
        ((arena + crate::xnu_relay::SPAWN_ID) as *mut u64).write_volatile(0);
        ((arena + crate::xnu_relay::SPAWN_ANSWER) as *mut u64)
            .write_volatile(crate::xnu_relay::THE_SYSTEM_IS_STARTING_IT);
    }
}

fn answer_what_was_started(arena: u64, id: u32) {
    unsafe {
        ((arena + crate::xnu_relay::SPAWN_ID) as *mut u64).write_volatile(id as u64);
        ((arena + crate::xnu_relay::SPAWN_ANSWER) as *mut u64).write_volatile(
            if id != 0 { crate::xnu_relay::DONE } else { crate::xnu_relay::REFUSED });
    }
}

fn length_of(said: *const u8) -> usize {
    let mut length = 0;
    while unsafe { said.add(length).read_volatile() } != 0 {
        length += 1;
    }

    length
}

const MOST_A_NAME_IS: usize = 256;

fn start_a_program(program: *const u8, words: &[*const u8]) -> u32 {
    let Some(start) = crate::xnu_libsystem::function_named(b"/libsystem_kernel.dylib",
        b"_posix_spawn")
    else {
        return 0;
    };

    type Start = unsafe extern "C" fn(*mut i32, *const u8, *const core::ffi::c_void,
        *const core::ffi::c_void, *const *const u8, *const *const u8) -> i32;
    let start: Start = unsafe { core::mem::transmute(start) };

    let mut id = 0i32;
    let went = unsafe {
        start(&mut id, program, core::ptr::null(), core::ptr::null(), words.as_ptr(),
            what_this_process_was_given())
    };

    if went == 0 { id as u32 } else { 0 }
}

fn what_this_process_was_given() -> *const *const u8 {
    let Some(held) = crate::xnu_libsystem::function_named(b"/libsystem_c.dylib", b"_environ") else {
        return core::ptr::null();
    };

    unsafe { (held as *const *const *const u8).read() }
}

const MOST_WORDS: usize = 32;

pub fn ask_for_protection(address: u64, size: usize, may: u32) -> bool {
    ask_the_other_half(&[(crate::xnu_relay::PROTECT_SIZE, size as u64),
        (crate::xnu_relay::PROTECT_TO, may as u64)],
        crate::xnu_relay::PROTECT_WANTED, address, crate::xnu_relay::PROTECT_ANSWER)
}

fn ask_the_other_half(along_with: &[(u64, u64)], asked_at: u64, asking: u64, answer_at: u64)
    -> bool
{
    let arena = unsafe { ARENA };
    if arena == 0 {
        return false;
    }

    let put = |at: u64, value: u64| unsafe { ((arena + at) as *mut u64).write_volatile(value) };
    put(answer_at, crate::xnu_relay::NOTHING_WANTED);
    for (at, value) in along_with {
        put(*at, *value);
    }
    put(asked_at, asking);

    crate::xnu_bell::ring_the_bell();

    let answered = &mut || unsafe {
        ((arena + answer_at) as *const u64).read_volatile()
    } != crate::xnu_relay::NOTHING_WANTED;
    while !answered() {
        crate::kernel::wait(crate::glib::wakeup_token(), None, answered);
    }

    unsafe { ((arena + answer_at) as *const u64).read_volatile() == crate::xnu_relay::DONE }
}

pub fn arena() -> u64 {
    unsafe { ARENA }
}

fn word_at(at: u64) -> u64 {
    unsafe { (at as *const u64).read_volatile() }
}

static mut ARENA: u64 = 0;

pub const AWAKE: u64 = 0x6672_6964_6100_0001;
