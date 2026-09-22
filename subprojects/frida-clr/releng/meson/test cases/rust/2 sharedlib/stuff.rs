#![crate_name = "stuff"]

extern "C" {
        fn c_value() -> i32;
}

pub fn explore() -> String {
    unsafe {
        format!("library{}string", c_value())
    }
}
