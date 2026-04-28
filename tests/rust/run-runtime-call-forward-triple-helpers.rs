fn main() -> i32 {
    return triple(w1(), w2(), w3());
}

fn w1() -> i32 {
    return 10;
}

fn w2() -> i32 {
    return 20;
}

fn w3() -> i32 {
    return 12;
}

fn triple(a: i32, b: i32, c: i32) -> i32 {
    return a + b + c;
}
