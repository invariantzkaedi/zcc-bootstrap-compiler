fn helper() -> i32 {
    return 42;
}

fn main() -> i32 {
    let ok = true;
    if ok {
        return helper();
    } else {
        return 0;
    }
}
