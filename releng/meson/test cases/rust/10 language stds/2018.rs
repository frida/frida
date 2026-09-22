const fn foo(x: i32) -> i32 {
    return x + 1;
}

const VALUE: i32 = foo(-1);

pub fn main() {
    std::process::exit(VALUE);
}
