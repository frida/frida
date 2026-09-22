extern crate proc_macro_examples;
use proc_macro_examples::make_answer;

make_answer!();

pub fn func() -> u32 {
    answer()
}
