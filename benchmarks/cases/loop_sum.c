int printf(const char *fmt, ...);
int run_loop_sum(int limit) {
    int i, j, sum = 0;
    for (i = 0; i < 5000000; i++) {
        if (limit > 0) {
            sum = sum + i;
        }
    }
    return sum;
}
int main() {
    printf("loop_sum res = %d\n", run_loop_sum(100));
    return 0;
}
