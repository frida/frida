#[repr(C)]
pub struct Adder {
  pub number: i32
}

extern "C" {
    pub fn zero() -> i32;
    pub fn zero_static() -> i32;
}

#[no_mangle]
pub extern fn adder_add_r(a: &Adder, number: i32) -> i32 {
  unsafe {
    return a.number + number + zero() + zero_static();
  }
}
