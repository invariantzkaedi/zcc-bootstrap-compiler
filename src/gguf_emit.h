#ifndef ZCC_GGUF_EMIT_H
#define ZCC_GGUF_EMIT_H

#include <stddef.h>
#include <stdint.h>

/* ================================================================= */
/* 🔱 GGUF v3 FILE FORMAT STRUCTURES                                */
/* ================================================================= */

#define GGUF_MAGIC 0x46554747 /* "GGUF" in Little Endian ASCII */

typedef struct {
    uint32_t magic;             /* Must be GGUF_MAGIC */
    uint32_t version;           /* Must be 3 for GGUF v3 */
    uint64_t tensor_count;      /* Number of tensors packed in the file */
    uint64_t metadata_kv_count; /* Number of metadata key-value pairs */
} gguf_header_t;

/* GGUF Metadata Value Types */
typedef enum {
    GGUF_VALUE_TYPE_UINT8 = 0,
    GGUF_VALUE_TYPE_INT8 = 1,
    GGUF_VALUE_TYPE_UINT16 = 2,
    GGUF_VALUE_TYPE_INT16 = 3,
    GGUF_VALUE_TYPE_UINT32 = 4,
    GGUF_VALUE_TYPE_INT32 = 5,
    GGUF_VALUE_TYPE_FLOAT32 = 6,
    GGUF_VALUE_TYPE_BOOL = 7,
    GGUF_VALUE_TYPE_STRING = 8,
    GGUF_VALUE_TYPE_ARRAY = 9,
    GGUF_VALUE_TYPE_UINT64 = 10,
    GGUF_VALUE_TYPE_INT64 = 11,
    GGUF_VALUE_TYPE_FLOAT64 = 12
} gguf_metadata_value_type_t;

/* GGML Numeric Tensor Types */
typedef enum {
    GGML_TYPE_F32 = 0,
    GGML_TYPE_F16 = 1,
    GGML_TYPE_Q4_0 = 2,
    GGML_TYPE_Q4_1 = 3,
    GGML_TYPE_Q5_0 = 6,
    GGML_TYPE_Q5_1 = 7,
    GGML_TYPE_Q8_0 = 8
} ggml_type_t;

/* GGUF Tensor Info Metadata Record */
typedef struct {
    uint64_t name_len;
    char *name;
    uint32_t n_dimensions;
    uint64_t dimensions[4];    /* Up to 4D tensors supported */
    uint32_t type;             /* ggml_type_t */
    uint64_t offset;           /* Byte offset from the start of tensor data */
} gguf_tensor_info_t;

/* ================================================================= */
/* HIGH-LEVEL DRIVER BRIDGE                                         */
/* ================================================================= */

int zcc_emit_gguf(void *cc_ptr, const char *out_path, int quantize_type);

#endif /* ZCC_GGUF_EMIT_H */
