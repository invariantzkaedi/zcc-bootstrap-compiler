/* ========================================================================= */
/* ZCC TOPOLOGICAL QPU: NON-ABELIAN MAJORANA BRAID ENGINE (T1-T5)            */
/* ========================================================================= */
/* File: src/quantum/zcc_topological_qpu.h                                   */
/* Description: Fault-Tolerant Topological Quantum Computation:              */
/*              T1: Fibonacci & Ising Anyon Fusion Tree Engine               */
/*              T2: Artin Braid Group (B_N) & Yang-Baxter Verification       */
/*              T3: Solovay-Kitaev Topological Gate Compiler                 */
/*              T4: Majorana Zero Mode (MZM) Parity Readout & Interference   */
/*              T5: Nanowire Cross-Junction Hardware Braid Emitter           */
/* ========================================================================= */

#ifndef ZCC_TOPOLOGICAL_QPU_H
#define ZCC_TOPOLOGICAL_QPU_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TOPO_MAX_ANYONS        16
#define TOPO_MAX_BRAID_STEPS   64
#define TOPO_FIBONACCI_TAU     1.618033988749895 /* (1 + sqrt(5))/2 */

/* Anyon Types */
typedef enum {
    ANYON_VACUUM = 0,   /* 1 */
    ANYON_MAJORANA = 1, /* sigma */
    ANYON_FERMION = 2,  /* psi */
    ANYON_FIBONACCI = 3 /* tau */
} AnyonType;

/* Braid Operation Generator: sigma_i or sigma_i^-1 */
typedef struct {
    uint8_t anyon_index; /* Strand index i in [0, N-2] */
    int8_t  direction;   /* +1 for clockwise, -1 for counter-clockwise */
} TopoBraidStep;

typedef struct {
    uint32_t      n_anyons;
    uint32_t      n_steps;
    TopoBraidStep steps[TOPO_MAX_BRAID_STEPS];
    double        topological_phase;
    double        unitary_fidelity;
} TopoBraidCircuit;

/* 2x2 Complex Unitary Matrix for Braid Evolution */
typedef struct {
    double r00, i00;
    double r01, i01;
    double r10, i10;
    double r11, i11;
} TopoUnitary2x2;

/* MZM Parity Measurement Receipt */
typedef struct {
    int8_t  fermion_parity;    /* +1 (even) or -1 (odd) */
    double  quantum_dimension;
    double  decoherence_immunity_db;
    bool    yang_baxter_satisfied;
} TopoParityReceipt;

/* ------------------------------------------------------------------------- */
/* FUNCTION PROTOTYPES (T1 - T5)                                             */
/* ------------------------------------------------------------------------- */

/* T1: Fusion Tree & Quantum Dimension Calculation */
double topo_compute_quantum_dimension(AnyonType type, uint32_t n_anyons);
bool topo_verify_fusion_rule(AnyonType a, AnyonType b, AnyonType result);

/* T2: Artin Braid Group & Yang-Baxter Verification */
bool topo_eval_braid_generator(uint32_t strand_idx, int8_t dir, TopoUnitary2x2 *out_u);
bool topo_verify_yang_baxter_relation(void);

/* T3: Solovay-Kitaev Topological Gate Compiler */
bool topo_compile_unitary_to_braid(
    double theta_target,
    double phi_target,
    TopoBraidCircuit *out_circuit
);

/* T4: MZM Parity Readout & Topological Phase Interference */
bool topo_measure_majorana_parity(
    const TopoBraidCircuit *circuit,
    TopoParityReceipt *out_receipt
);

/* T5: Nanowire Hardware Braid Bytecode Emitter */
int32_t topo_emit_hardware_microcode(
    const TopoBraidCircuit *circuit,
    char *out_buf,
    size_t buf_len
);

/* T6: Topological Peephole Optimizer (Artin Braid Group Reduction & Cancellation) */
bool topo_optimize_braid_circuit(
    const TopoBraidCircuit *in_circuit,
    TopoBraidCircuit *out_circuit,
    uint32_t *out_eliminated_steps
);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_TOPOLOGICAL_QPU_H */
