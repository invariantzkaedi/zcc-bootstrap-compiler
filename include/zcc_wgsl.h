/* SPDX-License-Identifier: MIT */
#ifndef ZCC_WGSL_H
#define ZCC_WGSL_H

/**
 * @file zcc_wgsl.h
 * @brief ZCC Native SSA IR to WebGPU Shading Language (WGSL) Compute Shader Emitter
 *        Pre-flight validated, bounds-guarded, and hardened for WebGPU Core 1.0.
 */

#include "ir.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int workgroup_size_x;
    int workgroup_size_y;
    int workgroup_size_z;
    int enable_f16;
    int max_storage_buffers;
    int enable_bounds_guard;      /* Emit runtime bounds guard: if (idx >= arrayLength(&buf)) return; */
    int enable_vector_types;      /* Lower 4-wide vectors directly to native WGSL vec4<f32> */
    int enable_crypto_helpers;    /* Emit WGSL helper functions for rotl, rotr, ch, maj */
} zcc_wgsl_config_t;

/**
 * @brief Default WGSL configuration targeting workgroup_size(64, 1, 1) with bounds guards.
 */
zcc_wgsl_config_t zcc_wgsl_default_config(void);

/**
 * @brief High-performance WGSL configuration targeting 256-thread workgroups.
 */
zcc_wgsl_config_t zcc_wgsl_perf_config(void);

/**
 * @brief Lowers a ZCC IR Module directly to WGSL Compute Shader code.
 * @param mod Input ZCC IR module
 * @param out Destination FILE stream
 * @param cfg WGSL generation configuration
 * @return 0 on success, negative error code on failure
 */
int ir_module_lower_wgsl(const ir_module_t *mod, FILE *out, const zcc_wgsl_config_t *cfg);

/**
 * @brief Convenience function to lower an IR Module to a heap-allocated WGSL string.
 */
char *ir_module_lower_wgsl_string(const ir_module_t *mod, const zcc_wgsl_config_t *cfg);

/**
 * @brief Convenience function to lower a single IR Function to a heap-allocated WGSL string.
 */
char *zcc_wgsl_lower_func_string(const ir_func_t *fn, const zcc_wgsl_config_t *cfg);

/**
 * @brief Pre-flight static syntax and security validator for emitted WGSL code.
 *        Guarantees absence of illegal tokens (e.g. unescaped '%'), balanced braces,
 *        valid storage buffer annotations, and shift-count type compliance.
 * @param wgsl_code WGSL shader code string
 * @param err_buf Buffer to receive diagnostic message on failure (optional)
 * @param err_sz Size of err_buf
 * @return 0 on success (valid WGSL), negative code on error
 */
int zcc_wgsl_validate_syntax(const char *wgsl_code, char *err_buf, size_t err_sz);

/**
 * @brief Emits a lightweight JavaScript WebGPU pipeline dispatch bridge.
 * @param fn Kernel function
 * @param wgsl_var_name JavaScript variable name containing the WGSL shader code
 * @param out Destination FILE stream
 * @return 0 on success, negative error code on failure
 */
int zcc_wgsl_emit_js_bridge(const ir_func_t *fn, const char *wgsl_var_name, FILE *out);

/**
 * @brief Convenience function to emit JS bridge as heap-allocated string.
 */
char *zcc_wgsl_emit_js_bridge_string(const ir_func_t *fn, const char *wgsl_var_name);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_WGSL_H */
