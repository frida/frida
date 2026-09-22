// use a name that collides with one of the rustc_private libraries
#![crate_name = "rand"]

pub fn explore() -> &'static str { "librarystring" }
