#ifndef ZCC_TENSOR_ATTEST_H
#define ZCC_TENSOR_ATTEST_H

#include <stdint.h>
#include <stddef.h>

#define ZCC_TENSOR_ATTEST_MAGIC 0x5453415f43435aULL
#define ZCC_TENSOR_ATTEST_VERSION 1

typedef struct __attribute__((packed)) {
    uint64_t magic;
    uint32_t schema_version;
    uint32_t verifier_version;
    uint32_t gguf_version;
    uint32_t flags;
    uint32_t record_count;
    uint8_t  manifest_sha256[32];
    uint8_t  gguf_sha256[32];
    uint64_t records_offset;
    uint64_t records_size;
    uint8_t  reserved[20]; /* Pad to 128 bytes (multiple of 32) */
} ZccTensorAttestHeader;

typedef struct __attribute__((packed)) {
    char     name[128];
    uint32_t dtype;
    uint32_t rank;
    uint64_t shape[4];
    uint64_t offset;
    uint64_t nbytes;
    uint32_t alignment;
    uint32_t flags;
    uint8_t  tensor_sha256[32];
    uint8_t  layout_sha256[32];
} ZccTensorAttestRecord;

typedef enum {
    ZCC_TENSOR_OK = 0,
    ZCC_TENSOR_ERR_MISSING,
    ZCC_TENSOR_ERR_LAYOUT_NONCANONICAL,
    ZCC_TENSOR_ERR_SHA256_MISMATCH,
    ZCC_TENSOR_ERR_SHAPE_MISMATCH,
    ZCC_TENSOR_ERR_DTYPE_MISMATCH,
    ZCC_TENSOR_ERR_OFFSET_MISMATCH,
    ZCC_TENSOR_ERR_NBYTES_MISMATCH,
    ZCC_TENSOR_ERR_ALIGNMENT_MISMATCH,
    ZCC_TENSOR_ERR_COUNT_MISMATCH,
    ZCC_TENSOR_ERR_DUPLICATE_NAME,
    ZCC_TENSOR_ERR_IO
} ZccTensorVerifyStatus;

typedef struct {
    int status;
    char error_msg[256];
} ZccTensorVerifyReport;

int zcc_verify_tensor_manifest(
    const void *elf_attest_base,
    size_t elf_attest_size,
    const char *gguf_path,
    ZccTensorVerifyReport *report
);

#endif /* ZCC_TENSOR_ATTEST_H */
