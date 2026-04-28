fn add_one(x: i32) -> i32 {
    return x + 1;
}

fn main() -> i32 {
    return add_one(add_one(40));
}
