use core::sync::atomic::{AtomicU32, Ordering};

pub struct Ring {
    pub buffer: u64,
    pub head: u64,
    pub tail: u64,
    pub lock: u64,
    pub room: u64,
    pub size: usize,
}

pub enum Taken {
    Nothing,
    Piece,
    Frame,
}

impl Ring {
    pub fn write(&self, arena: u64, buffer: u64, frame: &[u8], from: usize) -> Option<usize> {
        let head = self.index(arena, self.head).load(Ordering::Relaxed);
        let tail = self.index(arena, self.tail).load(Ordering::Acquire);

        let free = self.size - head.wrapping_sub(tail) as usize;
        if free <= HEADER {
            return None;
        }

        let piece = core::cmp::min(frame.len() - from, free - HEADER);
        let more = if from + piece == frame.len() { 0 } else { MORE };

        self.put(buffer, head, &(piece as u32 | more).to_le_bytes());
        self.put(buffer, head.wrapping_add(HEADER as u32), &frame[from..from + piece]);
        self.index(arena, self.head)
            .store(head.wrapping_add((HEADER + piece) as u32), Ordering::Release);

        Some(from + piece)
    }

    pub fn read(&self, arena: u64, buffer: u64, into: &mut alloc::vec::Vec<u8>) -> Taken {
        let head = self.index(arena, self.head).load(Ordering::Acquire);
        let tail = self.index(arena, self.tail).load(Ordering::Relaxed);
        if head == tail {
            return Taken::Nothing;
        }

        let mut header = [0u8; HEADER];
        self.get(buffer, tail, &mut header);
        let header = u32::from_le_bytes(header);
        let piece = (header & !MORE) as usize;

        let already = into.len();
        into.resize(already + piece, 0);
        self.get(buffer, tail.wrapping_add(HEADER as u32), &mut into[already..]);
        self.index(arena, self.tail)
            .store(tail.wrapping_add((HEADER + piece) as u32), Ordering::Release);

        if header & MORE == 0 {
            Taken::Frame
        } else {
            Taken::Piece
        }
    }

    pub fn take_lock(&self, arena: u64, rest: fn()) {
        while self.index(arena, self.lock)
            .compare_exchange(0, 1, Ordering::AcqRel, Ordering::Acquire)
            .is_err()
        {
            rest();
        }
    }

    pub fn let_lock_go(&self, arena: u64) {
        self.index(arena, self.lock).store(0, Ordering::Release);
    }

    pub fn ask_for_room(&self, arena: u64) {
        self.index(arena, self.room).store(1, Ordering::Release);
    }

    pub fn room_is_wanted(&self, arena: u64) -> bool {
        self.index(arena, self.room).swap(0, Ordering::AcqRel) != 0
    }

    pub fn has_room(&self, arena: u64) -> bool {
        let head = self.index(arena, self.head).load(Ordering::Relaxed);
        let tail = self.index(arena, self.tail).load(Ordering::Acquire);

        self.size - head.wrapping_sub(tail) as usize > HEADER
    }

    pub fn holds_anything(&self, arena: u64) -> bool {
        self.index(arena, self.head).load(Ordering::Acquire)
            != self.index(arena, self.tail).load(Ordering::Relaxed)
    }

    fn put(&self, buffer: u64, at: u32, bytes: &[u8]) {
        let at = at as usize & (self.size - 1);
        let first = core::cmp::min(bytes.len(), self.size - at);
        unsafe {
            core::ptr::copy_nonoverlapping(bytes.as_ptr(), (buffer + at as u64) as *mut u8, first);
            core::ptr::copy_nonoverlapping(bytes.as_ptr().add(first), buffer as *mut u8,
                bytes.len() - first);
        }
    }

    fn get(&self, buffer: u64, at: u32, bytes: &mut [u8]) {
        let at = at as usize & (self.size - 1);
        let first = core::cmp::min(bytes.len(), self.size - at);
        unsafe {
            core::ptr::copy_nonoverlapping((buffer + at as u64) as *const u8, bytes.as_mut_ptr(),
                first);
            core::ptr::copy_nonoverlapping(buffer as *const u8, bytes.as_mut_ptr().add(first),
                bytes.len() - first);
        }
    }

    fn index(&self, arena: u64, at: u64) -> &AtomicU32 {
        unsafe { AtomicU32::from_ptr((arena + at) as *mut u32) }
    }
}

const HEADER: usize = 4;
const MORE: u32 = 0x8000_0000;
