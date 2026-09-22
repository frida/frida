
use alloc::collections::{BTreeMap, VecDeque};
use alloc::vec::Vec;

use crate::ring::{Ring, Taken};

pub fn forward_frame(arena: u64, frame: &[u8]) -> bool {
    unsafe { waiting() }.entry(arena).or_default().push_back(Going { frame: frame.to_vec(),
        written: 0 });
    send_what_waits(arena);

    true
}

struct Going {
    frame: Vec<u8>,
    written: usize,
}

pub fn serve_waiting_frames() {
    let arenas: Vec<u64> = unsafe { waiting() }.keys().copied().collect();
    for arena in arenas {
        send_what_waits(arena);
    }
}

pub fn take_frame_from_target(arena: u64) -> Option<Vec<u8>> {
    let frame = read_frame(&TO_KERNEL, arena);
    if frame.is_some() {
        crate::xnu_injection::wake_the_copy_at(arena);
    }

    frame
}

pub fn holds_a_frame_from_target(arena: u64) -> bool {
    TO_KERNEL.holds_anything(arena)
}

pub fn a_frame_waits_for_room() -> bool {
    unsafe { waiting() }
        .iter()
        .any(|(arena, frames)| !frames.is_empty() && TO_COPY.has_room(*arena))
}

pub fn publish_frame_to_host(arena: u64, frame: &[u8]) -> bool {
    write_frame(&TO_KERNEL, arena, frame, tell_the_kernel_half)
}

pub fn take_frame_from_host(arena: u64) -> Option<Vec<u8>> {
    let frame = read_frame(&TO_COPY, arena);
    if frame.is_some() {
        tell_the_kernel_half(arena);
    }

    frame
}

pub fn holds_a_frame_from_host(arena: u64) -> bool {
    TO_COPY.holds_anything(arena)
}

pub fn forget(arena: u64) {
    unsafe { holds() }.retain(|(of, _), _| *of != arena);
    unsafe { waiting() }.remove(&arena);
}

fn send_what_waits(arena: u64) {
    let frames = unsafe { waiting() }.get_mut(&arena).unwrap();

    while let Some(going) = frames.front_mut() {
        if !write_what_fits(&TO_COPY, arena, going) {
            crate::xnu_injection::wake_the_copy_at(arena);
            TO_COPY.ask_for_room(arena);
            return;
        }
        frames.pop_front();
        crate::xnu_injection::wake_the_copy_at(arena);
    }
}

fn write_what_fits(ring: &Ring, arena: u64, going: &mut Going) -> bool {
    ring.take_lock(arena, rest);

    while going.written < going.frame.len() {
        match ring.write(arena, arena + ring.buffer, &going.frame, going.written) {
            Some(now) => going.written = now,
            None => break,
        }
    }

    ring.let_lock_go(arena);

    going.written == going.frame.len()
}

fn write_frame(ring: &Ring, arena: u64, frame: &[u8], tell: fn(u64)) -> bool {
    ring.take_lock(arena, rest);

    let mut written = 0;
    while written < frame.len() {
        match ring.write(arena, arena + ring.buffer, frame, written) {
            Some(now) => {
                written = now;
                tell(arena);
            }
            None => {
                ring.ask_for_room(arena);
                tell(arena);
                crate::kernel::wait(crate::glib::wakeup_token(), None,
                    &mut || ring.has_room(arena));
            }
        }
    }

    ring.let_lock_go(arena);

    true
}

fn tell_the_kernel_half(_arena: u64) {
    crate::xnu_bell::ring_the_bell();
}

fn read_frame(ring: &Ring, arena: u64) -> Option<Vec<u8>> {
    let mut frame = unsafe { holds() }.remove(&(arena, ring.buffer)).unwrap_or_default();

    loop {
        match ring.read(arena, arena + ring.buffer, &mut frame) {
            Taken::Nothing => {
                if !frame.is_empty() {
                    unsafe { holds() }.insert((arena, ring.buffer), frame);
                }
                return None;
            }
            Taken::Piece => {}
            Taken::Frame => return Some(frame),
        }
    }
}

fn rest() {
    core::hint::spin_loop();
}

unsafe fn holds() -> &'static mut BTreeMap<(u64, u64), Vec<u8>> {
    unsafe { (&raw mut HOLDS).as_mut().unwrap() }
}

static mut HOLDS: BTreeMap<(u64, u64), Vec<u8>> = BTreeMap::new();

unsafe fn waiting() -> &'static mut BTreeMap<u64, VecDeque<Going>> {
    unsafe { (&raw mut WAITING).as_mut().unwrap() }
}

static mut WAITING: BTreeMap<u64, VecDeque<Going>> = BTreeMap::new();

pub const ARENA_SIZE: u64 = 0x1000 + (2 * FRAME_BUFFER_SIZE as u64) + SAYING_ROOM + TAKEN_ROOM;

pub const WHAT_WE_TOOK: u64 = 0x1000 + (2 * FRAME_BUFFER_SIZE as u64) + SAYING_ROOM;
pub const TAKEN_ROOM: u64 = 0x1000;
pub const MOST_WE_TAKE: usize = 255;

pub const SAYING: u64 = 0x1000 + (2 * FRAME_BUFFER_SIZE as u64);
pub const SAYING_ROOM: u64 = 0x10000;

pub const AWAKE_AT: u64 = 0;
pub const IMAGE_BASE: u64 = 8;
pub const IMAGE_SIZE: u64 = 16;
pub const STOP_REQUEST: u64 = 24;
pub const WORKER_STOPPED: u64 = 28;
pub const HAS_RUN: u64 = 36;

pub const PAGE_SIZE: u64 = 40;
pub const CACHE_SHAPE: u64 = 48;


pub const PROTECT_WANTED: u64 = 544;
pub const PROTECT_SIZE: u64 = 552;
pub const PROTECT_TO: u64 = 560;
pub const PROTECT_ANSWER: u64 = 568;

pub const SPAWN_WANTED: u64 = 576;
pub const SPAWN_ANSWER: u64 = 584;
pub const SPAWN_ID: u64 = 592;
pub const WAKE_WORD: u64 = 616;

pub const BELL_WORD: u64 = 624;

pub const OUR_THREADS: u64 = 656;

pub const APPS_WANTED: u64 = 600;
pub const APPS_ANSWER: u64 = 608;

pub const SPAWN_WORDS: u64 = 0x800;
pub const SPAWN_WORDS_ROOM: u64 = 0x800;

pub const NOTHING_WANTED: u64 = 0;
pub const DONE: u64 = 1;
pub const REFUSED: u64 = 2;

pub const THE_SYSTEM_IS_STARTING_IT: u64 = 3;

pub const TO_COPY: Ring = Ring {
    buffer: 0x1000,
    head: 256,
    tail: 260,
    lock: 264,
    room: 268,
    size: FRAME_BUFFER_SIZE,
};

pub const TO_KERNEL: Ring = Ring {
    buffer: 0x1000 + FRAME_BUFFER_SIZE as u64,
    head: 272,
    tail: 276,
    lock: 280,
    room: 284,
    size: FRAME_BUFFER_SIZE,
};

const FRAME_BUFFER_SIZE: usize = 0x10000;
