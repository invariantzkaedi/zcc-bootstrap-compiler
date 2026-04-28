fn helper(flag: bool) {
    let x = 41;
    if flag {
        return x + 1;
    } else {
        return 0;
    }
}

fn main() -> i32 {
    return helper(true);
}
