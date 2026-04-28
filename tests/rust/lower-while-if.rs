fn main(x: i32) -> i32 {
    while x < 10 {
        if x == 5 {
            return 5;
        } else {
            return x;
        }
    }
    return 0;
}
