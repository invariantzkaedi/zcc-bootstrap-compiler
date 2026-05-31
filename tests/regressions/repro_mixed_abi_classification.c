/* tests/regressions/repro_mixed_abi_classification.c */
#include <stdio.h>

typedef struct {
    int i_val;
    float f_val;
} Mix;

/* Mix is passed entirely in registers: i_val in %rdi (low 32-bits), f_val in %xmm0 */
Mix process_mixed(Mix m, int scale) {
    Mix res;
    res.i_val = m.i_val * scale;
    res.f_val = m.f_val + (float)scale;
    return res;
}

int main(void) {
    Mix m;
    m.i_val = 42;
    m.f_val = 3.14f;

    Mix out = process_mixed(m, 2);
    /* Expecting exactly i_val=84, f_val=5.14 */
    printf("i_val=%d f_val=%.2f\n", out.i_val, out.f_val);
    return 0;
}
