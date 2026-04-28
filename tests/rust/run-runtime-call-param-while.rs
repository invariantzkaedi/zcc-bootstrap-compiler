fn step(x: i32) -> i32 {
    return x + 1;
}

fn main() -> i32 {
    let done = false;
    while done {
        return step(0);
    }
    return step(41);
}
