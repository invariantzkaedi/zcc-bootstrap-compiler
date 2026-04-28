fn _force_const_path() -> i32 {
    0;
    return 0;
}

fn a() -> i32 {
    return b();
}

fn b() -> i32 {
    return a();
}

fn main() -> i32 {
    return a();
}
