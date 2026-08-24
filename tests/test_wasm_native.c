/*
 * ZCC Native WebAssembly Verification Target
 * File: tests/test_wasm_native.c
 */

int add(int a, int b) {
    return a + b;
}

int sub(int a, int b) {
    return a - b;
}

int mul(int a, int b) {
    return a * b;
}

int factorial(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

int fib(int n) {
    if (n <= 0) return 0;
    if (n == 1) return 1;
    int a = 0;
    int b = 1;
    int i = 2;
    while (i <= n) {
        int c = a + b;
        a = b;
        b = c;
        i = i + 1;
    }
    return b;
}

int bitwise_ops(int a, int b) {
    int x = a ^ b;
    int y = (a & b) << 1;
    return x | y;
}

int main(void) {
    int r1 = add(20, 22);       /* 42 */
    int r2 = factorial(5);      /* 120 */
    int r3 = fib(10);           /* 55 */
    return r1 + r2 + r3;        /* 217 */
}
