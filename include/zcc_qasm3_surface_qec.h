/* ========================================================================= */
/* ZCC OPENQASM 3.0 DYNAMIC FEED-FORWARD & SURFACE-17 QEC COMPILER           */
/* ========================================================================= */
/* File: include/zcc_qasm3_surface_qec.h                                     */
/* Description: Fault-Tolerant Quantum Error Correction & Dynamic Flow:      */
/*              1. OpenQASM 3.0 Mid-Circuit Measurement & Feed-Forward       */
/*              2. Rotated Surface-17 (d=3) Stabilizer Plaquette Lattice     */
/*              3. Minimum-Weight Error Syndrome Decoder & Pauli Recovery    */
/*              4. Multi-Target Dynamic Lowering (AVX2, Majorana, PTX)       */
/* ========================================================================= */

#ifndef ZCC_QASM3_SURFACE_QEC_H
#define ZCC_QASM3_SURFACE_QEC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "include/zcc_multi_arch_qasm_dispatcher.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SURFACE17_DATA_QUBITS    9  /* D0 .. D8 (3x3 grid) */
#define SURFACE17_X_ANCILLAS     4  /* X0 .. X3 (X-type stabilizers) */
#define SURFACE17_Z_ANCILLAS     4  /* Z0 .. Z3 (Z-type stabilizers) */
#define SURFACE17_TOTAL_QUBITS   17
#define QASM3_MAX_CLBITS         64
#define QASM3_MAX_STATEMENTS     256

/* OpenQASM 3.0 Statement Kinds */
typedef enum {
    QASM3_STMT_GATE = 1,
    QASM3_STMT_MEASURE,
    QASM3_STMT_RESET,
    QASM3_STMT_IF_BRANCH,
    QASM3_STMT_BARRIER
} Qasm3StmtKind;

/* Dynamic Feed-Forward Branch */
typedef struct {
    uint32_t clbit_condition; /* Index of classical bit to test */
    uint32_t expected_value;  /* 0 or 1 */
    QGateType then_gate;
    uint32_t  then_target;
} Qasm3IfBranch;

/* OpenQASM 3.0 Statement */
typedef struct {
    Qasm3StmtKind kind;
    union {
        struct {
            QGateType type;
            uint32_t  target;
            uint32_t  control;
            double    theta;
        } gate;
        struct {
            uint32_t qubit;
            uint32_t clbit;
        } measure;
        struct {
            uint32_t qubit;
        } reset;
        Qasm3IfBranch branch;
    } data;
} Qasm3Statement;

/* OpenQASM 3.0 Dynamic Program AST */
typedef struct {
    char            name[64];
    uint32_t        n_qubits;
    uint32_t        n_clbits;
    uint32_t        n_stmts;
    Qasm3Statement  stmts[QASM3_MAX_STATEMENTS];
} Qasm3Program;

/* Surface-17 (d=3) Rotated Plaquette Lattice Layout */
typedef struct {
    /* 9 Data Qubits:
     *   D0  D1  D2
     *   D3  D4  D5
     *   D6  D7  D8
     */
    uint8_t data_indices[SURFACE17_DATA_QUBITS];
    uint8_t x_ancilla_indices[SURFACE17_X_ANCILLAS];
    uint8_t z_ancilla_indices[SURFACE17_Z_ANCILLAS];
    
    /* Plaquette Connectivity: list of 2 or 4 data qubits per ancilla */
    uint8_t x_plaquettes[4][4]; /* {d0, d1, d3, d4}, etc. (0xFF if 2-body boundary) */
    uint8_t z_plaquettes[4][4]; /* {d1, d2, d4, d5}, etc. (0xFF if 2-body boundary) */
} Surface17Lattice;

/* QEC Syndrome Measurement & Correction Receipt */
typedef struct {
    uint8_t x_syndrome_bits; /* 4 bits for X0..X3 (Z-error detection) */
    uint8_t z_syndrome_bits; /* 4 bits for Z0..Z3 (X-error detection) */
    uint8_t corrected_qubit; /* Data qubit identified for Pauli correction */
    QGateType correction_gate;/* QGATE_X, QGATE_Z, QGATE_Y, or 0 (no error) */
    bool    logical_state_preserved;
    double  fault_tolerance_gain_db;
} QECSyndromeReceipt;

/* ------------------------------------------------------------------------- */
/* API PROTOTYPES                                                            */
/* ------------------------------------------------------------------------- */

/**
 * @brief Initialize an OpenQASM 3.0 dynamic program.
 */
void qasm3_program_init(Qasm3Program *prog, const char *name, uint32_t n_qubits, uint32_t n_clbits);

/**
 * @brief Append mid-circuit measurement and conditional feed-forward to QASM 3.0 program.
 */
bool qasm3_append_measure(Qasm3Program *prog, uint32_t qubit, uint32_t clbit);
bool qasm3_append_reset(Qasm3Program *prog, uint32_t qubit);
bool qasm3_append_if_branch(Qasm3Program *prog, uint32_t clbit, uint32_t val, QGateType gate, uint32_t target);

/**
 * @brief Initialize and configure the Surface-17 (d=3) Rotated Code Lattice.
 */
void surface17_lattice_init(Surface17Lattice *lat);

/**
 * @brief Transpile a single logical quantum circuit into a Surface-17 Physical Plaquette QASM 3.0 Program.
 */
bool surface17_compile_logical_circuit(
    const QIRCircuit *logical_circuit,
    Qasm3Program *out_physical_prog
);

/**
 * @brief Run fast Syndrome Decoding (Lookup Table Decoder for Distance-3 Surface Code).
 */
bool surface17_decode_syndrome(
    uint8_t x_syndrome,
    uint8_t z_syndrome,
    QECSyndromeReceipt *out_receipt
);

/**
 * @brief Emit Dynamic Multi-Architecture Targets with Mid-Circuit Feed-Forward:
 *        1. AVX2 Statevector Simulator with projection & branchless feed-forward
 *        2. Majorana Nanowire Microcode with dynamic pulse conditioning
 *        3. NVIDIA PTX 7.8 with predicated @%p conditional execution
 */
int32_t qasm3_emit_dynamic_avx_simd_c(const Qasm3Program *prog, char *out_buf, size_t buf_len);
int32_t qasm3_emit_dynamic_majorana_s(const Qasm3Program *prog, char *out_buf, size_t buf_len);
int32_t qasm3_emit_dynamic_nvidia_ptx(const Qasm3Program *prog, char *out_buf, size_t buf_len);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_QASM3_SURFACE_QEC_H */
