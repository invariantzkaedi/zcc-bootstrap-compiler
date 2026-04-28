fn inc(x: i32) -> i32 {
    return x + 1;
}

fn dec(x: i32) -> i32 {
    return x - 1;
}

fn add(x: i32, y: i32) -> i32 {
    return x + y;
}

fn main() -> i32 {
    return add(inc(10), dec(32));
}
