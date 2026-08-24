#include <stdarg.h>

// 8+ argument SystemV ABI stack spill gauntlet
int pass_many_args(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10) {
    return a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + a9 + a10;
}

// Struct return (sret hidden pointer)
struct BigStruct {
    long long data[8];
};

struct BigStruct make_big_struct(long long val) {
    struct BigStruct res;
    for (int i = 0; i < 8; i++) {
        res.data[i] = val * (i + 1);
    }
    return res;
}

// Variadic function ABI
double sum_variadic(int count, ...) {
    va_list ap;
    va_start(ap, count);
    double total = 0.0;
    for (int i = 0; i < count; i++) {
        total += va_arg(ap, double);
    }
    va_end(ap);
    return total;
}

// 64-bit shifts, modulo, signed/unsigned divisions, floating point math
double complex_math_ops(double x, double y, long long a, unsigned long long b) {
    double f_add = x + y;
    double f_sub = x - y;
    double f_mul = x * y;
    double f_div = (y != 0.0) ? (x / y) : 0.0;
    
    long long s_div = (a != 0) ? (a / 3) : 0;
    long long s_mod = (a != 0) ? (a % 7) : 0;
    unsigned long long u_div = (b != 0) ? (b / 5) : 0;
    unsigned long long u_mod = (b != 0) ? (b % 11) : 0;
    
    long long bit_ops = (a << 5) ^ (a >> 3) | (~a & 0xFFFF);
    
    return f_add + f_sub + f_mul + f_div + s_div + s_mod + u_div + u_mod + bit_ops;
}

int test_codegen_main() {
    int r1 = pass_many_args(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    struct BigStruct bs = make_big_struct(100);
    double r2 = sum_variadic(4, 1.1, 2.2, 3.3, 4.4);
    double r3 = complex_math_ops(10.5, 2.5, 1000LL, 5000ULL);
    return r1 + (int)bs.data[0] + (int)r2 + (int)r3;
}