mod test;
use std::env;

fn main() {
    let args: Vec<String> = env::args().collect();
    let first = args[1].parse::<i32>().expect("Invalid value for first argument.");
    let second = args[2].parse::<i32>().expect("Invalid value for second argument.");

    let new = test::add(first, second);
    println!("New value: {}", new);
}
