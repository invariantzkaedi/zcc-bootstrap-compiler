/* SPDX-License-Identifier: MIT */
#include "include/zcc_wgsl.h"
#include "ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --kernel <name>       Target kernel: vec_add, vec_fma, sha256_round, quantum_step\n");
    fprintf(stderr, "  --workgroup <x,y,z>   Workgroup dimensions (default: 64,1,1)\n");
    fprintf(stderr, "  --bounds-guard        Enable runtime bounds guard against buffer overflows\n");
    fprintf(stderr, "  --emit-js             Also emit JavaScript WebGPU pipeline launcher\n");
    fprintf(stderr, "  --out <file>          Output file for WGSL shader (default: stdout)\n");
    fprintf(stderr, "  --validate            Run pre-flight syntactic and safety validator\n");
}

int main(int argc, char **argv) {
    const char *kernel = "vec_add";
    const char *out_file = NULL;
    int emit_js = 0;
    int do_validate = 1;
    zcc_wgsl_config_t cfg = zcc_wgsl_default_config();

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--kernel") == 0 && i + 1 < argc) {
            kernel = argv[++i];
        } else if (strcmp(argv[i], "--workgroup") == 0 && i + 1 < argc) {
            sscanf(argv[++i], "%d,%d,%d", &cfg.workgroup_size_x, &cfg.workgroup_size_y, &cfg.workgroup_size_z);
        } else if (strcmp(argv[i], "--bounds-guard") == 0) {
            cfg.enable_bounds_guard = 1;
        } else if (strcmp(argv[i], "--emit-js") == 0) {
            emit_js = 1;
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out_file = argv[++i];
        } else if (strcmp(argv[i], "--validate") == 0) {
            do_validate = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    ir_module_t *mod = ir_module_create();
    if (!mod) {
        fprintf(stderr, "[ZCC WGSL] Error: Failed to allocate IR module\n");
        return 1;
    }

    ir_func_t *fn = NULL;

    if (strcmp(kernel, "vec_add") == 0) {
        fn = ir_func_create(mod, "vec_add", IR_TY_VOID, 3);
        ir_emit(fn, IR_ARG, IR_TY_F32, "a", "", "", "", 0, 1);
        ir_emit(fn, IR_ARG, IR_TY_F32, "b", "", "", "", 0, 1);
        ir_emit(fn, IR_ARG, IR_TY_F32, "c", "", "", "", 0, 1);
        ir_emit(fn, IR_LOAD, IR_TY_F32, "va", "a", "", "", 0, 2);
        ir_emit(fn, IR_LOAD, IR_TY_F32, "vb", "b", "", "", 0, 3);
        ir_emit(fn, IR_FADD, IR_TY_F32, "vc", "va", "vb", "", 0, 4);
        ir_emit(fn, IR_STORE, IR_TY_F32, "c", "vc", "", "", 0, 5);
        ir_emit(fn, IR_RET, IR_TY_VOID, "", "", "", "", 0, 6);
    } else if (strcmp(kernel, "vec_fma") == 0) {
        fn = ir_func_create(mod, "vec_fma", IR_TY_VOID, 4);
        ir_emit(fn, IR_ARG, IR_TY_F32, "a", "", "", "", 0, 1);
        ir_emit(fn, IR_ARG, IR_TY_F32, "x", "", "", "", 0, 1);
        ir_emit(fn, IR_ARG, IR_TY_F32, "b", "", "", "", 0, 1);
        ir_emit(fn, IR_ARG, IR_TY_F32, "y", "", "", "", 0, 1);
        ir_emit(fn, IR_LOAD, IR_TY_F32, "va", "a", "", "", 0, 2);
        ir_emit(fn, IR_LOAD, IR_TY_F32, "vx", "x", "", "", 0, 3);
        ir_emit(fn, IR_LOAD, IR_TY_F32, "vb", "b", "", "", 0, 4);
        ir_emit(fn, IR_FMA, IR_TY_F32, "vy", "va", "vx", "vb", 0, 5);
        ir_emit(fn, IR_STORE, IR_TY_F32, "y", "vy", "", "", 0, 6);
        ir_emit(fn, IR_RET, IR_TY_VOID, "", "", "", "", 0, 7);
    } else if (strcmp(kernel, "quantum_step") == 0) {
        fn = ir_func_create(mod, "quantum_lattice_step", IR_TY_VOID, 3);
        ir_emit(fn, IR_ARG, IR_TY_F32, "psi_real", "", "", "", 0, 1);
        ir_emit(fn, IR_ARG, IR_TY_F32, "psi_imag", "", "", "", 0, 1);
        ir_emit(fn, IR_ARG, IR_TY_F32, "pot_well", "", "", "", 0, 1);
        ir_emit(fn, IR_LOAD, IR_TY_F32, "vr", "psi_real", "", "", 0, 2);
        ir_emit(fn, IR_LOAD, IR_TY_F32, "vi", "psi_imag", "", "", 0, 3);
        ir_emit(fn, IR_LOAD, IR_TY_F32, "vpot", "pot_well", "", "", 0, 4);
        ir_emit(fn, IR_FMUL, IR_TY_F32, "decay", "vr", "vpot", "", 0, 5);
        ir_emit(fn, IR_FSUB, IR_TY_F32, "next_r", "vr", "decay", "", 0, 6);
        ir_emit(fn, IR_STORE, IR_TY_F32, "psi_real", "next_r", "", "", 0, 7);
        ir_emit(fn, IR_RET, IR_TY_VOID, "", "", "", "", 0, 8);
    } else {
        fprintf(stderr, "[ZCC WGSL] Unknown kernel: %s\n", kernel);
        ir_module_free(mod);
        return 1;
    }

    char *wgsl = ir_module_lower_wgsl_string(mod, &cfg);
    if (!wgsl) {
        fprintf(stderr, "[ZCC WGSL] Error: Failed to generate WGSL\n");
        ir_module_free(mod);
        return 2;
    }

    if (do_validate) {
        char err[256];
        int v_rc = zcc_wgsl_validate_syntax(wgsl, err, sizeof(err));
        if (v_rc != 0) {
            fprintf(stderr, "[ZCC WGSL] Pre-flight Validation Failed: %s\n", err);
            free(wgsl);
            ir_module_free(mod);
            return 3;
        }
    }

    if (out_file) {
        FILE *fp = fopen(out_file, "w");
        if (!fp) {
            fprintf(stderr, "[ZCC WGSL] Error: Cannot open output file %s\n", out_file);
            free(wgsl);
            ir_module_free(mod);
            return 4;
        }
        fputs(wgsl, fp);
        fclose(fp);
        printf("[+] Emitted WGSL Shader -> %s\n", out_file);
    } else {
        printf("%s\n", wgsl);
    }

    if (emit_js && fn) {
        char *js = zcc_wgsl_emit_js_bridge_string(fn, "WGSL_SHADER_CODE");
        if (js) {
            if (out_file) {
                char js_path[256];
                snprintf(js_path, sizeof(js_path), "%s.js", out_file);
                FILE *jfp = fopen(js_path, "w");
                if (jfp) {
                    fputs(js, jfp);
                    fclose(jfp);
                    printf("[+] Emitted JavaScript WebGPU Bridge -> %s\n", js_path);
                }
            } else {
                printf("\n// --- WebGPU JavaScript Bridge ---\n%s\n", js);
            }
            free(js);
        }
    }

    free(wgsl);
    ir_module_free(mod);
    return 0;
}
