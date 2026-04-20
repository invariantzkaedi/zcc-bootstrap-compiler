#ifndef ZPRIME_H
#define ZPRIME_H

#include <stdio.h>
#include "ir.h"  // Rely on your existing AST and IR bridges
#ifndef _WIN32
#include <sys/mman.h>
#endif

// ZPrime ISA with self-modifying extension
typedef enum {
    ZPR_H_UPDATE,      // triggers runtime patch
    ZPR_LEA_H,
    ZPR_SIGMOID,
    ZPR_NOISE_INJ,
    ZPR_SELF_MOD,      // explicit self-modify opcode
} zprime_opcode;

void ir_to_zprime(ir_func_t *f, FILE *out);  
void emit_zprime_selfmod_runtime(FILE *out);  

#endif
