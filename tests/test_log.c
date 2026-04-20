int log2_of(long long val) {
    int n;
    n = 0;
    while (val > 1) {
        val = val >> 1;
        n = n + 1;
    }
    return n;
}
