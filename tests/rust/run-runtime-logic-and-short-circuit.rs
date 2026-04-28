fn main() -> i32 {
    let x = 0;
    if false && (1 / x == 1) {
        return 0;
    } else {
        return 42;
    }
}
