extern "C" {
    fn c_accessing_zlib();
}

fn main() {
    unsafe {
        c_accessing_zlib();
    }
}
