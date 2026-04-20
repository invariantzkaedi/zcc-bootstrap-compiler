int test_func() {
    int a = 1;
    if (a) {
        int block_var;
        block_var = 5;
        return block_var;
    }
    return 0;
}
