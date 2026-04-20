#ifndef TQPU_H
#define TQPU_H

#include "ir.h"
#include "zprime.h"   // self-mod runtime
#include "zquantum.h" // superposition + entanglement
#include "zqec.h"     // stabilizer syndromes
#include "zsurface.h" // 2D topological lattice + MWPM

// Topological Quantum Processing Unit — the living processor
typedef struct {
    double H[32][32];              // core Hamiltonian field
    QuantumState q;                // quantum amplitudes + phases
    QECState qec;                  // Shor-style syndromes
    SurfaceCodeState sc;           // full distance-3 surface lattice
    double logical_fidelity;       // topological protection metric
} TQPUState;

void tqpu_init(TQPUState *tqpu);
void tqpu_evolve_and_emit(ir_func_t *f, FILE *out);  // full pipeline: Hamiltonian → quantum → QEC → surface → self-mod emit

#endif
