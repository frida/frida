use alloc::vec::Vec;
use core::ffi::{c_char, c_int, c_void};

pub struct ProcessInfo {
    pub id: u32,
    pub name: *const u8,
    pub path: *const u8,
    pub command_line: *const u8,
}

// The kernel walks its own list, and where a process keeps its number and its name is what the
// host looked up before any of this started.
pub fn enumerate_processes(found: &mut dyn FnMut(ProcessInfo)) {
    let mut listed: Vec<(u32, [u8; NAME_SIZE])> = Vec::new();

    if let Some(iterate) = unsafe { _proc_iterate } {
        unsafe {
            iterate(
                ALL_PROCESSES,
                signed_to_be_called_back(note_a_process),
                &mut listed as *mut Vec<(u32, [u8; NAME_SIZE])> as *mut c_void,
                core::ptr::null_mut(),
                core::ptr::null_mut(),
            )
        };
    } else {
        walk_every_number(&mut listed);
    }

    for (id, name) in &listed {
        found(ProcessInfo {
            id: *id,
            name: name.as_ptr(),
            path: name.as_ptr(),
            command_line: core::ptr::null(),
        });
    }
}

pub fn describe_process(process: &ProcessInfo) -> *const u8 {
    process.name
}

pub fn enumerate_icons(_path: *const u8, _found: &mut dyn FnMut(&[u8])) {}

fn walk_every_number(listed: &mut Vec<(u32, [u8; NAME_SIZE])>) {
    let Some(find) = (unsafe { _proc_find }) else {
        return;
    };
    let release = unsafe { _proc_rele }.unwrap();

    let mut last_seen = 0;
    for number in 0..=HIGHEST_NUMBER {
        if number - last_seen > LONGEST_GAP {
            break;
        }

        let process = unsafe { find(number as c_int) };
        if process.is_null() {
            continue;
        }
        last_seen = number;

        unsafe {
            note_a_process(process, listed as *mut Vec<(u32, [u8; NAME_SIZE])> as *mut c_void);
            release(process);
        }
    }
}

fn signed_to_be_called_back(callout: ProcCallout) -> ProcCallout {
    unsafe {
        core::mem::transmute(crate::pac::ptrauth_sign(callout as *const u8,
            crate::xnu::what_a_proc_callout_is_signed_with()))
    }
}

unsafe extern "C" fn note_a_process(process: *mut c_void, listed: *mut c_void) -> c_int {
    let listed = listed as *mut Vec<(u32, [u8; NAME_SIZE])>;

    unsafe { (*listed).push((number_of(process), name_of(process))) };

    KEEP_GOING
}

unsafe fn number_of(process: *mut c_void) -> u32 {
    unsafe {
        if let Some(ask) = _proc_pid {
            return ask(process) as u32;
        }

        let where_it_is = crate::kernel::noted("process.number").unwrap() as usize;
        ((process as *const u8).add(where_it_is) as *const u32).read()
    }
}

unsafe fn name_of(process: *mut c_void) -> [u8; NAME_SIZE] {
    unsafe {
        let said = match _proc_best_name {
            Some(ask) => ask(process) as *const u8,
            None => {
                let where_it_is = crate::kernel::noted("process.name").unwrap() as usize;
                (process as *const u8).add(where_it_is)
            }
        };

        let mut name = [0u8; NAME_SIZE];
        for (at, letter) in name.iter_mut().enumerate().take(NAME_SIZE - 1) {
            let byte = said.add(at).read();
            if byte == 0 {
                break;
            }
            *letter = byte;
        }

        name
    }
}

type ProcCallout = unsafe extern "C" fn(*mut c_void, *mut c_void) -> c_int;

const NAME_SIZE: usize = 33;
const ALL_PROCESSES: c_int = 1;
const KEEP_GOING: c_int = 0;
const HIGHEST_NUMBER: u32 = 99999;
const LONGEST_GAP: u32 = 4096;

unsafe extern "C" {
    static _proc_iterate: Option<
        unsafe extern "C" fn(
            c_int,
            ProcCallout,
            *mut c_void,
            *mut c_void,
            *mut c_void,
        ),
    >;
    static _proc_find: Option<unsafe extern "C" fn(c_int) -> *mut c_void>;
    static _proc_rele: Option<unsafe extern "C" fn(*mut c_void)>;
    static _proc_pid: Option<unsafe extern "C" fn(*mut c_void) -> c_int>;
    static _proc_best_name: Option<unsafe extern "C" fn(*mut c_void) -> *const c_char>;
}
