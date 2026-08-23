#ifndef ZCC_QASM_H
#define ZCC_QASM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================ */
/* ZCC QUANTUM ASSEMBLY (OpenQASM 2.0) INTERMEDIATE REPRESENTATION  */
/* ================================================================ */

#define ZCC_QASM_MAX_REGS       128
#define MAX_QASM_GATE_QUBITS    8
#define MAX_QASM_GATE_PARAMS    8
#define MAX_QASM_CUSTOM_GATES   64
#define MAX_QASM_CUSTOM_OPS     64

typedef enum {
    QASM_REG_QUANTUM = 1,
    QASM_REG_CLASSICAL = 2
} ZCCQasmRegType;

typedef struct {
    char name[64];
    int size;
    ZCCQasmRegType type;
    int base_offset; /* Global qubit / clbit linear index offset */
    int line;
    int col;
} ZCCQasmRegister;

typedef struct {
    char reg_name[64];
    int index; /* 0-based index within register, or -1 for whole register broadcast */
    int line;
    int col;
} ZCCQasmQubitRef;

typedef enum {
    EXPR_NUM = 1,
    EXPR_PI,
    EXPR_PARAM_REF,
    EXPR_ADD,
    EXPR_SUB,
    EXPR_MUL,
    EXPR_DIV,
    EXPR_NEG,
    EXPR_SIN,
    EXPR_COS,
    EXPR_TAN,
    EXPR_LN,
    EXPR_EXP,
    EXPR_SQRT
} ZCCQasmExprKind;

typedef struct ZCCQasmExpr {
    ZCCQasmExprKind kind;
    double num_val;
    char param_name[32];
    struct ZCCQasmExpr *lhs;
    struct ZCCQasmExpr *rhs;
} ZCCQasmExpr;

typedef enum {
    /* Directives & Builtins */
    QASM_OP_HEADER = 1,
    QASM_OP_INCLUDE,
    QASM_OP_BARRIER,
    QASM_OP_RESET,
    QASM_OP_MEASURE,
    QASM_OP_OPAQUE,
    
    /* Standard 1-Qubit Gates */
    QASM_OP_U,
    QASM_OP_U3,
    QASM_OP_U2,
    QASM_OP_U1,
    QASM_OP_ID,
    QASM_OP_H,
    QASM_OP_X,
    QASM_OP_Y,
    QASM_OP_Z,
    QASM_OP_S,
    QASM_OP_SDG,
    QASM_OP_T,
    QASM_OP_TDG,
    QASM_OP_RX,
    QASM_OP_RY,
    QASM_OP_RZ,
    QASM_OP_P,

    /* Standard 2-Qubit Gates */
    QASM_OP_CX,
    QASM_OP_CY,
    QASM_OP_CZ,
    QASM_OP_CH,
    QASM_OP_SWAP,
    QASM_OP_ISWAP,
    QASM_OP_CRX,
    QASM_OP_CRY,
    QASM_OP_CRZ,
    QASM_OP_CU1,
    QASM_OP_CU3,
    QASM_OP_RZZ,

    /* Standard 3-Qubit Gates */
    QASM_OP_CCX,
    QASM_OP_CSWAP,

    /* Custom / User Gate Call */
    QASM_OP_CUSTOM
} ZCCQasmOpKind;

typedef struct ZCCQasmOp {
    ZCCQasmOpKind kind;
    char gate_name[64];
    
    ZCCQasmQubitRef qubits[MAX_QASM_GATE_QUBITS];
    int num_qubits;
    
    ZCCQasmExpr *params[MAX_QASM_GATE_PARAMS];
    int num_params;
    
    ZCCQasmQubitRef meas_target; /* For QASM_OP_MEASURE: classical bit target */
    
    /* Classical condition: if (cond_reg == cond_val) */
    int has_condition;
    char cond_reg[64];
    int cond_val;
    
    int line;
    int col;
    
    struct ZCCQasmOp *next;
} ZCCQasmOp;

typedef struct {
    char name[64];
    char param_names[MAX_QASM_GATE_PARAMS][32];
    int num_params;
    char qubit_names[MAX_QASM_GATE_QUBITS][32];
    int num_qubits;
    ZCCQasmOp *body_head;
    ZCCQasmOp *body_tail;
    int num_body_ops;
    int line;
    int col;
} ZCCQasmGateDef;

typedef struct {
    double version;
    char include_file[128];
    
    ZCCQasmRegister registers[ZCC_QASM_MAX_REGS];
    int num_registers;
    int total_qubits;
    int total_clbits;
    
    ZCCQasmGateDef custom_gates[MAX_QASM_CUSTOM_GATES];
    int num_custom_gates;
    
    ZCCQasmOp *head_op;
    ZCCQasmOp *tail_op;
    int num_ops;
    
    char filename[256];
} ZCCQasmCircuit;

/* ================================================================ */
/* EXPRESSION & MEMORY LIFETIME HELPERS                             */
/* ================================================================ */

ZCCQasmExpr *zcc_qasm_expr_num(double val);
ZCCQasmExpr *zcc_qasm_expr_pi(void);
ZCCQasmExpr *zcc_qasm_expr_param(const char *name);
ZCCQasmExpr *zcc_qasm_expr_binary(ZCCQasmExprKind kind, ZCCQasmExpr *lhs, ZCCQasmExpr *rhs);
ZCCQasmExpr *zcc_qasm_expr_unary(ZCCQasmExprKind kind, ZCCQasmExpr *child);
double zcc_qasm_expr_eval_const(const ZCCQasmExpr *expr, int *ok);
void zcc_qasm_expr_free(ZCCQasmExpr *expr);

/* ================================================================ */
/* CIRCUIT LIFECYCLE & PARSER API                                    */
/* ================================================================ */

ZCCQasmCircuit *zcc_qasm_circuit_create(void);
void zcc_qasm_circuit_free(ZCCQasmCircuit *circ);
void zcc_qasm_circuit_add_op(ZCCQasmCircuit *circ, ZCCQasmOp *op);

ZCCQasmCircuit *zcc_qasm_parse_string(const char *source, const char *filename, char *err_buf, size_t err_buf_size);
ZCCQasmCircuit *zcc_qasm_parse_file(const char *filename, char *err_buf, size_t err_buf_size);

/* ================================================================ */
/* SEMANTIC VALIDATOR & CANONICAL EMITTER                            */
/* ================================================================ */

int zcc_qasm_validate(const ZCCQasmCircuit *circ, char *err_buf, size_t err_buf_size);
char *zcc_qasm_emit_canonical(const ZCCQasmCircuit *circ);
int zcc_qasm_emit_file(const ZCCQasmCircuit *circ, const char *output_file);

/* ================================================================ */
/* HIGH-PRECISION STATEVECTOR SIMULATOR API                         */
/* ================================================================ */

#define ZCC_QASM_MAX_SIM_QUBITS 28

typedef struct {
    double real;
    double imag;
} ZCCComplex;

typedef struct {
    size_t num_qubits;
    size_t num_amplitudes;
    ZCCComplex *amplitudes;
    uint64_t rng_state;
    unsigned int *classical_bits;
    size_t num_classical_bits;
    char last_error[256];
} ZCCQasmSimulator;

ZCCQasmSimulator *zcc_qasm_sim_create(size_t num_qubits, size_t num_clbits, uint64_t seed);
void zcc_qasm_sim_free(ZCCQasmSimulator *sim);
void zcc_qasm_sim_reset_state(ZCCQasmSimulator *sim);

double zcc_qasm_sim_norm(const ZCCQasmSimulator *sim);
double zcc_qasm_sim_entropy_1q(const ZCCQasmSimulator *sim, size_t target_qubit);

int zcc_qasm_sim_apply_circuit(ZCCQasmSimulator *sim, const ZCCQasmCircuit *circ);
int zcc_qasm_sim_run_file(const char *filename, uint64_t seed, ZCCQasmSimulator **out_sim, char *err_buf, size_t err_buf_size);
char *zcc_qasm_sim_dump_state(const ZCCQasmSimulator *sim, double threshold);

/* ================================================================ */
/* QUANTUM CIRCUIT OPTIMIZER & ALGEBRAIC REWRITE API                */
/* ================================================================ */

typedef struct {
    unsigned max_iterations;
    double angle_epsilon;
    double equivalence_tolerance;
    int enable_local_rewrites;
    int enable_commutation;
    int enable_equivalence_check;
} ZCCQasmOptConfig;

typedef struct {
    size_t gates_before;
    size_t gates_after;
    size_t gates_removed;
    size_t gates_fused;
    size_t gates_slid;
    size_t rewrite_count;
    size_t iterations;
    int changed;
    int equivalence_verified;
} ZCCQasmOptStats;

void zcc_qasm_opt_config_default(ZCCQasmOptConfig *cfg);
ZCCQasmExpr *zcc_qasm_expr_clone(const ZCCQasmExpr *expr);
int zcc_qasm_circuit_clone(const ZCCQasmCircuit *src, ZCCQasmCircuit **dst, char *err_buf, size_t err_buf_size);
size_t zcc_qasm_circuit_gate_count(const ZCCQasmCircuit *circ);
uint64_t zcc_qasm_circuit_fingerprint(const ZCCQasmCircuit *circ);

int zcc_qasm_verify_equivalent(const ZCCQasmCircuit *c1, const ZCCQasmCircuit *c2, double tolerance, char *err_buf, size_t err_buf_size);

int zcc_qasm_optimize(const ZCCQasmCircuit *input,
                      ZCCQasmCircuit **output,
                      const ZCCQasmOptConfig *config,
                      ZCCQasmOptStats *stats,
                      char *err_buf,
                      size_t err_buf_size);

/* ================================================================ */
/* PHASE 0D: STANDALONE C SIMULATION CODE GENERATOR                */
/* ================================================================ */

typedef struct {
    int standalone_main;    /* Emit full standalone main() CLI (default: 1) */
    int include_comments;   /* Emit circuit line and gate comments (default: 1) */
    int enable_openmp;      /* Emit OpenMP parallelization directives (default: 0) */
    double print_threshold; /* Threshold for amplitude reporting (default: 1e-6) */
} ZCCQasmCEmitConfig;

void zcc_qasm_c_emit_config_default(ZCCQasmCEmitConfig *cfg);

char *zcc_qasm_emit_c_code(const ZCCQasmCircuit *circ,
                           const ZCCQasmCEmitConfig *config,
                           char *err_buf,
                           size_t err_buf_size);

int zcc_qasm_emit_c_file(const ZCCQasmCircuit *circ,
                         const char *filepath,
                         const ZCCQasmCEmitConfig *config,
                         char *err_buf,
                         size_t err_buf_size);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_QASM_H */

