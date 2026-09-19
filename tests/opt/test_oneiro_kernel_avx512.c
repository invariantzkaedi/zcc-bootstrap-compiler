/* ========================================================================= */
/* Test Specimen: Radix-2 NTT Butterfly & AVX Catalyst Hotspot Loop         */
/* Target: ZCC-ONEIRO-OPT Evolutionary Assembly Superoptimizer               */
/* ========================================================================= */

#define TORUS_MODULUS 12289

void ntt_butterfly_catalyst(int *a, int *b, int twiddle, int n) {
    int i;
    for (i = 0; i < n; i++) {
        int u = a[i];
        int v = b[i];
        
        /* Redundant reload / register swap candidate */
        long temp = (long)v * (long)twiddle;
        int t = (int)(temp % TORUS_MODULUS);
        
        /* Dead write to intermediate accumulator */
        int dead_accum = u + v + 42;
        (void)dead_accum;
        
        int new_a = u + t;
        if (new_a >= TORUS_MODULUS) new_a -= TORUS_MODULUS;
        
        int new_b = u - t;
        if (new_b < 0) new_b += TORUS_MODULUS;
        
        a[i] = new_a;
        b[i] = new_b;
    }
}

int main(void) {
    int a[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    int b[16] = {16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1};
    ntt_butterfly_catalyst(a, b, 317, 16);
    return 0;
}
