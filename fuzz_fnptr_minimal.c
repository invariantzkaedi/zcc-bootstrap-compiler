/* fuzz_fnptr_minimal.c
 * Minimal reproducer: function pointer indirect call → ZLD *%r10 bug
 * Triggered by recursive dynamic fuzzer fn-ptr dispatch injection path
 */
int printf(const char *fmt, ...);

static int f2(int a, int b, int c);
static int f1(int a, int b, int c);

static int f1(int a, int b, int c) {
    int acc = 0;
    /* Mutual-recursion ping via function pointer — the triggering pattern */
    { typedef int (*fp_t)(int,int,int); fp_t _fp = f2;
      if (a > 2) acc += _fp(a - 2, c, b & 15); }
    return a + b + c + acc;
}

static int f2(int a, int b, int c) {
    int acc = 0;
    { typedef int (*fp_t)(int,int,int); fp_t _fp = f1;
      if (a > 2) acc += _fp(a - 2, c, b & 15); }
    return a + b + c + acc;
}

int main(void) {
    long checksum = 0;
    checksum += (long)f1(6, 2, 3);
    checksum += (long)f2(6, 2, 3);
    printf("CHECKSUM=%ld\n", checksum);
    return 0;
}
