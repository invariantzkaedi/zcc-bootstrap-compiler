#ifndef TQPU_NEURAL_H
#define TQPU_NEURAL_H

#include "ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

typedef struct {
    double weights[8][8][4];      // 8x8 SOM map: [0]=reg_spill, [1]=fp_align, [2]=peephole, [3]=selfmod_success
    double learning_rate;
    double neighborhood_radius;
    int epochs_trained;
} NeuralPathwayMap;

void neural_init(NeuralPathwayMap *map);
void neural_update(NeuralPathwayMap *map, double reg_spill, double fp_align, double peephole, double selfmod_success);
void neural_save(const char *path);
void neural_load(const char *path);
int neural_best_path(NeuralPathwayMap *map, double reg_spill, double fp_align, double peephole);  // returns best reg assignment

extern NeuralPathwayMap NEURAL;

#endif
