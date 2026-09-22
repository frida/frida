use alloc::collections::{BTreeMap, VecDeque};
use alloc::vec::Vec;

use crate::ring::{Ring, Taken};

use super::arena::Arena;
use super::facade::{wait, yield_now};

// The kernel half's side: a frame for a process the host attached to goes to the copy living
// there, and what that copy answers goes on to the host.
pub fn forward_frame(arena: u64, frame: &[u8]) -> bool {
    unsafe { waiting() }.entry(arena).or_default().push_back(frame.to_vec());
    send_what_waits(arena);

    true
}

pub fn serve_waiting_frames() {
    let arenas: Vec<u64> = unsafe { waiting() }.keys().copied().collect();
    for arena in arenas {
        send_what_waits(arena);
    }
}

pub fn take_frame_from_target(arena: u64) -> Option<Vec<u8>> {
    read_frame(&TO_KERNEL, arena, wake_the_copy)
}

pub fn injected_arenas() -> Vec<u64> {
    super::injection::arenas()
}

pub fn holds_a_frame_from_target(arena: u64) -> bool {
    TO_KERNEL.holds_anything(arena)
}

pub fn a_frame_waits_for_room() -> bool {
    unsafe { waiting() }
        .iter()
        .any(|(arena, frames)| !frames.is_empty() && TO_COPY.has_room(*arena))
}

// The copy's side, where the same two rings are reached at an address of its own.
pub fn publish_frame_to_host(arena: u64, frame: &[u8]) -> bool {
    write_frame(&TO_KERNEL, arena, frame, wake_the_kernel_half)
}

pub fn take_frame_from_host(arena: u64) -> Option<Vec<u8>> {
    read_frame(&TO_COPY, arena, wake_the_kernel_half)
}

pub fn holds_a_frame_from_host(arena: u64) -> bool {
    TO_COPY.holds_anything(arena)
}

fn send_what_waits(arena: u64) {
    let frames = unsafe { waiting() }.get_mut(&arena).unwrap();

    while let Some(frame) = frames.front_mut() {
        TO_COPY.take_lock(arena, yield_now);
        let piece = TO_COPY.write(arena, arena + TO_COPY.buffer, frame, 0);
        if piece.is_none() {
            TO_COPY.ask_for_room(arena);
        }
        TO_COPY.let_lock_go(arena);

        let Some(written) = piece else {
            return;
        };
        publish_fence();
        wake_the_copy(arena);

        if written == frame.len() {
            frames.pop_front();
        } else {
            frame.drain(..written);
        }
    }
}

fn write_frame(ring: &Ring, arena: u64, frame: &[u8], tell: fn(u64)) -> bool {
    ring.take_lock(arena, yield_now);

    let mut written = 0;
    while written != frame.len() {
        match ring.write(arena, arena + ring.buffer, frame, written) {
            Some(now) => {
                written = now;
                publish_fence();
                tell(arena);
            }
            None => {
                ring.ask_for_room(arena);
                wait(crate::glib::wakeup_token(), None, &mut || ring.has_room(arena));
            }
        }
    }

    ring.let_lock_go(arena);

    true
}

fn read_frame(ring: &Ring, arena: u64, tell: fn(u64)) -> Option<Vec<u8>> {
    let hold = unsafe { holds() }.entry((arena, ring.head)).or_default();

    loop {
        match ring.read(arena, arena + ring.buffer, hold) {
            Taken::Nothing => return None,
            Taken::Piece => {
                if ring.room_is_wanted(arena) {
                    tell(arena);
                }
            }
            Taken::Frame => {
                let frame = core::mem::take(hold);
                if ring.room_is_wanted(arena) {
                    tell(arena);
                }
                return Some(frame);
            }
        }
    }
}

pub fn forget(arena: u64) {
    unsafe { holds() }.retain(|(of, _), _| *of != arena);
    unsafe { waiting() }.remove(&arena);
}

#[cfg(target_arch = "aarch64")]
fn publish_fence() {
    unsafe { core::arch::asm!("dsb ish", options(nostack, preserves_flags)) };
}

#[cfg(not(target_arch = "aarch64"))]
fn publish_fence() {
    core::sync::atomic::fence(core::sync::atomic::Ordering::SeqCst);
}

fn wake_the_copy(arena: u64) {
    super::injection::tell_the_copy_at(arena);
}

fn wake_the_kernel_half(arena: u64) {
    Arena::at(arena as usize).tell_the_kernel_half();
}

unsafe fn holds() -> &'static mut BTreeMap<(u64, u64), Vec<u8>> {
    unsafe { (&raw mut HOLDS).as_mut().unwrap() }
}

static mut HOLDS: BTreeMap<(u64, u64), Vec<u8>> = BTreeMap::new();

unsafe fn waiting() -> &'static mut BTreeMap<u64, VecDeque<Vec<u8>>> {
    unsafe { (&raw mut WAITING).as_mut().unwrap() }
}

static mut WAITING: BTreeMap<u64, VecDeque<Vec<u8>>> = BTreeMap::new();

const TO_COPY: Ring = Ring {
    buffer: 0x1000,
    head: 256,
    tail: 260,
    lock: 264,
    room: 268,
    size: FRAME_BUFFER_SIZE,
};

const TO_KERNEL: Ring = Ring {
    buffer: 0x1000 + FRAME_BUFFER_SIZE as u64,
    head: 272,
    tail: 276,
    lock: 280,
    room: 284,
    size: FRAME_BUFFER_SIZE,
};

const FRAME_BUFFER_SIZE: usize = 0x10000;
