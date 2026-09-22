// SPDX-license-identifer: Apache-2.0
// Copyright © 2023 Red Hat, Inc

#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

include!("header3.rs");

fn main() {
    unsafe {
        std::process::exit(add(0, sub(0, 0)));
    };
}
