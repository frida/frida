// SPDX-license-identifer: Apache-2.0
// Copyright © 2021 Intel Corporation

#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

include!("header.rs");

fn main() {
    unsafe {
        std::process::exit(add(0, 0));
    };
}
