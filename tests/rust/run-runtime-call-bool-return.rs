fn is_pos(x: i32) -> bool {
    return x > 0;
}

fn main() -> i32 {
    if is_pos(5) {
        return 42;
    } else {
        return 0;
    }
}
