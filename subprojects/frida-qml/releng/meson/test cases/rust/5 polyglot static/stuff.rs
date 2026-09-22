#[no_mangle]
pub extern "C" fn hello_from_rust(a: i32, b: i32) -> i32 {
    a + b
}
