#include <stdio.h>
#ifndef ZQEC_H
#define ZQEC_H

#include "zquantum.h"

// Quantum Error Correction primitives (Shor-code inspired, 9-qubit logical)
typedef struct {
    QuantumState q;                    // inherited quantum state
    double syndrome[9];                // stabilizer measurements (X and Z)
    double fidelity;                   // logical qubit fidelity after correction
} QECState;

void qec_init(QECState *qec);
void qec_measure_syndrome(QECState *qec, ir_node_t *insn, double H_field[32][32]);
void qec_apply_correction(QECState *qec, ir_node_t *insn, FILE *out);
int qec_detect_error(QECState *qec);   // returns 0=no error, 1=X, 2=Z, 3=YZ

#endif
