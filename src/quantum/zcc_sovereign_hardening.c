/* ========================================================================= */
/* ZCC SOVEREIGN HARDENING SHIELD IMPLEMENTATION                             */
/* ========================================================================= */
/* File: src/quantum/zcc_sovereign_hardening.c                               */
/* ========================================================================= */

#include "zcc_sovereign_hardening.h"
#include <string.h>
#include <math.h>

/* ------------------------------------------------------------------------- */
/* H1: STACK CANARY PLANTING & VERIFICATION                                  */
/* ------------------------------------------------------------------------- */

void zcc_hardening_init(zcc_hardening_context_t *ctx) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    
    for (int i = 0; i < ZCC_CANARY_SLOTS; i++) {
        ctx->prologue_canaries[i] = ZCC_CANARY_MAGIC_PROLOGUE ^ ((uint64_t)(i + 1) * 0x9e3779b97f4a7c15ULL);
        ctx->epilogue_canaries[i] = ZCC_CANARY_MAGIC_EPILOGUE ^ ((uint64_t)(i + 1) * 0x517cc1b727220a95ULL);
    }
    ctx->monotonic_epoch = 1;
    ctx->strict_bounds_enforced = true;
}

bool zcc_hardening_verify_canaries(const zcc_hardening_context_t *ctx) {
    if (!ctx) return false;

    for (int i = 0; i < ZCC_CANARY_SLOTS; i++) {
        uint64_t expected_prologue = ZCC_CANARY_MAGIC_PROLOGUE ^ ((uint64_t)(i + 1) * 0x9e3779b97f4a7c15ULL);
        uint64_t expected_epilogue = ZCC_CANARY_MAGIC_EPILOGUE ^ ((uint64_t)(i + 1) * 0x517cc1b727220a95ULL);

        if (ctx->prologue_canaries[i] != expected_prologue) return false;
        if (ctx->epilogue_canaries[i] != expected_epilogue) return false;
    }

    return true;
}

/* ------------------------------------------------------------------------- */
/* H2: CHECKED ARITHMETIC WITH ZERO-UB OVERFLOW TRAPPING                     */
/* ------------------------------------------------------------------------- */

bool zcc_safe_add_u64(uint64_t a, uint64_t b, uint64_t *out) {
    if (!out) return false;
    if (__builtin_add_overflow(a, b, out)) {
        *out = UINT64_MAX; // Safe saturation
        return false;
    }
    return true;
}

bool zcc_safe_mul_u64(uint64_t a, uint64_t b, uint64_t *out) {
    if (!out) return false;
    if (__builtin_mul_overflow(a, b, out)) {
        *out = UINT64_MAX; // Safe saturation
        return false;
    }
    return true;
}

bool zcc_safe_sub_u64(uint64_t a, uint64_t b, uint64_t *out) {
    if (!out) return false;
    if (__builtin_sub_overflow(a, b, out)) {
        *out = 0; // Safe floor
        return false;
    }
    return true;
}

/* ------------------------------------------------------------------------- */
/* H3: STRICT MEMORY BOUNDS & POINTER TAGGING                                */
/* ------------------------------------------------------------------------- */

zcc_mem_guard_t zcc_mem_guard_create(void *ptr, size_t size, bool immutable) {
    zcc_mem_guard_t g;
    g.base_addr = (uintptr_t)ptr;
    g.allocated_size = size;
    g.is_immutable = immutable;
    // FNV-1a checksum tag on metadata
    g.checksum_tag = (g.base_addr ^ (uint64_t)size) * 0x100000001b3ULL;
    return g;
}

bool zcc_mem_guard_validate_access(const zcc_mem_guard_t *guard, const void *ptr, size_t access_len, bool write) {
    if (!guard || !ptr) return false;
    
    // Verify guard integrity tag
    uint64_t expected_tag = (guard->base_addr ^ (uint64_t)guard->allocated_size) * 0x100000001b3ULL;
    if (guard->checksum_tag != expected_tag) return false;

    // Check immutability
    if (write && guard->is_immutable) return false;

    uintptr_t target = (uintptr_t)ptr;
    uintptr_t limit = guard->base_addr + guard->allocated_size;

    // Strict boundary checks
    if (target < guard->base_addr) return false;
    if (target + access_len > limit || (target + access_len < target)) return false;

    return true;
}

/* ------------------------------------------------------------------------- */
/* H4: MONOTONIC EPOCH INVALIDATION (XOR REVOCATION)                         */
/* ------------------------------------------------------------------------- */

uint64_t zcc_epoch_advance(zcc_hardening_context_t *ctx) {
    if (!ctx) return 0;
    return ++ctx->monotonic_epoch;
}

bool zcc_epoch_validate(const zcc_hardening_context_t *ctx, uint64_t token_epoch) {
    if (!ctx) return false;
    return (token_epoch == ctx->monotonic_epoch);
}

/* ------------------------------------------------------------------------- */
/* H5: FLOAT & DOUBLE ARRAY SANITIZATION (NaN, Inf, Out-Of-Bounds Traps)     */
/* ------------------------------------------------------------------------- */

bool zcc_harden_float_array(float *arr, size_t len, float clamp_min, float clamp_max) {
    if (!arr || len == 0 || clamp_min > clamp_max) return false;

    bool modified = false;
    for (size_t i = 0; i < len; i++) {
        if (isnan(arr[i]) || isinf(arr[i])) {
            arr[i] = 0.0f;
            modified = true;
        } else if (arr[i] < clamp_min) {
            arr[i] = clamp_min;
            modified = true;
        } else if (arr[i] > clamp_max) {
            arr[i] = clamp_max;
            modified = true;
        }
    }
    return !modified;
}

bool zcc_harden_double_array(double *arr, size_t len, double clamp_min, double clamp_max) {
    if (!arr || len == 0 || clamp_min > clamp_max) return false;

    bool modified = false;
    for (size_t i = 0; i < len; i++) {
        if (isnan(arr[i]) || isinf(arr[i])) {
            arr[i] = 0.0;
            modified = true;
        } else if (arr[i] < clamp_min) {
            arr[i] = clamp_min;
            modified = true;
        } else if (arr[i] > clamp_max) {
            arr[i] = clamp_max;
            modified = true;
        }
    }
    return !modified;
}
