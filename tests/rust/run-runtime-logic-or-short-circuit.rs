fn main() -> i32 {
    let x = 0;
    if true || (1 / x == 1) {
        return 42;
    } else {
        return 0;
    }
}
