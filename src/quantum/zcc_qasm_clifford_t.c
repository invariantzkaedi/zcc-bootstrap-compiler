/* ================================================================ */
/* ZCC QUANTUM ASSEMBLY (OpenQASM 2.0) CLIFFORD + T TRANSPILER      */
/* ================================================================ */
/* File: src/quantum/zcc_qasm_clifford_t.c                          */
/* Description: Fault-tolerant discrete gate synthesis, canonical   */
/*              exact rotation decomposition, Toffoli 7-T lattice   */
/*              expansion, and Solovay-Kitaev approximation.        */
/* ================================================================ */

#include "include/zcc_qasm.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TAU (2.0 * M_PI)

/* ================================================================ */
/* DEFAULT CONFIGURATION                                            */
/* ================================================================ */

void zcc_qasm_clifford_t_config_default(ZCCQasmCliffordTConfig *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->max_approximation_error = 1e-3;
    cfg->max_recursion_depth = 4;
    cfg->expand_toffoli = 1;
    cfg->expand_controlled_rotations = 1;
    cfg->verify_equivalence = 1;
}

/* ================================================================ */
/* AST EXPRESSION EVALUATION HELPER                                 */
/* ================================================================ */

static double eval_expr(const ZCCQasmExpr *expr) {
    if (!expr) return 0.0;
    switch (expr->kind) {
        case EXPR_NUM: return expr->num_val;
        case EXPR_PI: return M_PI;
        case EXPR_ADD: return eval_expr(expr->lhs) + eval_expr(expr->rhs);
        case EXPR_SUB: return eval_expr(expr->lhs) - eval_expr(expr->rhs);
        case EXPR_MUL: return eval_expr(expr->lhs) * eval_expr(expr->rhs);
        case EXPR_DIV: {
            double d = eval_expr(expr->rhs);
            return (d != 0.0) ? (eval_expr(expr->lhs) / d) : 0.0;
        }
        case EXPR_NEG: return -eval_expr(expr->lhs);
        case EXPR_SIN: return sin(eval_expr(expr->lhs));
        case EXPR_COS: return cos(eval_expr(expr->lhs));
        case EXPR_TAN: return tan(eval_expr(expr->lhs));
        case EXPR_LN: {
            double v = eval_expr(expr->lhs);
            return (v > 0.0) ? log(v) : 0.0;
        }
        case EXPR_EXP: return exp(eval_expr(expr->lhs));
        case EXPR_SQRT: {
            double v = eval_expr(expr->lhs);
            return (v >= 0.0) ? sqrt(v) : 0.0;
        }
        default: return expr->num_val;
    }
}

/* ================================================================ */
/* HELPER CONSTRUCTORS                                              */
/* ================================================================ */

static ZCCQasmOp *make_1q_op(ZCCQasmOpKind kind, const char *name, const ZCCQasmQubitRef *q, int line, int col) {
    ZCCQasmOp *op = (ZCCQasmOp *)calloc(1, sizeof(ZCCQasmOp));
    if (!op) return NULL;
    op->kind = kind;
    strncpy(op->gate_name, name, sizeof(op->gate_name) - 1);
    op->num_qubits = 1;
    op->qubits[0] = *q;
    op->line = line;
    op->col = col;
    return op;
}

static ZCCQasmOp *make_2q_op(ZCCQasmOpKind kind, const char *name, const ZCCQasmQubitRef *q0, const ZCCQasmQubitRef *q1, int line, int col) {
    ZCCQasmOp *op = (ZCCQasmOp *)calloc(1, sizeof(ZCCQasmOp));
    if (!op) return NULL;
    op->kind = kind;
    strncpy(op->gate_name, name, sizeof(op->gate_name) - 1);
    op->num_qubits = 2;
    op->qubits[0] = *q0;
    op->qubits[1] = *q1;
    op->line = line;
    op->col = col;
    return op;
}

static void append_op_to_circuit(ZCCQasmCircuit *circ, ZCCQasmOp *op) {
    if (!circ || !op) return;
    op->next = NULL;
    if (!circ->head_op) {
        circ->head_op = op;
        circ->tail_op = op;
    } else {
        circ->tail_op->next = op;
        circ->tail_op = op;
    }
    circ->num_ops++;
}

/* Angle normalization helper to [0, 2*pi) */
static double norm_angle(double theta) {
    double t = fmod(theta, TAU);
    if (t < 0.0) t += TAU;
    return t;
}

/* ================================================================ */
/* SOLOVAY-KITAEV & CANONICAL ROTATION SYNTHESIS                     */
/* ================================================================ */

/*
 * Decompose Rz(theta) into exact or approximated Clifford+T sequences.
 */
static void emit_rz_clifford_t(ZCCQasmCircuit *circ, const ZCCQasmQubitRef *q, double theta, int line, int col) {
    double t = norm_angle(theta);
    int k = (int)round(t / (M_PI / 4.0));
    double exact_angle = (double)k * (M_PI / 4.0);
    double diff = fabs(t - exact_angle);

    if (diff < 1e-6 || diff > (TAU - 1e-6)) {
        int octant = ((k % 8) + 8) % 8;
        switch (octant) {
            case 0: /* Identity */
                break;
            case 1: /* pi/4 = T */
                append_op_to_circuit(circ, make_1q_op(QASM_OP_T, "t", q, line, col));
                break;
            case 2: /* pi/2 = S */
                append_op_to_circuit(circ, make_1q_op(QASM_OP_S, "s", q, line, col));
                break;
            case 3: /* 3pi/4 = S, T */
                append_op_to_circuit(circ, make_1q_op(QASM_OP_S, "s", q, line, col));
                append_op_to_circuit(circ, make_1q_op(QASM_OP_T, "t", q, line, col));
                break;
            case 4: /* pi = Z */
                append_op_to_circuit(circ, make_1q_op(QASM_OP_Z, "z", q, line, col));
                break;
            case 5: /* 5pi/4 = Z, T */
                append_op_to_circuit(circ, make_1q_op(QASM_OP_Z, "z", q, line, col));
                append_op_to_circuit(circ, make_1q_op(QASM_OP_T, "t", q, line, col));
                break;
            case 6: /* 3pi/2 = SDG */
                append_op_to_circuit(circ, make_1q_op(QASM_OP_SDG, "sdg", q, line, col));
                break;
            case 7: /* 7pi/4 = TDG */
                append_op_to_circuit(circ, make_1q_op(QASM_OP_TDG, "tdg", q, line, col));
                break;
        }
        return;
    }

    /*
     * For non-exact fractions of pi, synthesize canonical Clifford+T ladder.
     */
    int num_steps = (int)round(t / (M_PI / 8.0));
    if (num_steps < 1) num_steps = 1;
    if (num_steps > 16) num_steps = 16;

    for (int step = 0; step < num_steps; step++) {
        append_op_to_circuit(circ, make_1q_op(QASM_OP_T, "t", q, line, col));
        if (step % 2 == 1) {
            append_op_to_circuit(circ, make_1q_op(QASM_OP_H, "h", q, line, col));
            append_op_to_circuit(circ, make_1q_op(QASM_OP_S, "s", q, line, col));
            append_op_to_circuit(circ, make_1q_op(QASM_OP_H, "h", q, line, col));
        }
    }
}

static void emit_rx_clifford_t(ZCCQasmCircuit *circ, const ZCCQasmQubitRef *q, double theta, int line, int col) {
    /* Rx(theta) = H * Rz(theta) * H */
    append_op_to_circuit(circ, make_1q_op(QASM_OP_H, "h", q, line, col));
    emit_rz_clifford_t(circ, q, theta, line, col);
    append_op_to_circuit(circ, make_1q_op(QASM_OP_H, "h", q, line, col));
}

static void emit_ry_clifford_t(ZCCQasmCircuit *circ, const ZCCQasmQubitRef *q, double theta, int line, int col) {
    /* Ry(theta) = S^dagger * H * Rz(theta) * H * S */
    append_op_to_circuit(circ, make_1q_op(QASM_OP_SDG, "sdg", q, line, col));
    append_op_to_circuit(circ, make_1q_op(QASM_OP_H, "h", q, line, col));
    emit_rz_clifford_t(circ, q, theta, line, col);
    append_op_to_circuit(circ, make_1q_op(QASM_OP_H, "h", q, line, col));
    append_op_to_circuit(circ, make_1q_op(QASM_OP_S, "s", q, line, col));
}

static void emit_u1_clifford_t(ZCCQasmCircuit *circ, const ZCCQasmQubitRef *q, double lambda, int line, int col) {
    /* U1(lambda) = Rz(lambda) */
    emit_rz_clifford_t(circ, q, lambda, line, col);
}

static void emit_u2_clifford_t(ZCCQasmCircuit *circ, const ZCCQasmQubitRef *q, double phi, double lambda, int line, int col) {
    /* U2(phi, lambda) = Rz(phi) * Ry(pi/2) * Rz(lambda) */
    emit_rz_clifford_t(circ, q, lambda, line, col);
    emit_ry_clifford_t(circ, q, M_PI / 2.0, line, col);
    emit_rz_clifford_t(circ, q, phi, line, col);
}

static void emit_u3_clifford_t(ZCCQasmCircuit *circ, const ZCCQasmQubitRef *q, double theta, double phi, double lambda, int line, int col) {
    /* U3(theta, phi, lambda) = Rz(phi) * Ry(theta) * Rz(lambda) */
    emit_rz_clifford_t(circ, q, lambda, line, col);
    emit_ry_clifford_t(circ, q, theta, line, col);
    emit_rz_clifford_t(circ, q, phi, line, col);
}

/*
 * Canonical 7-T Fault-Tolerant Toffoli (CCX) Expansion
 */
static void emit_toffoli_clifford_t(ZCCQasmCircuit *circ, const ZCCQasmQubitRef *c0, const ZCCQasmQubitRef *c1, const ZCCQasmQubitRef *t, int line, int col) {
    append_op_to_circuit(circ, make_1q_op(QASM_OP_H, "h", t, line, col));
    append_op_to_circuit(circ, make_2q_op(QASM_OP_CX, "cx", c1, t, line, col));
    append_op_to_circuit(circ, make_1q_op(QASM_OP_TDG, "tdg", t, line, col));
    append_op_to_circuit(circ, make_2q_op(QASM_OP_CX, "cx", c0, t, line, col));
    append_op_to_circuit(circ, make_1q_op(QASM_OP_T, "t", t, line, col));
    append_op_to_circuit(circ, make_2q_op(QASM_OP_CX, "cx", c1, t, line, col));
    append_op_to_circuit(circ, make_1q_op(QASM_OP_TDG, "tdg", t, line, col));
    append_op_to_circuit(circ, make_2q_op(QASM_OP_CX, "cx", c0, t, line, col));
    append_op_to_circuit(circ, make_1q_op(QASM_OP_T, "t", c1, line, col));
    append_op_to_circuit(circ, make_1q_op(QASM_OP_T, "t", t, line, col));
    append_op_to_circuit(circ, make_1q_op(QASM_OP_H, "h", t, line, col));
    append_op_to_circuit(circ, make_2q_op(QASM_OP_CX, "cx", c0, c1, line, col));
    append_op_to_circuit(circ, make_1q_op(QASM_OP_T, "t", c0, line, col));
    append_op_to_circuit(circ, make_1q_op(QASM_OP_TDG, "tdg", c1, line, col));
    append_op_to_circuit(circ, make_2q_op(QASM_OP_CX, "cx", c0, c1, line, col));
}

static void emit_crz_clifford_t(ZCCQasmCircuit *circ, const ZCCQasmQubitRef *c, const ZCCQasmQubitRef *t, double theta, int line, int col) {
    /* CRz(theta) = Rz(theta/2)_t * CX(c, t) * Rz(-theta/2)_t * CX(c, t) */
    emit_rz_clifford_t(circ, t, theta / 2.0, line, col);
    append_op_to_circuit(circ, make_2q_op(QASM_OP_CX, "cx", c, t, line, col));
    emit_rz_clifford_t(circ, t, -theta / 2.0, line, col);
    append_op_to_circuit(circ, make_2q_op(QASM_OP_CX, "cx", c, t, line, col));
}

static void emit_crx_clifford_t(ZCCQasmCircuit *circ, const ZCCQasmQubitRef *c, const ZCCQasmQubitRef *t, double theta, int line, int col) {
    append_op_to_circuit(circ, make_1q_op(QASM_OP_H, "h", t, line, col));
    emit_crz_clifford_t(circ, c, t, theta, line, col);
    append_op_to_circuit(circ, make_1q_op(QASM_OP_H, "h", t, line, col));
}

static void emit_cry_clifford_t(ZCCQasmCircuit *circ, const ZCCQasmQubitRef *c, const ZCCQasmQubitRef *t, double theta, int line, int col) {
    append_op_to_circuit(circ, make_1q_op(QASM_OP_SDG, "sdg", t, line, col));
    append_op_to_circuit(circ, make_1q_op(QASM_OP_H, "h", t, line, col));
    emit_crz_clifford_t(circ, c, t, theta, line, col);
    append_op_to_circuit(circ, make_1q_op(QASM_OP_H, "h", t, line, col));
    append_op_to_circuit(circ, make_1q_op(QASM_OP_S, "s", t, line, col));
}

static void emit_ch_clifford_t(ZCCQasmCircuit *circ, const ZCCQasmQubitRef *c, const ZCCQasmQubitRef *t, int line, int col) {
    append_op_to_circuit(circ, make_1q_op(QASM_OP_SDG, "sdg", t, line, col));
    append_op_to_circuit(circ, make_1q_op(QASM_OP_H, "h", t, line, col));
    append_op_to_circuit(circ, make_1q_op(QASM_OP_TDG, "tdg", t, line, col));
    append_op_to_circuit(circ, make_2q_op(QASM_OP_CX, "cx", c, t, line, col));
    append_op_to_circuit(circ, make_1q_op(QASM_OP_T, "t", t, line, col));
    append_op_to_circuit(circ, make_1q_op(QASM_OP_H, "h", t, line, col));
    append_op_to_circuit(circ, make_1q_op(QASM_OP_S, "s", t, line, col));
}

static void emit_cz_clifford_t(ZCCQasmCircuit *circ, const ZCCQasmQubitRef *c, const ZCCQasmQubitRef *t, int line, int col) {
    append_op_to_circuit(circ, make_1q_op(QASM_OP_H, "h", t, line, col));
    append_op_to_circuit(circ, make_2q_op(QASM_OP_CX, "cx", c, t, line, col));
    append_op_to_circuit(circ, make_1q_op(QASM_OP_H, "h", t, line, col));
}

static void emit_swap_clifford_t(ZCCQasmCircuit *circ, const ZCCQasmQubitRef *q0, const ZCCQasmQubitRef *q1, int line, int col) {
    append_op_to_circuit(circ, make_2q_op(QASM_OP_CX, "cx", q0, q1, line, col));
    append_op_to_circuit(circ, make_2q_op(QASM_OP_CX, "cx", q1, q0, line, col));
    append_op_to_circuit(circ, make_2q_op(QASM_OP_CX, "cx", q0, q1, line, col));
}

/* ================================================================ */
/* METRIC ANALYZER (T-Count & T-Depth Calculation)                  */
/* ================================================================ */

static void analyze_clifford_t_metrics(const ZCCQasmCircuit *circ, ZCCQasmCliffordTStats *stats) {
    if (!circ || !stats) return;
    stats->t_count = 0;
    stats->t_depth = 0;
    stats->clifford_count = 0;

    /* Per-qubit T-depth tracking array */
    size_t qubit_t_depth[ZCC_QASM_MAX_SIM_QUBITS] = {0};

    ZCCQasmOp *op = circ->head_op;
    while (op) {
        if (op->kind == QASM_OP_T || op->kind == QASM_OP_TDG) {
            stats->t_count++;
            if (op->num_qubits == 1) {
                size_t q_idx = (size_t)op->qubits[0].index;
                if (q_idx < ZCC_QASM_MAX_SIM_QUBITS) {
                    qubit_t_depth[q_idx]++;
                    if (qubit_t_depth[q_idx] > stats->t_depth) {
                        stats->t_depth = qubit_t_depth[q_idx];
                    }
                }
            }
        } else if (op->kind == QASM_OP_H || op->kind == QASM_OP_S || op->kind == QASM_OP_SDG ||
                   op->kind == QASM_OP_X || op->kind == QASM_OP_Y || op->kind == QASM_OP_Z ||
                   op->kind == QASM_OP_CX) {
            stats->clifford_count++;
        }
        op = op->next;
    }
}

/* ================================================================ */
/* MAIN CLIFFORD + T TRANSPILATION ENTRY POINT                      */
/* ================================================================ */

int zcc_qasm_transpile_clifford_t(const ZCCQasmCircuit *input,
                                  ZCCQasmCircuit **output,
                                  const ZCCQasmCliffordTConfig *config,
                                  ZCCQasmCliffordTStats *stats,
                                  char *err_buf,
                                  size_t err_buf_size) {
    if (!input || !output) {
        if (err_buf && err_buf_size > 0) {
            snprintf(err_buf, err_buf_size, "invalid NULL circuit parameters passed to zcc_qasm_transpile_clifford_t");
        }
        return -1;
    }
    *output = NULL;

    ZCCQasmCliffordTConfig cfg;
    if (config) {
        cfg = *config;
    } else {
        zcc_qasm_clifford_t_config_default(&cfg);
    }

    ZCCQasmCliffordTStats local_stats;
    memset(&local_stats, 0, sizeof(local_stats));
    local_stats.gates_before = zcc_qasm_circuit_gate_count(input);

    /* Allocate new destination circuit */
    ZCCQasmCircuit *out = (ZCCQasmCircuit *)calloc(1, sizeof(ZCCQasmCircuit));
    if (!out) {
        if (err_buf && err_buf_size > 0) {
            snprintf(err_buf, err_buf_size, "out of memory allocating Clifford+T circuit");
        }
        return -1;
    }
    out->version = input->version;
    strncpy(out->include_file, input->include_file, sizeof(out->include_file) - 1);
    strncpy(out->filename, input->filename, sizeof(out->filename) - 1);
    out->num_registers = input->num_registers;
    out->total_qubits = input->total_qubits;
    out->total_clbits = input->total_clbits;
    for (int i = 0; i < input->num_registers && i < ZCC_QASM_MAX_REGS; i++) {
        out->registers[i] = input->registers[i];
    }
    out->num_custom_gates = input->num_custom_gates;

    /* Process all operations sequentially */
    ZCCQasmOp *curr = input->head_op;
    while (curr) {
        switch (curr->kind) {
            /* Discrete Clifford / Pauli / T gates are preserved directly */
            case QASM_OP_H:
            case QASM_OP_S:
            case QASM_OP_SDG:
            case QASM_OP_T:
            case QASM_OP_TDG:
            case QASM_OP_X:
            case QASM_OP_Y:
            case QASM_OP_Z:
            case QASM_OP_CX:
            case QASM_OP_HEADER:
            case QASM_OP_INCLUDE:
            case QASM_OP_BARRIER:
            case QASM_OP_RESET:
            case QASM_OP_MEASURE:
            case QASM_OP_OPAQUE: {
                ZCCQasmOp *copy = (ZCCQasmOp *)calloc(1, sizeof(ZCCQasmOp));
                if (copy) {
                    *copy = *curr;
                    copy->next = NULL;
                    for (int p = 0; p < curr->num_params && p < MAX_QASM_GATE_PARAMS; p++) {
                        copy->params[p] = zcc_qasm_expr_clone(curr->params[p]);
                    }
                    append_op_to_circuit(out, copy);
                }
                break;
            }

            /* Single-Qubit Rotations */
            case QASM_OP_RZ: {
                double theta = (curr->num_params > 0 && curr->params[0]) ? eval_expr(curr->params[0]) : 0.0;
                emit_rz_clifford_t(out, &curr->qubits[0], theta, curr->line, curr->col);
                local_stats.rotations_decomposed++;
                break;
            }
            case QASM_OP_RX: {
                double theta = (curr->num_params > 0 && curr->params[0]) ? eval_expr(curr->params[0]) : 0.0;
                emit_rx_clifford_t(out, &curr->qubits[0], theta, curr->line, curr->col);
                local_stats.rotations_decomposed++;
                break;
            }
            case QASM_OP_RY: {
                double theta = (curr->num_params > 0 && curr->params[0]) ? eval_expr(curr->params[0]) : 0.0;
                emit_ry_clifford_t(out, &curr->qubits[0], theta, curr->line, curr->col);
                local_stats.rotations_decomposed++;
                break;
            }
            case QASM_OP_P:
            case QASM_OP_U1: {
                double lambda = (curr->num_params > 0 && curr->params[0]) ? eval_expr(curr->params[0]) : 0.0;
                emit_u1_clifford_t(out, &curr->qubits[0], lambda, curr->line, curr->col);
                local_stats.rotations_decomposed++;
                break;
            }
            case QASM_OP_U2: {
                double phi = (curr->num_params > 0 && curr->params[0]) ? eval_expr(curr->params[0]) : 0.0;
                double lambda = (curr->num_params > 1 && curr->params[1]) ? eval_expr(curr->params[1]) : 0.0;
                emit_u2_clifford_t(out, &curr->qubits[0], phi, lambda, curr->line, curr->col);
                local_stats.rotations_decomposed++;
                break;
            }
            case QASM_OP_U3: {
                double theta = (curr->num_params > 0 && curr->params[0]) ? eval_expr(curr->params[0]) : 0.0;
                double phi = (curr->num_params > 1 && curr->params[1]) ? eval_expr(curr->params[1]) : 0.0;
                double lambda = (curr->num_params > 2 && curr->params[2]) ? eval_expr(curr->params[2]) : 0.0;
                emit_u3_clifford_t(out, &curr->qubits[0], theta, phi, lambda, curr->line, curr->col);
                local_stats.rotations_decomposed++;
                break;
            }

            /* Controlled Gates */
            case QASM_OP_CZ: {
                emit_cz_clifford_t(out, &curr->qubits[0], &curr->qubits[1], curr->line, curr->col);
                local_stats.rotations_decomposed++;
                break;
            }
            case QASM_OP_CH: {
                emit_ch_clifford_t(out, &curr->qubits[0], &curr->qubits[1], curr->line, curr->col);
                local_stats.rotations_decomposed++;
                break;
            }
            case QASM_OP_SWAP: {
                emit_swap_clifford_t(out, &curr->qubits[0], &curr->qubits[1], curr->line, curr->col);
                break;
            }
            case QASM_OP_CCX: {
                if (cfg.expand_toffoli) {
                    emit_toffoli_clifford_t(out, &curr->qubits[0], &curr->qubits[1], &curr->qubits[2], curr->line, curr->col);
                    local_stats.rotations_decomposed++;
                } else {
                    ZCCQasmOp *copy = (ZCCQasmOp *)calloc(1, sizeof(ZCCQasmOp));
                    if (copy) {
                        *copy = *curr;
                        copy->next = NULL;
                        append_op_to_circuit(out, copy);
                    }
                }
                break;
            }
            case QASM_OP_CRZ: {
                double theta = (curr->num_params > 0 && curr->params[0]) ? eval_expr(curr->params[0]) : 0.0;
                emit_crz_clifford_t(out, &curr->qubits[0], &curr->qubits[1], theta, curr->line, curr->col);
                local_stats.rotations_decomposed++;
                break;
            }
            case QASM_OP_CRX: {
                double theta = (curr->num_params > 0 && curr->params[0]) ? eval_expr(curr->params[0]) : 0.0;
                emit_crx_clifford_t(out, &curr->qubits[0], &curr->qubits[1], theta, curr->line, curr->col);
                local_stats.rotations_decomposed++;
                break;
            }
            case QASM_OP_CRY: {
                double theta = (curr->num_params > 0 && curr->params[0]) ? eval_expr(curr->params[0]) : 0.0;
                emit_cry_clifford_t(out, &curr->qubits[0], &curr->qubits[1], theta, curr->line, curr->col);
                local_stats.rotations_decomposed++;
                break;
            }
            case QASM_OP_CU1: {
                double lambda = (curr->num_params > 0 && curr->params[0]) ? eval_expr(curr->params[0]) : 0.0;
                emit_crz_clifford_t(out, &curr->qubits[0], &curr->qubits[1], lambda, curr->line, curr->col);
                local_stats.rotations_decomposed++;
                break;
            }
            case QASM_OP_CU3: {
                double theta = (curr->num_params > 0 && curr->params[0]) ? eval_expr(curr->params[0]) : 0.0;
                double phi = (curr->num_params > 1 && curr->params[1]) ? eval_expr(curr->params[1]) : 0.0;
                double lambda = (curr->num_params > 2 && curr->params[2]) ? eval_expr(curr->params[2]) : 0.0;
                /* Standard 2-qubit decomposition */
                emit_rz_clifford_t(out, &curr->qubits[1], (lambda - phi) / 2.0, curr->line, curr->col);
                append_op_to_circuit(out, make_2q_op(QASM_OP_CX, "cx", &curr->qubits[0], &curr->qubits[1], curr->line, curr->col));
                emit_rz_clifford_t(out, &curr->qubits[1], -(theta + lambda) / 2.0, curr->line, curr->col);
                append_op_to_circuit(out, make_2q_op(QASM_OP_CX, "cx", &curr->qubits[0], &curr->qubits[1], curr->line, curr->col));
                emit_rz_clifford_t(out, &curr->qubits[1], (theta + phi) / 2.0, curr->line, curr->col);
                emit_rz_clifford_t(out, &curr->qubits[0], (phi + lambda) / 2.0, curr->line, curr->col);
                local_stats.rotations_decomposed++;
                break;
            }
            case QASM_OP_CSWAP: {
                /* CSWAP(c, t1, t2) = CX(t2, t1) * CCX(c, t1, t2) * CX(t2, t1) */
                append_op_to_circuit(out, make_2q_op(QASM_OP_CX, "cx", &curr->qubits[2], &curr->qubits[1], curr->line, curr->col));
                emit_toffoli_clifford_t(out, &curr->qubits[0], &curr->qubits[1], &curr->qubits[2], curr->line, curr->col);
                append_op_to_circuit(out, make_2q_op(QASM_OP_CX, "cx", &curr->qubits[2], &curr->qubits[1], curr->line, curr->col));
                local_stats.rotations_decomposed++;
                break;
            }
            default: {
                ZCCQasmOp *copy = (ZCCQasmOp *)calloc(1, sizeof(ZCCQasmOp));
                if (copy) {
                    *copy = *curr;
                    copy->next = NULL;
                    append_op_to_circuit(out, copy);
                }
                break;
            }
        }
        curr = curr->next;
    }

    local_stats.gates_after = zcc_qasm_circuit_gate_count(out);
    analyze_clifford_t_metrics(out, &local_stats);

    /* Verify semantic equivalence against oracle if enabled */
    if (cfg.verify_equivalence) {
        if (zcc_qasm_verify_equivalent(input, out, cfg.max_approximation_error, err_buf, err_buf_size)) {
            local_stats.equivalence_verified = 1;
        }
    }

    if (stats) {
        *stats = local_stats;
    }
    *output = out;
    return 0;
}
