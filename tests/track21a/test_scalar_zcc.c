/* SPDX-License-Identifier: MIT */
/* T21-G04 Dual-compiler check: compiled by ./zcc against runtime/compute_scalar.c */
#include "zcc_compute.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

extern float fmaf(float a, float b, float c);

int main(void) {
    float a[8], b[8], c[8], out[8];
    int i;
    for (i = 0; i < 8; i++) {
        a[i] = (float)(i + 1) * 0.25f;
        b[i] = (float)(8 - i) * 0.5f;
        c[i] = 1.0f;
        out[i] = 0.0f;
    }

    zcc_tensor_desc_t ins[3], outd;
    memset(ins, 0, sizeof(ins));
    memset(&outd, 0, sizeof(outd));
    for (i = 0; i < 3; i++) {
        ins[i].struct_size = (uint32_t)sizeof(zcc_tensor_desc_t);
        ins[i].dtype = ZCC_DTYPE_F32;
        ins[i].rank = 1U;
        ins[i].ownership_flags = ZCC_OWNER_BORROWED_HOST;
        ins[i].shape[0] = 8;
        ins[i].nbytes = sizeof(a);
        ins[i].alignment = 4U;
    }
    ins[0].data = a;
    ins[1].data = b;
    ins[2].data = c;

    outd.struct_size = (uint32_t)sizeof(zcc_tensor_desc_t);
    outd.dtype = ZCC_DTYPE_F32;
    outd.rank = 1U;
    outd.ownership_flags = ZCC_OWNER_BORROWED_HOST;
    outd.shape[0] = 8;
    outd.nbytes = sizeof(out);
    outd.alignment = 4U;
    outd.data = out;

    zcc_exec_options_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.abi_version = ZCC_COMPUTE_ABI_VERSION;
    opts.struct_size = (uint32_t)sizeof(opts);
    opts.accum_policy = ZCC_ACCUM_FP64_REF;
    opts.rmsnorm_eps = 1e-5f;

    char err[64];
    int rc = zcc_scalar_exec(ZCC_OP_VEC_FMA_F32, ins, 3U, &outd, &opts, err, sizeof(err));
    if (rc != 0) {
        fprintf(stderr, "zcc scalar exec failed: %d\n", rc);
        return 1;
    }

    float dot_out = 0.0f;
    zcc_tensor_desc_t dotd;
    memset(&dotd, 0, sizeof(dotd));
    dotd.struct_size = (uint32_t)sizeof(dotd);
    dotd.dtype = ZCC_DTYPE_F32;
    dotd.rank = 1U;
    dotd.ownership_flags = ZCC_OWNER_BORROWED_HOST;
    dotd.shape[0] = 1;
    dotd.nbytes = sizeof(float);
    dotd.alignment = 4U;
    dotd.data = &dot_out;

    rc = zcc_scalar_exec(ZCC_OP_DOT_F32, ins, 2U, &dotd, &opts, err, sizeof(err));
    if (rc != 0) return 2;

    printf("ZCC_SCALAR_PORTABLE_OK fma3=%.6f dot=%.6f\n", (double)out[3], (double)dot_out);
    return 0;
}
