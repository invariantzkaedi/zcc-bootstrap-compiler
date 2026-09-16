/* ========================================================================= */
/* ZCC MULTI-ARCHITECTURE QUANTUM HYBRID DISPATCHER                          */
/* ========================================================================= */
/* File: include/zcc_multi_arch_qasm_dispatcher.h                             */
/* Description: Unified IR lowering from QASM/Quantum Circuits to:          */
/*              1. AVX2/AVX-512 C-Native Statevector Kernels (.so)          */
/*              2. Majorana Nanowire Non-Abelian Braid Microcode (.s)        */
/*              3. NVIDIA CUDA/PTX Tensor Statevector Kernels (.ptx)         */
/* ========================================================================= */

#ifndef ZCC_MULTI_ARCH_QASM_DISPATCHER_H
#define ZCC_MULTI_ARCH_QASM_DISPATCHER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "src/quantum/zcc_topological_qpu.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Target Architecture Tier */
typedef enum {
    QDISP_TARGET_AVX_SIMD    = (1 << 0), /* Classical AVX2/AVX-512 SIMD */
    QDISP_TARGET_TOPO_BRAID  = (1 << 1), /* Majorana Nanowire Microcode */
    QDISP_TARGET_NVIDIA_PTX  = (1 << 2), /* NVIDIA PTX 7.8 / SM_89 (RTX 5070) */
    QDISP_TARGET_ALL         = 0x07
} QDispTargetMask;

/* Quantum Intermediate Representation Gate Node */
typedef enum {
    QGATE_H = 1,
    QGATE_X,
    QGATE_Y,
    QGATE_Z,
    QGATE_S,
    QGATE_T,
    QGATE_RX,
    QGATE_RY,
    QGATE_RZ,
    QGATE_CX,
    QGATE_CZ,
    QGATE_SWAP
} QGateType;

typedef struct {
    QGateType type;
    uint32_t  qubit_target;
    uint32_t  qubit_control; /* Only for 2-qubit gates (CX, CZ, SWAP) */
    double    theta;         /* Rotation angle for RX/RY/RZ */
} QIRGate;

#define QIR_MAX_GATES 256

typedef struct {
    char     circuit_name[64];
    uint32_t n_qubits;
    uint32_t n_gates;
    QIRGate  gates[QIR_MAX_GATES];
} QIRCircuit;

/* Multi-Target Dispatch Receipts */
typedef struct {
    uint32_t avx_c_bytes;
    uint32_t topo_s_bytes;
    uint32_t ptx_bytes;
    uint32_t topo_steps_unopt;
    uint32_t topo_steps_opt;
    double   opt_step_compression_pct;
    bool     all_targets_emitted_cleanly;
} QDispArtifactReceipt;

/* ------------------------------------------------------------------------- */
/* API FUNCTIONS                                                             */
/* ------------------------------------------------------------------------- */

/**
 * @brief Initialize a QIR Circuit structure.
 */
void qir_circuit_init(QIRCircuit *circuit, const char *name, uint32_t n_qubits);

/**
 * @brief Append gates to QIR circuit.
 */
bool qir_circuit_append_1q(QIRCircuit *circuit, QGateType type, uint32_t target, double theta);
bool qir_circuit_append_2q(QIRCircuit *circuit, QGateType type, uint32_t control, uint32_t target);

/**
 * @brief Parse a subset of OpenQASM 2.0 text into QIR.
 */
bool qir_parse_qasm_string(const char *qasm_str, QIRCircuit *out_circuit);

/**
 * @brief Target 1: Emit AVX2/AVX-512 C-Native Statevector simulation code.
 */
int32_t qdisp_emit_avx_simd_c(
    const QIRCircuit *circuit,
    char *out_buf,
    size_t buf_len
);

/**
 * @brief Target 2: Lower unitary gates to Solovay-Kitaev / Majorana Nanowire Braid Microcode (.s).
 */
int32_t qdisp_emit_topological_braid_s(
    const QIRCircuit *circuit,
    char *out_buf,
    size_t buf_len,
    uint32_t *out_unopt_steps,
    uint32_t *out_opt_steps
);

/**
 * @brief Target 3: Lower QIR directly to NVIDIA PTX 7.8 Assembly (.ptx).
 */
int32_t qdisp_emit_nvidia_ptx(
    const QIRCircuit *circuit,
    char *out_buf,
    size_t buf_len
);

/**
 * @brief Master Dispatcher: Emit all 3 target artifacts and optionally write them to disk.
 */
bool qdisp_lower_and_emit_all(
    const QIRCircuit *circuit,
    const char *out_dir,
    QDispArtifactReceipt *out_receipt
);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_MULTI_ARCH_QASM_DISPATCHER_H */
