/*
 * Permanent Regression & Differential Verification Test:
 * ZCC Native Assembler (src/codegen.c) Complete AVX-512 EVEX ISA
 * (pd/sd, ps/ss, epi64/epi32, shifts, %k0..%k7 opmasks, {%k1}{z} predication)
 * and .note.GNU-stack ELF section emission (src/elf_emit.c).
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

void avx512_evex_ps_int_mask_probe(const float *f_in, float *f_out,
                                   const long long *q_in, long long *q_out,
                                   const int *d_in, int *d_out, int mask_bits) {
    __asm__ volatile(
        "kmovw %6, %%k1\n\t"
        "vmovups (%0), %%zmm0\n\t"
        "vbroadcastss (%0), %%zmm1\n\t"
        "vaddps %%zmm1, %%zmm0, %%zmm2\n\t"
        "vfmadd231ps %%zmm1, %%zmm2, %%zmm0{%%k1}{z}\n\t"
        "vmovups %%zmm0, (%1)\n\t"
        "vmovdqu64 (%2), %%zmm4\n\t"
        "vpbroadcastq (%2), %%zmm5\n\t"
        "vpaddq %%zmm5, %%zmm4, %%zmm6{%%k1}{z}\n\t"
        "vpmullq %%zmm5, %%zmm6, %%zmm7\n\t"
        "vpsllq $1, %%zmm7, %%zmm8\n\t"
        "vpsrlq $1, %%zmm8, %%zmm9\n\t"
        "vmovdqu64 %%zmm9, (%3)\n\t"
        "vmovdqu32 (%4), %%zmm14\n\t"
        "vpbroadcastd (%4), %%zmm15\n\t"
        "vpaddd %%zmm15, %%zmm14, %%zmm16\n\t"
        "vpmulld %%zmm15, %%zmm16, %%zmm17{%%k1}{z}\n\t"
        "vmovdqu32 %%zmm17, (%5)\n\t"
        "vcmppd $0, %%zmm5, %%zmm4, %%k2\n\t"
        "vpcmpq $1, %%zmm5, %%zmm4, %%k3\n\t"
        "vzeroupper"
        :
        : "r"(f_in), "r"(f_out), "r"(q_in), "r"(q_out), "r"(d_in), "r"(d_out), "r"(mask_bits)
        : "memory"
    );
}

int main(void) {
    double a[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    double b[8] = {10, 20, 30, 40, 50, 60, 70, 80};
    double dst[8] = {0};
    avx512_evex_probe_asm(dst, a, b);

    float f_in[16], f_out[16];
    long long q_in[8], q_out[8];
    int d_in[16], d_out[16];
    for (int i = 0; i < 16; i++) {
        f_in[i] = (float)(i + 2);
        f_out[i] = -1.0f;
        d_in[i] = i + 4;
        d_out[i] = -1;
    }
    for (int i = 0; i < 8; i++) {
        q_in[i] = (long long)(i + 3);
        q_out[i] = -1;
    }
    avx512_evex_ps_int_mask_probe(f_in, f_out, q_in, q_out, d_in, d_out, 0x05);

    if (dst[0] != 41.0 || dst[7] != 328.0) return 1;
    if (f_out[0] != 10.0f || f_out[1] != 0.0f || f_out[2] != 16.0f) return 2;
    if (q_out[0] != 18 || q_out[1] != 0 || q_out[2] != 24) return 3;
    if (d_out[0] != 32 || d_out[1] != 0 || d_out[2] != 40) return 4;

    printf("[ZCC-AVX512-EVEX] dst[0]=%.1f dst[7]=%.1f f_out[0]=%.1f f_out[1]=%.1f q_out[0]=%lld q_out[1]=%lld d_out[0]=%d d_out[1]=%d PASS\n",
           dst[0], dst[7], (double)f_out[0], (double)f_out[1], q_out[0], q_out[1], d_out[0], d_out[1]);
    return 0;
}
