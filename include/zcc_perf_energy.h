/* ========================================================================= */
/* ZCC MICROARCHITECTURAL PROFILER & RAPL ENERGY HARVESTER (E1-E5)           */
/* ========================================================================= */
/* File: include/zcc_perf_energy.h                                           */
/* Description: Real-time hardware performance counter sampling and RAPL     */
/*              Joules energy profiling injected into AST optimization.      */
/*                                                                           */
/* Features:                                                                 */
/*   E1: Hardware Performance Counter Ingestion (Cycles, IPC, L1-Miss, Branch)*/
/*   E2: Intel/AMD RAPL Energy Measurement (Joules/Microjoules)              */
/*   E3: Thermal Budget & Dynamic Energy Throttle Policy                     */
/*   E4: AST Green-Computing Pass Selector (AVX2 Vector vs Low-Power Scalar) */
/*   E5: Microarchitectural Efficiency Metric (Joules/Instruction & IPC-Gain)*/
/* ========================================================================= */

#ifndef ZCC_PERF_ENERGY_H
#define ZCC_PERF_ENERGY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Operating Power Regimes */
typedef enum {
    ZCC_POWER_REGIME_PERFORMANCE = 0, /* Max Throughput: AVX2/FMA Unrolled */
    ZCC_POWER_REGIME_BALANCED    = 1, /* Adaptive: Vector loops, scalar tails */
    ZCC_POWER_REGIME_GREEN       = 2, /* Energy Cap: Low-Power Scalar Sliver */
    ZCC_POWER_REGIME_EMERGENCY   = 3  /* Thermal Throttle: Min Frequency */
} zcc_power_regime_t;

/* Microarchitectural Hardware Counters */
typedef struct {
    uint64_t cpu_cycles;
    uint64_t instructions;
    uint64_t l1_cache_misses;
    uint64_t l2_cache_misses;
    uint64_t l3_cache_misses;
    uint64_t branch_mispredictions;
    double   ipc;                 /* Instructions Per Cycle */
    double   l1_miss_rate;        /* L1 Misses / Instructions */
    double   branch_miss_rate;    /* Mispredicts / Instructions */
} zcc_hw_counters_t;

/* RAPL Energy Metrics */
typedef struct {
    double   package_joules;      /* Total CPU Package Energy */
    double   core_joules;         /* CPU Core Domain Energy */
    double   dram_joules;         /* Memory Domain Energy */
    double   duration_seconds;    /* Elapsed Sampling Duration */
    double   average_watts;       /* Total Watts (Joules / sec) */
    double   joules_per_instruction; /* JPI: Energy Cost Per Op */
} zcc_rapl_energy_t;

/* Unified Profiler State & Optimization Profile */
typedef struct {
    zcc_hw_counters_t  hw_start;
    zcc_hw_counters_t  hw_end;
    zcc_hw_counters_t  hw_delta;
    
    zcc_rapl_energy_t  energy;
    
    double             thermal_temp_c;      /* Temperature in Celsius */
    double             thermal_budget_watts;/* Target Power Budget */
    zcc_power_regime_t active_regime;       /* Current Optimization Strategy */
    
    uint32_t           avx2_vector_decisions;
    uint32_t           scalar_sliver_decisions;
    double             energy_efficiency_score; /* (IPC / Watts) */

    /* E6: Quantum Thermal-Flux Annealing State */
    double             thermal_entropy;         /* S = k_B * ln(Omega) */
    double             flux_anneal_temperature; /* T_anneal for pass scheduling */
    uint32_t           passes_reordered;

    /* E7: Carbon-Aware Grid Sync State */
    double             grid_carbon_intensity_gco2_kwh; /* Current Grid Carbon (gCO2/kWh) */
    bool               grid_surge_mode;                /* Clean Energy Surge Mode Active */
    double             carbon_emissions_saved_g;       /* Grams CO2 Saved vs Baseline */

    /* E8: DPA Side-Channel Power Shield State */
    double             dpa_power_variance_watts;       /* Power Trace Variance sigma^2 */
    bool               dpa_balanced_power_mode;        /* Constant-Power AST Emission */

    /* E9: Sovereign ESG Energy Ledger Receipt */
    char               energy_ledger_hash[65];         /* SHA-256 Digest of ESG Certificate */
    double             total_lifetime_joules_saved;    /* Cumulative Joules Offset */
} zcc_energy_profiler_t;

/* ------------------------------------------------------------------------- */
/* FUNCTION PROTOTYPES (E1 - E9)                                             */
/* ------------------------------------------------------------------------- */

/* E1: Initialize and Read Microarchitectural Hardware Counters */
void zcc_hw_counters_sample(zcc_hw_counters_t *out_counters);
void zcc_hw_counters_compute_delta(
    const zcc_hw_counters_t *start,
    const zcc_hw_counters_t *end,
    zcc_hw_counters_t *out_delta
);

/* E2: RAPL Energy Sampling */
void zcc_rapl_energy_sample(zcc_rapl_energy_t *out_energy);
void zcc_rapl_energy_compute(
    const zcc_rapl_energy_t *start,
    const zcc_rapl_energy_t *end,
    uint64_t total_instructions,
    zcc_rapl_energy_t *out_result
);

/* E3: Profiler Lifecycle & Thermal Budgeting */
void zcc_energy_profiler_init(zcc_energy_profiler_t *prof, double budget_watts);
void zcc_energy_profiler_begin(zcc_energy_profiler_t *prof);
void zcc_energy_profiler_end(zcc_energy_profiler_t *prof);

/* E4: AST Green-Computing Pass Selector Decision Engine */
zcc_power_regime_t zcc_energy_evaluate_regime(
    zcc_energy_profiler_t *prof,
    double current_temp_c,
    double current_watts
);

bool zcc_ast_should_vectorize(
    zcc_energy_profiler_t *prof,
    uint32_t loop_trip_count,
    uint32_t memory_accesses_per_iter
);

/* E5: Audit & Efficiency Score Report */
void zcc_energy_profiler_print_report(const zcc_energy_profiler_t *prof);

/* E6: Quantum Thermal-Flux Annealing Pass Scheduler */
double zcc_thermal_flux_anneal_step(
    zcc_energy_profiler_t *prof,
    double initial_energy_cost,
    uint32_t num_passes,
    int *pass_schedule_indices
);

/* E7: Carbon-Aware Grid Synchronization */
void zcc_grid_carbon_sync(
    zcc_energy_profiler_t *prof,
    double carbon_intensity_gco2_kwh
);

double zcc_compute_carbon_offset(
    const zcc_energy_profiler_t *prof,
    double baseline_joules
);

/* E8: DPA Side-Channel Power Shield & Trace Flattening */
bool zcc_dpa_flatten_power_profile(
    zcc_energy_profiler_t *prof,
    double *power_samples_watts,
    size_t num_samples,
    double target_mean_watts
);

/* E9: Sovereign ESG Energy Ledger & Cryptographic Receipt Mint */
bool zcc_energy_ledger_mint_receipt(
    zcc_energy_profiler_t *prof,
    const char *project_name,
    char *out_json_receipt,
    size_t max_receipt_len
);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_PERF_ENERGY_H */
