extern crate pm;
use pm::make_answer;

make_answer!();

#[no_mangle]
pub fn foo_rs() -> u32 {
    answer()
}
