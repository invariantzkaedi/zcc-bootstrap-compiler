/*
 * ZCC AI Bridge — Microsecond Compiler Pass Decision Engine Header
 * Connects ZCC Codegen (part3.c) to ZKAEDI Dual-Tier Model (zcc_ir_dual_tier_model.pkl)
 */

#ifndef ZCC_AI_BRIDGE_H
#define ZCC_AI_BRIDGE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// Compiler Pass Recommendation Types
typedef enum {
    ZCC_OPT_CONSTANT_FOLDING = 0,
    ZCC_OPT_LOOP_UNROLLING   = 1,
    ZCC_OPT_FUNCTION_INLINE  = 2,
    ZCC_OPT_VECTORIZE_SIMD   = 3
} ZccOptPassType;

// Pointer Alias Safety Categories
typedef enum {
    ZCC_ALIAS_NO_ALIAS_SAFE  = 0,
    ZCC_ALIAS_STACK_ALIASED  = 1,
    ZCC_ALIAS_HEAP_ESCAPE    = 2
} ZccAliasSafetyType;

// ZCC IR Features extracted in nanoseconds
typedef struct {
    int node_count;
    int branch_count;
    int call_count;
    int memory_count;
    int const_count;
    int arith_count;
    int phi_count;
} ZccIRStats;

// Microsecond AI Decision Output
typedef struct {
    double predicted_prime_energy;
    ZccOptPassType recommended_pass;
    ZccAliasSafetyType alias_safety;
    const char* predicted_subsystem;
} ZccAIDecision;

// Core Bridge Functions
void zcc_ai_bridge_init(void);
ZccAIDecision zcc_ai_predict_pass(const ZccIRStats* stats, const char* func_name);
const char* zcc_opt_pass_to_str(ZccOptPassType pass);

#ifdef __cplusplus
}
#endif

#endif // ZCC_AI_BRIDGE_H
