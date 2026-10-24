int printf(const char *fmt, ...);
int main() {
    int i, sum = 0;
    for (i = 0; i < 100000; i++) {
        if (i % 2 == 0) {
            sum = sum + 3;
        } else {
            sum = sum - 1;
        }
        if (i % 3 == 0) {
            sum = sum * 2;
        } else if (i % 5 == 0) {
            sum = sum + 7;
        } else {
            sum = sum - 2;
        }
    }
    printf("sum = %d\n", sum);
    return 0;
}
