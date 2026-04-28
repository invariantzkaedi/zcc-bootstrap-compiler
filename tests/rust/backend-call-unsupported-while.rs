fn helper() -> i32 {
    while 1 < 2 {
        return 42;
    }
    return 0;
}

fn main() -> i32 {
    return helper();
}
