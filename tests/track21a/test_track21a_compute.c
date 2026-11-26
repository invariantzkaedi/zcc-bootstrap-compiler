/* SPDX-License-Identifier: MIT */
#define _POSIX_C_SOURCE 200809L
#include "zcc_compute.h"
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int g_pass = 0;
static int g_fail = 0;
static sigjmp_buf g_sigill_jmp;
static volatile sig_atomic_t g_got_sigill = 0;

static void sigill_handler(int sig) {
    (void)sig;
    g_got_sigill = 1;
    siglongjmp(g_sigill_jmp, 1);
}

#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; fprintf(stderr, "FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); } \
} while (0)

typedef struct {
    int calls;
    int fail_on_call;
    int active_allocs;
} fail_alloc_state_t;

static void *fault_alloc(size_t bytes, void *ctx) {
    fail_alloc_state_t *st = (fail_alloc_state_t *)ctx;
    int idx = st->calls++;
    if (idx >= st->fail_on_call) return NULL;
    void *p = malloc(bytes);
    if (p) st->active_allocs++;
    return p;
}

static void fault_free(void *ptr, void *ctx) {
    fail_alloc_state_t *st = (fail_alloc_state_t *)ctx;
    if (ptr) {
        st->active_allocs--;
        free(ptr);
    }
}

static uint16_t f32_to_f16_bits(float v) {
    uint32_t x = 0U;
    memcpy(&x, &v, 4U);
    uint32_t sign = (x >> 16) & 0x8000U;
    int exp = (int)((x >> 23) & 0xFFU) - 127 + 15;
    uint32_t mant = (x & 0x007FFFFFU) >> 13;
    if (exp <= 0) return (uint16_t)sign;
    if (exp >= 31) return (uint16_t)(sign | 0x7C00U);
    return (uint16_t)(sign | ((uint32_t)exp << 10) | mant);
}

static int g_enable_ptx = 1;

static void test_capability_and_fallback_g03(void) {
    zcc_cpu_caps_t live_cpu;
    zcc_ptx_caps_t live_ptx;
    memset(&live_ptx, 0, sizeof(live_ptx));
    live_ptx.struct_size = (uint32_t)sizeof(live_ptx);
    CHECK(zcc_probe_cpu_caps(&live_cpu) == ZCC_STATUS_OK, "probe_cpu_caps ok");
    if (g_enable_ptx) {
        CHECK(zcc_probe_ptx_caps(&live_ptx, NULL) == ZCC_STATUS_OK, "probe_ptx_caps ok");
    }
    printf("CAPS_CPU cpuid=%u osxsave=%u xcr0_xmm=%u xcr0_ymm=%u avx=%u avx2=%u fma=%u xcr0=0x%llx\n",
           live_cpu.has_cpuid, live_cpu.has_osxsave, live_cpu.xcr0_xmm, live_cpu.xcr0_ymm,
           live_cpu.has_avx, live_cpu.has_avx2, live_cpu.has_fma, (unsigned long long)live_cpu.xcr0_raw);
    if (g_enable_ptx) {
        printf("CAPS_PTX cuda=%u name=\"%s\" cc=%d.%d driver=%d\n",
               live_ptx.cuda_available, live_ptx.device_name, live_ptx.cc_major, live_ptx.cc_minor, live_ptx.driver_version);
    }

    CHECK(live_cpu.has_cpuid == 1U, "host has cpuid");
    CHECK(zcc_cpu_has_avx2_fma(&live_cpu) == 1, "host supports verified AVX2+OSXSAVE+XCR0+FMA");

    /* Pure negotiation tests against simulated missing CPU/OS bits */
    struct {
        const char *label;
        int clear_osxsave;
        int clear_xcr0_ymm;
        int clear_fma;
        int clear_avx2;
    } cases[4] = {
        { "no_osxsave", 1, 0, 0, 0 },
        { "no_xcr0_ymm", 0, 1, 0, 0 },
        { "no_fma", 0, 0, 1, 0 },
        { "no_avx2", 0, 0, 0, 1 }
    };

    float a[16], b[16], c[16];
    for (int i = 0; i < 16; i++) { a[i] = (float)i * 0.5f; b[i] = 2.0f; c[i] = -999.0f; }

    struct sigaction sa_new, sa_old;
    memset(&sa_new, 0, sizeof(sa_new));
    sa_new.sa_handler = sigill_handler;
    sigemptyset(&sa_new.sa_mask);
    sigaction(SIGILL, &sa_new, &sa_old);

    for (int ci = 0; ci < 4; ci++) {
        for (int k = 0; k < 16; k++) c[k] = -999.0f;
        zcc_cpu_caps_t sim = live_cpu;
        if (cases[ci].clear_osxsave)  sim.has_osxsave = 0U;
        if (cases[ci].clear_xcr0_ymm) { sim.xcr0_ymm = 0U; sim.xcr0_raw &= ~0x04ULL; }
        if (cases[ci].clear_fma)      sim.has_fma = 0U;
        if (cases[ci].clear_avx2)     sim.has_avx2 = 0U;

        CHECK(zcc_cpu_has_avx2_fma(&sim) == 0, cases[ci].label);

        uint32_t sel = 0U, fb = 0U;
        char reason[128];
        zcc_status_t st_exact = zcc_negotiate_backend_with_caps(
            &sim, &live_ptx, ZCC_BACKEND_AVX2_FMA, ZCC_POLICY_EXACT_REQUIRED,
            &sel, &fb, reason, sizeof(reason));
        CHECK(st_exact == ZCC_STATUS_ERR_UNSUPPORTED_HW, "exact AVX2 fails unsupported HW");

        /* Run restricted compute with SIGILL guard */
        zcc_tensor_desc_t ins[2], out;
        zcc_tensor_desc_init_1d(&ins[0], ZCC_DTYPE_F32, 16, a, sizeof(a), 4, ZCC_OWNER_BORROWED_HOST);
        zcc_tensor_desc_init_1d(&ins[1], ZCC_DTYPE_F32, 16, b, sizeof(b), 4, ZCC_OWNER_BORROWED_HOST);
        zcc_tensor_desc_init_1d(&out,    ZCC_DTYPE_F32, 16, c, sizeof(c), 4, ZCC_OWNER_BORROWED_HOST);

        zcc_exec_options_t opts;
        zcc_exec_options_init(&opts);
        opts.requested_backend = ZCC_BACKEND_AVX2_FMA;
        opts.selection_policy  = ZCC_POLICY_EXACT_REQUIRED;
        zcc_compute_receipt_t rec;

        g_got_sigill = 0;
        if (sigsetjmp(g_sigill_jmp, 1) == 0) {
            zcc_status_t r1 = zcc_compute_exec_with_caps(ZCC_OP_VEC_ADD_F32, ins, 2U, &out, &opts, &sim, &rec);
            CHECK(r1 == ZCC_STATUS_ERR_UNSUPPORTED_HW, "exec_with_caps rejects EXACT AVX2");
            CHECK(c[0] == -999.0f, "failed EXACT does not touch output");

            opts.selection_policy = ZCC_POLICY_AUTO_FALLBACK;
            zcc_status_t r2 = zcc_compute_exec_with_caps(ZCC_OP_VEC_ADD_F32, ins, 2U, &out, &opts, &sim, &rec);
            CHECK(r2 == ZCC_STATUS_OK, "fallback succeeds on restricted CPU");
            CHECK(rec.backend_executed == ZCC_BACKEND_SCALAR_C99, "fallback runs SCALAR_C99");
            CHECK(rec.fallback_occurred == 1U, "fallback_occurred recorded");
            CHECK(fabsf(c[3] - (a[3] + b[3])) < 1e-6f, "fallback computed correct value");
        }
        CHECK(g_got_sigill == 0, "zero SIGILL during restricted-cap execution");
    }

    /* Simulated missing GPU rejects PTX EXACT_REQUIRED without silent fallback */
    zcc_ptx_caps_t no_gpu = live_ptx;
    no_gpu.cuda_available = 0U;
    uint32_t psel = 0U, pfb = 0U;
    char preason[128];
    CHECK(zcc_negotiate_backend_with_caps(&live_cpu, &no_gpu, ZCC_BACKEND_PTX_SM120,
          ZCC_POLICY_EXACT_REQUIRED, &psel, &pfb, preason, sizeof(preason)) == ZCC_STATUS_ERR_BACKEND_UNAVAILABLE,
          "PTX EXACT fails when GPU missing");

    sigaction(SIGILL, &sa_old, NULL);
}

static void test_memory_safety_and_edges_g06(void) {
    float buf_a[32], buf_b[32], buf_out[32];
    for (int i = 0; i < 32; i++) { buf_a[i] = (float)(i + 1); buf_b[i] = 2.0f; buf_out[i] = -777.0f; }

    zcc_tensor_desc_t ins[2], out;
    zcc_exec_options_t opts;
    zcc_exec_options_init(&opts);
    opts.requested_backend = ZCC_BACKEND_SCALAR_C99;

    zcc_tensor_desc_init_1d(&ins[0], ZCC_DTYPE_F32, 8, buf_a, sizeof(buf_a), 4, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&ins[1], ZCC_DTYPE_F32, 8, buf_b, sizeof(buf_b), 4, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&out,    ZCC_DTYPE_F32, 8, buf_out, sizeof(buf_out), 4, ZCC_OWNER_BORROWED_HOST);

    /* NULL and ABI mismatches */
    CHECK(zcc_compute_exec(ZCC_OP_VEC_ADD_F32, NULL, 2U, &out, &opts, NULL) == ZCC_STATUS_ERR_NULL_PTR, "null inputs rejected");
    opts.abi_version = 0xBAD00001U;
    CHECK(zcc_compute_exec(ZCC_OP_VEC_ADD_F32, ins, 2U, &out, &opts, NULL) == ZCC_STATUS_ERR_ABI_MISMATCH, "abi mismatch rejected");
    zcc_exec_options_init(&opts);
    opts.requested_backend = ZCC_BACKEND_SCALAR_C99;

    /* READONLY output guard */
    out.ownership_flags = ZCC_OWNER_READONLY | ZCC_OWNER_BORROWED_HOST;
    CHECK(zcc_compute_exec(ZCC_OP_VEC_ADD_F32, ins, 2U, &out, &opts, NULL) == ZCC_STATUS_ERR_READONLY, "readonly output rejected");
    CHECK(buf_out[0] == -777.0f, "readonly buffer untouched");
    out.ownership_flags = ZCC_OWNER_BORROWED_HOST;

    /* Rank / shape / OOB / overflow / stride / alignment / aliasing */
    out.rank = 0U;
    CHECK(zcc_compute_exec(ZCC_OP_VEC_ADD_F32, ins, 2U, &out, &opts, NULL) == ZCC_STATUS_ERR_INVALID_SHAPE, "rank 0 rejected");
    out.rank = 1U;
    out.nbytes = 31U; /* need 32 bytes for 8 floats */
    CHECK(zcc_compute_exec(ZCC_OP_VEC_ADD_F32, ins, 2U, &out, &opts, NULL) == ZCC_STATUS_ERR_OOB, "1B short nbytes rejected");
    out.nbytes = sizeof(buf_out);

    zcc_tensor_desc_t big_a = ins[0], big_b = ins[1], big_o = out;
    big_a.rank = 2U; big_a.shape[0] = INT64_MAX / 2; big_a.shape[1] = 8;
    big_b.rank = 2U; big_b.shape[0] = INT64_MAX / 2; big_b.shape[1] = 8;
    big_o.rank = 2U; big_o.shape[0] = INT64_MAX / 2; big_o.shape[1] = 8;
    CHECK(zcc_compute_exec(ZCC_OP_VEC_ADD_F32, (zcc_tensor_desc_t[]){big_a, big_b}, 2U, &big_o, &opts, NULL) == ZCC_STATUS_ERR_OVERFLOW, "integer overflow rejected");

    ins[0].strides[0] = -4;
    CHECK(zcc_compute_exec(ZCC_OP_VEC_ADD_F32, ins, 2U, &out, &opts, NULL) == ZCC_STATUS_ERR_INVALID_STRIDE, "negative stride rejected");
    ins[0].strides[0] = 2;
    CHECK(zcc_compute_exec(ZCC_OP_VEC_ADD_F32, ins, 2U, &out, &opts, NULL) == ZCC_STATUS_ERR_INVALID_STRIDE, "overlapping stride rejected");
    ins[0].strides[0] = 0;

    ins[0].alignment = 3U;
    CHECK(zcc_compute_exec(ZCC_OP_VEC_ADD_F32, ins, 2U, &out, &opts, NULL) == ZCC_STATUS_ERR_MISALIGNED, "non-pow2 alignment rejected");
    ins[0].alignment = 64U;
    ins[0].data = (void *)((uint8_t *)buf_a + 4);
    CHECK(zcc_compute_exec(ZCC_OP_VEC_ADD_F32, ins, 2U, &out, &opts, NULL) == ZCC_STATUS_ERR_MISALIGNED, "unaligned ptr with align=64 rejected");
    ins[0].alignment = 4U;
    ins[0].data = buf_a;

    /* Partial overlap aliasing */
    out.data = (void *)(buf_a + 1);
    out.nbytes = 8U * sizeof(float);
    CHECK(zcc_compute_exec(ZCC_OP_VEC_ADD_F32, ins, 2U, &out, &opts, NULL) == ZCC_STATUS_ERR_ALIASING, "partial overlap alias rejected");
    /* Exact in-place vec_add allowed; GEMM/DOT out==in rejected */
    out.data = buf_a;
    out.nbytes = sizeof(buf_a);
    CHECK(zcc_compute_exec(ZCC_OP_VEC_ADD_F32, ins, 2U, &out, &opts, NULL) == ZCC_STATUS_OK, "exact in-place vec_add ok");
    CHECK(fabsf(buf_a[0] - 3.0f) < 1e-6f, "in-place vec_add result valid");

    /* Invalid RMSNorm eps */
    float w_arr[8] = {1,1,1,1,1,1,1,1};
    zcc_tensor_desc_t r_in[2];
    zcc_tensor_desc_init_1d(&r_in[0], ZCC_DTYPE_F32, 8, buf_b, sizeof(buf_b), 4, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&r_in[1], ZCC_DTYPE_F32, 8, w_arr, sizeof(w_arr), 4, ZCC_OWNER_BORROWED_HOST);
    out.data = buf_out;
    opts.rmsnorm_eps = 0.0f;
    CHECK(zcc_compute_exec(ZCC_OP_RMSNORM_F32, r_in, 2U, &out, &opts, NULL) == ZCC_STATUS_ERR_INVALID_ARG, "eps=0 rejected");
    opts.rmsnorm_eps = NAN;
    CHECK(zcc_compute_exec(ZCC_OP_RMSNORM_F32, r_in, 2U, &out, &opts, NULL) == ZCC_STATUS_ERR_INVALID_ARG, "eps=NaN rejected");
    opts.rmsnorm_eps = 1e-5f;

    /* Quant invalid K divisibility */
    uint8_t qblk[64] = {0};
    zcc_tensor_desc_t qd;
    zcc_tensor_desc_init_1d(&qd, ZCC_DTYPE_Q8_0, 30, qblk, sizeof(qblk), 1, ZCC_OWNER_BORROWED_HOST);
    out.shape[0] = 30;
    CHECK(zcc_compute_exec(ZCC_OP_DEQUANT_Q8_0, &qd, 1U, &out, &opts, NULL) == ZCC_STATUS_ERR_INVALID_SHAPE, "Q8_0 non-mult-32 K rejected");
    qd.dtype = ZCC_DTYPE_Q4_K;
    qd.shape[0] = 128;
    out.shape[0] = 128;
    CHECK(zcc_compute_exec(ZCC_OP_DEQUANT_Q4_K, &qd, 1U, &out, &opts, NULL) == ZCC_STATUS_ERR_INVALID_SHAPE, "Q4_K non-mult-256 K rejected");

    /* Unaligned host (+4 byte offset) valid execution across Scalar, AVX2, and PTX */
    uint8_t raw_unaligned[256];
    memset(raw_unaligned, 0, sizeof(raw_unaligned));
    float *ua = (float *)(raw_unaligned + 4);
    float *ub = (float *)(raw_unaligned + 68);
    float *uo = (float *)(raw_unaligned + 132);
    for (int i = 0; i < 13; i++) { ua[i] = (float)i + 0.25f; ub[i] = 2.0f * (float)i - 1.0f; }
    zcc_tensor_desc_t u_in[2], u_out;
    zcc_tensor_desc_init_1d(&u_in[0], ZCC_DTYPE_F32, 13, ua, 13 * 4, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&u_in[1], ZCC_DTYPE_F32, 13, ub, 13 * 4, 1, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&u_out,    ZCC_DTYPE_F32, 13, uo, 13 * 4, 1, ZCC_OWNER_BORROWED_HOST);
    int nbks = g_enable_ptx ? 3 : 2;
    uint32_t bks[3] = { ZCC_BACKEND_SCALAR_C99, ZCC_BACKEND_AVX2_FMA, ZCC_BACKEND_PTX_SM120 };
    for (int bi = 0; bi < nbks; bi++) {
        opts.requested_backend = bks[bi];
        opts.selection_policy = ZCC_POLICY_EXACT_REQUIRED;
        CHECK(zcc_compute_exec(ZCC_OP_VEC_ADD_F32, u_in, 2U, &u_out, &opts, NULL) == ZCC_STATUS_OK, "unaligned vec_add ok");
        CHECK(fabsf(uo[12] - (ua[12] + ub[12])) < 1e-6f, "unaligned tail index 12 ok");
    }

    /* OOM fault injection across Scalar and PTX multi-alloc staging paths with leak check */
    int oom_nbks = g_enable_ptx ? 2 : 1;
    for (int bi = 0; bi < oom_nbks; bi++) {
        uint32_t target_bk = (bi == 0) ? ZCC_BACKEND_SCALAR_C99 : ZCC_BACKEND_PTX_SM120;
        for (int fail_at = 0; fail_at <= 3; fail_at++) {
            fail_alloc_state_t fstate = { 0, fail_at, 0 };
            zcc_exec_options_t oopts;
            zcc_exec_options_init(&oopts);
            oopts.requested_backend = target_bk;
            oopts.selection_policy = ZCC_POLICY_EXACT_REQUIRED;
            oopts.alloc_fn = fault_alloc;
            oopts.free_fn  = fault_free;
            oopts.alloc_ctx = &fstate;
            zcc_status_t ost = zcc_compute_exec(ZCC_OP_VEC_ADD_F32, u_in, 2U, &u_out, &oopts, NULL);
            if (bi == 0 && fail_at > 0) {
                CHECK(ost == ZCC_STATUS_OK, "scalar succeeds when 1st alloc passes");
            } else {
                CHECK(ost == ZCC_STATUS_ERR_OOM, "OOM returned cleanly");
            }
            CHECK(fstate.active_allocs == 0, "zero leaked host allocations after OOM");
        }
    }
}

static void test_deterministic_replay_and_receipt_tamper_g08_g09(void) {
    float a[64], b[64], out[64];
    for (int i = 0; i < 64; i++) { a[i] = sinf((float)i * 0.1f); b[i] = cosf((float)i * 0.2f); }
    zcc_tensor_desc_t ins[2], outd;
    zcc_tensor_desc_init_1d(&ins[0], ZCC_DTYPE_F32, 64, a, sizeof(a), 4, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&ins[1], ZCC_DTYPE_F32, 64, b, sizeof(b), 4, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&outd,   ZCC_DTYPE_F32, 64, out, sizeof(out), 4, ZCC_OWNER_BORROWED_HOST);

    const uint8_t key[] = "zcc-track21a-hmac-test-key";
    int nbks = g_enable_ptx ? 3 : 2;
    uint32_t bks[3] = { ZCC_BACKEND_SCALAR_C99, ZCC_BACKEND_AVX2_FMA, ZCC_BACKEND_PTX_SM120 };
    for (int bi = 0; bi < nbks; bi++) {
        char first_out_hash[65] = {0};
        for (int rep = 0; rep < 3; rep++) {
            memset(out, 0xA5, sizeof(out));
            zcc_exec_options_t opts;
            zcc_exec_options_init(&opts);
            opts.requested_backend = bks[bi];
            opts.selection_policy = ZCC_POLICY_EXACT_REQUIRED;
            opts.determinism_mode = ZCC_DET_FIXED_REDUCTION;
            opts.hmac_key = key;
            opts.hmac_key_len = sizeof(key) - 1U;
            zcc_compute_receipt_t rec;
            CHECK(zcc_compute_exec(ZCC_OP_SOFTMAX_F32, ins, 1U, &outd, &opts, &rec) == ZCC_STATUS_OK, "replay softmax ok");
            CHECK(zcc_receipt_verify_canonical(&rec, key, sizeof(key) - 1U) == ZCC_STATUS_OK, "hmac receipt verifies");
            CHECK(strcmp(rec.verification_status, "UNVERIFIED_RUNTIME_ONLY") == 0, "runtime never self-certifies PASS");
            if (rep == 0) {
                snprintf(first_out_hash, sizeof(first_out_hash), "%s", rec.output_buffer_sha256);
            } else {
                CHECK(strcmp(first_out_hash, rec.output_buffer_sha256) == 0, "deterministic replay output SHA-256 identical");
            }
            if (bi == 0 && rep == 0) {
                /* T21-G09 negative tamper controls */
                zcc_compute_receipt_t tamp = rec;
                tamp.output_buffer_sha256[0] ^= 1;
                CHECK(zcc_receipt_verify_canonical(&tamp, key, sizeof(key) - 1U) == ZCC_STATUS_ERR_RECEIPT_TAMPER, "tamper output hash caught");

                tamp = rec;
                tamp.input_buffers_sha256[0] ^= 1;
                CHECK(zcc_receipt_verify_canonical(&tamp, key, sizeof(key) - 1U) == ZCC_STATUS_ERR_RECEIPT_TAMPER, "tamper input hash caught");

                tamp = rec;
                tamp.backend_executed = ZCC_BACKEND_PTX_SM120;
                CHECK(zcc_receipt_verify_canonical(&tamp, key, sizeof(key) - 1U) == ZCC_STATUS_ERR_RECEIPT_TAMPER, "tamper backend spoof caught");

                tamp = rec;
                snprintf(tamp.verification_status, sizeof(tamp.verification_status), "PASS");
                CHECK(zcc_receipt_verify_canonical(&tamp, key, sizeof(key) - 1U) == ZCC_STATUS_ERR_RECEIPT_TAMPER, "runtime self-certified PASS rejected");

                tamp = rec;
                tamp.zk_proof_present = 1U;
                CHECK(zcc_receipt_verify_canonical(&tamp, key, sizeof(key) - 1U) == ZCC_STATUS_ERR_RECEIPT_TAMPER, "forged zk_proof_present rejected");

                tamp = rec;
                snprintf(tamp.schema_version, sizeof(tamp.schema_version), "0.9");
                CHECK(zcc_receipt_verify_canonical(&tamp, key, sizeof(key) - 1U) == ZCC_STATUS_ERR_RECEIPT_TAMPER, "schema downgrade rejected");

                const uint8_t wrong_key[] = "wrong-key";
                CHECK(zcc_receipt_verify_canonical(&rec, wrong_key, sizeof(wrong_key) - 1U) == ZCC_STATUS_ERR_RECEIPT_TAMPER, "wrong hmac key caught");
            }
        }
    }
}

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static void *alloc_a64(size_t sz) {
    void *p = NULL;
    if (posix_memalign(&p, 64U, sz) != 0) return NULL;
    return p;
}

static int run_benchmarks(const char *json_out) {
    /* Real observed hardware benchmarks across GEMM, RMSNORM, SOFTMAX, MATVEC_Q4_K, VEC_FMA */
    uint32_t bks[3] = { ZCC_BACKEND_SCALAR_C99, ZCC_BACKEND_AVX2_FMA, ZCC_BACKEND_PTX_SM120 };
    FILE *fp = json_out ? fopen(json_out, "w") : NULL;
    if (json_out && !fp) {
        fprintf(stderr, "Cannot open benchmark output: %s\n", json_out);
        return 1;
    }

    const uint32_t M = 128, K = 512, N = 128;
    float *A = (float *)alloc_a64((size_t)M * K * 4U);
    float *B = (float *)alloc_a64((size_t)K * N * 4U);
    float *C = (float *)alloc_a64((size_t)M * N * 4U);
    float *X = (float *)alloc_a64((size_t)M * K * 4U);
    float *W = (float *)alloc_a64((size_t)K * 4U);
    float *Y = (float *)alloc_a64((size_t)M * K * 4U);
    uint8_t *WQ = (uint8_t *)alloc_a64((size_t)M * (K / 256U) * 144U);
    float *YQ = (float *)alloc_a64((size_t)M * 4U);

    for (uint32_t i = 0; i < M * K; i++) {
        A[i] = (float)((int)(i % 13U) - 6) * 0.05f;
        X[i] = (float)((int)(i % 17U) - 8) * 0.1f;
    }
    for (uint32_t i = 0; i < K * N; i++) B[i] = (float)((int)(i % 11U) - 5) * 0.04f;
    for (uint32_t i = 0; i < K; i++) W[i] = 1.0f + 0.01f * (float)(i % 7U);
    for (uint32_t r = 0; r < M; r++) {
        for (uint32_t sb = 0; sb < K / 256U; sb++) {
            uint8_t *blk = WQ + ((size_t)r * (K / 256U) + sb) * 144U;
            uint16_t d = f32_to_f16_bits(0.125f);
            uint16_t dm = f32_to_f16_bits(0.0625f);
            blk[0] = (uint8_t)(d & 255U); blk[1] = (uint8_t)(d >> 8);
            blk[2] = (uint8_t)(dm & 255U); blk[3] = (uint8_t)(dm >> 8);
            for (int i = 0; i < 12; i++) blk[4 + i] = (uint8_t)(3 + (i % 7));
            for (int i = 0; i < 128; i++) blk[16 + i] = (uint8_t)(((i * 7 + r) & 15) | ((((i + 3) * 5) & 15) << 4));
        }
    }

    zcc_tensor_desc_t da, db, dc, dx, dw, dy, dwq, dx1, dyq;
    zcc_tensor_desc_init_2d(&da, ZCC_DTYPE_F32, M, K, 0, A, (uint64_t)M * K * 4U, 32, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&db, ZCC_DTYPE_F32, K, N, 0, B, (uint64_t)K * N * 4U, 32, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dc, ZCC_DTYPE_F32, M, N, 0, C, (uint64_t)M * N * 4U, 32, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dx, ZCC_DTYPE_F32, M, K, 0, X, (uint64_t)M * K * 4U, 32, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dw, ZCC_DTYPE_F32, K, W, (uint64_t)K * 4U, 32, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dy, ZCC_DTYPE_F32, M, K, 0, Y, (uint64_t)M * K * 4U, 32, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_2d(&dwq, ZCC_DTYPE_Q4_K, M, K, 0, WQ, (uint64_t)M * (K / 256U) * 144ULL, 32, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dx1, ZCC_DTYPE_F32, K, X, (uint64_t)K * 4U, 32, ZCC_OWNER_BORROWED_HOST);
    zcc_tensor_desc_init_1d(&dyq, ZCC_DTYPE_F32, M, YQ, (uint64_t)M * 4U, 32, ZCC_OWNER_BORROWED_HOST);

    zcc_cpu_caps_t cpu; zcc_ptx_caps_t ptx;
    zcc_probe_cpu_caps(&cpu);
    zcc_probe_ptx_caps(&ptx, NULL);

    if (fp) {
        fprintf(fp, "{\n  \"schema_version\": \"track21a.performance.v1\",\n"
                    "  \"workload_dimensions\": {\"gemm_m\": %u, \"gemm_k\": %u, \"gemm_n\": %u, \"rmsnorm_rows\": %u, \"rmsnorm_dim\": %u, \"q4k_rows\": %u, \"q4k_cols\": %u},\n"
                    "  \"hardware\": {\"avx2\": %u, \"fma\": %u, \"gpu_available\": %u, \"gpu_name\": \"%s\", \"gpu_cc\": \"%d.%d\"},\n"
                    "  \"notes\": \"Measurements include full runtime dispatch + buffer SHA-256 receipt sealing + HtoD/DtoH transfers on PTX_SM120 (not kernel-only microseconds).\",\n"
                    "  \"results\": [\n",
                M, K, N, M, K, M, K, cpu.has_avx2, cpu.has_fma, ptx.cuda_available, ptx.device_name, ptx.cc_major, ptx.cc_minor);
    }

    int first_entry = 1;
    for (int bi = 0; bi < 3; bi++) {
        zcc_exec_options_t opts;
        zcc_exec_options_init(&opts);
        opts.requested_backend = bks[bi];
        opts.selection_policy = ZCC_POLICY_EXACT_REQUIRED;

        struct {
            const char *name;
            zcc_op_t op;
            zcc_tensor_desc_t ins[2];
            uint32_t nin;
            zcc_tensor_desc_t *out;
            int iters;
        } bms[4] = {
            { "GEMM_F32_128x512x128", ZCC_OP_GEMM_F32, {da, db}, 2U, &dc, 10 },
            { "RMSNORM_F32_128x512", ZCC_OP_RMSNORM_F32, {dx, dw}, 2U, &dy, 20 },
            { "SOFTMAX_F32_128x512", ZCC_OP_SOFTMAX_F32, {dx, dx}, 1U, &dy, 20 },
            { "MATVEC_Q4_K_128x512", ZCC_OP_MATVEC_Q4_K, {dwq, dx1}, 2U, &dyq, 20 }
        };

        for (int ki = 0; ki < 4; ki++) {
            zcc_compute_receipt_t rec;
            /* Warmup */
            zcc_status_t wst = zcc_compute_exec(bms[ki].op, bms[ki].ins, bms[ki].nin, bms[ki].out, &opts, &rec);
            if (wst != ZCC_STATUS_OK) {
                fprintf(stderr, "BENCH FAILED %s on %s: %d\n", bms[ki].name, zcc_backend_str(bks[bi]), wst);
                if (fp) fclose(fp);
                return 1;
            }
            uint64_t t0 = now_ns();
            for (int it = 0; it < bms[ki].iters; it++) {
                zcc_compute_exec(bms[ki].op, bms[ki].ins, bms[ki].nin, bms[ki].out, &opts, &rec);
            }
            uint64_t dt_ns = now_ns() - t0;
            double avg_us = (double)dt_ns / (double)bms[ki].iters / 1000.0;
            printf("BENCH backend=%-10s op=%-22s iters=%2d avg_us=%10.2f out_sha=%.16s\n",
                   zcc_backend_str(bks[bi]), bms[ki].name, bms[ki].iters, avg_us, rec.output_buffer_sha256);
            if (fp) {
                fprintf(fp, "%s    {\"backend\": \"%s\", \"workload\": \"%s\", \"iters\": %d, \"avg_us\": %.2f, \"last_output_sha256\": \"%s\"}\n",
                        first_entry ? "" : ",",
                        zcc_backend_str(bks[bi]), bms[ki].name, bms[ki].iters, avg_us, rec.output_buffer_sha256);
                first_entry = 0;
            }
        }
    }

    if (fp) {
        fprintf(fp, "  ]\n}\n");
        fclose(fp);
    }

    free(A); free(B); free(C); free(X); free(W); free(Y); free(WQ); free(YQ);
    return 0;
}

int main(int argc, char **argv) {
    const char *bench_json = NULL;
    int bench_only = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--bench") == 0 && i + 1 < argc) {
            bench_json = argv[++i];
            bench_only = 1;
        } else if (strcmp(argv[i], "--cpu-only") == 0) {
            g_enable_ptx = 0;
        }
    }

    if (!bench_only) {
        test_capability_and_fallback_g03();
        test_memory_safety_and_edges_g06();
        test_deterministic_replay_and_receipt_tamper_g08_g09();
        printf("TRACK21A_C_TESTS pass=%d fail=%d\n", g_pass, g_fail);
        if (g_fail != 0) {
            zcc_ptx_shutdown();
            return 1;
        }
    } else {
        if (run_benchmarks(bench_json) != 0) {
            zcc_ptx_shutdown();
            return 1;
        }
    }

    zcc_ptx_shutdown();
    return 0;
}
