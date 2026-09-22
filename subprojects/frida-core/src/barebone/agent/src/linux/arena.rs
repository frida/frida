use core::sync::atomic::{AtomicU32, Ordering};

// What the kernel half and the copy leave for each other. Both reach it by its own address, so
// each side is told only where it begins.
#[derive(Clone, Copy)]
pub struct Arena {
    begins: usize,
}

impl Arena {
    pub fn at(begins: usize) -> Self {
        Arena { begins }
    }

    pub fn at_offset(&self, offset: usize) -> usize {
        self.begins + offset
    }

    // Until the frames flow, what the copy has to say is left here for the kernel half to read.
    pub fn say(&self, msg: &str) {
        let said = msg.as_bytes();
        let length = said.iter().position(|byte| *byte == 0).unwrap_or(said.len());
        let kept = length.min(SAID_SIZE - 1);

        unsafe {
            core::ptr::copy_nonoverlapping(said.as_ptr(), (self.begins + SAID) as *mut u8, kept);
            ((self.begins + SAID + kept) as *mut u8).write(0);
        }
    }

    pub fn said(&self) -> &str {
        let bytes =
            unsafe { core::slice::from_raw_parts((self.begins + SAID) as *const u8, SAID_SIZE) };
        let length = bytes.iter().position(|byte| *byte == 0).unwrap_or(SAID_SIZE);

        core::str::from_utf8(&bytes[..length]).unwrap_or("")
    }

    pub fn page_size(&self) -> u32 {
        self.word(PAGE_SIZE).load(Ordering::Acquire)
    }

    pub fn leave(&self, offset: usize, value: u64) {
        unsafe { ((self.begins + offset) as *mut u64).write_volatile(value) };
    }

    pub fn progress(&self) -> u32 {
        self.word(PROGRESS).load(Ordering::Acquire)
    }

    pub fn note(&self, step: u32) {
        self.word(PROGRESS).store(step, Ordering::Release);
    }

    pub fn home(&self) -> u32 {
        self.word(HOME).load(Ordering::Acquire)
    }

    pub fn report_home(&self) {
        self.word(REPORTED).store(self.word(HOME).load(Ordering::Acquire), Ordering::Release);
    }

    pub fn reachable_at(&self, says: u32, hears: u32) {
        self.word(SAYS).store(says, Ordering::Release);
        self.word(HEARS).store(hears, Ordering::Release);
    }

    pub fn note_a_thread(&self, thread: u32, is_gone: bool) {
        let head = self.word(THREADS_HEAD).load(Ordering::Relaxed);
        let tail = self.word(THREADS_TAIL).load(Ordering::Acquire);
        if head.wrapping_sub(tail) == THREAD_SLOTS {
            return;
        }

        let slot = (head % THREAD_SLOTS) as usize;
        self.word(THREADS + (slot * 4)).store((thread << 1) | is_gone as u32, Ordering::Relaxed);
        self.word(THREADS_HEAD).store(head.wrapping_add(1), Ordering::Release);
    }

    pub fn take_a_thread(&self) -> Option<(u32, bool)> {
        let head = self.word(THREADS_HEAD).load(Ordering::Acquire);
        let tail = self.word(THREADS_TAIL).load(Ordering::Relaxed);
        if head == tail {
            return None;
        }

        let slot = (tail % THREAD_SLOTS) as usize;
        let noted = self.word(THREADS + (slot * 4)).load(Ordering::Relaxed);
        self.word(THREADS_TAIL).store(tail.wrapping_add(1), Ordering::Release);

        Some((noted >> 1, noted & 1 != 0))
    }

    pub fn holds_a_thread(&self) -> bool {
        self.word(THREADS_HEAD).load(Ordering::Acquire)
            != self.word(THREADS_TAIL).load(Ordering::Relaxed)
    }

    pub fn tell_it_to_go(&self) {
        self.word(GO).store(1, Ordering::Release);
    }

    pub fn was_told_to_go(&self) -> bool {
        self.word(GO).load(Ordering::Acquire) != 0
    }

    pub fn gone(&self) {
        self.word(GONE).store(1, Ordering::Release);
    }

    pub fn has_gone(&self) -> bool {
        self.word(GONE).load(Ordering::Acquire) != 0
    }

    pub fn says_through(&self) -> u32 {
        self.word(SAYS).load(Ordering::Acquire)
    }

    pub fn hears_through(&self) -> u32 {
        self.word(HEARS).load(Ordering::Acquire)
    }

    pub fn tell_the_kernel_half(&self) {
        self.word(TO_KERNEL).fetch_add(1, Ordering::AcqRel);
        super::user::say_something();
    }

    pub fn request_executable(&self, address: u64, size: u64) -> u32 {
        self.leave(EXEC_ADDR, address);
        self.leave(EXEC_SIZE, size);
        let seq = self.word(EXEC_SEQ).load(Ordering::Relaxed).wrapping_add(1).max(1);
        self.word(EXEC_SEQ).store(seq, Ordering::Release);
        seq
    }

    pub fn executable_settled(&self, seq: u32) -> bool {
        self.word(EXEC_ACK).load(Ordering::Acquire) == seq
    }

    pub fn executable_was_granted(&self) -> bool {
        self.word(EXEC_OK).load(Ordering::Acquire) != 0
    }

    pub fn wants_executable(&self) -> Option<(u64, u64)> {
        let seq = self.word(EXEC_SEQ).load(Ordering::Acquire);
        if seq == 0 || seq == self.word(EXEC_ACK).load(Ordering::Acquire) {
            return None;
        }
        Some((self.read_wide(EXEC_ADDR), self.read_wide(EXEC_SIZE)))
    }

    pub fn grant_executable(&self, ok: bool) {
        self.word(EXEC_OK).store(ok as u32, Ordering::Release);
        let seq = self.word(EXEC_SEQ).load(Ordering::Acquire);
        self.word(EXEC_ACK).store(seq, Ordering::Release);
    }

    fn read_wide(&self, offset: usize) -> u64 {
        unsafe { ((self.begins + offset) as *const u64).read_volatile() }
    }

    fn word(&self, offset: usize) -> &AtomicU32 {
        unsafe { &*((self.begins + offset) as *const AtomicU32) }
    }
}

pub const PROGRESS: usize = 20;
pub const PAGE_SIZE: usize = 48;
pub const FAULT_KIND: usize = 24;
pub const FAULT_ADDRESS: usize = 32;
pub const FAULT_PC: usize = 40;
pub const FAULT_LR: usize = 56;
pub const SAID: usize = 64;
pub const GO: usize = 288;
pub const GONE: usize = 292;
const THREADS_HEAD: usize = 296;
const THREADS_TAIL: usize = 300;
const THREADS: usize = 304;
const THREAD_SLOTS: u32 = 32;
pub const SAID_SIZE: usize = 192;


pub const REPORTED: usize = 0;
pub const HOME: usize = 4;
pub const SAYS: usize = 8;
pub const HEARS: usize = 52;
pub const TO_KERNEL: usize = 12;
pub const WOKEN: usize = 16;

const EXEC_SEQ: usize = 432;
const EXEC_ACK: usize = 436;
const EXEC_ADDR: usize = 440;
const EXEC_SIZE: usize = 448;
const EXEC_OK: usize = 456;
