#include <stdio.h>
#ifndef ZSURFACE_H
#define ZSURFACE_H

#include "zqec.h"

// Surface Code lattice (distance-3 for demo; easily scaled)
typedef struct {
    QECState qec;                      // inherit previous QEC + quantum
    int lattice_size;                  // d=3 → 3x3 logical surface
    double syndrome_grid[5][5][2];     // [row][col][X/Z] stabilizers
    int matching[32];                  // simple MWPM decoder output (error locations)
    double logical_fidelity;           // topological protection metric
} SurfaceCodeState;

void surface_init(SurfaceCodeState *sc);
void surface_measure_stabilizers(SurfaceCodeState *sc, ir_node_t *insn, double H_field[32][32]);
void surface_decode_and_correct(SurfaceCodeState *sc, ir_node_t *insn, FILE *out);
void ir_to_zsurface(ir_func_t *f, FILE *out);

#endif
