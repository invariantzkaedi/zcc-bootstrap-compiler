fn main() -> i32 {
    let x = 42;
    let ok = x > 0 && x < 100;
    if ok {
        return 42;
    } else {
        return 0;
    }
}
