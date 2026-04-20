#include "tqpu_neural.h"

NeuralPathwayMap NEURAL;

void neural_init(NeuralPathwayMap *map) {
    map->learning_rate = 0.12;
    map->neighborhood_radius = 3.0;
    map->epochs_trained = 0;
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            for (int k = 0; k < 4; k++) {
                map->weights[i][j][k] = (rand() / (double)RAND_MAX) * 0.3 + 0.7;  // bias toward high success
            }
        }
    }
    neural_load(".zcc_neural_weights");  // load previous learning if exists
}

void neural_update(NeuralPathwayMap *map, double reg_spill, double fp_align, double peephole, double selfmod_success) {
    double input[4] = {reg_spill, fp_align, peephole, selfmod_success};
    // Find Best Matching Unit (BMU)
    int bmu_i = 0, bmu_j = 0;
    double min_dist = INFINITY;
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            double dist = 0.0;
            for (int k = 0; k < 4; k++) {
                dist += pow(map->weights[i][j][k] - input[k], 2);
            }
            if (dist < min_dist) {
                min_dist = dist;
                bmu_i = i;
                bmu_j = j;
            }
        }
    }
    // Update neighborhood with gradient descent
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            double dist = hypot(i - bmu_i, j - bmu_j);
            if (dist < map->neighborhood_radius) {
                double lr = map->learning_rate * exp(-dist / map->neighborhood_radius) * exp(-map->epochs_trained / 50.0);
                for (int k = 0; k < 4; k++) {
                    map->weights[i][j][k] += lr * (input[k] - map->weights[i][j][k]);
                }
            }
        }
    }
    map->epochs_trained++;
    if (map->epochs_trained % 5 == 0) neural_save(".zcc_neural_weights");
}

void neural_save(const char *path) {
    FILE *f = fopen(path, "wb");
    if (f) {
        fwrite(&NEURAL, sizeof(NeuralPathwayMap), 1, f);
        fclose(f);
    }
}

void neural_load(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f) {
        fread(&NEURAL, sizeof(NeuralPathwayMap), 1, f);
        fclose(f);
    } else {
        neural_init(&NEURAL);
    }
}

int neural_best_path(NeuralPathwayMap *map, double reg_spill, double fp_align, double peephole) {
    double input[4] = {reg_spill, fp_align, peephole, 1.0};
    int best_i = 0, best_j = 0;
    double best_score = -INFINITY;
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            double score = 0.0;
            for (int k = 0; k < 3; k++) score += map->weights[i][j][k] * input[k];
            score += map->weights[i][j][3];  // success bias
            if (score > best_score) {
                best_score = score;
                best_i = i;
                best_j = j;
            }
        }
    }
    return (best_i * 8 + best_j) % 32;  // map to register index
}
