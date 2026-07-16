fn run_div(x: i32, y: i32) -> i32 {
    let q: i32 = x / y;
    return q;
}

fn main() -> i32 {
    let num: i32 = 42;
    let zero: i32 = 0;
    let res: i32 = run_div(num, zero);
    return res;
}
