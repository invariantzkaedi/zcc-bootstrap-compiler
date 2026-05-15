fn mutate_c_pointer(ptr_addr: i32, limit: i32) -> i32 {
    let mut i: i32 = 0;
    while i < limit {
        i = i + 1;
    }
    return i;
}
