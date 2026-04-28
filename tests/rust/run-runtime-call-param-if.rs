fn add_one(x: i32) -> i32 {
    return x + 1;
}

fn main() -> i32 {
    let ok = true;
    if ok {
        return add_one(41);
    } else {
        return 0;
    }
}
