fn a() -> i32 {
    return 40;
}

fn b() -> i32 {
    return a() + 2;
}

fn main() -> i32 {
    return b();
}
