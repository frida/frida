extern "C" {
    fn c_func() -> i32;
}

pub fn r2() -> i32 {
    unsafe {
        c_func()
    }
}
