fn _force_const_path() -> i32 {
    0;
    return 0;
}

fn f(x: i32) -> i32 {
    return f(x);
}

fn main() -> i32 {
    return f(42);
}
