fn main() -> i32 {
    return add(a(), b());
}

fn a() -> i32 {
    return 20;
}

fn b() -> i32 {
    return 22;
}

fn add(x: i32, y: i32) -> i32 {
    return x + y;
}
