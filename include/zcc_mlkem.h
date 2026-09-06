/* ================================================================ */
/* ZCC CRYPTOGRAPHIC SUBSTRATE: NIST FIPS 203 ML-KEM-768 / 1024     */
/* ================================================================ */
/* File: include/zcc_mlkem.h                                        */
/* Description: High-speed C99 implementation of Module-Lattice     */
/*              Key Encapsulation Mechanism with fast NTT,          */
/*              Montgomery reduction, and Keccak-f[1600].          */
/* ================================================================ */

#ifndef ZCC_MLKEM_H
#define ZCC_MLKEM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ML-KEM-768 Parameter Set */
#define MLKEM_N 256
#define MLKEM_Q 3329
#define MLKEM_K 3
#define MLKEM_ETA1 2
#define MLKEM_ETA2 2
#define MLKEM_DU 10
#define MLKEM_DV 4

#define MLKEM768_PUBLICKEYBYTES  1184 /* (384 * k) + 32 = 1184 */
#define MLKEM768_SECRETKEYBYTES  2400 /* 384*k + 1184 + 32 + 32 = 2400 */
#define MLKEM768_CIPHERTEXTBYTES 1088 /* (320 * k) + 128 = 1088 */
#define MLKEM768_BYTES           32   /* Shared secret size */

/* Polynomial representation in R_q = Z_q[X]/(X^256 + 1) */
typedef struct {
    int16_t coeffs[MLKEM_N];
} zcc_poly;

typedef struct {
    zcc_poly vec[MLKEM_K];
} zcc_polyvec;

/* Keccak-f[1600] state and hash functions */
typedef struct {
    uint64_t s[25];
    size_t pos;
    size_t rate;
} zcc_keccak_state;

void zcc_shake128_init(zcc_keccak_state *state);
void zcc_shake128_absorb(zcc_keccak_state *state, const uint8_t *in, size_t inlen);
void zcc_shake128_finalize(zcc_keccak_state *state);
void zcc_shake128_squeeze(zcc_keccak_state *state, uint8_t *out, size_t outlen);

void zcc_shake256_init(zcc_keccak_state *state);
void zcc_shake256_absorb(zcc_keccak_state *state, const uint8_t *in, size_t inlen);
void zcc_shake256_finalize(zcc_keccak_state *state);
void zcc_shake256_squeeze(zcc_keccak_state *state, uint8_t *out, size_t outlen);

void zcc_sha3_256(uint8_t out[32], const uint8_t *in, size_t inlen);
void zcc_sha3_512(uint8_t out[64], const uint8_t *in, size_t inlen);
void zcc_shake256(uint8_t *out, size_t outlen, const uint8_t *in, size_t inlen);

/* Fast Number Theoretic Transform (NTT) */
void zcc_ntt(int16_t r[256]);
void zcc_invntt(int16_t r[256]);
int16_t zcc_montgomery_reduce(int32_t a);
int16_t zcc_barrett_reduce(int16_t a);
void zcc_poly_basemul_montgomery(int16_t r[2], const int16_t a[2], const int16_t b[2], int16_t zeta);

/* Core ML-KEM-768 API */
int zcc_mlkem768_keypair_derand(uint8_t pk[MLKEM768_PUBLICKEYBYTES],
                                uint8_t sk[MLKEM768_SECRETKEYBYTES],
                                const uint8_t d[32],
                                const uint8_t z[32]);

int zcc_mlkem768_keypair(uint8_t pk[MLKEM768_PUBLICKEYBYTES],
                         uint8_t sk[MLKEM768_SECRETKEYBYTES]);

int zcc_mlkem768_encaps_derand(uint8_t ct[MLKEM768_CIPHERTEXTBYTES],
                               uint8_t ss[MLKEM768_BYTES],
                               const uint8_t pk[MLKEM768_PUBLICKEYBYTES],
                               const uint8_t coins[32]);

int zcc_mlkem768_encaps(uint8_t ct[MLKEM768_CIPHERTEXTBYTES],
                        uint8_t ss[MLKEM768_BYTES],
                        const uint8_t pk[MLKEM768_PUBLICKEYBYTES]);

int zcc_mlkem768_decaps(uint8_t ss[MLKEM768_BYTES],
                        const uint8_t ct[MLKEM768_CIPHERTEXTBYTES],
                        const uint8_t sk[MLKEM768_SECRETKEYBYTES]);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_MLKEM_H */
