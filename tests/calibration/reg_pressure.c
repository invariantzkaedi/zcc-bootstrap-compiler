/*
 * reg_pressure.c — D-30: Register Pressure Workload
 *
 * Stresses register allocator and spills by keeping 20+ variables live.
 */
#include <stdio.h>
#include <stdlib.h>

int run_register_pressure(int seed) {
    int a = seed * 3;
    int b = a + 7;
    int c = b * 11;
    int d = c - 13;
    int e = d ^ 17;
    int f = e + 19;
    int g = f * 23;
    int h = g - 29;
    int i = h ^ 31;
    int j = i + 37;
    int k = j * 41;
    int l = k - 43;
    int m = l ^ 47;
    int n = m + 53;
    int o = n * 59;
    int p = o - 61;
    int q = p ^ 67;
    int r = q + 71;
    int s = r * 73;
    int t = s - 79;
    
    /* Accumulate them in a way that requires all to be live at the end */
    int res = a + b;
    res -= c;
    res += d * e;
    res -= f;
    res += g;
    res -= h * i;
    res += j;
    res -= k;
    res += l * m;
    res -= n;
    res += o;
    res -= p * q;
    res += r;
    res -= s;
    res += t;
    return res;
}

int main(int argc, char **argv) {
    int seed = 42;
    if (argc > 1) {
        seed = atoi(argv[1]);
    }
    int result = run_register_pressure(seed);
    printf("result=%d\n", result);
    return 0;
}
