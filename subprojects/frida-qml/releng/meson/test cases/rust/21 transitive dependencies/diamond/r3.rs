#[no_mangle]
pub fn r3() -> i32 {
    r1::r1() + r2::r2()
}
