fn test_sum(a: f32, b: f32) -> f32 {
    return a + b;
}

fn main() -> i32 {
    let x: f32 = 1.5f32;
    let y: f32 = 2.5f32;
    let sum: f32 = test_sum(x, y);
    if sum > 3.9f32 {
        if sum < 4.1f32 {
            return 0; // SUCCESS
        }
    }
    return 1; // FAILURE
}
