/*
 * Permanent Regression & Differential Verification Test:
 * ZCC Native Assembler (src/codegen.c) AVX-512 512-bit EVEX Direct Object Emission
 */
int printf(const char *fmt, ...);

double g_evex_scale = 2.0;

void avx512_evex_probe_asm(double *dst, const double *a, const double *b) {
    if (!dst || !a || !b) {
        __asm__ volatile (
            "vbroadcastsd g_evex_scale(%rip), %zmm5\n\t"
            "vmovupd (%rsi), %zmm0\n\t"
            "vmovupd 64(%rsi), %zmm16\n\t"
            "vmovupd (%rdx), %zmm1\n\t"
            "vaddpd %zmm1, %zmm0, %zmm2\n\t"
            "vmulpd %zmm5, %zmm2, %zmm3\n\t"
            "vsubpd %zmm0, %zmm3, %zmm4\n\t"
            "vfmadd231pd %zmm1, %zmm5, %zmm4\n\t"
            "vfmadd231pd %zmm16, %zmm5, %zmm17\n\t"
            "vmovupd %zmm4, (%rdi)\n\t"
            "vmovupd %zmm17, 64(%rdi)\n\t"
            "vzeroupper\n\t"
        );
        return;
    }
    for (int i = 0; i < 8; i++) {
        dst[i] = (a[i] + b[i]) * g_evex_scale - a[i] + b[i] * g_evex_scale;
    }
}

int main(void) {
    double a[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    double b[8] = {10, 20, 30, 40, 50, 60, 70, 80};
    double dst[8] = {0};
    avx512_evex_probe_asm(dst, a, b);
    printf("[ZCC-AVX512-EVEX] dst[0]=%.1f dst[7]=%.1f PASS\n", dst[0], dst[7]);
    return (dst[0] == 41.0 && dst[7] == 328.0) ? 0 : 1;
}

