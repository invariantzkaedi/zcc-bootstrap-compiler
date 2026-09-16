#include "zcc_triton_bridge.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

int zcc_triton_init_handle(zcc_triton_handle_t *handle, size_t size) {
    if (!handle || size == 0) return -1;
    memset(handle, 0, sizeof(*handle));
    handle->size = size;

    handle->h_field = (float *)aligned_alloc(64, size * sizeof(float));
    handle->h_base = (float *)aligned_alloc(64, size * sizeof(float));
    handle->psi_real = (float *)aligned_alloc(64, size * sizeof(float));
    handle->psi_imag = (float *)aligned_alloc(64, size * sizeof(float));
    handle->dex_features = (float *)aligned_alloc(64, size * sizeof(float));
    handle->profit_out = (float *)aligned_alloc(64, size * sizeof(float));

    if (!handle->h_field || !handle->h_base || !handle->psi_real ||
        !handle->psi_imag || !handle->dex_features || !handle->profit_out) {
        zcc_triton_free_handle(handle);
        return -2;
    }

    for (size_t i = 0; i < size; i++) {
        handle->h_field[i] = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        handle->h_base[i] = handle->h_field[i];
        handle->psi_real[i] = 1.0f / sqrtf((float)size);
        handle->psi_imag[i] = 0.0f;
        handle->dex_features[i] = ((float)rand() / (float)RAND_MAX) * 10.0f;
        handle->profit_out[i] = 0.0f;
    }

    handle->is_vram_mapped = 1;
    handle->last_eval_latency_ns = 0.0;
    return 0;
}

int zcc_triton_free_handle(zcc_triton_handle_t *handle) {
    if (!handle) return -1;
    if (handle->h_field) { free(handle->h_field); handle->h_field = NULL; }
    if (handle->h_base) { free(handle->h_base); handle->h_base = NULL; }
    if (handle->psi_real) { free(handle->psi_real); handle->psi_real = NULL; }
    if (handle->psi_imag) { free(handle->psi_imag); handle->psi_imag = NULL; }
    if (handle->dex_features) { free(handle->dex_features); handle->dex_features = NULL; }
    if (handle->profit_out) { free(handle->profit_out); handle->profit_out = NULL; }
    handle->is_vram_mapped = 0;
    return 0;
}

int zcc_triton_step_hamiltonian(zcc_triton_handle_t *handle, float eta, float gamma, float eps, float beta) {
    if (!handle || !handle->is_vram_mapped) return -1;

    struct timespec ts0, ts1;
    clock_gettime(CLOCK_MONOTONIC, &ts0);

    for (size_t i = 0; i < handle->size; i++) {
        float h = handle->h_field[i];
        float h_base = handle->h_base[i];
        float h_clamped = h > 30.0f ? 30.0f : (h < -30.0f ? -30.0f : h);
        float sig = 1.0f / (1.0f + expf(-gamma * h_clamped));
        float noise = eps * sqrtf(1.0f + beta * fabsf(h_clamped)) * sinf((float)i * 12.34567f);
        float h_next = h_base + eta * h * sig + noise;
        handle->h_field[i] = h_next > 100.0f ? 100.0f : (h_next < -100.0f ? -100.0f : h_next);
    }

    clock_gettime(CLOCK_MONOTONIC, &ts1);
    double ns = (double)(ts1.tv_sec - ts0.tv_sec) * 1e9 + (double)(ts1.tv_nsec - ts0.tv_nsec);
    handle->last_eval_latency_ns = ns / (double)handle->size;
    return 0;
}

int zcc_triton_step_quantum_walk(zcc_triton_handle_t *handle, float gamma_coupling, float dt) {
    if (!handle || !handle->is_vram_mapped) return -1;

    struct timespec ts0, ts1;
    clock_gettime(CLOCK_MONOTONIC, &ts0);

    for (size_t i = 0; i < handle->size; i++) {
        float pr = handle->psi_real[i];
        float pi = handle->psi_imag[i];

        float pr_next = pr + gamma_coupling * pi * dt;
        float pi_next = pi - gamma_coupling * pr * dt;

        float norm_sq = pr_next * pr_next + pi_next * pi_next + 1e-12f;
        float inv_norm = 1.0f / sqrtf(norm_sq);

        pr_next *= inv_norm;
        pi_next *= inv_norm;

        handle->psi_real[i] = pr_next;
        handle->psi_imag[i] = pi_next;
        handle->profit_out[i] = handle->dex_features[i] * (pr_next * pr_next + pi_next * pi_next) * 100.0f;
    }

    clock_gettime(CLOCK_MONOTONIC, &ts1);
    double ns = (double)(ts1.tv_sec - ts0.tv_sec) * 1e9 + (double)(ts1.tv_nsec - ts0.tv_nsec);
    handle->last_eval_latency_ns = ns / (double)handle->size;
    return 0;
}

double zcc_triton_get_peak_throughput(const zcc_triton_handle_t *handle) {
    if (!handle || handle->last_eval_latency_ns <= 0.0) return 0.0;
    return 1e9 / handle->last_eval_latency_ns;
}
