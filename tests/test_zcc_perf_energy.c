#include "zcc_perf_energy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

int main(void) {
    printf("╔════════════════════════════════════════════════════════════════════════╗\n");
    printf("║ ZCC MICROARCHITECTURAL PROFILER & RAPL ENERGY HARVESTER GAUNTLET (E1-E5)║\n");
    printf("╚════════════════════════════════════════════════════════════════════════╝\n\n");

    /* E1 & E3: Profiler Lifecycle & Hardware Counter Ingestion */
    printf("[E1] Testing Hardware Performance Counter Sampling & Delta Calculations...\n");
    zcc_energy_profiler_t prof;
    zcc_energy_profiler_init(&prof, 65.0); // 65W TDP budget

    zcc_energy_profiler_begin(&prof);

    // Synthetic workload loop to generate instructions
    volatile double dummy = 1.0;
    for (int i = 0; i < 500000; i++) {
        dummy += sin((double)i * 0.01);
    }

    zcc_energy_profiler_end(&prof);

    printf("  CPU Cycles Elapsed: %lu | Instructions: %lu | IPC: %.3f\n",
           prof.hw_delta.cpu_cycles, prof.hw_delta.instructions, prof.hw_delta.ipc);
    assert(prof.hw_delta.cpu_cycles > 0);
    assert(prof.hw_delta.ipc > 0.0);
    printf("  [PASS] E1: Hardware counter sampling and delta calculations verified.\n\n");

    /* E2: RAPL Joules Energy Sampling */
    printf("[E2] Testing Intel/AMD RAPL Energy Measurement & Watts Metrics...\n");
    printf("  Package Energy: %.6f J | Core Energy: %.6f J | DRAM Energy: %.6f J\n",
           prof.energy.package_joules, prof.energy.core_joules, prof.energy.dram_joules);
    printf("  Average Power: %.2f Watts | Joules/Instruction: %.3e J/op\n",
           prof.energy.average_watts, prof.energy.joules_per_instruction);
    assert(prof.energy.package_joules > 0.0);
    assert(prof.energy.average_watts > 0.0);
    assert(prof.energy.joules_per_instruction > 0.0);
    printf("  [PASS] E2: RAPL energy telemetry and JPI metrics verified.\n\n");

    /* E3 & E4: Thermal Budgeting & AST Pass Selection */
    printf("[E3 & E4] Testing Dynamic Thermal Budgeting & AST Pass Selector...\n");
    
    // Test Case 1: Cool Temp (45 C) -> Maximum Performance (AVX2 Vector)
    zcc_power_regime_t r1 = zcc_energy_evaluate_regime(&prof, 45.0, 35.0);
    assert(r1 == ZCC_POWER_REGIME_PERFORMANCE);
    bool vec1 = zcc_ast_should_vectorize(&prof, 8, 2);
    assert(vec1 == true);

    // Test Case 2: Elevated Temp (78 C) -> Green Computing (Scalar Sliver Preferred)
    zcc_power_regime_t r2 = zcc_energy_evaluate_regime(&prof, 78.0, 68.0);
    assert(r2 == ZCC_POWER_REGIME_GREEN);
    bool vec2 = zcc_ast_should_vectorize(&prof, 16, 2);
    assert(vec2 == false); // Loop trip count 16 < 128 threshold in Green mode -> choose scalar

    // Test Case 3: Extreme Thermal Throttle (92 C) -> Emergency Low-Power
    zcc_power_regime_t r3 = zcc_energy_evaluate_regime(&prof, 92.0, 95.0);
    assert(r3 == ZCC_POWER_REGIME_EMERGENCY);
    bool vec3 = zcc_ast_should_vectorize(&prof, 256, 2);
    assert(vec3 == false); // Pure scalar force-down
    printf("  [PASS] E3 & E4: Dynamic thermal regimes and AST pass selection verified.\n\n");

    /* E5: Efficiency Audit Report */
    printf("[E5] Emitting Microarchitectural Efficiency Audit Report...\n");
    zcc_energy_profiler_print_report(&prof);
    assert(prof.energy_efficiency_score > 0.0);
    printf("  [PASS] E5: Efficiency score and audit report emission verified.\n\n");

    /* E6: Quantum Thermal-Flux Annealing */
    printf("[E6] Testing Quantum Thermal-Flux Annealing Pass Scheduler...\n");
    int passes[6] = {0, 1, 2, 3, 4, 5};
    double cost_before = 100.0;
    double cost_after = zcc_thermal_flux_anneal_step(&prof, cost_before, 6, passes);
    printf("  Anneal Cost Delta: %.3f -> %.3f (Passes Swapped: %u)\n", cost_before, cost_after, prof.passes_reordered);
    assert(cost_after <= cost_before);
    assert(prof.thermal_entropy > 0.0);
    printf("  [PASS] E6: Quantum thermal-flux annealing schedule verified.\n\n");

    /* E7: Carbon-Aware Grid Synchronization */
    printf("[E7] Testing Carbon-Aware Grid Synchronization & ESG Offset...\n");
    // 1. High Clean Surge (Wind/Solar: 42 gCO2/kWh)
    zcc_grid_carbon_sync(&prof, 42.0);
    assert(prof.grid_surge_mode == true);
    assert(prof.active_regime == ZCC_POWER_REGIME_PERFORMANCE);

    // 2. High Dirty Grid (Coal: 420 gCO2/kWh)
    zcc_grid_carbon_sync(&prof, 420.0);
    assert(prof.grid_surge_mode == false);
    assert(prof.active_regime == ZCC_POWER_REGIME_GREEN);

    // 3. Offset Calculation
    double grams_saved = zcc_compute_carbon_offset(&prof, prof.energy.package_joules + 3600000.0); // +1 kWh baseline
    printf("  Grams CO2 Avoided: %.4f g\n", grams_saved);
    assert(grams_saved > 0.0);
    printf("  [PASS] E7: Real-time carbon-aware grid sync & ESG offset verified.\n\n");

    /* E8: DPA Side-Channel Power Shield */
    printf("[E8] Testing DPA Side-Channel Power Trace Flattening...\n");
    double raw_power_traces[8] = {12.4, 45.8, 18.2, 85.1, 14.0, 72.3, 22.1, 91.5}; // High power jitter
    bool dpa_ok = zcc_dpa_flatten_power_profile(&prof, raw_power_traces, 8, 40.0);
    assert(dpa_ok);
    printf("  Flattened Power Trace Variance: %.6f W^2 (Shield: ACTIVE)\n", prof.dpa_power_variance_watts);
    assert(prof.dpa_power_variance_watts < 10.0);
    printf("  [PASS] E8: DPA side-channel power flattening verified.\n\n");

    /* E9: Sovereign ESG Energy Ledger Mint */
    printf("[E9] Minting Sovereign Cryptographic ESG Energy Receipt...\n");
    char receipt_json[1024];
    bool mint_ok = zcc_energy_ledger_mint_receipt(&prof, "ZCC-Compiler-Core-v3.0", receipt_json, sizeof(receipt_json));
    assert(mint_ok);
    printf("  Ledger SHA-256 Seal: %s\n", prof.energy_ledger_hash);
    printf("  Generated ESG Certificate:\n%s\n", receipt_json);
    assert(strlen(prof.energy_ledger_hash) == 64);
    printf("  [PASS] E9: Cryptographic ESG energy ledger receipt mint verified.\n\n");

    printf("========================================================================\n");
    printf("  🏆 MICROARCHITECTURAL PROFILER & CARBON HARVESTER (E1-E9): 100%% PASSED!\n");
    printf("========================================================================\n");
    return 0;
}
