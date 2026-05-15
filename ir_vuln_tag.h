/*
 * ir_vuln_tag.h — ZCC IR Vulnerability / Security Tag Schema
 *
 * PURPOSE: Defensive security-analysis scaffold ONLY.
 *   Provides a machine-parseable, extensible bitmask schema for annotating
 *   IR instructions (ir_node_t) with security-relevant metadata.
 *   This does NOT perform exploit execution, payload generation, or
 *   network calls.  It is a static-analysis metadata layer.
 *
 * DESIGN:
 *   - Tags are bit flags stored in ir_node_t.vuln_tags (unsigned int).
 *   - Multiple tags can coexist on a single instruction via OR.
 *   - IR_VULN_NONE == 0 is the safe default; calloc / ir_node_alloc()
 *     zeroes the field automatically.
 *   - ir_vuln_tag_from_str() always returns IR_VULN_UNKNOWN rather than
 *     failing on unrecognized input — safe extensibility.
 *   - ir_vuln_tag_unknown_safe() gracefully handles values with bits
 *     outside IR_VULN_ALL_KNOWN.
 *
 * USAGE:
 *   #include "ir_vuln_tag.h"
 *
 *   // Tag a call node as an untrusted external call:
 *   ir_vuln_tag_set(call_node, IR_VULN_UNTRUSTED_CALL);
 *
 *   // Tag a delegatecall node with delegate + privilege boundary:
 *   ir_vuln_tag_set(dcall_node,
 *       IR_VULN_DELEGATE_CALL | IR_VULN_PRIV_BOUNDARY);
 *
 *   // Query:
 *   if (ir_vuln_tag_has(node, IR_VULN_STATE_WRITE)) { ... }
 *
 *   // Serialize for machine parsing:
 *   const char *name = ir_vuln_tag_to_str(IR_VULN_DELEGATE_CALL);
 *
 * INTEGRATION WITH COMPILER PASSES:
 *   Call ir_pass_vuln_scan() (declared in this header, defined in
 *   ir_vuln_tag.c) from any compiler pass or analysis driver to enumerate
 *   all security-tagged IR nodes in a module.  RegisterWarden, liveness,
 *   and dominance analyses can query ir_node_t.vuln_tags directly.
 *
 * LEGACY COMPATIBILITY:
 *   ir_vuln_map_from_evm_tag() bridges the legacy evm_ir_tag_t integer
 *   values (in evm_lifter.h) into ir_vuln_tag_t bitmasks so that existing
 *   EVM lifter code can participate in the schema without a rewrite.
 */

#ifndef ZCC_IR_VULN_TAG_H
#define ZCC_IR_VULN_TAG_H

#include "ir.h"

/* ── Tag enum (bitmask) ────────────────────────────────────────────────── */
/*
 * Each value is a single bit.  Tags may be OR-ed together.
 * Stable string names (ir_vuln_tag_to_str) are the machine-parseable
 * canonical form.  Do NOT change numeric values of existing tags.
 */
typedef enum {
    IR_VULN_NONE             = 0,          /* untagged — no security concern          */
    IR_VULN_UNTRUSTED_CALL   = (1 << 0),   /* untrusted external call (CALL/CALLCODE) */
    IR_VULN_DELEGATE_CALL    = (1 << 1),   /* delegate call — caller context hazard   */
    IR_VULN_STATIC_CALL      = (1 << 2),   /* static/read-only call                   */
    IR_VULN_STATE_WRITE      = (1 << 3),   /* persistent state mutation (SSTORE etc.) */
    IR_VULN_PRIV_BOUNDARY    = (1 << 4),   /* privilege boundary crossing             */
    IR_VULN_UNKNOWN          = (1 << 5),   /* unknown or unclassified security event  */
    IR_VULN_SELFDESTRUCT     = (1 << 6),   /* destructive termination (SELFDESTRUCT)  */
    IR_VULN_CONTRACT_CREATE  = (1 << 7),   /* new contract deployment (CREATE/CREATE2)*/
    IR_VULN_EXEC_BARRIER     = (1 << 8),   /* execution barrier (REVERT/INVALID)      */
    /* bits 9–29 reserved for future schema extensions                         */
    IR_VULN_ALL_KNOWN        = (1 << 9) - 1, /* mask of all currently defined flags  */
    IR_VULN_FLAG_MAX         = (1 << 30)   /* sentinel — do not assign to nodes       */
} ir_vuln_tag_t;

/* ── Helper API ────────────────────────────────────────────────────────── */

/*
 * ir_vuln_tag_set — set one or more tags on an IR node.
 * Idempotent: calling twice with the same tags is safe.
 * `tags` may be a single flag or multiple flags OR-ed together.
 */
void ir_vuln_tag_set(ir_node_t *n, ir_vuln_tag_t tags);

/*
 * ir_vuln_tag_has — check whether ALL bits in `tags` are set on `n`.
 * Returns 1 if all requested bits are set, 0 otherwise.
 */
int  ir_vuln_tag_has(const ir_node_t *n, ir_vuln_tag_t tags);

/*
 * ir_vuln_tag_to_str — convert a single-bit tag to its stable string name.
 * Bit 0 → "IR_VULN_UNTRUSTED_CALL", etc.
 * IR_VULN_NONE (0) → "IR_VULN_NONE".
 * Multi-bit values (combinations) → "IR_VULN_MULTI".
 * Unrecognized single-bit → "IR_VULN_UNKNOWN".
 */
const char *ir_vuln_tag_to_str(ir_vuln_tag_t tag);

/*
 * ir_vuln_tag_from_str — parse a tag name string to its enum value.
 * Accepts the exact strings produced by ir_vuln_tag_to_str().
 * Returns IR_VULN_UNKNOWN for any unrecognized or NULL input (never fails).
 */
ir_vuln_tag_t ir_vuln_tag_from_str(const char *s);

/*
 * ir_vuln_tag_unknown_safe — safe coercion of a raw unsigned int.
 * If `raw` contains only bits within IR_VULN_ALL_KNOWN, returns `raw`
 * cast to ir_vuln_tag_t directly.
 * If `raw` contains bits outside IR_VULN_ALL_KNOWN (future extension),
 * returns IR_VULN_UNKNOWN to signal unrecognized schema extensions.
 */
ir_vuln_tag_t ir_vuln_tag_unknown_safe(unsigned int raw);

/*
 * ir_vuln_map_from_evm_tag — bridge legacy evm_ir_tag_t → ir_vuln_tag_t.
 * Maps the integer values set in ir_node_t.tag by the EVM lifter to
 * their corresponding ir_vuln_tag_t bitmask equivalents.
 * Returns IR_VULN_UNKNOWN for unrecognized evm_tag values.
 */
ir_vuln_tag_t ir_vuln_map_from_evm_tag(int evm_tag);

/*
 * ir_vuln_tags_to_json — emit a compact JSON array of tag name strings
 * for all set bits in n->vuln_tags, e.g.:
 *   ["IR_VULN_UNTRUSTED_CALL","IR_VULN_PRIV_BOUNDARY"]
 * Writes into `buf` (NUL-terminated, max `bufsz` bytes).
 * Safe for nodes with no tags (emits "[]").
 */
void ir_vuln_tags_to_json(const ir_node_t *n, char *buf, int bufsz);

/*
 * ir_pass_vuln_scan — compiler-pass integration hook.
 * Iterates all IR nodes in `mod`, collects security-tagged nodes, and
 * writes a human-readable summary to `fp` (pass NULL for stdout).
 *
 * This function is the entry point for future liveness/dominance/
 * RegisterWarden passes to consume vulnerability tags.  It does NOT
 * modify the IR; it is a read-only analysis pass.
 *
 * Returns the total count of nodes with non-zero vuln_tags across the
 * entire module.
 */
int ir_pass_vuln_scan(const ir_module_t *mod, FILE *fp);

#endif /* ZCC_IR_VULN_TAG_H */
