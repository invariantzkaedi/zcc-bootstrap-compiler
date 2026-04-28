fn helper() -> i32 {
    if 1 < 2 {
        return 42;
    } else {
        return 0;
    }
}

fn main() -> i32 {
    return helper();
}
