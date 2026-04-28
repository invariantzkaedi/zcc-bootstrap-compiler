fn choose(flag: bool) -> i32 {
    if flag {
        return 42;
    } else {
        return 7;
    }
}

fn main() -> i32 {
    return choose(true);
}
