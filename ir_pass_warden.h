/*
 * ir_pass_warden.h — ZCC IR Data-Flow Taint Analysis Engine
 * ==========================================================
 * PURPOSE: Track tainted (externally-controlled) data flowing through the
 *   IR graph and detect when it reaches security-critical sinks (SSTORE,
 *   CALL, SELFDESTRUCT).  This is a read-only analysis pass — it never
 *   mutates the IR.
 *
 * DESIGN:
 *   - Every VReg (ir_node_t.dst) is mapped to a taint state.
 *   - Taint sources: IR_CALL nodes tagged IR_TAG_UNTRUSTED_EXTERNAL_CALL,
 *     IR_LOAD from CALLDATALOAD (approximation: all untagged IR_LOAD),
 *     and CALLER/CALLVALUE environment reads.
 *   - Propagation: any arithmetic/bitwise/cast op inherits taint from
 *     any tainted source operand.
 *   - Sinks: IR_STORE tagged IR_TAG_SSTORE, IR_CALL tagged as untrusted
 *     external call.
 *   - Conditional sanitization: comparison (EQ/LT/GT) of a tainted vreg
 *     against a known constant flags the result as "checked" (weaker taint).
 *
 * INTEGRATION:
 *   Wire ir_pass_warden into ir_pm_run_default AFTER GVN/const_fold so
 *   the analysis runs on maximally reduced IR.
 *
 * OUTPUT:
 *   Emits ANSI-colored trace diagnostics to stderr when a tainted value
 *   reaches a sink.  Returns the count of detected taint violations via
 *   ir_pass_result_t.nodes_modified.
 */

#ifndef ZCC_IR_PASS_WARDEN_H
#define ZCC_IR_PASS_WARDEN_H

#include "ir.h"
#include "ir_pass_manager.h"

/* ── Taint states ────────────────────────────────────────────────────── */
typedef enum {
    WARDEN_SAFE     = 0,  /* no external taint — provably safe         */
    WARDEN_TAINTED  = 1,  /* derived from untrusted external input     */
    WARDEN_CHECKED  = 2   /* tainted but compared against a constant   */
} warden_taint_t;

/* ── Public API ──────────────────────────────────────────────────────── */

/*
 * ir_pass_warden — data-flow taint analysis pass.
 *
 * Conforms to the ir_pass_fn signature (void* → ir_pass_result_t).
 * fn_ptr must be an ir_func_t*.
 *
 * Does NOT modify the IR.  Returns:
 *   .nodes_modified = number of taint violations detected
 *   .changed = 0 (always — read-only pass)
 *
 * Taint traces are emitted to stderr with ANSI formatting.
 */
ir_pass_result_t ir_pass_warden(void *fn_ptr);

#endif /* ZCC_IR_PASS_WARDEN_H */
