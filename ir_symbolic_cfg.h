/*
 * ir_symbolic_cfg.h — Backward-Tracing Symbolic Jump Resolver
 * ============================================================
 * PURPOSE: Resolve dynamic EVM jump targets by walking the IR def-use
 *   chain backward from unresolved JUMP/JUMPI sites to their constant
 *   origins.  Rewrites ghost IR_NOP (JUMP) and opaque IR_BR_IF (JUMPI)
 *   nodes into proper IR_BR / IR_BR_IF with resolved `.L_evm_<offset>`
 *   labels, giving downstream passes a complete Control Flow Graph.
 *
 * DESIGN:
 *   - Zero malloc during backward walk.  All state is on the C stack.
 *   - Hard recursion depth limit (MAX_SYMBOLIC_DEPTH = 32) prevents
 *     infinite loops from circular IR or adversarial EVM bytecode.
 *   - Validates resolved targets against existing IR_LABEL nodes in
 *     the function — equivalent to the lifter's valid_jumpdest check.
 *
 * INTEGRATION:
 *   Wire ir_pass_symbolic_cfg as the FIRST pass in ir_pm_run_default,
 *   before DCE or const fold, so all downstream optimization operates
 *   on a fully mapped CFG.
 */

#ifndef ZCC_IR_SYMBOLIC_CFG_H
#define ZCC_IR_SYMBOLIC_CFG_H

#include "ir.h"
#include "ir_pass_manager.h"

/* ── Configuration ───────────────────────────────────────────────────── */
enum {
    MAX_SYMBOLIC_DEPTH = 32  /* recursion ceiling for backward walk */
};

/* ── Resolve result ──────────────────────────────────────────────────── */
typedef enum {
    SYMCFG_UNRESOLVED = 0,   /* could not resolve to a constant       */
    SYMCFG_RESOLVED   = 1,   /* resolved to a concrete integer value  */
    SYMCFG_DEPTH_EXCEEDED = 2 /* hit MAX_SYMBOLIC_DEPTH               */
} symcfg_status_t;

/* ── Public API ──────────────────────────────────────────────────────── */

/*
 * ir_pass_symbolic_cfg — backward-tracing symbolic jump resolver.
 *
 * Conforms to ir_pass_fn signature (void* → ir_pass_result_t).
 * fn_ptr must be an ir_func_t*.
 *
 * For each ghost jump (IR_NOP with src1 = target VReg), walks the IR
 * def-use chain backward to resolve the target to a constant.  On
 * success, rewrites to IR_BR / IR_BR_IF with `.L_evm_<offset>` label.
 *
 * Returns:
 *   .nodes_modified = number of jumps successfully resolved
 *   .changed = non-zero if any rewrites occurred
 */
ir_pass_result_t ir_pass_symbolic_cfg(void *fn_ptr);

#endif /* ZCC_IR_SYMBOLIC_CFG_H */
