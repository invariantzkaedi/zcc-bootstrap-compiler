/* SPDX-License-Identifier: MIT */
#define _POSIX_C_SOURCE 200809L
#include "zcc_compute.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t monotonic_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0ULL;
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static void set_err(char *buf, size_t cap, const char *msg) {
    if (buf && cap > 0U) snprintf(buf, cap, "%s", msg ? msg : "");
}

void zcc_exec_options_init(zcc_exec_options_t *opts) {
    if (!opts) return;
    memset(opts, 0, sizeof(*opts));
    opts->abi_version = ZCC_COMPUTE_ABI_VERSION;
    opts->struct_size = (uint32_t)sizeof(*opts);
    opts->requested_backend = ZCC_BACKEND_AUTO;
    opts->selection_policy = ZCC_POLICY_EXACT_REQUIRED;
    opts->accum_policy = ZCC_ACCUM_FP32_FMA;
    opts->determinism_mode = ZCC_DET_FIXED_REDUCTION;
    opts->seed = 42ULL;
    opts->rmsnorm_eps = 1e-5f;
    opts->emit_receipt = 1U;
}

void zcc_tensor_desc_init_1d(zcc_tensor_desc_t *t, zcc_dtype_t dtype, int64_t n, void *data, uint64_t nbytes, uint32_t align, uint32_t owner_flags) {
    if (!t) return;
    memset(t, 0, sizeof(*t));
    t->struct_size = (uint32_t)sizeof(*t);
    t->dtype = (uint32_t)dtype;
    t->rank = 1U;
    t->ownership_flags = owner_flags ? owner_flags : ZCC_OWNER_BORROWED_HOST;
    t->shape[0] = n;
    t->data = data;
    t->nbytes = nbytes;
    t->alignment = align ? align : 1U;
}

void zcc_tensor_desc_init_2d(zcc_tensor_desc_t *t, zcc_dtype_t dtype, int64_t rows, int64_t cols, int64_t row_stride_bytes, void *data, uint64_t nbytes, uint32_t align, uint32_t owner_flags) {
    if (!t) return;
    memset(t, 0, sizeof(*t));
    t->struct_size = (uint32_t)sizeof(*t);
    t->dtype = (uint32_t)dtype;
    t->rank = 2U;
    t->ownership_flags = owner_flags ? owner_flags : ZCC_OWNER_BORROWED_HOST;
    t->shape[0] = rows;
    t->shape[1] = cols;
    t->strides[0] = row_stride_bytes;
    t->data = data;
    t->nbytes = nbytes;
    t->alignment = align ? align : 1U;
}

const char *zcc_status_str(zcc_status_t status) {
    switch (status) {
        case ZCC_STATUS_OK: return "OK";
        case ZCC_STATUS_ERR_NULL_PTR: return "ERR_NULL_PTR";
        case ZCC_STATUS_ERR_INVALID_OP: return "ERR_INVALID_OP";
        case ZCC_STATUS_ERR_INVALID_DTYPE: return "ERR_INVALID_DTYPE";
        case ZCC_STATUS_ERR_INVALID_SHAPE: return "ERR_INVALID_SHAPE";
        case ZCC_STATUS_ERR_INVALID_STRIDE: return "ERR_INVALID_STRIDE";
        case ZCC_STATUS_ERR_MISALIGNED: return "ERR_MISALIGNED";
        case ZCC_STATUS_ERR_OOB: return "ERR_OOB";
        case ZCC_STATUS_ERR_READONLY: return "ERR_READONLY";
        case ZCC_STATUS_ERR_ALIASING: return "ERR_ALIASING";
        case ZCC_STATUS_ERR_INVALID_ARG: return "ERR_INVALID_ARG";
        case ZCC_STATUS_ERR_UNSUPPORTED_HW: return "ERR_UNSUPPORTED_HW";
        case ZCC_STATUS_ERR_BACKEND_UNAVAILABLE: return "ERR_BACKEND_UNAVAILABLE";
        case ZCC_STATUS_ERR_BACKEND_RUNTIME: return "ERR_BACKEND_RUNTIME";
        case ZCC_STATUS_ERR_OOM: return "ERR_OOM";
        case ZCC_STATUS_ERR_OVERFLOW: return "ERR_OVERFLOW";
        case ZCC_STATUS_ERR_RECEIPT_TAMPER: return "ERR_RECEIPT_TAMPER";
        case ZCC_STATUS_ERR_ABI_MISMATCH: return "ERR_ABI_MISMATCH";
        default: return "ERR_UNKNOWN";
    }
}

const char *zcc_op_str(uint32_t op_id) {
    switch (op_id) {
        case ZCC_OP_VEC_ADD_F32: return "VEC_ADD_F32";
        case ZCC_OP_VEC_MUL_F32: return "VEC_MUL_F32";
        case ZCC_OP_VEC_FMA_F32: return "VEC_FMA_F32";
        case ZCC_OP_DOT_F32: return "DOT_F32";
        case ZCC_OP_GEMM_F32: return "GEMM_F32";
        case ZCC_OP_RMSNORM_F32: return "RMSNORM_F32";
        case ZCC_OP_SOFTMAX_F32: return "SOFTMAX_F32";
        case ZCC_OP_DEQUANT_Q8_0: return "DEQUANT_Q8_0";
        case ZCC_OP_MATVEC_Q8_0: return "MATVEC_Q8_0";
        case ZCC_OP_DEQUANT_Q4_0: return "DEQUANT_Q4_0";
        case ZCC_OP_MATVEC_Q4_0: return "MATVEC_Q4_0";
        case ZCC_OP_DEQUANT_Q4_K: return "DEQUANT_Q4_K";
        case ZCC_OP_MATVEC_Q4_K: return "MATVEC_Q4_K";
        default: return "UNKNOWN_OP";
    }
}

const char *zcc_dtype_str(uint32_t dtype) {
    switch (dtype) {
        case ZCC_DTYPE_F32: return "f32";
        case ZCC_DTYPE_F16: return "f16";
        case ZCC_DTYPE_BF16: return "bf16";
        case ZCC_DTYPE_Q8_0: return "q8_0";
        case ZCC_DTYPE_Q4_0: return "q4_0";
        case ZCC_DTYPE_Q4_K: return "q4_k";
        case ZCC_DTYPE_U8: return "u8";
        case ZCC_DTYPE_I32: return "i32";
        default: return "unknown";
    }
}

const char *zcc_backend_str(uint32_t backend_id) {
    switch (backend_id) {
        case ZCC_BACKEND_AUTO: return "AUTO";
        case ZCC_BACKEND_SCALAR_C99: return "SCALAR_C99";
        case ZCC_BACKEND_AVX2_FMA: return "AVX2_FMA";
        case ZCC_BACKEND_PTX_SM120: return "PTX_SM120";
        default: return "UNKNOWN_BACKEND";
    }
}

zcc_tolerance_policy_t zcc_default_tolerance(uint32_t op_id) {
    zcc_tolerance_policy_t p = { 1e-5, 1e-4, 0U, 0U };
    switch (op_id) {
        case ZCC_OP_VEC_ADD_F32:
        case ZCC_OP_VEC_MUL_F32:
            p.atol = 1e-7; p.rtol = 1e-6; p.max_ulp = 2U; break;
        case ZCC_OP_VEC_FMA_F32:
            p.atol = 1e-6; p.rtol = 1e-6; p.max_ulp = 4U; break;
        case ZCC_OP_DOT_F32:
        case ZCC_OP_GEMM_F32:
            p.atol = 1e-4; p.rtol = 1e-4; p.max_ulp = 0U; break;
        case ZCC_OP_RMSNORM_F32:
        case ZCC_OP_SOFTMAX_F32:
            p.atol = 1e-5; p.rtol = 1e-4; p.max_ulp = 0U; break;
        case ZCC_OP_DEQUANT_Q8_0:
        case ZCC_OP_DEQUANT_Q4_0:
        case ZCC_OP_DEQUANT_Q4_K:
            p.atol = 1e-6; p.rtol = 1e-5; p.max_ulp = 4U; break;
        case ZCC_OP_MATVEC_Q8_0:
        case ZCC_OP_MATVEC_Q4_0:
        case ZCC_OP_MATVEC_Q4_K:
            p.atol = 5e-4; p.rtol = 5e-4; p.max_ulp = 0U; break;
        default: break;
    }
    return p;
}

static int u64_mul_overflow(uint64_t a, uint64_t b, uint64_t *res) {
    if (a == 0ULL || b == 0ULL) { *res = 0ULL; return 0; }
    if (a > UINT64_MAX / b) return 1;
    *res = a * b;
    return 0;
}

static int u64_add_overflow(uint64_t a, uint64_t b, uint64_t *res) {
    if (a > UINT64_MAX - b) return 1;
    *res = a + b;
    return 0;
}

static int is_quant_dtype(uint32_t dtype) {
    return dtype == ZCC_DTYPE_Q8_0 || dtype == ZCC_DTYPE_Q4_0 || dtype == ZCC_DTYPE_Q4_K;
}

static uint64_t dense_elem_bytes(uint32_t dtype) {
    switch (dtype) {
        case ZCC_DTYPE_F32:
        case ZCC_DTYPE_I32: return 4ULL;
        case ZCC_DTYPE_F16:
        case ZCC_DTYPE_BF16: return 2ULL;
        case ZCC_DTYPE_U8: return 1ULL;
        default: return 0ULL;
    }
}

static zcc_status_t validate_tensor(const zcc_tensor_desc_t *t, int is_output, uint64_t *out_span_bytes, char *err_buf, size_t err_cap) {
    if (!t) {
        set_err(err_buf, err_cap, "NULL tensor descriptor");
        return ZCC_STATUS_ERR_NULL_PTR;
    }
    if (t->struct_size != sizeof(zcc_tensor_desc_t)) {
        set_err(err_buf, err_cap, "Invalid zcc_tensor_desc_t struct_size");
        return ZCC_STATUS_ERR_ABI_MISMATCH;
    }
    if (!t->data) {
        set_err(err_buf, err_cap, "NULL tensor data buffer");
        return ZCC_STATUS_ERR_NULL_PTR;
    }
    if (is_output && (t->ownership_flags & ZCC_OWNER_READONLY)) {
        set_err(err_buf, err_cap, "Cannot write to ZCC_OWNER_READONLY output tensor");
        return ZCC_STATUS_ERR_READONLY;
    }
    if (t->rank < 1U || t->rank > ZCC_COMPUTE_MAX_RANK) {
        set_err(err_buf, err_cap, "Tensor rank must be in [1, 4]");
        return ZCC_STATUS_ERR_INVALID_SHAPE;
    }
    if (t->alignment != 0U && (t->alignment & (t->alignment - 1U)) != 0U) {
        set_err(err_buf, err_cap, "Tensor alignment must be a power of 2");
        return ZCC_STATUS_ERR_MISALIGNED;
    }
    if (t->alignment > 1U && (((uintptr_t)t->data) % (uintptr_t)t->alignment) != 0U) {
        set_err(err_buf, err_cap, "Tensor data pointer fails declared alignment");
        return ZCC_STATUS_ERR_MISALIGNED;
    }

    for (uint32_t d = 0U; d < t->rank; d++) {
        if (t->shape[d] <= 0) {
            set_err(err_buf, err_cap, "Tensor dimensions must be strictly positive");
            return ZCC_STATUS_ERR_INVALID_SHAPE;
        }
        if (t->strides[d] < 0) {
            set_err(err_buf, err_cap, "Negative tensor strides are invalid");
            return ZCC_STATUS_ERR_INVALID_STRIDE;
        }
    }

    if (is_quant_dtype(t->dtype)) {
        if (t->rank > 2U) {
            set_err(err_buf, err_cap, "Quantized tensors require rank 1 or 2");
            return ZCC_STATUS_ERR_INVALID_SHAPE;
        }
        for (uint32_t d = 0U; d < t->rank; d++) {
            if (t->strides[d] != 0) {
                set_err(err_buf, err_cap, "Quantized tensors must have canonical zero strides");
                return ZCC_STATUS_ERR_INVALID_STRIDE;
            }
        }
        uint64_t blk_elems = (t->dtype == ZCC_DTYPE_Q4_K) ? 256ULL : 32ULL;
        uint64_t blk_bytes = (t->dtype == ZCC_DTYPE_Q4_K) ? 144ULL
                           : (t->dtype == ZCC_DTYPE_Q4_0) ? 18ULL : 34ULL;
        int64_t N = (t->rank == 1U) ? 1 : t->shape[0];
        int64_t K = (t->rank == 1U) ? t->shape[0] : t->shape[1];
        if (((uint64_t)K % blk_elems) != 0ULL) {
            set_err(err_buf, err_cap, "Quantized K dimension not divisible by block size");
            return ZCC_STATUS_ERR_INVALID_SHAPE;
        }
        uint64_t blocks = 0ULL, req = 0ULL;
        if (u64_mul_overflow((uint64_t)N, (uint64_t)K / blk_elems, &blocks) ||
            u64_mul_overflow(blocks, blk_bytes, &req)) {
            set_err(err_buf, err_cap, "Quantized buffer size overflow");
            return ZCC_STATUS_ERR_OVERFLOW;
        }
        if (t->nbytes < req) {
            set_err(err_buf, err_cap, "Quantized tensor nbytes smaller than required blocks");
            return ZCC_STATUS_ERR_OOB;
        }
        if (out_span_bytes) *out_span_bytes = req;
        return ZCC_STATUS_OK;
    }

    uint64_t esz = dense_elem_bytes(t->dtype);
    if (esz == 0ULL) {
        set_err(err_buf, err_cap, "Invalid tensor dtype");
        return ZCC_STATUS_ERR_INVALID_DTYPE;
    }

    uint64_t eff_strides[ZCC_COMPUTE_MAX_RANK];
    uint64_t exp_st = esz;
    for (int d = (int)t->rank - 1; d >= 0; d--) {
        uint64_t st = (t->strides[d] > 0) ? (uint64_t)t->strides[d] : exp_st;
        eff_strides[d] = st;
        if (u64_mul_overflow(st, (uint64_t)t->shape[d], &exp_st)) {
            set_err(err_buf, err_cap, "Tensor stride span overflow");
            return ZCC_STATUS_ERR_OVERFLOW;
        }
    }

    /* Check non-overlapping strides across arbitrary permutation order */
    uint32_t order[ZCC_COMPUTE_MAX_RANK];
    for (uint32_t i = 0U; i < t->rank; i++) order[i] = i;
    for (uint32_t i = 0U; i < t->rank; i++) {
        for (uint32_t j = i + 1U; j < t->rank; j++) {
            if (eff_strides[order[j]] < eff_strides[order[i]]) {
                uint32_t tmp = order[i]; order[i] = order[j]; order[j] = tmp;
            }
        }
    }

    uint64_t min_allowed = esz;
    uint64_t max_off = 0ULL;
    for (uint32_t k = 0U; k < t->rank; k++) {
        uint32_t d = order[k];
        uint64_t dim = (uint64_t)t->shape[d];
        uint64_t st  = eff_strides[d];
        if (st < min_allowed) {
            set_err(err_buf, err_cap, "Overlapping or sub-element tensor stride");
            return ZCC_STATUS_ERR_INVALID_STRIDE;
        }
        uint64_t term = 0ULL;
        if (u64_mul_overflow(dim - 1ULL, st, &term) || u64_add_overflow(max_off, term, &max_off)) {
            set_err(err_buf, err_cap, "Tensor byte offset overflow");
            return ZCC_STATUS_ERR_OVERFLOW;
        }
        if (u64_mul_overflow(st, dim, &min_allowed)) {
            set_err(err_buf, err_cap, "Tensor stride span overflow");
            return ZCC_STATUS_ERR_OVERFLOW;
        }
    }
    uint64_t req_bytes = 0ULL;
    if (u64_add_overflow(max_off, esz, &req_bytes)) {
        set_err(err_buf, err_cap, "Tensor byte span overflow");
        return ZCC_STATUS_ERR_OVERFLOW;
    }
    if (t->nbytes < req_bytes) {
        set_err(err_buf, err_cap, "Tensor nbytes smaller than accessed extent");
        return ZCC_STATUS_ERR_OOB;
    }
    if (out_span_bytes) *out_span_bytes = req_bytes;
    return ZCC_STATUS_OK;
}

static int shapes_equal(const zcc_tensor_desc_t *a, const zcc_tensor_desc_t *b) {
    if (a->rank != b->rank) return 0;
    for (uint32_t d = 0U; d < a->rank; d++) {
        if (a->shape[d] != b->shape[d]) return 0;
    }
    return 1;
}

static int ptr_ranges_overlap(const void *p1, uint64_t n1, const void *p2, uint64_t n2) {
    uintptr_t a0 = (uintptr_t)p1;
    uintptr_t b0 = (uintptr_t)p2;
    if (n1 == 0ULL || n2 == 0ULL) return 0;
    if (a0 < b0) return ((uint64_t)(b0 - a0) < n1) ? 1 : 0;
    if (b0 < a0) return ((uint64_t)(a0 - b0) < n2) ? 1 : 0;
    return 1;
}

zcc_status_t zcc_compute_validate(
    zcc_op_t op,
    const zcc_tensor_desc_t *inputs,
    uint32_t num_inputs,
    const zcc_tensor_desc_t *output,
    const zcc_exec_options_t *opts,
    char *err_buf,
    size_t err_cap
) {
    if (!opts || !inputs || !output) {
        set_err(err_buf, err_cap, "NULL argument passed to zcc_compute_validate");
        return ZCC_STATUS_ERR_NULL_PTR;
    }
    if (opts->abi_version != ZCC_COMPUTE_ABI_VERSION || opts->struct_size != sizeof(zcc_exec_options_t)) {
        set_err(err_buf, err_cap, "ABI version or exec_options struct_size mismatch");
        return ZCC_STATUS_ERR_ABI_MISMATCH;
    }
    if (opts->accum_policy != ZCC_ACCUM_FP32_FMA &&
        opts->accum_policy != ZCC_ACCUM_FP64_REF &&
        opts->accum_policy != ZCC_ACCUM_STRICT_FP32) {
        set_err(err_buf, err_cap, "Invalid accumulation policy");
        return ZCC_STATUS_ERR_INVALID_ARG;
    }
    if (opts->selection_policy != ZCC_POLICY_EXACT_REQUIRED &&
        opts->selection_policy != ZCC_POLICY_AUTO_FALLBACK) {
        set_err(err_buf, err_cap, "Invalid selection policy");
        return ZCC_STATUS_ERR_INVALID_ARG;
    }
    if (num_inputs == 0U || num_inputs > ZCC_COMPUTE_MAX_INPUTS) {
        set_err(err_buf, err_cap, "Invalid num_inputs");
        return ZCC_STATUS_ERR_INVALID_ARG;
    }

    uint32_t expected_inputs = 0U;
    switch (op) {
        case ZCC_OP_VEC_ADD_F32:
        case ZCC_OP_VEC_MUL_F32:
        case ZCC_OP_DOT_F32:
        case ZCC_OP_GEMM_F32:
        case ZCC_OP_RMSNORM_F32:
        case ZCC_OP_MATVEC_Q8_0:
        case ZCC_OP_MATVEC_Q4_0:
        case ZCC_OP_MATVEC_Q4_K: expected_inputs = 2U; break;
        case ZCC_OP_VEC_FMA_F32: expected_inputs = 3U; break;
        case ZCC_OP_SOFTMAX_F32:
        case ZCC_OP_DEQUANT_Q8_0:
        case ZCC_OP_DEQUANT_Q4_0:
        case ZCC_OP_DEQUANT_Q4_K: expected_inputs = 1U; break;
        default:
            set_err(err_buf, err_cap, "Unknown compute op_id");
            return ZCC_STATUS_ERR_INVALID_OP;
    }
    if (num_inputs != expected_inputs) {
        set_err(err_buf, err_cap, "Input operand count mismatch for operation");
        return ZCC_STATUS_ERR_INVALID_SHAPE;
    }

    uint64_t in_spans[ZCC_COMPUTE_MAX_INPUTS] = {0};
    for (uint32_t i = 0U; i < num_inputs; i++) {
        zcc_status_t st = validate_tensor(&inputs[i], 0, &in_spans[i], err_buf, err_cap);
        if (st != ZCC_STATUS_OK) return st;
    }
    uint64_t out_span = 0ULL;
    zcc_status_t st_out = validate_tensor(output, 1, &out_span, err_buf, err_cap);
    if (st_out != ZCC_STATUS_OK) return st_out;

    if (output->dtype != ZCC_DTYPE_F32) {
        set_err(err_buf, err_cap, "Track 21A outputs require f32 dtype");
        return ZCC_STATUS_ERR_INVALID_DTYPE;
    }

    switch (op) {
        case ZCC_OP_VEC_ADD_F32:
        case ZCC_OP_VEC_MUL_F32:
        case ZCC_OP_VEC_FMA_F32:
            for (uint32_t i = 0U; i < num_inputs; i++) {
                if (inputs[i].dtype != ZCC_DTYPE_F32) {
                    set_err(err_buf, err_cap, "Elementwise F32 inputs must be f32");
                    return ZCC_STATUS_ERR_INVALID_DTYPE;
                }
                if (!shapes_equal(&inputs[i], output)) {
                    set_err(err_buf, err_cap, "Elementwise shape mismatch");
                    return ZCC_STATUS_ERR_INVALID_SHAPE;
                }
            }
            break;

        case ZCC_OP_DOT_F32:
            if (inputs[0].dtype != ZCC_DTYPE_F32 || inputs[1].dtype != ZCC_DTYPE_F32) {
                set_err(err_buf, err_cap, "DOT_F32 inputs must be f32");
                return ZCC_STATUS_ERR_INVALID_DTYPE;
            }
            if (!shapes_equal(&inputs[0], &inputs[1]) || output->rank != 1U || output->shape[0] != 1) {
                set_err(err_buf, err_cap, "DOT_F32 requires equal input shapes and output [1]");
                return ZCC_STATUS_ERR_INVALID_SHAPE;
            }
            break;

        case ZCC_OP_GEMM_F32:
            if (inputs[0].dtype != ZCC_DTYPE_F32 || inputs[1].dtype != ZCC_DTYPE_F32) {
                set_err(err_buf, err_cap, "GEMM_F32 inputs must be f32");
                return ZCC_STATUS_ERR_INVALID_DTYPE;
            }
            if (inputs[0].rank != 2U || inputs[1].rank != 2U || output->rank != 2U ||
                inputs[0].shape[1] != inputs[1].shape[0] ||
                output->shape[0] != inputs[0].shape[0] || output->shape[1] != inputs[1].shape[1]) {
                set_err(err_buf, err_cap, "GEMM_F32 requires A[M,K], B[K,N], C[M,N]");
                return ZCC_STATUS_ERR_INVALID_SHAPE;
            }
            break;

        case ZCC_OP_RMSNORM_F32: {
            if (inputs[0].dtype != ZCC_DTYPE_F32 || inputs[1].dtype != ZCC_DTYPE_F32) {
                set_err(err_buf, err_cap, "RMSNORM_F32 inputs must be f32");
                return ZCC_STATUS_ERR_INVALID_DTYPE;
            }
            if (inputs[0].rank > 2U || inputs[1].rank != 1U || !shapes_equal(&inputs[0], output)) {
                set_err(err_buf, err_cap, "RMSNORM_F32 requires X[R,D] or [D], W[D], Y==X");
                return ZCC_STATUS_ERR_INVALID_SHAPE;
            }
            int64_t dim = (inputs[0].rank == 1U) ? inputs[0].shape[0] : inputs[0].shape[1];
            if (inputs[1].shape[0] != dim) {
                set_err(err_buf, err_cap, "RMSNORM weight length != hidden dim");
                return ZCC_STATUS_ERR_INVALID_SHAPE;
            }
            if (opts->rmsnorm_eps <= 0.0f || isnan(opts->rmsnorm_eps) || isinf(opts->rmsnorm_eps)) {
                set_err(err_buf, err_cap, "RMSNORM eps must be finite and strictly positive");
                return ZCC_STATUS_ERR_INVALID_ARG;
            }
            break;
        }

        case ZCC_OP_SOFTMAX_F32:
            if (inputs[0].dtype != ZCC_DTYPE_F32) {
                set_err(err_buf, err_cap, "SOFTMAX_F32 input must be f32");
                return ZCC_STATUS_ERR_INVALID_DTYPE;
            }
            if (inputs[0].rank > 2U || !shapes_equal(&inputs[0], output)) {
                set_err(err_buf, err_cap, "SOFTMAX_F32 requires rank 1 or 2 and matching output shape");
                return ZCC_STATUS_ERR_INVALID_SHAPE;
            }
            break;

        case ZCC_OP_DEQUANT_Q8_0:
        case ZCC_OP_DEQUANT_Q4_0:
        case ZCC_OP_DEQUANT_Q4_K: {
            uint32_t exp_dt = (op == ZCC_OP_DEQUANT_Q8_0) ? ZCC_DTYPE_Q8_0
                            : (op == ZCC_OP_DEQUANT_Q4_0) ? ZCC_DTYPE_Q4_0 : ZCC_DTYPE_Q4_K;
            if (inputs[0].dtype != exp_dt) {
                set_err(err_buf, err_cap, "Quantized dequant input dtype mismatch");
                return ZCC_STATUS_ERR_INVALID_DTYPE;
            }
            if (!shapes_equal(&inputs[0], output)) {
                set_err(err_buf, err_cap, "Dequant output shape must match input shape");
                return ZCC_STATUS_ERR_INVALID_SHAPE;
            }
            break;
        }

        case ZCC_OP_MATVEC_Q8_0:
        case ZCC_OP_MATVEC_Q4_0:
        case ZCC_OP_MATVEC_Q4_K: {
            uint32_t exp_dt = (op == ZCC_OP_MATVEC_Q8_0) ? ZCC_DTYPE_Q8_0
                            : (op == ZCC_OP_MATVEC_Q4_0) ? ZCC_DTYPE_Q4_0 : ZCC_DTYPE_Q4_K;
            if (inputs[0].dtype != exp_dt || inputs[1].dtype != ZCC_DTYPE_F32) {
                set_err(err_buf, err_cap, "Quantized matvec dtype mismatch");
                return ZCC_STATUS_ERR_INVALID_DTYPE;
            }
            if (inputs[0].rank != 2U || inputs[1].rank != 1U || output->rank != 1U ||
                inputs[0].shape[1] != inputs[1].shape[0] || inputs[0].shape[0] != output->shape[0]) {
                set_err(err_buf, err_cap, "Quantized matvec requires W[N,K], X[K], Y[N]");
                return ZCC_STATUS_ERR_INVALID_SHAPE;
            }
            break;
        }
        default: break;
    }

    int allow_identical_inplace = (op == ZCC_OP_VEC_ADD_F32 || op == ZCC_OP_VEC_MUL_F32 || op == ZCC_OP_VEC_FMA_F32);
    for (uint32_t i = 0U; i < num_inputs; i++) {
        if (ptr_ranges_overlap(inputs[i].data, in_spans[i], output->data, out_span)) {
            int exact_match = (inputs[i].data == output->data && in_spans[i] == out_span &&
                               memcmp(inputs[i].strides, output->strides, sizeof(output->strides)) == 0);
            if (!(allow_identical_inplace && exact_match)) {
                set_err(err_buf, err_cap, "Unsafe memory aliasing between input and output");
                return ZCC_STATUS_ERR_ALIASING;
            }
        }
    }

    return ZCC_STATUS_OK;
}

static uint32_t min_u32(uint32_t a, uint32_t b) { return (a < b) ? a : b; }

static void clamp_cpu_caps(zcc_cpu_caps_t *effective, const zcc_cpu_caps_t *live, const zcc_cpu_caps_t *bound) {
    *effective = *live;
    if (!bound) return;
    effective->has_cpuid    = min_u32(live->has_cpuid, bound->has_cpuid);
    effective->has_sse2     = min_u32(live->has_sse2, bound->has_sse2);
    effective->has_osxsave  = min_u32(live->has_osxsave, bound->has_osxsave);
    effective->xcr0_xmm     = min_u32(live->xcr0_xmm, bound->xcr0_xmm);
    effective->xcr0_ymm     = min_u32(live->xcr0_ymm, bound->xcr0_ymm);
    effective->has_avx      = min_u32(live->has_avx, bound->has_avx);
    effective->has_f16c     = min_u32(live->has_f16c, bound->has_f16c);
    effective->has_fma      = min_u32(live->has_fma, bound->has_fma);
    effective->has_avx2     = min_u32(live->has_avx2, bound->has_avx2);
    effective->has_avx512f  = min_u32(live->has_avx512f, bound->has_avx512f);
    effective->xcr0_raw    &= bound->xcr0_raw;
}

static void populate_receipt_dims(zcc_op_t op, const zcc_tensor_desc_t *inputs, const zcc_tensor_desc_t *output, zcc_compute_receipt_t *rec) {
    (void)output;
    if (op == ZCC_OP_GEMM_F32) {
        rec->dim_m = inputs[0].shape[0];
        rec->dim_k = inputs[0].shape[1];
        rec->dim_n = inputs[1].shape[1];
    } else if (inputs[0].rank == 2U) {
        rec->dim_m = inputs[0].shape[0];
        rec->dim_n = inputs[0].shape[1];
        rec->dim_k = inputs[0].shape[1];
    } else {
        rec->dim_m = 1;
        rec->dim_n = inputs[0].shape[0];
        rec->dim_k = inputs[0].shape[0];
    }
}

static void hash_io_pre(const zcc_tensor_desc_t *inputs, uint32_t num_inputs, const zcc_tensor_desc_t *output, zcc_compute_receipt_t *rec) {
    zcc_sha256_ctx_t ctx_d, ctx_b;
    uint8_t dig[32];
    zcc_sha256_init(&ctx_d);
    zcc_sha256_init(&ctx_b);
    for (uint32_t i = 0U; i < num_inputs; i++) {
        zcc_tensor_desc_t meta = inputs[i];
        meta.data = NULL;
        zcc_sha256_update(&ctx_d, &meta, sizeof(meta));
        if (inputs[i].data && inputs[i].nbytes > 0ULL) {
            zcc_sha256_update(&ctx_b, inputs[i].data, (size_t)inputs[i].nbytes);
        }
    }
    zcc_sha256_final(&ctx_d, dig);
    zcc_sha256_digest_to_hex(dig, rec->inputs_descriptor_sha256);
    zcc_sha256_final(&ctx_b, dig);
    zcc_sha256_digest_to_hex(dig, rec->input_buffers_sha256);

    zcc_tensor_desc_t ometa = *output;
    ometa.data = NULL;
    zcc_sha256_hex(&ometa, sizeof(ometa), rec->output_descriptor_sha256);
}

static zcc_status_t exec_internal(
    zcc_op_t op,
    const zcc_tensor_desc_t *inputs,
    uint32_t num_inputs,
    zcc_tensor_desc_t *output,
    const zcc_exec_options_t *opts,
    const zcc_cpu_caps_t *restricted_cpu_caps,
    zcc_compute_receipt_t *out_receipt
) {
    uint64_t t0 = monotonic_ns();
    char err_msg[192];
    memset(err_msg, 0, sizeof(err_msg));

    zcc_cpu_caps_t live_cpu, eff_cpu;
    zcc_ptx_caps_t ptx_caps;
    memset(&ptx_caps, 0, sizeof(ptx_caps));
    ptx_caps.struct_size = (uint32_t)sizeof(ptx_caps);
    zcc_probe_cpu_caps(&live_cpu);
    clamp_cpu_caps(&eff_cpu, &live_cpu, restricted_cpu_caps);
    if (opts && (opts->requested_backend == ZCC_BACKEND_AUTO || opts->requested_backend == ZCC_BACKEND_PTX_SM120)) {
        zcc_probe_ptx_caps(&ptx_caps, opts->ptx_path_override);
    }

    zcc_status_t st = zcc_compute_validate(op, inputs, num_inputs, output, opts, err_msg, sizeof(err_msg));
    uint32_t selected_backend = ZCC_BACKEND_SCALAR_C99;
    uint32_t fallback_occurred = 0U;
    char reason[128] = {0};

    if (st == ZCC_STATUS_OK) {
        st = zcc_negotiate_backend_with_caps(
            &eff_cpu, &ptx_caps,
            opts->requested_backend, opts->selection_policy,
            &selected_backend, &fallback_occurred,
            reason, sizeof(reason)
        );
        if (st == ZCC_STATUS_OK && selected_backend == ZCC_BACKEND_AVX2_FMA &&
            opts->accum_policy != ZCC_ACCUM_FP32_FMA) {
            if (opts->selection_policy == ZCC_POLICY_AUTO_FALLBACK) {
                selected_backend = ZCC_BACKEND_SCALAR_C99;
                fallback_occurred = 1U;
                snprintf(reason, sizeof(reason), "fallback_scalar_for_accum_policy");
            } else {
                st = ZCC_STATUS_ERR_INVALID_ARG;
                snprintf(err_msg, sizeof(err_msg), "AVX2_FMA requires ZCC_ACCUM_FP32_FMA under EXACT_REQUIRED");
            }
        }
        if (st == ZCC_STATUS_OK && selected_backend == ZCC_BACKEND_PTX_SM120 &&
            opts->accum_policy != ZCC_ACCUM_FP32_FMA) {
            if (opts->selection_policy == ZCC_POLICY_AUTO_FALLBACK) {
                selected_backend = ZCC_BACKEND_SCALAR_C99;
                fallback_occurred = 1U;
                snprintf(reason, sizeof(reason), "fallback_scalar_for_accum_policy");
            } else {
                st = ZCC_STATUS_ERR_INVALID_ARG;
                snprintf(err_msg, sizeof(err_msg), "PTX_SM120 requires ZCC_ACCUM_FP32_FMA under EXACT_REQUIRED");
            }
        }
    }

    zcc_compute_receipt_t local_rec;
    zcc_compute_receipt_t *rec = out_receipt ? out_receipt : &local_rec;
    memset(rec, 0, sizeof(*rec));
    rec->struct_size = (uint32_t)sizeof(*rec);
    rec->abi_version = ZCC_COMPUTE_ABI_VERSION;
    snprintf(rec->schema_version, sizeof(rec->schema_version), "%s", ZCC_COMPUTE_RECEIPT_SCHEMA);
    snprintf(rec->contract_version, sizeof(rec->contract_version), "%s", ZCC_COMPUTE_CONTRACT_VERSION);
    snprintf(rec->compiler_id, sizeof(rec->compiler_id), "%s", (opts && opts->compiler_id) ? opts->compiler_id : "gcc-c99-track21a");
    snprintf(rec->compiler_sha256_hex, sizeof(rec->compiler_sha256_hex), "%s", (opts && opts->compiler_sha256_hex) ? opts->compiler_sha256_hex : "unrecorded");
    snprintf(rec->executable_sha256_hex, sizeof(rec->executable_sha256_hex), "%s", (opts && opts->executable_sha256_hex) ? opts->executable_sha256_hex : "unrecorded");
    snprintf(rec->ptx_module_sha256_hex, sizeof(rec->ptx_module_sha256_hex), "none");
    rec->cpu_caps = eff_cpu;
    rec->ptx_caps = ptx_caps;
    rec->op_id = (uint32_t)op;

    if (inputs && num_inputs > 0U) {
        rec->primary_dtype = inputs[0].dtype;
        rec->input_count = num_inputs;
        populate_receipt_dims(op, inputs, output, rec);
    }
    if (opts) {
        rec->backend_requested = opts->requested_backend;
        rec->selection_policy = opts->selection_policy;
        rec->accum_policy = opts->accum_policy;
        rec->determinism_mode = opts->determinism_mode;
        rec->seed = opts->seed;
    }

    if (st == ZCC_STATUS_OK && inputs && output) {
        hash_io_pre(inputs, num_inputs, output, rec);
        /* Exercise custom allocator workspace hook when provided (enables OOM tests across all backends) */
        if (opts->alloc_fn) {
            void *guard_scratch = opts->alloc_fn(64U, opts->alloc_ctx);
            if (!guard_scratch) {
                st = ZCC_STATUS_ERR_OOM;
                snprintf(err_msg, sizeof(err_msg), "Workspace allocation failed (OOM)");
            } else if (opts->free_fn) {
                opts->free_fn(guard_scratch, opts->alloc_ctx);
            } else {
                free(guard_scratch);
            }
        }
    }

    if (st == ZCC_STATUS_OK) {
        rec->backend_executed = selected_backend;
        rec->fallback_occurred = fallback_occurred;
        snprintf(rec->selection_reason, sizeof(rec->selection_reason), "%s", reason);

        if (selected_backend == ZCC_BACKEND_SCALAR_C99) {
            st = zcc_scalar_exec(op, inputs, num_inputs, output, opts, err_msg, sizeof(err_msg));
        } else if (selected_backend == ZCC_BACKEND_AVX2_FMA) {
            st = zcc_avx2_exec(op, inputs, num_inputs, output, opts, err_msg, sizeof(err_msg));
        } else if (selected_backend == ZCC_BACKEND_PTX_SM120) {
            st = zcc_ptx_exec(op, inputs, num_inputs, output, opts, &ptx_caps, rec->ptx_module_sha256_hex, err_msg, sizeof(err_msg));
        } else {
            st = ZCC_STATUS_ERR_INVALID_ARG;
        }
    } else {
        rec->backend_executed = 0U;
        rec->fallback_occurred = fallback_occurred;
        snprintf(rec->selection_reason, sizeof(rec->selection_reason), "%s", reason[0] ? reason : "validation_or_negotiation_failed");
    }

    if (st == ZCC_STATUS_OK && output && output->data && output->nbytes > 0ULL) {
        zcc_sha256_hex(output->data, (size_t)output->nbytes, rec->output_buffer_sha256);
    } else {
        snprintf(rec->output_buffer_sha256, sizeof(rec->output_buffer_sha256), "0000000000000000000000000000000000000000000000000000000000000000");
    }

    rec->status_code = (int32_t)st;
    snprintf(rec->execution_status, sizeof(rec->execution_status), "%s", zcc_status_str(st));
    snprintf(rec->error_message, sizeof(rec->error_message), "%s", err_msg);
    snprintf(rec->verification_status, sizeof(rec->verification_status), "UNVERIFIED_RUNTIME_ONLY");
    rec->zk_proof_present = 0U;
    rec->elapsed_ns = monotonic_ns() - t0;

    if (!opts || opts->emit_receipt) {
        const uint8_t *hkey = opts ? opts->hmac_key : NULL;
        size_t hlen = opts ? opts->hmac_key_len : 0U;
        zcc_receipt_seal(rec, hkey, hlen);
    }

    return st;
}

zcc_status_t zcc_compute_exec(
    zcc_op_t op,
    const zcc_tensor_desc_t *inputs,
    uint32_t num_inputs,
    zcc_tensor_desc_t *output,
    const zcc_exec_options_t *opts,
    zcc_compute_receipt_t *out_receipt
) {
    return exec_internal(op, inputs, num_inputs, output, opts, NULL, out_receipt);
}

zcc_status_t zcc_compute_exec_with_caps(
    zcc_op_t op,
    const zcc_tensor_desc_t *inputs,
    uint32_t num_inputs,
    zcc_tensor_desc_t *output,
    const zcc_exec_options_t *opts,
    const zcc_cpu_caps_t *restricted_cpu_caps,
    zcc_compute_receipt_t *out_receipt
) {
    return exec_internal(op, inputs, num_inputs, output, opts, restricted_cpu_caps, out_receipt);
}
