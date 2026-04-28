fn ping(n: i32) -> i32 {
    if n == 0 {
        return 0;
    } else {
        return 1 + pong(n - 1);
    }
}

fn pong(n: i32) -> i32 {
    if n == 0 {
        return 0;
    } else {
        return 1 + ping(n - 1);
    }
}

fn main() -> i32 {
    return ping(42);
}
