/* ========================================================================= */
/* ZCC FHE-C / ENCRYPTED-SSA: AUTOMATED NOISE-OPTIMIZED HOMOMORPHIC COMPILER */
/* ========================================================================= */
/* File: src/crypto/fhe_encrypted_ssa.h                                      */
/* Description: Compiles C expressions into BFV/CKKS homomorphic polynomial  */
/*              circuits with automated noise budget tracking & bootstrapping*/
/* ========================================================================= */

#ifndef ZCC_FHE_ENCRYPTED_SSA_H
#define ZCC_FHE_ENCRYPTED_SSA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FHE_POLY_DEGREE 1024
#define FHE_DEFAULT_MODULUS 1152921504606830593ULL /* 60-bit prime for BFV/CKKS */
#define FHE_BOOTSTRAP_THRESHOLD_BITS 14

typedef struct {
    uint64_t c0[FHE_POLY_DEGREE]; /* Ciphertext component 0 */
    uint64_t c1[FHE_POLY_DEGREE]; /* Ciphertext component 1 */
    uint32_t noise_budget_bits;   /* Remaining noise budget */
    uint32_t depth;               /* Multiplicative depth */
} FheCiphertext;

typedef struct {
    uint64_t modulus;
    uint32_t poly_degree;
    uint32_t bootstraps_inserted;
    uint32_t homomorphic_adds;
    uint32_t homomorphic_muls;
} FheCircuitStats;

/* Encrypt plaintext 64-bit scalar into polynomial ring ciphertext */
void fhe_encrypt_scalar(uint64_t plaintext, uint64_t secret_key, FheCiphertext *out_ct);

/* Decrypt polynomial ring ciphertext back into plaintext */
uint64_t fhe_decrypt_scalar(const FheCiphertext *ct, uint64_t secret_key);

/* Homomorphic Addition: CT3 = CT1 + CT2 (Noise increases additively) */
void fhe_eval_add(const FheCiphertext *ct1, const FheCiphertext *ct2, FheCiphertext *out_ct, FheCircuitStats *stats);

/* Homomorphic Multiplication: CT3 = CT1 * CT2 (Noise increases quadratically) */
void fhe_eval_mul(const FheCiphertext *ct1, const FheCiphertext *ct2, FheCiphertext *out_ct, FheCircuitStats *stats);

/* Automated Bootstrapping / Noise Reduction */
void fhe_eval_bootstrap(FheCiphertext *ct, FheCircuitStats *stats);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_FHE_ENCRYPTED_SSA_H */
