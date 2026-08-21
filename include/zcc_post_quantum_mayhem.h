/*
 * zcc_post_quantum_mayhem.h — Sovereign Bare-Metal Post-Quantum Mayhem Engine
 * ===========================================================================
 * Hardened lattice-based quantum resistance (Kyber/Dilithium polynomial ring NTT),
 * speculative execution side-channel barricades (Spectre-V2/SLS return trampolines),
 * stack canary poison traps, and hardware-enforced memory sanitization.
 */

#ifndef ZCC_POST_QUANTUM_MAYHEM_H
#define ZCC_POST_QUANTUM_MAYHEM_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define KYBER_N 256
#define KYBER_Q 3329

#define MAYHEM_CANARY_MAGIC   0xFEDCBA9876543210ULL
#define MAYHEM_QUANTUM_SEAL   0x5051435345414C24ULL /* 'PQCSEAL$' */

/* Post-Quantum Key Encapsulation Context */
typedef struct {
    int16_t public_poly[KYBER_N];
    int16_t secret_poly[KYBER_N];
    uint8_t shared_secret[32];
    uint64_t hardware_entropy_seed;
} ZCCPostQuantumContext;

/* Bare-Metal Hardware Defense Metrics */
typedef struct {
    uint32_t sls_barriers_injected;   /* Straight-Line Speculation Retpolines */
    uint32_t retpoline_thunks_active; /* Spectre-v2 Return Trampolines */
    uint32_t canary_traps_planted;    /* Stack Poison Defense Tripwires */
    uint32_t memory_scrubs_executed;  /* Zero-Trace Secret Scrambling */
} ZCCBareMetalMayhemAudit;

/* Core Function Declarations */
void zcc_pqc_mayhem_init(ZCCPostQuantumContext *ctx, uint64_t seed);
void zcc_pqc_ntt_transform(int16_t poly[KYBER_N]);
void zcc_pqc_inv_ntt_transform(int16_t poly[KYBER_N]);
int  zcc_pqc_encapsulate(ZCCPostQuantumContext *ctx, uint8_t ciphertext[64]);
int  zcc_pqc_decapsulate(ZCCPostQuantumContext *ctx, const uint8_t ciphertext[64]);

/* Bare-Metal Hardware Hardening Interventions */
void zcc_baremetal_inject_speculation_barrier(void);
void zcc_baremetal_secure_wipe(void *v, size_t n);
ZCCBareMetalMayhemAudit zcc_baremetal_run_mayhem_gauntlet(void);

#endif /* ZCC_POST_QUANTUM_MAYHEM_H */
