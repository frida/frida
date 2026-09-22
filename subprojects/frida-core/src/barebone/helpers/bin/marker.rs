#![no_main]
#![no_std]

static ANSWER: u32 = 0x1234abcd;

#[no_mangle]
pub extern "C" fn _start(destination: *mut u32, _size: usize) {
    unsafe {
        *destination = &ANSWER as *const u32 as u32;
        *destination.add(1) = core::ptr::read_volatile(&ANSWER);
    }
}

#[panic_handler]
fn panic(_info: &core::panic::PanicInfo<'_>) -> ! {
    loop {}
}
