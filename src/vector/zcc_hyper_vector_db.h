/* ========================================================================= */
/* ZCC HYPERVECTORDB: 4-BIT ASYMMETRIC PQ & HNSW SIMD ENGINE (V1-V5)         */
/* ========================================================================= */
/* File: src/vector/zcc_hyper_vector_db.h                                    */
/* Description: Sub-Microsecond High-Dimensional Vector Search Engine:       */
/*              V1: 4-Bit Asymmetric Product Quantization (AQ/PQ-4) FastScan */
/*              V2: 64-Byte Cache-Aligned HNSW Multi-Layer Skip Graph        */
/*              V3: Branchless Priority Beam Search Traverser (<500ns)       */
/*              V4: Zero-Copy MMap Persistence (.hndb) & Static Arena        */
/*              V5: SIMD Predicated Hybrid Metadata Filtering                */
/* ========================================================================= */

#ifndef ZCC_HYPER_VECTOR_DB_H
#define ZCC_HYPER_VECTOR_DB_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HV_MAX_DIM             256
#define HV_MAX_SUB_VECTORS     16
#define HV_CENTROIDS_PER_SUB   16    /* 4-bit quantization = 16 centroids */
#define HV_SUB_DIM             16    /* HV_MAX_DIM / HV_MAX_SUB_VECTORS */
#define HV_MAX_NODES           1024
#define HV_MAX_LEVELS          8
#define HV_MAX_DEGREE          10    /* Exactly 10 * 4 = 40 bytes -> struct total = 64 bytes */
#define HV_MAGIC_HEADER        0x5A434356 /* 'ZCCV' */

/* ------------------------------------------------------------------------- */
/* V1: 4-Bit Product Quantization Data Structures                            */
/* ------------------------------------------------------------------------- */

typedef struct {
    uint32_t dim;
    uint32_t n_sub_vectors;
    uint32_t sub_dim;
    float    centroids[HV_MAX_SUB_VECTORS][HV_CENTROIDS_PER_SUB][HV_SUB_DIM];
} HvPqCodebook;

/* Query Asymmetric Distance Lookup Table (LUT) */
typedef struct {
    float dist_table[HV_MAX_SUB_VECTORS][HV_CENTROIDS_PER_SUB];
} HvPqQueryLut;

/* ------------------------------------------------------------------------- */
/* V2: 64-Byte Cache-Aligned HNSW Node Layout                                */
/* ------------------------------------------------------------------------- */

#if defined(_MSC_VER)
#define HV_ALIGNED_STRUCT(x) __declspec(align(x)) struct
#else
#define HV_ALIGNED_STRUCT(x) struct __attribute__((aligned(x)))
#endif

/* Exactly 64 Bytes (Fits into 1 L1 Cache Line to prevent split-load stalls) */
typedef HV_ALIGNED_STRUCT(64) {
    uint32_t node_id;
    uint16_t level;
    uint16_t degree;
    uint64_t metadata_bitmask;      /* V5: 64-bit metadata flags */
    uint8_t  code_nibbles[8];       /* V1: 16 sub-vectors packed in 8 bytes */
    uint32_t neighbors[HV_MAX_DEGREE]; /* 12 neighbors * 4 bytes = 48 bytes */
} HvNode;

typedef struct {
    uint32_t   n_nodes;
    uint32_t   max_level;
    uint32_t   entry_point_id;
    HvNode     nodes[HV_MAX_NODES];
    HvPqCodebook codebook;
} HvGraph;

/* ------------------------------------------------------------------------- */
/* V3: Priority Queue & Search Result Structures                             */
/* ------------------------------------------------------------------------- */

typedef struct {
    uint32_t node_id;
    float    distance;
} HvCandidate;

typedef struct {
    uint32_t    count;
    HvCandidate items[64];
} HvPriorityQueue;

typedef struct {
    uint32_t top_k;
    uint32_t node_ids[16];
    float    distances[16];
    double   search_latency_ns;
    uint32_t nodes_visited;
} HvSearchResult;

/* ------------------------------------------------------------------------- */
/* FUNCTION PROTOTYPES (V1 - V5)                                             */
/* ------------------------------------------------------------------------- */

/* V1: PQ-4 Codebook Initialization, Quantization & Fast-Scan LUT */
bool hv_pq_init_codebook(HvPqCodebook *cb, uint32_t dim, uint32_t n_sub_vectors);
bool hv_pq_quantize_vector(const HvPqCodebook *cb, const float *vec, uint8_t *out_codes);
bool hv_pq_compute_query_lut(const HvPqCodebook *cb, const float *query_vec, HvPqQueryLut *out_lut);
float hv_pq_fast_distance(const HvPqQueryLut *lut, const uint8_t *codes, uint32_t n_sub_vectors);

/* V2: HNSW Multi-Layer Skip Graph */
HvGraph* hv_graph_create(uint32_t dim, uint32_t n_sub_vectors);
void hv_graph_free(HvGraph *g);
bool hv_graph_init(HvGraph *g, uint32_t dim, uint32_t n_sub_vectors);
uint32_t hv_graph_insert_node(
    HvGraph *g,
    const float *vec,
    uint64_t metadata_mask,
    uint16_t target_level
);

/* V3: SIMD Priority Beam Search (<500ns) */
bool hv_graph_search_knn(
    const HvGraph *g,
    const float *query_vec,
    uint32_t k,
    uint32_t ef_search,
    uint64_t metadata_filter_mask,
    HvSearchResult *out_res
);

/* V4: Zero-Copy MMap Persistence (.hndb) */
bool hv_graph_save_file(const HvGraph *g, const char *file_path);
bool hv_graph_load_file(HvGraph *g, const char *file_path);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_HYPER_VECTOR_DB_H */
