/* SPDX-License-Identifier: MIT */
#ifndef ZCC_TRITON_BRIDGE_H
#define ZCC_TRITON_BRIDGE_H

/**
 * @file zcc_triton_bridge.h
 * @brief ZCC Native C-to-GPU Triton Super-Kernel Bridge Interface
 * @notice System V ABI compliant extern symbols for hardware-accelerated kernels
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Finite Field BabyBear Prime */
#define ZCC_BABYBEAR_PRIME 2013265921ULL

/* Hardware Engine Status Codes */
typedef enum {
    ZCC_GPU_SUCCESS = 0,
    ZCC_GPU_ERR_OUT_OF_MEMORY = 1,
    ZCC_GPU_ERR_STREAM_SYNC = 2,
    ZCC_GPU_ERR_INVALID_DEGREE = 3
} zcc_gpu_status_t;

/* Triton Super-Kernel Handle */
typedef struct {
    size_t size;
    float *h_field;
    float *h_base;
    float *psi_real;
    float *psi_imag;
    float *dex_features;
    float *profit_out;
    int is_vram_mapped;
    double last_eval_latency_ns;
} zcc_triton_handle_t;

int zcc_triton_init_handle(zcc_triton_handle_t *handle, size_t size);
int zcc_triton_free_handle(zcc_triton_handle_t *handle);
int zcc_triton_step_hamiltonian(zcc_triton_handle_t *handle, float eta, float gamma, float eps, float beta);
int zcc_triton_step_quantum_walk(zcc_triton_handle_t *handle, float gamma_coupling, float dt);
double zcc_triton_get_peak_throughput(const zcc_triton_handle_t *handle);

/* Pillar 1: DEX Arbitrage Kernel Extern */
zcc_gpu_status_t zcc_gpu_scan_mempool_routes(
    const float *reserves_a,
    const float *reserves_b,
    float *out_profit,
    uint32_t num_routes
);

/* Pillar 2: 50M-Cell Navier-Stokes Fluid Sim Extern */
zcc_gpu_status_t zcc_gpu_advect_fluid_cells(
    float *density,
    float *vx,
    float *vy,
    uint32_t width,
    uint32_t height,
    float dt
);

/* Pillar 3: Microsecond AI Flash-Attention INT4 Extern */
zcc_gpu_status_t zcc_gpu_forward_flash_attention(
    const void *q_ptr,
    const void *k_ptr,
    const void *v_ptr,
    void *out_attn_ptr,
    uint32_t seq_len,
    uint32_t num_heads,
    uint32_t head_dim
);

/* Pillar 4: 16.7M-Gate ZK-STARK NTT Prover Extern */
zcc_gpu_status_t zcc_gpu_prove_zk_stark_circuit(
    const uint32_t *trace_poly,
    uint32_t *out_quotient,
    uint32_t degree,
    uint64_t prime_mod,
    uint8_t out_merkle_root[32]
);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_TRITON_BRIDGE_H */
