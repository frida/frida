#![crate_name = "rs_math"]

use std::os::raw::c_double;

extern "C" {
    fn log2(n: c_double) -> c_double;
}

#[no_mangle]
pub extern fn rs_log2(n: c_double) -> c_double {
    unsafe { log2(n) }
}
