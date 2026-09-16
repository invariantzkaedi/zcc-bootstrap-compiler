/* ========================================================================= */
/* ZCC ONEIROKERNEL: AUTONOMOUS RUNTIME RE-WRITING MICRO-JIT                  */
/* ========================================================================= */
/* File: src/dynamic/oneiro_kernel.h                                         */
/* Description: Dynamic in-memory live code patching, stack rewinding,       */
/*              and adaptive telemetry-guided execution polymorphism.       */
/* ========================================================================= */

#ifndef ZCC_ONEIRO_KERNEL_H
#define ZCC_ONEIRO_KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ONEIRO_STATE_COLD = 0,
    ONEIRO_STATE_WARMING,
    ONEIRO_STATE_HOT_PATCHED,
    ONEIRO_STATE_AVX512_FUSED
} OneiroPatchState;

typedef struct {
    uint32_t         site_id;
    const char      *function_name;
    uint8_t         *code_ptr;
    size_t           code_size;
    uint64_t         invocation_count;
    double           avg_latency_ns;
    OneiroPatchState state;
    uint32_t         patches_applied;
} OneiroHotPatchSite;

typedef struct {
    OneiroHotPatchSite sites[64];
    uint32_t           n_sites;
    uint64_t           total_patches_applied;
    double             global_speedup_multiplier;
} OneiroKernelContext;

/* Initialize OneiroKernel runtime manager */
OneiroKernelContext *oneiro_kernel_init(void);
void oneiro_kernel_destroy(OneiroKernelContext *ctx);

/* Register executable site for autonomous telemetry monitoring */
uint32_t oneiro_kernel_register_site(
    OneiroKernelContext *ctx,
    const char          *func_name,
    uint8_t             *code_ptr,
    size_t               code_size
);

/* Feed telemetry sample and trigger in-memory code rewrite if hot threshold exceeded */
bool oneiro_kernel_sample_feedback(
    OneiroKernelContext *ctx,
    uint32_t             site_id,
    double               exec_time_ns
);

/* Apply in-memory binary patch with hardware memory protection toggle */
bool oneiro_kernel_apply_patch(OneiroKernelContext *ctx, uint32_t site_id, const uint8_t *new_code, size_t new_len);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_ONEIRO_KERNEL_H */
