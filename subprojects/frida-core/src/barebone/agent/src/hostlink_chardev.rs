// Linux transport: /dev/frida, framed the same way as the XNU hostlinks (4-byte
// little-endian length prefix) so the host-side (yqv) protocol is identical across
// every transport.
//
// The character device itself lives in linux/frida-kmod.c and hands us a byte
// stream; whoever opens the device decides how it reaches the host.

use core::cell::UnsafeCell;
use core::ffi::{c_int, c_void};

use alloc::vec::Vec;

struct Inner {
    rx_lenbuf: [u8; 4],
    rx_lenhave: usize,
    rx_buf: Vec<u8>,
    rx_have: usize,
    rx_need: usize,
}

pub struct Hostlink {
    state: UnsafeCell<Inner>,
    on_rx: Option<fn(&[u8])>,
}

unsafe impl Send for Hostlink {}

impl Hostlink {
    pub fn init(on_rx: Option<fn(&[u8])>) -> Result<Self, ()> {
        if unsafe { frida_kmod_link_open() } != 0 {
            return Err(());
        }

        Ok(Hostlink {
            state: UnsafeCell::new(Inner {
                rx_lenbuf: [0; 4],
                rx_lenhave: 0,
                rx_buf: Vec::new(),
                rx_have: 0,
                rx_need: 0,
            }),
            on_rx,
        })
    }

    pub fn send(&self, payload: &[u8]) {
        let header = (payload.len() as u32).to_le_bytes();
        unsafe {
            frida_kmod_link_send(header.as_ptr() as *const c_void, header.len());
            frida_kmod_link_send(payload.as_ptr() as *const c_void, payload.len());
        }
    }

    pub fn process(&self) {
        let s = unsafe { &mut *self.state.get() };
        loop {
            if s.rx_lenhave < 4 {
                let lo = s.rx_lenhave;
                let n = recv_nonblocking(&mut s.rx_lenbuf[lo..4]);
                if n == 0 {
                    return;
                }
                s.rx_lenhave += n;
                if s.rx_lenhave < 4 {
                    continue;
                }
                s.rx_need = u32::from_le_bytes(s.rx_lenbuf) as usize;
                s.rx_have = 0;
                s.rx_buf.resize(s.rx_need, 0);
            }

            while s.rx_have < s.rx_need {
                let lo = s.rx_have;
                let hi = s.rx_need;
                let n = recv_nonblocking(&mut s.rx_buf[lo..hi]);
                if n == 0 {
                    return;
                }
                s.rx_have += n;
            }

            // Detach the frame and reset receive state before dispatching: the callback may
            // re-enter process() (a synchronous host RPC issued while handling a command), and
            // it must start from a clean state instead of re-dispatching this same frame.
            let frame = core::mem::take(&mut s.rx_buf);
            let need = s.rx_need;
            s.rx_lenhave = 0;
            s.rx_have = 0;
            s.rx_need = 0;

            if let Some(cb) = self.on_rx {
                cb(&frame[..need]);
            }
        }
    }
}

impl Drop for Hostlink {
    fn drop(&mut self) {
        unsafe { frida_kmod_link_close() };
    }
}

pub fn a_turn_is_wanted() -> bool {
    unsafe { frida_kmod_link_pending() }
}

fn recv_nonblocking(dst: &mut [u8]) -> usize {
    let n = unsafe { frida_kmod_link_recv(dst.as_mut_ptr() as *mut c_void, dst.len()) };
    if n <= 0 { 0 } else { n as usize }
}

unsafe extern "C" {
    fn frida_kmod_link_open() -> c_int;
    fn frida_kmod_link_close();
    fn frida_kmod_link_send(data: *const c_void, size: usize) -> c_int;
    fn frida_kmod_link_recv(data: *mut c_void, size: usize) -> isize;
    fn frida_kmod_link_pending() -> bool;
}
