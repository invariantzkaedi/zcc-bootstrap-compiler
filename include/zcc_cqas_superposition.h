#ifndef ZCC_CQAS_SUPERPOSITION_H
#define ZCC_CQAS_SUPERPOSITION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CQAS_ALIGN 64
#define CQAS_MAX_VARIANTS 4
#define CQAS_NODES 16

/* Variant classification */
typedef enum {
    CQAS_VARIANT_UNSELECTED  = 0,
    CQAS_VARIANT_AVX2_FMA    = 1, /* Vectorized 256-bit AVX2 + Reciprocal-FMA */
    CQAS_VARIANT_SCALAR_SLIP = 2, /* 64-byte L1D Cache-Slip Zero-Spill */
    CQAS_VARIANT_DIRECT_TAIL = 3  /* Direct-Threaded Jump Table */
} cqas_variant_t;

/* Status codes */
typedef enum {
    CQAS_OK = 0,
    CQAS_ERR_NULL = -1,
    CQAS_ERR_NO_VARIANTS = -2,
    CQAS_ERR_PATCH_FAILED = -3,
    CQAS_ERR_DRIFT = -4
} cqas_status_t;

/* Superposition Variant Descriptor */
typedef struct {
    cqas_variant_t type;
    const char *name;
    void *entry_point;
    uint64_t execution_count;
    float avg_latency_ns;
    float l1_cache_miss_rate;
} cqas_variant_descriptor_t;

/* Quantum Superposition Dispatch Block */
typedef struct {
    uint8_t patch_site[16] __attribute__((aligned(CQAS_ALIGN))); /* 16-byte aligned trampoline */
    cqas_variant_descriptor_t variants[CQAS_MAX_VARIANTS];
    size_t num_variants;
    cqas_variant_t selected_variant;
    float wave_amplitudes_re[CQAS_NODES];
    float wave_amplitudes_im[CQAS_NODES];
    float wave_probabilities[CQAS_NODES];
    uint32_t collapse_epoch;
    bool is_collapsed;
} cqas_dispatch_block_t;

/* Initialize Quantum Superposition Dispatch Block */
cqas_status_t cqas_init_block(cqas_dispatch_block_t *block);

/* Register a Variant into the Superposition Block */
cqas_status_t cqas_register_variant(cqas_dispatch_block_t *block,
                                    cqas_variant_t type,
                                    const char *name,
                                    void *entry_point);

/* Sub-Nanosecond Quantum Walk Wave-Packet Collapse */
cqas_variant_t cqas_collapse_wave_packet(cqas_dispatch_block_t *block,
                                         float l1_pressure,
                                         float branch_pressure);

/* Apply in-memory atomic patch to direct jump */
cqas_status_t cqas_apply_atomic_patch(cqas_dispatch_block_t *block);

/* Zero-Indirection Direct Call-Site In-Place Rewriting (0xE8 <rel32>) */
cqas_status_t cqas_patch_direct_callsite(void *callsite_addr, void *target_fn);

/* Execute Dispatch Call */
typedef void (*cqas_kernel_fn)(const float *in, float *out, size_t n);
void cqas_dispatch_execute(cqas_dispatch_block_t *block, const float *in, float *out, size_t n);

/* Log Provenance to .zcc.oracle ledger */
cqas_status_t cqas_record_provenance(const cqas_dispatch_block_t *block, const char *oracle_path);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_CQAS_SUPERPOSITION_H */
