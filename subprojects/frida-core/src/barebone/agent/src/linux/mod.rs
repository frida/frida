pub mod hostlink_pipe_vsock;
// Linux backend. The agent reaches the kernel in one of two ways, and the rest
// of the agent cannot tell which: loaded from the inside as a kernel module,
// where every primitive is a call into a shim kbuild compiled against the
// target's headers; or injected from the outside like XNU and Windows, where
// the host patches the addresses it needs into .kernel_addrs and the memory it
// cannot touch is reached over the hostlink.

use alloc::string::String;

pub fn stack_headroom() -> usize {
    let marker = 0usize;
    let here = &marker as *const usize as usize;

    here - (here & !(STACK_SPAN - 1))
}

#[cfg(any(target_arch = "aarch64", target_arch = "x86_64"))]
pub(crate) const STACK_SPAN: usize = 16 * 1024;

#[cfg(any(target_arch = "arm", target_arch = "x86"))]
pub(crate) const STACK_SPAN: usize = 8 * 1024;

#[cfg(feature = "linux")]
mod kmod;
#[cfg(feature = "linux-injected")]
mod arena;
#[cfg(feature = "linux-injected")]
mod facade;
#[cfg(feature = "linux-injected")]
#[cfg(feature = "linux-injected")]
mod injection;
#[cfg(feature = "linux-injected")]
pub(crate) mod layout;
#[cfg(feature = "linux-injected")]
mod mapped;
#[cfg(feature = "linux-injected")]
mod modules;
#[cfg(feature = "linux-injected")]
mod kfault;
#[cfg(feature = "linux-injected")]
mod native;
#[cfg(feature = "linux-injected")]
mod relay;
#[cfg(feature = "linux-injected")]
mod spawn;
#[cfg(feature = "linux-injected")]
mod threads;
#[cfg(feature = "linux-injected")]
pub(crate) mod user;
#[cfg(feature = "linux-injected")]
mod processes;

#[cfg(feature = "linux")]
pub use self::kmod::*;
#[cfg(feature = "linux-injected")]
pub use self::injection::*;
#[cfg(feature = "linux-injected")]
pub use self::relay::*;
#[cfg(feature = "linux-injected")]
pub use self::facade::*;
#[cfg(feature = "linux-injected")]
pub use self::native::{
    alloc_dma, free_dma, get_kernel_base, get_kernel_size, install_interrupt_handler, map_io,
    map_pages,
    mmio_interrupt, pci_interrupt, release_fault_reporter, release_interrupt, run_when_ready,
    patch_text, set_kernel_base, set_protection, virt_to_phys,
};
#[cfg(feature = "linux-injected")]
pub use self::mapped::*;
#[cfg(feature = "linux-injected")]
pub use self::modules::*;
#[cfg(feature = "linux-injected")]
pub use self::processes::*;
#[cfg(feature = "linux-injected")]
pub use self::spawn::*;
#[cfg(feature = "linux-injected")]
pub use self::threads::*;

#[derive(Debug, Clone)]
pub enum ModuleEvent {
    Loaded,
    Unloaded,
}

#[derive(Debug, Clone)]
pub struct LoadedModule {
    pub name: String,
    pub version: String,
    pub base: u64,
    pub size: u64,
}
