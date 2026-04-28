fn main() -> i32 {
    return f(41);
}

fn f(x: i32) -> i32 {
    return g(x);
}

fn g(y: i32) -> i32 {
    return y + 1;
}
