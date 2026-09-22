// Same 4-byte LE length-prefix framing as hostlink_virtio so the host-side
// (yqv) protocol is identical across transports.

use core::cell::UnsafeCell;
use core::ffi::{c_int, c_void};
use core::ptr;
use core::sync::atomic::{AtomicU32, Ordering};

use alloc::vec::Vec;

use crate::kernel;

const AF_VSOCK: c_int = 40;
const SOCK_STREAM: c_int = 1;
const VMADDR_CID_HOST: u32 = 2;

const MSG_DONTWAIT: c_int = 0x80;

// Darwin errno values for a non-blocking connect handshake that completes in the background.
const EWOULDBLOCK: c_int = 35;
const EINPROGRESS: c_int = 36;
const EISCONN: c_int = 56;

#[repr(C)]
struct SockaddrVm {
    svm_len: u8,
    svm_family: u8,
    svm_reserved1: u16,
    svm_port: u32,
    svm_cid: u32,
    svm_zero: [u8; 4],
}

#[repr(C)]
struct Iovec {
    iov_base: *mut c_void,
    iov_len: usize,
}

#[repr(C)]
struct Msghdr {
    msg_name: *mut c_void,
    msg_namelen: u32,
    msg_iov: *mut Iovec,
    msg_iovlen: c_int,
    msg_control: *mut c_void,
    msg_controllen: u32,
    msg_flags: c_int,
}

type SocketT = *mut c_void;
type SockUpcall = unsafe extern "C" fn(so: SocketT, cookie: *mut c_void, waitf: c_int);

unsafe extern "C" {
    static _sock_socket: unsafe extern "C" fn(
        domain: c_int,
        type_: c_int,
        proto: c_int,
        callback: Option<SockUpcall>,
        cookie: *mut c_void,
        new_so: *mut SocketT,
    ) -> c_int;
    static _sock_connect:
        unsafe extern "C" fn(so: SocketT, to: *const c_void, flags: c_int) -> c_int;
    static _sock_send: unsafe extern "C" fn(
        so: SocketT,
        msg: *const Msghdr,
        flags: c_int,
        sentlen: *mut usize,
    ) -> c_int;
    static _sock_receive: unsafe extern "C" fn(
        so: SocketT,
        msg: *mut Msghdr,
        flags: c_int,
        recvlen: *mut usize,
    ) -> c_int;
    static _sock_close: unsafe extern "C" fn(so: SocketT);
}

static UPCALL_PENDING: AtomicU32 = AtomicU32::new(0);

pub fn a_turn_is_wanted() -> bool {
    UPCALL_PENDING.load(Ordering::Acquire) != 0
}

struct Inner {
    so: SocketT,
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
    pub fn init(
        host_port: u32,
        on_rx: Option<fn(&[u8])>,
        wake_token: *const u8,
    ) -> Result<Self, ()> {
        unsafe {
            let mut so: SocketT = ptr::null_mut();
            // The kernel invokes the sock_upcall via an authenticated branch (blraa, IA key)
            // with the sock_upcall type discriminator, so the pointer must be signed to match.
            let signed_upcall: SockUpcall =
                core::mem::transmute(crate::pac::ptrauth_sign(upcall as *const u8, 0x12f7));
            let rc = _sock_socket(
                AF_VSOCK,
                SOCK_STREAM,
                0,
                Some(signed_upcall),
                wake_token as *mut c_void,
                &mut so,
            );
            if rc != 0 || so.is_null() {
                return Err(());
            }

            let addr = SockaddrVm {
                svm_len: core::mem::size_of::<SockaddrVm>() as u8,
                svm_family: AF_VSOCK as u8,
                svm_reserved1: 0,
                svm_port: host_port,
                svm_cid: VMADDR_CID_HOST,
                svm_zero: [0; 4],
            };
            // The KPI connect is asynchronous: it returns EWOULDBLOCK/EINPROGRESS and the
            // handshake completes in the background once the host accepts. Re-calling connect to
            // poll is invalid (it corrupts the connecting state), so accept the in-progress codes
            // as success and let the main loop drive I/O once the link is up.
            let rc = _sock_connect(so, &addr as *const _ as *const c_void, 0);
            if rc != 0 && !matches!(rc, EWOULDBLOCK | EINPROGRESS | EISCONN) {
                _sock_close(so);
                return Err(());
            }

            Ok(Hostlink {
                state: UnsafeCell::new(Inner {
                    so,
                    rx_lenbuf: [0; 4],
                    rx_lenhave: 0,
                    rx_buf: Vec::new(),
                    rx_have: 0,
                    rx_need: 0,
                }),
                on_rx,
            })
        }
    }

    pub fn send(&self, payload: &[u8]) {
        let s = unsafe { &*self.state.get() };
        let length = (payload.len() as u32).to_le_bytes();

        unsafe {
            send_all(s.so, &length);
            send_all(s.so, payload);
        }
    }

    pub fn process(&self) {
        UPCALL_PENDING.store(0, Ordering::Release);

        let s = unsafe { &mut *self.state.get() };
        loop {
            if s.rx_lenhave < 4 {
                let lo = s.rx_lenhave;
                let n = unsafe { recv_nonblocking(s.so, &mut s.rx_lenbuf[lo..4]) };
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
                let n = unsafe { recv_nonblocking(s.so, &mut s.rx_buf[lo..hi]) };
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
        unsafe {
            let s = &*self.state.get();
            _sock_close(s.so);
        }
    }
}

unsafe fn recv_nonblocking(so: SocketT, dst: &mut [u8]) -> usize {
    unsafe {
        let mut iov = Iovec {
            iov_base: dst.as_mut_ptr() as *mut c_void,
            iov_len: dst.len(),
        };
        let mut msg = Msghdr {
            msg_name: ptr::null_mut(),
            msg_namelen: 0,
            msg_iov: &mut iov,
            msg_iovlen: 1,
            msg_control: ptr::null_mut(),
            msg_controllen: 0,
            msg_flags: 0,
        };
        let mut got: usize = 0;
        let _ = _sock_receive(so, &mut msg, MSG_DONTWAIT, &mut got);
        got
    }
}

unsafe fn send_all(so: SocketT, bytes: &[u8]) {
    let mut gone = 0;
    while gone < bytes.len() {
        let mut iov = [Iovec {
            iov_base: unsafe { bytes.as_ptr().add(gone) } as *mut c_void,
            iov_len: bytes.len() - gone,
        }];
        let msg = Msghdr {
            msg_name: ptr::null_mut(),
            msg_namelen: 0,
            msg_iov: iov.as_mut_ptr(),
            msg_iovlen: 1,
            msg_control: ptr::null_mut(),
            msg_controllen: 0,
            msg_flags: 0,
        };

        let mut sent: usize = 0;
        if unsafe { _sock_send(so, &msg, 0, &mut sent) } != 0 || sent == 0 {
            return;
        }
        gone += sent;
    }
}

unsafe extern "C" fn upcall(_so: SocketT, cookie: *mut c_void, _waitf: c_int) {
    UPCALL_PENDING.fetch_add(1, Ordering::Release);
    crate::nudge_the_loop(cookie as *const u8);
}
