/* =========================================================================
 * tests/zcc_quad_isa_torture.c
 * ZCC Stratified 3-Tier Quad-ISA Compiler Torture & Differential Oracle TU
 *
 * Governing Rules: ZCC-EF-0 (Independent Verification Authority) & ZCC-EF-7
 * Source Equivalence Class:
 *   - Class I across ZCC x86-64, ZCC ARM64, ZCC RISC-V 64, and ZCC WASM32.
 *   - Class III vs GCC/Clang on Kernels 1-2 (__builtin_qpauli_* reference C
 *     shim); Class I identical C99 source on Kernels 3-10.
 *
 * Stratification:
 *   - Tier 1 (Default — Mandatory 7-Way Regression Gate):
 *       K1: zcc_quad_bitops_mix (popcount, clz, ctz, bswap32, qpauli_*)
 *       K2: zcc_steane_qec_recover (Steane [[7,1,3]] Fano CSS decoder)
 *       K3: zcc_rotor4d_norm_q12 (4D SO(4) double-isoclinic Q12 rotor)
 *       K4: zcc_dsp_harmonic_q15 (64-bit long long Q15 parabolic oscillator)
 *       K5: zcc_arena_canary_audit (stack frame red-zone canary 0x5A434321)
 *       K6: zcc_prime_step_q12 (ZKAEDI PRIME two-regime Q12 step)
 *   - Tier 2 (-DZCC_TORTURE_STRUCT_UNION — Layout & Sub-Word Aliasing Probe):
 *       K7: zcc_packed_misaligned_torture (__attribute__((packed)) FORENSIC_025)
 *       K8: zcc_union_pun_gvn_torture (union u32/u16/u8 GVN hazard E-LEARN-019)
 *   - Tier 3 (-DZCC_TORTURE_EDGE_ABI — Stack-Spill & Indirect Call Probe):
 *       K9: zcc_spill_9arg_torture (9-arg stack spill >6 SysV / >8 ARM64,RV64)
 *      K10: zcc_fnptr_alias_torture (indirect fnptr table + struct ptr alias)
 * ========================================================================= */

#if (defined(__GNUC__) || defined(__clang__)) && !defined(__ZCC__)
static int zcc_hw_qpauli_weight(unsigned int p) {
    unsigned int m = (p | (p >> 16)) & 0xFFFFu;
    return __builtin_popcount(m);
}
static int zcc_hw_qpauli_commute(unsigned int a, unsigned int b) {
    unsigned int anti = ((a & (b >> 16)) ^ ((a >> 16) & b)) & 0xFFFFu;
    return __builtin_popcount(anti) & 1;
}
static unsigned int zcc_hw_qpauli_mul(unsigned int a, unsigned int b) {
    return a ^ b;
}
static int zcc_hw_qpauli_phase(unsigned int a, unsigned int b) {
    unsigned int z1 = a >> 16;
    unsigned int z2 = b >> 16;
    unsigned int u = a & z2;
    unsigned int v = z1 & b;
    unsigned int anti = (u ^ v) & 0xFFFFu;
    unsigned int pos = ((u & (b ^ z1)) | (v & ~(a | z2))) & 0xFFFFu;
    unsigned int neg = anti ^ pos;
    return (__builtin_popcount(pos) - __builtin_popcount(neg)) & 3;
}
#else
#define zcc_hw_qpauli_weight(p)      __builtin_qpauli_weight(p)
#define zcc_hw_qpauli_commute(a, b)  __builtin_qpauli_commute((a), (b))
#define zcc_hw_qpauli_mul(a, b)      __builtin_qpauli_mul((a), (b))
#define zcc_hw_qpauli_phase(a, b)    __builtin_qpauli_phase((a), (b))
#endif

/* K1. Quad-ISA Bitops & 16-Qubit Symplectic Pauli Mixer */
unsigned int zcc_quad_bitops_mix(unsigned int x, unsigned int y) {
    unsigned int sx = x | 1u;
    unsigned int sy = y | 0x80000000u;
    int pc = __builtin_popcount(x ^ y);
    int lz = __builtin_clz(sy);
    int tz = __builtin_ctz(sx);
    unsigned int bs = __builtin_bswap32(x + 0x9E3779B9u);
    unsigned int pm = zcc_hw_qpauli_mul(x, y);
    int ph = zcc_hw_qpauli_phase(x, y);
    int wt = zcc_hw_qpauli_weight(pm);
    int cm = zcc_hw_qpauli_commute(x, y);
    return bs ^ (pm + (unsigned int)((pc << 12) | (lz << 7) | (tz << 3) | (ph << 1) | cm) + (unsigned int)(wt * 2654435761u));
}

/* K2. Steane [[7, 1, 3]] CSS Fano-Plane Syndrome Decoder & Recovery */
int zcc_steane_qec_recover(unsigned int err_pauli) {
    unsigned int gx0 = 0x00000055u;
    unsigned int gx1 = 0x00000066u;
    unsigned int gx2 = 0x00000078u;
    unsigned int gz0 = 0x00550000u;
    unsigned int gz1 = 0x00660000u;
    unsigned int gz2 = 0x00780000u;

    int syn_x = zcc_hw_qpauli_commute(err_pauli, gz0)
              | (zcc_hw_qpauli_commute(err_pauli, gz1) << 1)
              | (zcc_hw_qpauli_commute(err_pauli, gz2) << 2);
    int syn_z = zcc_hw_qpauli_commute(err_pauli, gx0)
              | (zcc_hw_qpauli_commute(err_pauli, gx1) << 1)
              | (zcc_hw_qpauli_commute(err_pauli, gx2) << 2);

    unsigned int rec = 0u;
    if (syn_x > 0 && syn_x <= 7) {
        rec = rec | (1u << (syn_x - 1));
    }
    if (syn_z > 0 && syn_z <= 7) {
        rec = rec | (1u << ((syn_z - 1) + 16));
    }
    unsigned int residual = zcc_hw_qpauli_mul(err_pauli, rec);
    int res_wt = zcc_hw_qpauli_weight(residual);
    return (syn_x << 8) | (syn_z << 4) | (res_wt & 15);
}

/* K3. 4D Hypercube SO(4) Double-Isoclinic Rotor Norm-Preserving Projection (Q12) */
int zcc_rotor4d_norm_q12(int x, int y, int z, int w, int cos_q12, int sin_q12) {
    int xr = (x * cos_q12 - w * sin_q12) >> 12;
    int wr = (x * sin_q12 + w * cos_q12) >> 12;
    int yr = (y * cos_q12 - z * sin_q12) >> 12;
    int zr = (y * sin_q12 + z * cos_q12) >> 12;
    return ((xr * xr) + (yr * yr) + (zr * zr) + (wr * wr)) >> 12;
}

/* K4. Deterministic Q15 Harmonic Oscillator Sample Generator */
int zcc_dsp_harmonic_q15(int phase_q15, int harmonic_gain_q8) {
    int p = phase_q15 & 0xFFFF;
    int half = p & 0x7FFF;
    long long u = (long long)half;
    int parabola = (int)((u * (32768LL - u)) >> 13);
    int fundamental = (p & 0x8000) ? -parabola : parabola;

    int p3 = (phase_q15 * 3) & 0xFFFF;
    int h3 = p3 & 0x7FFF;
    long long u3 = (long long)h3;
    int par3 = (int)((u3 * (32768LL - u3)) >> 13);
    int overtone = (p3 & 0x8000) ? -par3 : par3;

    int mixed = fundamental + ((overtone * harmonic_gain_q8) >> 10);
    if (mixed > 32767) mixed = 32767;
    if (mixed < -32767) mixed = -32767;
    return mixed;
}

/* K5. Linear Arena & Stack Red-Zone Canary Sentinel */
int zcc_arena_canary_audit(int blocks, int seed) {
    unsigned int arena[36];
    int i;
    int n = blocks;
    if (n < 1) n = 1;
    if (n > 32) n = 32;

    arena[0] = 0x5A434321u;
    arena[1] = 0xDEADBEEFu;
    arena[34] = 0xCAFEBABEu;
    arena[35] = 0x5A434321u;

    for (i = 0; i < 32; i++) arena[i + 2] = 0u;
    for (i = 0; i < n; i++) {
        arena[i + 2] = (unsigned int)(seed + i * 7919) ^ 0xA5A55A5Au;
    }

    if (arena[0] != 0x5A434321u || arena[1] != 0xDEADBEEFu) return -1;
    if (arena[34] != 0xCAFEBABEu || arena[35] != 0x5A434321u) return -2;

    unsigned int acc = 2166136261u;
    for (i = 0; i < n; i++) {
        acc = (acc ^ arena[i + 2]) * 16777619u;
    }
    return (int)(acc & 0x7FFFFFFFu);
}

/* K6. ZKAEDI PRIME Two-Regime Discrete Field Step (Q12 = 4096) */
int zcc_prime_step_q12(int h_base_q12, int h_prev_q12, int eta_q12, int scar_kick_q12) {
    int abs_h = h_prev_q12 < 0 ? -h_prev_q12 : h_prev_q12;
    int num = 3 * h_prev_q12;
    int den = 10 + ((3 * abs_h) >> 11);
    int sig_q12 = 2048 + (num / den);
    if (sig_q12 < 0) sig_q12 = 0;
    if (sig_q12 > 4096) sig_q12 = 4096;

    long long feedback = ((long long)eta_q12 * (long long)h_prev_q12) >> 12;
    feedback = (feedback * (long long)sig_q12) >> 12;
    return h_base_q12 + scar_kick_q12 + (int)feedback;
}

#ifdef ZCC_TORTURE_STRUCT_UNION
/* K7. Packed Misaligned Struct Torture (FORENSIC_025_PACKED_ALIGNMENT) */
struct __attribute__((packed)) ZccPackedDescriptor {
    unsigned char tag;       /* offset 0 */
    unsigned int base32;     /* offset 1 (unaligned!) */
    unsigned short limit16;  /* offset 5 (unaligned!) */
    unsigned char flags;     /* offset 7 */
    unsigned int crc32;      /* offset 8 */
};

unsigned int zcc_packed_misaligned_torture(unsigned int seed) {
    struct ZccPackedDescriptor desc;
    desc.tag = (unsigned char)((seed & 0x7Fu) | 0x40u);
    desc.base32 = seed ^ 0x13579BDFu;
    desc.limit16 = (unsigned short)((seed >> 11) ^ 0xBEEFu);
    desc.flags = (unsigned char)((seed >> 3) & 0xFFu);
    desc.crc32 = (desc.base32 + (unsigned int)desc.limit16 * 65599u) ^ (unsigned int)desc.tag;
    return ((unsigned int)sizeof(struct ZccPackedDescriptor) << 24)
         ^ desc.crc32
         ^ ((unsigned int)desc.flags << 8)
         ^ (desc.base32 >> 5);
}

/* K8. Union Byte/Halfword Type-Punning & GVN Store-to-Load Forwarding Hazard */
union ZccPunWord {
    unsigned int u32;
    unsigned short u16[2];
    unsigned char b[4];
};

unsigned int zcc_union_pun_gvn_torture(unsigned int word) {
    union ZccPunWord u;
    u.u32 = word;
    u.b[1] = (unsigned char)(u.b[1] ^ 0x5Au);
    u.u16[1] = (unsigned short)(u.u16[1] + 0x1337u);
    u.b[0] = (unsigned char)(u.b[0] + u.b[3]);
    return u.u32 ^ ((unsigned int)u.u16[0] << 16) ^ (unsigned int)u.b[2];
}
#endif

#ifdef ZCC_TORTURE_EDGE_ABI
/* K9. 9-Argument Stack-Spill Calling Convention & Caller-Slot Preservation */
static unsigned int zcc_nine_arg_callee(
    unsigned int a0, unsigned int a1, unsigned int a2,
    unsigned int a3, unsigned int a4, unsigned int a5,
    unsigned int a6, unsigned int a7, unsigned int a8
) {
    unsigned int lo = (a0 * 3u) + (a1 * 5u) + (a2 * 7u) + (a3 * 11u) + (a4 * 13u);
    unsigned int hi = (a5 * 17u) ^ (a6 * 19u) ^ (a7 * 23u) ^ (a8 * 29u);
    return (lo ^ ((hi << 5) | (hi >> 27))) + a8 - a6;
}

unsigned int zcc_spill_9arg_torture(unsigned int s) {
    unsigned int caller_guard = s ^ 0xCAFE1234u;
    unsigned int r1 = zcc_nine_arg_callee(
        s + 1u, s + 2u, s + 3u, s + 4u, s + 5u, s + 6u,
        s ^ 0x11111111u, s ^ 0x22222222u, s ^ 0x33333333u
    );
    unsigned int r2 = zcc_nine_arg_callee(
        r1, caller_guard, s + 9u, s + 10u, s + 11u, s + 12u,
        r1 ^ 0x44444444u, caller_guard ^ 0x55555555u, s ^ 0x66666666u
    );
    if (caller_guard != (s ^ 0xCAFE1234u)) return 0xBAD09999u;
    return r1 ^ r2 ^ caller_guard;
}

/* K10. Indirect Function-Pointer Dispatch & Pointer Aliasing Torture */
static int zcc_op_affine(int a, int b) {
    return (a * 13) - (b * 7) + 0x10203;
}
static int zcc_op_xorshift(int a, int b) {
    unsigned int ua = (unsigned int)a ^ ((unsigned int)b * 2246822519u);
    ua ^= ua >> 13;
    ua ^= ua << 7;
    return (int)(ua & 0x7FFFFFFFu);
}

struct ZccAccBox {
    int acc;
    int tag;
};

unsigned int zcc_fnptr_alias_torture(int base, int sel) {
    int (*table[2])(int, int);
    struct ZccAccBox box;
    int *alias_ptr = &box.acc;
    table[0] = zcc_op_affine;
    table[1] = zcc_op_xorshift;
    box.acc = base;
    box.tag = sel ^ 0x55AA;
    *alias_ptr = table[sel & 1](box.acc, box.tag);
    *alias_ptr += table[(sel + 1) & 1](*alias_ptr, base ^ 0x33);
    return (unsigned int)box.acc ^ ((unsigned int)box.tag << 12);
}
#endif

static void fnv1a64_mix_u32(unsigned long long *h, unsigned int val) {
    unsigned long long hash = *h;
    int b;
    for (b = 0; b < 4; b++) {
        hash ^= (unsigned long long)((val >> (b * 8)) & 0xFFu);
        hash *= 1099511628211ULL;
    }
    *h = hash;
}

static unsigned long long zcc_compute_campaign_fnv1a64(int rounds) {
    unsigned long long h = 14695981039346656037ULL;
    int idx;
    for (idx = 1; idx <= rounds; idx++) {
        unsigned int a = ((unsigned int)idx * 0x9E3779B1u) ^ 0x00150007u;
        unsigned int b = ((unsigned int)idx * 0x85EBCA77u) ^ 0x000B0013u;
        int q = idx % 7;
        unsigned int err = (1u << q) | (1u << (q + 16));
        fnv1a64_mix_u32(&h, zcc_quad_bitops_mix(a, b));
        fnv1a64_mix_u32(&h, (unsigned int)zcc_steane_qec_recover(err));
        fnv1a64_mix_u32(&h, (unsigned int)zcc_rotor4d_norm_q12(4096, -4096, 4096, -4096, 3547, 2048));
        fnv1a64_mix_u32(&h, (unsigned int)(zcc_dsp_harmonic_q15((idx * 1024) & 0xFFFF, 128 + (idx & 63)) + 32768));
        fnv1a64_mix_u32(&h, (unsigned int)zcc_arena_canary_audit(4 + (idx % 28), 1337 + idx * 31));
        fnv1a64_mix_u32(&h, (unsigned int)zcc_prime_step_q12(1000 + idx * 16, -1024 + idx * 64, 1638, 0));
#ifdef ZCC_TORTURE_STRUCT_UNION
        fnv1a64_mix_u32(&h, zcc_packed_misaligned_torture(a));
        fnv1a64_mix_u32(&h, zcc_union_pun_gvn_torture(b));
#endif
#ifdef ZCC_TORTURE_EDGE_ABI
        fnv1a64_mix_u32(&h, zcc_spill_9arg_torture(a ^ b));
        fnv1a64_mix_u32(&h, zcc_fnptr_alias_torture((int)(a & 0xFFFFu), idx));
#endif
    }
    return h;
}

unsigned int zcc_run_campaign_digest_lo(int rounds) {
    unsigned long long h = zcc_compute_campaign_fnv1a64(rounds);
    return (unsigned int)(h & 0xFFFFFFFFULL);
}

unsigned int zcc_run_campaign_digest_hi(int rounds) {
    unsigned long long h = zcc_compute_campaign_fnv1a64(rounds);
    return (unsigned int)((h >> 32) & 0xFFFFFFFFULL);
}

#ifdef ZCC_HOST_STDOUT
int putchar(int c);
static void emit_hex32(char *dst, unsigned int v) {
    const char *hex = "0123456789abcdef";
    int i;
    for (i = 0; i < 8; i++) {
        dst[7 - i] = hex[(v >> (i * 4)) & 0xFu];
    }
}
#endif

#ifndef ZCC_CAMPAIGN_ROUNDS
#define ZCC_CAMPAIGN_ROUNDS 64
#endif

int main(void) {
    unsigned int hi = zcc_run_campaign_digest_hi(ZCC_CAMPAIGN_ROUNDS);
    unsigned int lo = zcc_run_campaign_digest_lo(ZCC_CAMPAIGN_ROUNDS);
#ifdef ZCC_HOST_STDOUT
    char line[28];
    int k;
    line[0] = 'F'; line[1] = 'N'; line[2] = 'V'; line[3] = '1';
    line[4] = 'A'; line[5] = '6'; line[6] = '4'; line[7] = '=';
    line[8] = '0'; line[9] = 'x';
    emit_hex32(&line[10], hi);
    emit_hex32(&line[18], lo);
    line[26] = '\n';
    for (k = 0; k < 27; k++) {
        putchar((unsigned char)line[k]);
    }
#endif
    if (hi == 0u && lo == 0u) return 1;
    return 0;
}

