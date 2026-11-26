/* SPDX-License-Identifier: MIT */
/* Standalone ZCC SPIR-V CLI frontend exercising src/part6_spirv.c without modifying PARTS */
#include "zcc_compute.h"
#include "zcc_spirv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_op(const char *s) {
    if (strcmp(s, "VEC_ADD_F32") == 0 || strcmp(s, "1") == 0) return ZCC_OP_VEC_ADD_F32;
    if (strcmp(s, "VEC_MUL_F32") == 0 || strcmp(s, "2") == 0) return ZCC_OP_VEC_MUL_F32;
    if (strcmp(s, "VEC_FMA_F32") == 0 || strcmp(s, "3") == 0) return ZCC_OP_VEC_FMA_F32;
    if (strcmp(s, "DOT_F32") == 0 || strcmp(s, "4") == 0) return ZCC_OP_DOT_F32;
    if (strcmp(s, "GEMM_F32") == 0 || strcmp(s, "5") == 0) return ZCC_OP_GEMM_F32;
    if (strcmp(s, "RMSNORM_F32") == 0 || strcmp(s, "6") == 0) return ZCC_OP_RMSNORM_F32;
    if (strcmp(s, "SOFTMAX_F32") == 0 || strcmp(s, "7") == 0) return ZCC_OP_SOFTMAX_F32;
    if (strcmp(s, "DEQUANT_Q8_0") == 0 || strcmp(s, "8") == 0) return ZCC_OP_DEQUANT_Q8_0;
    if (strcmp(s, "MATVEC_Q8_0") == 0 || strcmp(s, "9") == 0) return ZCC_OP_MATVEC_Q8_0;
    if (strcmp(s, "DEQUANT_Q4_0") == 0 || strcmp(s, "10") == 0) return ZCC_OP_DEQUANT_Q4_0;
    if (strcmp(s, "MATVEC_Q4_0") == 0 || strcmp(s, "11") == 0) return ZCC_OP_MATVEC_Q4_0;
    if (strcmp(s, "DEQUANT_Q4_K") == 0 || strcmp(s, "12") == 0) return ZCC_OP_DEQUANT_Q4_K;
    if (strcmp(s, "MATVEC_Q4_K") == 0 || strcmp(s, "13") == 0) return ZCC_OP_MATVEC_Q4_K;
    return 0;
}

static const char *op_name(uint32_t op_id) {
    static const char *names[14] = {
        "UNKNOWN", "VEC_ADD_F32", "VEC_MUL_F32", "VEC_FMA_F32", "DOT_F32",
        "GEMM_F32", "RMSNORM_F32", "SOFTMAX_F32", "DEQUANT_Q8_0", "MATVEC_Q8_0",
        "DEQUANT_Q4_0", "MATVEC_Q4_0", "DEQUANT_Q4_K", "MATVEC_Q4_K"
    };
    return (op_id >= 1U && op_id <= 13U) ? names[op_id] : "UNKNOWN";
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: zcc-spirv --emit-op <op> -o <out.spv>\n"
                        "       zcc-spirv --emit-all <dir>\n");
        return 2;
    }

    if (strcmp(argv[1], "--emit-all") == 0) {
        const char *dir = argv[2];
        for (uint32_t op = ZCC_OP_VEC_ADD_F32; op <= ZCC_OP_MATVEC_Q4_K; op++) {
            zcc_spv_module_t mod;
            if (zcc_spirv_emit_op_kernel(op, &mod) != 0) {
                fprintf(stderr, "FAIL emit op=%u (%s)\n", op, op_name(op));
                return 1;
            }
            char path[512];
            snprintf(path, sizeof(path), "%s/t21b_op%02u_%s.spv", dir, op, op_name(op));
            int rc = zcc_spirv_write_file(path, &mod);
            printf("ZCC_SPIRV_EMIT op=%02u name=%-14s words=%4zu bytes=%5zu local=(%u,%u,%u) out=%s rc=%d\n",
                   op, op_name(op), mod.word_count, mod.byte_size, mod.local_x, mod.local_y, mod.local_z, path, rc);
            zcc_spirv_module_free(&mod);
            if (rc != 0) return 1;
        }
        return 0;
    }

    if (strcmp(argv[1], "--emit-op") == 0 && argc >= 5 && strcmp(argv[3], "-o") == 0) {
        int op = parse_op(argv[2]);
        if (op <= 0) {
            fprintf(stderr, "Unknown op '%s'\n", argv[2]);
            return 2;
        }
        zcc_spv_module_t mod;
        if (zcc_spirv_emit_op_kernel((uint32_t)op, &mod) != 0) return 1;
        int rc = zcc_spirv_write_file(argv[4], &mod);
        zcc_spirv_module_free(&mod);
        return rc;
    }

    fprintf(stderr, "Invalid arguments\n");
    return 2;
}
