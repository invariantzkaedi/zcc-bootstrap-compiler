fn g(z: i32) -> i32 {
    return z + 10;
}

fn h(z: i32) -> i32 {
    return z + 100;
}

fn f(x: i32) -> i32 {
    let t = x + 1;
    if t < 100 {
        return g(t);
    } else {
        return h(t);
    }
}

fn main() -> i32 {
    return f(31);
}
