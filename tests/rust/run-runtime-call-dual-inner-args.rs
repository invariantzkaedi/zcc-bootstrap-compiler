fn inc(x: i32) -> i32 {
    return x + 1;
}

fn add(x: i32, y: i32) -> i32 {
    return x + y;
}

fn main() -> i32 {
    return add(inc(20), inc(20));
}
