#ifndef AVXZKD_SUPREME_H
#define AVXZKD_SUPREME_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AVXZKD_ALIGN 64
#define AVXZKD_CACHELINE 64

/* Engine Status Codes */
typedef enum {
    AVXZKD_OK               = 0,
    AVXZKD_ERR_NULL_PTR     = -1,
    AVXZKD_ERR_ALIGNMENT    = -2,
    AVXZKD_ERR_NAN_FAULT    = -3,
    AVXZKD_ERR_BIFURCATION  = -4,
    AVXZKD_ERR_PATH_BLOCKED = -5
} avxzkd_status_t;

/* CPU Feature Flags */
typedef enum {
    AVXZKD_CPU_SCALAR  = 0x01,
    AVXZKD_CPU_AVX2    = 0x02,
    AVXZKD_CPU_FMA     = 0x04,
    AVXZKD_CPU_AVX512F = 0x08,
    AVXZKD_CPU_AVX512BW= 0x10
} avxzkd_cpu_features_t;

/* System Parameters */
typedef struct {
    float eta;           /* Recursive field gain (default: 0.4) */
    float gamma;         /* Non-linear sharpness (default: 0.3) */
    float beta;          /* State noise modulation (default: 0.1) */
    float eps;           /* Stochastic tie-breaker (default: 0.05) */
    float kick;          /* Scar departure potential (default: 2.0) */
    float kappa;         /* Inter-field resonance coupling (default: 0.0) */
    float momentum;      /* Inertial reversal penalty [0.0, 1.0] (default: 0.25) */
    uint64_t seed[4];    /* 256-bit xoshiro256+ cryptographic PRNG state */
} avxzkd_params_t;

/* 64-Byte Aligned Field Lattice */
typedef struct {
    float *base;         /* Immutable reference potential [64-byte aligned] */
    float *current;      /* Active potential field [64-byte aligned] */
    float *scars;        /* Visited-cell memory map [64-byte aligned] */
    float *curvature;    /* Topological Laplacian del^2 H [64-byte aligned] */
    float *hessian_det;  /* Saddle/Ridge discriminator det(Hessian) */
    uint32_t width;      /* SIMD-padded width (multiple of 16) */
    uint32_t height;     /* Lattice height */
    uint32_t stride;     /* Row pitch in floats */
    uint32_t total_cells;
    uint32_t tripwires;  /* IEEE NaN / Inf anomaly tripwire count */
} avxzkd_field_t;

/* Invariant Audit Certificate */
typedef struct {
    float l_inf_error;        /* Strict max error vs double-precision scalar reference */
    float mean_error;         /* Mean absolute error */
    float measured_gain;      /* Empirical gain in saturated zone (target: 1 / (1 - eta)) */
    float floor_drift;        /* Max drift in negative floor (target: 0.0) */
    float lyapunov_exponent;  /* Mean local Lyapunov exponent */
    uint64_t state_digest;    /* 64-bit cryptographic attestation hash of field */
    bool pass_all_invariants;
} avxzkd_audit_t;

/* Sovereign Two-Regime Walker */
typedef struct {
    int32_t x;
    int32_t y;
    int32_t prev_dx;
    int32_t prev_dy;
    int32_t target_x;
    int32_t target_y;
    uint32_t steps_taken;
    uint32_t backtracks;
    bool solved;
    int32_t *path_x;
    int32_t *path_y;
    uint32_t path_len;
    uint32_t capacity;
} avxzkd_walker_t;

/* Layer 1: Discrete-Time Quantum Walk (DTQW) State */
typedef struct {
    float real0[16];     /* Coin 0 Real amplitudes [64-byte aligned] */
    float imag0[16];     /* Coin 0 Imaginary amplitudes [64-byte aligned] */
    float real1[16];     /* Coin 1 Real amplitudes [64-byte aligned] */
    float imag1[16];     /* Coin 1 Imaginary amplitudes [64-byte aligned] */
    double node_probs[16];   /* Born probability distribution P(x) */
    double node_phases[16];  /* Phase vector H_phase(x) */
    double s_q0;             /* Subsystem coin entanglement entropy */
    double coherence;        /* Quantum purity / state coherence [0.0, 1.0] */
    uint32_t total_steps;
} avxzkd_dtqw_t;

/* Lifecycle */
avxzkd_field_t* avxzkd_create(uint32_t width, uint32_t height);
avxzkd_status_t avxzkd_init(avxzkd_field_t *field, const float *init_base);
void avxzkd_destroy(avxzkd_field_t *field);

/* Hardware Capabilities */
uint32_t avxzkd_get_cpu_features(void);

/* SIMD Field Kernels */
avxzkd_status_t avxzkd_deep_recurse_avx2(avxzkd_field_t *field, avxzkd_params_t *params, uint32_t k_steps);
avxzkd_status_t avxzkd_deep_recurse_avx512(avxzkd_field_t *field, avxzkd_params_t *params, uint32_t k_steps);
avxzkd_status_t avxzkd_deep_recurse_auto(avxzkd_field_t *field, avxzkd_params_t *params, uint32_t k_steps);
avxzkd_status_t avxzkd_deep_recurse_parallel(avxzkd_field_t *field, avxzkd_params_t *params, uint32_t k_steps, uint32_t threads);

avxzkd_status_t avxzkd_compute_topology_avx2(avxzkd_field_t *field);
avxzkd_status_t avxzkd_compute_topology_avx512(avxzkd_field_t *field);

avxzkd_status_t avxzkd_couple_fields_avx2(avxzkd_field_t *fa, avxzkd_field_t *fb, float kappa);
avxzkd_status_t avxzkd_couple_fields_avx512(avxzkd_field_t *fa, avxzkd_field_t *fb, float kappa);

/* Cryptographic Attestation & Invariant Audit */
avxzkd_status_t avxzkd_audit(const avxzkd_field_t *field, const avxzkd_params_t *params, avxzkd_audit_t *audit);

/* Two-Regime Navigator */
avxzkd_walker_t* avxzkd_walker_create(int32_t sx, int32_t sy, int32_t tx, int32_t ty, uint32_t capacity);
int32_t avxzkd_walker_solve(avxzkd_walker_t *walker, avxzkd_field_t *field, avxzkd_params_t *params, uint32_t max_steps);
void avxzkd_walker_prune_loops(avxzkd_walker_t *walker);
void avxzkd_walker_destroy(avxzkd_walker_t *walker);

/* Layer 1 Quantum DTQW Acceleration & Decoherence */
avxzkd_dtqw_t* avxzkd_dtqw_create(void);
avxzkd_status_t avxzkd_dtqw_step_avx2(avxzkd_dtqw_t *qw, uint32_t steps);
avxzkd_status_t avxzkd_dtqw_step_avx512(avxzkd_dtqw_t *qw, uint32_t steps);
avxzkd_status_t avxzkd_dtqw_step_auto(avxzkd_dtqw_t *qw, uint32_t steps);
avxzkd_status_t avxzkd_dtqw_dephase(avxzkd_dtqw_t *qw, float gamma_dephase);
void avxzkd_dtqw_destroy(avxzkd_dtqw_t *qw);

#ifdef __cplusplus
}
#endif

#endif /* AVXZKD_SUPREME_H */
