int log2_of(long long val) { int n = 0; while (val > 1) { val >>= 1; n += 1; } return n; }
