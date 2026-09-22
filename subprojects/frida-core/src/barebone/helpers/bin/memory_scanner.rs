#![no_main]
#![no_std]

use memory_scanner;

#[no_mangle]
pub extern "C" fn _start(parameters_location: *const memory_scanner::SearchParameters, results_location: *mut memory_scanner::SearchResults) -> usize {
    memory_scanner::scan(parameters_location, results_location)
}

#[panic_handler]
fn panic(_info: &core::panic::PanicInfo<'_>) -> ! {
    #[cfg(feature = "panic-info")]
    panic_info::store(_info);
    loop {}
}

#[cfg(feature = "panic-info")]
mod panic_info {
    use core::fmt::Write;
    use core::mem::MaybeUninit;

    const PANIC_INFO_SIZE: usize = 1024;
    #[no_mangle]
    #[link_section = ".bss.panic_info"]
    static mut PANIC_INFO: [MaybeUninit<u8>; PANIC_INFO_SIZE] = [MaybeUninit::uninit(); PANIC_INFO_SIZE];

    pub fn store(info: &core::panic::PanicInfo) {
        writeln!(PanicInfoSink {}, "{}", info).ok();
    }

    struct PanicInfoSink {
    }

    impl Write for PanicInfoSink {
        fn write_str(&mut self, s: &str) -> core::fmt::Result {
            let data = s.as_bytes();

            unsafe {
                core::ptr::copy(
                    data.as_ptr(),
                    PANIC_INFO.as_mut_ptr() as *mut u8,
                    core::cmp::min(data.len(), PANIC_INFO_SIZE));
            }

            Ok(())
        }
    }
}
