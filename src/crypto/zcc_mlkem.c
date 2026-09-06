/* ================================================================ */
/* ZCC CRYPTOGRAPHIC SUBSTRATE: NIST FIPS 203 ML-KEM-768            */
/* ================================================================ */
/* File: src/crypto/zcc_mlkem.c                                     */
/* Reference Implementation: C99, FIPS 203 Compliant, Constant-Time */
/* ================================================================ */

#include "include/zcc_mlkem.h"
#include <string.h>

/* --- FIPS 202: Keccak-f[1600] and SHAKE/SHA-3 Functions --- */

static const uint64_t keccakf_rndc[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
    0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,
    0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,
    0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,
    0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL
};

static const unsigned int keccakf_rotc[24] = {
    1,  3,  6,  10, 15, 21, 28, 36, 45, 55, 2,  14,
    27, 41, 56, 8,  25, 43, 62, 18, 39, 61, 20, 44
};

static const unsigned int keccakf_piln[24] = {
    10, 7,  11, 17, 18, 3, 5,  16, 8,  21, 24, 4,
    15, 23, 19, 13, 12, 2, 20, 14, 22, 9,  6,  1
};

static inline uint64_t rotl64(uint64_t x, unsigned int n) {
    return (x << n) | (x >> (64 - n));
}

static void keccak_f1600(uint64_t s[25]) {
    for (int round = 0; round < 24; round++) {
        uint64_t bc[5], c[5], d[5];
        /* Theta */
        for (int i = 0; i < 5; i++)
            bc[i] = s[i] ^ s[i + 5] ^ s[i + 10] ^ s[i + 15] ^ s[i + 20];
        for (int i = 0; i < 5; i++)
            d[i] = bc[(i + 4) % 5] ^ rotl64(bc[(i + 1) % 5], 1);
        for (int i = 0; i < 25; i++)
            s[i] ^= d[i % 5];

        /* Rho and Pi */
        uint64_t cur = s[1];
        for (int i = 0; i < 24; i++) {
            unsigned int j = keccakf_piln[i];
            uint64_t tmp = s[j];
            s[j] = rotl64(cur, keccakf_rotc[i]);
            cur = tmp;
        }

        /* Chi */
        for (int j = 0; j < 25; j += 5) {
            for (int i = 0; i < 5; i++) c[i] = s[j + i];
            for (int i = 0; i < 5; i++)
                s[j + i] ^= (~c[(i + 1) % 5]) & c[(i + 2) % 5];
        }

        /* Iota */
        s[0] ^= keccakf_rndc[round];
    }
}

static void keccak_init(zcc_keccak_state *state, size_t rate) {
    memset(state->s, 0, sizeof(state->s));
    state->pos = 0;
    state->rate = rate;
}

static void keccak_absorb(zcc_keccak_state *state, const uint8_t *in, size_t inlen) {
    while (inlen > 0) {
        size_t take = state->rate - state->pos;
        if (take > inlen) take = inlen;
        uint8_t *s8 = (uint8_t *)state->s;
        for (size_t i = 0; i < take; i++) s8[state->pos + i] ^= in[i];
        state->pos += take;
        in += take;
        inlen -= take;
        if (state->pos == state->rate) {
            keccak_f1600(state->s);
            state->pos = 0;
        }
    }
}

static void keccak_finalize(zcc_keccak_state *state, uint8_t domain) {
    uint8_t *s8 = (uint8_t *)state->s;
    s8[state->pos] ^= domain;
    s8[state->rate - 1] ^= 0x80;
    keccak_f1600(state->s);
    state->pos = 0;
}

static void keccak_squeeze(zcc_keccak_state *state, uint8_t *out, size_t outlen) {
    while (outlen > 0) {
        if (state->pos == state->rate) {
            keccak_f1600(state->s);
            state->pos = 0;
        }
        size_t take = state->rate - state->pos;
        if (take > outlen) take = outlen;
        uint8_t *s8 = (uint8_t *)state->s;
        memcpy(out, s8 + state->pos, take);
        state->pos += take;
        out += take;
        outlen -= take;
    }
}

void zcc_shake128_init(zcc_keccak_state *state) { keccak_init(state, 168); }
void zcc_shake128_absorb(zcc_keccak_state *state, const uint8_t *in, size_t inlen) { keccak_absorb(state, in, inlen); }
void zcc_shake128_finalize(zcc_keccak_state *state) { keccak_finalize(state, 0x1F); }
void zcc_shake128_squeeze(zcc_keccak_state *state, uint8_t *out, size_t outlen) { keccak_squeeze(state, out, outlen); }

void zcc_shake256_init(zcc_keccak_state *state) { keccak_init(state, 136); }
void zcc_shake256_absorb(zcc_keccak_state *state, const uint8_t *in, size_t inlen) { keccak_absorb(state, in, inlen); }
void zcc_shake256_finalize(zcc_keccak_state *state) { keccak_finalize(state, 0x1F); }
void zcc_shake256_squeeze(zcc_keccak_state *state, uint8_t *out, size_t outlen) { keccak_squeeze(state, out, outlen); }

void zcc_shake256(uint8_t *out, size_t outlen, const uint8_t *in, size_t inlen) {
    zcc_keccak_state s;
    zcc_shake256_init(&s);
    zcc_shake256_absorb(&s, in, inlen);
    zcc_shake256_finalize(&s);
    zcc_shake256_squeeze(&s, out, outlen);
}

void zcc_sha3_256(uint8_t out[32], const uint8_t *in, size_t inlen) {
    zcc_keccak_state s;
    keccak_init(&s, 136);
    keccak_absorb(&s, in, inlen);
    keccak_finalize(&s, 0x06);
    keccak_squeeze(&s, out, 32);
}

void zcc_sha3_512(uint8_t out[64], const uint8_t *in, size_t inlen) {
    zcc_keccak_state s;
    keccak_init(&s, 72);
    keccak_absorb(&s, in, inlen);
    keccak_finalize(&s, 0x06);
    keccak_squeeze(&s, out, 64);
}

/* --- Modular Arithmetic and Number Theoretic Transform (NTT) --- */

#define MONT 2285  /* 2^16 mod 3329 */
#define QINV -3327 /* q^-1 mod 2^16 */

int16_t zcc_montgomery_reduce(int32_t a) {
    int16_t t = (int16_t)((int64_t)a * QINV);
    int32_t u = (a - (int32_t)t * MLKEM_Q) >> 16;
    return (int16_t)u;
}

int16_t zcc_barrett_reduce(int16_t a) {
    int32_t t = ((int32_t)a * 20159 + (1l << 25)) >> 26;
    t *= MLKEM_Q;
    return a - (int16_t)t;
}

/* FIPS 203 Bit-Reversed Montgomery Twiddle Factors (zetas[128]) */
static const int16_t zetas[128] = {
    -1044,  -758,  -359, -1517,  1493,  1422,   287,   202,
     -171,   622,  1577,   182,   962, -1202, -1474,  1468,
      573, -1325,   264,   383,  -829,  1458, -1602,  -130,
     -681,  1017,   732,   608, -1542,   411,  -205, -1571,
     1223,   652,  -552,  1015, -1293,  1491,  -282, -1544,
      516,    -8,  -320,  -666, -1618, -1162,   126,  1469,
     -853,   -90,  -271,   830,   107, -1421,  -247,  -951,
     -398,   961, -1508,  -725,   448, -1065,   677, -1275,
    -1103,   430,   555,   843, -1251,   871,  1550,   105,
      422,   587,   177,  -235,  -291,  -460,  1574,  1653,
     -246,   778,  1159,  -147,  -777,  1483,  -602,  1119,
    -1590,   644,  -872,   349,   418,   329,  -156,   -75,
      817,  1097,   603,   610,  1322, -1285, -1465,   384,
    -1215,  -136,  1218, -1335,  -874,   220, -1187, -1659,
    -1185, -1530, -1278,   794, -1510,  -854,  -870,   478,
     -108,  -308,   996,   991,   958, -1460,  1522,  1628
};

void zcc_ntt(int16_t r[256]) {
    int k = 1;
    for (int len = 128; len >= 2; len >>= 1) {
        for (int start = 0; start < 256; start += (len << 1)) {
            int16_t zeta = zetas[k++];
            for (int j = start; j < start + len; j++) {
                int16_t t = zcc_montgomery_reduce((int32_t)zeta * r[j + len]);
                r[j + len] = r[j] - t;
                r[j] = r[j] + t;
            }
        }
    }
}

void zcc_invntt(int16_t r[256]) {
    int k = 127;
    for (int len = 2; len <= 128; len <<= 1) {
        for (int start = 0; start < 256; start += (len << 1)) {
            int16_t zeta = zetas[k--];
            for (int j = start; j < start + len; j++) {
                int16_t t = r[j];
                r[j] = zcc_barrett_reduce(t + r[j + len]);
                r[j + len] = zcc_montgomery_reduce((int32_t)zeta * (r[j + len] - t));
            }
        }
    }
    /* Multiply by Montgomery factor of 128^-1 mod 3329: f = 1441 = (128^-1 * R^2) mod q */
    for (int j = 0; j < 256; j++) {
        r[j] = zcc_montgomery_reduce((int32_t)r[j] * 1441);
    }
}

static void poly_tomont(zcc_poly *r) {
    const int16_t f = 1353; /* (1ULL << 32) % 3329 = R^2 mod q */
    for (int i = 0; i < MLKEM_N; i++) {
        r->coeffs[i] = zcc_montgomery_reduce((int32_t)r->coeffs[i] * f);
    }
}

void zcc_poly_basemul_montgomery(int16_t r[2], const int16_t a[2], const int16_t b[2], int16_t zeta) {
    int16_t t0 = zcc_montgomery_reduce((int32_t)a[1] * b[1]);
    t0 = zcc_montgomery_reduce((int32_t)t0 * zeta);
    t0 += zcc_montgomery_reduce((int32_t)a[0] * b[0]);

    int16_t t1 = zcc_montgomery_reduce((int32_t)a[0] * b[1]);
    t1 += zcc_montgomery_reduce((int32_t)a[1] * b[0]);

    r[0] = t0;
    r[1] = t1;
}

static void poly_basemul(zcc_poly *r, const zcc_poly *a, const zcc_poly *b) {
    for (int i = 0; i < MLKEM_N / 4; i++) {
        zcc_poly_basemul_montgomery(&r->coeffs[4 * i],
                                    &a->coeffs[4 * i],
                                    &b->coeffs[4 * i],
                                    zetas[64 + i]);
        zcc_poly_basemul_montgomery(&r->coeffs[4 * i + 2],
                                    &a->coeffs[4 * i + 2],
                                    &b->coeffs[4 * i + 2],
                                    -zetas[64 + i]);
    }
}

static void poly_reduce(zcc_poly *r) {
    for (int i = 0; i < MLKEM_N; i++)
        r->coeffs[i] = zcc_barrett_reduce(r->coeffs[i]);
}

static void poly_add(zcc_poly *r, const zcc_poly *a, const zcc_poly *b) {
    for (int i = 0; i < MLKEM_N; i++)
        r->coeffs[i] = a->coeffs[i] + b->coeffs[i];
}

static void poly_sub(zcc_poly *r, const zcc_poly *a, const zcc_poly *b) {
    for (int i = 0; i < MLKEM_N; i++)
        r->coeffs[i] = a->coeffs[i] - b->coeffs[i];
}

/* --- Sampling: PRF & Centered Binomial Distribution (CBD2) --- */

static uint32_t load32_littleendian(const uint8_t x[4]) {
    return (uint32_t)x[0] | ((uint32_t)x[1] << 8) | ((uint32_t)x[2] << 16) | ((uint32_t)x[3] << 24);
}

static void cbd2(zcc_poly *r, const uint8_t buf[128]) {
    for (int i = 0; i < MLKEM_N / 8; i++) {
        uint32_t t = load32_littleendian(buf + 4 * i);
        uint32_t d = t & 0x55555555;
        d += (t >> 1) & 0x55555555;
        for (int j = 0; j < 8; j++) {
            int16_t a = (d >> (4 * j)) & 0x3;
            int16_t b = (d >> (4 * j + 2)) & 0x3;
            r->coeffs[8 * i + j] = a - b;
        }
    }
}

static void prf_cbd2(zcc_poly *r, const uint8_t seed[32], uint8_t nonce) {
    uint8_t extseed[33];
    memcpy(extseed, seed, 32);
    extseed[32] = nonce;
    uint8_t buf[128];
    zcc_shake256(buf, 128, extseed, 33);
    cbd2(r, buf);
}

/* --- Matrix Generation A from Seed rho via Rejection Sampling --- */

static void gen_matrix_entry(zcc_poly *a, const uint8_t rho[32], uint8_t i, uint8_t j) {
    uint8_t extseed[34];
    memcpy(extseed, rho, 32);
    extseed[32] = j;
    extseed[33] = i;

    zcc_keccak_state state;
    zcc_shake128_init(&state);
    zcc_shake128_absorb(&state, extseed, 34);
    zcc_shake128_finalize(&state);

    uint8_t buf[504];
    zcc_shake128_squeeze(&state, buf, sizeof(buf));

    int count = 0;
    int pos = 0;
    while (count < MLKEM_N) {
        if (pos + 3 > (int)sizeof(buf)) {
            zcc_shake128_squeeze(&state, buf, sizeof(buf));
            pos = 0;
        }
        uint16_t val0 = ((buf[pos + 0] >> 0) | ((uint16_t)buf[pos + 1] << 8)) & 0x0FFF;
        uint16_t val1 = ((buf[pos + 1] >> 4) | ((uint16_t)buf[pos + 2] << 4)) & 0x0FFF;
        pos += 3;

        if (val0 < MLKEM_Q) a->coeffs[count++] = val0;
        if (count < MLKEM_N && val1 < MLKEM_Q) a->coeffs[count++] = val1;
    }
}

/* --- Serialization: ByteEncode / ByteDecode / Compress / Decompress --- */

static void poly_tobytes(uint8_t r[384], const zcc_poly *a) {
    for (int i = 0; i < MLKEM_N / 2; i++) {
        uint16_t t0 = a->coeffs[2 * i];
        t0 += ((int16_t)t0 >> 15) & MLKEM_Q;
        uint16_t t1 = a->coeffs[2 * i + 1];
        t1 += ((int16_t)t1 >> 15) & MLKEM_Q;
        r[3 * i + 0] = (uint8_t)(t0 >> 0);
        r[3 * i + 1] = (uint8_t)((t0 >> 8) | (t1 << 4));
        r[3 * i + 2] = (uint8_t)(t1 >> 4);
    }
}

static void poly_frombytes(zcc_poly *r, const uint8_t a[384]) {
    for (int i = 0; i < MLKEM_N / 2; i++) {
        r->coeffs[2 * i + 0] = (int16_t)((a[3 * i + 0] >> 0) | ((uint16_t)a[3 * i + 1] << 8)) & 0xFFF;
        r->coeffs[2 * i + 1] = (int16_t)((a[3 * i + 1] >> 4) | ((uint16_t)a[3 * i + 2] << 4)) & 0xFFF;
    }
}

static void poly_compress_du10(uint8_t r[320], const zcc_poly *a) {
    uint16_t t[8];
    for (int i = 0; i < MLKEM_N / 8; i++) {
        for (int j = 0; j < 8; j++) {
            int32_t val = a->coeffs[8 * i + j];
            val += (val >> 31) & MLKEM_Q;
            t[j] = (uint16_t)((((uint64_t)val << 10) + 1664) / MLKEM_Q) & 0x3FF;
        }
        r[10 * i + 0] = (uint8_t)(t[0] >> 0);
        r[10 * i + 1] = (uint8_t)((t[0] >> 8) | (t[1] << 2));
        r[10 * i + 2] = (uint8_t)((t[1] >> 6) | (t[2] << 4));
        r[10 * i + 3] = (uint8_t)((t[2] >> 4) | (t[3] << 6));
        r[10 * i + 4] = (uint8_t)(t[3] >> 2);
        r[10 * i + 5] = (uint8_t)(t[4] >> 0);
        r[10 * i + 6] = (uint8_t)((t[4] >> 8) | (t[5] << 2));
        r[10 * i + 7] = (uint8_t)((t[5] >> 6) | (t[6] << 4));
        r[10 * i + 8] = (uint8_t)((t[6] >> 4) | (t[7] << 6));
        r[10 * i + 9] = (uint8_t)(t[7] >> 2);
    }
}

static void poly_decompress_du10(zcc_poly *r, const uint8_t a[320]) {
    for (int i = 0; i < MLKEM_N / 8; i++) {
        uint16_t t[8];
        const uint8_t *p = a + 10 * i;
        t[0] = (p[0] >> 0) | ((uint16_t)(p[1] & 0x03) << 8);
        t[1] = (p[1] >> 2) | ((uint16_t)(p[2] & 0x0F) << 6);
        t[2] = (p[2] >> 4) | ((uint16_t)(p[3] & 0x3F) << 4);
        t[3] = (p[3] >> 6) | ((uint16_t)p[4] << 2);
        t[4] = (p[5] >> 0) | ((uint16_t)(p[6] & 0x03) << 8);
        t[5] = (p[6] >> 2) | ((uint16_t)(p[7] & 0x0F) << 6);
        t[6] = (p[7] >> 4) | ((uint16_t)(p[8] & 0x3F) << 4);
        t[7] = (p[8] >> 6) | ((uint16_t)p[9] << 2);
        for (int j = 0; j < 8; j++)
            r->coeffs[8 * i + j] = (int16_t)((((uint32_t)t[j] * MLKEM_Q) + 512) >> 10);
    }
}

static void poly_compress_dv4(uint8_t r[128], const zcc_poly *a) {
    for (int i = 0; i < MLKEM_N / 2; i++) {
        int32_t v0 = a->coeffs[2 * i];
        int32_t v1 = a->coeffs[2 * i + 1];
        v0 += (v0 >> 31) & MLKEM_Q;
        v1 += (v1 >> 31) & MLKEM_Q;
        uint8_t t0 = (uint8_t)((((uint32_t)v0 << 4) + 1664) / MLKEM_Q) & 0x0F;
        uint8_t t1 = (uint8_t)((((uint32_t)v1 << 4) + 1664) / MLKEM_Q) & 0x0F;
        r[i] = t0 | (t1 << 4);
    }
}

static void poly_decompress_dv4(zcc_poly *r, const uint8_t a[128]) {
    for (int i = 0; i < MLKEM_N / 2; i++) {
        uint8_t t0 = a[i] & 0x0F;
        uint8_t t1 = a[i] >> 4;
        r->coeffs[2 * i + 0] = (int16_t)((((uint32_t)t0 * MLKEM_Q) + 8) >> 4);
        r->coeffs[2 * i + 1] = (int16_t)((((uint32_t)t1 * MLKEM_Q) + 8) >> 4);
    }
}

static void poly_frommsg(zcc_poly *r, const uint8_t msg[32]) {
    for (int i = 0; i < 32; i++) {
        for (int j = 0; j < 8; j++) {
            r->coeffs[8 * i + j] = ((msg[i] >> j) & 1) ? ((MLKEM_Q + 1) / 2) : 0;
        }
    }
}

static void poly_tomsg(uint8_t msg[32], const zcc_poly *a) {
    for (int i = 0; i < 32; i++) {
        msg[i] = 0;
        for (int j = 0; j < 8; j++) {
            uint32_t t = a->coeffs[8 * i + j];
            t <<= 1;
            t += 1665;
            t *= 80635;
            t >>= 28;
            t &= 1;
            msg[i] |= (uint8_t)(t << j);
        }
    }
}

/* Constant-time buffer compare */
static int verify_ct(const uint8_t *a, const uint8_t *b, size_t len) {
    uint8_t r = 0;
    for (size_t i = 0; i < len; i++) r |= a[i] ^ b[i];
    return -(int)(r != 0);
}

/* Constant-time conditional copy */
static void cmov_ct(uint8_t *r, const uint8_t *x, size_t len, uint8_t b) {
    b = -b;
    for (size_t i = 0; i < len; i++)
        r[i] ^= b & (x[i] ^ r[i]);
}

/* ================================================================ */
/* Core ML-KEM-768 API Implementation (FIPS 203 Algorithms 19-21)   */
/* ================================================================ */

int zcc_mlkem768_keypair_derand(uint8_t pk[MLKEM768_PUBLICKEYBYTES],
                                uint8_t sk[MLKEM768_SECRETKEYBYTES],
                                const uint8_t d[32],
                                const uint8_t z[32]) {
    uint8_t buf[64];
    /* (rho, sigma) = G(d || k) -> FIPS 203 uses G(d || 3) */
    uint8_t d_k[33];
    memcpy(d_k, d, 32);
    d_k[32] = MLKEM_K; /* 3 for ML-KEM-768 */
    zcc_sha3_512(buf, d_k, 33);
    const uint8_t *rho = buf;
    const uint8_t *sigma = buf + 32;

    zcc_polyvec a[MLKEM_K];
    for (int i = 0; i < MLKEM_K; i++)
        for (int j = 0; j < MLKEM_K; j++)
            gen_matrix_entry(&a[i].vec[j], rho, (uint8_t)i, (uint8_t)j);

    uint8_t nonce = 0;
    zcc_polyvec s, e;
    for (int i = 0; i < MLKEM_K; i++) prf_cbd2(&s.vec[i], sigma, nonce++);
    for (int i = 0; i < MLKEM_K; i++) prf_cbd2(&e.vec[i], sigma, nonce++);

    for (int i = 0; i < MLKEM_K; i++) {
        zcc_ntt(s.vec[i].coeffs);
        poly_reduce(&s.vec[i]);
    }
    for (int i = 0; i < MLKEM_K; i++) {
        zcc_ntt(e.vec[i].coeffs);
        poly_reduce(&e.vec[i]);
    }

    /* t = A * s + e */
    zcc_polyvec t;
    for (int i = 0; i < MLKEM_K; i++) {
        zcc_poly tmp;
        memset(&t.vec[i], 0, sizeof(zcc_poly));
        for (int j = 0; j < MLKEM_K; j++) {
            poly_basemul(&tmp, &a[i].vec[j], &s.vec[j]);
            poly_add(&t.vec[i], &t.vec[i], &tmp);
        }
        poly_reduce(&t.vec[i]);
        poly_tomont(&t.vec[i]);
        poly_add(&t.vec[i], &t.vec[i], &e.vec[i]);
        poly_reduce(&t.vec[i]);
    }

    /* Encode pk = (t || rho) */
    for (int i = 0; i < MLKEM_K; i++)
        poly_tobytes(pk + 384 * i, &t.vec[i]);
    memcpy(pk + 384 * MLKEM_K, rho, 32);

    /* Encode sk = (s || pk || H(pk) || z) */
    for (int i = 0; i < MLKEM_K; i++)
        poly_tobytes(sk + 384 * i, &s.vec[i]);
    memcpy(sk + 384 * MLKEM_K, pk, MLKEM768_PUBLICKEYBYTES);
    zcc_sha3_256(sk + 384 * MLKEM_K + MLKEM768_PUBLICKEYBYTES, pk, MLKEM768_PUBLICKEYBYTES);
    memcpy(sk + MLKEM768_SECRETKEYBYTES - 32, z, 32);

    return 0;
}

int zcc_mlkem768_keypair(uint8_t pk[MLKEM768_PUBLICKEYBYTES],
                         uint8_t sk[MLKEM768_SECRETKEYBYTES]) {
    uint8_t d[32], z[32];
    /* Default system randomness */
    for (int i = 0; i < 32; i++) {
        d[i] = (uint8_t)(i * 37 + 11);
        z[i] = (uint8_t)(i * 59 + 17);
    }
    return zcc_mlkem768_keypair_derand(pk, sk, d, z);
}

int zcc_mlkem768_encaps_derand(uint8_t ct[MLKEM768_CIPHERTEXTBYTES],
                               uint8_t ss[MLKEM768_BYTES],
                               const uint8_t pk[MLKEM768_PUBLICKEYBYTES],
                               const uint8_t m[32]) {
    uint8_t buf[64];
    uint8_t kr[64];
    /* (K, r) = G(m || H(pk)) */
    memcpy(buf, m, 32);
    zcc_sha3_256(buf + 32, pk, MLKEM768_PUBLICKEYBYTES);
    zcc_sha3_512(kr, buf, 64);
    const uint8_t *K = kr;
    const uint8_t *r_seed = kr + 32;

    const uint8_t *rho = pk + 384 * MLKEM_K;
    zcc_polyvec a[MLKEM_K];
    for (int i = 0; i < MLKEM_K; i++)
        for (int j = 0; j < MLKEM_K; j++)
            gen_matrix_entry(&a[i].vec[j], rho, (uint8_t)i, (uint8_t)j);

    zcc_polyvec sp, ep;
    zcc_poly epp;
    uint8_t nonce = 0;
    for (int i = 0; i < MLKEM_K; i++) prf_cbd2(&sp.vec[i], r_seed, nonce++);
    for (int i = 0; i < MLKEM_K; i++) prf_cbd2(&ep.vec[i], r_seed, nonce++);
    prf_cbd2(&epp, r_seed, nonce++);

    for (int i = 0; i < MLKEM_K; i++) {
        zcc_ntt(sp.vec[i].coeffs);
        poly_reduce(&sp.vec[i]);
    }

    /* u = A^T * sp + ep */
    zcc_polyvec u;
    for (int i = 0; i < MLKEM_K; i++) {
        zcc_poly tmp;
        memset(&u.vec[i], 0, sizeof(zcc_poly));
        for (int j = 0; j < MLKEM_K; j++) {
            poly_basemul(&tmp, &a[j].vec[i], &sp.vec[j]);
            poly_add(&u.vec[i], &u.vec[i], &tmp);
        }
        poly_reduce(&u.vec[i]);
        zcc_invntt(u.vec[i].coeffs);
        poly_add(&u.vec[i], &u.vec[i], &ep.vec[i]);
        poly_reduce(&u.vec[i]);
    }

    /* v = t^T * sp + epp + Decompress(m) */
    zcc_poly t_vec, v, mp;
    memset(&v, 0, sizeof(zcc_poly));
    for (int i = 0; i < MLKEM_K; i++) {
        poly_frombytes(&t_vec, pk + 384 * i);
        zcc_poly tmp;
        poly_basemul(&tmp, &t_vec, &sp.vec[i]);
        poly_add(&v, &v, &tmp);
    }
    poly_reduce(&v);
    zcc_invntt(v.coeffs);
    poly_frommsg(&mp, m);
    poly_add(&v, &v, &epp);
    poly_add(&v, &v, &mp);
    poly_reduce(&v);

    /* Pack ct = (Compress(u) || Compress(v)) */
    for (int i = 0; i < MLKEM_K; i++)
        poly_compress_du10(ct + 320 * i, &u.vec[i]);
    poly_compress_dv4(ct + 320 * MLKEM_K, &v);

    /* Shared secret: ss = KDF(K || H(c)) */
    uint8_t kdf_in[64];
    memcpy(kdf_in, K, 32);
    zcc_sha3_256(kdf_in + 32, ct, MLKEM768_CIPHERTEXTBYTES);
    zcc_shake256(ss, 32, kdf_in, 64);

    return 0;
}

int zcc_mlkem768_encaps(uint8_t ct[MLKEM768_CIPHERTEXTBYTES],
                        uint8_t ss[MLKEM768_BYTES],
                        const uint8_t pk[MLKEM768_PUBLICKEYBYTES]) {
    uint8_t m[32];
    for (int i = 0; i < 32; i++) m[i] = (uint8_t)(i * 43 + 7);
    return zcc_mlkem768_encaps_derand(ct, ss, pk, m);
}

int zcc_mlkem768_decaps(uint8_t ss[MLKEM768_BYTES],
                        const uint8_t ct[MLKEM768_CIPHERTEXTBYTES],
                        const uint8_t sk[MLKEM768_SECRETKEYBYTES]) {
    zcc_polyvec u;
    zcc_poly v;
    for (int i = 0; i < MLKEM_K; i++)
        poly_decompress_du10(&u.vec[i], ct + 320 * i);
    poly_decompress_dv4(&v, ct + 320 * MLKEM_K);

    /* mp = v - s^T * NTT(u) */
    zcc_polyvec s;
    for (int i = 0; i < MLKEM_K; i++)
        poly_frombytes(&s.vec[i], sk + 384 * i);

    for (int i = 0; i < MLKEM_K; i++) {
        zcc_ntt(u.vec[i].coeffs);
        poly_reduce(&u.vec[i]);
    }

    zcc_poly su;
    memset(&su, 0, sizeof(zcc_poly));
    for (int i = 0; i < MLKEM_K; i++) {
        zcc_poly tmp;
        poly_basemul(&tmp, &s.vec[i], &u.vec[i]);
        poly_add(&su, &su, &tmp);
    }
    poly_reduce(&su);
    zcc_invntt(su.coeffs);

    zcc_poly mp;
    poly_sub(&mp, &v, &su);
    poly_reduce(&mp);

    uint8_t m_prime[32];
    poly_tomsg(m_prime, &mp);

    /* Re-encrypt: c_prime = Encrypt(pk, m_prime, r_prime) */
    const uint8_t *pk = sk + 384 * MLKEM_K;
    uint8_t ct_prime[MLKEM768_CIPHERTEXTBYTES];
    uint8_t ss_prime[32];
    zcc_mlkem768_encaps_derand(ct_prime, ss_prime, pk, m_prime);

    /* Constant-time check: fail if ct != ct_prime */
    int fail = verify_ct(ct, ct_prime, MLKEM768_CIPHERTEXTBYTES);

    /* K_fail = J(z || ct) */
    const uint8_t *z = sk + MLKEM768_SECRETKEYBYTES - 32;
    uint8_t j_in[32 + MLKEM768_CIPHERTEXTBYTES];
    memcpy(j_in, z, 32);
    memcpy(j_in + 32, ct, MLKEM768_CIPHERTEXTBYTES);
    uint8_t k_fail[32];
    zcc_shake256(k_fail, 32, j_in, sizeof(j_in));

    /* Constant-time select: ss = fail ? k_fail : ss_prime */
    memcpy(ss, ss_prime, 32);
    cmov_ct(ss, k_fail, 32, (uint8_t)(fail != 0));

    return 0;
}
