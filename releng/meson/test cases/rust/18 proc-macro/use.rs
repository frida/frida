extern crate proc_macro_examples;
use proc_macro_examples::make_answer;

make_answer!();

fn main() {
    assert_eq!(42, answer());
}
