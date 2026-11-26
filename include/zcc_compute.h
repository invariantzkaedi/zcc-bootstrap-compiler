/* SPDX-License-Identifier: MIT */
#ifndef ZCC_COMPUTE_H
#define ZCC_COMPUTE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZCC_COMPUTE_ABI_VERSION      0x00010000U
#define ZCC_COMPUTE_CONTRACT_VERSION "1.0.0"
#define ZCC_COMPUTE_RECEIPT_SCHEMA   "track21a.receipt.v1"
#define ZCC_COMPUTE_MAX_RANK         4U
#define ZCC_COMPUTE_MAX_INPUTS       4U

typedef enum zcc_dtype_e {
    ZCC_DTYPE_F32  = 1,
    ZCC_DTYPE_F16  = 2,
    ZCC_DTYPE_BF16 = 3,
    ZCC_DTYPE_Q8_0 = 4,
    ZCC_DTYPE_Q4_0 = 5,
    ZCC_DTYPE_Q4_K = 6,
    ZCC_DTYPE_U8   = 7,
    ZCC_DTYPE_I32  = 8
} zcc_dtype_t;

typedef enum zcc_ownership_e {
    ZCC_OWNER_BORROWED_HOST = 0x01U,
    ZCC_OWNER_OWNED_HOST    = 0x02U,
    ZCC_OWNER_READONLY      = 0x04U,
    ZCC_OWNER_DEVICE_PINNED = 0x08U
} zcc_ownership_t;

typedef enum zcc_op_e {
    ZCC_OP_VEC_ADD_F32   = 1,
    ZCC_OP_VEC_MUL_F32   = 2,
    ZCC_OP_VEC_FMA_F32   = 3,
    ZCC_OP_DOT_F32       = 4,
    ZCC_OP_GEMM_F32      = 5,
    ZCC_OP_RMSNORM_F32   = 6,
    ZCC_OP_SOFTMAX_F32   = 7,
    ZCC_OP_DEQUANT_Q8_0  = 8,
    ZCC_OP_MATVEC_Q8_0   = 9,
    ZCC_OP_DEQUANT_Q4_0  = 10,
    ZCC_OP_MATVEC_Q4_0   = 11,
    ZCC_OP_DEQUANT_Q4_K  = 12,
    ZCC_OP_MATVEC_Q4_K   = 13
} zcc_op_t;

typedef enum zcc_backend_id_e {
    ZCC_BACKEND_AUTO       = 0,
    ZCC_BACKEND_SCALAR_C99 = 1,
    ZCC_BACKEND_AVX2_FMA   = 2,
    ZCC_BACKEND_PTX_SM120  = 3
} zcc_backend_id_t;

typedef enum zcc_selection_policy_e {
    ZCC_POLICY_EXACT_REQUIRED = 1,
    ZCC_POLICY_AUTO_FALLBACK  = 2
} zcc_selection_policy_t;

typedef enum zcc_accum_policy_e {
    ZCC_ACCUM_FP32_FMA    = 1,
    ZCC_ACCUM_FP64_REF    = 2,
    ZCC_ACCUM_STRICT_FP32 = 3
} zcc_accum_policy_t;

typedef enum zcc_determinism_mode_e {
    ZCC_DET_FIXED_REDUCTION = 1,
    ZCC_DET_RELAXED         = 2
} zcc_determinism_mode_t;

typedef enum zcc_status_e {
    ZCC_STATUS_OK                      = 0,
    ZCC_STATUS_ERR_NULL_PTR            = -1,
    ZCC_STATUS_ERR_INVALID_OP          = -2,
    ZCC_STATUS_ERR_INVALID_DTYPE       = -3,
    ZCC_STATUS_ERR_INVALID_SHAPE       = -4,
    ZCC_STATUS_ERR_INVALID_STRIDE      = -5,
    ZCC_STATUS_ERR_MISALIGNED          = -6,
    ZCC_STATUS_ERR_OOB                 = -7,
    ZCC_STATUS_ERR_READONLY            = -8,
    ZCC_STATUS_ERR_ALIASING            = -9,
    ZCC_STATUS_ERR_INVALID_ARG         = -10,
    ZCC_STATUS_ERR_UNSUPPORTED_HW      = -11,
    ZCC_STATUS_ERR_BACKEND_UNAVAILABLE = -12,
    ZCC_STATUS_ERR_BACKEND_RUNTIME     = -13,
    ZCC_STATUS_ERR_OOM                 = -14,
    ZCC_STATUS_ERR_OVERFLOW            = -15,
    ZCC_STATUS_ERR_RECEIPT_TAMPER      = -16,
    ZCC_STATUS_ERR_ABI_MISMATCH        = -17
} zcc_status_t;

typedef struct zcc_tensor_desc_s {
    uint32_t struct_size;
    uint32_t dtype;
    uint32_t rank;
    uint32_t ownership_flags;
    int64_t  shape[ZCC_COMPUTE_MAX_RANK];
    int64_t  strides[ZCC_COMPUTE_MAX_RANK];
    void    *data;
    uint64_t nbytes;
    uint32_t alignment;
    uint32_t reserved;
} zcc_tensor_desc_t;

typedef struct zcc_cpu_caps_s {
    uint32_t struct_size;
    uint32_t max_basic_leaf;
    uint32_t has_cpuid;
    uint32_t has_sse2;
    uint32_t has_osxsave;
    uint32_t xcr0_xmm;
    uint32_t xcr0_ymm;
    uint32_t has_avx;
    uint32_t has_f16c;
    uint32_t has_fma;
    uint32_t has_avx2;
    uint32_t has_avx512f;
    uint64_t xcr0_raw;
} zcc_cpu_caps_t;

typedef struct zcc_ptx_caps_s {
    uint32_t struct_size;
    uint32_t cuda_available;
    int32_t  driver_version;
    int32_t  device_count;
    int32_t  cc_major;
    int32_t  cc_minor;
    uint64_t vram_bytes;
    char     device_name[128];
} zcc_ptx_caps_t;

typedef struct zcc_tolerance_policy_s {
    double atol;
    double rtol;
    uint32_t max_ulp;
    uint32_t reserved;
} zcc_tolerance_policy_t;

typedef struct zcc_exec_options_s {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t requested_backend;
    uint32_t selection_policy;
    uint32_t accum_policy;
    uint32_t determinism_mode;
    uint64_t seed;
    float    rmsnorm_eps;
    uint32_t emit_receipt;
    const char *compiler_id;
    const char *compiler_sha256_hex;
    const char *executable_sha256_hex;
    const char *ptx_path_override;
    const uint8_t *hmac_key;
    size_t   hmac_key_len;
    void *(*alloc_fn)(size_t bytes, void *ctx);
    void (*free_fn)(void *ptr, void *ctx);
    void    *alloc_ctx;
} zcc_exec_options_t;

typedef struct zcc_compute_receipt_s {
    uint32_t struct_size;
    uint32_t abi_version;
    char     schema_version[32];
    char     contract_version[16];
    char     compiler_id[64];
    char     compiler_sha256_hex[65];
    char     executable_sha256_hex[65];
    char     ptx_module_sha256_hex[65];
    uint32_t backend_requested;
    uint32_t backend_executed;
    uint32_t selection_policy;
    uint32_t fallback_occurred;
    char     selection_reason[128];
    zcc_cpu_caps_t cpu_caps;
    zcc_ptx_caps_t ptx_caps;
    uint32_t op_id;
    uint32_t primary_dtype;
    uint32_t accum_policy;
    uint32_t determinism_mode;
    uint64_t seed;
    uint32_t input_count;
    int64_t  dim_m;
    int64_t  dim_n;
    int64_t  dim_k;
    char     inputs_descriptor_sha256[65];
    char     input_buffers_sha256[65];
    char     output_descriptor_sha256[65];
    char     output_buffer_sha256[65];
    int32_t  status_code;
    char     execution_status[32];
    char     error_message[192];
    char     verification_status[32];
    char     integrity_kind[40];
    uint32_t zk_proof_present;
    uint64_t elapsed_ns;
    char     receipt_canonical_sha256[65];
    char     hmac_sha256_hex[65];
} zcc_compute_receipt_t;

/* Canonical SHA-256 & HMAC-SHA256 primitives (runtime/compute_sha256.c) */
typedef struct zcc_sha256_ctx_s {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t  buf[64];
    size_t   buf_len;
} zcc_sha256_ctx_t;

void zcc_sha256_init(zcc_sha256_ctx_t *ctx);
void zcc_sha256_update(zcc_sha256_ctx_t *ctx, const void *data, size_t len);
void zcc_sha256_final(zcc_sha256_ctx_t *ctx, uint8_t out_digest[32]);
void zcc_sha256_hex(const void *data, size_t len, char out_hex[65]);
void zcc_sha256_digest_to_hex(const uint8_t digest[32], char out_hex[65]);
void zcc_hmac_sha256_hex(const uint8_t *key, size_t key_len, const void *data, size_t data_len, char out_hex[65]);

/* CPU & Backend capability probing / negotiation (runtime/cpu_capabilities.c) */
zcc_status_t zcc_probe_cpu_caps(zcc_cpu_caps_t *out_caps);
int          zcc_cpu_has_avx2_fma(const zcc_cpu_caps_t *caps);
zcc_status_t zcc_negotiate_backend_with_caps(
    const zcc_cpu_caps_t *cpu_caps,
    const zcc_ptx_caps_t *ptx_caps,
    uint32_t requested_backend,
    uint32_t selection_policy,
    uint32_t *out_selected_backend,
    uint32_t *out_fallback_occurred,
    char *reason_buf,
    size_t reason_cap
);

/* Initialization & string helpers */
void         zcc_exec_options_init(zcc_exec_options_t *opts);
void         zcc_tensor_desc_init_1d(zcc_tensor_desc_t *t, zcc_dtype_t dtype, int64_t n, void *data, uint64_t nbytes, uint32_t align, uint32_t owner_flags);
void         zcc_tensor_desc_init_2d(zcc_tensor_desc_t *t, zcc_dtype_t dtype, int64_t rows, int64_t cols, int64_t row_stride_bytes, void *data, uint64_t nbytes, uint32_t align, uint32_t owner_flags);
const char  *zcc_status_str(zcc_status_t status);
const char  *zcc_op_str(uint32_t op_id);
const char  *zcc_dtype_str(uint32_t dtype);
const char  *zcc_backend_str(uint32_t backend_id);
zcc_tolerance_policy_t zcc_default_tolerance(uint32_t op_id);

/* Unified compute validation & execution (runtime/compute_dispatch.c) */
zcc_status_t zcc_compute_validate(
    zcc_op_t op,
    const zcc_tensor_desc_t *inputs,
    uint32_t num_inputs,
    const zcc_tensor_desc_t *output,
    const zcc_exec_options_t *opts,
    char *err_buf,
    size_t err_cap
);

zcc_status_t zcc_compute_exec(
    zcc_op_t op,
    const zcc_tensor_desc_t *inputs,
    uint32_t num_inputs,
    zcc_tensor_desc_t *output,
    const zcc_exec_options_t *opts,
    zcc_compute_receipt_t *out_receipt
);

/* Down-gated test-friendly execution using explicit restricted/mocked CPU caps.
 * Never elevates privileges: live hardware caps always act as upper bound. */
zcc_status_t zcc_compute_exec_with_caps(
    zcc_op_t op,
    const zcc_tensor_desc_t *inputs,
    uint32_t num_inputs,
    zcc_tensor_desc_t *output,
    const zcc_exec_options_t *opts,
    const zcc_cpu_caps_t *restricted_cpu_caps,
    zcc_compute_receipt_t *out_receipt
);

/* PTX capabilities & kernel teardown */
zcc_status_t zcc_probe_ptx_caps(zcc_ptx_caps_t *out_caps, const char *ptx_path_override);
void         zcc_ptx_shutdown(void);

/* Receipt serialization & canonical verification (runtime/execution_receipt.c) */
zcc_status_t zcc_receipt_seal(zcc_compute_receipt_t *receipt, const uint8_t *hmac_key, size_t hmac_key_len);
int          zcc_receipt_to_json(const zcc_compute_receipt_t *receipt, char *buf, size_t cap);
zcc_status_t zcc_receipt_verify_canonical(const zcc_compute_receipt_t *receipt, const uint8_t *hmac_key, size_t hmac_key_len);

/* Backend implementation symbols (internal runtime dispatch boundaries) */
zcc_status_t zcc_scalar_exec(
    zcc_op_t op,
    const zcc_tensor_desc_t *inputs,
    uint32_t num_inputs,
    zcc_tensor_desc_t *output,
    const zcc_exec_options_t *opts,
    char *err_buf,
    size_t err_cap
);

zcc_status_t zcc_avx2_exec(
    zcc_op_t op,
    const zcc_tensor_desc_t *inputs,
    uint32_t num_inputs,
    zcc_tensor_desc_t *output,
    const zcc_exec_options_t *opts,
    char *err_buf,
    size_t err_cap
);

zcc_status_t zcc_ptx_exec(
    zcc_op_t op,
    const zcc_tensor_desc_t *inputs,
    uint32_t num_inputs,
    zcc_tensor_desc_t *output,
    const zcc_exec_options_t *opts,
    const zcc_ptx_caps_t *caps,
    char *ptx_sha_out,
    char *err_buf,
    size_t err_cap
);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_COMPUTE_H */
