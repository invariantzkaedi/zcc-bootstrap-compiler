#ifndef ZQUANTUM_H
#define ZQUANTUM_H

#include <stdio.h>
#include "ir.h"
  // _Complex double for amplitudes

// Quantum-inspired primitives layered on Hamiltonian
typedef _Complex double QuantumAmplitude;

#ifndef QREAL
#define QREAL(x) (((double*)&(x))[0])
#define QIMAG(x) (((double*)&(x))[1])
#define QMAP(x, r, i) do { QREAL(x) = (r); QIMAG(x) = (i); } while(0)
#endif
  // |ψ|² = probability

typedef struct {
    QuantumAmplitude amplitudes[32][32];   // superposition over reg pairs
    double phase[32][32];                  // entanglement phase
    double entanglement_mask[32];          // which regs are entangled
} QuantumState;

void quantum_init_state(QuantumState *q);
void quantum_evolve(QuantumState *q, ir_node_t *insn, double H_field[32][32]);
void quantum_measure_and_collapse(QuantumState *q, ir_node_t *insn, FILE *out);
void quantum_entangle(ir_node_t *a, ir_node_t *b, QuantumState *q);

#endif
