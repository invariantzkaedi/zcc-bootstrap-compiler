#ifndef ZCC_PARALLEL_DISPATCHER_H
#define ZCC_PARALLEL_DISPATCHER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>
#include "quantum_dex_walk_avx2.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZCC_MAX_THREADS 64
#define ZCC_L1_TILE_SIZE 2048 /* 2048 pools * 16 floats * 4B = 128 KB cache-blocked chunk */

/* Parallel Execution Context */
typedef struct {
    pthread_t threads[ZCC_MAX_THREADS];
    int num_threads;
    bool is_initialized;
    bool is_shutdown;
} zcc_parallel_pool_t;

/* Task payload for worker threads */
typedef struct {
    const float *features_matrix;
    const qdex_model_t *model;
    float *out_predictions;
    size_t start_idx;
    size_t count;
} zcc_tile_task_t;

/* Initialize Parallel Thread Pool */
zcc_parallel_pool_t* zcc_parallel_pool_create(int num_threads);

/* Execute Multi-Threaded Tiled AVX2 Prediction */
qdex_status_t zcc_parallel_predict_batch_avx2(zcc_parallel_pool_t *pool,
                                              const float *features_matrix,
                                              const qdex_model_t *model,
                                              float *out_predictions,
                                              size_t total_pools);

/* Destroy and clean up Parallel Thread Pool */
void zcc_parallel_pool_destroy(zcc_parallel_pool_t *pool);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_PARALLEL_DISPATCHER_H */
