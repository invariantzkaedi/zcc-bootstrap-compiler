/* SPDX-License-Identifier: MIT */
#ifndef ZCC_SPIRV_H
#define ZCC_SPIRV_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZCC_SPIRV_MAGIC_NUMBER 0x07230203U
#define ZCC_SPIRV_VERSION_1_0  0x00010000U
#define ZCC_SPIRV_GENERATOR_ID 0x5A434300U /* "ZCC\0" */

/* Canonical 64-byte PushConstant layout shared between src/part6_spirv.c and runtime/compute_vulkan.c */
typedef struct zcc_spv_push_s {
    uint32_t dim_m;
    uint32_t dim_n;
    uint32_t dim_k;
    uint32_t rms_eps_bits;
    uint32_t stride_a0;
    uint32_t stride_a1;
    uint32_t stride_b0;
    uint32_t stride_b1;
    uint32_t stride_c0;
    uint32_t stride_c1;
    uint32_t stride_o0;
    uint32_t stride_o1;
    uint32_t off_a_bytes;
    uint32_t off_b_bytes;
    uint32_t off_c_bytes;
    uint32_t off_o_bytes;
} zcc_spv_push_t;

typedef struct zcc_spv_module_s {
    uint32_t *words;
    size_t    word_count;
    size_t    byte_size;
    uint32_t  local_x;
    uint32_t  local_y;
    uint32_t  local_z;
    char      entry_name[32];
} zcc_spv_module_t;

/* Emit deterministic SPIR-V 1.0 binary compute shader for a Track 21A/21B operation ID (1..13).
 * Uses only SpvCapabilityShader + Uniform BufferBlock uint32_t descriptors + 64B PushConstant.
 * Returns 0 on success, non-zero on invalid op or allocation failure. */
int zcc_spirv_emit_op_kernel(uint32_t op_id, zcc_spv_module_t *out_mod);

/* Free allocated SPIR-V word buffer. */
void zcc_spirv_module_free(zcc_spv_module_t *mod);

/* Write emitted SPIR-V module cleanly to a .spv file on disk (returns 0 on success). */
int zcc_spirv_write_file(const char *path, const zcc_spv_module_t *mod);

/* Determine Vulkan workgroup dispatch counts (gx, gy, gz) matching emitted LocalSize for (op_id, M, N, K). */
void zcc_spirv_dispatch_grid(uint32_t op_id, uint32_t m, uint32_t n, uint32_t k,
                             uint32_t *out_gx, uint32_t *out_gy, uint32_t *out_gz);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_SPIRV_H */
