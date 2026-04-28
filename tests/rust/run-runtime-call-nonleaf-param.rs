fn g(y: i32) -> i32 {
    return y + 1;
}

fn f(x: i32) -> i32 {
    return g(x);
}

fn main() -> i32 {
    return f(41);
}
