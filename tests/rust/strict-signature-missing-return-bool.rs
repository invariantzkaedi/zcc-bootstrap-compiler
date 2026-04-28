fn ok(flag: bool) {
    if flag {
        return true;
    } else {
        return false;
    }
}

fn main() -> i32 {
    if ok(true) {
        return 42;
    } else {
        return 0;
    }
}
