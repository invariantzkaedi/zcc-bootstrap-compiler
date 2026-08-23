/*
 * ZCC Quantum Compiler - OpenQASM 2.0 Standalone C Code Generator (Phase 0D)
 * Emits self-contained, standalone C99/C11 simulation code with zero runtime
 * dependencies on ZCC.
 */

#include "include/zcc_qasm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} ZCCStrBuf;

static void buf_init(ZCCStrBuf *b) {
    b->cap = 8192;
    b->len = 0;
    b->data = (char *)malloc(b->cap);
    if (b->data) b->data[0] = '\0';
}

static void buf_free(ZCCStrBuf *b) {
    if (b->data) free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static void buf_puts(ZCCStrBuf *b, const char *s) {
    if (!s) return;
    size_t slen = strlen(s);
    if (b->len + slen + 1 > b->cap) {
        size_t new_cap = (b->cap * 2 > b->len + slen + 1) ? b->cap * 2 : b->len + slen + 1024;
        char *new_data = (char *)realloc(b->data, new_cap);
        if (!new_data) return;
        b->data = new_data;
        b->cap = new_cap;
    }
    memcpy(b->data + b->len, s, slen);
    b->len += slen;
    b->data[b->len] = '\0';
}

static void buf_printf(ZCCStrBuf *b, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (needed < 0) {
        va_end(args);
        return;
    }

    if (b->len + (size_t)needed + 1 > b->cap) {
        size_t new_cap = (b->cap * 2 > b->len + (size_t)needed + 1) ? b->cap * 2 : b->len + (size_t)needed + 1024;
        char *new_data = (char *)realloc(b->data, new_cap);
        if (!new_data) {
            va_end(args);
            return;
        }
        b->data = new_data;
        b->cap = new_cap;
    }

    vsnprintf(b->data + b->len, b->cap - b->len, fmt, args);
    b->len += (size_t)needed;
    va_end(args);
}

void zcc_qasm_c_emit_config_default(ZCCQasmCEmitConfig *cfg) {
    if (!cfg) return;
    cfg->standalone_main = 1;
    cfg->include_comments = 1;
    cfg->enable_openmp = 0;
    cfg->print_threshold = 1e-6;
}

static int resolve_qubit_idx(const ZCCQasmCircuit *circ, const ZCCQasmQubitRef *ref) {
    if (!circ || !ref) return -1;
    for (int i = 0; i < circ->num_registers; i++) {
        if (circ->registers[i].type == QASM_REG_QUANTUM && strcmp(circ->registers[i].name, ref->reg_name) == 0) {
            int idx = (ref->index >= 0) ? ref->index : 0;
            return circ->registers[i].base_offset + idx;
        }
    }
    return -1;
}

static int resolve_clbit_idx(const ZCCQasmCircuit *circ, const ZCCQasmQubitRef *ref) {
    if (!circ || !ref) return -1;
    for (int i = 0; i < circ->num_registers; i++) {
        if (circ->registers[i].type == QASM_REG_CLASSICAL && strcmp(circ->registers[i].name, ref->reg_name) == 0) {
            int idx = (ref->index >= 0) ? ref->index : 0;
            return circ->registers[i].base_offset + idx;
        }
    }
    return -1;
}

static int resolve_bound_qubit(const ZCCQasmCircuit *circ, const ZCCQasmQubitRef *ref,
                              const char *qubit_names[MAX_QASM_GATE_QUBITS],
                              const int *qubit_indices, int n_qbound) {
    if (n_qbound > 0 && qubit_names && qubit_indices) {
        for (int i = 0; i < n_qbound; i++) {
            if (qubit_names[i] && strcmp(qubit_names[i], ref->reg_name) == 0) {
                return qubit_indices[i];
            }
        }
    }
    return resolve_qubit_idx(circ, ref);
}

static double eval_param_expr(const ZCCQasmExpr *expr,
                              const char *param_names[MAX_QASM_GATE_PARAMS],
                              const double *param_vals, int n_pbound) {
    if (!expr) return 0.0;
    if (expr->kind == EXPR_PARAM_REF) {
        if (n_pbound > 0 && param_names && param_vals) {
            for (int i = 0; i < n_pbound; i++) {
                if (param_names[i] && strcmp(param_names[i], expr->param_name) == 0) {
                    return param_vals[i];
                }
            }
        }
        return 0.0;
    }
    if (expr->kind == EXPR_NUM) return expr->num_val;
    if (expr->kind == EXPR_PI) return M_PI;
    if (expr->kind == EXPR_ADD) return eval_param_expr(expr->lhs, param_names, param_vals, n_pbound) + eval_param_expr(expr->rhs, param_names, param_vals, n_pbound);
    if (expr->kind == EXPR_SUB) return eval_param_expr(expr->lhs, param_names, param_vals, n_pbound) - eval_param_expr(expr->rhs, param_names, param_vals, n_pbound);
    if (expr->kind == EXPR_MUL) return eval_param_expr(expr->lhs, param_names, param_vals, n_pbound) * eval_param_expr(expr->rhs, param_names, param_vals, n_pbound);
    if (expr->kind == EXPR_DIV) {
        double r = eval_param_expr(expr->rhs, param_names, param_vals, n_pbound);
        return (r != 0.0) ? (eval_param_expr(expr->lhs, param_names, param_vals, n_pbound) / r) : 0.0;
    }
    if (expr->kind == EXPR_NEG) return -eval_param_expr(expr->lhs, param_names, param_vals, n_pbound);
    if (expr->kind == EXPR_SIN) return sin(eval_param_expr(expr->lhs, param_names, param_vals, n_pbound));
    if (expr->kind == EXPR_COS) return cos(eval_param_expr(expr->lhs, param_names, param_vals, n_pbound));
    if (expr->kind == EXPR_TAN) return tan(eval_param_expr(expr->lhs, param_names, param_vals, n_pbound));
    if (expr->kind == EXPR_LN) {
        double v = eval_param_expr(expr->lhs, param_names, param_vals, n_pbound);
        return (v > 0.0) ? log(v) : 0.0;
    }
    if (expr->kind == EXPR_EXP) return exp(eval_param_expr(expr->lhs, param_names, param_vals, n_pbound));
    if (expr->kind == EXPR_SQRT) {
        double v = eval_param_expr(expr->lhs, param_names, param_vals, n_pbound);
        return (v >= 0.0) ? sqrt(v) : 0.0;
    }
    return 0.0;
}

static const ZCCQasmGateDef *find_custom_gate(const ZCCQasmCircuit *circ, const char *name) {
    if (!circ || !name) return NULL;
    for (int i = 0; i < circ->num_custom_gates; i++) {
        if (strcmp(circ->custom_gates[i].name, name) == 0) return &circ->custom_gates[i];
    }
    return NULL;
}

static void emit_runtime_library(ZCCStrBuf *b, int enable_openmp) {
    const char *code =
"/* ========================================================================= */\n"
"/* STANDALONE QUANTUM SIMULATION RUNTIME (GENERATED BY ZCC PHASE 0D)        */\n"
"/* Zero runtime dependency on ZCC - requires only C99/C11 libc and libm (-lm)*/\n"
"/* ========================================================================= */\n\n"
"#include <stdio.h>\n"
"#include <stdlib.h>\n"
"#include <stdint.h>\n"
"#include <stdbool.h>\n"
"#include <math.h>\n"
"#include <string.h>\n"
"#include <inttypes.h>\n\n"
"#ifndef M_PI\n"
"#define M_PI 3.14159265358979323846\n"
"#endif\n"
"#ifndef M_SQRT1_2\n"
"#define M_SQRT1_2 0.70710678118654752440\n"
"#endif\n\n"
"typedef struct {\n"
"    double real;\n"
"    double imag;\n"
"} zcc_complex_t;\n\n"
"static inline zcc_complex_t c_make(double r, double i) { zcc_complex_t c = {r, i}; return c; }\n"
"static inline zcc_complex_t c_add(zcc_complex_t a, zcc_complex_t b) { return c_make(a.real + b.real, a.imag + b.imag); }\n"
"static inline zcc_complex_t c_sub(zcc_complex_t a, zcc_complex_t b) { return c_make(a.real - b.real, a.imag - b.imag); }\n"
"static inline zcc_complex_t c_mul(zcc_complex_t a, zcc_complex_t b) {\n"
"    return c_make(a.real * b.real - a.imag * b.imag, a.real * b.imag + a.imag * b.real);\n"
"}\n"
"static inline zcc_complex_t c_scale(zcc_complex_t a, double s) { return c_make(a.real * s, a.imag * s); }\n"
"static inline double c_mag_sq(zcc_complex_t a) { return a.real * a.real + a.imag * a.imag; }\n"
"static inline double c_mag(zcc_complex_t a) { return sqrt(c_mag_sq(a)); }\n"
"static inline zcc_complex_t c_exp_i(double theta) { return c_make(cos(theta), sin(theta)); }\n\n"
"typedef struct {\n"
"    size_t num_qubits;\n"
"    size_t num_clbits;\n"
"    size_t num_amplitudes;\n"
"    zcc_complex_t *amplitudes;\n"
"    uint8_t *clbits;\n"
"    uint64_t rng_state;\n"
"} ZCCQasmState;\n\n"
"static uint64_t zcc_rng_next(ZCCQasmState *s) {\n"
"    s->rng_state += 0x9e3779b97f4a7c15ULL;\n"
"    uint64_t z = s->rng_state;\n"
"    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;\n"
"    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;\n"
"    return z ^ (z >> 31);\n"
"}\n\n"
"static double zcc_rng_uniform(ZCCQasmState *s) {\n"
"    return (double)(zcc_rng_next(s) >> 11) * (1.0 / 9007199254740992.0);\n"
"}\n\n"
"static ZCCQasmState *zcc_state_create(size_t num_qubits, size_t num_clbits, uint64_t seed) {\n"
"    if (num_qubits > 28) {\n"
"        fprintf(stderr, \"error: requested qubit count %zu exceeds standalone simulation limit (28)\\n\", num_qubits);\n"
"        return NULL;\n"
"    }\n"
"    ZCCQasmState *s = (ZCCQasmState *)calloc(1, sizeof(ZCCQasmState));\n"
"    if (!s) return NULL;\n"
"    s->num_qubits = num_qubits;\n"
"    s->num_clbits = num_clbits;\n"
"    s->num_amplitudes = (size_t)1 << num_qubits;\n"
"    s->amplitudes = (zcc_complex_t *)calloc(s->num_amplitudes, sizeof(zcc_complex_t));\n"
"    if (!s->amplitudes) {\n"
"        free(s);\n"
"        return NULL;\n"
"    }\n"
"    if (num_clbits > 0) {\n"
"        s->clbits = (uint8_t *)calloc(num_clbits, sizeof(uint8_t));\n"
"    }\n"
"    s->amplitudes[0] = c_make(1.0, 0.0); /* Ground state |0...0> */\n"
"    s->rng_state = (seed != 0) ? seed : 0x123456789ABCDEF0ULL;\n"
"    return s;\n"
"}\n\n"
"static void zcc_state_free(ZCCQasmState *s) {\n"
"    if (!s) return;\n"
"    if (s->amplitudes) free(s->amplitudes);\n"
"    if (s->clbits) free(s->clbits);\n"
"    free(s);\n"
"}\n\n"
"/* 1-Qubit Unitary Transformation */\n"
"static void zcc_apply_1q(ZCCQasmState *s, size_t q, zcc_complex_t u00, zcc_complex_t u01, zcc_complex_t u10, zcc_complex_t u11) {\n"
"    size_t stride = (size_t)1 << q;\n"
"    size_t num_amps = s->num_amplitudes;\n"
"    for (size_t i = 0; i < num_amps; i += (stride << 1)) {\n"
"        for (size_t j = 0; j < stride; j++) {\n"
"            size_t idx0 = i + j;\n"
"            size_t idx1 = idx0 + stride;\n"
"            zcc_complex_t a0 = s->amplitudes[idx0];\n"
"            zcc_complex_t a1 = s->amplitudes[idx1];\n"
"            s->amplitudes[idx0] = c_add(c_mul(u00, a0), c_mul(u01, a1));\n"
"            s->amplitudes[idx1] = c_add(c_mul(u10, a0), c_mul(u11, a1));\n"
"        }\n"
"    }\n"
"}\n\n"
"/* Controlled 1-Qubit Unitary Transformation */\n"
"static void zcc_apply_controlled_1q(ZCCQasmState *s, size_t ctrl, size_t target, zcc_complex_t u00, zcc_complex_t u01, zcc_complex_t u10, zcc_complex_t u11) {\n"
"    size_t stride = (size_t)1 << target;\n"
"    size_t ctrl_mask = (size_t)1 << ctrl;\n"
"    size_t num_amps = s->num_amplitudes;\n"
"    for (size_t i = 0; i < num_amps; i += (stride << 1)) {\n"
"        for (size_t j = 0; j < stride; j++) {\n"
"            size_t idx0 = i + j;\n"
"            size_t idx1 = idx0 + stride;\n"
"            if (idx0 & ctrl_mask) {\n"
"                zcc_complex_t a0 = s->amplitudes[idx0];\n"
"                zcc_complex_t a1 = s->amplitudes[idx1];\n"
"                s->amplitudes[idx0] = c_add(c_mul(u00, a0), c_mul(u01, a1));\n"
"                s->amplitudes[idx1] = c_add(c_mul(u10, a0), c_mul(u11, a1));\n"
"            }\n"
"        }\n"
"    }\n"
"}\n\n"
"/* Standard 1-Qubit Gates */\n"
"static inline void zcc_gate_h(ZCCQasmState *s, size_t q) {\n"
"    zcc_apply_1q(s, q, c_make(M_SQRT1_2, 0.0), c_make(M_SQRT1_2, 0.0),\n"
"                       c_make(M_SQRT1_2, 0.0), c_make(-M_SQRT1_2, 0.0));\n"
"}\n"
"static inline void zcc_gate_x(ZCCQasmState *s, size_t q) {\n"
"    zcc_apply_1q(s, q, c_make(0.0, 0.0), c_make(1.0, 0.0),\n"
"                       c_make(1.0, 0.0), c_make(0.0, 0.0));\n"
"}\n"
"static inline void zcc_gate_y(ZCCQasmState *s, size_t q) {\n"
"    zcc_apply_1q(s, q, c_make(0.0, 0.0), c_make(0.0, -1.0),\n"
"                       c_make(0.0, 1.0), c_make(0.0, 0.0));\n"
"}\n"
"static inline void zcc_gate_z(ZCCQasmState *s, size_t q) {\n"
"    zcc_apply_1q(s, q, c_make(1.0, 0.0), c_make(0.0, 0.0),\n"
"                       c_make(0.0, 0.0), c_make(-1.0, 0.0));\n"
"}\n"
"static inline void zcc_gate_s(ZCCQasmState *s, size_t q) {\n"
"    zcc_apply_1q(s, q, c_make(1.0, 0.0), c_make(0.0, 0.0),\n"
"                       c_make(0.0, 0.0), c_make(0.0, 1.0));\n"
"}\n"
"static inline void zcc_gate_sdg(ZCCQasmState *s, size_t q) {\n"
"    zcc_apply_1q(s, q, c_make(1.0, 0.0), c_make(0.0, 0.0),\n"
"                       c_make(0.0, 0.0), c_make(0.0, -1.0));\n"
"}\n"
"static inline void zcc_gate_t(ZCCQasmState *s, size_t q) {\n"
"    zcc_apply_1q(s, q, c_make(1.0, 0.0), c_make(0.0, 0.0),\n"
"                       c_make(0.0, 0.0), c_exp_i(M_PI / 4.0));\n"
"}\n"
"static inline void zcc_gate_tdg(ZCCQasmState *s, size_t q) {\n"
"    zcc_apply_1q(s, q, c_make(1.0, 0.0), c_make(0.0, 0.0),\n"
"                       c_make(0.0, 0.0), c_exp_i(-M_PI / 4.0));\n"
"}\n"
"static inline void zcc_gate_rx(ZCCQasmState *s, size_t q, double theta) {\n"
"    double half = theta * 0.5;\n"
"    zcc_apply_1q(s, q, c_make(cos(half), 0.0), c_make(0.0, -sin(half)),\n"
"                       c_make(0.0, -sin(half)), c_make(cos(half), 0.0));\n"
"}\n"
"static inline void zcc_gate_ry(ZCCQasmState *s, size_t q, double theta) {\n"
"    double half = theta * 0.5;\n"
"    zcc_apply_1q(s, q, c_make(cos(half), 0.0), c_make(-sin(half), 0.0),\n"
"                       c_make(sin(half), 0.0), c_make(cos(half), 0.0));\n"
"}\n"
"static inline void zcc_gate_rz(ZCCQasmState *s, size_t q, double theta) {\n"
"    double half = theta * 0.5;\n"
"    zcc_apply_1q(s, q, c_exp_i(-half), c_make(0.0, 0.0),\n"
"                       c_make(0.0, 0.0), c_exp_i(half));\n"
"}\n"
"static inline void zcc_gate_p(ZCCQasmState *s, size_t q, double lambda) {\n"
"    zcc_apply_1q(s, q, c_make(1.0, 0.0), c_make(0.0, 0.0),\n"
"                       c_make(0.0, 0.0), c_exp_i(lambda));\n"
"}\n"
"static inline void zcc_gate_u3(ZCCQasmState *s, size_t q, double theta, double phi, double lambda) {\n"
"    double half = theta * 0.5;\n"
"    double c_val = cos(half);\n"
"    double s_val = sin(half);\n"
"    zcc_complex_t u00 = c_make(c_val, 0.0);\n"
"    zcc_complex_t u01 = c_scale(c_exp_i(lambda), -s_val);\n"
"    zcc_complex_t u10 = c_scale(c_exp_i(phi), s_val);\n"
"    zcc_complex_t u11 = c_scale(c_exp_i(phi + lambda), c_val);\n"
"    zcc_apply_1q(s, q, u00, u01, u10, u11);\n"
"}\n"
"static inline void zcc_gate_u2(ZCCQasmState *s, size_t q, double phi, double lambda) {\n"
"    zcc_complex_t u00 = c_make(M_SQRT1_2, 0.0);\n"
"    zcc_complex_t u01 = c_scale(c_exp_i(lambda), -M_SQRT1_2);\n"
"    zcc_complex_t u10 = c_scale(c_exp_i(phi), M_SQRT1_2);\n"
"    zcc_complex_t u11 = c_scale(c_exp_i(phi + lambda), M_SQRT1_2);\n"
"    zcc_apply_1q(s, q, u00, u01, u10, u11);\n"
"}\n"
"static inline void zcc_gate_u1(ZCCQasmState *s, size_t q, double lambda) {\n"
"    zcc_gate_p(s, q, lambda);\n"
"}\n\n"
"/* 2-Qubit Gates */\n"
"static inline void zcc_gate_cx(ZCCQasmState *s, size_t ctrl, size_t target) {\n"
"    zcc_apply_controlled_1q(s, ctrl, target, c_make(0.0, 0.0), c_make(1.0, 0.0),\n"
"                                             c_make(1.0, 0.0), c_make(0.0, 0.0));\n"
"}\n"
"static inline void zcc_gate_cy(ZCCQasmState *s, size_t ctrl, size_t target) {\n"
"    zcc_apply_controlled_1q(s, ctrl, target, c_make(0.0, 0.0), c_make(0.0, -1.0),\n"
"                                             c_make(0.0, 1.0), c_make(0.0, 0.0));\n"
"}\n"
"static inline void zcc_gate_cz(ZCCQasmState *s, size_t q0, size_t q1) {\n"
"    zcc_apply_controlled_1q(s, q0, q1, c_make(1.0, 0.0), c_make(0.0, 0.0),\n"
"                                       c_make(0.0, 0.0), c_make(-1.0, 0.0));\n"
"}\n"
"static inline void zcc_gate_ch(ZCCQasmState *s, size_t ctrl, size_t target) {\n"
"    zcc_apply_controlled_1q(s, ctrl, target, c_make(M_SQRT1_2, 0.0), c_make(M_SQRT1_2, 0.0),\n"
"                                             c_make(M_SQRT1_2, 0.0), c_make(-M_SQRT1_2, 0.0));\n"
"}\n"
"static void zcc_gate_swap(ZCCQasmState *s, size_t q0, size_t q1) {\n"
"    size_t mask0 = (size_t)1 << q0;\n"
"    size_t mask1 = (size_t)1 << q1;\n"
"    size_t num_amps = s->num_amplitudes;\n"
"    for (size_t i = 0; i < num_amps; i++) {\n"
"        if ((i & mask0) && !(i & mask1)) {\n"
"            size_t j = (i & ~mask0) | mask1;\n"
"            zcc_complex_t tmp = s->amplitudes[i];\n"
"            s->amplitudes[i] = s->amplitudes[j];\n"
"            s->amplitudes[j] = tmp;\n"
"        }\n"
"    }\n"
"}\n"
"static void zcc_gate_iswap(ZCCQasmState *s, size_t q0, size_t q1) {\n"
"    size_t mask0 = (size_t)1 << q0;\n"
"    size_t mask1 = (size_t)1 << q1;\n"
"    size_t num_amps = s->num_amplitudes;\n"
"    for (size_t i = 0; i < num_amps; i++) {\n"
"        if ((i & mask0) && !(i & mask1)) {\n"
"            size_t j = (i & ~mask0) | mask1;\n"
"            zcc_complex_t ai = s->amplitudes[i];\n"
"            zcc_complex_t aj = s->amplitudes[j];\n"
"            s->amplitudes[i] = c_make(-aj.imag, aj.real);\n"
"            s->amplitudes[j] = c_make(-ai.imag, ai.real);\n"
"        }\n"
"    }\n"
"}\n"
"static inline void zcc_gate_crx(ZCCQasmState *s, size_t ctrl, size_t target, double theta) {\n"
"    double half = theta * 0.5;\n"
"    zcc_apply_controlled_1q(s, ctrl, target, c_make(cos(half), 0.0), c_make(0.0, -sin(half)),\n"
"                                             c_make(0.0, -sin(half)), c_make(cos(half), 0.0));\n"
"}\n"
"static inline void zcc_gate_cry(ZCCQasmState *s, size_t ctrl, size_t target, double theta) {\n"
"    double half = theta * 0.5;\n"
"    zcc_apply_controlled_1q(s, ctrl, target, c_make(cos(half), 0.0), c_make(-sin(half), 0.0),\n"
"                                             c_make(sin(half), 0.0), c_make(cos(half), 0.0));\n"
"}\n"
"static inline void zcc_gate_crz(ZCCQasmState *s, size_t ctrl, size_t target, double theta) {\n"
"    double half = theta * 0.5;\n"
"    zcc_apply_controlled_1q(s, ctrl, target, c_exp_i(-half), c_make(0.0, 0.0),\n"
"                                             c_make(0.0, 0.0), c_exp_i(half));\n"
"}\n"
"static inline void zcc_gate_cu1(ZCCQasmState *s, size_t ctrl, size_t target, double lambda) {\n"
"    zcc_apply_controlled_1q(s, ctrl, target, c_make(1.0, 0.0), c_make(0.0, 0.0),\n"
"                                             c_make(0.0, 0.0), c_exp_i(lambda));\n"
"}\n"
"static inline void zcc_gate_cu3(ZCCQasmState *s, size_t ctrl, size_t target, double theta, double phi, double lambda) {\n"
"    double half = theta * 0.5;\n"
"    double c_val = cos(half);\n"
"    double s_val = sin(half);\n"
"    zcc_complex_t u00 = c_make(c_val, 0.0);\n"
"    zcc_complex_t u01 = c_scale(c_exp_i(lambda), -s_val);\n"
"    zcc_complex_t u10 = c_scale(c_exp_i(phi), s_val);\n"
"    zcc_complex_t u11 = c_scale(c_exp_i(phi + lambda), c_val);\n"
"    zcc_apply_controlled_1q(s, ctrl, target, u00, u01, u10, u11);\n"
"}\n"
"static void zcc_gate_rzz(ZCCQasmState *s, size_t q0, size_t q1, double theta) {\n"
"    double half = theta * 0.5;\n"
"    zcc_complex_t p_neg = c_exp_i(-half);\n"
"    zcc_complex_t p_pos = c_exp_i(half);\n"
"    size_t mask0 = (size_t)1 << q0;\n"
"    size_t mask1 = (size_t)1 << q1;\n"
"    size_t num_amps = s->num_amplitudes;\n"
"    for (size_t i = 0; i < num_amps; i++) {\n"
"        int b0 = (i & mask0) ? 1 : 0;\n"
"        int b1 = (i & mask1) ? 1 : 0;\n"
"        s->amplitudes[i] = c_mul(s->amplitudes[i], (b0 == b1) ? p_neg : p_pos);\n"
"    }\n"
"}\n\n"
"/* 3-Qubit Gates */\n"
"static void zcc_gate_ccx(ZCCQasmState *s, size_t c0, size_t c1, size_t target) {\n"
"    size_t stride = (size_t)1 << target;\n"
"    size_t ctrl_mask = ((size_t)1 << c0) | ((size_t)1 << c1);\n"
"    size_t num_amps = s->num_amplitudes;\n"
"    for (size_t i = 0; i < num_amps; i += (stride << 1)) {\n"
"        for (size_t j = 0; j < stride; j++) {\n"
"            size_t idx0 = i + j;\n"
"            size_t idx1 = idx0 + stride;\n"
"            if ((idx0 & ctrl_mask) == ctrl_mask) {\n"
"                zcc_complex_t tmp = s->amplitudes[idx0];\n"
"                s->amplitudes[idx0] = s->amplitudes[idx1];\n"
"                s->amplitudes[idx1] = tmp;\n"
"            }\n"
"        }\n"
"    }\n"
"}\n"
"static void zcc_gate_cswap(ZCCQasmState *s, size_t ctrl, size_t a, size_t b) {\n"
"    size_t ctrl_mask = (size_t)1 << ctrl;\n"
"    size_t mask_a = (size_t)1 << a;\n"
"    size_t mask_b = (size_t)1 << b;\n"
"    size_t num_amps = s->num_amplitudes;\n"
"    for (size_t i = 0; i < num_amps; i++) {\n"
"        if ((i & ctrl_mask) && (i & mask_a) && !(i & mask_b)) {\n"
"            size_t j = (i & ~mask_a) | mask_b;\n"
"            zcc_complex_t tmp = s->amplitudes[i];\n"
"            s->amplitudes[i] = s->amplitudes[j];\n"
"            s->amplitudes[j] = tmp;\n"
"        }\n"
"    }\n"
"}\n\n"
"/* Projective Measurement */\n"
"static int zcc_op_measure(ZCCQasmState *s, size_t q, size_t clbit_idx) {\n"
"    size_t mask = (size_t)1 << q;\n"
"    double p1 = 0.0;\n"
"    size_t num_amps = s->num_amplitudes;\n"
"    for (size_t i = 0; i < num_amps; i++) {\n"
"        if (i & mask) p1 += c_mag_sq(s->amplitudes[i]);\n"
"    }\n"
"    double r = zcc_rng_uniform(s);\n"
"    int outcome = (r >= (1.0 - p1)) ? 1 : 0;\n"
"    double norm_factor = outcome ? sqrt(p1) : sqrt(1.0 - p1);\n"
"    if (norm_factor < 1e-15) norm_factor = 1e-15;\n"
"    double inv_norm = 1.0 / norm_factor;\n"
"    for (size_t i = 0; i < num_amps; i++) {\n"
"        int bit = (i & mask) ? 1 : 0;\n"
"        if (bit == outcome) {\n"
"            s->amplitudes[i] = c_scale(s->amplitudes[i], inv_norm);\n"
"        } else {\n"
"            s->amplitudes[i] = c_make(0.0, 0.0);\n"
"        }\n"
"    }\n"
"    if (s->clbits && clbit_idx < s->num_clbits) {\n"
"        s->clbits[clbit_idx] = (uint8_t)outcome;\n"
"    }\n"
"    return outcome;\n"
"}\n\n"
"/* Qubit Reset Operator */\n"
"static void zcc_op_reset(ZCCQasmState *s, size_t q) {\n"
"    int outcome = zcc_op_measure(s, q, (size_t)-1);\n"
"    if (outcome == 1) {\n"
"        zcc_gate_x(s, q);\n"
"    }\n"
"}\n\n"
"/* Reduced 1-Qubit Von Neumann Entropy */\n"
"static double zcc_entropy_1q(const ZCCQasmState *s, size_t q) {\n"
"    size_t mask = (size_t)1 << q;\n"
"    double rho00 = 0.0, rho11 = 0.0;\n"
"    zcc_complex_t rho01 = c_make(0.0, 0.0);\n"
"    size_t num_amps = s->num_amplitudes;\n"
"    for (size_t i = 0; i < num_amps; i++) {\n"
"        if ((i & mask) == 0) {\n"
"            size_t i1 = i | mask;\n"
"            rho00 += c_mag_sq(s->amplitudes[i]);\n"
"            rho11 += c_mag_sq(s->amplitudes[i1]);\n"
"            rho01 = c_add(rho01, c_mul(s->amplitudes[i], c_make(s->amplitudes[i1].real, -s->amplitudes[i1].imag)));\n"
"        }\n"
"    }\n"
"    double disc = sqrt((rho00 - rho11) * (rho00 - rho11) + 4.0 * c_mag_sq(rho01));\n"
"    double l0 = 0.5 * (rho00 + rho11 + disc);\n"
"    double l1 = 0.5 * (rho00 + rho11 - disc);\n"
"    double ent = 0.0;\n"
"    if (l0 > 1e-15) ent -= l0 * (log(l0) / log(2.0));\n"
"    if (l1 > 1e-15) ent -= l1 * (log(l1) / log(2.0));\n"
"    return (ent < 0.0) ? 0.0 : ent;\n"
"}\n\n"
"/* Statevector Formatting */\n"
"static void zcc_dump_state(const ZCCQasmState *s, double threshold) {\n"
"    size_t num_amps = s->num_amplitudes;\n"
"    size_t n = s->num_qubits;\n"
"    for (size_t i = 0; i < num_amps; i++) {\n"
"        double p = c_mag_sq(s->amplitudes[i]);\n"
"        if (p >= threshold) {\n"
"            printf(\"|\");\n"
"            for (int bit = (int)n - 1; bit >= 0; bit--) {\n"
"                printf(\"%c\", (i & ((size_t)1 << bit)) ? '1' : '0');\n"
"            }\n"
"            printf(\">: %+.8f %+.8fi (prob: %.6f)\\n\", s->amplitudes[i].real, s->amplitudes[i].imag, p);\n"
"        }\n"
"    }\n"
"}\n\n";

    buf_puts(b, code);
}

static void emit_op_recursive(ZCCStrBuf *b, const ZCCQasmCircuit *circ, const ZCCQasmOp *op,
                              const char *param_names[MAX_QASM_GATE_PARAMS], const double *param_vals, int n_pbound,
                              const char *qubit_names[MAX_QASM_GATE_QUBITS], const int *qubit_indices, int n_qbound,
                              int depth, const char *indent) {
    if (!op) return;
    if (depth > 16) return;

    if (op->has_condition) {
        /* Classical condition: if (creg == val) */
        int creg_base = -1;
        int creg_size = 0;
        for (int r = 0; r < circ->num_registers; r++) {
            if (circ->registers[r].type == QASM_REG_CLASSICAL && strcmp(circ->registers[r].name, op->cond_reg) == 0) {
                creg_base = circ->registers[r].base_offset;
                creg_size = circ->registers[r].size;
                break;
            }
        }
        if (creg_base >= 0 && creg_size > 0) {
            buf_printf(b, "%s{\n", indent);
            buf_printf(b, "%s    uint64_t _cval = 0;\n", indent);
            for (int bit = 0; bit < creg_size; bit++) {
                buf_printf(b, "%s    if (state->clbits[%d]) _cval |= (1ULL << %d);\n", indent, creg_base + bit, bit);
            }
            buf_printf(b, "%s    if (_cval == %d) {\n", indent, op->cond_val);
            char sub_indent[64];
            snprintf(sub_indent, sizeof(sub_indent), "%s        ", indent);

            /* Create non-conditional copy of op to emit recursively */
            ZCCQasmOp non_cond_op = *op;
            non_cond_op.has_condition = 0;
            emit_op_recursive(b, circ, &non_cond_op, param_names, param_vals, n_pbound, qubit_names, qubit_indices, n_qbound, depth, sub_indent);

            buf_printf(b, "%s    }\n", indent);
            buf_printf(b, "%s}\n", indent);
            return;
        }
    }

    /* Resolve actual qubit indices */
    int actual_qubits[MAX_QASM_GATE_QUBITS] = {-1, -1, -1, -1};
    for (int q = 0; q < op->num_qubits; q++) {
        actual_qubits[q] = resolve_bound_qubit(circ, &op->qubits[q], qubit_names, qubit_indices, n_qbound);
    }

    /* Evaluate actual parameters */
    double actual_params[MAX_QASM_GATE_PARAMS] = {0.0};
    for (int p = 0; p < op->num_params; p++) {
        actual_params[p] = eval_param_expr(op->params[p], param_names, param_vals, n_pbound);
    }

    switch (op->kind) {
        case QASM_OP_BARRIER:
            buf_printf(b, "%s/* barrier */\n", indent);
            break;
        case QASM_OP_RESET: {
            if (actual_qubits[0] >= 0) buf_printf(b, "%szcc_op_reset(state, %d);\n", indent, actual_qubits[0]);
            break;
        }
        case QASM_OP_MEASURE: {
            int c = resolve_clbit_idx(circ, &op->meas_target);
            if (actual_qubits[0] >= 0 && c >= 0) {
                buf_printf(b, "%szcc_op_measure(state, %d, %d);\n", indent, actual_qubits[0], c);
            }
            break;
        }
        case QASM_OP_ID: {
            if (actual_qubits[0] >= 0) buf_printf(b, "%s/* id on q[%d] */\n", indent, actual_qubits[0]);
            break;
        }
        case QASM_OP_H: {
            if (actual_qubits[0] >= 0) buf_printf(b, "%szcc_gate_h(state, %d);\n", indent, actual_qubits[0]);
            break;
        }
        case QASM_OP_X: {
            if (actual_qubits[0] >= 0) buf_printf(b, "%szcc_gate_x(state, %d);\n", indent, actual_qubits[0]);
            break;
        }
        case QASM_OP_Y: {
            if (actual_qubits[0] >= 0) buf_printf(b, "%szcc_gate_y(state, %d);\n", indent, actual_qubits[0]);
            break;
        }
        case QASM_OP_Z: {
            if (actual_qubits[0] >= 0) buf_printf(b, "%szcc_gate_z(state, %d);\n", indent, actual_qubits[0]);
            break;
        }
        case QASM_OP_S: {
            if (actual_qubits[0] >= 0) buf_printf(b, "%szcc_gate_s(state, %d);\n", indent, actual_qubits[0]);
            break;
        }
        case QASM_OP_SDG: {
            if (actual_qubits[0] >= 0) buf_printf(b, "%szcc_gate_sdg(state, %d);\n", indent, actual_qubits[0]);
            break;
        }
        case QASM_OP_T: {
            if (actual_qubits[0] >= 0) buf_printf(b, "%szcc_gate_t(state, %d);\n", indent, actual_qubits[0]);
            break;
        }
        case QASM_OP_TDG: {
            if (actual_qubits[0] >= 0) buf_printf(b, "%szcc_gate_tdg(state, %d);\n", indent, actual_qubits[0]);
            break;
        }
        case QASM_OP_RX: {
            if (actual_qubits[0] >= 0) buf_printf(b, "%szcc_gate_rx(state, %d, %.17g);\n", indent, actual_qubits[0], actual_params[0]);
            break;
        }
        case QASM_OP_RY: {
            if (actual_qubits[0] >= 0) buf_printf(b, "%szcc_gate_ry(state, %d, %.17g);\n", indent, actual_qubits[0], actual_params[0]);
            break;
        }
        case QASM_OP_RZ: {
            if (actual_qubits[0] >= 0) buf_printf(b, "%szcc_gate_rz(state, %d, %.17g);\n", indent, actual_qubits[0], actual_params[0]);
            break;
        }
        case QASM_OP_P: {
            if (actual_qubits[0] >= 0) buf_printf(b, "%szcc_gate_p(state, %d, %.17g);\n", indent, actual_qubits[0], actual_params[0]);
            break;
        }
        case QASM_OP_U1: {
            if (actual_qubits[0] >= 0) buf_printf(b, "%szcc_gate_u1(state, %d, %.17g);\n", indent, actual_qubits[0], actual_params[0]);
            break;
        }
        case QASM_OP_U2: {
            if (actual_qubits[0] >= 0) buf_printf(b, "%szcc_gate_u2(state, %d, %.17g, %.17g);\n", indent, actual_qubits[0], actual_params[0], actual_params[1]);
            break;
        }
        case QASM_OP_U:
        case QASM_OP_U3: {
            if (actual_qubits[0] >= 0) buf_printf(b, "%szcc_gate_u3(state, %d, %.17g, %.17g, %.17g);\n", indent, actual_qubits[0], actual_params[0], actual_params[1], actual_params[2]);
            break;
        }
        case QASM_OP_CX: {
            if (actual_qubits[0] >= 0 && actual_qubits[1] >= 0) buf_printf(b, "%szcc_gate_cx(state, %d, %d);\n", indent, actual_qubits[0], actual_qubits[1]);
            break;
        }
        case QASM_OP_CY: {
            if (actual_qubits[0] >= 0 && actual_qubits[1] >= 0) buf_printf(b, "%szcc_gate_cy(state, %d, %d);\n", indent, actual_qubits[0], actual_qubits[1]);
            break;
        }
        case QASM_OP_CZ: {
            if (actual_qubits[0] >= 0 && actual_qubits[1] >= 0) buf_printf(b, "%szcc_gate_cz(state, %d, %d);\n", indent, actual_qubits[0], actual_qubits[1]);
            break;
        }
        case QASM_OP_CH: {
            if (actual_qubits[0] >= 0 && actual_qubits[1] >= 0) buf_printf(b, "%szcc_gate_ch(state, %d, %d);\n", indent, actual_qubits[0], actual_qubits[1]);
            break;
        }
        case QASM_OP_SWAP: {
            if (actual_qubits[0] >= 0 && actual_qubits[1] >= 0) buf_printf(b, "%szcc_gate_swap(state, %d, %d);\n", indent, actual_qubits[0], actual_qubits[1]);
            break;
        }
        case QASM_OP_ISWAP: {
            if (actual_qubits[0] >= 0 && actual_qubits[1] >= 0) buf_printf(b, "%szcc_gate_iswap(state, %d, %d);\n", indent, actual_qubits[0], actual_qubits[1]);
            break;
        }
        case QASM_OP_CRX: {
            if (actual_qubits[0] >= 0 && actual_qubits[1] >= 0) buf_printf(b, "%szcc_gate_crx(state, %d, %d, %.17g);\n", indent, actual_qubits[0], actual_qubits[1], actual_params[0]);
            break;
        }
        case QASM_OP_CRY: {
            if (actual_qubits[0] >= 0 && actual_qubits[1] >= 0) buf_printf(b, "%szcc_gate_cry(state, %d, %d, %.17g);\n", indent, actual_qubits[0], actual_qubits[1], actual_params[0]);
            break;
        }
        case QASM_OP_CRZ: {
            if (actual_qubits[0] >= 0 && actual_qubits[1] >= 0) buf_printf(b, "%szcc_gate_crz(state, %d, %d, %.17g);\n", indent, actual_qubits[0], actual_qubits[1], actual_params[0]);
            break;
        }
        case QASM_OP_CU1: {
            if (actual_qubits[0] >= 0 && actual_qubits[1] >= 0) buf_printf(b, "%szcc_gate_cu1(state, %d, %d, %.17g);\n", indent, actual_qubits[0], actual_qubits[1], actual_params[0]);
            break;
        }
        case QASM_OP_CU3: {
            if (actual_qubits[0] >= 0 && actual_qubits[1] >= 0) buf_printf(b, "%szcc_gate_cu3(state, %d, %d, %.17g, %.17g, %.17g);\n", indent, actual_qubits[0], actual_qubits[1], actual_params[0], actual_params[1], actual_params[2]);
            break;
        }
        case QASM_OP_RZZ: {
            if (actual_qubits[0] >= 0 && actual_qubits[1] >= 0) buf_printf(b, "%szcc_gate_rzz(state, %d, %d, %.17g);\n", indent, actual_qubits[0], actual_qubits[1], actual_params[0]);
            break;
        }
        case QASM_OP_CCX: {
            if (actual_qubits[0] >= 0 && actual_qubits[1] >= 0 && actual_qubits[2] >= 0) buf_printf(b, "%szcc_gate_ccx(state, %d, %d, %d);\n", indent, actual_qubits[0], actual_qubits[1], actual_qubits[2]);
            break;
        }
        case QASM_OP_CSWAP: {
            if (actual_qubits[0] >= 0 && actual_qubits[1] >= 0 && actual_qubits[2] >= 0) buf_printf(b, "%szcc_gate_cswap(state, %d, %d, %d);\n", indent, actual_qubits[0], actual_qubits[1], actual_qubits[2]);
            break;
        }
        default: {
            /* Custom gate lookup & expansion */
            const ZCCQasmGateDef *gdef = find_custom_gate(circ, op->gate_name);
            if (gdef) {
                const char *new_pnames[MAX_QASM_GATE_PARAMS] = {0};
                double new_pvals[MAX_QASM_GATE_PARAMS] = {0};
                for (int pi = 0; pi < gdef->num_params && pi < op->num_params; pi++) {
                    new_pnames[pi] = gdef->param_names[pi];
                    new_pvals[pi] = actual_params[pi];
                }

                const char *new_qnames[MAX_QASM_GATE_QUBITS] = {0};
                int new_qindices[MAX_QASM_GATE_QUBITS] = {0};
                for (int qi = 0; qi < gdef->num_qubits && qi < op->num_qubits; qi++) {
                    new_qnames[qi] = gdef->qubit_names[qi];
                    new_qindices[qi] = actual_qubits[qi];
                }

                const ZCCQasmOp *bop = gdef->body_head;
                while (bop) {
                    emit_op_recursive(b, circ, bop, new_pnames, new_pvals, gdef->num_params,
                                      new_qnames, new_qindices, gdef->num_qubits, depth + 1, indent);
                    bop = bop->next;
                }
            } else {
                buf_printf(b, "%s/* unrecognized gate: %s */\n", indent, op->gate_name);
            }
            break;
        }
    }
}

char *zcc_qasm_emit_c_code(const ZCCQasmCircuit *circ, const ZCCQasmCEmitConfig *config, char *err_buf, size_t err_buf_size) {
    if (!circ) {
        if (err_buf && err_buf_size > 0) snprintf(err_buf, err_buf_size, "null circuit pointer passed to C emitter");
        return NULL;
    }

    ZCCQasmCEmitConfig cfg;
    if (config) cfg = *config;
    else zcc_qasm_c_emit_config_default(&cfg);

    ZCCStrBuf b;
    buf_init(&b);

    emit_runtime_library(&b, cfg.enable_openmp);

    buf_puts(&b, "/* Circuit Execution Function */\n");
    buf_puts(&b, "void run_quantum_circuit(ZCCQasmState *state) {\n");

    const ZCCQasmOp *op = circ->head_op;
    while (op) {
        emit_op_recursive(&b, circ, op, NULL, NULL, 0, NULL, NULL, 0, 0, "    ");
        op = op->next;
    }

    buf_puts(&b, "}\n\n");

    if (cfg.standalone_main) {
        buf_puts(&b,
"int main(int argc, char **argv) {\n"
"    uint64_t seed = 0x123456789ABCDEF0ULL;\n"
"    double threshold = 1e-6;\n"
"    int dump_state = 1;\n"
"    int show_entropy = 0;\n\n"
"    for (int i = 1; i < argc; i++) {\n"
"        if (strncmp(argv[i], \"--seed=\", 7) == 0) {\n"
"            seed = strtoull(argv[i] + 7, NULL, 0);\n"
"        } else if (strncmp(argv[i], \"--threshold=\", 12) == 0) {\n"
"            threshold = atof(argv[i] + 12);\n"
"        } else if (strcmp(argv[i], \"--entropy\") == 0) {\n"
"            show_entropy = 1;\n"
"        } else if (strcmp(argv[i], \"--no-dump\") == 0) {\n"
"            dump_state = 0;\n"
"        }\n"
"    }\n\n"
        );

        buf_printf(&b, "    size_t num_qubits = %d;\n", circ->total_qubits);
        buf_printf(&b, "    size_t num_clbits = %d;\n", circ->total_clbits);
        buf_printf(&b, "    threshold = %.17g;\n", cfg.print_threshold);

        buf_puts(&b,
"    ZCCQasmState *state = zcc_state_create(num_qubits, num_clbits, seed);\n"
"    if (!state) {\n"
"        fprintf(stderr, \"error: statevector allocation failed\\n\");\n"
"        return 1;\n"
"    }\n\n"
"    run_quantum_circuit(state);\n\n"
"    if (dump_state) {\n"
"        zcc_dump_state(state, threshold);\n"
"    }\n\n"
"    if (show_entropy && num_qubits > 1) {\n"
"        for (size_t q = 0; q < num_qubits; q++) {\n"
"            printf(\"S(q%zu) = %.6f bits\\n\", q, zcc_entropy_1q(state, q));\n"
"        }\n"
"    }\n\n"
"    zcc_state_free(state);\n"
"    return 0;\n"
"}\n"
        );
    }

    char *result = b.data;
    return result;
}

int zcc_qasm_emit_c_file(const ZCCQasmCircuit *circ, const char *filepath, const ZCCQasmCEmitConfig *config, char *err_buf, size_t err_buf_size) {
    if (!filepath) {
        if (err_buf && err_buf_size > 0) snprintf(err_buf, err_buf_size, "null filepath provided");
        return -1;
    }

    char *code = zcc_qasm_emit_c_code(circ, config, err_buf, err_buf_size);
    if (!code) return -1;

    FILE *fp = fopen(filepath, "w");
    if (!fp) {
        if (err_buf && err_buf_size > 0) snprintf(err_buf, err_buf_size, "failed to open output file '%s'", filepath);
        free(code);
        return -1;
    }

    fputs(code, fp);
    fclose(fp);
    free(code);
    return 0;
}
