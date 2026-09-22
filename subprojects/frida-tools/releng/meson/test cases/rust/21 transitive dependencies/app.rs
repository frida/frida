extern "C" {
    fn static2() -> i32;
}

fn main() {
    unsafe {
        static2();
    }
}
