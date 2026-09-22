extern "C" {
    fn test_function();
}

pub fn main() {
    unsafe {
        test_function();
    }
}
