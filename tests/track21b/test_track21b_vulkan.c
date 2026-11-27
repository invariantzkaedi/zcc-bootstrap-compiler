/* SPDX-License-Identifier: MIT */
#define _POSIX_C_SOURCE 200809L
#include "zcc_vulkan_compute.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int g_pass = 0;
static int g_fail = 0;

#define T_CHECK(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; fprintf(stderr, "FAIL[line %d]: %s\n", __LINE__, (msg)); } \
} while (0)

static double now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec * 1e-3;
}

static uint16_t encode_f16_simple(float x) {
    if (x == 0.0f) return 0U;
    uint32_t u = 0U; memcpy(&u, &x, 4U);
    uint32_t sign = (u >> 16) & 0x8000U;
    int32_t exp = (int32_t)((u >> 23) & 0xFFU) - 127 + 15;
    uint32_t mant = (u >> 13) & 0x03FFU;
    if (exp <= 0) return (uint16_t)sign;
    if (exp >= 31) return (uint16_t)(sign | 0x7C00U);
    return (uint16_t)(sign | ((uint32_t)exp << 10) | mant);
}

static void store_le16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
}

static uint32_t g_oom_after = 0U;
static uint32_t g_oom_calls = 0U;
static void *mock_oom_alloc(size_t bytes, void *ctx) {
    (void)ctx;
    g_oom_calls++;
    if (g_oom_calls > g_oom_after) return NULL;
    return malloc(bytes);
}
static void mock_oom_free(void *ptr, void *ctx) {
    (void)ctx;
    free(ptr);
}

static void *cov_custom_alloc(size_t bytes, void *ctx) {
    (void)ctx;
    return malloc(bytes);
}

static int compare_arr_f32(const float *a, const float *b, size_t n, float atol, float rtol) {
    for (size_t i = 0; i < n; i++) {
        if (isnan(a[i]) && isnan(b[i])) continue;
        if (isinf(a[i]) && isinf(b[i]) && (signbit(a[i]) == signbit(b[i]))) continue;
        if (isnan(a[i]) || isnan(b[i])) return 0;
        float diff = fabsf(a[i] - b[i]);
        float tol = atol + rtol * fmaxf(fabsf(a[i]), fabsf(b[i]));
        if (diff > tol) {
            fprintf(stderr, "MISMATCH idx=%zu got=%.6f ref=%.6f diff=%.6g tol=%.6g\n", i, a[i], b[i], diff, tol);
            return 0;
        }
    }
    return 1;
}

static void test_device_caps_and_negotiation(void) {
    zcc_vulkan_caps_t vkc;
    zcc_status_t st = zcc_probe_vulkan_caps(&vkc, 1);
    printf("CAPS_VK avail=%u val_avail=%u val_en=%u type=%s(0x%x) name='%s' vendor=0x%04x api=%u.%u.%u push=%u ssbo=%llu\n",
           vkc.vulkan_available, vkc.validation_layer_available, vkc.validation_layer_enabled,
           vkc.device_type_str, vkc.physical_device_type, vkc.device_name, vkc.vendor_id,
           (vkc.api_version >> 22) & 0x7fU, (vkc.api_version >> 12) & 0x3ffU, vkc.api_version & 0xfffU,
           vkc.max_push_constants_size, (unsigned long long)vkc.max_storage_buffer_range);

    T_CHECK(st == ZCC_STATUS_OK && vkc.vulkan_available == 1U, "Vulkan loader + ICD available");
    T_CHECK(vkc.validation_layer_available == 1U && vkc.validation_layer_enabled == 1U, "VK_LAYER_KHRONOS_validation active");
    int is_hw_gpu = (vkc.physical_device_type == ZCC_VK_DEVTYPE_DISCRETE_GPU ||
                     vkc.physical_device_type == ZCC_VK_DEVTYPE_INTEGRATED_GPU);
    int devtype_ok = (strcmp(vkc.device_type_str, zcc_vulkan_device_type_str(vkc.physical_device_type)) == 0) &&
                     (vkc.physical_device_type == ZCC_VK_DEVTYPE_CPU || is_hw_gpu);
    T_CHECK(devtype_ok, "Vulkan physical deviceType matches reported device_type_str");

    float a[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float b[4] = {2.0f, 3.0f, 4.0f, 5.0f};
    float out[4] = {0};
    zcc_tensor_desc_t da, db, dout;
    zcc_tensor_desc_init_1d(&da, ZCC_DTYPE_F32, 4, a, sizeof(a), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&db, ZCC_DTYPE_F32, 4, b, sizeof(b), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dout, ZCC_DTYPE_F32, 4, out, sizeof(out), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_t ins[2] = {da, db};

    zcc_vulkan_exec_options_t opts;
    zcc_vulkan_exec_options_init(&opts);
    opts.base.requested_backend = ZCC_BACKEND_VULKAN_GPU;
    opts.base.selection_policy = ZCC_POLICY_EXACT_REQUIRED;
    zcc_compute_v2_receipt_t rec;
    st = zcc_compute_exec_v2(ZCC_OP_VEC_ADD_F32, ins, 2, &dout, &opts, &rec);
    if (is_hw_gpu) {
        T_CHECK(st == ZCC_STATUS_OK && rec.base.backend_executed == ZCC_BACKEND_VULKAN_GPU,
                "VULKAN_GPU executes directly on hardware GPU ICD");
        T_CHECK(rec.base.fallback_occurred == 0U && strcmp(rec.v2_verification_status, "UNVERIFIED_RUNTIME_ONLY") == 0,
                "Hardware GPU request records no fallback and UNVERIFIED_RUNTIME_ONLY");
        T_CHECK(fabsf(out[0] - 3.0f) < 1e-6f && fabsf(out[3] - 9.0f) < 1e-6f,
                "Hardware VULKAN_GPU produces exact VEC_ADD_F32 result");

        opts.base.selection_policy = ZCC_POLICY_AUTO_FALLBACK;
        st = zcc_compute_exec_v2(ZCC_OP_VEC_ADD_F32, ins, 2, &dout, &opts, &rec);
        T_CHECK(st == ZCC_STATUS_OK && rec.base.fallback_occurred == 0U && rec.base.backend_executed == ZCC_BACKEND_VULKAN_GPU,
                "AUTO_FALLBACK stays on VULKAN_GPU when hardware GPU is active");

        zcc_vulkan_caps_t mock_cpu = vkc;
        mock_cpu.physical_device_type = ZCC_VK_DEVTYPE_CPU;
        snprintf(mock_cpu.device_type_str, sizeof(mock_cpu.device_type_str), "CPU");
        opts.base.selection_policy = ZCC_POLICY_EXACT_REQUIRED;
        st = zcc_compute_exec_v2_with_caps(ZCC_OP_VEC_ADD_F32, ins, 2, &dout, &opts, NULL, &mock_cpu, &rec);
        T_CHECK(st == ZCC_STATUS_ERR_UNSUPPORTED_HW && rec.base.fallback_occurred == 0U,
                "CPU-clamped VULKAN_GPU rejected fail-closed under EXACT_REQUIRED");
        opts.base.selection_policy = ZCC_POLICY_AUTO_FALLBACK;
        st = zcc_compute_exec_v2_with_caps(ZCC_OP_VEC_ADD_F32, ins, 2, &dout, &opts, NULL, &mock_cpu, &rec);
        T_CHECK(st == ZCC_STATUS_OK && rec.base.fallback_occurred == 1U && rec.base.backend_executed == ZCC_BACKEND_VULKAN_SPV,
                "CPU-clamped AUTO_FALLBACK records explicit fallback to VULKAN_SPV");
    } else {
        /* CPU llvmpipe must reject ZCC_BACKEND_VULKAN_GPU under EXACT_REQUIRED */
        T_CHECK(st == ZCC_STATUS_ERR_UNSUPPORTED_HW, "VULKAN_GPU rejected fail-closed on CPU ICD");
        T_CHECK(rec.base.fallback_occurred == 0U && strcmp(rec.v2_verification_status, "UNVERIFIED_RUNTIME_ONLY") == 0,
                "Rejected GPU request records no silent fallback and UNVERIFIED_RUNTIME_ONLY");

        /* With AUTO_FALLBACK, VULKAN_GPU falls back cleanly to VULKAN_SPV and records fallback */
        opts.base.selection_policy = ZCC_POLICY_AUTO_FALLBACK;
        st = zcc_compute_exec_v2(ZCC_OP_VEC_ADD_F32, ins, 2, &dout, &opts, &rec);
        T_CHECK(st == ZCC_STATUS_OK && rec.base.fallback_occurred == 1U && rec.base.backend_executed == ZCC_BACKEND_VULKAN_SPV,
                "AUTO_FALLBACK records explicit fallback from VULKAN_GPU to VULKAN_SPV");
    }

    /* Restricted vk_caps controls */
    zcc_vulkan_caps_t mock_unavail = vkc;
    mock_unavail.vulkan_available = 0U;
    opts.base.requested_backend = ZCC_BACKEND_VULKAN_SPV;
    opts.base.selection_policy = ZCC_POLICY_EXACT_REQUIRED;
    st = zcc_compute_exec_v2_with_caps(ZCC_OP_VEC_ADD_F32, ins, 2, &dout, &opts, NULL, &mock_unavail, &rec);
    T_CHECK(st == ZCC_STATUS_ERR_BACKEND_UNAVAILABLE, "Mocked vulkan_available=0 fails closed");

    zcc_vulkan_caps_t mock_noq = vkc;
    mock_noq.has_compute_queue = 0U;
    st = zcc_compute_exec_v2_with_caps(ZCC_OP_VEC_ADD_F32, ins, 2, &dout, &opts, NULL, &mock_noq, &rec);
    T_CHECK(st == ZCC_STATUS_ERR_UNSUPPORTED_HW, "Mocked missing compute queue fails closed");

    zcc_vulkan_caps_t mock_small_push = vkc;
    mock_small_push.max_push_constants_size = 32U;
    st = zcc_compute_exec_v2_with_caps(ZCC_OP_VEC_ADD_F32, ins, 2, &dout, &opts, NULL, &mock_small_push, &rec);
    T_CHECK(st == ZCC_STATUS_ERR_UNSUPPORTED_HW, "Mocked push constant limit < 64 fails closed");

    zcc_vulkan_caps_t mock_small_ssbo = vkc;
    mock_small_ssbo.max_storage_buffer_range = 8ULL;
    st = zcc_compute_exec_v2_with_caps(ZCC_OP_VEC_ADD_F32, ins, 2, &dout, &opts, NULL, &mock_small_ssbo, &rec);
    T_CHECK(st == ZCC_STATUS_ERR_UNSUPPORTED_HW, "Mocked storage buffer limit exceeded fails closed");

    /* FP64_REF unsupported on Vulkan SPV */
    opts.base.accum_policy = ZCC_ACCUM_FP64_REF;
    st = zcc_compute_exec_v2(ZCC_OP_DOT_F32, ins, 2, &dout, &opts, &rec);
    /* note: output [1] needed for DOT; check VEC_ADD or DOT [1] */
    float dot_o[1] = {0};
    zcc_tensor_desc_t ddot;
    zcc_tensor_desc_init_1d(&ddot, ZCC_DTYPE_F32, 1, dot_o, sizeof(dot_o), 1, ZCC_OWNER_BORROWED_HOST);
    st = zcc_compute_exec_v2(ZCC_OP_DOT_F32, ins, 2, &ddot, &opts, &rec);
    T_CHECK(st == ZCC_STATUS_ERR_UNSUPPORTED_HW, "FP64_REF rejected under EXACT_REQUIRED on VULKAN_SPV");

    /* Corrupted SPIR-V override rejected cleanly without crash/std::bad_alloc/leak */
    zcc_vulkan_exec_options_init(&opts);
    opts.base.requested_backend = ZCC_BACKEND_VULKAN_SPV;
    opts.base.selection_policy = ZCC_POLICY_EXACT_REQUIRED;
    uint32_t bad_spv[8] = {0xDEADBEEFU, 0x00010000U, 0U, 10U, 0U, 0U, 0U, 0U};
    opts.spirv_override_words = bad_spv;
    opts.spirv_override_word_count = 8U;
    st = zcc_compute_exec_v2(ZCC_OP_VEC_ADD_F32, ins, 2, &dout, &opts, &rec);
    T_CHECK(st == ZCC_STATUS_ERR_BACKEND_RUNTIME, "Corrupted SPIR-V magic payload fails closed cleanly");

    uint32_t bad_spv_wc[8] = {ZCC_SPIRV_MAGIC_NUMBER, ZCC_SPIRV_VERSION_1_0, 0U, 10U, 0U, 0U, 0U, 0U};
    opts.spirv_override_words = bad_spv_wc;
    opts.spirv_override_word_count = 8U;
    st = zcc_compute_exec_v2(ZCC_OP_VEC_ADD_F32, ins, 2, &dout, &opts, &rec);
    T_CHECK(st == ZCC_STATUS_ERR_BACKEND_RUNTIME, "Zero-wordCount SPIR-V payload fails closed cleanly");
}

static zcc_status_t exec_maybe_ctx(
    zcc_vulkan_ctx_t *ctx,
    zcc_op_t op,
    const zcc_tensor_desc_t *inputs,
    uint32_t num_inputs,
    zcc_tensor_desc_t *output,
    const zcc_vulkan_exec_options_t *opt,
    zcc_compute_v2_receipt_t *rec
) {
    zcc_status_t st = ctx ? zcc_vulkan_ctx_exec(ctx, op, inputs, num_inputs, output, opt, rec)
                          : zcc_compute_exec_v2(op, inputs, num_inputs, output, opt, rec);
    if (st != ZCC_STATUS_OK && st != ZCC_STATUS_ERR_OOM && st != ZCC_STATUS_ERR_ABI_MISMATCH) {
        fprintf(stderr, "VK_UNEXPECTED_ERR op=%d ctx=%d st=%d val_errs=%u msg='%s'\n",
                (int)op, ctx ? 1 : 0, (int)st,
                rec ? rec->validation_error_count : 0U,
                rec ? rec->base.error_message : "");
    }
    return st;
}

static void test_all_13_ops_and_replay(int run_ptx, int prefer_ctx) {
    zcc_vulkan_ctx_t *shared_ctx = NULL;
    if (prefer_ctx) {
        T_CHECK(zcc_vulkan_ctx_create(1, 0U, &shared_ctx) == ZCC_STATUS_OK && shared_ctx != NULL,
                "shared ctx create validated PASS");
    }
    float vec_a[17], vec_b[17], vec_c[17];
    for (int i = 0; i < 17; i++) {
        vec_a[i] = (float)(i + 1) * 0.25f - 1.5f;
        vec_b[i] = (float)(17 - i) * 0.125f;
        vec_c[i] = (float)((i % 5) - 2) * 0.5f;
    }

    zcc_tensor_desc_t da, db, dc, dout_ref, dout_vk;
    float o_ref[256], o_vk[256], o_vk2[256];
    memset(o_ref, 0, sizeof(o_ref));
    memset(o_vk, 0, sizeof(o_vk));
    memset(o_vk2, 0, sizeof(o_vk2));

    zcc_vulkan_exec_options_t opt_sc, opt_vk;
    zcc_vulkan_exec_options_init(&opt_sc);
    opt_sc.base.requested_backend = ZCC_BACKEND_SCALAR_C99;
    opt_sc.base.selection_policy = ZCC_POLICY_EXACT_REQUIRED;
    zcc_vulkan_exec_options_init(&opt_vk);
    opt_vk.base.requested_backend = ZCC_BACKEND_VULKAN_SPV;
    opt_vk.base.selection_policy = ZCC_POLICY_EXACT_REQUIRED;
    opt_vk.require_validation_layer = 1U;

    zcc_compute_v2_receipt_t r_sc, r_vk, r_vk2;
    const uint8_t hkey[12] = "t21b-v2-key";
    opt_vk.base.hmac_key = hkey;
    opt_vk.base.hmac_key_len = sizeof(hkey);

    /* Op 1..3: Vector Add, Mul, FMA (17 odd size) */
    zcc_tensor_desc_init_1d(&da, ZCC_DTYPE_F32, 17, vec_a, sizeof(vec_a), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&db, ZCC_DTYPE_F32, 17, vec_b, sizeof(vec_b), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dc, ZCC_DTYPE_F32, 17, vec_c, sizeof(vec_c), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dout_ref, ZCC_DTYPE_F32, 17, o_ref, 17U * 4U, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dout_vk, ZCC_DTYPE_F32, 17, o_vk, 17U * 4U, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_t ins3[3] = {da, db, dc};
    for (zcc_op_t op = ZCC_OP_VEC_ADD_F32; op <= ZCC_OP_VEC_FMA_F32; op++) {
        uint32_t nin = (op == ZCC_OP_VEC_FMA_F32) ? 3U : 2U;
        T_CHECK(zcc_compute_exec_v2(op, ins3, nin, &dout_ref, &opt_sc, &r_sc) == ZCC_STATUS_OK, "scalar vec op");
        T_CHECK(exec_maybe_ctx(shared_ctx, op, ins3, nin, &dout_vk, &opt_vk, &r_vk) == ZCC_STATUS_OK, "vk vec op");
        T_CHECK(r_vk.sync_barrier_count == 2U && r_vk.validation_error_count == 0U, "vk sync & validation");
        T_CHECK(compare_arr_f32(o_vk, o_ref, 17U, 1e-6f, 1e-6f), "vk vec matches scalar");
        T_CHECK(zcc_receipt_v2_verify_canonical(&r_vk, hkey, sizeof(hkey)) == ZCC_STATUS_OK, "v2 hmac receipt verifies");
    }

    /* Op 4: DOT */
    zcc_tensor_desc_init_1d(&dout_ref, ZCC_DTYPE_F32, 1, o_ref, 4U, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dout_vk, ZCC_DTYPE_F32, 1, o_vk, 4U, 1, ZCC_OWNER_BORROWED_HOST);
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_DOT_F32, ins3, 2U, &dout_ref, &opt_sc, &r_sc) == ZCC_STATUS_OK, "scalar dot");
    T_CHECK(exec_maybe_ctx(shared_ctx, ZCC_OP_DOT_F32, ins3, 2U, &dout_vk, &opt_vk, &r_vk) == ZCC_STATUS_OK, "vk dot");
    T_CHECK(compare_arr_f32(o_vk, o_ref, 1U, 1e-4f, 1e-4f), "vk dot matches scalar");

    /* Op 5: GEMM [3, 5] x [5, 4] */
    float gm_a[15], gm_b[20];
    for (int i = 0; i < 15; i++) gm_a[i] = (float)(i % 7) - 3.0f;
    for (int i = 0; i < 20; i++) gm_b[i] = (float)(i % 5) * 0.5f - 1.0f;
    zcc_tensor_desc_init_2d(&da, ZCC_DTYPE_F32, 3, 5, 0, gm_a, sizeof(gm_a), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&db, ZCC_DTYPE_F32, 5, 4, 0, gm_b, sizeof(gm_b), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dout_ref, ZCC_DTYPE_F32, 3, 4, 0, o_ref, 12U * 4U, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dout_vk, ZCC_DTYPE_F32, 3, 4, 0, o_vk, 12U * 4U, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_t ins_g[2] = {da, db};
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_GEMM_F32, ins_g, 2U, &dout_ref, &opt_sc, &r_sc) == ZCC_STATUS_OK, "scalar gemm");
    T_CHECK(exec_maybe_ctx(shared_ctx, ZCC_OP_GEMM_F32, ins_g, 2U, &dout_vk, &opt_vk, &r_vk) == ZCC_STATUS_OK, "vk gemm");
    T_CHECK(compare_arr_f32(o_vk, o_ref, 12U, 1e-4f, 1e-4f), "vk gemm matches scalar");

    /* Op 6 & 7: RMSNORM + SOFTMAX [3, 7] */
    float rn_x[21], rn_w[7];
    for (int i = 0; i < 21; i++) rn_x[i] = ((float)(i - 10)) * 0.35f;
    for (int j = 0; j < 7; j++) rn_w[j] = 1.0f + (float)j * 0.1f;
    zcc_tensor_desc_init_2d(&da, ZCC_DTYPE_F32, 3, 7, 0, rn_x, sizeof(rn_x), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&db, ZCC_DTYPE_F32, 7, rn_w, sizeof(rn_w), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dout_ref, ZCC_DTYPE_F32, 3, 7, 0, o_ref, 21U * 4U, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dout_vk, ZCC_DTYPE_F32, 3, 7, 0, o_vk, 21U * 4U, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_t ins_r[2] = {da, db};
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_RMSNORM_F32, ins_r, 2U, &dout_ref, &opt_sc, &r_sc) == ZCC_STATUS_OK, "scalar rmsnorm");
    T_CHECK(exec_maybe_ctx(shared_ctx, ZCC_OP_RMSNORM_F32, ins_r, 2U, &dout_vk, &opt_vk, &r_vk) == ZCC_STATUS_OK, "vk rmsnorm");
    T_CHECK(compare_arr_f32(o_vk, o_ref, 21U, 1e-5f, 1e-4f), "vk rmsnorm matches scalar");

    T_CHECK(zcc_compute_exec_v2(ZCC_OP_SOFTMAX_F32, &da, 1U, &dout_ref, &opt_sc, &r_sc) == ZCC_STATUS_OK, "scalar softmax");
    T_CHECK(exec_maybe_ctx(shared_ctx, ZCC_OP_SOFTMAX_F32, &da, 1U, &dout_vk, &opt_vk, &r_vk) == ZCC_STATUS_OK, "vk softmax");
    T_CHECK(compare_arr_f32(o_vk, o_ref, 21U, 1e-5f, 1e-4f), "vk softmax matches scalar");

    /* Strided GEMM with canary gap preservation + NaN row propagation on SOFTMAX */
    float gm_sa[3 * 8], gm_sb[5 * 6], gm_so[3 * 9], gm_vo[3 * 9];
    for (int i = 0; i < 24; i++) gm_sa[i] = (float)(i % 5) - 2.0f;
    for (int i = 0; i < 30; i++) gm_sb[i] = (float)(i % 4) * 0.5f - 0.5f;
    for (int i = 0; i < 27; i++) { gm_so[i] = 999.25f; gm_vo[i] = 999.25f; }
    zcc_tensor_desc_init_2d(&da, ZCC_DTYPE_F32, 3, 5, 8 * 4, gm_sa, sizeof(gm_sa), 4, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&db, ZCC_DTYPE_F32, 5, 4, 6 * 4, gm_sb, sizeof(gm_sb), 4, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dout_ref, ZCC_DTYPE_F32, 3, 4, 9 * 4, gm_so, sizeof(gm_so), 4, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dout_vk, ZCC_DTYPE_F32, 3, 4, 9 * 4, gm_vo, sizeof(gm_vo), 4, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_t ins_sg[2] = {da, db};
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_GEMM_F32, ins_sg, 2U, &dout_ref, &opt_sc, &r_sc) == ZCC_STATUS_OK, "sc strided gemm");
    T_CHECK(exec_maybe_ctx(shared_ctx, ZCC_OP_GEMM_F32, ins_sg, 2U, &dout_vk, &opt_vk, &r_vk) == ZCC_STATUS_OK, "vk strided gemm");
    int sg_ok = 1;
    for (int rr = 0; rr < 3; rr++) {
        for (int cc = 0; cc < 9; cc++) {
            float gv = gm_vo[rr * 9 + cc], gs = gm_so[rr * 9 + cc];
            if (cc >= 4) { if (gv != 999.25f) sg_ok = 0; }
            else if (!compare_arr_f32(&gv, &gs, 1U, 1e-4f, 1e-4f)) sg_ok = 0;
        }
    }
    T_CHECK(sg_ok, "vk strided gemm preserves canary gaps");

    float rn_nan[21]; memcpy(rn_nan, rn_x, sizeof(rn_nan)); rn_nan[2] = NAN;
    zcc_tensor_desc_init_2d(&da, ZCC_DTYPE_F32, 3, 7, 0, rn_nan, sizeof(rn_nan), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dout_ref, ZCC_DTYPE_F32, 3, 7, 0, o_ref, 21U * 4U, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dout_vk, ZCC_DTYPE_F32, 3, 7, 0, o_vk, 21U * 4U, 1, ZCC_OWNER_BORROWED_HOST);
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_SOFTMAX_F32, &da, 1U, &dout_ref, &opt_sc, &r_sc) == ZCC_STATUS_OK, "sc softmax nan");
    T_CHECK(exec_maybe_ctx(shared_ctx, ZCC_OP_SOFTMAX_F32, &da, 1U, &dout_vk, &opt_vk, &r_vk) == ZCC_STATUS_OK, "vk softmax nan");
    int nan_ok = 1;
    for (int j = 0; j < 7; j++) if (!isnan(o_vk[j])) nan_ok = 0;
    for (int j = 7; j < 21; j++) if (!compare_arr_f32(&o_vk[j], &o_ref[j], 1U, 1e-5f, 1e-4f)) nan_ok = 0;
    T_CHECK(nan_ok, "vk softmax propagates NaN without swallowing");

    /* Op 8..13: Q8_0 (34B), Q4_0 (18B), Q4_K (144B) + Unaligned +1B host pointer test */
    uint8_t q8_buf[128], q4_buf[128], q4k_buf[300];
    memset(q8_buf, 0, sizeof(q8_buf));
    memset(q4_buf, 0, sizeof(q4_buf));
    memset(q4k_buf, 0, sizeof(q4k_buf));
    uint8_t *q8_un = q8_buf + 1; /* deliberate +1 byte misalignment */
    store_le16(q8_un, encode_f16_simple(0.25f));
    for (int i = 0; i < 32; i++) q8_un[2 + i] = (uint8_t)(int8_t)((i % 15) - 7);

    store_le16(q4_buf, encode_f16_simple(0.5f));
    for (int i = 0; i < 16; i++) q4_buf[2 + i] = (uint8_t)(((i & 0x0F) << 4) | ((15 - i) & 0x0F));

    uint8_t *q4k_un = q4k_buf + 1; /* +1 byte unaligned Q4_K */
    store_le16(q4k_un, encode_f16_simple(0.25f));
    store_le16(q4k_un + 2, encode_f16_simple(0.125f));
    for (int i = 0; i < 12; i++) q4k_un[4 + i] = (uint8_t)(0x12U + (uint8_t)i);
    for (int i = 0; i < 128; i++) q4k_un[16 + i] = (uint8_t)((i * 37 + 11) & 0xFFU);

    float xv[256];
    for (int i = 0; i < 256; i++) xv[i] = ((float)(i % 11) - 5.0f) * 0.2f;

    /* Q8_0 [1, 32] */
    zcc_tensor_desc_init_2d(&da, ZCC_DTYPE_Q8_0, 1, 32, 0, q8_un, 34ULL, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dout_ref, ZCC_DTYPE_F32, 1, 32, 0, o_ref, 32U * 4U, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dout_vk, ZCC_DTYPE_F32, 1, 32, 0, o_vk, 32U * 4U, 1, ZCC_OWNER_BORROWED_HOST);
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_DEQUANT_Q8_0, &da, 1U, &dout_ref, &opt_sc, &r_sc) == ZCC_STATUS_OK, "sc q8_0");
    T_CHECK(exec_maybe_ctx(shared_ctx, ZCC_OP_DEQUANT_Q8_0, &da, 1U, &dout_vk, &opt_vk, &r_vk) == ZCC_STATUS_OK, "vk q8_0 unaligned+1");
    T_CHECK(compare_arr_f32(o_vk, o_ref, 32U, 1e-6f, 1e-5f), "vk q8_0 unaligned+1 matches scalar");

    zcc_tensor_desc_init_1d(&db, ZCC_DTYPE_F32, 32, xv, 32U * 4U, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dout_ref, ZCC_DTYPE_F32, 1, o_ref, 4U, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dout_vk, ZCC_DTYPE_F32, 1, o_vk, 4U, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_t ins_q[2] = {da, db};
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_MATVEC_Q8_0, ins_q, 2U, &dout_ref, &opt_sc, &r_sc) == ZCC_STATUS_OK, "sc mv q8_0");
    T_CHECK(exec_maybe_ctx(shared_ctx, ZCC_OP_MATVEC_Q8_0, ins_q, 2U, &dout_vk, &opt_vk, &r_vk) == ZCC_STATUS_OK, "vk mv q8_0");
    T_CHECK(compare_arr_f32(o_vk, o_ref, 1U, 5e-4f, 5e-4f), "vk mv q8_0 matches scalar");

    /* Q4_0 [1, 32] */
    zcc_tensor_desc_init_2d(&da, ZCC_DTYPE_Q4_0, 1, 32, 0, q4_buf, 18ULL, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dout_ref, ZCC_DTYPE_F32, 1, 32, 0, o_ref, 32U * 4U, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dout_vk, ZCC_DTYPE_F32, 1, 32, 0, o_vk, 32U * 4U, 1, ZCC_OWNER_BORROWED_HOST);
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_DEQUANT_Q4_0, &da, 1U, &dout_ref, &opt_sc, &r_sc) == ZCC_STATUS_OK, "sc q4_0");
    T_CHECK(exec_maybe_ctx(shared_ctx, ZCC_OP_DEQUANT_Q4_0, &da, 1U, &dout_vk, &opt_vk, &r_vk) == ZCC_STATUS_OK, "vk q4_0");
    T_CHECK(compare_arr_f32(o_vk, o_ref, 32U, 1e-6f, 1e-5f), "vk q4_0 matches scalar");

    ins_q[0] = da;
    zcc_tensor_desc_init_1d(&dout_ref, ZCC_DTYPE_F32, 1, o_ref, 4U, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dout_vk, ZCC_DTYPE_F32, 1, o_vk, 4U, 1, ZCC_OWNER_BORROWED_HOST);
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_MATVEC_Q4_0, ins_q, 2U, &dout_ref, &opt_sc, &r_sc) == ZCC_STATUS_OK, "sc mv q4_0");
    T_CHECK(exec_maybe_ctx(shared_ctx, ZCC_OP_MATVEC_Q4_0, ins_q, 2U, &dout_vk, &opt_vk, &r_vk) == ZCC_STATUS_OK, "vk mv q4_0");
    T_CHECK(compare_arr_f32(o_vk, o_ref, 1U, 5e-4f, 5e-4f), "vk mv q4_0 matches scalar");

    /* Q4_K [1, 256] + Deterministic Replay */
    zcc_tensor_desc_init_2d(&da, ZCC_DTYPE_Q4_K, 1, 256, 0, q4k_un, 144ULL, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dout_ref, ZCC_DTYPE_F32, 1, 256, 0, o_ref, 256U * 4U, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dout_vk, ZCC_DTYPE_F32, 1, 256, 0, o_vk, 256U * 4U, 1, ZCC_OWNER_BORROWED_HOST);
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_DEQUANT_Q4_K, &da, 1U, &dout_ref, &opt_sc, &r_sc) == ZCC_STATUS_OK, "sc q4_k");
    T_CHECK(exec_maybe_ctx(shared_ctx, ZCC_OP_DEQUANT_Q4_K, &da, 1U, &dout_vk, &opt_vk, &r_vk) == ZCC_STATUS_OK, "vk q4_k unaligned+1");
    T_CHECK(compare_arr_f32(o_vk, o_ref, 256U, 1e-6f, 1e-5f), "vk q4_k matches scalar");

    zcc_tensor_desc_init_1d(&db, ZCC_DTYPE_F32, 256, xv, sizeof(xv), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dout_ref, ZCC_DTYPE_F32, 1, o_ref, 4U, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dout_vk, ZCC_DTYPE_F32, 1, o_vk, 4U, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_t dout_vk2;
    zcc_tensor_desc_init_1d(&dout_vk2, ZCC_DTYPE_F32, 1, o_vk2, 4U, 1, ZCC_OWNER_BORROWED_HOST);
    ins_q[0] = da; ins_q[1] = db;
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_MATVEC_Q4_K, ins_q, 2U, &dout_ref, &opt_sc, &r_sc) == ZCC_STATUS_OK, "sc mv q4_k");
    T_CHECK(exec_maybe_ctx(shared_ctx, ZCC_OP_MATVEC_Q4_K, ins_q, 2U, &dout_vk, &opt_vk, &r_vk) == ZCC_STATUS_OK, "vk mv q4_k #1");
    T_CHECK(exec_maybe_ctx(shared_ctx, ZCC_OP_MATVEC_Q4_K, ins_q, 2U, &dout_vk2, &opt_vk, &r_vk2) == ZCC_STATUS_OK, "vk mv q4_k #2");
    T_CHECK(compare_arr_f32(o_vk, o_ref, 1U, 5e-4f, 5e-4f), "vk mv q4_k matches scalar");
    T_CHECK(strcmp(r_vk.base.output_buffer_sha256, r_vk2.base.output_buffer_sha256) == 0 &&
            strcmp(r_vk.spirv_module_sha256_hex, r_vk2.spirv_module_sha256_hex) == 0,
            "vk mv q4_k bitwise deterministic replay");

    /* Tamper negative checks on v2 receipt */
    T_CHECK(zcc_receipt_v2_verify_canonical(&r_vk, hkey, sizeof(hkey)) == ZCC_STATUS_OK, "valid v2 receipt passes");
    zcc_compute_v2_receipt_t tamp = r_vk;
    snprintf(tamp.v2_verification_status, sizeof(tamp.v2_verification_status), "PASS");
    T_CHECK(zcc_receipt_v2_verify_canonical(&tamp, hkey, sizeof(hkey)) == ZCC_STATUS_ERR_RECEIPT_TAMPER, "self-certified v2 rejected");
    tamp = r_vk; tamp.spirv_module_sha256_hex[0] ^= 1;
    T_CHECK(zcc_receipt_v2_verify_canonical(&tamp, hkey, sizeof(hkey)) == ZCC_STATUS_ERR_RECEIPT_TAMPER, "tampered spv hash rejected");
    tamp = r_vk;
    snprintf(tamp.vulkan_caps.device_type_str, sizeof(tamp.vulkan_caps.device_type_str),
             (strcmp(r_vk.vulkan_caps.device_type_str, "DISCRETE_GPU") == 0) ? "CPU" : "DISCRETE_GPU");
    T_CHECK(zcc_receipt_v2_verify_canonical(&tamp, hkey, sizeof(hkey)) == ZCC_STATUS_ERR_RECEIPT_TAMPER, "spoofed device_type_str rejected");
    tamp = r_vk; tamp.sync_barrier_count = 0U;
    T_CHECK(zcc_receipt_v2_verify_canonical(&tamp, hkey, sizeof(hkey)) == ZCC_STATUS_ERR_RECEIPT_TAMPER, "tampered barrier count rejected");

    /* OOM hook & ABI mismatch checks */
    g_oom_after = 0U; g_oom_calls = 0U;
    opt_vk.base.alloc_fn = mock_oom_alloc;
    opt_vk.base.free_fn = mock_oom_free;
    T_CHECK(exec_maybe_ctx(shared_ctx, ZCC_OP_MATVEC_Q4_K, ins_q, 2U, &dout_vk, &opt_vk, &r_vk) == ZCC_STATUS_ERR_OOM, "OOM hook clean");

    zcc_vulkan_exec_options_t bad_abi = opt_vk;
    bad_abi.abi_v2_version = 0x9999U;
    T_CHECK(exec_maybe_ctx(shared_ctx, ZCC_OP_MATVEC_Q4_K, ins_q, 2U, &dout_vk, &bad_abi, &r_vk) == ZCC_STATUS_ERR_ABI_MISMATCH, "ABI mismatch rejected");

    /* Optional cross-check PTX vs Vulkan on same op when non-ASan and CUDA GPU present */
    if (run_ptx) {
        zcc_ptx_caps_t ptxc;
        memset(&ptxc, 0, sizeof(ptxc));
        zcc_probe_ptx_caps(&ptxc, NULL);
        if (ptxc.cuda_available == 1U) {
            zcc_vulkan_exec_options_t opt_ptx;
            zcc_vulkan_exec_options_init(&opt_ptx);
            opt_ptx.base.requested_backend = ZCC_BACKEND_PTX_SM120;
            opt_ptx.base.selection_policy = ZCC_POLICY_EXACT_REQUIRED;
            float o_ptx[4] = {0};
            zcc_tensor_desc_t dout_ptx;
            zcc_tensor_desc_init_1d(&dout_ptx, ZCC_DTYPE_F32, 1, o_ptx, sizeof(o_ptx), 1, ZCC_OWNER_BORROWED_HOST);
            zcc_status_t st_ptx = zcc_compute_exec_v2(ZCC_OP_MATVEC_Q4_K, ins_q, 2U, &dout_ptx, &opt_ptx, &r_sc);
            T_CHECK(st_ptx == ZCC_STATUS_OK && compare_arr_f32(o_ptx, o_vk, 1U, 5e-4f, 5e-4f), "PTX matches Vulkan Q4_K matvec");
        } else {
            printf("PTX_CROSS_CHECK_SKIP cuda_available=0\n");
        }
    }
    if (shared_ctx) zcc_vulkan_ctx_destroy(shared_ctx);
}

static void test_persistent_vulkan_context(void) {
    zcc_vulkan_caps_t live_vk;
    T_CHECK(zcc_probe_vulkan_caps(&live_vk, 1) == ZCC_STATUS_OK && live_vk.vulkan_available == 1U, "probe live vk before ctx");
    int is_hw_gpu = (live_vk.physical_device_type == ZCC_VK_DEVTYPE_DISCRETE_GPU ||
                     live_vk.physical_device_type == ZCC_VK_DEVTYPE_INTEGRATED_GPU);

    zcc_vulkan_ctx_t *bad_ctx = NULL;
    T_CHECK(zcc_vulkan_ctx_create(1, 0U, NULL) == ZCC_STATUS_ERR_NULL_PTR, "ctx null out rejected");
    if (is_hw_gpu) {
        zcc_vulkan_ctx_t *hw_ctx = NULL;
        zcc_vulkan_caps_t hcaps;
        T_CHECK(zcc_vulkan_ctx_create(1, 1U, &hw_ctx) == ZCC_STATUS_OK && hw_ctx != NULL,
                "ctx require_hardware_gpu succeeds on HW GPU ICD");
        T_CHECK(zcc_vulkan_ctx_get_caps(hw_ctx, &hcaps) == ZCC_STATUS_OK &&
                hcaps.physical_device_type == live_vk.physical_device_type &&
                hcaps.validation_layer_enabled == 1U,
                "hw ctx caps match live HW GPU + validation");
        zcc_vulkan_ctx_destroy(hw_ctx);
    } else {
        T_CHECK(zcc_vulkan_ctx_create(1, 1U, &bad_ctx) == ZCC_STATUS_ERR_UNSUPPORTED_HW && bad_ctx == NULL,
                "ctx require_hardware_gpu rejected on CPU ICD");
    }

    zcc_vulkan_caps_t ccaps;
    T_CHECK(zcc_vulkan_ctx_get_caps(NULL, &ccaps) == ZCC_STATUS_ERR_INVALID_ARG, "ctx get_caps NULL rejected");

    zcc_vulkan_ctx_t *ctx = NULL;
    T_CHECK(zcc_vulkan_ctx_create(1, 0U, &ctx) == ZCC_STATUS_OK && ctx != NULL, "ctx create validated PASS");
    T_CHECK(zcc_vulkan_ctx_get_caps(ctx, &ccaps) == ZCC_STATUS_OK &&
            ccaps.physical_device_type == live_vk.physical_device_type &&
            ccaps.validation_layer_enabled == 1U,
            "ctx caps expose active deviceType + validation");

    const uint8_t hkey[8] = {0x4B, 0x45, 0x59, 0x21, 0x32, 0x31, 0x42, 0x02};
    zcc_vulkan_exec_options_t opt;
    zcc_vulkan_exec_options_init(&opt);
    opt.base.requested_backend = ZCC_BACKEND_VULKAN_SPV;
    opt.base.selection_policy = ZCC_POLICY_EXACT_REQUIRED;
    opt.require_validation_layer = 1U;
    opt.base.hmac_key = hkey;
    opt.base.hmac_key_len = sizeof(hkey);

    /* Non-Vulkan SCALAR_C99 skips expensive Vulkan ICD/layer init when ctx == NULL */
    float a4[4] = {1.5f, -2.0f, 3.25f, 4.0f};
    float b4[4] = {0.5f,  4.0f, -1.25f, 2.0f};
    float o_sc[4] = {0}, o_c1[4] = {0}, o_c2[4] = {0};
    zcc_tensor_desc_t da4, db4, dos, do1, do2;
    zcc_tensor_desc_init_1d(&da4, ZCC_DTYPE_F32, 4, a4, sizeof(a4), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&db4, ZCC_DTYPE_F32, 4, b4, sizeof(b4), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dos, ZCC_DTYPE_F32, 4, o_sc, sizeof(o_sc), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&do1, ZCC_DTYPE_F32, 4, o_c1, sizeof(o_c1), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&do2, ZCC_DTYPE_F32, 4, o_c2, sizeof(o_c2), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_t ins4[2] = {da4, db4};

    zcc_vulkan_exec_options_t opt_sc = opt;
    opt_sc.base.requested_backend = ZCC_BACKEND_SCALAR_C99;
    zcc_compute_v2_receipt_t r_sc, r1, r2;
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_VEC_ADD_F32, ins4, 2U, &dos, &opt_sc, &r_sc) == ZCC_STATUS_OK, "scalar v2 ok");
    T_CHECK(strcmp(r_sc.vulkan_caps.device_type_str, "NONE") == 0 && r_sc.vulkan_caps.vulkan_available == 0U,
            "scalar v2 skips redundant Vulkan probe");

    /* Small 4-elem dispatch on ctx -> grow to 256 -> shrink back to 5 elems cleanly */
    T_CHECK(zcc_vulkan_ctx_exec(ctx, ZCC_OP_VEC_ADD_F32, ins4, 2U, &do1, &opt, &r1) == ZCC_STATUS_OK, "ctx vec_add #1");
    T_CHECK(compare_arr_f32(o_c1, o_sc, 4U, 1e-6f, 1e-6f), "ctx vec_add matches scalar");
    T_CHECK(r1.sync_barrier_count == 2U && r1.validation_error_count == 0U, "ctx barriers=2 valerr=0");
    T_CHECK(zcc_receipt_v2_verify_canonical(&r1, hkey, sizeof(hkey)) == ZCC_STATUS_OK, "ctx v2 receipt valid");

    float *big_a = (float *)calloc(256U, sizeof(float));
    float *big_b = (float *)calloc(256U, sizeof(float));
    float *big_s = (float *)calloc(256U, sizeof(float));
    float *big_v = (float *)calloc(256U, sizeof(float));
    for (int i = 0; i < 256; i++) {
        big_a[i] = (float)(i - 100) * 0.25f;
        big_b[i] = (float)(i % 11) * 0.5f;
    }
    zcc_tensor_desc_t dba, dbb, dbs, dbv;
    zcc_tensor_desc_init_1d(&dba, ZCC_DTYPE_F32, 256, big_a, 256U * 4ULL, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dbb, ZCC_DTYPE_F32, 256, big_b, 256U * 4ULL, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dbs, ZCC_DTYPE_F32, 256, big_s, 256U * 4ULL, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dbv, ZCC_DTYPE_F32, 256, big_v, 256U * 4ULL, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_t ins_big[2] = {dba, dbb};
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_VEC_MUL_F32, ins_big, 2U, &dbs, &opt_sc, &r_sc) == ZCC_STATUS_OK, "scalar big mul");
    T_CHECK(zcc_vulkan_ctx_exec(ctx, ZCC_OP_VEC_MUL_F32, ins_big, 2U, &dbv, &opt, &r2) == ZCC_STATUS_OK, "ctx grow big mul");
    T_CHECK(compare_arr_f32(big_v, big_s, 256U, 1e-6f, 1e-6f), "ctx grow matches scalar");

    /* Shrink back + unaligned input staging after buffer reuse */
    uint8_t u_buf_raw[64] = {0};
    float *u_a = (float *)(u_buf_raw + 1);
    float u_vals[5] = {1.0f, 2.0f, 3.0f, -4.0f, 5.5f};
    memcpy(u_a, u_vals, sizeof(u_vals));
    float o_dot_s[1] = {0}, o_dot_c[1] = {0};
    zcc_tensor_desc_t dua, dub, ddos, ddoc;
    zcc_tensor_desc_init_1d(&dua, ZCC_DTYPE_F32, 5, u_a, sizeof(u_vals), 0, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dub, ZCC_DTYPE_F32, 5, u_vals, sizeof(u_vals), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&ddos, ZCC_DTYPE_F32, 1, o_dot_s, sizeof(o_dot_s), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&ddoc, ZCC_DTYPE_F32, 1, o_dot_c, sizeof(o_dot_c), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_t ins_dot[2] = {dua, dub};
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_DOT_F32, ins_dot, 2U, &ddos, &opt_sc, &r_sc) == ZCC_STATUS_OK, "scalar dot");
    T_CHECK(zcc_vulkan_ctx_exec(ctx, ZCC_OP_DOT_F32, ins_dot, 2U, &ddoc, &opt, &r2) == ZCC_STATUS_OK, "ctx shrink dot");
    T_CHECK(compare_arr_f32(o_dot_c, o_dot_s, 1U, 1e-5f, 1e-5f), "ctx shrink unaligned dot matches scalar");

    /* Deterministic cached pipeline replay */
    T_CHECK(zcc_vulkan_ctx_exec(ctx, ZCC_OP_VEC_ADD_F32, ins4, 2U, &do2, &opt, &r2) == ZCC_STATUS_OK, "ctx vec_add #2");
    T_CHECK(strcmp(r1.base.output_buffer_sha256, r2.base.output_buffer_sha256) == 0 &&
            strcmp(r1.spirv_module_sha256_hex, r2.spirv_module_sha256_hex) == 0,
            "ctx cached pipeline deterministic replay");

    /* Restricted caps fail cleanly on ctx and leave ctx healthy for subsequent dispatch */
    zcc_vulkan_caps_t small_ssbo = ccaps;
    small_ssbo.max_storage_buffer_range = 8U;
    T_CHECK(zcc_compute_exec_v2_with_ctx(ZCC_OP_VEC_ADD_F32, ins4, 2U, &do2, &opt, NULL, &small_ssbo, ctx, &r2) ==
            ZCC_STATUS_ERR_UNSUPPORTED_HW,
            "ctx restricted small ssbo rejected cleanly");
    T_CHECK(zcc_vulkan_ctx_exec(ctx, ZCC_OP_VEC_ADD_F32, ins4, 2U, &do2, &opt, &r2) == ZCC_STATUS_OK &&
            strcmp(r1.base.output_buffer_sha256, r2.base.output_buffer_sha256) == 0,
            "ctx remains healthy after restricted failure");

    /* Fast context (validation disabled) rejects require_validation_layer=1, succeeds when 0 */
    zcc_vulkan_ctx_t *ctx_noval = NULL;
    T_CHECK(zcc_vulkan_ctx_create(0, 0U, &ctx_noval) == ZCC_STATUS_OK && ctx_noval != NULL, "fast ctx create ok");
    T_CHECK(zcc_vulkan_ctx_exec(ctx_noval, ZCC_OP_VEC_ADD_F32, ins4, 2U, &do2, &opt, &r2) == ZCC_STATUS_ERR_UNSUPPORTED_HW,
            "fast ctx rejects require_validation_layer=1");
    zcc_vulkan_exec_options_t opt_fast = opt;
    opt_fast.require_validation_layer = 0U;
    T_CHECK(zcc_vulkan_ctx_exec(ctx_noval, ZCC_OP_VEC_ADD_F32, ins4, 2U, &do2, &opt_fast, &r2) == ZCC_STATUS_OK &&
            strcmp(r1.base.output_buffer_sha256, r2.base.output_buffer_sha256) == 0,
            "fast ctx executes deterministically when validation optional");

    zcc_vulkan_ctx_destroy(ctx_noval);
    zcc_vulkan_ctx_destroy(ctx);
    zcc_vulkan_ctx_destroy(NULL);
    free(big_a); free(big_b); free(big_s); free(big_v);
}

static int run_benchmarks(const char *out_path) {
    const int M = 128, K = 512, N = 128;
    float *A = (float *)calloc((size_t)M * K, sizeof(float));
    float *B = (float *)calloc((size_t)K * N, sizeof(float));
    float *C = (float *)calloc((size_t)M * N, sizeof(float));
    float *W = (float *)calloc((size_t)K, sizeof(float));
    float *X = (float *)calloc((size_t)K, sizeof(float));
    float *Y = (float *)calloc((size_t)M, sizeof(float));
    uint8_t *Q = (uint8_t *)calloc((size_t)M * (K / 256) * 144U, 1U);
    if (!A || !B || !C || !W || !X || !Y || !Q) return 1;

    for (int i = 0; i < M * K; i++) A[i] = (float)((i % 19) - 9) * 0.05f;
    for (int i = 0; i < K * N; i++) B[i] = (float)((i % 13) - 6) * 0.04f;
    for (int i = 0; i < K; i++) { W[i] = 1.0f + (float)(i % 7) * 0.05f; X[i] = (float)((i % 17) - 8) * 0.1f; }
    for (int r = 0; r < M; r++) {
        for (int b = 0; b < K / 256; b++) {
            uint8_t *blk = Q + (size_t)(r * (K / 256) + b) * 144U;
            store_le16(blk, encode_f16_simple(0.125f));
            store_le16(blk + 2, encode_f16_simple(0.0625f));
            memset(blk + 4, 0x15, 12U);
            memset(blk + 16, 0x37, 128U);
        }
    }

    zcc_tensor_desc_t dA, dB, dC, dXrm, dW, dYrm, dQ, dXv, dYv;
    zcc_tensor_desc_init_2d(&dA, ZCC_DTYPE_F32, M, K, 0, A, (uint64_t)M * K * 4ULL, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dB, ZCC_DTYPE_F32, K, N, 0, B, (uint64_t)K * N * 4ULL, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dC, ZCC_DTYPE_F32, M, N, 0, C, (uint64_t)M * N * 4ULL, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dXrm, ZCC_DTYPE_F32, M, K, 0, A, (uint64_t)M * K * 4ULL, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dW, ZCC_DTYPE_F32, K, W, (uint64_t)K * 4ULL, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dYrm, ZCC_DTYPE_F32, M, K, 0, A, (uint64_t)M * K * 4ULL, 1, ZCC_OWNER_BORROWED_HOST);
    dYrm.data = malloc((size_t)M * K * sizeof(float));
    dYrm.nbytes = (uint64_t)M * K * 4ULL;
    zcc_tensor_desc_init_2d(&dQ, ZCC_DTYPE_Q4_K, M, K, 0, Q, (uint64_t)M * (K / 256) * 144ULL, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dXv, ZCC_DTYPE_F32, K, X, (uint64_t)K * 4ULL, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dYv, ZCC_DTYPE_F32, M, Y, (uint64_t)M * 4ULL, 1, ZCC_OWNER_BORROWED_HOST);

    uint32_t backends[4] = {ZCC_BACKEND_SCALAR_C99, ZCC_BACKEND_AVX2_FMA, ZCC_BACKEND_PTX_SM120, ZCC_BACKEND_VULKAN_SPV};
    FILE *fp = out_path ? fopen(out_path, "w") : NULL;
    zcc_cpu_caps_t cpuc; zcc_probe_cpu_caps(&cpuc);
    zcc_ptx_caps_t ptxc; memset(&ptxc, 0, sizeof(ptxc)); zcc_probe_ptx_caps(&ptxc, NULL);
    zcc_vulkan_caps_t vkc; zcc_probe_vulkan_caps(&vkc, 0);
    zcc_vulkan_ctx_t *bench_ctx = NULL;
    if (zcc_vulkan_ctx_create(0, 0U, &bench_ctx) != ZCC_STATUS_OK || !bench_ctx) {
        if (fp) fclose(fp);
        free(dYrm.data); free(A); free(B); free(C); free(W); free(X); free(Y); free(Q);
        return 4;
    }
    if (fp) {
        fprintf(fp, "{\n  \"schema_version\": \"track21b.performance.v1\",\n"
                    "  \"hardware\": {\"vulkan_device\": \"%s\", \"vulkan_device_type\": \"%s\", \"vendor_id\": \"0x%04x\", \"ptx_available\": %u},\n"
                    "  \"notes\": \"Full dispatch + buffer SHA-256 + HtoD/DtoH (PTX when available) + stateless Vulkan vs persistent zcc_vulkan_ctx_t (%s %s).\",\n"
                    "  \"results\": [\n", vkc.device_name, vkc.device_type_str, vkc.vendor_id, ptxc.cuda_available, vkc.device_name, vkc.device_type_str);
    }

    int first = 1;
    for (int bi = 0; bi < 5; bi++) {
        int use_ctx = (bi == 4);
        uint32_t b_id = use_ctx ? ZCC_BACKEND_VULKAN_SPV : backends[bi];
        if (!use_ctx && b_id == ZCC_BACKEND_AVX2_FMA && !zcc_cpu_has_avx2_fma(&cpuc)) continue;
        if (!use_ctx && b_id == ZCC_BACKEND_PTX_SM120 && ptxc.cuda_available == 0U) continue;
        const char *b_label = use_ctx ? "VULKAN_SPV_CTX" : zcc_backend_v2_str(b_id);
        zcc_vulkan_exec_options_t opts;
        zcc_vulkan_exec_options_init(&opts);
        opts.base.requested_backend = b_id;
        opts.base.selection_policy = ZCC_POLICY_EXACT_REQUIRED;
        opts.require_validation_layer = 0U; /* Benchmark production release without debug validation overhead */

        const char *wl[4] = {"GEMM_F32_128x512x128", "RMSNORM_F32_128x512", "SOFTMAX_F32_128x512", "MATVEC_Q4_K_128x512"};
        int iters[4] = {5, 10, 10, 10};
        for (int wi = 0; wi < 4; wi++) {
            zcc_compute_v2_receipt_t r;
            zcc_tensor_desc_t ins[2], *outp = NULL;
            uint32_t nin = 2U;
            zcc_op_t op = ZCC_OP_GEMM_F32;
            if (wi == 0) { op = ZCC_OP_GEMM_F32; ins[0] = dA; ins[1] = dB; outp = &dC; }
            else if (wi == 1) { op = ZCC_OP_RMSNORM_F32; ins[0] = dXrm; ins[1] = dW; outp = &dYrm; }
            else if (wi == 2) { op = ZCC_OP_SOFTMAX_F32; ins[0] = dXrm; nin = 1U; outp = &dYrm; }
            else { op = ZCC_OP_MATVEC_Q4_K; ins[0] = dQ; ins[1] = dXv; outp = &dYv; }

            /* Warmup */
            zcc_status_t wst = use_ctx
                ? zcc_vulkan_ctx_exec(bench_ctx, op, ins, nin, outp, &opts, &r)
                : zcc_compute_exec_v2(op, ins, nin, outp, &opts, &r);
            if (wst != ZCC_STATUS_OK) {
                zcc_vulkan_ctx_destroy(bench_ctx);
                if (fp) fclose(fp);
                free(dYrm.data); free(A); free(B); free(C); free(W); free(X); free(Y); free(Q);
                return 2;
            }
            double t0 = now_us();
            for (int it = 0; it < iters[wi]; it++) {
                zcc_status_t ist = use_ctx
                    ? zcc_vulkan_ctx_exec(bench_ctx, op, ins, nin, outp, &opts, &r)
                    : zcc_compute_exec_v2(op, ins, nin, outp, &opts, &r);
                if (ist != ZCC_STATUS_OK) {
                    zcc_vulkan_ctx_destroy(bench_ctx);
                    if (fp) fclose(fp);
                    free(dYrm.data); free(A); free(B); free(C); free(W); free(X); free(Y); free(Q);
                    return 3;
                }
            }
            double avg = (now_us() - t0) / (double)iters[wi];
            printf("BENCH backend=%-14s op=%-22s avg_us=%9.2f out_sha=%.16s\n",
                   b_label, wl[wi], avg, r.base.output_buffer_sha256);
            if (fp) {
                fprintf(fp, "%s    {\"backend\": \"%s\", \"device_type\": \"%s\", \"workload\": \"%s\", \"iters\": %d, \"avg_us\": %.2f, \"last_output_sha256\": \"%s\"}\n",
                        first ? "" : ",", b_label,
                        (b_id == ZCC_BACKEND_VULKAN_SPV) ? vkc.device_type_str : "N/A",
                        wl[wi], iters[wi], avg, r.base.output_buffer_sha256);
                first = 0;
            }
        }
    }

    zcc_vulkan_ctx_destroy(bench_ctx);
    if (fp) {
        fprintf(fp, "  ]\n}\n");
        fclose(fp);
    }
    free(dYrm.data); free(A); free(B); free(C); free(W); free(X); free(Y); free(Q);
    return 0;
}

static void test_coverage_and_edge_contracts(void) {
    /* 1. SPIR-V emitter negative & error contracts */
    zcc_spv_module_t m_bad;
    memset(&m_bad, 0, sizeof(m_bad));
    T_CHECK(zcc_spirv_emit_op_kernel(0U, &m_bad) != 0, "emit_op_kernel rejects op_id=0");
    T_CHECK(zcc_spirv_emit_op_kernel(99U, &m_bad) != 0, "emit_op_kernel rejects op_id=99");
    T_CHECK(zcc_spirv_emit_op_kernel(ZCC_OP_VEC_ADD_F32, NULL) != 0, "emit_op_kernel rejects NULL module");
    T_CHECK(zcc_spirv_write_file(NULL, &m_bad) != 0, "write_file rejects NULL path");
    T_CHECK(zcc_spirv_write_file("/tmp/zcc_t21b_unused.spv", NULL) != 0, "write_file rejects NULL mod");
    zcc_spirv_module_free(NULL);

    zcc_spv_module_t m_ok;
    memset(&m_ok, 0, sizeof(m_ok));
    T_CHECK(zcc_spirv_emit_op_kernel(ZCC_OP_VEC_ADD_F32, &m_ok) == 0 && m_ok.words != NULL, "emit_op_kernel valid");
    T_CHECK(zcc_spirv_write_file("/nonexistent_zcc_t21b_dir/bad.spv", &m_ok) != 0, "write_file fails on missing dir");
    zcc_spirv_module_free(&m_ok);

    /* 2. Device type and backend string enum sweeps */
    T_CHECK(strcmp(zcc_vulkan_device_type_str(0U), "OTHER") == 0, "devtype 0 OTHER");
    T_CHECK(strcmp(zcc_vulkan_device_type_str(1U), "INTEGRATED_GPU") == 0, "devtype 1 INTEGRATED_GPU");
    T_CHECK(strcmp(zcc_vulkan_device_type_str(2U), "DISCRETE_GPU") == 0, "devtype 2 DISCRETE_GPU");
    T_CHECK(strcmp(zcc_vulkan_device_type_str(3U), "VIRTUAL_GPU") == 0, "devtype 3 VIRTUAL_GPU");
    T_CHECK(strcmp(zcc_vulkan_device_type_str(4U), "CPU") == 0, "devtype 4 CPU");
    T_CHECK(strcmp(zcc_vulkan_device_type_str(99U), "OTHER") == 0, "devtype 99 OTHER");
    T_CHECK(strcmp(zcc_backend_v2_str(ZCC_BACKEND_VULKAN_SPV), "VULKAN_SPV") == 0, "backend VULKAN_SPV");
    T_CHECK(strcmp(zcc_backend_v2_str(ZCC_BACKEND_VULKAN_GPU), "VULKAN_GPU") == 0, "backend VULKAN_GPU");
    T_CHECK(strcmp(zcc_backend_v2_str(ZCC_BACKEND_SCALAR_C99), "SCALAR_C99") == 0, "backend SCALAR_C99");
    T_CHECK(strcmp(zcc_backend_v2_str(ZCC_BACKEND_AVX2_FMA), "AVX2_FMA") == 0, "backend AVX2_FMA");
    T_CHECK(strcmp(zcc_backend_v2_str(ZCC_BACKEND_PTX_SM120), "PTX_SM120") == 0, "backend PTX_SM120");
    T_CHECK(strcmp(zcc_backend_v2_str(ZCC_BACKEND_AUTO), "AUTO") == 0, "backend AUTO");

    /* 3. Backend v2 negotiation negative/fallback legs */
    zcc_cpu_caps_t cpu_avx, cpu_noavx;
    zcc_probe_cpu_caps(&cpu_avx);
    cpu_noavx = cpu_avx;
    cpu_noavx.has_avx2 = 0U;
    cpu_noavx.has_fma = 0U;
    zcc_ptx_caps_t ptx_none;
    memset(&ptx_none, 0, sizeof(ptx_none));
    ptx_none.struct_size = (uint32_t)sizeof(ptx_none);
    zcc_vulkan_caps_t vk_none;
    memset(&vk_none, 0, sizeof(vk_none));
    vk_none.struct_size = (uint32_t)sizeof(vk_none);

    uint32_t sel = 0U, fb = 0U;
    char rsn[128] = {0};
    T_CHECK(zcc_negotiate_backend_v2_with_caps(NULL, &ptx_none, &vk_none, ZCC_BACKEND_VULKAN_SPV,
                                               ZCC_POLICY_EXACT_REQUIRED, 0U, &sel, &fb, rsn, sizeof(rsn)) ==
            ZCC_STATUS_ERR_NULL_PTR, "negotiate v2 NULL cpu fails");
    T_CHECK(zcc_negotiate_backend_v2_with_caps(&cpu_avx, &ptx_none, &vk_none, ZCC_BACKEND_VULKAN_SPV,
                                               999U, 0U, &sel, &fb, rsn, sizeof(rsn)) ==
            ZCC_STATUS_ERR_INVALID_ARG, "negotiate v2 invalid policy fails");
    T_CHECK(zcc_negotiate_backend_v2_with_caps(&cpu_avx, &ptx_none, &vk_none, ZCC_BACKEND_SCALAR_C99,
                                               ZCC_POLICY_EXACT_REQUIRED, 0U, &sel, &fb, rsn, sizeof(rsn)) ==
            ZCC_STATUS_OK && sel == ZCC_BACKEND_SCALAR_C99, "negotiate v2 delegates <= PTX_SM120");
    T_CHECK(zcc_negotiate_backend_v2_with_caps(&cpu_avx, &ptx_none, &vk_none, 999U,
                                               ZCC_POLICY_EXACT_REQUIRED, 0U, &sel, &fb, rsn, sizeof(rsn)) ==
            ZCC_STATUS_ERR_INVALID_ARG, "negotiate v2 rejects invalid backend id");

    if (zcc_cpu_has_avx2_fma(&cpu_avx)) {
        T_CHECK(zcc_negotiate_backend_v2_with_caps(&cpu_avx, &ptx_none, &vk_none, ZCC_BACKEND_VULKAN_GPU,
                                                   ZCC_POLICY_AUTO_FALLBACK, 0U, &sel, &fb, rsn, sizeof(rsn)) ==
                ZCC_STATUS_OK && fb == 1U && sel == ZCC_BACKEND_AVX2_FMA,
                "negotiate v2 GPU auto-fallback to AVX2 when no Vulkan");
        T_CHECK(zcc_negotiate_backend_v2_with_caps(&cpu_avx, &ptx_none, &vk_none, ZCC_BACKEND_VULKAN_SPV,
                                                   ZCC_POLICY_AUTO_FALLBACK, 0U, &sel, &fb, rsn, sizeof(rsn)) ==
                ZCC_STATUS_OK && fb == 1U && sel == ZCC_BACKEND_AVX2_FMA,
                "negotiate v2 SPV auto-fallback to AVX2 when no Vulkan");
    }
    T_CHECK(zcc_negotiate_backend_v2_with_caps(&cpu_noavx, &ptx_none, &vk_none, ZCC_BACKEND_VULKAN_GPU,
                                               ZCC_POLICY_AUTO_FALLBACK, 0U, &sel, &fb, rsn, sizeof(rsn)) ==
            ZCC_STATUS_OK && fb == 1U && sel == ZCC_BACKEND_SCALAR_C99,
            "negotiate v2 GPU auto-fallback to SCALAR when no Vulkan/AVX2");
    T_CHECK(zcc_negotiate_backend_v2_with_caps(&cpu_noavx, &ptx_none, &vk_none, ZCC_BACKEND_VULKAN_SPV,
                                               ZCC_POLICY_AUTO_FALLBACK, 0U, &sel, &fb, rsn, sizeof(rsn)) ==
            ZCC_STATUS_OK && fb == 1U && sel == ZCC_BACKEND_SCALAR_C99,
            "negotiate v2 SPV auto-fallback to SCALAR when no Vulkan/AVX2");
    T_CHECK(zcc_negotiate_backend_v2_with_caps(&cpu_noavx, &ptx_none, &vk_none, ZCC_BACKEND_VULKAN_SPV,
                                               ZCC_POLICY_EXACT_REQUIRED, 0U, &sel, &fb, rsn, sizeof(rsn)) ==
            ZCC_STATUS_ERR_BACKEND_UNAVAILABLE, "negotiate v2 SPV missing vulkan unavailable");

    /* 4. Direct zcc_vulkan_exec wrapper + null/ABI/ctx error checks */
    float a[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float b[4] = {5.0f, 6.0f, 7.0f, 8.0f};
    float o[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    zcc_tensor_desc_t da, db, dout;
    zcc_tensor_desc_init_1d(&da, ZCC_DTYPE_F32, 4, a, sizeof(a), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&db, ZCC_DTYPE_F32, 4, b, sizeof(b), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dout, ZCC_DTYPE_F32, 4, o, sizeof(o), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_t ins[2] = {da, db};

    zcc_vulkan_caps_t vkc;
    zcc_probe_vulkan_caps(&vkc, 1);
    zcc_vulkan_exec_options_t opt;
    zcc_vulkan_exec_options_init(&opt);
    opt.base.requested_backend = ZCC_BACKEND_VULKAN_SPV;
    opt.base.selection_policy = ZCC_POLICY_EXACT_REQUIRED;
    opt.require_validation_layer = 1U;

    char spv_sha[65] = {0}, err[192] = {0};
    uint32_t wc = 0U, bar = 0U, fl = 0U, inv = 0U, ve = 0U;
    T_CHECK(zcc_vulkan_exec(ZCC_OP_VEC_ADD_F32, ins, 2U, &dout, &opt, &vkc,
                            spv_sha, &wc, &bar, &fl, &inv, &ve, err, sizeof(err)) == ZCC_STATUS_OK &&
            wc > 5U && ve == 0U && fabsf(o[0] - 6.0f) < 1e-6f && fabsf(o[3] - 12.0f) < 1e-6f,
            "direct zcc_vulkan_exec wrapper succeeds");
    T_CHECK(zcc_vulkan_exec(ZCC_OP_VEC_ADD_F32, NULL, 2U, &dout, &opt, &vkc,
                            spv_sha, &wc, &bar, &fl, &inv, &ve, err, sizeof(err)) == ZCC_STATUS_ERR_INVALID_ARG,
            "zcc_vulkan_exec NULL inputs rejected");
    T_CHECK(zcc_vulkan_exec_on_ctx(NULL, ZCC_OP_VEC_ADD_F32, ins, 0U, &dout, &opt, &vkc,
                                   spv_sha, &wc, &bar, &fl, &inv, &ve, err, sizeof(err)) == ZCC_STATUS_ERR_INVALID_ARG,
            "zcc_vulkan_exec_on_ctx zero inputs rejected");
    T_CHECK(zcc_vulkan_ctx_create(1, 0U, NULL) == ZCC_STATUS_ERR_NULL_PTR, "ctx_create NULL out rejected");
    T_CHECK(zcc_vulkan_ctx_get_caps(NULL, &vkc) == ZCC_STATUS_ERR_INVALID_ARG, "ctx_get_caps NULL ctx rejected");

    zcc_compute_v2_receipt_t rec;
    T_CHECK(zcc_vulkan_ctx_exec(NULL, ZCC_OP_VEC_ADD_F32, ins, 2U, &dout, &opt, &rec) == ZCC_STATUS_ERR_NULL_PTR,
            "ctx_exec NULL ctx rejected");
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_VEC_ADD_F32, ins, 2U, &dout, NULL, &rec) == ZCC_STATUS_ERR_NULL_PTR,
            "exec_v2 NULL opts rejected");
    zcc_vulkan_exec_options_t bad_opt = opt;
    bad_opt.abi_v2_version = 0U;
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_VEC_ADD_F32, ins, 2U, &dout, &bad_opt, &rec) == ZCC_STATUS_ERR_ABI_MISMATCH,
            "exec_v2 bad ABI version rejected");
    T_CHECK(zcc_compute_exec_v2((zcc_op_t)9999, ins, 2U, &dout, &opt, &rec) != ZCC_STATUS_OK,
            "exec_v2 invalid op rejected");

    /* 5. Persistent ctx edge/validation/stride/1D op tests */
    zcc_vulkan_ctx_t *ctx = NULL;
    T_CHECK(zcc_vulkan_ctx_create(1, 0U, &ctx) == ZCC_STATUS_OK && ctx != NULL, "cov ctx create ok");
    zcc_vulkan_caps_t cc;
    T_CHECK(zcc_vulkan_ctx_get_caps(ctx, &cc) == ZCC_STATUS_OK && cc.vulkan_available == 1U, "cov ctx get_caps ok");
    T_CHECK(zcc_vulkan_ctx_get_caps(ctx, NULL) == ZCC_STATUS_ERR_NULL_PTR, "ctx get_caps NULL out rejected");

    zcc_vulkan_caps_t zero_grid = cc;
    zero_grid.max_workgroup_count[0] = 0U;
    T_CHECK(zcc_vulkan_exec_on_ctx(ctx, ZCC_OP_VEC_ADD_F32, ins, 2U, &dout, &opt, &zero_grid,
                                   spv_sha, &wc, &bar, &fl, &inv, &ve, err, sizeof(err)) ==
            ZCC_STATUS_ERR_UNSUPPORTED_HW, "grid limit exceeded fails closed");

    zcc_vulkan_caps_t no_avail = cc;
    no_avail.vulkan_available = 0U;
    T_CHECK(zcc_vulkan_exec_on_ctx(ctx, ZCC_OP_VEC_ADD_F32, ins, 2U, &dout, &opt, &no_avail,
                                   spv_sha, &wc, &bar, &fl, &inv, &ve, err, sizeof(err)) ==
            ZCC_STATUS_ERR_BACKEND_UNAVAILABLE, "direct exec_on_ctx unavail fails closed");

    zcc_vulkan_caps_t small_p = cc;
    small_p.max_push_constants_size = 32U;
    T_CHECK(zcc_vulkan_exec_on_ctx(ctx, ZCC_OP_VEC_ADD_F32, ins, 2U, &dout, &opt, &small_p,
                                   spv_sha, &wc, &bar, &fl, &inv, &ve, err, sizeof(err)) ==
            ZCC_STATUS_ERR_UNSUPPORTED_HW, "direct exec_on_ctx push<64 fails closed");

    zcc_vulkan_caps_t no_vvl = cc;
    no_vvl.validation_layer_available = 0U;
    T_CHECK(zcc_vulkan_exec_on_ctx(ctx, ZCC_OP_VEC_ADD_F32, ins, 2U, &dout, &opt, &no_vvl,
                                   spv_sha, &wc, &bar, &fl, &inv, &ve, err, sizeof(err)) ==
            ZCC_STATUS_ERR_BACKEND_UNAVAILABLE, "direct exec_on_ctx missing validation fails closed");

    float ga[8] = {0}, gb[8] = {0}, gc[16] = {0};
    zcc_tensor_desc_t dga, dgb, dgo_mis;
    zcc_tensor_desc_init_2d(&dga, ZCC_DTYPE_F32, 2, 2, 8, ga, sizeof(ga), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dgb, ZCC_DTYPE_F32, 2, 2, 8, gb, sizeof(gb), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dgo_mis, ZCC_DTYPE_F32, 2, 2, 10, gc, sizeof(gc), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_t gins[2] = {dga, dgb};
    T_CHECK(zcc_vulkan_exec_on_ctx(ctx, ZCC_OP_GEMM_F32, gins, 2U, &dgo_mis, &opt, &cc,
                                   spv_sha, &wc, &bar, &fl, &inv, &ve, err, sizeof(err)) ==
            ZCC_STATUS_ERR_MISALIGNED, "direct exec_on_ctx unaligned output byte stride rejected");

    /* validate_spv_framing boundaries + VVL vk_dbg_cb rejection */
    zcc_vulkan_exec_options_t spv_opt = opt;
    uint32_t short_spv[3] = {ZCC_SPIRV_MAGIC_NUMBER, ZCC_SPIRV_VERSION_1_0, 0U};
    spv_opt.spirv_override_words = short_spv;
    spv_opt.spirv_override_word_count = 3U;
    T_CHECK(zcc_vulkan_ctx_exec(ctx, ZCC_OP_VEC_ADD_F32, ins, 2U, &dout, &spv_opt, &rec) ==
            ZCC_STATUS_ERR_BACKEND_RUNTIME, "short 3-word SPIR-V rejected");

    uint32_t zero_bound_spv[6] = {ZCC_SPIRV_MAGIC_NUMBER, ZCC_SPIRV_VERSION_1_0, 0U, 0U, 0U, (1U << 16) | 1U};
    spv_opt.spirv_override_words = zero_bound_spv;
    spv_opt.spirv_override_word_count = 6U;
    T_CHECK(zcc_vulkan_ctx_exec(ctx, ZCC_OP_VEC_ADD_F32, ins, 2U, &dout, &spv_opt, &rec) ==
            ZCC_STATUS_ERR_BACKEND_RUNTIME, "zero-bound SPIR-V header rejected");

    uint32_t trunc_spv[7] = {ZCC_SPIRV_MAGIC_NUMBER, ZCC_SPIRV_VERSION_1_0, 0U, 2U, 0U, (4U << 16) | 17U, 1U};
    spv_opt.spirv_override_words = trunc_spv;
    spv_opt.spirv_override_word_count = 7U;
    T_CHECK(zcc_vulkan_ctx_exec(ctx, ZCC_OP_VEC_ADD_F32, ins, 2U, &dout, &spv_opt, &rec) ==
            ZCC_STATUS_ERR_BACKEND_RUNTIME, "truncated SPIR-V instruction rejected");

    /* Valid-framed OpCapability Linkage (id=5) triggers VVL vk_dbg_cb & vkCreateShaderModule rejection */
    uint32_t bad_vvl_spv[7] = {
        ZCC_SPIRV_MAGIC_NUMBER, ZCC_SPIRV_VERSION_1_0, ZCC_SPIRV_GENERATOR_ID, 2U, 0U, (2U << 16) | 17U, 5U
    };
    spv_opt.spirv_override_words = bad_vvl_spv;
    spv_opt.spirv_override_word_count = 7U;
    zcc_status_t vvl_st = zcc_vulkan_ctx_exec(ctx, ZCC_OP_VEC_ADD_F32, ins, 2U, &dout, &spv_opt, &rec);
    T_CHECK(vvl_st == ZCC_STATUS_ERR_BACKEND_RUNTIME, "OpCapability Linkage rejected via vkCreateShaderModule/VVL");
    if (cc.validation_layer_enabled) {
        T_CHECK(rec.validation_error_count > 0U, "OpCapability Linkage triggers vk_dbg_cb validation error count > 0");
    }

    /* Strided 2D VEC_ADD_F32 fails closed on Vulkan (non-contiguous rank > 1) */
    float s_a[12] = {0}, s_b[12] = {0}, s_o[12] = {0};
    zcc_tensor_desc_t dsa, dsb, dso;
    zcc_tensor_desc_init_2d(&dsa, ZCC_DTYPE_F32, 2, 4, 6 * 4, s_a, sizeof(s_a), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dsb, ZCC_DTYPE_F32, 2, 4, 6 * 4, s_b, sizeof(s_b), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dso, ZCC_DTYPE_F32, 2, 4, 6 * 4, s_o, sizeof(s_o), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_t sins[2] = {dsa, dsb};
    T_CHECK(zcc_vulkan_ctx_exec(ctx, ZCC_OP_VEC_ADD_F32, sins, 2U, &dso, &opt, &rec) == ZCC_STATUS_ERR_INVALID_STRIDE,
            "strided rank=2 VEC_ADD_F32 rejected on Vulkan");

    /* Rank=1 1D RMSNORM_F32, SOFTMAX_F32, DEQUANT_Q8_0, DEQUANT_Q4_0, DEQUANT_Q4_K match SCALAR_C99 */
    zcc_vulkan_exec_options_t opt_sc;
    zcc_vulkan_exec_options_init(&opt_sc);
    opt_sc.base.requested_backend = ZCC_BACKEND_SCALAR_C99;
    opt_sc.base.selection_policy = ZCC_POLICY_EXACT_REQUIRED;

    float rx[8] = {1.0f, -2.0f, 3.0f, -0.5f, 0.25f, 1.5f, -1.25f, 2.0f};
    float rw[8] = {1.0f, 1.1f, 0.9f, 1.2f, 0.8f, 1.0f, 1.05f, 0.95f};
    float ro_s[8] = {0}, ro_v[8] = {0}, so_s[8] = {0}, so_v[8] = {0};
    zcc_tensor_desc_t drx, drw, dros, drov, dsos, dsov;
    zcc_tensor_desc_init_1d(&drx, ZCC_DTYPE_F32, 8, rx, sizeof(rx), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&drw, ZCC_DTYPE_F32, 8, rw, sizeof(rw), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dros, ZCC_DTYPE_F32, 8, ro_s, sizeof(ro_s), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&drov, ZCC_DTYPE_F32, 8, ro_v, sizeof(ro_v), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dsos, ZCC_DTYPE_F32, 8, so_s, sizeof(so_s), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dsov, ZCC_DTYPE_F32, 8, so_v, sizeof(so_v), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_t rins[2] = {drx, drw};
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_RMSNORM_F32, rins, 2U, &dros, &opt_sc, &rec) == ZCC_STATUS_OK, "1D scalar rmsnorm");
    T_CHECK(zcc_vulkan_ctx_exec(ctx, ZCC_OP_RMSNORM_F32, rins, 2U, &drov, &opt, &rec) == ZCC_STATUS_OK, "1D vk rmsnorm");
    T_CHECK(compare_arr_f32(ro_v, ro_s, 8U, 1e-4f, 1e-4f), "1D vk rmsnorm matches scalar");

    T_CHECK(zcc_compute_exec_v2(ZCC_OP_SOFTMAX_F32, &drx, 1U, &dsos, &opt_sc, &rec) == ZCC_STATUS_OK, "1D scalar softmax");
    T_CHECK(zcc_vulkan_ctx_exec(ctx, ZCC_OP_SOFTMAX_F32, &drx, 1U, &dsov, &opt, &rec) == ZCC_STATUS_OK, "1D vk softmax");
    T_CHECK(compare_arr_f32(so_v, so_s, 8U, 1e-5f, 1e-5f), "1D vk softmax matches scalar");

    uint8_t q8[34]; memset(q8, 0, sizeof(q8)); store_le16(q8, encode_f16_simple(0.25f));
    for (int i = 0; i < 32; i++) q8[2 + i] = (uint8_t)((int8_t)(i - 16));
    float q8_s[32] = {0}, q8_v[32] = {0};
    zcc_tensor_desc_t dq8, dq8s, dq8v;
    zcc_tensor_desc_init_1d(&dq8, ZCC_DTYPE_Q8_0, 32, q8, sizeof(q8), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dq8s, ZCC_DTYPE_F32, 32, q8_s, sizeof(q8_s), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dq8v, ZCC_DTYPE_F32, 32, q8_v, sizeof(q8_v), 1, ZCC_OWNER_BORROWED_HOST);
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_DEQUANT_Q8_0, &dq8, 1U, &dq8s, &opt_sc, &rec) == ZCC_STATUS_OK, "1D scalar dq8");
    T_CHECK(zcc_vulkan_ctx_exec(ctx, ZCC_OP_DEQUANT_Q8_0, &dq8, 1U, &dq8v, &opt, &rec) == ZCC_STATUS_OK, "1D vk dq8");
    T_CHECK(compare_arr_f32(q8_v, q8_s, 32U, 1e-5f, 1e-5f), "1D vk dq8 matches scalar");

    uint8_t q4[18]; memset(q4, 0, sizeof(q4)); store_le16(q4, encode_f16_simple(0.5f));
    for (int i = 0; i < 16; i++) q4[2 + i] = (uint8_t)(((i & 0x0F) << 4) | ((15 - i) & 0x0F));
    float q4_s[32] = {0}, q4_v[32] = {0};
    zcc_tensor_desc_t dq4, dq4s, dq4v;
    zcc_tensor_desc_init_1d(&dq4, ZCC_DTYPE_Q4_0, 32, q4, sizeof(q4), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dq4s, ZCC_DTYPE_F32, 32, q4_s, sizeof(q4_s), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dq4v, ZCC_DTYPE_F32, 32, q4_v, sizeof(q4_v), 1, ZCC_OWNER_BORROWED_HOST);
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_DEQUANT_Q4_0, &dq4, 1U, &dq4s, &opt_sc, &rec) == ZCC_STATUS_OK, "1D scalar dq4");
    T_CHECK(zcc_vulkan_ctx_exec(ctx, ZCC_OP_DEQUANT_Q4_0, &dq4, 1U, &dq4v, &opt, &rec) == ZCC_STATUS_OK, "1D vk dq4");
    T_CHECK(compare_arr_f32(q4_v, q4_s, 32U, 1e-5f, 1e-5f), "1D vk dq4 matches scalar");

    uint8_t q4k[144]; memset(q4k, 0, sizeof(q4k));
    store_le16(q4k, encode_f16_simple(0.125f)); store_le16(q4k + 2, encode_f16_simple(0.0625f));
    for (int i = 0; i < 12; i++) q4k[4 + i] = (uint8_t)(0x21U + (i & 7));
    for (int i = 0; i < 128; i++) q4k[16 + i] = (uint8_t)(((i * 3) & 0xF0U) | ((i + 5) & 0x0FU));
    float q4k_s[256] = {0}, q4k_v[256] = {0};
    zcc_tensor_desc_t dq4k, dq4ks, dq4kv;
    zcc_tensor_desc_init_1d(&dq4k, ZCC_DTYPE_Q4_K, 256, q4k, sizeof(q4k), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dq4ks, ZCC_DTYPE_F32, 256, q4k_s, sizeof(q4k_s), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dq4kv, ZCC_DTYPE_F32, 256, q4k_v, sizeof(q4k_v), 1, ZCC_OWNER_BORROWED_HOST);
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_DEQUANT_Q4_K, &dq4k, 1U, &dq4ks, &opt_sc, &rec) == ZCC_STATUS_OK, "1D scalar dq4k");
    T_CHECK(zcc_vulkan_ctx_exec(ctx, ZCC_OP_DEQUANT_Q4_K, &dq4k, 1U, &dq4kv, &opt, &rec) == ZCC_STATUS_OK, "1D vk dq4k");
    T_CHECK(compare_arr_f32(q4k_v, q4k_s, 256U, 1e-4f, 1e-4f), "1D vk dq4k matches scalar");

    /* 6. CPU fallback from VULKAN_SPV via FP64_REF + AUTO_FALLBACK + custom alloc (free_fn == NULL) */
    const uint8_t hkey[8] = "covkey1";
    float dot_o[1] = {0};
    zcc_tensor_desc_t ddot;
    zcc_tensor_desc_init_1d(&ddot, ZCC_DTYPE_F32, 1, dot_o, sizeof(dot_o), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_vulkan_exec_options_t fb_opt;
    zcc_vulkan_exec_options_init(&fb_opt);
    fb_opt.base.requested_backend = ZCC_BACKEND_VULKAN_SPV;
    fb_opt.base.selection_policy = ZCC_POLICY_AUTO_FALLBACK;
    fb_opt.base.accum_policy = ZCC_ACCUM_FP64_REF;
    fb_opt.base.alloc_fn = cov_custom_alloc;
    fb_opt.base.free_fn = NULL;
    fb_opt.base.hmac_key = hkey;
    fb_opt.base.hmac_key_len = sizeof(hkey);
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_DOT_F32, ins, 2U, &ddot, &fb_opt, &rec) == ZCC_STATUS_OK &&
            rec.base.fallback_occurred == 1U && rec.base.backend_executed == ZCC_BACKEND_SCALAR_C99 &&
            fabsf(dot_o[0] - 70.0f) < 1e-5f,
            "FP64_REF AUTO_FALLBACK routes cleanly to SCALAR_C99 with custom alloc");
    T_CHECK(zcc_receipt_v2_verify_canonical(&rec, hkey, sizeof(hkey)) == ZCC_STATUS_OK, "fallback v2 receipt valid");

    char jbuf[4096];
    T_CHECK(zcc_receipt_v2_to_json(&rec, jbuf, sizeof(jbuf)) > 0, "receipt_v2_to_json ok");
    T_CHECK(zcc_receipt_v2_to_json(NULL, jbuf, sizeof(jbuf)) < 0, "receipt_v2_to_json NULL rec fails");
    T_CHECK(zcc_receipt_v2_to_json(&rec, NULL, 0U) < 0, "receipt_v2_to_json NULL buf fails");
    T_CHECK(zcc_receipt_v2_seal(NULL, hkey, sizeof(hkey)) == ZCC_STATUS_ERR_NULL_PTR, "receipt_v2_seal NULL fails");
    T_CHECK(zcc_receipt_v2_verify_canonical(NULL, hkey, sizeof(hkey)) == ZCC_STATUS_ERR_NULL_PTR,
            "receipt_v2_verify_canonical NULL fails");
    rec.spirv_word_count += 1U;
    T_CHECK(zcc_receipt_v2_verify_canonical(&rec, hkey, sizeof(hkey)) == ZCC_STATUS_ERR_RECEIPT_TAMPER,
            "receipt_v2_verify_canonical detects spirv_word_count tamper");

    zcc_vulkan_ctx_destroy(ctx);
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    int run_ptx = 1;
    int prefer_ctx = 0;
    const char *bench_out = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--skip-ptx") == 0) run_ptx = 0;
        else if (strcmp(argv[i], "--prefer-ctx") == 0) prefer_ctx = 1;
        else if (strcmp(argv[i], "--bench") == 0 && i + 1 < argc) bench_out = argv[++i];
    }
    if (bench_out) return run_benchmarks(bench_out);

    test_device_caps_and_negotiation();
    test_all_13_ops_and_replay(run_ptx, prefer_ctx);
    test_persistent_vulkan_context();
    test_coverage_and_edge_contracts();
    printf("TRACK21B_C_TESTS pass=%d fail=%d\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
