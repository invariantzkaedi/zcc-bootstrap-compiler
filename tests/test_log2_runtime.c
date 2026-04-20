#include <stdio.h>
static int is_power_of_2_val(long long val) {
    return val > 0 && (val & (val - 1)) == 0;
}
static int log2_of(long long val) {
    int n = 0;
    while (val > 1) {
        val = val >> 1;
        n = n + 1;
    }
    return n;
}
int main() {
    printf(" log2_of 8 = %d\n\,