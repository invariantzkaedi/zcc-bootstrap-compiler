fn pick(x: i32) -> i32 {
    if x == 42 {
        return 42;
    } else {
        return 0;
    }
}

fn main() -> i32 {
    return pick(42);
}
