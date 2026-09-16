/* ========================================================================= */
/* ZCC POST-QUANTUM STARK LIGHT CLIENT (P1-P5)                               */
/* ========================================================================= */
/* File: src/zk/zcc_pq_light_client.h                                        */
/* Description: Quantum-Resilient STARK Zero-Knowledge Light Client:         */
/*              P1: FRI (Fast Reed-Solomon IOP) Low-Degree Testing           */
/*              P2: Algebraic Poseidon / Rescue-Prime Hash Merkle Tree       */
/*              P3: AIR Transition & Boundary Constraint Evaluator           */
/*              P4: Sub-50ms Header Transition Verification Engine           */
/*              P5: Native Vectorized STARK Verifier Microcode Emitter       */
/* ========================================================================= */

#ifndef ZCC_PQ_LIGHT_CLIENT_H
#define ZCC_PQ_LIGHT_CLIENT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PQLC_FIELD_MODULUS       0xFFFFFFFF00000001ULL /* Goldilocks Prime: 2^64 - 2^32 + 1 */
#define PQLC_MAX_FRI_ROUNDS      8
#define PQLC_MAX_QUERIES         32
#define PQLC_PROOF_MAX_BYTES     24576 /* < 24 kB Proof Size */
#define PQLC_DIGEST_LEN          32
#define PQLC_MAX_VERIFY_TIME_MS  50.0

typedef struct {
    uint8_t root[PQLC_DIGEST_LEN];
    uint32_t depth;
    uint32_t n_leaves;
} PqlcMerkleTree;

typedef struct {
    uint64_t step_index;
    uint64_t state_before[4];
    uint64_t state_after[4];
    bool     boundary_valid;
    bool     transition_valid;
} PqlcAirConstraints;

typedef struct {
    uint32_t fri_rounds;
    uint64_t fri_roots[PQLC_MAX_FRI_ROUNDS][4];
    uint64_t query_positions[PQLC_MAX_QUERIES];
    uint64_t query_evaluations[PQLC_MAX_QUERIES];
    uint32_t proof_size_bytes;
    uint64_t genesis_block_hash;
    uint64_t target_block_hash;
    uint64_t state_transition_root;
} PqlcStarkProof;

typedef struct {
    uint32_t proof_size_bytes;
    double   verification_time_ms;
    bool     fri_soundness_verified;
    bool     air_constraints_satisfied;
    bool     quantum_security_128bit;
    bool     light_client_verified;
} PqlcVerificationReceipt;

/* ------------------------------------------------------------------------- */
/* FUNCTION PROTOTYPES (P1 - P5)                                             */
/* ------------------------------------------------------------------------- */

/* P1: Goldilocks Prime Field & FRI Low-Degree Testing */
uint64_t pqlc_field_add(uint64_t a, uint64_t b);
uint64_t pqlc_field_mul(uint64_t a, uint64_t b);
uint64_t pqlc_field_inv(uint64_t a);
bool pqlc_verify_fri_round(
    uint64_t domain_size,
    uint64_t fold_factor,
    const uint64_t *evals_in,
    uint64_t *evals_out
);

/* P2: Post-Quantum Algebraic Merkle Tree Commitments */
bool pqlc_compute_algebraic_hash(const uint64_t state[4], uint64_t out_digest[4]);
bool pqlc_build_merkle_tree(PqlcMerkleTree *tree, const uint64_t *leaves, uint32_t count);

/* P3: AIR Transition & Boundary Constraints */
bool pqlc_evaluate_air_constraints(PqlcAirConstraints *air, uint64_t step, const uint64_t state[4]);

/* P4: Header Transition STARK Verification (< 24 kB, < 50ms) */
bool pqlc_generate_light_client_proof(
    uint64_t genesis_hash,
    uint64_t target_hash,
    uint32_t n_blocks,
    PqlcStarkProof *out_proof
);
bool pqlc_verify_light_client_proof(
    const PqlcStarkProof *proof,
    PqlcVerificationReceipt *out_receipt
);

/* P5: Native Vectorized STARK Verifier Emitter */
int32_t pqlc_emit_verifier_assembly(
    const PqlcStarkProof *proof,
    char *out_buf,
    size_t buf_len
);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_PQ_LIGHT_CLIENT_H */
