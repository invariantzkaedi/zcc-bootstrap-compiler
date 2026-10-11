#include <stdint.h>
#include <stdarg.h>
int var_vla_sink(int count, int *ptr, int a1, int a2, int a3, int a4, int a5, int a6, ...) {
    va_list ap;
    va_start(ap, a6);
    int extra = va_arg(ap, int);
    va_end(ap);
    int sum = ptr[0] + ptr[1] + a1 + a2 + a3 + a4 + a5 + a6 + extra;
    return sum;
}
int main(void) {
    int n = 4;
    int dyn_arr[n];
    dyn_arr[0] = -6; dyn_arr[1] = 13; dyn_arr[2] = -12; dyn_arr[3] = 26;
    int res = var_vla_sink(n, dyn_arr, 1, 2, 3, 4, 5, 6, 7);
    return (uint8_t)(res & 0xFF);
}
