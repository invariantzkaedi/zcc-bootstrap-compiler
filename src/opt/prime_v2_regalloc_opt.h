/* src/opt/prime_v2_regalloc_opt.h
 * ZKAEDI PRIME v2.0 LEGENDARY — Multi-Field Coupled Hamiltonian Optimizer Pass for ZCC
 */

#ifndef PRIME_V2_REGALLOC_OPT_H
#define PRIME_V2_REGALLOC_OPT_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

struct Function;
typedef struct Function Function;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float eta;             /* Recursive Gain parameter (default: 0.42) */
    float gamma;           /* Sigmoidal slope parameter (default: 0.35) */
    float beta;            /* Noise scaling parameter (default: 0.12) */
    float epsilon;         /* Stochastic perturbation magnitude (default: 0.05) */
    float kappa[4][4];     /* 4x4 Inter-field coupling matrix */
    bool auto_tune;        /* Enable adaptive self-tuning via Lyapunov monitoring */
    bool spectral_mode;    /* Enable ZCC_PRIME_SPECTRAL mode for spectral QR + cache-set allocator */
} ZKAEDIPrimeV2Config;

typedef struct {
    float lyapunov_exponent;       /* Attractor stability trace */
    float lyapunov_lambda1;        /* Largest Lyapunov exponent from 4D QR spectrum */
    float synergy_score;           /* Phi = kappa * sqrt(Coherence * Diversity * Actionability) */
    bool is_chaotic;               /* True if Lyapunov > 0.05 */
    bool is_fixed_point;           /* True if lambda1 < 0.0 and attractor collapsed */
    uint32_t instructions_optimized;
    uint32_t spills_avoided;
    uint32_t cache_collisions_mitigated; /* L1 64-byte cache set collisions displaced */
} ZKAEDIPrimeV2Metrics;

/* Initialize default configuration for 4-field coupling */
void zcc_prime_v2_init_config(ZKAEDIPrimeV2Config *cfg);

/* Core ZKAEDI PRIME v2 Multi-Field IR Optimization Pass */
uint32_t zcc_opt_prime_v2_pass(Function *fn);

/* Query recent execution metrics */
void zcc_prime_v2_get_metrics(ZKAEDIPrimeV2Metrics *metrics);

#ifdef __cplusplus
}
#endif

#endif /* PRIME_V2_REGALLOC_OPT_H */
