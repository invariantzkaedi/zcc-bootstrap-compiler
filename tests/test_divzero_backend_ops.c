#include <stdio.h>

int g_val = 100;

static double convert_and_div(int val, int divisor) {
    double d_val = (double)val;
    return d_val / divisor;
}

int main() {
    // 1. power-of-two unsigned division/modulo paths
    unsigned int u_val = 128;
    unsigned int u_div = u_val / 8;
    unsigned int u_mod = u_val % 8;

    // 2. power-of-two signed division/modulo paths
    int s_val = -128;
    int s_div = s_val / 8;

    // 3. Float conversion path (emit_cvtsi2fd)
    double d_res = convert_and_div(g_val, 2);

    // 4. Constant division & modulo by zero (proven at compile time)
    int div_zero = g_val / 0;
    int mod_zero = g_val % 0;

    printf("u_div=%u, u_mod=%u, s_div=%d, d_res=%f, div_zero=%d, mod_zero=%d\n",
           u_div, u_mod, s_div, d_res, div_zero, mod_zero);
    return 0;
}
