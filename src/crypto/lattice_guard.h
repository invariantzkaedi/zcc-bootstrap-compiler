/* ========================================================================= */
/* ZCC LATTICEGUARD: CONSTANT-TIME & POWER-EQUALIZED POST-QUANTUM JIT        */
/* ========================================================================= */
/* File: src/crypto/lattice_guard.h                                          */
/* Description: Automated side-channel immunity for FIPS 203 ML-KEM / Kyber  */
/*              and FIPS 204 ML-DSA, with constant-time NTT & DPA shielding. */
/* ========================================================================= */

#ifndef ZCC_LATTICE_GUARD_H
#define ZCC_LATTICE_GUARD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KYBER_N 256
#define KYBER_Q 3329
#define KYBER_MONT 2285

typedef struct {
    int16_t coeffs[KYBER_N];
} KyberPoly;

typedef struct {
    uint32_t secret_branches_detected;
    uint32_t power_balancing_ops_injected;
    bool     constant_time_verified;
    bool     dpa_shield_active;
} LatticeGuardAuditReport;

/* Constant-Time Montgomery Reduction: returns (a * R^-1) mod Q */
int16_t lattice_guard_montgomery_reduce(int32_t a);

/* Constant-Time Number Theoretic Transform (NTT) for Kyber-768/1024 */
void lattice_guard_ct_ntt(KyberPoly *p);

/* Constant-Time Inverse NTT */
void lattice_guard_ct_invntt(KyberPoly *p);

/* Audit function bytecode / AST for secret-dependent timing leakage */
LatticeGuardAuditReport lattice_guard_audit_function(
    const uint8_t *code,
    size_t         len,
    bool           inject_dpa_mitigation
);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_LATTICE_GUARD_H */
