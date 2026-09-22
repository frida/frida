// Facade over the host kernel the agent is running inside of. Both backends
// expose the same primitives, so the rest of the agent never names a specific
// kernel.
//
// A kernel is reached from the outside -- a remote stub injects the agent and
// patches the addresses it needs into .kernel_addrs -- or from the inside,
// where the agent is a kernel module whose glue is an ordinary C translation
// unit compiled against the target kernel's headers. Linux is reached either
// way; the others only from the outside.

#[cfg(any(feature = "linux", feature = "linux-injected"))]
pub use crate::linux::*;
#[cfg(feature = "win9x")]
pub use crate::win9x::*;
#[cfg(feature = "winnt")]
pub use crate::winnt::*;
#[cfg(feature = "xnu-core")]
pub use crate::xnu::*;
#[cfg(feature = "xnu-core")]
pub use crate::xnu_applications::*;
#[cfg(feature = "xnu-core")]
pub use crate::xnu_bell::*;
#[cfg(feature = "xnu-core")]
pub use crate::xnu_hiding::*;
#[cfg(feature = "xnu-core")]
pub use crate::xnu_injection::*;
#[cfg(feature = "xnu-core")]
pub use crate::xnu_processes::*;
#[cfg(feature = "xnu-core")]
pub use crate::xnu_relay::*;
#[cfg(feature = "xnu-core")]
pub use crate::xnu_spawn::*;

use core::ffi::c_void;

pub type ThreadEntry = unsafe extern "C" fn(parameter: *mut c_void, wait_result: i32);

pub struct ThreadInfo {
    pub id: u32,
    pub cpu_state: Option<CpuState>,
}

#[cfg(target_arch = "x86_64")]
pub struct CpuState {
    pub rip: u64,
    pub r15: u64,
    pub r14: u64,
    pub r13: u64,
    pub r12: u64,
    pub r11: u64,
    pub r10: u64,
    pub r9: u64,
    pub r8: u64,
    pub rdi: u64,
    pub rsi: u64,
    pub rbp: u64,
    pub rsp: u64,
    pub rbx: u64,
    pub rdx: u64,
    pub rcx: u64,
    pub rax: u64,
}

#[cfg(target_arch = "arm")]
pub struct CpuState {
    pub pc: u32,
    pub sp: u32,
    pub cpsr: u32,
    pub r: [u32; 8],
    pub r8: u32,
    pub r9: u32,
    pub r10: u32,
    pub r11: u32,
    pub r12: u32,
    pub lr: u32,
}

#[cfg(target_arch = "aarch64")]
pub struct CpuState {
    pub pc: u64,
    pub sp: u64,
    pub nzcv: u64,
    pub x: [u64; 29],
    pub fp: u64,
    pub lr: u64,
}

#[cfg(target_arch = "x86")]
pub struct CpuState {
    pub eip: u32,
    pub edi: u32,
    pub esi: u32,
    pub ebp: u32,
    pub esp: u32,
    pub ebx: u32,
    pub edx: u32,
    pub ecx: u32,
    pub eax: u32,
}

// What the host looked up before the agent started, for the kernels that say nothing about
// their own layout and leave nowhere for the agent to read it.
pub fn take_note_of(what: &str, number: u64) {
    unsafe { noted_numbers() }.push((alloc::string::String::from(what), number));
}

pub fn noted(what: &str) -> Option<u64> {
    unsafe { noted_numbers() }
        .iter()
        .find(|(name, _)| name == what)
        .map(|(_, number)| *number)
}

pub fn take_note_of_mapping(address: u64, size: usize, protection: u32) {
    unsafe { mappings() }.push((address, size, protection));
}

pub fn noted_mappings() -> &'static [(u64, usize, u32)] {
    unsafe { mappings() }.as_slice()
}

unsafe fn mappings() -> &'static mut alloc::vec::Vec<(u64, usize, u32)> {
    unsafe { (&raw mut MAPPINGS).as_mut().unwrap() }
}

static mut MAPPINGS: alloc::vec::Vec<(u64, usize, u32)> = alloc::vec::Vec::new();

unsafe fn noted_numbers() -> &'static mut alloc::vec::Vec<(alloc::string::String, u64)> {
    unsafe { (&raw mut NOTED).as_mut().unwrap() }
}

static mut NOTED: alloc::vec::Vec<(alloc::string::String, u64)> = alloc::vec::Vec::new();
