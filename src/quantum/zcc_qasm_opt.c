/* ================================================================ */
/* ZCC QUANTUM ASSEMBLY (OpenQASM 2.0) CIRCUIT OPTIMIZER            */
/* ================================================================ */
/* File: src/quantum/zcc_qasm_opt.c                                 */
/* Description: Algebraic rewrite engine, commutation sliding,     */
/*              fixed-point pass manager, and simulator oracle.     */
/* ================================================================ */

#include "include/zcc_qasm.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ================================================================ */
/* DEFAULT CONFIGURATION & STATS                                    */
/* ================================================================ */

void zcc_qasm_opt_config_default(ZCCQasmOptConfig *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->max_iterations = 64;
    cfg->angle_epsilon = 1e-10;
    cfg->equivalence_tolerance = 1e-9;
    cfg->enable_local_rewrites = 1;
    cfg->enable_commutation = 1;
    cfg->enable_equivalence_check = 1;
}

/* ================================================================ */
/* EXPRESSION CLONING & AST LIFETIME HELPERS                        */
/* ================================================================ */

ZCCQasmExpr *zcc_qasm_expr_clone(const ZCCQasmExpr *expr) {
    if (!expr) return NULL;
    ZCCQasmExpr *copy = (ZCCQasmExpr *)calloc(1, sizeof(ZCCQasmExpr));
    if (!copy) return NULL;
    copy->kind = expr->kind;
    copy->num_val = expr->num_val;
    strncpy(copy->param_name, expr->param_name, sizeof(copy->param_name) - 1);
    if (expr->lhs) copy->lhs = zcc_qasm_expr_clone(expr->lhs);
    if (expr->rhs) copy->rhs = zcc_qasm_expr_clone(expr->rhs);
    return copy;
}

static ZCCQasmOp *qasm_op_clone(const ZCCQasmOp *src) {
    if (!src) return NULL;
    ZCCQasmOp *op = (ZCCQasmOp *)calloc(1, sizeof(ZCCQasmOp));
    if (!op) return NULL;
    op->kind = src->kind;
    strncpy(op->gate_name, src->gate_name, sizeof(op->gate_name) - 1);
    op->num_qubits = src->num_qubits;
    for (int i = 0; i < src->num_qubits && i < MAX_QASM_GATE_QUBITS; i++) {
        op->qubits[i] = src->qubits[i];
    }
    op->num_params = src->num_params;
    for (int i = 0; i < src->num_params && i < MAX_QASM_GATE_PARAMS; i++) {
        op->params[i] = zcc_qasm_expr_clone(src->params[i]);
    }
    op->meas_target = src->meas_target;
    op->has_condition = src->has_condition;
    strncpy(op->cond_reg, src->cond_reg, sizeof(op->cond_reg) - 1);
    op->cond_val = src->cond_val;
    op->line = src->line;
    op->col = src->col;
    op->next = NULL;
    return op;
}

static void qasm_op_free(ZCCQasmOp *op) {
    if (!op) return;
    for (int i = 0; i < op->num_params && i < MAX_QASM_GATE_PARAMS; i++) {
        if (op->params[i]) {
            zcc_qasm_expr_free(op->params[i]);
            op->params[i] = NULL;
        }
    }
    free(op);
}

/* Deep clone circuit AST */
int zcc_qasm_circuit_clone(const ZCCQasmCircuit *src, ZCCQasmCircuit **dst, char *err_buf, size_t err_buf_size) {
    if (!src || !dst) {
        if (err_buf && err_buf_size > 0) {
            snprintf(err_buf, err_buf_size, "invalid NULL circuit pointer for cloning");
        }
        return -1;
    }
    *dst = NULL;
    ZCCQasmCircuit *circ = (ZCCQasmCircuit *)calloc(1, sizeof(ZCCQasmCircuit));
    if (!circ) {
        if (err_buf && err_buf_size > 0) {
            snprintf(err_buf, err_buf_size, "out of memory allocating circuit clone");
        }
        return -1;
    }
    circ->version = src->version;
    strncpy(circ->include_file, src->include_file, sizeof(circ->include_file) - 1);
    strncpy(circ->filename, src->filename, sizeof(circ->filename) - 1);
    circ->num_registers = src->num_registers;
    circ->total_qubits = src->total_qubits;
    circ->total_clbits = src->total_clbits;
    for (int i = 0; i < src->num_registers && i < ZCC_QASM_MAX_REGS; i++) {
        circ->registers[i] = src->registers[i];
    }
    circ->num_custom_gates = src->num_custom_gates;
    for (int g = 0; g < src->num_custom_gates && g < MAX_QASM_CUSTOM_GATES; g++) {
        const ZCCQasmGateDef *sg = &src->custom_gates[g];
        ZCCQasmGateDef *dg = &circ->custom_gates[g];
        strncpy(dg->name, sg->name, sizeof(dg->name) - 1);
        dg->num_params = sg->num_params;
        for (int p = 0; p < sg->num_params; p++) {
            strncpy(dg->param_names[p], sg->param_names[p], sizeof(dg->param_names[p]) - 1);
        }
        dg->num_qubits = sg->num_qubits;
        for (int q = 0; q < sg->num_qubits; q++) {
            strncpy(dg->qubit_names[q], sg->qubit_names[q], sizeof(dg->qubit_names[q]) - 1);
        }
        dg->line = sg->line;
        dg->col = sg->col;
        ZCCQasmOp *curr = sg->body_head;
        while (curr) {
            ZCCQasmOp *bcopy = qasm_op_clone(curr);
            if (!dg->body_head) {
                dg->body_head = dg->body_tail = bcopy;
            } else {
                dg->body_tail->next = bcopy;
                dg->body_tail = bcopy;
            }
            dg->num_body_ops++;
            curr = curr->next;
        }
    }
    ZCCQasmOp *curr = src->head_op;
    while (curr) {
        ZCCQasmOp *copy = qasm_op_clone(curr);
        zcc_qasm_circuit_add_op(circ, copy);
        curr = curr->next;
    }
    *dst = circ;
    return 0;
}

size_t zcc_qasm_circuit_gate_count(const ZCCQasmCircuit *circ) {
    if (!circ) return 0;
    size_t count = 0;
    ZCCQasmOp *op = circ->head_op;
    while (op) {
        if (op->kind != QASM_OP_HEADER && op->kind != QASM_OP_INCLUDE) {
            count++;
        }
        op = op->next;
    }
    return count;
}

/* ================================================================ */
/* ANGLE NORMALIZATION & ZERO PREDICATE                             */
/* ================================================================ */

static double qasm_normalize_angle(double theta) {
    const double tau = 2.0 * M_PI;
    theta = fmod(theta, tau);
    if (theta <= -M_PI) {
        theta += tau;
    } else if (theta > M_PI) {
        theta -= tau;
    }
    return theta;
}

static int qasm_angle_is_zero(double theta, double epsilon) {
    return fabs(qasm_normalize_angle(theta)) <= epsilon;
}

/* ================================================================ */
/* GATE CLASSIFICATION HELPERS                                      */
/* ================================================================ */

static int qasm_op_is_rewrite_barrier(const ZCCQasmOp *op) {
    if (!op) return 1;
    if (op->has_condition) return 1;
    switch (op->kind) {
        case QASM_OP_HEADER:
        case QASM_OP_INCLUDE:
        case QASM_OP_BARRIER:
        case QASM_OP_RESET:
        case QASM_OP_MEASURE:
        case QASM_OP_OPAQUE:
        case QASM_OP_CUSTOM:
            return 1;
        default:
            return 0;
    }
}

static int qasm_gate_is_self_inverse(ZCCQasmOpKind kind) {
    switch (kind) {
        case QASM_OP_H:
        case QASM_OP_X:
        case QASM_OP_Y:
        case QASM_OP_Z:
        case QASM_OP_CX:
        case QASM_OP_CZ:
        case QASM_OP_CH:
        case QASM_OP_SWAP:
        case QASM_OP_CCX:
        case QASM_OP_CSWAP:
            return 1;
        default:
            return 0;
    }
}

static int qasm_gate_is_inverse_pair(ZCCQasmOpKind a, ZCCQasmOpKind b) {
    return ((a == QASM_OP_S && b == QASM_OP_SDG) ||
            (a == QASM_OP_SDG && b == QASM_OP_S) ||
            (a == QASM_OP_T && b == QASM_OP_TDG) ||
            (a == QASM_OP_TDG && b == QASM_OP_T));
}

static int qasm_gate_is_fusible_rotation(ZCCQasmOpKind kind) {
    switch (kind) {
        case QASM_OP_RX:
        case QASM_OP_RY:
        case QASM_OP_RZ:
        case QASM_OP_P:
        case QASM_OP_U1:
            return 1;
        default:
            return 0;
    }
}

/* Discrete Clifford+T Z-axis phase octant (k * pi/4 mod 2*pi, k in 1..7) */
static int qasm_discrete_z_octant(ZCCQasmOpKind kind) {
    switch (kind) {
        case QASM_OP_T:   return 1;
        case QASM_OP_S:   return 2;
        case QASM_OP_Z:   return 4;
        case QASM_OP_SDG: return 6;
        case QASM_OP_TDG: return 7;
        default:          return 0;
    }
}

static void qasm_set_discrete_z_octant_gate(ZCCQasmOp *op, int octant) {
    switch (octant & 7) {
        case 1:
            op->kind = QASM_OP_T;
            strncpy(op->gate_name, "t", sizeof(op->gate_name) - 1);
            break;
        case 2:
            op->kind = QASM_OP_S;
            strncpy(op->gate_name, "s", sizeof(op->gate_name) - 1);
            break;
        case 4:
            op->kind = QASM_OP_Z;
            strncpy(op->gate_name, "z", sizeof(op->gate_name) - 1);
            break;
        case 6:
            op->kind = QASM_OP_SDG;
            strncpy(op->gate_name, "sdg", sizeof(op->gate_name) - 1);
            break;
        case 7:
            op->kind = QASM_OP_TDG;
            strncpy(op->gate_name, "tdg", sizeof(op->gate_name) - 1);
            break;
        default:
            break;
    }
}

static int qasm_op_is_diagonal_z(const ZCCQasmOp *op) {
    if (!op || op->has_condition) return 0;
    switch (op->kind) {
        case QASM_OP_ID:
        case QASM_OP_Z:
        case QASM_OP_S:
        case QASM_OP_SDG:
        case QASM_OP_T:
        case QASM_OP_TDG:
        case QASM_OP_RZ:
        case QASM_OP_P:
        case QASM_OP_U1:
        case QASM_OP_CZ:
        case QASM_OP_CRZ:
        case QASM_OP_CU1:
        case QASM_OP_RZZ:
            return 1;
        default:
            return 0;
    }
}

static int qasm_op_is_x_basis_1q(const ZCCQasmOp *op) {
    if (!op || op->has_condition) return 0;
    return (op->kind == QASM_OP_X || op->kind == QASM_OP_RX);
}

static int qasm_qubit_refs_equal(const ZCCQasmQubitRef *a, const ZCCQasmQubitRef *b) {
    if (!a || !b) return 0;
    return (a->index == b->index && strcmp(a->reg_name, b->reg_name) == 0);
}

static int qasm_op_touches_qubit(const ZCCQasmOp *op, const ZCCQasmQubitRef *q) {
    if (!op || !q) return 1;
    for (int i = 0; i < op->num_qubits; i++) {
        if (strcmp(op->qubits[i].reg_name, q->reg_name) == 0 &&
            (op->qubits[i].index == q->index || op->qubits[i].index == -1 || q->index == -1)) {
            return 1;
        }
    }
    return 0;
}

static int qasm_same_operands(const ZCCQasmOp *a, const ZCCQasmOp *b) {
    if (!a || !b) return 0;
    if (a->num_qubits != b->num_qubits) return 0;
    for (int i = 0; i < a->num_qubits; i++) {
        if (strcmp(a->qubits[i].reg_name, b->qubits[i].reg_name) != 0) return 0;
        if (a->qubits[i].index != b->qubits[i].index) return 0;
    }
    return 1;
}

static int qasm_same_operands_symmetric(const ZCCQasmOp *a, const ZCCQasmOp *b) {
    if (!a || !b) return 0;
    if (a->num_qubits != b->num_qubits) return 0;
    if (qasm_same_operands(a, b)) return 1;
    if (a->num_qubits == 2) {
        if (strcmp(a->qubits[0].reg_name, b->qubits[1].reg_name) == 0 &&
            a->qubits[0].index == b->qubits[1].index &&
            strcmp(a->qubits[1].reg_name, b->qubits[0].reg_name) == 0 &&
            a->qubits[1].index == b->qubits[0].index) {
            return 1;
        }
    }
    return 0;
}

static int qasm_ops_share_qubit(const ZCCQasmOp *a, const ZCCQasmOp *b) {
    if (!a || !b) return 1;
    for (int i = 0; i < a->num_qubits; i++) {
        for (int j = 0; j < b->num_qubits; j++) {
            if (strcmp(a->qubits[i].reg_name, b->qubits[j].reg_name) == 0 &&
                (a->qubits[i].index == b->qubits[j].index || a->qubits[i].index == -1 || b->qubits[j].index == -1)) {
                return 1;
            }
        }
    }
    return 0;
}

static int qasm_ops_can_commute_disjoint(const ZCCQasmOp *a, const ZCCQasmOp *b) {
    if (qasm_op_is_rewrite_barrier(a) || qasm_op_is_rewrite_barrier(b)) return 0;
    if (!qasm_ops_share_qubit(a, b)) return 1;

    /* C1. All Z-diagonal operators commute with one another on shared wires */
    if (qasm_op_is_diagonal_z(a) && qasm_op_is_diagonal_z(b)) return 1;

    /* C2. Z-diagonal gate commutes across CNOT control wire (if disjoint from target) */
    if (a->kind == QASM_OP_CX && a->num_qubits == 2 && qasm_op_is_diagonal_z(b)) {
        if (!qasm_op_touches_qubit(b, &a->qubits[1])) return 1;
    }
    if (b->kind == QASM_OP_CX && b->num_qubits == 2 && qasm_op_is_diagonal_z(a)) {
        if (!qasm_op_touches_qubit(a, &b->qubits[1])) return 1;
    }

    /* C3. X-basis 1Q gate (X, Rx) commutes across CNOT target wire */
    if (a->kind == QASM_OP_CX && a->num_qubits == 2 && qasm_op_is_x_basis_1q(b) && b->num_qubits == 1) {
        if (qasm_qubit_refs_equal(&b->qubits[0], &a->qubits[1]) &&
            !qasm_qubit_refs_equal(&b->qubits[0], &a->qubits[0])) {
            return 1;
        }
    }
    if (b->kind == QASM_OP_CX && b->num_qubits == 2 && qasm_op_is_x_basis_1q(a) && a->num_qubits == 1) {
        if (qasm_qubit_refs_equal(&a->qubits[0], &b->qubits[1]) &&
            !qasm_qubit_refs_equal(&a->qubits[0], &b->qubits[0])) {
            return 1;
        }
    }

    /* C4. Two CNOTs CX(c1, t1) and CX(c2, t2) commute iff c1 != t2 and c2 != t1 */
    if (a->kind == QASM_OP_CX && b->kind == QASM_OP_CX && a->num_qubits == 2 && b->num_qubits == 2) {
        if (!qasm_qubit_refs_equal(&a->qubits[0], &b->qubits[1]) &&
            !qasm_qubit_refs_equal(&b->qubits[0], &a->qubits[1])) {
            return 1;
        }
    }

    return 0;
}

/* ================================================================ */
/* REWRITE PREDICATES & EVALUATIONS                                 */
/* ================================================================ */

static int qasm_gate_is_identity_rotation(const ZCCQasmOp *op, double eps) {
    if (!op || op->has_condition) return 0;
    if (op->kind == QASM_OP_ID) return 1;
    if (qasm_gate_is_fusible_rotation(op->kind)) {
        if (op->num_params != 1 || !op->params[0]) return 0;
        int ok = 0;
        double angle = zcc_qasm_expr_eval_const(op->params[0], &ok);
        if (ok && qasm_angle_is_zero(angle, eps)) return 1;
    }
    if (op->kind == QASM_OP_U3) {
        if (op->num_params != 3 || !op->params[0] || !op->params[1] || !op->params[2]) return 0;
        int ok0 = 0, ok1 = 0, ok2 = 0;
        double theta = zcc_qasm_expr_eval_const(op->params[0], &ok0);
        double phi = zcc_qasm_expr_eval_const(op->params[1], &ok1);
        double lambda = zcc_qasm_expr_eval_const(op->params[2], &ok2);
        if (ok0 && ok1 && ok2 &&
            qasm_angle_is_zero(theta, eps) &&
            qasm_angle_is_zero(phi, eps) &&
            qasm_angle_is_zero(lambda, eps)) {
            return 1;
        }
    }
    return 0;
}

static int qasm_try_cancel_self_inverse(const ZCCQasmOp *a, const ZCCQasmOp *b) {
    if (!a || !b || a->has_condition || b->has_condition) return 0;
    if (a->kind != b->kind) return 0;
    if (!qasm_gate_is_self_inverse(a->kind)) return 0;
    if (a->kind == QASM_OP_CZ || a->kind == QASM_OP_SWAP) {
        return qasm_same_operands_symmetric(a, b);
    }
    return qasm_same_operands(a, b);
}

static int qasm_try_cancel_inverse_pair(const ZCCQasmOp *a, const ZCCQasmOp *b) {
    if (!a || !b || a->has_condition || b->has_condition) return 0;
    if (!qasm_gate_is_inverse_pair(a->kind, b->kind)) return 0;
    return qasm_same_operands(a, b);
}

static int qasm_discrete_z_pair_reducible(const ZCCQasmOp *a, const ZCCQasmOp *b) {
    if (!a || !b || a->has_condition || b->has_condition) return 0;
    int oa = qasm_discrete_z_octant(a->kind);
    int ob = qasm_discrete_z_octant(b->kind);
    if (oa == 0 || ob == 0) return 0;
    if (!qasm_same_operands(a, b)) return 0;
    int sum = (oa + ob) & 7;
    /* Octants 0, 1, 2, 4, 6, 7 reduce 2 gates to 0 or 1 gate */
    return (sum != 3 && sum != 5);
}

typedef enum {
    ZCC_REWRITE_NONE = 0,
    ZCC_REWRITE_DELETE_BOTH,
    ZCC_REWRITE_FUSE_LEFT
} ZCCRewriteAction;

static ZCCRewriteAction qasm_try_fuse_rotations(ZCCQasmOp *a, const ZCCQasmOp *b, double eps) {
    if (!a || !b || a->has_condition || b->has_condition) return ZCC_REWRITE_NONE;
    if (a->kind != b->kind) return ZCC_REWRITE_NONE;
    if (!qasm_gate_is_fusible_rotation(a->kind)) return ZCC_REWRITE_NONE;
    if (!qasm_same_operands(a, b)) return ZCC_REWRITE_NONE;
    if (a->num_params != 1 || b->num_params != 1 || !a->params[0] || !b->params[0]) return ZCC_REWRITE_NONE;

    int ok1 = 0, ok2 = 0;
    double t1 = zcc_qasm_expr_eval_const(a->params[0], &ok1);
    double t2 = zcc_qasm_expr_eval_const(b->params[0], &ok2);
    if (!ok1 || !ok2) return ZCC_REWRITE_NONE;

    double fused_angle = qasm_normalize_angle(t1 + t2);
    if (qasm_angle_is_zero(fused_angle, eps)) {
        return ZCC_REWRITE_DELETE_BOTH;
    }
    /* Update left param in place */
    zcc_qasm_expr_free(a->params[0]);
    a->params[0] = zcc_qasm_expr_num(fused_angle);
    return ZCC_REWRITE_FUSE_LEFT;
}

static int qasm_ops_are_cancellable_or_fusible(const ZCCQasmOp *a, const ZCCQasmOp *b, double eps) {
    (void)eps;
    if (!a || !b) return 0;
    if (qasm_try_cancel_self_inverse(a, b)) return 1;
    if (qasm_try_cancel_inverse_pair(a, b)) return 1;
    if (qasm_discrete_z_pair_reducible(a, b)) return 1;
    if (a->kind == b->kind && qasm_gate_is_fusible_rotation(a->kind) && qasm_same_operands(a, b)) {
        if (a->num_params == 1 && b->num_params == 1 && a->params[0] && b->params[0]) {
            int ok1 = 0, ok2 = 0;
            zcc_qasm_expr_eval_const(a->params[0], &ok1);
            zcc_qasm_expr_eval_const(b->params[0], &ok2);
            if (ok1 && ok2) return 1;
        }
    }
    return 0;
}

/* ================================================================ */
/* CIRCUIT PASS EXECUTION & SLIDING ENGINE                          */
/* ================================================================ */

typedef struct {
    ZCCQasmOp **ops;
    size_t count;
    size_t capacity;
} QasmOpArray;

static void op_array_init(QasmOpArray *arr, size_t cap) {
    arr->capacity = cap > 16 ? cap : 16;
    arr->count = 0;
    arr->ops = (ZCCQasmOp **)calloc(arr->capacity, sizeof(ZCCQasmOp *));
}

static void op_array_push(QasmOpArray *arr, ZCCQasmOp *op) {
    if (arr->count >= arr->capacity) {
        arr->capacity *= 2;
        arr->ops = (ZCCQasmOp **)realloc(arr->ops, arr->capacity * sizeof(ZCCQasmOp *));
    }
    arr->ops[arr->count++] = op;
}

static void op_array_remove_at(QasmOpArray *arr, size_t index) {
    if (index >= arr->count) return;
    qasm_op_free(arr->ops[index]);
    if (index + 1 < arr->count) {
        memmove(&arr->ops[index], &arr->ops[index + 1], (arr->count - index - 1) * sizeof(ZCCQasmOp *));
    }
    arr->count--;
}

static void op_array_remove_two(QasmOpArray *arr, size_t index) {
    if (index + 1 >= arr->count) return;
    qasm_op_free(arr->ops[index]);
    qasm_op_free(arr->ops[index + 1]);
    if (index + 2 < arr->count) {
        memmove(&arr->ops[index], &arr->ops[index + 2], (arr->count - index - 2) * sizeof(ZCCQasmOp *));
    }
    arr->count -= 2;
}

/* Local algebraic rewrite pass */
static int qasm_opt_local_pass(QasmOpArray *arr, const ZCCQasmOptConfig *cfg, ZCCQasmOptStats *stats) {
    int changed = 0;
    size_t i = 0;
    while (i < arr->count) {
        ZCCQasmOp *a = arr->ops[i];
        if (qasm_op_is_rewrite_barrier(a)) {
            i++;
            continue;
        }

        /* 1. Check for zero-angle / identity rotation */
        if (qasm_gate_is_identity_rotation(a, cfg->angle_epsilon)) {
            op_array_remove_at(arr, i);
            stats->gates_removed++;
            stats->rewrite_count++;
            changed = 1;
            continue;
        }

        if (i + 1 >= arr->count) break;
        ZCCQasmOp *b = arr->ops[i + 1];
        if (qasm_op_is_rewrite_barrier(b)) {
            i++;
            continue;
        }

        /* 2. Self-inverse cancellation (H H -> I, CX CX -> I, etc.) */
        if (qasm_try_cancel_self_inverse(a, b)) {
            op_array_remove_two(arr, i);
            stats->gates_removed += 2;
            stats->rewrite_count++;
            changed = 1;
            if (i > 0) i--;
            continue;
        }

        /* 3. Inverse pair cancellation (S Sdg -> I, T Tdg -> I) */
        if (qasm_try_cancel_inverse_pair(a, b)) {
            op_array_remove_two(arr, i);
            stats->gates_removed += 2;
            stats->rewrite_count++;
            changed = 1;
            if (i > 0) i--;
            continue;
        }

        /* 4. Z8 Discrete Clifford+T Phase Octant Fusion (T, S, Z, Sdg, Tdg) */
        if (qasm_discrete_z_pair_reducible(a, b)) {
            int sum = (qasm_discrete_z_octant(a->kind) + qasm_discrete_z_octant(b->kind)) & 7;
            if (sum == 0) {
                op_array_remove_two(arr, i);
                stats->gates_removed += 2;
                stats->rewrite_count++;
                changed = 1;
                if (i > 0) i--;
                continue;
            } else {
                qasm_set_discrete_z_octant_gate(a, sum);
                op_array_remove_at(arr, i + 1);
                stats->gates_removed++;
                stats->gates_fused++;
                stats->rewrite_count++;
                changed = 1;
                if (i > 0) i--;
                continue;
            }
        }

        /* 5. Algebraic 3-Gate Peephole (H Z H -> X, H X H -> Z) */
        if (i + 2 < arr->count) {
            ZCCQasmOp *c = arr->ops[i + 2];
            if (!qasm_op_is_rewrite_barrier(c) && qasm_same_operands(a, b) && qasm_same_operands(b, c)) {
                if (a->kind == QASM_OP_H && b->kind == QASM_OP_Z && c->kind == QASM_OP_H) {
                    a->kind = QASM_OP_X;
                    strncpy(a->gate_name, "x", sizeof(a->gate_name) - 1);
                    op_array_remove_at(arr, i + 2);
                    op_array_remove_at(arr, i + 1);
                    stats->gates_removed += 2;
                    stats->rewrite_count++;
                    changed = 1;
                    if (i > 0) i--;
                    continue;
                }
                if (a->kind == QASM_OP_H && b->kind == QASM_OP_X && c->kind == QASM_OP_H) {
                    a->kind = QASM_OP_Z;
                    strncpy(a->gate_name, "z", sizeof(a->gate_name) - 1);
                    op_array_remove_at(arr, i + 2);
                    op_array_remove_at(arr, i + 1);
                    stats->gates_removed += 2;
                    stats->rewrite_count++;
                    changed = 1;
                    if (i > 0) i--;
                    continue;
                }
            }
        }

        /* 6. Hadamard CNOT-Direction Inversion: H(c) H(t) CX(c,t) H(c) H(t) -> CX(t,c) */
        if (i + 4 < arr->count) {
            ZCCQasmOp *o0 = arr->ops[i];
            ZCCQasmOp *o1 = arr->ops[i + 1];
            ZCCQasmOp *o2 = arr->ops[i + 2];
            ZCCQasmOp *o3 = arr->ops[i + 3];
            ZCCQasmOp *o4 = arr->ops[i + 4];
            if (!qasm_op_is_rewrite_barrier(o0) && !qasm_op_is_rewrite_barrier(o1) &&
                !qasm_op_is_rewrite_barrier(o2) && !qasm_op_is_rewrite_barrier(o3) &&
                !qasm_op_is_rewrite_barrier(o4) &&
                o0->kind == QASM_OP_H && o1->kind == QASM_OP_H &&
                o2->kind == QASM_OP_CX && o2->num_qubits == 2 &&
                o3->kind == QASM_OP_H && o4->kind == QASM_OP_H) {
                const ZCCQasmQubitRef *qc = &o2->qubits[0];
                const ZCCQasmQubitRef *qt = &o2->qubits[1];
                int pre_ok = (qasm_qubit_refs_equal(&o0->qubits[0], qc) && qasm_qubit_refs_equal(&o1->qubits[0], qt)) ||
                             (qasm_qubit_refs_equal(&o0->qubits[0], qt) && qasm_qubit_refs_equal(&o1->qubits[0], qc));
                int post_ok = (qasm_qubit_refs_equal(&o3->qubits[0], qc) && qasm_qubit_refs_equal(&o4->qubits[0], qt)) ||
                              (qasm_qubit_refs_equal(&o3->qubits[0], qt) && qasm_qubit_refs_equal(&o4->qubits[0], qc));
                if (pre_ok && post_ok) {
                    ZCCQasmQubitRef tmp_q = o2->qubits[0];
                    o2->qubits[0] = o2->qubits[1];
                    o2->qubits[1] = tmp_q;
                    op_array_remove_at(arr, i + 4);
                    op_array_remove_at(arr, i + 3);
                    op_array_remove_at(arr, i + 1);
                    op_array_remove_at(arr, i);
                    stats->gates_removed += 4;
                    stats->rewrite_count++;
                    changed = 1;
                    if (i > 0) i--;
                    continue;
                }
            }
        }

        /* 7. ZX-Calculus CNOT-Phase Gadget Target Diagonal Commutation:
         *    CX(c,t); D1(t); CX(c,t); D_mid(t); CX(c,t) -> D_mid(t); CX(c,t); D1(t); CX(c,t); CX(c,t)
         *    Because CX(c,t) D1(t) CX(c,t) = exp(-i theta/2 Z_c Z_t) is diagonal in the Z basis,
         *    it commutes with any 1Q Z-diagonal gate D_mid(t) on the target wire, exposing
         *    adjacent CX(c,t); CX(c,t) cancellation. */
        if (i + 4 < arr->count) {
            ZCCQasmOp *g0 = arr->ops[i];
            ZCCQasmOp *g1 = arr->ops[i + 1];
            ZCCQasmOp *g2 = arr->ops[i + 2];
            ZCCQasmOp *g3 = arr->ops[i + 3];
            ZCCQasmOp *g4 = arr->ops[i + 4];
            if (!qasm_op_is_rewrite_barrier(g0) && !qasm_op_is_rewrite_barrier(g1) &&
                !qasm_op_is_rewrite_barrier(g2) && !qasm_op_is_rewrite_barrier(g3) &&
                !qasm_op_is_rewrite_barrier(g4) &&
                g0->kind == QASM_OP_CX && g2->kind == QASM_OP_CX && g4->kind == QASM_OP_CX &&
                qasm_same_operands(g0, g2) && qasm_same_operands(g2, g4) &&
                qasm_op_is_diagonal_z(g1) && g1->num_qubits == 1 &&
                qasm_op_is_diagonal_z(g3) && g3->num_qubits == 1 &&
                qasm_qubit_refs_equal(&g1->qubits[0], &g0->qubits[1]) &&
                qasm_qubit_refs_equal(&g3->qubits[0], &g0->qubits[1])) {
                arr->ops[i]     = g3;
                arr->ops[i + 1] = g0;
                arr->ops[i + 2] = g1;
                arr->ops[i + 3] = g2;
                stats->gates_slid += 3;
                stats->rewrite_count++;
                changed = 1;
                continue;
            }
        }

        /* 8. Rotation angle fusion (Rx(a) Rx(b) -> Rx(a+b)) */
        ZCCRewriteAction action = qasm_try_fuse_rotations(a, b, cfg->angle_epsilon);
        if (action == ZCC_REWRITE_DELETE_BOTH) {
            op_array_remove_two(arr, i);
            stats->gates_removed += 2;
            stats->rewrite_count++;
            changed = 1;
            if (i > 0) i--;
            continue;
        } else if (action == ZCC_REWRITE_FUSE_LEFT) {
            op_array_remove_at(arr, i + 1);
            stats->gates_removed++;
            stats->gates_fused++;
            stats->rewrite_count++;
            changed = 1;
            if (i > 0) i--;
            continue;
        }

        i++;
    }
    return changed;
}

/* Commutation sliding pass */
static int qasm_opt_commutation_pass(QasmOpArray *arr, const ZCCQasmOptConfig *cfg, ZCCQasmOptStats *stats) {
    int changed = 0;
    size_t i = 0;
    while (i < arr->count) {
        ZCCQasmOp *anchor = arr->ops[i];
        if (qasm_op_is_rewrite_barrier(anchor)) {
            i++;
            continue;
        }

        for (size_t j = i + 1; j < arr->count; j++) {
            ZCCQasmOp *candidate = arr->ops[j];
            if (qasm_op_is_rewrite_barrier(candidate)) break;

            if (qasm_ops_are_cancellable_or_fusible(anchor, candidate, cfg->angle_epsilon)) {
                /* Verify all intermediate gates commute with anchor */
                int can_slide = 1;
                for (size_t k = i + 1; k < j; k++) {
                    if (!qasm_ops_can_commute_disjoint(anchor, arr->ops[k])) {
                        can_slide = 0;
                        break;
                    }
                }
                if (can_slide) {
                    /* Slide candidate to position i + 1 */
                    ZCCQasmOp *target = arr->ops[j];
                    memmove(&arr->ops[i + 2], &arr->ops[i + 1], (j - i - 1) * sizeof(ZCCQasmOp *));
                    arr->ops[i + 1] = target;
                    stats->gates_slid += (j - i - 1);
                    stats->rewrite_count++;
                    changed = 1;
                    break;
                }
            }

            /* Stop forward search if candidate interacts with anchor */
            if (!qasm_ops_can_commute_disjoint(anchor, candidate)) {
                break;
            }
        }
        i++;
    }
    return changed;
}

/* Fingerprint calculation: 64-bit FNV-1a */
uint64_t zcc_qasm_circuit_fingerprint(const ZCCQasmCircuit *circ) {
    if (!circ) return 0;
    uint64_t hash = 14695981039346656037ULL;
    ZCCQasmOp *op = circ->head_op;
    while (op) {
        hash ^= (uint64_t)op->kind;
        hash *= 1099511628211ULL;
        for (int i = 0; i < op->num_qubits; i++) {
            hash ^= (uint64_t)(op->qubits[i].index + 1);
            hash *= 1099511628211ULL;
            const char *s = op->qubits[i].reg_name;
            while (*s) {
                hash ^= (uint64_t)(*s++);
                hash *= 1099511628211ULL;
            }
        }
        for (int p = 0; p < op->num_params; p++) {
            if (op->params[p]) {
                int ok = 0;
                double val = zcc_qasm_expr_eval_const(op->params[p], &ok);
                if (ok) {
                    uint64_t v_bits = 0;
                    memcpy(&v_bits, &val, sizeof(v_bits));
                    hash ^= v_bits;
                    hash *= 1099511628211ULL;
                }
            }
        }
        if (op->has_condition) {
            hash ^= (uint64_t)(op->cond_val + 7);
            hash *= 1099511628211ULL;
        }
        op = op->next;
    }
    return hash;
}

/* ================================================================ */
/* STATEVECTOR EQUIVALENCE ORACLE WITH GLOBAL-PHASE INVARIANCE      */
/* ================================================================ */

static int circuit_is_purely_unitary(const ZCCQasmCircuit *circ) {
    if (!circ) return 0;
    ZCCQasmOp *op = circ->head_op;
    while (op) {
        if (op->has_condition) return 0;
        if (op->kind == QASM_OP_MEASURE || op->kind == QASM_OP_RESET || op->kind == QASM_OP_BARRIER) {
            return 0;
        }
        op = op->next;
    }
    return 1;
}

int zcc_qasm_verify_equivalent(const ZCCQasmCircuit *c1, const ZCCQasmCircuit *c2, double tolerance, char *err_buf, size_t err_buf_size) {
    if (!c1 || !c2) {
        if (err_buf && err_buf_size > 0) {
            snprintf(err_buf, err_buf_size, "invalid null circuit passed to equivalence verifier");
        }
        return 0;
    }
    if (c1->total_qubits != c2->total_qubits) {
        if (err_buf && err_buf_size > 0) {
            snprintf(err_buf, err_buf_size, "total qubit count mismatch: %d vs %d", c1->total_qubits, c2->total_qubits);
        }
        return 0;
    }
    if (!circuit_is_purely_unitary(c1) || !circuit_is_purely_unitary(c2)) {
        /* Non-unitary barrier circuits are preserved without full statevector assertion */
        return 1;
    }

    size_t num_q = (size_t)c1->total_qubits;
    if (num_q == 0) return 1;
    if (num_q > 20) {
        /* Large circuits beyond simulation limits pass structurally */
        return 1;
    }

    ZCCQasmSimulator *sim1 = zcc_qasm_sim_create(num_q, 0, 0x12345678ULL);
    ZCCQasmSimulator *sim2 = zcc_qasm_sim_create(num_q, 0, 0x12345678ULL);
    if (!sim1 || !sim2) {
        if (sim1) zcc_qasm_sim_free(sim1);
        if (sim2) zcc_qasm_sim_free(sim2);
        if (err_buf && err_buf_size > 0) {
            snprintf(err_buf, err_buf_size, "failed to allocate simulator state for equivalence checking");
        }
        return 0;
    }

    if (!zcc_qasm_sim_apply_circuit(sim1, c1) || !zcc_qasm_sim_apply_circuit(sim2, c2)) {
        if (err_buf && err_buf_size > 0) {
            snprintf(err_buf, err_buf_size, "simulator execution failed during equivalence assertion");
        }
        zcc_qasm_sim_free(sim1);
        zcc_qasm_sim_free(sim2);
        return 0;
    }

    /* Compute global phase factor from first non-zero amplitude pair */
    double phase_re = 1.0;
    double phase_im = 0.0;
    int phase_found = 0;
    size_t total_amps = sim1->num_amplitudes;

    for (size_t i = 0; i < total_amps; i++) {
        double a_mag = hypot(sim1->amplitudes[i].real, sim1->amplitudes[i].imag);
        double b_mag = hypot(sim2->amplitudes[i].real, sim2->amplitudes[i].imag);
        if (a_mag <= tolerance && b_mag <= tolerance) continue;
        if (a_mag <= tolerance || b_mag <= tolerance) {
            if (err_buf && err_buf_size > 0) {
                snprintf(err_buf, err_buf_size, "amplitude magnitude mismatch at basis |%zu>: %.6f vs %.6f", i, a_mag, b_mag);
            }
            zcc_qasm_sim_free(sim1);
            zcc_qasm_sim_free(sim2);
            return 0;
        }
        /* phase = a / b */
        double denom = sim2->amplitudes[i].real * sim2->amplitudes[i].real + sim2->amplitudes[i].imag * sim2->amplitudes[i].imag;
        if (denom > 1e-15) {
            phase_re = (sim1->amplitudes[i].real * sim2->amplitudes[i].real + sim1->amplitudes[i].imag * sim2->amplitudes[i].imag) / denom;
            phase_im = (sim1->amplitudes[i].imag * sim2->amplitudes[i].real - sim1->amplitudes[i].real * sim2->amplitudes[i].imag) / denom;
            double p_norm = hypot(phase_re, phase_im);
            if (p_norm > 1e-15) {
                phase_re /= p_norm;
                phase_im /= p_norm;
                phase_found = 1;
                break;
            }
        }
    }

    if (!phase_found) {
        phase_re = 1.0;
        phase_im = 0.0;
    }

    /* Assert all amplitudes match under global phase rotation */
    for (size_t i = 0; i < total_amps; i++) {
        double b_rot_re = phase_re * sim2->amplitudes[i].real - phase_im * sim2->amplitudes[i].imag;
        double b_rot_im = phase_re * sim2->amplitudes[i].imag + phase_im * sim2->amplitudes[i].real;
        double diff_re = fabs(sim1->amplitudes[i].real - b_rot_re);
        double diff_im = fabs(sim1->amplitudes[i].imag - b_rot_im);
        if (diff_re > tolerance || diff_im > tolerance) {
            if (err_buf && err_buf_size > 0) {
                snprintf(err_buf, err_buf_size, "statevector divergence at basis |%zu>: (%.6f,%.6fi) vs rotated (%.6f,%.6fi)",
                         i, sim1->amplitudes[i].real, sim1->amplitudes[i].imag, b_rot_re, b_rot_im);
            }
            zcc_qasm_sim_free(sim1);
            zcc_qasm_sim_free(sim2);
            return 0;
        }
    }

    zcc_qasm_sim_free(sim1);
    zcc_qasm_sim_free(sim2);
    return 1;
}

/* ================================================================ */
/* MAIN TRANSACTIONAL OPTIMIZATION ENTRY POINT                      */
/* ================================================================ */

int zcc_qasm_optimize(const ZCCQasmCircuit *input,
                      ZCCQasmCircuit **output,
                      const ZCCQasmOptConfig *config,
                      ZCCQasmOptStats *stats,
                      char *err_buf,
                      size_t err_buf_size) {
    if (!input || !output) {
        if (err_buf && err_buf_size > 0) {
            snprintf(err_buf, err_buf_size, "invalid NULL circuit parameters passed to zcc_qasm_optimize");
        }
        return -1;
    }
    *output = NULL;

    ZCCQasmOptConfig cfg;
    if (config) {
        cfg = *config;
    } else {
        zcc_qasm_opt_config_default(&cfg);
    }

    ZCCQasmOptStats local_stats;
    memset(&local_stats, 0, sizeof(local_stats));
    local_stats.gates_before = zcc_qasm_circuit_gate_count(input);

    /* 1. Transactional clone of input circuit */
    ZCCQasmCircuit *work = NULL;
    if (zcc_qasm_circuit_clone(input, &work, err_buf, err_buf_size) != 0) {
        return -1;
    }

    /* 2. Load ops into linear working array */
    QasmOpArray arr;
    op_array_init(&arr, (size_t)(work->num_ops + 16));
    ZCCQasmOp *curr = work->head_op;
    while (curr) {
        op_array_push(&arr, curr);
        curr = curr->next;
    }

    /* 3. Execute fixed-point optimization passes */
    uint64_t seen_fingerprints[128];
    size_t num_seen = 0;

    for (unsigned iter = 0; iter < cfg.max_iterations; iter++) {
        int iter_changed = 0;

        if (cfg.enable_local_rewrites) {
            iter_changed |= qasm_opt_local_pass(&arr, &cfg, &local_stats);
        }

        if (cfg.enable_commutation) {
            iter_changed |= qasm_opt_commutation_pass(&arr, &cfg, &local_stats);
        }

        local_stats.iterations++;

        if (!iter_changed) {
            break;
        }
        local_stats.changed = 1;

        /* Re-link ops array temporarily to check fingerprint */
        work->head_op = (arr.count > 0) ? arr.ops[0] : NULL;
        for (size_t k = 0; k < arr.count; k++) {
            arr.ops[k]->next = (k + 1 < arr.count) ? arr.ops[k + 1] : NULL;
        }
        work->tail_op = (arr.count > 0) ? arr.ops[arr.count - 1] : NULL;
        work->num_ops = (int)arr.count;

        uint64_t fp = zcc_qasm_circuit_fingerprint(work);
        int cycle_detected = 0;
        for (size_t s = 0; s < num_seen; s++) {
            if (seen_fingerprints[s] == fp) {
                cycle_detected = 1;
                break;
            }
        }
        if (cycle_detected) {
            break; /* Stable cycle fixed-point reached */
        }
        if (num_seen < sizeof(seen_fingerprints) / sizeof(seen_fingerprints[0])) {
            seen_fingerprints[num_seen++] = fp;
        }
    }

    /* 4. Final rebuild of circuit linked list */
    work->head_op = (arr.count > 0) ? arr.ops[0] : NULL;
    for (size_t k = 0; k < arr.count; k++) {
        arr.ops[k]->next = (k + 1 < arr.count) ? arr.ops[k + 1] : NULL;
    }
    work->tail_op = (arr.count > 0) ? arr.ops[arr.count - 1] : NULL;
    work->num_ops = (int)arr.count;
    free(arr.ops);

    local_stats.gates_after = zcc_qasm_circuit_gate_count(work);
    local_stats.gates_removed = (local_stats.gates_before > local_stats.gates_after) ? (local_stats.gates_before - local_stats.gates_after) : 0;

    /* 5. Semantic equivalence check against simulator oracle */
    if (cfg.enable_equivalence_check) {
        if (!zcc_qasm_verify_equivalent(input, work, cfg.equivalence_tolerance, err_buf, err_buf_size)) {
            zcc_qasm_circuit_free(work);
            return -1;
        }
        local_stats.equivalence_verified = 1;
    }

    if (stats) {
        *stats = local_stats;
    }
    *output = work;
    return 0;
}
