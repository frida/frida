extern "C" {
    fn c_func() -> i32;
}

pub fn r1() -> i32 {
    unsafe {
        c_func()
    }
}
