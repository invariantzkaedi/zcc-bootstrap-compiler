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

    T_CHECK(st == ZCC_STATUS_OK && vkc.vulkan_available == 1U, "Vulkan loader + llvmpipe available");
    T_CHECK(vkc.validation_layer_available == 1U && vkc.validation_layer_enabled == 1U, "VK_LAYER_KHRONOS_validation active");
    T_CHECK(vkc.physical_device_type == ZCC_VK_DEVTYPE_CPU && strcmp(vkc.device_type_str, "CPU") == 0,
            "WSL2 llvmpipe explicitly identified as CPU deviceType");

    /* CPU llvmpipe must reject ZCC_BACKEND_VULKAN_GPU under EXACT_REQUIRED */
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
    T_CHECK(st == ZCC_STATUS_ERR_UNSUPPORTED_HW, "VULKAN_GPU rejected fail-closed on CPU llvmpipe");
    T_CHECK(rec.base.fallback_occurred == 0U && strcmp(rec.v2_verification_status, "UNVERIFIED_RUNTIME_ONLY") == 0,
            "Rejected GPU request records no silent fallback and UNVERIFIED_RUNTIME_ONLY");

    /* With AUTO_FALLBACK, VULKAN_GPU falls back cleanly to VULKAN_SPV and records fallback */
    opts.base.selection_policy = ZCC_POLICY_AUTO_FALLBACK;
    st = zcc_compute_exec_v2(ZCC_OP_VEC_ADD_F32, ins, 2, &dout, &opts, &rec);
    T_CHECK(st == ZCC_STATUS_OK && rec.base.fallback_occurred == 1U && rec.base.backend_executed == ZCC_BACKEND_VULKAN_SPV,
            "AUTO_FALLBACK records explicit fallback from VULKAN_GPU to VULKAN_SPV");

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

    /* Corrupted SPIR-V override rejected cleanly without crash/leak */
    zcc_vulkan_exec_options_init(&opts);
    opts.base.requested_backend = ZCC_BACKEND_VULKAN_SPV;
    opts.base.selection_policy = ZCC_POLICY_EXACT_REQUIRED;
    uint32_t bad_spv[8] = {0xDEADBEEFU, 0x00010000U, 0U, 10U, 0U, 0U, 0U, 0U};
    opts.spirv_override_words = bad_spv;
    opts.spirv_override_word_count = 8U;
    st = zcc_compute_exec_v2(ZCC_OP_VEC_ADD_F32, ins, 2, &dout, &opts, &rec);
    T_CHECK(st == ZCC_STATUS_ERR_BACKEND_RUNTIME, "Corrupted SPIR-V payload fails closed in vkCreateShaderModule");
}

static void test_all_13_ops_and_replay(int run_ptx) {
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
        T_CHECK(zcc_compute_exec_v2(op, ins3, nin, &dout_vk, &opt_vk, &r_vk) == ZCC_STATUS_OK, "vk vec op");
        T_CHECK(r_vk.sync_barrier_count == 2U && r_vk.validation_error_count == 0U, "vk sync & validation");
        T_CHECK(compare_arr_f32(o_vk, o_ref, 17U, 1e-6f, 1e-6f), "vk vec matches scalar");
        T_CHECK(zcc_receipt_v2_verify_canonical(&r_vk, hkey, sizeof(hkey)) == ZCC_STATUS_OK, "v2 hmac receipt verifies");
    }

    /* Op 4: DOT */
    zcc_tensor_desc_init_1d(&dout_ref, ZCC_DTYPE_F32, 1, o_ref, 4U, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dout_vk, ZCC_DTYPE_F32, 1, o_vk, 4U, 1, ZCC_OWNER_BORROWED_HOST);
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_DOT_F32, ins3, 2U, &dout_ref, &opt_sc, &r_sc) == ZCC_STATUS_OK, "scalar dot");
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_DOT_F32, ins3, 2U, &dout_vk, &opt_vk, &r_vk) == ZCC_STATUS_OK, "vk dot");
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
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_GEMM_F32, ins_g, 2U, &dout_vk, &opt_vk, &r_vk) == ZCC_STATUS_OK, "vk gemm");
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
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_RMSNORM_F32, ins_r, 2U, &dout_vk, &opt_vk, &r_vk) == ZCC_STATUS_OK, "vk rmsnorm");
    T_CHECK(compare_arr_f32(o_vk, o_ref, 21U, 1e-5f, 1e-4f), "vk rmsnorm matches scalar");

    T_CHECK(zcc_compute_exec_v2(ZCC_OP_SOFTMAX_F32, &da, 1U, &dout_ref, &opt_sc, &r_sc) == ZCC_STATUS_OK, "scalar softmax");
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_SOFTMAX_F32, &da, 1U, &dout_vk, &opt_vk, &r_vk) == ZCC_STATUS_OK, "vk softmax");
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
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_GEMM_F32, ins_sg, 2U, &dout_vk, &opt_vk, &r_vk) == ZCC_STATUS_OK, "vk strided gemm");
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
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_SOFTMAX_F32, &da, 1U, &dout_vk, &opt_vk, &r_vk) == ZCC_STATUS_OK, "vk softmax nan");
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
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_DEQUANT_Q8_0, &da, 1U, &dout_vk, &opt_vk, &r_vk) == ZCC_STATUS_OK, "vk q8_0 unaligned+1");
    T_CHECK(compare_arr_f32(o_vk, o_ref, 32U, 1e-6f, 1e-5f), "vk q8_0 unaligned+1 matches scalar");

    zcc_tensor_desc_init_1d(&db, ZCC_DTYPE_F32, 32, xv, 32U * 4U, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dout_ref, ZCC_DTYPE_F32, 1, o_ref, 4U, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dout_vk, ZCC_DTYPE_F32, 1, o_vk, 4U, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_t ins_q[2] = {da, db};
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_MATVEC_Q8_0, ins_q, 2U, &dout_ref, &opt_sc, &r_sc) == ZCC_STATUS_OK, "sc mv q8_0");
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_MATVEC_Q8_0, ins_q, 2U, &dout_vk, &opt_vk, &r_vk) == ZCC_STATUS_OK, "vk mv q8_0");
    T_CHECK(compare_arr_f32(o_vk, o_ref, 1U, 5e-4f, 5e-4f), "vk mv q8_0 matches scalar");

    /* Q4_0 [1, 32] */
    zcc_tensor_desc_init_2d(&da, ZCC_DTYPE_Q4_0, 1, 32, 0, q4_buf, 18ULL, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dout_ref, ZCC_DTYPE_F32, 1, 32, 0, o_ref, 32U * 4U, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dout_vk, ZCC_DTYPE_F32, 1, 32, 0, o_vk, 32U * 4U, 1, ZCC_OWNER_BORROWED_HOST);
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_DEQUANT_Q4_0, &da, 1U, &dout_ref, &opt_sc, &r_sc) == ZCC_STATUS_OK, "sc q4_0");
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_DEQUANT_Q4_0, &da, 1U, &dout_vk, &opt_vk, &r_vk) == ZCC_STATUS_OK, "vk q4_0");
    T_CHECK(compare_arr_f32(o_vk, o_ref, 32U, 1e-6f, 1e-5f), "vk q4_0 matches scalar");

    ins_q[0] = da;
    zcc_tensor_desc_init_1d(&dout_ref, ZCC_DTYPE_F32, 1, o_ref, 4U, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dout_vk, ZCC_DTYPE_F32, 1, o_vk, 4U, 1, ZCC_OWNER_BORROWED_HOST);
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_MATVEC_Q4_0, ins_q, 2U, &dout_ref, &opt_sc, &r_sc) == ZCC_STATUS_OK, "sc mv q4_0");
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_MATVEC_Q4_0, ins_q, 2U, &dout_vk, &opt_vk, &r_vk) == ZCC_STATUS_OK, "vk mv q4_0");
    T_CHECK(compare_arr_f32(o_vk, o_ref, 1U, 5e-4f, 5e-4f), "vk mv q4_0 matches scalar");

    /* Q4_K [1, 256] + Deterministic Replay */
    zcc_tensor_desc_init_2d(&da, ZCC_DTYPE_Q4_K, 1, 256, 0, q4k_un, 144ULL, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dout_ref, ZCC_DTYPE_F32, 1, 256, 0, o_ref, 256U * 4U, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dout_vk, ZCC_DTYPE_F32, 1, 256, 0, o_vk, 256U * 4U, 1, ZCC_OWNER_BORROWED_HOST);
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_DEQUANT_Q4_K, &da, 1U, &dout_ref, &opt_sc, &r_sc) == ZCC_STATUS_OK, "sc q4_k");
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_DEQUANT_Q4_K, &da, 1U, &dout_vk, &opt_vk, &r_vk) == ZCC_STATUS_OK, "vk q4_k unaligned+1");
    T_CHECK(compare_arr_f32(o_vk, o_ref, 256U, 1e-6f, 1e-5f), "vk q4_k matches scalar");

    zcc_tensor_desc_init_1d(&db, ZCC_DTYPE_F32, 256, xv, sizeof(xv), 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dout_ref, ZCC_DTYPE_F32, 1, o_ref, 4U, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dout_vk, ZCC_DTYPE_F32, 1, o_vk, 4U, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_t dout_vk2;
    zcc_tensor_desc_init_1d(&dout_vk2, ZCC_DTYPE_F32, 1, o_vk2, 4U, 1, ZCC_OWNER_BORROWED_HOST);
    ins_q[0] = da; ins_q[1] = db;
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_MATVEC_Q4_K, ins_q, 2U, &dout_ref, &opt_sc, &r_sc) == ZCC_STATUS_OK, "sc mv q4_k");
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_MATVEC_Q4_K, ins_q, 2U, &dout_vk, &opt_vk, &r_vk) == ZCC_STATUS_OK, "vk mv q4_k #1");
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_MATVEC_Q4_K, ins_q, 2U, &dout_vk2, &opt_vk, &r_vk2) == ZCC_STATUS_OK, "vk mv q4_k #2");
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
    tamp = r_vk; snprintf(tamp.vulkan_caps.device_type_str, sizeof(tamp.vulkan_caps.device_type_str), "DISCRETE_GPU");
    T_CHECK(zcc_receipt_v2_verify_canonical(&tamp, hkey, sizeof(hkey)) == ZCC_STATUS_ERR_RECEIPT_TAMPER, "spoofed device_type_str rejected");
    tamp = r_vk; tamp.sync_barrier_count = 0U;
    T_CHECK(zcc_receipt_v2_verify_canonical(&tamp, hkey, sizeof(hkey)) == ZCC_STATUS_ERR_RECEIPT_TAMPER, "tampered barrier count rejected");

    /* OOM hook & ABI mismatch checks */
    g_oom_after = 0U; g_oom_calls = 0U;
    opt_vk.base.alloc_fn = mock_oom_alloc;
    opt_vk.base.free_fn = mock_oom_free;
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_MATVEC_Q4_K, ins_q, 2U, &dout_vk, &opt_vk, &r_vk) == ZCC_STATUS_ERR_OOM, "OOM hook clean");

    zcc_vulkan_exec_options_t bad_abi = opt_vk;
    bad_abi.abi_v2_version = 0x9999U;
    T_CHECK(zcc_compute_exec_v2(ZCC_OP_MATVEC_Q4_K, ins_q, 2U, &dout_vk, &bad_abi, &r_vk) == ZCC_STATUS_ERR_ABI_MISMATCH, "ABI mismatch rejected");

    /* Optional cross-check PTX vs Vulkan on same op when non-ASan */
    if (run_ptx) {
        zcc_vulkan_exec_options_t opt_ptx;
        zcc_vulkan_exec_options_init(&opt_ptx);
        opt_ptx.base.requested_backend = ZCC_BACKEND_PTX_SM120;
        opt_ptx.base.selection_policy = ZCC_POLICY_EXACT_REQUIRED;
        float o_ptx[4] = {0};
        zcc_tensor_desc_t dout_ptx;
        zcc_tensor_desc_init_1d(&dout_ptx, ZCC_DTYPE_F32, 1, o_ptx, sizeof(o_ptx), 1, ZCC_OWNER_BORROWED_HOST);
        zcc_status_t st_ptx = zcc_compute_exec_v2(ZCC_OP_MATVEC_Q4_K, ins_q, 2U, &dout_ptx, &opt_ptx, &r_sc);
        T_CHECK(st_ptx == ZCC_STATUS_OK && compare_arr_f32(o_ptx, o_vk, 1U, 5e-4f, 5e-4f), "PTX matches Vulkan Q4_K matvec");
    }
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
    zcc_vulkan_caps_t vkc; zcc_probe_vulkan_caps(&vkc, 0);
    if (fp) {
        fprintf(fp, "{\n  \"schema_version\": \"track21b.performance.v1\",\n"
                    "  \"hardware\": {\"vulkan_device\": \"%s\", \"vulkan_device_type\": \"%s\", \"vendor_id\": \"0x%04x\"},\n"
                    "  \"notes\": \"Full dispatch + buffer SHA-256 + HtoD/DtoH (PTX) + Vulkan staging/pipeline/sync (llvmpipe CPU ICD).\",\n"
                    "  \"results\": [\n", vkc.device_name, vkc.device_type_str, vkc.vendor_id);
    }

    int first = 1;
    for (int bi = 0; bi < 4; bi++) {
        zcc_vulkan_exec_options_t opts;
        zcc_vulkan_exec_options_init(&opts);
        opts.base.requested_backend = backends[bi];
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
            if (zcc_compute_exec_v2(op, ins, nin, outp, &opts, &r) != ZCC_STATUS_OK) {
                if (fp) fclose(fp);
                free(dYrm.data); free(A); free(B); free(C); free(W); free(X); free(Y); free(Q);
                return 2;
            }
            double t0 = now_us();
            for (int it = 0; it < iters[wi]; it++) {
                if (zcc_compute_exec_v2(op, ins, nin, outp, &opts, &r) != ZCC_STATUS_OK) return 3;
            }
            double avg = (now_us() - t0) / (double)iters[wi];
            printf("BENCH backend=%-10s op=%-22s avg_us=%9.2f out_sha=%.16s\n",
                   zcc_backend_v2_str(backends[bi]), wl[wi], avg, r.base.output_buffer_sha256);
            if (fp) {
                fprintf(fp, "%s    {\"backend\": \"%s\", \"device_type\": \"%s\", \"workload\": \"%s\", \"iters\": %d, \"avg_us\": %.2f, \"last_output_sha256\": \"%s\"}\n",
                        first ? "" : ",", zcc_backend_v2_str(backends[bi]),
                        (backends[bi] == ZCC_BACKEND_VULKAN_SPV) ? vkc.device_type_str : "N/A",
                        wl[wi], iters[wi], avg, r.base.output_buffer_sha256);
                first = 0;
            }
        }
    }

    if (fp) {
        fprintf(fp, "  ]\n}\n");
        fclose(fp);
    }
    free(dYrm.data); free(A); free(B); free(C); free(W); free(X); free(Y); free(Q);
    return 0;
}

int main(int argc, char **argv) {
    int run_ptx = 1;
    const char *bench_out = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--skip-ptx") == 0) run_ptx = 0;
        else if (strcmp(argv[i], "--bench") == 0 && i + 1 < argc) bench_out = argv[++i];
    }
    if (bench_out) return run_benchmarks(bench_out);

    test_device_caps_and_negotiation();
    test_all_13_ops_and_replay(run_ptx);
    printf("TRACK21B_C_TESTS pass=%d fail=%d\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
