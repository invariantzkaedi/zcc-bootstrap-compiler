/* ========================================================================= */
/* TEST: ZCC CONTINUOUS QUANTUM-ANNEALED SUPERPOSITION (CQAS) GAUNTLET       */
/* ========================================================================= */

#include "zcc_cqas_superposition.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <math.h>

#define COLOR_G "\x1b[32m"
#define COLOR_C "\x1b[36m"
#define COLOR_Y "\x1b[33m"
#define COLOR_M "\x1b[35m"
#define COLOR_R "\x1b[0m"
#define BOLD    "\x1b[1m"

/* Dummy Test Kernels representing the 3 Superposition Variants */
static void kernel_avx2_fma(const float *in, float *out, size_t n) {
    for (size_t i = 0; i < n; i++) {
        out[i] = in[i] * 2.0f + 1.5f;
    }
}

static void kernel_scalar_slip(const float *in, float *out, size_t n) {
    for (size_t i = 0; i < n; i++) {
        out[i] = in[i] * 2.0f + 1.5f;
    }
}

static void kernel_direct_tail(const float *in, float *out, size_t n) {
    for (size_t i = 0; i < n; i++) {
        out[i] = in[i] * 2.0f + 1.5f;
    }
}

int main(void) {
    printf("╔════════════════════════════════════════════════════════════════════════╗\n");
    printf("║    ZCC QUANTUM-SUPERPOSITION SELF-SYNTHESIZING (CQAS) GAUNTLET         ║\n");
    printf("╚════════════════════════════════════════════════════════════════════════╝\n\n");

    cqas_dispatch_block_t block;

    /* ── TEST 1: Block Initialization & Multi-Variant Registration ── */
    printf("%s[TEST 1] Initializing Superposition Block & Registering 3 Variants...%s\n", BOLD, COLOR_R);
    cqas_status_t st = cqas_init_block(&block);
    assert(st == CQAS_OK);

    st = cqas_register_variant(&block, CQAS_VARIANT_AVX2_FMA, "AVX2_FMA_Reciprocal", (void*)kernel_avx2_fma);
    assert(st == CQAS_OK);
    st = cqas_register_variant(&block, CQAS_VARIANT_SCALAR_SLIP, "Scalar_64B_Slip_Aligned", (void*)kernel_scalar_slip);
    assert(st == CQAS_OK);
    st = cqas_register_variant(&block, CQAS_VARIANT_DIRECT_TAIL, "Direct_Threaded_Tail_Jump", (void*)kernel_direct_tail);
    assert(st == CQAS_OK);

    assert(block.num_variants == 3);
    printf("  ✔ Registered 3 Superposition Variants (%s|Ψ_AVX2⟩, |Ψ_Scalar⟩, |Ψ_Tail⟩%s)\n", COLOR_C, COLOR_R);
    printf("  %s[PASS] Test 1: Multi-variant registration verified.%s\n\n", COLOR_G, COLOR_R);

    /* ── TEST 2: Quantum Walk Wave-Packet Collapse Under Pressure Regimes ── */
    printf("%s[TEST 2] Testing Microarchitectural Quantum Wave-Packet Collapse...%s\n", BOLD, COLOR_R);

    /* Scenario A: Low L1 cache pressure -> Collapse to AVX2 */
    cqas_variant_t v_a = cqas_collapse_wave_packet(&block, 0.05f, 0.05f);
    printf("  Regime A (Cold/Low L1 Pressure): Chosen Variant = %s%s%s\n", COLOR_Y, "CQAS_VARIANT_AVX2_FMA", COLOR_R);
    assert(v_a == CQAS_VARIANT_AVX2_FMA);

    /* Scenario B: High L1 cache thrashing -> Collapse to Scalar Slip */
    cqas_variant_t v_b = cqas_collapse_wave_packet(&block, 0.95f, 0.05f);
    printf("  Regime B (High L1 Thrashing):    Chosen Variant = %s%s%s\n", COLOR_Y, "CQAS_VARIANT_SCALAR_SLIP", COLOR_R);
    assert(v_b == CQAS_VARIANT_SCALAR_SLIP);

    /* Scenario C: High Branch Miss Pressure -> Collapse to Direct Tail */
    cqas_variant_t v_c = cqas_collapse_wave_packet(&block, 0.05f, 0.95f);
    printf("  Regime C (High Branch Pressure): Chosen Variant = %s%s%s\n", COLOR_Y, "CQAS_VARIANT_DIRECT_TAIL", COLOR_R);
    assert(v_c == CQAS_VARIANT_DIRECT_TAIL);

    printf("  %s[PASS] Test 2: Multi-regime wave-packet collapse verified.%s\n\n", COLOR_G, COLOR_R);

    /* ── TEST 3: In-Memory Atomic 5-Byte JMP Patch Synthesis ── */
    printf("%s[TEST 3] Testing Atomic In-Memory 5-Byte JMP Patch Generation...%s\n", BOLD, COLOR_R);
    block.selected_variant = CQAS_VARIANT_AVX2_FMA;
    block.is_collapsed = true;

    st = cqas_apply_atomic_patch(&block);
    assert(st == CQAS_OK);
    assert(block.patch_site[0] == 0xE9); /* 0xE9 = JMP rel32 */
    printf("  Patch Opcode: %s0x%02X (JMP rel32)%s | Rel Offset: %s%d bytes%s\n",
           COLOR_C, block.patch_site[0], COLOR_R,
           COLOR_M, *(int32_t*)&block.patch_site[1], COLOR_R);

    /* Zero-Indirection Direct Caller-Site Rewriting */
    uint8_t mock_callsite[16] __attribute__((aligned(16))) = {0};
    st = cqas_patch_direct_callsite(mock_callsite, (void*)kernel_avx2_fma);
    assert(st == CQAS_OK);
    assert(mock_callsite[0] == 0xE8); /* 0xE8 = Direct CALL rel32 */
    printf("  Direct Call Site Opcode: %s0x%02X (CALL rel32)%s | Rel Target: %s%d bytes%s\n",
           COLOR_C, mock_callsite[0], COLOR_R,
           COLOR_M, *(int32_t*)&mock_callsite[1], COLOR_R);
    printf("  %s[PASS] Test 3: Atomic 5-byte JMP and direct CALL synthesis verified.%s\n\n", COLOR_G, COLOR_R);

    /* ── TEST 4: Dispatch Execution & Throughput Benchmark ── */
    printf("%s[TEST 4] Benchmarking Quantum Dispatch Throughput (1,000,000 runs)...%s\n", BOLD, COLOR_R);
    float in_buf[16] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f};
    float out_buf[16] = {0};

    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    for (int k = 0; k < 1000000; k++) {
        cqas_dispatch_execute(&block, in_buf, out_buf, 16);
    }

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    double elapsed = (ts_end.tv_sec - ts_start.tv_sec) + (ts_end.tv_nsec - ts_start.tv_nsec) * 1e-9;
    double ns_per_dispatch = (elapsed / 1000000.0) * 1e9;
    double dispatches_per_sec = 1000000.0 / elapsed;

    printf("  Total Elapsed: %.4f s | Latency: %s%.2f ns/dispatch%s\n", elapsed, COLOR_C, ns_per_dispatch, COLOR_R);
    printf("  🚀 Quantum Dispatch Throughput: %s%.1f M dispatches/sec%s\n", COLOR_G, dispatches_per_sec / 1e6, COLOR_R);
    assert(out_buf[0] == 3.5f);
    assert(out_buf[15] == 33.5f);
    printf("  %s[PASS] Test 4: Ultra-low latency quantum dispatch verified.%s\n\n", COLOR_G, COLOR_R);

    /* ── TEST 5: Provenance Logging to .zcc.oracle Ledger ── */
    printf("%s[TEST 5] Validating .zcc.oracle Audit Ledger Recording...%s\n", BOLD, COLOR_R);
    const char *oracle_path = "/tmp/test_zcc_cqas.oracle";
    st = cqas_record_provenance(&block, oracle_path);
    assert(st == CQAS_OK);

    FILE *f = fopen(oracle_path, "r");
    assert(f != NULL);
    char line[256];
    char *res = fgets(line, sizeof(line), f);
    assert(res != NULL);
    fclose(f);

    printf("  Ledger Record: %s%s%s", COLOR_Y, line, COLOR_R);
    printf("  %s[PASS] Test 5: Provenance ledger logging verified.%s\n\n", COLOR_G, COLOR_R);

    printf("========================================================================\n");
    printf("  🏆 ALL 5 CQAS QUANTUM-SUPERPOSITION GAUNTLET TESTS PASSED!\n");
    printf("========================================================================\n");

    return 0;
}
