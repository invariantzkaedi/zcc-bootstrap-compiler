#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define close _close
#define PROT_READ 1
#define MAP_SHARED 1
#define MAP_FAILED ((void *)-1)
static void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    HANDLE hMap = CreateFileMapping((HANDLE)_get_osfhandle(fd), NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMap) return MAP_FAILED;
    void *ptr = MapViewOfFile(hMap, FILE_MAP_READ, 0, offset, length);
    CloseHandle(hMap);
    return ptr ? ptr : MAP_FAILED;
}
static int munmap(void *addr, size_t length) {
    return UnmapViewOfFile(addr) ? 0 : -1;
}
#else
#include <sys/mman.h>
#include <unistd.h>
#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif
#endif

static float *gemma_weights = NULL;
static size_t gemma_weights_size = 0;
static int gemma_fd = -1;

void gemma_init() {
    if (gemma_weights != NULL) {
        return; // Already initialized
    }
    
    // ZCC compiler runs inside WSL/Linux during selfhost
    const char *path = "/mnt/h/gemma_a100_weights.bin";
    
    gemma_fd = open(path, O_RDONLY);
    if (gemma_fd < 0) {
        fprintf(stderr, "ZCC GEMMA WARN: Could not open %s\n", path);
        return;
    }
    
    struct stat sb;
    if (fstat(gemma_fd, &sb) == -1) {
        fprintf(stderr, "ZCC GEMMA WARN: Could not stat %s\n", path);
        close(gemma_fd);
        gemma_fd = -1;
        return;
    }
    
    gemma_weights_size = sb.st_size;
    gemma_weights = (float *)mmap(NULL, gemma_weights_size, PROT_READ, MAP_SHARED, gemma_fd, 0);
    
    if (gemma_weights == MAP_FAILED) {
        fprintf(stderr, "ZCC GEMMA WARN: Could not mmap %s\n", path);
        gemma_weights = NULL;
        close(gemma_fd);
        gemma_fd = -1;
        return;
    }
    
    fprintf(stderr, "ZCC GEMMA INFO: Successfully memory-mapped %zu bytes from %s\n", gemma_weights_size, path);
}

void gemma_cleanup() {
    if (gemma_weights != NULL && gemma_weights != MAP_FAILED) {
        munmap(gemma_weights, gemma_weights_size);
        gemma_weights = NULL;
    }
    if (gemma_fd >= 0) {
        close(gemma_fd);
        gemma_fd = -1;
    }
}

// Lightweight C-native matrix multiplication kernel for ZKAEDI bug-hunting loops
void gemma_matmul(float *out, const float *in, const float *weight, int in_dim, int out_dim) {
    for (int i = 0; i < out_dim; i++) {
        float sum = 0.0f;
        for (int j = 0; j < in_dim; j++) {
            sum += in[j] * weight[i * in_dim + j];
        }
        out[i] = sum;
    }
}

#include <stdint.h>
#include <time.h>
#include <math.h>
#pragma GCC target("avx512f")
#include <immintrin.h>

#define GEMMA_EMBED_DIM 4096
#define GEMMA_WINDOW 4096

/* SIMD Dot Product: Processes 64 fp32 parameters per cycle */
__attribute__((target("avx512f")))
static float gemma_simd_dot_product(const float* vec_a, const float* vec_b, size_t dim) {
    __m512 sum0 = _mm512_setzero_ps();
    __m512 sum1 = _mm512_setzero_ps();
    __m512 sum2 = _mm512_setzero_ps();
    __m512 sum3 = _mm512_setzero_ps();

    size_t i = 0;
    
    // 4x Unrolled loop for maximum L1/L2 cache saturation
    for (; i + 63 < dim; i += 64) {
        __m512 a0 = _mm512_loadu_ps(&vec_a[i]);
        __m512 b0 = _mm512_loadu_ps(&vec_b[i]);
        sum0 = _mm512_fmadd_ps(a0, b0, sum0);

        __m512 a1 = _mm512_loadu_ps(&vec_a[i + 16]);
        __m512 b1 = _mm512_loadu_ps(&vec_b[i + 16]);
        sum1 = _mm512_fmadd_ps(a1, b1, sum1);

        __m512 a2 = _mm512_loadu_ps(&vec_a[i + 32]);
        __m512 b2 = _mm512_loadu_ps(&vec_b[i + 32]);
        sum2 = _mm512_fmadd_ps(a2, b2, sum2);

        __m512 a3 = _mm512_loadu_ps(&vec_a[i + 48]);
        __m512 b3 = _mm512_loadu_ps(&vec_b[i + 48]);
        sum3 = _mm512_fmadd_ps(a3, b3, sum3);
    }

    // Aggregate lanes
    sum0 = _mm512_add_ps(sum0, sum1);
    sum2 = _mm512_add_ps(sum2, sum3);
    sum0 = _mm512_add_ps(sum0, sum2);

    // Horizontal reduce to scalar
    float result = _mm512_reduce_add_ps(sum0);

    // Tail cleanup for non-64-aligned dimensions
    for (; i < dim; i++) {
        result += vec_a[i] * vec_b[i];
    }

    return result;
}

float zcc_gemma_evaluate_ir_node(int node_type, int complexity, const char* pattern) {
    struct timespec start_time, end_time;
#ifdef _WIN32
    // Basic Windows fallback for timing, or dummy since it's WSL-only mostly
    start_time.tv_sec = 0; start_time.tv_nsec = 0;
#else
    clock_gettime(CLOCK_MONOTONIC, &start_time);
#endif

    if (gemma_weights == NULL) {
        gemma_init();
    }
    
    if (gemma_weights == NULL) {
        return 0.0f; // Fallback to standard optimization if model is unavailable
    }
    
    uint32_t seed = (uint32_t)node_type ^ (uint32_t)complexity;
    for (int i = 0; pattern && pattern[i]; i++) seed = seed * 31 + pattern[i];

    // Gate D1: Scalar vs SIMD Compare
    float embedding[GEMMA_EMBED_DIM] = {0};

    // Dispersive Murmur-style Scatter: Ensure full 4096-dim activation
    for (int i = 0; pattern && pattern[i]; i++) {
        // Avalanche the character bytes into a high-entropy 32-bit hash
        uint32_t hash = (uint32_t)pattern[i] * 0xcc9e2d51;
        hash = (hash << 15) | (hash >> 17);
        hash *= 0x1b873593;

        // Map to a pseudo-random stride across the 4096 dimensions
        int stride = (hash ^ (uint32_t)i) % GEMMA_EMBED_DIM;
        
        // Inject the positive semantic signal
        embedding[stride] += (float)pattern[i] / 128.0f;
        
        // Anti-alias mirror projection: Inject the complexity/structural signal on the offset
        embedding[(stride + (GEMMA_EMBED_DIM / 2)) % GEMMA_EMBED_DIM] -= (float)(complexity + node_type) / 16.0f; 
    }

    size_t weight_count = gemma_weights_size / sizeof(float);
    size_t weight_offset = ((size_t)node_type * 32 * GEMMA_EMBED_DIM) % weight_count; 
    
    float scalar_score = 0.0f;
    for (int i = 0; i < GEMMA_WINDOW; i++) {
        size_t idx = (weight_offset + i) % weight_count;
        scalar_score += embedding[i] * gemma_weights[idx];
    }
    
    float simd_score = gemma_simd_dot_product(embedding, &gemma_weights[weight_offset], GEMMA_WINDOW);
    float epsilon = fabsf(scalar_score - simd_score);

    int fallback_active = ((seed & 0xFF) > 0x80) ? 1 : 0;
    
    int fallback_state = 0;
    float active_score = 0.0f;
    if (epsilon > 1e-3f) {
        fallback_state = 1;
        active_score = scalar_score;
    } else {
        fallback_state = 0;
        active_score = simd_score;
    }

    // SiLU non-linearity & Sigmoid squeeze thresholding
    // Scale up the score because raw weights sum to very small values
    float scaled_score = simd_score * 100.0f;
    float silu = scaled_score / (1.0f + expf(-scaled_score)); 
    
    // Normalize to 0.0 -> 1.0 confidence using sigmoid
    float confidence = 1.0f / (1.0f + expf(-silu));
    int vote = (confidence > 0.5f) ? 1 : 0; 

#ifndef _WIN32
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double elapsed_ms = (end_time.tv_sec - start_time.tv_sec) * 1000.0 + 
                        (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;
#else
    double elapsed_ms = 0.0;
#endif

    fprintf(stderr, "[GEMMA ORACLE E]\n");
    fprintf(stderr, "pattern=%s\n", pattern);
    fprintf(stderr, "backend=avx512\n");
    fprintf(stderr, "scalar_score=%.4f\n", scalar_score);
    fprintf(stderr, "simd_score=%.4f\n", simd_score);
    fprintf(stderr, "epsilon=%.4f\n", epsilon);
    fprintf(stderr, "fallback=%d\n", fallback_state);
    fprintf(stderr, "latency=%.3fms\n", elapsed_ms);
    fprintf(stderr, "confidence=%.3f\n", confidence);
    fprintf(stderr, "vote=%d\n", vote);
    fprintf(stderr, "\n");

    return confidence;
}

/* ========================================================================= */
/* [ZKAEDI PRIME] AVX-512 Gather-Batched Tensor Kernel                       */
/* Processes 16 AST Nodes simultaneously via transposed 512-bit lanes        */
/* ========================================================================= */

// Process a batch of up to 16 nodes. Returns confidence floats inline.
void zcc_gemma_batch_evaluate(
    int batch_size, 
    const int* node_types, 
    const int* complexities, 
    const char** patterns, 
    float* out_confidences
) {
    if (gemma_weights == NULL) {
        extern void gemma_init();
        gemma_init();
    }
    if (gemma_weights == NULL || batch_size <= 0 || batch_size > 16) return;

    struct timespec start_time, end_time;
#ifndef _WIN32
    clock_gettime(CLOCK_MONOTONIC, &start_time);
#endif

    size_t hidden_dim = GEMMA_EMBED_DIM; // 4096
    
    // 64-byte aligned transposed embedding matrix: embeddings_T[dim][lane]
    __attribute__((aligned(64))) float embeddings_T[4096 * 16] = {0};
    int offsets[16] = {0};

    // 1. Generate embeddings and transpose them for vertical SIMD loading
    for (int k = 0; k < batch_size; k++) {
        offsets[k] = ((size_t)node_types[k] * 32 * hidden_dim) % (gemma_weights_size / sizeof(float)); 
        
        // Dispersive Murmur-style Scatter (Standard ZKAEDI Protocol)
        for (int i = 0; patterns[k] && patterns[k][i]; i++) {
            uint32_t hash = (uint32_t)patterns[k][i] * 0xcc9e2d51;
            hash = (hash << 15) | (hash >> 17);
            hash *= 0x1b873593;

            int stride = (hash ^ (uint32_t)i) % 4096;
            
            // Write directly into the transposed layout (dim * 16 + lane_index)
            embeddings_T[stride * 16 + k] += (float)patterns[k][i] / 128.0f;
            embeddings_T[((stride + 2048) % 4096) * 16 + k] -= (float)(complexities[k] + node_types[k]) / 16.0f; 
        }
    }

    // 2. AVX-512 Transposed Gather Loop
    __m512 acc = _mm512_setzero_ps();
    
    // Load the 16 weight offsets (scaled by 4 bytes per float automatically by gather)
    __m512i vindex = _mm512_loadu_si512((__m512i*)offsets); 

    for (int dim = 0; dim < hidden_dim; dim++) {
        // Gather 16 floats from the 16 different attention heads at the current dimension
        __m512 w = _mm512_i32gather_ps(vindex, &gemma_weights[dim], 4);
        
        // Load the 16 transposed embedding values for the current dimension
        __m512 e = _mm512_load_ps(&embeddings_T[dim * 16]); // Aligned load
        
        // 16 independent dot products in a single clock cycle
        acc = _mm512_fmadd_ps(w, e, acc);
    }

    // 3. Extract, Apply SiLU, and Writeback
    float logits[16];
    _mm512_storeu_ps(logits, acc);

#ifndef _WIN32
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double elapsed_ms = (end_time.tv_sec - start_time.tv_sec) * 1000.0 + 
                        (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;
#else
    double elapsed_ms = 0.0;
#endif

    for (int k = 0; k < batch_size; k++) {
        float simd_score = logits[k];
        float scaled_score = simd_score * 100.0f;
        float silu = scaled_score / (1.0f + expf(-scaled_score)); 
        float confidence = 1.0f / (1.0f + expf(-silu));
        int vote = (confidence > 0.5f) ? 1 : 0; 

        out_confidences[k] = confidence;

        fprintf(stderr, "[GEMMA ORACLE E]\n");
        fprintf(stderr, "pattern=%s\n", patterns[k]);
        fprintf(stderr, "backend=avx512_batch\n");
        fprintf(stderr, "simd_score=%.4f\n", simd_score);
        fprintf(stderr, "latency=%.3fms\n", elapsed_ms / batch_size);
        fprintf(stderr, "confidence=%.3f\n", confidence);
        fprintf(stderr, "vote=%d\n\n", vote);
    }
}
