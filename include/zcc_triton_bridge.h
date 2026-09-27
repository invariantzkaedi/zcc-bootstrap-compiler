/* SPDX-License-Identifier: MIT */
#ifndef ZCC_TRITON_BRIDGE_H
#define ZCC_TRITON_BRIDGE_H

/**
 * @file zcc_triton_bridge.h
 * @brief ZCC Native C-to-GPU Triton Super-Kernel Bridge Interface
 * @notice System V ABI compliant extern symbols for hardware-accelerated kernels
 *
 * Dispatch Architecture (LIMIT-GPU-001):
 *   - Layer 0 (C99 CPU Fallback): all pillars have reference scalar impls.
 *   - Layer 1 (CUDA Driver API): zcc_gpu_init_blackwell() wires nvcuda.dll /
 *     libcuda.so via dlopen for zero build-time CUDA dependency.
 *   - Layer 2 (Tensor Core Hot Path): zcc_triton_dispatch_gpu_gauntlet() invokes
 *     all four pillars via the Python CUDA runtime using shared-library ABI.
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Status codes                                                         */
/* ------------------------------------------------------------------ */
#define ZCC_BABYBEAR_PRIME 2013265921ULL

typedef enum {
    ZCC_GPU_SUCCESS             = 0,
    ZCC_GPU_ERR_OUT_OF_MEMORY   = 1,
    ZCC_GPU_ERR_STREAM_SYNC     = 2,
    ZCC_GPU_ERR_INVALID_DEGREE  = 3,
    ZCC_GPU_ERR_NO_DEVICE       = 4,
    ZCC_GPU_ERR_INIT_FAILED     = 5,
    ZCC_GPU_ERR_MODULE_LOAD     = 6,
    ZCC_GPU_ERR_DISPATCH        = 7
} zcc_gpu_status_t;

/* ------------------------------------------------------------------ */
/* CUDA Driver Context (opaque; zero build-time CUDA dependency)        */
/* ------------------------------------------------------------------ */
typedef struct {
    void  *cu_device;           /* CUdevice (int-sized) stored as ptr */
    void  *cu_context;          /* CUcontext */
    int    sm_major;
    int    sm_minor;
    int    device_index;
    char   device_name[128];
    int    is_initialized;
} zcc_gpu_dispatch_ctx_t;

/**
 * @brief Initialize CUDA Driver API context for Blackwell SM 12.0.
 *        Zero build-time CUDA dependency — loads libcuda.so / nvcuda.dll
 *        at runtime via dlopen/LoadLibrary.
 * @param ctx Output dispatch context.
 * @return ZCC_GPU_SUCCESS or error code.
 */
zcc_gpu_status_t zcc_gpu_init_blackwell(zcc_gpu_dispatch_ctx_t *ctx);

/**
 * @brief Destroy the CUDA Driver context and free all GPU resources.
 */
void zcc_gpu_destroy_ctx(zcc_gpu_dispatch_ctx_t *ctx);

/* ------------------------------------------------------------------ */
/* Mode A/C Hamiltonian Handles (quantum walk engine)                  */
/* ------------------------------------------------------------------ */
typedef struct {
    size_t  size;
    float  *h_field;
    float  *h_base;
    float  *psi_real;
    float  *psi_imag;
    float  *dex_features;
    float  *profit_out;
    int     is_vram_mapped;
    double  last_eval_latency_ns;
} zcc_triton_handle_t;

int    zcc_triton_init_handle(zcc_triton_handle_t *handle, size_t size);
int    zcc_triton_free_handle(zcc_triton_handle_t *handle);
int    zcc_triton_step_hamiltonian(zcc_triton_handle_t *handle,
                                   float eta, float gamma, float eps, float beta);
int    zcc_triton_step_quantum_walk(zcc_triton_handle_t *handle,
                                    float gamma_coupling, float dt);
double zcc_triton_get_peak_throughput(const zcc_triton_handle_t *handle);

/* ------------------------------------------------------------------ */
/* Pillar 1: DEX Arbitrage — Bellman-Ford parallel route scan          */
/* GPU hot path: warp-parallel float32 max-profit accumulation          */
/* ------------------------------------------------------------------ */
zcc_gpu_status_t zcc_gpu_scan_mempool_routes(
    const float *reserves_a,
    const float *reserves_b,
    float       *out_profit,
    uint32_t     num_routes
);

/* ------------------------------------------------------------------ */
/* Pillar 2: Navier-Stokes advection — 50 M-cell semi-Lagrangian step  */
/* GPU hot path: 2D thread-block bilinear advection on density/vel      */
/* ------------------------------------------------------------------ */
zcc_gpu_status_t zcc_gpu_advect_fluid_cells(
    float    *density,
    float    *vx,
    float    *vy,
    uint32_t  width,
    uint32_t  height,
    float     dt
);

/* ------------------------------------------------------------------ */
/* Pillar 3: Flash-Attention — FP16 Tensor Core SDPA kernel            */
/* GPU hot path: m16n8k16 warp-tile softmax-fused attention             */
/* ------------------------------------------------------------------ */
zcc_gpu_status_t zcc_gpu_forward_flash_attention(
    const void *q_ptr,
    const void *k_ptr,
    const void *v_ptr,
    void       *out_attn_ptr,
    uint32_t    seq_len,
    uint32_t    num_heads,
    uint32_t    head_dim
);

/* ------------------------------------------------------------------ */
/* Pillar 4: ZK-STARK NTT Prover — BabyBear prime polynomial quotient  */
/* GPU hot path: butterfly NTT with 16.7M-gate trace polynomials        */
/* ------------------------------------------------------------------ */
zcc_gpu_status_t zcc_gpu_prove_zk_stark_circuit(
    const uint32_t *trace_poly,
    uint32_t       *out_quotient,
    uint32_t        degree,
    uint64_t        prime_mod,
    uint8_t         out_merkle_root[32]
);

/* ------------------------------------------------------------------ */
/* Full-pipeline GPU dispatch gauntlet (all 4 pillars, returns receipt) */
/* ------------------------------------------------------------------ */
typedef struct {
    zcc_gpu_status_t pillar1_status;
    zcc_gpu_status_t pillar2_status;
    zcc_gpu_status_t pillar3_status;
    zcc_gpu_status_t pillar4_status;
    double pillar1_latency_ns;
    double pillar2_latency_ns;
    double pillar3_latency_ns;
    double pillar4_latency_ns;
    uint8_t stark_merkle_root[32];
    char device_name[128];
    int sm_major;
    int sm_minor;
} zcc_gpu_gauntlet_receipt_t;

zcc_gpu_status_t zcc_triton_dispatch_gpu_gauntlet(
    zcc_gpu_dispatch_ctx_t    *ctx,
    zcc_gpu_gauntlet_receipt_t *receipt
);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_TRITON_BRIDGE_H */
