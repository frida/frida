
use core::cell::UnsafeCell;
use core::ffi::c_void;
use core::ptr;

use alloc::collections::VecDeque;
use alloc::vec::Vec;

use crate::kernel;
use crate::winnt::windows_fn;

pub struct Hostlink {
    state: UnsafeCell<Inner>,
    on_rx: Option<fn(&[u8])>,
}

struct Inner {
    port: *mut c_void,
    event: *mut c_void,
}

fn issue<F>(event: *mut c_void, request: F) -> Option<usize>
where
    F: FnOnce(*mut c_void, *mut c_void) -> i32,
{
    let mut status_block = [0usize; STATUS_BLOCK_WORDS];
    unsafe {
        (_ZwClearEvent)(event);
    }

    let status = request(event, status_block.as_mut_ptr() as *mut c_void);
    if status == STATUS_PENDING {
        unsafe {
            (_ZwWaitForSingleObject)(event, 0, ptr::null());
        }
    }

    if status < 0 || (status_block[STATUS_BLOCK_STATUS] as i32) < 0 {
        return None;
    }

    Some(status_block[STATUS_BLOCK_COUNT])
}

fn create_event() -> Option<*mut c_void> {
    let mut event: *mut c_void = ptr::null_mut();
    let status = unsafe {
        (_ZwCreateEvent)(&mut event, EVENT_ALL_ACCESS, ptr::null_mut(), SYNCHRONIZATION_EVENT, 0)
    };
    if status < 0 {
        return None;
    }
    Some(event)
}

impl Hostlink {
    pub fn init(on_rx: Option<fn(&[u8])>, wake_token: *const u8) -> Result<Self, ()> {
        let port = open_first_serial_port()?;
        let event = create_event().ok_or(())?;
        let reader_event = create_event().ok_or(())?;

        unsafe {
            WAKE_TOKEN = wake_token;
            READER_PORT = port;
            READER_EVENT = reader_event;
        }
        kernel::spawn_thread(read_from_host, ptr::null_mut());

        Ok(Self {
            state: UnsafeCell::new(Inner { port, event }),
            on_rx,
        })
    }

    pub fn send(&self, payload: &[u8]) {
        let s = unsafe { &*self.state.get() };

        write_all(s.port, s.event, &(payload.len() as u32).to_le_bytes());
        write_all(s.port, s.event, payload);
    }

    pub fn process(&self) {
        loop {
            let Some(frame) = take_frame() else {
                return;
            };
            if let Some(on_rx) = self.on_rx {
                on_rx(&frame);
            }
        }
    }

    pub fn shutdown(&self) {
        let s = unsafe { &*self.state.get() };
        unsafe {
            (_ZwClose)(s.port);
            (_ZwClose)(s.event);
        }
    }
}

unsafe extern "C" fn read_from_host(_parameter: *mut c_void, _wait_result: i32) {
    let port = unsafe { READER_PORT };
    let event = unsafe { READER_EVENT };

    let mut length = [0u8; FRAME_LENGTH_SIZE];
    loop {
        if !read_exactly(port, event, &mut length) {
            return;
        }

        let mut frame = alloc::vec![0u8; u32::from_le_bytes(length) as usize];
        if !read_exactly(port, event, &mut frame) {
            return;
        }

        put_frame(frame);
        kernel::wake(unsafe { WAKE_TOKEN });
    }
}

fn read_exactly(port: *mut c_void, event: *mut c_void, buffer: &mut [u8]) -> bool {
    let mut read = 0;
    while read != buffer.len() {
        let mut offset = 0i64;
        let moved = issue(event, |event, status_block| unsafe {
            (_ZwReadFile)(
                port,
                event,
                ptr::null_mut(),
                ptr::null_mut(),
                status_block,
                buffer.as_mut_ptr().add(read),
                (buffer.len() - read) as u32,
                &mut offset,
                ptr::null_mut(),
            )
        });

        let Some(moved) = moved else {
            return false;
        };
        if moved == 0 {
            return false;
        }
        read += moved;
    }

    true
}

fn write_all(port: *mut c_void, event: *mut c_void, buffer: &[u8]) {
    let mut written = 0;
    while written != buffer.len() {
        let mut offset = 0i64;
        let moved = issue(event, |event, status_block| unsafe {
            (_ZwWriteFile)(
                port,
                event,
                ptr::null_mut(),
                ptr::null_mut(),
                status_block,
                buffer.as_ptr().add(written),
                (buffer.len() - written) as u32,
                &mut offset,
                ptr::null_mut(),
            )
        });

        let Some(moved) = moved else {
            return;
        };
        if moved == 0 {
            return;
        }
        written += moved;
    }
}

pub fn a_turn_is_wanted() -> bool {
    A_TURN_IS_WANTED.load(Ordering::Acquire)
}

static A_TURN_IS_WANTED: AtomicBool = AtomicBool::new(false);

fn put_frame(frame: Vec<u8>) {
    lock_frames();
    unsafe {
        (*ptr::addr_of_mut!(FRAMES)).push_back(frame);
    }
    unlock_frames();
    A_TURN_IS_WANTED.store(true, Ordering::Release);
}

fn take_frame() -> Option<Vec<u8>> {
    lock_frames();
    let frame = unsafe { (*ptr::addr_of_mut!(FRAMES)).pop_front() };
    let drained = unsafe { (*ptr::addr_of!(FRAMES)).is_empty() };
    unlock_frames();
    if drained {
        A_TURN_IS_WANTED.store(false, Ordering::Release);
    }
    frame
}

fn lock_frames() {
    while FRAMES_LOCK
        .compare_exchange(0, 1, Ordering::Acquire, Ordering::Relaxed)
        .is_err()
    {
        kernel::yield_now();
    }
}

fn unlock_frames() {
    FRAMES_LOCK.store(0, Ordering::Release);
}

static mut FRAMES: VecDeque<Vec<u8>> = VecDeque::new();
static FRAMES_LOCK: AtomicU32 = AtomicU32::new(0);
static mut WAKE_TOKEN: *const u8 = ptr::null();
static mut READER_PORT: *mut c_void = ptr::null_mut();
static mut READER_EVENT: *mut c_void = ptr::null_mut();

fn open_first_serial_port() -> Result<*mut c_void, ()> {
    let ports = open_key(SERIAL_PORT_KEY)?;

    let mut name = [0u8; VALUE_NAME_SIZE];
    let mut size = 0u32;
    let status = unsafe {
        (_ZwEnumerateValueKey)(ports, 0, VALUE_NAME_INFORMATION, name.as_mut_ptr(),
            name.len() as u32, &mut size)
    };
    unsafe {
        (_ZwClose)(ports);
    }
    if status < 0 {
        return Err(());
    }

    let length = u32::from_le_bytes(name[VALUE_NAME_LENGTH..VALUE_NAME_LENGTH + 4]
        .try_into()
        .unwrap()) as usize;
    open_device(&name[VALUE_NAME_OFFSET..VALUE_NAME_OFFSET + length])
}

fn open_key(path: &[u16]) -> Result<*mut c_void, ()> {
    let mut handle: *mut c_void = ptr::null_mut();
    let mut name = unicode_string(path);
    let mut attributes = object_attributes(&mut name);

    let status =
        unsafe { (_ZwOpenKey)(&mut handle, KEY_READ, attributes.as_mut_ptr() as *mut c_void) };
    if status < 0 {
        return Err(());
    }

    Ok(handle)
}

fn open_device(path: &[u8]) -> Result<*mut c_void, ()> {
    let mut handle: *mut c_void = ptr::null_mut();
    let mut name = UnicodeString {
        length: path.len() as u16,
        maximum_length: path.len() as u16,
        buffer: path.as_ptr() as *const u16,
    };
    let mut attributes = object_attributes(&mut name);
    let mut status_block = [0usize; STATUS_BLOCK_WORDS];

    let status = unsafe {
        (_ZwCreateFile)(
            &mut handle,
            GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE,
            attributes.as_mut_ptr() as *mut c_void,
            status_block.as_mut_ptr() as *mut c_void,
            ptr::null_mut(),
            0,
            0,
            FILE_OPEN,
            0,
            ptr::null_mut(),
            0,
        )
    };
    if status < 0 {
        return Err(());
    }

    configure_port(handle);

    Ok(handle)
}

fn configure_port(port: *mut c_void) {
    let mut baud_rate = [0u32; 1];
    baud_rate[0] = BAUD_RATE;
    control_port(port, SET_BAUD_RATE, baud_rate.as_ptr() as *const u8,
        core::mem::size_of_val(&baud_rate) as u32);

    let line_control = [ONE_STOP_BIT, NO_PARITY, DATA_BITS];
    control_port(port, SET_LINE_CONTROL, line_control.as_ptr(),
        core::mem::size_of_val(&line_control) as u32);

    let hand_flow = [0u32; HAND_FLOW_WORDS];
    control_port(port, SET_HAND_FLOW, hand_flow.as_ptr() as *const u8,
        core::mem::size_of_val(&hand_flow) as u32);

    control_port(port, SET_DTR, core::ptr::null(), 0);
    control_port(port, SET_RTS, core::ptr::null(), 0);

    let mut timeouts = [0u32; TIMEOUT_WORDS];
    timeouts[READ_INTERVAL_TIMEOUT] = FOREVER;
    timeouts[READ_TOTAL_TIMEOUT_MULTIPLIER] = FOREVER;
    timeouts[READ_TOTAL_TIMEOUT_CONSTANT] = ALMOST_FOREVER;
    control_port(port, SET_TIMEOUTS, timeouts.as_ptr() as *const u8,
        core::mem::size_of_val(&timeouts) as u32);
}

fn control_port(port: *mut c_void, code: u32, input: *const u8, length: u32) {
    let Some(event) = create_event() else {
        return;
    };

    issue(event, |event, status_block| unsafe {
        (_ZwDeviceIoControlFile)(
            port,
            event,
            ptr::null_mut(),
            ptr::null_mut(),
            status_block,
            code,
            input,
            length,
            ptr::null_mut(),
            0,
        )
    });

    unsafe {
        (_ZwClose)(event);
    }
}

#[repr(C)]
struct UnicodeString {
    length: u16,
    maximum_length: u16,
    buffer: *const u16,
}

fn unicode_string(text: &[u16]) -> UnicodeString {
    let bytes = (text.len() * 2) as u16;
    UnicodeString {
        length: bytes,
        maximum_length: bytes,
        buffer: text.as_ptr(),
    }
}

fn object_attributes(name: &mut UnicodeString) -> [usize; OBJECT_ATTRIBUTES_WORDS] {
    let mut attributes = [0usize; OBJECT_ATTRIBUTES_WORDS];
    attributes[0] = core::mem::size_of::<[usize; OBJECT_ATTRIBUTES_WORDS]>();
    attributes[2] = name as *mut UnicodeString as usize;
    attributes[3] = OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE;
    attributes
}

const FRAME_LENGTH_SIZE: usize = 4;
const STATUS_BLOCK_WORDS: usize = 2;
const STATUS_BLOCK_COUNT: usize = 1;
const STATUS_BLOCK_STATUS: usize = 0;
const OBJECT_ATTRIBUTES_WORDS: usize = 6;

const SERIAL_PORT_KEY: &[u16] = &[
    0x5c, 0x52, 0x45, 0x47, 0x49, 0x53, 0x54, 0x52, 0x59, 0x5c, 0x4d, 0x41, 0x43, 0x48, 0x49, 0x4e,
    0x45, 0x5c, 0x48, 0x41, 0x52, 0x44, 0x57, 0x41, 0x52, 0x45, 0x5c, 0x44, 0x45, 0x56, 0x49, 0x43,
    0x45, 0x4d, 0x41, 0x50, 0x5c, 0x53, 0x45, 0x52, 0x49, 0x41, 0x4c, 0x43, 0x4f, 0x4d, 0x4d,
];

const VALUE_NAME_INFORMATION: u32 = 0;
const VALUE_NAME_SIZE: usize = 512;
const VALUE_NAME_LENGTH: usize = 0x08;
const VALUE_NAME_OFFSET: usize = 0x0c;

const SET_BAUD_RATE: u32 = 0x001b_0004;
const SET_LINE_CONTROL: u32 = 0x001b_000c;
const SET_TIMEOUTS: u32 = 0x001b_001c;
const SET_DTR: u32 = 0x001b_0024;
const SET_RTS: u32 = 0x001b_0030;
const SET_HAND_FLOW: u32 = 0x001b_0064;

const BAUD_RATE: u32 = 921_600;
const DATA_BITS: u8 = 8;
const NO_PARITY: u8 = 0;
const ONE_STOP_BIT: u8 = 0;
const HAND_FLOW_WORDS: usize = 4;
const TIMEOUT_WORDS: usize = 5;
const READ_INTERVAL_TIMEOUT: usize = 0;
const READ_TOTAL_TIMEOUT_MULTIPLIER: usize = 1;
const READ_TOTAL_TIMEOUT_CONSTANT: usize = 2;
const FOREVER: u32 = u32::MAX;
const ALMOST_FOREVER: u32 = u32::MAX - 1;
const STATUS_PENDING: i32 = 0x103;
const SYNCHRONIZATION_EVENT: u32 = 1;
const EVENT_ALL_ACCESS: u32 = 0x1f_0003;
const EXECUTIVE: u32 = 0;
const KERNEL_MODE: u32 = 0;

const KEY_READ: u32 = 0x2_0019;
const GENERIC_READ: u32 = 0x8000_0000;
const GENERIC_WRITE: u32 = 0x4000_0000;
const SYNCHRONIZE: u32 = 0x0010_0000;
const FILE_OPEN: u32 = 1;
const FILE_SYNCHRONOUS_IO_NONALERT: u32 = 0x20;
const OBJ_CASE_INSENSITIVE: usize = 0x40;
const OBJ_KERNEL_HANDLE: usize = 0x200;

use core::sync::atomic::{AtomicBool, AtomicU32, Ordering};

unsafe extern "C" {
    static _ZwOpenKey: windows_fn!(*mut *mut c_void, u32, *mut c_void => i32);
    static _ZwEnumerateValueKey: windows_fn!(
        *mut c_void, u32, u32, *mut u8, u32, *mut u32 => i32);
    static _ZwCreateFile: windows_fn!(
        *mut *mut c_void, u32, *mut c_void, *mut c_void, *mut i64, u32, u32, u32, u32,
        *mut c_void, u32 => i32);
    static _ZwReadFile: windows_fn!(
        *mut c_void, *mut c_void, *mut c_void, *mut c_void, *mut c_void, *mut u8, u32, *mut i64,
        *mut u32 => i32);
    static _ZwWriteFile: windows_fn!(
        *mut c_void, *mut c_void, *mut c_void, *mut c_void, *mut c_void, *const u8, u32, *mut i64,
        *mut u32 => i32);
    static _ZwDeviceIoControlFile: windows_fn!(
        *mut c_void, *mut c_void, *mut c_void, *mut c_void, *mut c_void, u32, *const u8, u32,
        *mut u8, u32 => i32);
    static _ZwClose: windows_fn!(*mut c_void => i32);
    static _ZwCreateEvent: windows_fn!(*mut *mut c_void, u32, *mut c_void, u32, u8 => i32);
    static _ZwClearEvent: windows_fn!(*mut c_void => i32);
    static _ZwWaitForSingleObject: windows_fn!(*mut c_void, u8, *const i64 => i32);
    static _ObReferenceObjectByHandle: windows_fn!(
        *mut c_void, u32, *mut c_void, u8, *mut *mut c_void, *mut c_void => i32);
    static _KeWaitForSingleObject: windows_fn!(*mut c_void, u32, u32, u8, *const i64 => i32);
}
