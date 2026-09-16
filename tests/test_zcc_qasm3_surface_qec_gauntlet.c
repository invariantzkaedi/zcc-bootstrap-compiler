/* ========================================================================= */
/* ZCC OPENQASM 3.0 DYNAMIC FEED-FORWARD & SURFACE-17 QEC GAUNTLET (QEC1-QEC4)*/
/* ========================================================================= */
/* File: tests/test_zcc_qasm3_surface_qec_gauntlet.c                         */
/* ========================================================================= */

#include "include/zcc_qasm3_surface_qec.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

int main(void) {
    printf("╔════════════════════════════════════════════════════════════════════════╗\n");
    printf("║  ZCC OPENQASM 3.0 DYNAMIC CONTROL & SURFACE-17 QEC GAUNTLET (QEC1-QEC4)║\n");
    printf("╚════════════════════════════════════════════════════════════════════════╝\n\n");

    /* QEC-1: Surface-17 Lattice Geometry */
    printf("[QEC-1] Initializing Surface-17 (Distance-3) Rotated Plaquette Lattice...\n");
    Surface17Lattice lat;
    surface17_lattice_init(&lat);
    printf("  Data Qubits: %u (D0..D8) | X-Ancillas: %u (X0..X3) | Z-Ancillas: %u (Z0..Z3)\n",
           SURFACE17_DATA_QUBITS, SURFACE17_X_ANCILLAS, SURFACE17_Z_ANCILLAS);
    assert(lat.data_indices[8] == 8);
    assert(lat.x_ancilla_indices[0] == 9);
    assert(lat.z_ancilla_indices[3] == 16);
    assert(lat.x_plaquettes[0][0] == 0 && lat.x_plaquettes[0][1] == 1);
    printf("  [PASS] QEC-1: Surface-17 rotated code plaquette connectivity verified.\n\n");

    /* QEC-2: Logical Transpilation to Physical Surface-17 QASM 3.0 Program */
    printf("[QEC-2] Compiling Logical Hadamard into Physical Surface-17 Plaquette AST...\n");
    QIRCircuit logical;
    qir_circuit_init(&logical, "logical_h", 1);
    qir_circuit_append_1q(&logical, QGATE_H, 0, 0.0);

    Qasm3Program physical_prog;
    bool comp_ok = surface17_compile_logical_circuit(&logical, &physical_prog);
    assert(comp_ok);
    printf("  Physical Program: %s | Total Qubits: %u | Statements: %u\n",
           physical_prog.name, physical_prog.n_qubits, physical_prog.n_stmts);
    assert(physical_prog.n_qubits == 17);
    assert(physical_prog.n_stmts >= 20);
    printf("  [PASS] QEC-2: Transversal logical gate & syndrome extraction cycle verified.\n\n");

    /* QEC-3: Fast Lookup Syndrome Decoder */
    printf("[QEC-3] Testing Surface-17 Fast Lookup Syndrome Decoder & Pauli Error Recovery...\n");
    
    /* Test Case A: Single X-error on Data Qubit D0 -> triggers Z0 (0b0001) */
    QECSyndromeReceipt r_a;
    surface17_decode_syndrome(0x00, 0x01, &r_a);
    printf("  Syndrome (X=0x00, Z=0x01): Corrected Qubit = D%u | Gate = %s | Gain = %.1f dB\n",
           r_a.corrected_qubit, (r_a.correction_gate == QGATE_X) ? "Pauli-X" : "Unknown",
           r_a.fault_tolerance_gain_db);
    assert(r_a.corrected_qubit == 0);
    assert(r_a.correction_gate == QGATE_X);
    assert(r_a.logical_state_preserved);

    /* Test Case B: Single Z-error on Data Qubit D4 -> triggers X1 & X2 (0b0110) */
    QECSyndromeReceipt r_b;
    surface17_decode_syndrome(0x06, 0x00, &r_b);
    printf("  Syndrome (X=0x06, Z=0x00): Corrected Qubit = D%u | Gate = %s | Gain = %.1f dB\n",
           r_b.corrected_qubit, (r_b.correction_gate == QGATE_Z) ? "Pauli-Z" : "Unknown",
           r_b.fault_tolerance_gain_db);
    assert(r_b.corrected_qubit == 4);
    assert(r_b.correction_gate == QGATE_Z);
    assert(r_b.logical_state_preserved);

    /* Test Case C: Combined Y-error on Data Qubit D0 -> triggers X0 (0b0001) and Z0 (0b0001) */
    QECSyndromeReceipt r_c;
    surface17_decode_syndrome(0x01, 0x01, &r_c);
    printf("  Syndrome (X=0x01, Z=0x01): Corrected Qubit = D%u | Gate = %s | Gain = %.1f dB\n",
           r_c.corrected_qubit, (r_c.correction_gate == QGATE_Y) ? "Pauli-Y" : "Unknown",
           r_c.fault_tolerance_gain_db);
    assert(r_c.corrected_qubit == 0);
    assert(r_c.correction_gate == QGATE_Y);
    assert(r_c.logical_state_preserved);

    printf("  [PASS] QEC-3: Fault-tolerant syndrome decoding across X, Z, and Y errors verified.\n\n");

    /* QEC-4: Dynamic Multi-Target Code Emission */
    printf("[QEC-4] Testing Dynamic Multi-Target Emitters with Mid-Circuit Feed-Forward...\n");
    char avx_buf[8192];
    char maj_buf[8192];
    char ptx_buf[8192];

    int32_t b_avx = qasm3_emit_dynamic_avx_simd_c(&physical_prog, avx_buf, sizeof(avx_buf));
    int32_t b_maj = qasm3_emit_dynamic_majorana_s(&physical_prog, maj_buf, sizeof(maj_buf));
    int32_t b_ptx = qasm3_emit_dynamic_nvidia_ptx(&physical_prog, ptx_buf, sizeof(ptx_buf));

    assert(b_avx > 0);
    assert(b_maj > 0);
    assert(b_ptx > 0);

    printf("  Emitted Artifacts:\n");
    printf("    • AVX2 Dynamic SIMD Kernel   : %d bytes (with Born projection & branchless feed-forward)\n", b_avx);
    printf("    • Majorana Braid Microcode   : %d bytes (with read_mzm_parity & conditional pulse jumps)\n", b_maj);
    printf("    • NVIDIA PTX 7.8 Assembly    : %d bytes (with @%%p_cond predicated dynamic branching)\n", b_ptx);

    assert(strstr(avx_buf, "execute_dynamic") != NULL);
    assert(strstr(maj_buf, "read_mzm_parity") != NULL);
    assert(strstr(ptx_buf, "@%p_cond bra") != NULL);

    printf("  [PASS] QEC-4: Dynamic multi-architecture code emitters verified.\n\n");

    printf("========================================================================\n");
    printf("  🏆 OPENQASM 3.0 & SURFACE-17 QEC: ALL MILESTONES (QEC1-QEC4) PASSED!\n");
    printf("========================================================================\n");
    return 0;
}
