/* ========================================================================= */
/* ZCC VERIFIABLE COMPILATION: BABYBEAR AIR TRACE GENERATOR                  */
/* ========================================================================= */
/* File: src/zk/zk_air_trace.c                                               */
/* Description: Generates Algebraic Intermediate Representation (AIR) trace  */
/*              matrices over the BabyBear prime field (p = 2^31 - 2^27 + 1) */
/*              to prove semantic compilation correctness in Zero-Knowledge. */
/* ========================================================================= */

#include "src/zk/zk_air_trace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ZkAirTraceContext *zk_air_trace_create(size_t capacity) {
    if (capacity == 0) capacity = 1024;

    ZkAirTraceContext *ctx = calloc(1, sizeof(ZkAirTraceContext));
    if (!ctx) return NULL;

    ctx->matrix = calloc(capacity * ZK_AIR_TRACE_WIDTH, sizeof(bb_elem_t));
    if (!ctx->matrix) {
        free(ctx);
        return NULL;
    }

    ctx->capacity = capacity;
    ctx->n_steps = 0;
    return ctx;
}

void zk_air_trace_destroy(ZkAirTraceContext *ctx) {
    if (!ctx) return;
    if (ctx->matrix) free(ctx->matrix);
    free(ctx);
}

static void set_cell(ZkAirTraceContext *ctx, size_t step, enum ZkAirColumn col, bb_elem_t val) {
    if (step >= ctx->capacity) return;
    ctx->matrix[step * ZK_AIR_TRACE_WIDTH + col] = val % BABYBEAR_PRIME;
}

static bb_elem_t get_cell(const ZkAirTraceContext *ctx, size_t step, enum ZkAirColumn col) {
    if (step >= ctx->n_steps) return 0;
    return ctx->matrix[step * ZK_AIR_TRACE_WIDTH + col];
}

int zk_air_trace_generate_fn(ZkAirTraceContext *ctx, const Function *fn) {
    if (!ctx || !fn) return -1;

    size_t step = 0;
    uint32_t pc = 0;

    for (BlockID bi = 0; bi < fn->n_blocks; bi++) {
        Block *bb = fn->blocks[bi];
        if (!bb || !bb->reachable) continue;

        for (Instr *it = bb->head; it; it = it->next) {
            if (step >= ctx->capacity) {
                size_t new_cap = ctx->capacity * 2;
                bb_elem_t *new_mat = realloc(ctx->matrix, new_cap * ZK_AIR_TRACE_WIDTH * sizeof(bb_elem_t));
                if (!new_mat) return -1;
                ctx->matrix = new_mat;
                ctx->capacity = new_cap;
            }

            set_cell(ctx, step, COL_STEP, (bb_elem_t)step);
            set_cell(ctx, step, COL_PC, (bb_elem_t)pc++);
            set_cell(ctx, step, COL_OPCODE, (bb_elem_t)it->op);
            set_cell(ctx, step, COL_DST_REG, (bb_elem_t)it->dst);

            bb_elem_t s0 = 0, s1 = 0, dst_val = 0;
            if (it->src[0] < MAX_INSTRS) s0 = ctx->vreg_state[it->src[0]];
            if (it->src[1] < MAX_INSTRS) s1 = ctx->vreg_state[it->src[1]];

            set_cell(ctx, step, COL_SRC0_VAL, s0);
            set_cell(ctx, step, COL_SRC1_VAL, s1);
            set_cell(ctx, step, COL_AUX0, (bb_elem_t)it->src[0]);
            set_cell(ctx, step, COL_AUX1, (bb_elem_t)it->src[1]);

            if (it->op == OP_CONST) {
                dst_val = (bb_elem_t)(it->imm % BABYBEAR_PRIME);
                set_cell(ctx, step, COL_IS_ARITH, 1);
            } else if (it->op == OP_ADD) {
                dst_val = bb_add(s0, s1);
                set_cell(ctx, step, COL_IS_ARITH, 1);
            } else if (it->op == OP_SUB) {
                dst_val = bb_sub(s0, s1);
                set_cell(ctx, step, COL_IS_ARITH, 1);
            } else if (it->op == OP_MUL) {
                dst_val = bb_mul(s0, s1);
                set_cell(ctx, step, COL_IS_ARITH, 1);
            } else if (it->op == OP_BR || it->op == OP_CONDBR) {
                set_cell(ctx, step, COL_IS_BRANCH, 1);
                set_cell(ctx, step, COL_FLAG_BRANCH_TAKEN, 1);
            } else if (it->op == OP_LOAD || it->op == OP_STORE) {
                set_cell(ctx, step, COL_IS_MEM, 1);
                set_cell(ctx, step, COL_MEM_ADDR, s0);
                set_cell(ctx, step, COL_MEM_VAL, s1);
            }

            set_cell(ctx, step, COL_DST_VAL, dst_val);
            if (it->dst && it->dst < MAX_INSTRS) {
                ctx->vreg_state[it->dst] = dst_val;
            }

            /* Step Checksum column */
            bb_elem_t chk = bb_add(bb_add(s0, s1), dst_val);
            set_cell(ctx, step, COL_CHECKSUM, chk);

            step++;
        }
    }

    ctx->n_steps = step;
    return 0;
}

bool zk_air_verify_arithmetic_constraints(const ZkAirTraceContext *ctx) {
    if (!ctx || ctx->n_steps == 0) return false;

    for (size_t s = 0; s < ctx->n_steps; s++) {
        bb_elem_t is_arith = get_cell(ctx, s, COL_IS_ARITH);
        if (is_arith != 1) continue;

        bb_elem_t op = get_cell(ctx, s, COL_OPCODE);
        bb_elem_t s0 = get_cell(ctx, s, COL_SRC0_VAL);
        bb_elem_t s1 = get_cell(ctx, s, COL_SRC1_VAL);
        bb_elem_t dst = get_cell(ctx, s, COL_DST_VAL);

        if (op == OP_ADD) {
            bb_elem_t expected = bb_add(s0, s1);
            if (dst != expected) return false;
        } else if (op == OP_SUB) {
            bb_elem_t expected = bb_sub(s0, s1);
            if (dst != expected) return false;
        } else if (op == OP_MUL) {
            bb_elem_t expected = bb_mul(s0, s1);
            if (dst != expected) return false;
        }
    }

    return true;
}

bool zk_air_verify_control_flow_constraints(const ZkAirTraceContext *ctx) {
    if (!ctx || ctx->n_steps == 0) return false;

    for (size_t s = 0; s < ctx->n_steps; s++) {
        bb_elem_t step_idx = get_cell(ctx, s, COL_STEP);
        if (step_idx != (bb_elem_t)s) return false;
    }

    return true;
}

/* ========================================================================= */
/* REMEDIATED SOUNDNESS VERIFIER (ZCC-QV-R01-M2)                             */
/* ========================================================================= */
bool zk_air_verify_full_soundness(const ZkAirTraceContext *ctx, const ZkAirVerifyContext *vctx) {
    if (!ctx || ctx->n_steps == 0) return false;

    /* 1. Program Identity Boundary Check */
#ifndef ZCC_MUT_DISABLE_PROGRAM_BINDING
    if (vctx && vctx->require_program_bound) {
        bb_elem_t trace_prog_hash = get_cell(ctx, 0, COL_AUX0);
        if (trace_prog_hash != (bb_elem_t)vctx->expected_program_hash) {
            return false; /* Program substitution rejected */
        }
    }
#endif

    /* Shadow register state for def-use tracking */
    bb_elem_t shadow_vregs[ZK_MAX_VREGS];
    bool      vreg_defined[ZK_MAX_VREGS];
    memset(shadow_vregs, 0, sizeof(shadow_vregs));
    memset(vreg_defined, 0, sizeof(vreg_defined));

    /* 2. Public Input Boundary Initialization */
    if (vctx && vctx->n_inputs > 0) {
        for (size_t i = 0; i < vctx->n_inputs && i < ZK_MAX_PUBLIC_INPUTS; i++) {
            uint32_t reg_id = (uint32_t)(1 + i);
#ifndef ZCC_MUT_DISABLE_INPUT_BINDING
            shadow_vregs[reg_id] = vctx->public_inputs[i];
#else
            shadow_vregs[reg_id] = 999; /* MUTATED: Accepts unverified trace value */
#endif
            vreg_defined[reg_id] = true;
        }
    }

    bb_elem_t final_output_val = 0;
    bool      has_output = false;

    /* 3. Instruction-by-Instruction Transition Verification */
    for (size_t s = 0; s < ctx->n_steps; s++) {
        /* A. Step monotonicity */
        bb_elem_t step_idx = get_cell(ctx, s, COL_STEP);
        if (step_idx != (bb_elem_t)s) return false;

        bb_elem_t op = get_cell(ctx, s, COL_OPCODE);
        bb_elem_t dst_reg = get_cell(ctx, s, COL_DST_REG);
        bb_elem_t src0_val = get_cell(ctx, s, COL_SRC0_VAL);
        bb_elem_t src1_val = get_cell(ctx, s, COL_SRC1_VAL);
        bb_elem_t dst_val = get_cell(ctx, s, COL_DST_VAL);
        bb_elem_t src0_reg = get_cell(ctx, s, COL_AUX0);
        bb_elem_t src1_reg = get_cell(ctx, s, COL_AUX1);

        /* B. Def-Use Consistency Enforcement */
#ifndef ZCC_MUT_DISABLE_DEF_USE
        if (src0_reg > 0 && src0_reg < ZK_MAX_VREGS) {
            if (!vreg_defined[src0_reg] || shadow_vregs[src0_reg] != src0_val) {
                return false; /* Operand 0 def-use chain violation */
            }
        }
        if (src1_reg > 0 && src1_reg < ZK_MAX_VREGS) {
            if (!vreg_defined[src1_reg] || shadow_vregs[src1_reg] != src1_val) {
                return false; /* Operand 1 def-use chain violation */
            }
        }
#endif

        /* C. Opcode-Specific Semantic Constraints (Fail-Closed Policy) */
        switch (op) {
            case OP_CONST: {
                if (dst_reg < ZK_MAX_VREGS) {
                    shadow_vregs[dst_reg] = dst_val;
                    vreg_defined[dst_reg] = true;
                }
                break;
            }
            case OP_ADD: {
                bb_elem_t expected = bb_add(src0_val, src1_val);
                if (dst_val != expected) return false;
                if (dst_reg < ZK_MAX_VREGS) {
                    shadow_vregs[dst_reg] = dst_val;
                    vreg_defined[dst_reg] = true;
                }
                break;
            }
            case OP_SUB: {
                bb_elem_t expected = bb_sub(src0_val, src1_val);
                if (dst_val != expected) return false;
                if (dst_reg < ZK_MAX_VREGS) {
                    shadow_vregs[dst_reg] = dst_val;
                    vreg_defined[dst_reg] = true;
                }
                break;
            }
            case OP_MUL: {
                bb_elem_t expected = bb_mul(src0_val, src1_val);
                if (dst_val != expected) return false;
                if (dst_reg < ZK_MAX_VREGS) {
                    shadow_vregs[dst_reg] = dst_val;
                    vreg_defined[dst_reg] = true;
                }
                break;
            }
            case OP_NOP: {
                break;
            }
            case OP_BR: {
                bb_elem_t taken = get_cell(ctx, s, COL_FLAG_BRANCH_TAKEN);
                if (taken != 1) return false;
                break;
            }
            case OP_CONDBR: {
#ifndef ZCC_MUT_DISABLE_BRANCH_CHECK
                bb_elem_t taken = get_cell(ctx, s, COL_FLAG_BRANCH_TAKEN);
                bool condition_true = (src0_val != 0);
                if ((condition_true && taken != 1) || (!condition_true && taken != 0)) {
                    return false; /* Branch predicate mismatch */
                }
#endif
                break;
            }
            case OP_RET: {
                final_output_val = src0_val;
                has_output = true;
                break;
            }
            default:
#ifdef ZCC_MUT_FAIL_OPEN_OPCODE
                if (dst_reg < ZK_MAX_VREGS) {
                    shadow_vregs[dst_reg] = dst_val;
                    vreg_defined[dst_reg] = true;
                }
                break; /* MUTATED: fail-open */
#else
                /* FAIL-CLOSED POLICY: Any unsupported executable opcode is strictly REJECTED */
                return false;
#endif
        }

        /* If this is the last step and no explicit OP_RET was seen, record dst_val */
        if (s == ctx->n_steps - 1 && !has_output) {
            final_output_val = dst_val;
            has_output = true;
        }
    }

    /* 4. Final Output Boundary Check */
#ifndef ZCC_MUT_DISABLE_OUTPUT_BINDING
    if (vctx && vctx->require_output_bound) {
        if (!has_output || final_output_val != vctx->expected_output) {
            return false; /* Final output claim mismatch */
        }
    }
#endif

    return true;
}
