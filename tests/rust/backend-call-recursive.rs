fn _force_const_path() -> i32 {
    0;
    return 0;
}

fn helper() -> i32 {
    return helper();
}

fn main() -> i32 {
    return helper();
}
