// Linux hostlink over AF_VSOCK, bridged to the host by the Android emulator's
// pipe-over-vsock connector: connect to CID 2 (host) on port 5002, send a
// NUL-terminated "pipe:unix:<path>" handshake, and the emulator connects that
// stream to the host UNIX socket <path>, where frida-core listens. Framed the
// same way as the other hostlinks (4-byte little-endian length prefix).
//
// The agent uses the guest kernel's own socket API, so it never drives a virtio
// device and never fights the guest's own drivers.

use core::cell::UnsafeCell;
use core::ffi::{c_int, c_void};
use core::ptr;
use core::sync::atomic::{AtomicPtr, AtomicU32, AtomicUsize, Ordering};

use alloc::vec::Vec;

const AF_VSOCK: c_int = 40;
const SOCK_STREAM: c_int = 1;
const VMADDR_CID_HOST: u32 = 2;
const MSG_DONTWAIT: c_int = 0x40;
const PIPE_CONNECTOR_PORT: u32 = 5002;

#[repr(C)]
struct SockaddrVm {
    svm_family: u16,
    svm_reserved1: u16,
    svm_port: u32,
    svm_cid: u32,
    svm_zero: [u8; 4],
}

#[repr(C)]
struct Kvec {
    iov_base: *mut c_void,
    iov_len: usize,
}

type Socket = *mut c_void;

static PENDING: AtomicU32 = AtomicU32::new(0);
static WAKE_TOKEN: AtomicPtr<u8> = AtomicPtr::new(ptr::null_mut());
static ORIG_DATA_READY: AtomicUsize = AtomicUsize::new(0);

pub fn a_turn_is_wanted() -> bool {
    PENDING.load(Ordering::Acquire) != 0
}

struct Inner {
    so: Socket,
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
    pub fn init(path: &str, on_rx: Option<fn(&[u8])>, wake_token: *const u8) -> Result<Self, ()> {
        WAKE_TOKEN.store(wake_token as *mut u8, Ordering::Release);

        unsafe {
            let mut so: Socket = ptr::null_mut();
            if _sock_create_kern(_init_net_addr(), AF_VSOCK, SOCK_STREAM, 0, &mut so) != 0
                || so.is_null()
            {
                return Err(());
            }

            let addr = SockaddrVm {
                svm_family: AF_VSOCK as u16,
                svm_reserved1: 0,
                svm_port: PIPE_CONNECTOR_PORT,
                svm_cid: VMADDR_CID_HOST,
                svm_zero: [0; 4],
            };
            if _kernel_connect(
                so,
                &addr as *const _ as *const c_void,
                core::mem::size_of::<SockaddrVm>() as c_int,
                0,
            ) != 0
            {
                _sock_release(so);
                return Err(());
            }

            hook_data_ready(so);

            let hostlink = Hostlink {
                state: UnsafeCell::new(Inner {
                    so,
                    rx_lenbuf: [0; 4],
                    rx_lenhave: 0,
                    rx_buf: Vec::new(),
                    rx_have: 0,
                    rx_need: 0,
                }),
                on_rx,
            };

            let mut handshake = Vec::with_capacity(path.len() + 12);
            handshake.extend_from_slice(b"pipe:unix:");
            handshake.extend_from_slice(path.as_bytes());
            handshake.push(0);
            send_all(so, &handshake);

            Ok(hostlink)
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
        PENDING.store(0, Ordering::Release);

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

    pub fn shutdown(&self) {
        let s = unsafe { &*self.state.get() };
        unsafe { _sock_release(s.so) };
    }
}

// The vsock stack calls sk->sk_data_ready(sk) via a CONFIG_CFI_CLANG-checked indirect
// call, so the replacement must carry the type id of a void(struct sock*) callback;
// sock_def_readable is the kernel's own one. Store the original and chain to it so the
// socket's normal wakeups still fire.
unsafe fn hook_data_ready(so: Socket) {
    let (Some(sk_off), Some(dr_off)) = (
        super::layout::field_offset("socket", "sk"),
        super::layout::field_offset("sock", "sk_data_ready"),
    ) else {
        return;
    };

    let sk = unsafe { *((so as usize + sk_off) as *const usize) };
    if sk == 0 {
        return;
    }
    let slot = (sk + dr_off) as *mut usize;
    ORIG_DATA_READY.store(unsafe { *slot }, Ordering::Release);

    let thunk = super::native::kcfi_thunk(on_data_ready as usize, sock_def_readable_ref());
    unsafe { *slot = thunk };
}

fn sock_def_readable_ref() -> Option<usize> {
    let addr = unsafe { _sock_def_readable_addr() };
    if addr == 0 { None } else { Some(addr) }
}

unsafe extern "C" fn on_data_ready(sk: *mut c_void) {
    PENDING.fetch_add(1, Ordering::Release);
    let token = WAKE_TOKEN.load(Ordering::Acquire);
    if !token.is_null() {
        crate::nudge_the_loop(token as *const u8);
    }
    let orig = ORIG_DATA_READY.load(Ordering::Acquire);
    if orig != 0 {
        let f: unsafe extern "C" fn(*mut c_void) = unsafe { core::mem::transmute(orig) };
        unsafe { f(sk) };
    }
}

unsafe fn recv_nonblocking(so: Socket, dst: &mut [u8]) -> usize {
    let mut vec = Kvec {
        iov_base: dst.as_mut_ptr() as *mut c_void,
        iov_len: dst.len(),
    };
    let mut msg = [0u64; 16];
    let n = unsafe {
        _kernel_recvmsg(
            so,
            msg.as_mut_ptr() as *mut c_void,
            &mut vec,
            1,
            dst.len(),
            MSG_DONTWAIT,
        )
    };
    if n <= 0 { 0 } else { n as usize }
}

unsafe fn send_all(so: Socket, bytes: &[u8]) {
    let mut gone = 0;
    while gone < bytes.len() {
        let mut vec = Kvec {
            iov_base: unsafe { bytes.as_ptr().add(gone) } as *mut c_void,
            iov_len: bytes.len() - gone,
        };
        let mut msg = [0u64; 16];
        let n = unsafe {
            _kernel_sendmsg(
                so,
                msg.as_mut_ptr() as *mut c_void,
                &mut vec,
                1,
                bytes.len() - gone,
            )
        };
        if n <= 0 {
            return;
        }
        gone += n as usize;
    }
}

unsafe fn _init_net_addr() -> *mut c_void {
    unsafe { ptr::addr_of!(_init_net) as *mut c_void }
}

unsafe fn _sock_def_readable_addr() -> usize {
    _sock_def_readable.map_or(0, |f| f as usize)
}

unsafe extern "C" {
    static _init_net: c_void;
    static _sock_create_kern:
        unsafe extern "C" fn(*mut c_void, c_int, c_int, c_int, *mut Socket) -> c_int;
    static _kernel_connect:
        unsafe extern "C" fn(Socket, *const c_void, c_int, c_int) -> c_int;
    static _kernel_sendmsg:
        unsafe extern "C" fn(Socket, *mut c_void, *mut Kvec, usize, usize) -> c_int;
    static _kernel_recvmsg:
        unsafe extern "C" fn(Socket, *mut c_void, *mut Kvec, usize, usize, c_int) -> c_int;
    static _sock_release: unsafe extern "C" fn(Socket);
    static _sock_def_readable: Option<unsafe extern "C" fn()>;
}
