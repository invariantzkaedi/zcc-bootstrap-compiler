/* ========================================================================= */
/* ZCC SOVEREIGN HARDENING SHIELD (H1-H5)                                    */
/* ========================================================================= */
/* File: include/zcc_sovereign_hardening.h                                  */
/* Description: Universal Invariant Verification, Memory Canary Rings,       */
/*              Bounds Enforcement, and ROP/JOP Execution Traps.             */
/* ========================================================================= */

#ifndef ZCC_SOVEREIGN_HARDENING_H
#define ZCC_SOVEREIGN_HARDENING_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZCC_CANARY_MAGIC_PROLOGUE  0x7F5A434353484C44ULL /* "\x7f\ZCCSHLD" */
#define ZCC_CANARY_MAGIC_EPILOGUE  0x5A43434755415244ULL /* "ZCCGUARD" */
#define ZCC_CANARY_SLOTS           8

/* Hardened Execution Context */
typedef struct {
    uint64_t prologue_canaries[ZCC_CANARY_SLOTS];
    uint64_t epilogue_canaries[ZCC_CANARY_SLOTS];
    uint64_t monotonic_epoch;
    uint32_t memory_violations_trapped;
    uint32_t arithmetic_overflows_trapped;
    bool     strict_bounds_enforced;
} zcc_hardening_context_t;

/* Memory Boundary Guard */
typedef struct {
    uintptr_t base_addr;
    size_t    allocated_size;
    uint64_t  checksum_tag;
    bool      is_immutable;
} zcc_mem_guard_t;

/* ------------------------------------------------------------------------- */
/* FUNCTION PROTOTYPES (H1 - H5)                                             */
/* ------------------------------------------------------------------------- */

/* H1: Initialize Hardening Shield & Plant Stack Canaries */
void zcc_hardening_init(zcc_hardening_context_t *ctx);
bool zcc_hardening_verify_canaries(const zcc_hardening_context_t *ctx);

/* H2: Checked Arithmetic & Saturation Operations (Zero-UB Overflows) */
bool zcc_safe_add_u64(uint64_t a, uint64_t b, uint64_t *out);
bool zcc_safe_mul_u64(uint64_t a, uint64_t b, uint64_t *out);
bool zcc_safe_sub_u64(uint64_t a, uint64_t b, uint64_t *out);

/* H3: Strict Memory Bounds & Pointer Tagging */
zcc_mem_guard_t zcc_mem_guard_create(void *ptr, size_t size, bool immutable);
bool zcc_mem_guard_validate_access(const zcc_mem_guard_t *guard, const void *ptr, size_t access_len, bool write);

/* H4: Monotonic Epoch Invalidation (Sub-Nanosecond XOR Revocation) */
uint64_t zcc_epoch_advance(zcc_hardening_context_t *ctx);
bool zcc_epoch_validate(const zcc_hardening_context_t *ctx, uint64_t token_epoch);

/* H5: Sanitize Array Inputs (NaN, Infinity, Pointer Null Traps) */
bool zcc_harden_float_array(float *arr, size_t len, float clamp_min, float clamp_max);
bool zcc_harden_double_array(double *arr, size_t len, double clamp_min, double clamp_max);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_SOVEREIGN_HARDENING_H */
