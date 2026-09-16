/* ========================================================================= */
/* ZCC NATIVE BARE-METAL GGUF / SAFETENSORS INFERENCE SUBSTRATE              */
/* ========================================================================= */
/* File: include/zcc_ai_tensor_ir.h                                          */
/* Description: Zero-copy mmap loading, Tensor SSA representation,           */
/*              Polyhedral loop tiling, and AVX2 quantized inference.        */
/* ========================================================================= */

#ifndef ZCC_AI_TENSOR_IR_H
#define ZCC_AI_TENSOR_IR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GGUF_MAGIC              0x46554747 /* 'GGUF' in LE */
#define GGUF_VERSION            3
#define Q4_0_BLOCK_SIZE         32
#define Q8_0_BLOCK_SIZE         32
#define MAX_TENSOR_DIMS         4
#define MAX_TENSOR_NODES        512

/* Quantized Block Types */
typedef enum {
    ZCC_DTYPE_F32  = 0,
    ZCC_DTYPE_F16  = 1,
    ZCC_DTYPE_Q4_0 = 2,
    ZCC_DTYPE_Q8_0 = 3
} zcc_tensor_dtype_t;

/* Q4_0 Block layout: 1 f16 scale (2 bytes) + 16 bytes for 32 4-bit nibbles = 18 bytes */
#pragma pack(push, 1)
typedef struct {
    uint16_t d;          /* f16 scale factor */
    uint8_t  qs[16];     /* 32 x 4-bit nibbles */
} zcc_block_q4_0_t;

/* Q8_0 Block layout: 1 f16 scale (2 bytes) + 32 int8 values = 34 bytes */
typedef struct {
    uint16_t d;          /* f16 scale factor */
    int8_t   qs[32];     /* 32 x int8 weights */
} zcc_block_q8_0_t;
#pragma pack(pop)

/* In-Memory Tensor Descriptor */
typedef struct {
    char               name[64];
    zcc_tensor_dtype_t dtype;
    uint32_t           n_dims;
    uint64_t           shape[MAX_TENSOR_DIMS];
    uint64_t           strides[MAX_TENSOR_DIMS];
    size_t             byte_size;
    void              *data;           /* Direct mmap pointer */
} zcc_tensor_t;

/* GGUF Model Context (Zero-Copy MMap Handle) */
typedef struct {
    int          fd;
    void        *mmap_addr;
    size_t       file_size;
    uint32_t     version;
    uint64_t     n_tensors;
    uint64_t     n_kv;
    zcc_tensor_t tensors[MAX_TENSOR_NODES];
} zcc_gguf_model_t;

/* Tensor SSA Node Operations */
typedef enum {
    TENSOR_OP_INPUT,
    TENSOR_OP_MATMUL_Q4_0,
    TENSOR_OP_MATMUL_Q8_0,
    TENSOR_OP_RMS_NORM,
    TENSOR_OP_SWIGLU,
    TENSOR_OP_ROPE,
    TENSOR_OP_SOFTMAX
} zcc_tensor_op_t;

typedef struct {
    uint32_t        node_id;
    zcc_tensor_op_t op;
    zcc_tensor_t    output;
    uint32_t        inputs[4];
    uint32_t        n_inputs;
} zcc_tensor_ssa_node_t;

/* Polyhedral Loop Tiling & Graph Context */
typedef struct {
    uint32_t              n_nodes;
    zcc_tensor_ssa_node_t nodes[MAX_TENSOR_NODES];
    uint32_t              tile_size_m;
    uint32_t              tile_size_k;
    uint32_t              tile_size_n;
} zcc_tensor_ir_graph_t;

/* ------------------------------------------------------------------------- */
/* FUNCTION PROTOTYPES (T1 - T5)                                             */
/* ------------------------------------------------------------------------- */

/* T1: GGUF Model Creation & Zero-Copy MMap Loading */
bool zcc_gguf_create_mock_file(const char *path, uint64_t n_embd, uint64_t n_vocab);
bool zcc_gguf_load_mmap(zcc_gguf_model_t *model, const char *path);
void zcc_gguf_free_mmap(zcc_gguf_model_t *model);

/* T2: AVX2 Quantized Vector-Matrix Multiplication (Q4_0 & Q8_0) */
void zcc_quantize_row_q4_0(const float *src, zcc_block_q4_0_t *dst, size_t k);
void zcc_quantize_row_q8_0(const float *src, zcc_block_q8_0_t *dst, size_t k);
void zcc_gemv_q4_0_avx2(const zcc_block_q4_0_t *weights, const float *x, float *y, size_t n_rows, size_t k);
void zcc_gemv_q8_0_avx2(const zcc_block_q8_0_t *weights, const float *x, float *y, size_t n_rows, size_t k);

/* T3: Fused Activation Kernels (RMSNorm, SwiGLU, RoPE) */
void zcc_tensor_rmsnorm(const float *x, const float *weight, float *out, size_t dim, float eps);
void zcc_tensor_swiglu(const float *gate, const float *up, float *out, size_t dim);
void zcc_tensor_rope(float *q, float *k, size_t head_dim, size_t pos, float theta_base);

/* T4: Polyhedral Cache-Tiled Executor */
void zcc_tensor_ir_init_graph(zcc_tensor_ir_graph_t *graph);
bool zcc_tensor_ir_execute(const zcc_tensor_ir_graph_t *graph, const zcc_gguf_model_t *weights);

/* T5: High-Speed Token Generation Benchmark */
double zcc_benchmark_token_generation_throughput(const zcc_gguf_model_t *model, size_t n_tokens);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_AI_TENSOR_IR_H */
