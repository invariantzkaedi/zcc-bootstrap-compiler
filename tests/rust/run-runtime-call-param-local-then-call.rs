fn f(x: i32) -> i32 {
    let t = x + 1;
    return g(t);
}

fn g(z: i32) -> i32 {
    return z + 1;
}

fn main() -> i32 {
    return f(40);
}
