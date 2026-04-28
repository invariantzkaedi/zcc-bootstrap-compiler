fn negate(x: bool) -> bool {
    return !x;
}

fn main() -> i32 {
    if negate(false) {
        return 42;
    } else {
        return 0;
    }
}
