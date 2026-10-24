int printf(const char *fmt, ...);
int fib(int n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}
int main() {
    printf("fib(34) = %d\n", fib(34));
    return 0;
}
