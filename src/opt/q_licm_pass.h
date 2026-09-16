/* ========================================================================= */
/* ZCC QUANTUM-CLASSICAL LOOP INVARIANT CODE MOTION (Q-LICM) PASS HEADER     */
/* ========================================================================= */
/* File: src/opt/q_licm_pass.h                                               */
/* Description: Hoists loop-invariant quantum gate synthesis and parameter   */
/*              transformations out of variational ansatz and hybrid loops.  */
/* ========================================================================= */

#ifndef ZCC_Q_LICM_PASS_H
#define ZCC_Q_LICM_PASS_H

#include "prelude.h"
#include "src/opt/loop_validator.h"
#include "zcc_opt_metrics.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t quantum_gates_inspected;
    uint32_t quantum_gates_hoisted;
    uint32_t preheaders_serviced;
    double   estimated_t_depth_savings;
} QLicmMetrics;

/* Core pass entry point */
bool opt_q_licm_pass(Function *fn, OptMetricsSink *metrics);

/* Helper queries */
bool is_quantum_gate_instr(const Instr *it);
bool is_instr_loop_invariant_quantum(Function *fn, const Instr *it, BlockID header_id, const bool *in_loop);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_Q_LICM_PASS_H */
