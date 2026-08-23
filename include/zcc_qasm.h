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

#ifdef __cplusplus
}
#endif

#endif /* ZCC_QASM_H */
