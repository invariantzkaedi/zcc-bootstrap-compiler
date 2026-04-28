fn main() -> i32 {
    let x = 42;
    while x > 0 {
        while x == 42 {
            return 42;
        }
        return 0;
    }
    return 0;
}
