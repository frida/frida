extern "C" {
    fn c_func();
}

fn main() {
    unsafe {
        c_func();
    }
}
