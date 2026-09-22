// SPDX-License-Identifier: Apache-2.0
// Copyright © 2023 Intel Corporation

use std::ffi::CString;
use std::os::raw::c_char;

extern "C" {
        fn lib_length(s: *const c_char) -> u64;
}

fn main() {
        let len: u64;
        unsafe {
                let c_str = CString::new("Hello, world!").unwrap();
                len = lib_length(c_str.as_ptr());
        }

        std::process::exit(if len == 13 { 0 } else { 1 })
}
