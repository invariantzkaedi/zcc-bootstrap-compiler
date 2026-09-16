#ifndef ZCC_QUANTUM_UNIFIED_H
#define ZCC_QUANTUM_UNIFIED_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZQ_ALIGN 64
#define ZQ_MAX_MUTATIONS 16
#define ZQ_MAX_REGISTERS 16

/* Status codes */
typedef enum {
    ZQ_OK = 0,
    ZQ_ERR_NULL_PTR = -1,
    ZQ_ERR_DIMENSION = -2,
    ZQ_ERR_NON_CONVERGED = -3,
    ZQ_ERR_NORM_DRIFT = -4
} zq_status_t;

/* ──────────────────────────────────────────────────────────────────────────
 * 1. Quantum Spectral Form Factor (SFF) & Graph Rigidity
 * ────────────────────────────────────────────────────────────────────────── */
typedef struct {
    float spectral_dim;      /* d_s spectral dimension */
    float r_spacing_ratio;   /* <r> level repulsion spacing ratio (GOE ~0.536, Poisson ~0.386) */
    float sff_mean;          /* Mean spectral form factor <K(t)> */
    bool is_chaotic_goe;     /* true if GOE level repulsion, false if Poisson modular */
} zq_spectral_metrics_t;

/* Compute spectral dimension d_s and Level Repulsion <r> across graph eigenvalues */
zq_status_t zcc_qcfg_spectral_analyze_avx2(const float *eigenvalues,
                                          size_t n_evals,
                                          zq_spectral_metrics_t *out_metrics);

/* ──────────────────────────────────────────────────────────────────────────
 * 2. 16-Node Quantum Walk Mutation Superposition Tournament
 * ────────────────────────────────────────────────────────────────────────── */
typedef struct {
    /* Complex wave packet across 16 candidate mutations */
    float re[ZQ_MAX_MUTATIONS];
    float im[ZQ_MAX_MUTATIONS];
    float prob[ZQ_MAX_MUTATIONS];
    /* 16x16 Conflict/Interaction Matrix (1.0 = conflict, 0.0 = orthogonal, -1.0 = synergy) */
    float conflict_mat[ZQ_MAX_MUTATIONS * ZQ_MAX_MUTATIONS];
    /* Expected fitness delta for each mutation candidate */
    float fitness_deltas[ZQ_MAX_MUTATIONS];
    uint32_t step;
    float total_norm;
} zq_mut_tournament_t;

/* Initialize mutation tournament wave packet */
zq_status_t zcc_qmut_tournament_init(zq_mut_tournament_t *tourn,
                                     const float *conflict_mat,
                                     const float *fitness_deltas);

/* Execute 1-step quantum superposition evolution (constructive amplification + destructive phase cancel) */
zq_status_t zcc_qmut_tournament_step_avx2(zq_mut_tournament_t *tourn, float dt);

/* Select top K non-conflicting mutation candidates based on wave packet amplitudes */
zq_status_t zcc_qmut_tournament_select(const zq_mut_tournament_t *tourn,
                                       int *out_selected_indices,
                                       int max_select,
                                       int *out_count);

/* ──────────────────────────────────────────────────────────────────────────
 * 3. Transverse-Field Ising Spin-Glass Register Allocator
 * ────────────────────────────────────────────────────────────────────────── */
typedef struct {
    uint32_t num_vars;
    uint32_t num_colors;
    /* Adjacency / Interference Matrix: 1.0 if variables interfere, 0.0 otherwise */
    float interference[ZQ_MAX_REGISTERS * ZQ_MAX_REGISTERS];
    /* Spin orientation angles theta_i in [0, 2*pi] representing color assignments */
    float spins[ZQ_MAX_REGISTERS];
    /* Conjugate momentum */
    float momentum[ZQ_MAX_REGISTERS];
    float transverse_field; /* Gamma(t) quantum tunneling field */
} zq_ising_regalloc_t;

/* Initialize Ising register allocator */
zq_status_t zcc_qising_init(zq_ising_regalloc_t *alloc,
                            uint32_t num_vars,
                            uint32_t num_colors,
                            const float *interference_mat);

/* Anneal step with quantum tunneling */
zq_status_t zcc_qising_anneal_step_avx2(zq_ising_regalloc_t *alloc, float dt, float gamma_t);

/* Extract discrete register color assignments (0..num_colors-1) */
zq_status_t zcc_qising_extract_colors(const zq_ising_regalloc_t *alloc,
                                      int *out_colors,
                                      int *out_spill_count);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_QUANTUM_UNIFIED_H */
