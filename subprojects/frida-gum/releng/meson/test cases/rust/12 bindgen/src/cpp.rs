// SPDX-license-identifer: Apache-2.0
// Copyright © 2023 Intel Corporation

#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

include!("generated-cpp.rs");

fn main() {
    let mut instance = std::mem::MaybeUninit::<MyClass>::uninit();
    let val: i32;
    unsafe {
        MyClass_MyClass(instance.as_mut_ptr());
        val = instance.assume_init_mut().method();
    }
    let success = val == 7;
    std::process::exit(!success as i32);
}
