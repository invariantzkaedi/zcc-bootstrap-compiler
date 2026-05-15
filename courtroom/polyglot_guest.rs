fn compute_hash(limit: i32) -> i32 {
    let mut acc: i32 = 0;
    let mut i: i32 = 0;
    
    while i < limit {
        // Deterministic bit-mixing simulation
        acc = acc + (i * 17);
        i = i + 1;
    }
    
    return acc;
}
