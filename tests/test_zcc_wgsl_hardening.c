/* SPDX-License-Identifier: MIT */
#include "include/zcc_wgsl.h"
#include "ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int main(void) {
    printf("================================================================================\n");
    printf("  ZCC NATIVE C-TO-WGSL WEBGPU COMPILER: PRE/POST HARDENING GAUNTLET\n");
    printf("================================================================================\n");

    /* -------------------------------------------------------------------------
     * STAGE 1: Vector Addition Kernel (F32) with Bounds Guard
     * ------------------------------------------------------------------------- */
    printf("\n[STAGE 1] Testing Vector Addition Kernel Lowering (vec_add)...");
    ir_module_t *mod1 = ir_module_create();
    ir_func_t *fn1 = ir_func_create(mod1, "vec_add", IR_TY_VOID, 3);
    ir_emit(fn1, IR_ARG, IR_TY_F32, "a", "", "", "", 0, 1);
    ir_emit(fn1, IR_ARG, IR_TY_F32, "b", "", "", "", 0, 1);
    ir_emit(fn1, IR_ARG, IR_TY_F32, "c", "", "", "", 0, 1);
    ir_emit(fn1, IR_LOAD, IR_TY_F32, "va", "a", "", "", 0, 2);
    ir_emit(fn1, IR_LOAD, IR_TY_F32, "vb", "b", "", "", 0, 3);
    ir_emit(fn1, IR_FADD, IR_TY_F32, "vc", "va", "vb", "", 0, 4);
    ir_emit(fn1, IR_STORE, IR_TY_F32, "c", "vc", "", "", 0, 5);
    ir_emit(fn1, IR_RET, IR_TY_VOID, "", "", "", "", 0, 6);

    zcc_wgsl_config_t cfg = zcc_wgsl_default_config();
    char *wgsl1 = ir_module_lower_wgsl_string(mod1, &cfg);
    assert(wgsl1 != NULL);
    assert(strstr(wgsl1, "@compute @workgroup_size(64, 1, 1)") != NULL);
    assert(strstr(wgsl1, "@group(0) @binding(0) var<storage, read_write> buf_a: array<f32>;") != NULL);
    assert(strstr(wgsl1, "let total_elements: u32 = arrayLength(&buf_a);") != NULL);
    assert(strstr(wgsl1, "if (idx >= total_elements)") != NULL);
    assert(strstr(wgsl1, "buf_c[idx] = v_vc;") != NULL);
    printf(" [PASS]\n");

    /* -------------------------------------------------------------------------
     * STAGE 2: FMA Vector Kernel Lowering
     * ------------------------------------------------------------------------- */
    printf("[STAGE 2] Testing FMA Vector Kernel Lowering (vec_fma)...");
    ir_module_t *mod2 = ir_module_create();
    ir_func_t *fn2 = ir_func_create(mod2, "vec_fma", IR_TY_VOID, 4);
    ir_emit(fn2, IR_ARG, IR_TY_F32, "a", "", "", "", 0, 1);
    ir_emit(fn2, IR_ARG, IR_TY_F32, "x", "", "", "", 0, 1);
    ir_emit(fn2, IR_ARG, IR_TY_F32, "b", "", "", "", 0, 1);
    ir_emit(fn2, IR_ARG, IR_TY_F32, "y", "", "", "", 0, 1);
    ir_emit(fn2, IR_LOAD, IR_TY_F32, "va", "a", "", "", 0, 2);
    ir_emit(fn2, IR_LOAD, IR_TY_F32, "vx", "x", "", "", 0, 3);
    ir_emit(fn2, IR_LOAD, IR_TY_F32, "vb", "b", "", "", 0, 4);
    ir_emit(fn2, IR_FMA, IR_TY_F32, "vy", "va", "vx", "vb", 0, 5);
    ir_emit(fn2, IR_STORE, IR_TY_F32, "y", "vy", "", "", 0, 6);
    ir_emit(fn2, IR_RET, IR_TY_VOID, "", "", "", "", 0, 7);

    char *wgsl2 = ir_module_lower_wgsl_string(mod2, &cfg);
    assert(wgsl2 != NULL);
    assert(strstr(wgsl2, "fma(v_va, v_vx, v_vb)") != NULL);
    printf(" [PASS]\n");

    /* -------------------------------------------------------------------------
     * STAGE 3: Bitwise Shift & Comparison Primitives
     * ------------------------------------------------------------------------- */
    printf("[STAGE 3] Testing Bitwise Shifts & Select Comparisons...");
    ir_module_t *mod3 = ir_module_create();
    ir_func_t *fn3 = ir_func_create(mod3, "bitwise_kernel", IR_TY_VOID, 2);
    ir_emit(fn3, IR_ARG, IR_TY_U32, "in_data", "", "", "", 0, 1);
    ir_emit(fn3, IR_ARG, IR_TY_U32, "out_data", "", "", "", 0, 1);
    ir_emit(fn3, IR_LOAD, IR_TY_U32, "vin", "in_data", "", "", 0, 2);
    ir_emit(fn3, IR_SHL, IR_TY_U32, "vshl", "vin", "", "", 7, 3);
    ir_emit(fn3, IR_SHR, IR_TY_U32, "vshr", "vin", "", "", 25, 4);
    ir_emit(fn3, IR_OR, IR_TY_U32, "vrot", "vshl", "vshr", "", 0, 5);
    ir_emit(fn3, IR_GT, IR_TY_U32, "vgt", "vrot", "vin", "", 0, 6);
    ir_emit(fn3, IR_STORE, IR_TY_U32, "out_data", "vgt", "", "", 0, 7);
    ir_emit(fn3, IR_RET, IR_TY_VOID, "", "", "", "", 0, 8);

    char *wgsl3 = ir_module_lower_wgsl_string(mod3, &cfg);
    assert(wgsl3 != NULL);
    assert(strstr(wgsl3, "<< 7u;") != NULL);
    assert(strstr(wgsl3, ">> 25u;") != NULL);
    assert(strstr(wgsl3, "select(0u, 1u, v_vrot > v_vin);") != NULL);
    printf(" [PASS]\n");

    /* -------------------------------------------------------------------------
     * STAGE 4: Pre-Flight Static Syntactic Validator (Positive & Negative Controls)
     * ------------------------------------------------------------------------- */
    printf("[STAGE 4] Testing Pre-Flight Syntactic Validator (EF-6 Controls)...");
    char err[256];
    /* Positive controls: All generated WGSL must pass validation */
    assert(zcc_wgsl_validate_syntax(wgsl1, err, sizeof(err)) == 0);
    assert(zcc_wgsl_validate_syntax(wgsl2, err, sizeof(err)) == 0);
    assert(zcc_wgsl_validate_syntax(wgsl3, err, sizeof(err)) == 0);

    /* Negative Control 1: Unescaped register '%r0' */
    const char *bad_reg = "@compute @workgroup_size(64,1,1) fn main() { %r0 = 1; }";
    assert(zcc_wgsl_validate_syntax(bad_reg, err, sizeof(err)) == -2);

    /* Negative Control 2: Unbalanced braces */
    const char *bad_brace = "@compute @workgroup_size(64,1,1) fn main() { var x = 1; ";
    assert(zcc_wgsl_validate_syntax(bad_brace, err, sizeof(err)) == -4);

    /* Negative Control 3: Missing compute attribute */
    const char *bad_compute = "fn main() { }";
    assert(zcc_wgsl_validate_syntax(bad_compute, err, sizeof(err)) == -5);
    printf(" [PASS]\n");

    /* -------------------------------------------------------------------------
     * STAGE 5: JavaScript WebGPU Dispatch Bridge Generation
     * ------------------------------------------------------------------------- */
    printf("[STAGE 5] Testing JavaScript WebGPU Dispatch Bridge Emission...");
    char *js_bridge = zcc_wgsl_emit_js_bridge_string(fn1, "WGSL_VEC_ADD");
    assert(js_bridge != NULL);
    assert(strstr(js_bridge, "export async function launch_vec_add") != NULL);
    assert(strstr(js_bridge, "device.createComputePipeline") != NULL);
    assert(strstr(js_bridge, "passEncoder.dispatchWorkgroups") != NULL);
    printf(" [PASS]\n");

    printf("\n================================================================================\n");
    printf("  ALL 5 WGSL HARDENING & VALIDATION STAGES: PASS (100%% VERIFIED)\n");
    printf("================================================================================\n");

    free(wgsl1);
    free(wgsl2);
    free(wgsl3);
    free(js_bridge);
    ir_module_free(mod1);
    ir_module_free(mod2);
    ir_module_free(mod3);
    return 0;
}
