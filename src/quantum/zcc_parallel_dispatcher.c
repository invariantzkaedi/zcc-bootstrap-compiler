/* ========================================================================= */
/* ZCC MULTI-THREADED CACHE-TILED AVX2 PARALLEL DISPATCHER                   */
/* ========================================================================= */

#include "zcc_parallel_dispatcher.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    zcc_parallel_pool_t *pool;
    int thread_id;
    zcc_tile_task_t current_task;
    pthread_mutex_t mutex;
    pthread_cond_t start_cond;
    pthread_cond_t done_cond;
    bool has_work;
    bool terminate;
} worker_ctx_t;

static worker_ctx_t g_workers[ZCC_MAX_THREADS];

static void *worker_thread_fn(void *arg) {
    worker_ctx_t *w = (worker_ctx_t*)arg;

    while (1) {
        pthread_mutex_lock(&w->mutex);
        while (!w->has_work && !w->terminate) {
            pthread_cond_wait(&w->start_cond, &w->mutex);
        }

        if (w->terminate) {
            pthread_mutex_unlock(&w->mutex);
            break;
        }

        /* Execute Cache-Tiled AVX2 batch prediction */
        const float *feat_sub = &w->current_task.features_matrix[w->current_task.start_idx * QDEX_FEATURES];
        float *out_sub = &w->current_task.out_predictions[w->current_task.start_idx];

        qdex_predict_batch_avx2(feat_sub, w->current_task.model, out_sub, w->current_task.count);

        w->has_work = false;
        pthread_cond_signal(&w->done_cond);
        pthread_mutex_unlock(&w->mutex);
    }
    return NULL;
}

zcc_parallel_pool_t* zcc_parallel_pool_create(int num_threads) {
    if (num_threads <= 0) {
        num_threads = (int)sysconf(_SC_NPROCESSORS_ONLN);
        if (num_threads <= 0) num_threads = 4;
    }
    if (num_threads > ZCC_MAX_THREADS) num_threads = ZCC_MAX_THREADS;

    zcc_parallel_pool_t *pool = (zcc_parallel_pool_t*)calloc(1, sizeof(zcc_parallel_pool_t));
    if (!pool) return NULL;

    pool->num_threads = num_threads;
    pool->is_initialized = true;
    pool->is_shutdown = false;

    for (int i = 0; i < num_threads; i++) {
        worker_ctx_t *w = &g_workers[i];
        memset(w, 0, sizeof(worker_ctx_t));
        w->pool = pool;
        w->thread_id = i;
        pthread_mutex_init(&w->mutex, NULL);
        pthread_cond_init(&w->start_cond, NULL);
        pthread_cond_init(&w->done_cond, NULL);
        w->has_work = false;
        w->terminate = false;

        pthread_create(&pool->threads[i], NULL, worker_thread_fn, w);
    }

    return pool;
}

qdex_status_t zcc_parallel_predict_batch_avx2(zcc_parallel_pool_t *pool,
                                              const float *features_matrix,
                                              const qdex_model_t *model,
                                              float *out_predictions,
                                              size_t total_pools) {
    if (!pool || !features_matrix || !model || !out_predictions) return QDEX_ERR_NULL_PTR;

    size_t chunk_size = total_pools / pool->num_threads;
    size_t remainder = total_pools % pool->num_threads;

    size_t current_offset = 0;
    for (int i = 0; i < pool->num_threads; i++) {
        size_t count = chunk_size + (i == pool->num_threads - 1 ? remainder : 0);
        worker_ctx_t *w = &g_workers[i];

        pthread_mutex_lock(&w->mutex);
        w->current_task.features_matrix = features_matrix;
        w->current_task.model = model;
        w->current_task.out_predictions = out_predictions;
        w->current_task.start_idx = current_offset;
        w->current_task.count = count;
        w->has_work = true;
        pthread_cond_signal(&w->start_cond);
        pthread_mutex_unlock(&w->mutex);

        current_offset += count;
    }

    /* Barrier Synchronization */
    for (int i = 0; i < pool->num_threads; i++) {
        worker_ctx_t *w = &g_workers[i];
        pthread_mutex_lock(&w->mutex);
        while (w->has_work) {
            pthread_cond_wait(&w->done_cond, &w->mutex);
        }
        pthread_mutex_unlock(&w->mutex);
    }

    return QDEX_OK;
}

void zcc_parallel_pool_destroy(zcc_parallel_pool_t *pool) {
    if (!pool) return;

    for (int i = 0; i < pool->num_threads; i++) {
        worker_ctx_t *w = &g_workers[i];
        pthread_mutex_lock(&w->mutex);
        w->terminate = true;
        pthread_cond_signal(&w->start_cond);
        pthread_mutex_unlock(&w->mutex);

        pthread_join(pool->threads[i], NULL);
        pthread_mutex_destroy(&w->mutex);
        pthread_cond_destroy(&w->start_cond);
        pthread_cond_destroy(&w->done_cond);
    }

    free(pool);
}
