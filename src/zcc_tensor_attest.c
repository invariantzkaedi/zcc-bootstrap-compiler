#include "zcc_tensor_attest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* SHA-256 freestanding implementation */
typedef struct {
    uint8_t data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} ZccSHA256_CTX;

#define ROTRIGHT(a,b) (((a) >> (b)) | ((a) << (32-(b))))
#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT(x,2) ^ ROTRIGHT(x,13) ^ ROTRIGHT(x,22))
#define EP1(x) (ROTRIGHT(x,6) ^ ROTRIGHT(x,11) ^ ROTRIGHT(x,25))
#define SIG0(x) (ROTRIGHT(x,7) ^ ROTRIGHT(x,18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT(x,17) ^ ROTRIGHT(x,19) ^ ((x) >> 10))

static const uint32_t k_sha256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static void zcc_sha256_transform(ZccSHA256_CTX *ctx, const uint8_t data[]) {
    uint32_t a, b, c, d, e, f, g, h, i, j, t1, t2, m[64];
    for (i = 0, j = 0; i < 16; ++i, j += 4)
        m[i] = (data[j] << 24) | (data[j + 1] << 16) | (data[j + 2] << 8) | (data[j + 3]);
    for (; i < 64; ++i)
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];
    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];
    for (i = 0; i < 64; ++i) {
        t1 = h + EP1(e) + CH(e, f, g) + k_sha256[i] + m[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void zcc_sha256_init(ZccSHA256_CTX *ctx) {
    ctx->datalen = 0; ctx->bitlen = 0;
    ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85; ctx->state[2] = 0x3c6ef372; ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c; ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
}

static void zcc_sha256_update(ZccSHA256_CTX *ctx, const uint8_t data[], size_t len) {
    size_t i;
    for (i = 0; i < len; ++i) {
        ctx->data[ctx->datalen] = data[i];
        ctx->datalen++;
        if (ctx->datalen == 64) {
            zcc_sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

static void zcc_sha256_final(ZccSHA256_CTX *ctx, uint8_t hash[]) {
    uint32_t i;
    i = ctx->datalen;
    if (ctx->datalen < 56) {
        ctx->data[i++] = 0x80;
        while (i < 56) ctx->data[i++] = 0x00;
    } else {
        ctx->data[i++] = 0x80;
        while (i < 64) ctx->data[i++] = 0x00;
        zcc_sha256_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    }
    ctx->bitlen += ctx->datalen * 8;
    ctx->data[56] = ctx->bitlen >> 56; ctx->data[57] = ctx->bitlen >> 48;
    ctx->data[58] = ctx->bitlen >> 40; ctx->data[59] = ctx->bitlen >> 32;
    ctx->data[60] = ctx->bitlen >> 24; ctx->data[61] = ctx->bitlen >> 16;
    ctx->data[62] = ctx->bitlen >> 8;  ctx->data[63] = ctx->bitlen;
    zcc_sha256_transform(ctx, ctx->data);
    for (i = 0; i < 4; ++i) {
        hash[i]      = (ctx->state[0] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 4]  = (ctx->state[1] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 8]  = (ctx->state[2] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 20] = (ctx->state[5] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 24] = (ctx->state[6] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 28] = (ctx->state[7] >> (24 - i * 8)) & 0x000000ff;
    }
}

static int read_gguf_string(FILE *f, char *buf, size_t max_len) {
    uint64_t len = 0;
    if (fread(&len, 1, 8, f) != 8) return -1;
    if (len >= max_len) {
        if (fseek(f, len, SEEK_CUR) != 0) return -1;
        buf[0] = '\0';
        return 0;
    }
    if (fread(buf, 1, len, f) != len) return -1;
    buf[len] = '\0';
    return 0;
}

static int skip_metadata_value(FILE *f, uint32_t val_type) {
    if (val_type == 0 || val_type == 1) {
        return fseek(f, 1, SEEK_CUR);
    } else if (val_type == 2 || val_type == 3) {
        return fseek(f, 2, SEEK_CUR);
    } else if (val_type == 4 || val_type == 5 || val_type == 6 || val_type == 7) {
        return fseek(f, 4, SEEK_CUR);
    } else if (val_type == 10 || val_type == 11 || val_type == 12) {
        return fseek(f, 8, SEEK_CUR);
    } else if (val_type == 8) {
        uint64_t len;
        if (fread(&len, 1, 8, f) != 8) return -1;
        return fseek(f, len, SEEK_CUR);
    } else if (val_type == 9) {
        uint32_t elem_type;
        uint64_t count;
        if (fread(&elem_type, 1, 4, f) != 4) return -1;
        if (fread(&count, 1, 8, f) != 8) return -1;
        uint64_t i;
        for (i = 0; i < count; i++) {
            if (skip_metadata_value(f, elem_type) != 0) return -1;
        }
        return 0;
    }
    return -1;
}

static uint64_t compute_nbytes(uint32_t dtype, uint32_t rank, const uint64_t *shape) {
    uint64_t total_elements = 1;
    uint32_t i;
    for (i = 0; i < rank; i++) {
        total_elements *= shape[i];
    }
    if (dtype == 0) return total_elements * 4;
    if (dtype == 1) return total_elements * 2;
    if (dtype == 2) return ((total_elements + 31) / 32) * 18;
    if (dtype == 3) return ((total_elements + 31) / 32) * 20;
    if (dtype == 6) return ((total_elements + 31) / 32) * 22;
    if (dtype == 7) return ((total_elements + 31) / 32) * 24;
    if (dtype == 8) return ((total_elements + 31) / 32) * 34;
    return 0;
}

static void compute_merkle_root(const uint8_t *leaf_hashes, uint32_t leaf_count, uint8_t *root_out) {
    if (leaf_count == 0) {
        memset(root_out, 0, 32);
        return;
    }
    if (leaf_count == 1) {
        memcpy(root_out, leaf_hashes, 32);
        return;
    }

    uint32_t level_size = leaf_count;
    uint8_t *level = malloc(level_size * 32);
    if (!level) {
        memset(root_out, 0, 32);
        return;
    }
    memcpy(level, leaf_hashes, level_size * 32);

    while (level_size > 1) {
        uint32_t next_level_size = (level_size + 1) / 2;
        uint8_t *next_level = malloc(next_level_size * 32);
        if (!next_level) {
            free(level);
            memset(root_out, 0, 32);
            return;
        }
        uint32_t i;
        for (i = 0; i < level_size; i += 2) {
            ZccSHA256_CTX ctx;
            zcc_sha256_init(&ctx);
            zcc_sha256_update(&ctx, level + i * 32, 32);
            if (i + 1 < level_size) {
                zcc_sha256_update(&ctx, level + (i + 1) * 32, 32);
            } else {
                uint8_t zero_hash[32];
                memset(zero_hash, 0, 32);
                zcc_sha256_update(&ctx, zero_hash, 32);
            }
            zcc_sha256_final(&ctx, next_level + (i / 2) * 32);
        }
        free(level);
        level = next_level;
        level_size = next_level_size;
    }

    memcpy(root_out, level, 32);
    free(level);
}

int zcc_verify_tensor_manifest(
    const void *elf_attest_base,
    size_t elf_attest_size,
    const char *gguf_path,
    ZccTensorVerifyReport *report
) {
    if (!report) return -1;
    memset(report, 0, sizeof(*report));
    uint8_t buf[4096];

    if (!elf_attest_base || elf_attest_size < sizeof(ZccTensorAttestHeader)) {
        report->status = ZCC_TENSOR_ERR_LAYOUT_NONCANONICAL;
        snprintf(report->error_msg, sizeof(report->error_msg), "Attestation base or size too small");
        return -1;
    }

    const ZccTensorAttestHeader *hdr = (const ZccTensorAttestHeader *)elf_attest_base;
    if (hdr->magic != ZCC_TENSOR_ATTEST_MAGIC) {
        report->status = ZCC_TENSOR_ERR_LAYOUT_NONCANONICAL;
        snprintf(report->error_msg, sizeof(report->error_msg), "Invalid magic: 0x%llx", (unsigned long long)hdr->magic);
        return -1;
    }
    if (hdr->schema_version != ZCC_TENSOR_ATTEST_VERSION) {
        report->status = ZCC_TENSOR_ERR_LAYOUT_NONCANONICAL;
        snprintf(report->error_msg, sizeof(report->error_msg), "Unsupported schema version: %u", hdr->schema_version);
        return -1;
    }
    if (hdr->verifier_version != 1) {
        report->status = ZCC_TENSOR_ERR_LAYOUT_NONCANONICAL;
        snprintf(report->error_msg, sizeof(report->error_msg), "Unsupported verifier version: %u", hdr->verifier_version);
        return -1;
    }

    /* Open GGUF to compute Merkle tree root and full SHA-256 */
    FILE *f = fopen(gguf_path, "rb");
    if (!f) {
        report->status = ZCC_TENSOR_ERR_MISSING;
        snprintf(report->error_msg, sizeof(report->error_msg), "Failed to open GGUF file '%s'", gguf_path);
        return -1;
    }

    uint32_t leaf_size = hdr->leaf_size ? hdr->leaf_size : 1024 * 1024;
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint32_t expected_leaf_count = (file_size + leaf_size - 1) / leaf_size;
    if (expected_leaf_count != hdr->leaf_count) {
        fclose(f);
        report->status = ZCC_TENSOR_ERR_COUNT_MISMATCH;
        snprintf(report->error_msg, sizeof(report->error_msg), "Merkle leaf count mismatch: expected %u, got %u", hdr->leaf_count, expected_leaf_count);
        return -1;
    }

    uint8_t *computed_leaf_hashes = calloc(expected_leaf_count, 32);
    if (!computed_leaf_hashes) {
        fclose(f);
        report->status = ZCC_TENSOR_ERR_LAYOUT_NONCANONICAL;
        snprintf(report->error_msg, sizeof(report->error_msg), "Failed to allocate memory for leaf hashes");
        return -1;
    }

    ZccSHA256_CTX global_sha_ctx;
    zcc_sha256_init(&global_sha_ctx);

    uint8_t *leaf_buf = malloc(leaf_size);
    if (!leaf_buf) {
        free(computed_leaf_hashes);
        fclose(f);
        report->status = ZCC_TENSOR_ERR_LAYOUT_NONCANONICAL;
        snprintf(report->error_msg, sizeof(report->error_msg), "Failed to allocate memory for leaf buffer");
        return -1;
    }

    uint32_t leaf_idx = 0;
    size_t bytes_read;
    while ((bytes_read = fread(leaf_buf, 1, leaf_size, f)) > 0) {
        zcc_sha256_update(&global_sha_ctx, leaf_buf, bytes_read);

        ZccSHA256_CTX leaf_sha_ctx;
        zcc_sha256_init(&leaf_sha_ctx);
        zcc_sha256_update(&leaf_sha_ctx, leaf_buf, bytes_read);
        zcc_sha256_final(&leaf_sha_ctx, computed_leaf_hashes + leaf_idx * 32);

        leaf_idx++;
    }
    free(leaf_buf);

    uint8_t actual_gguf_sha[32];
    zcc_sha256_final(&global_sha_ctx, actual_gguf_sha);

    if (memcmp(actual_gguf_sha, hdr->gguf_sha256, 32) != 0) {
        free(computed_leaf_hashes);
        fclose(f);
        report->status = ZCC_TENSOR_ERR_SHA256_MISMATCH;
        snprintf(report->error_msg, sizeof(report->error_msg), "GGUF SHA-256 mismatch");
        return -1;
    }

    uint8_t actual_merkle_root[32];
    compute_merkle_root(computed_leaf_hashes, expected_leaf_count, actual_merkle_root);

    if (memcmp(actual_merkle_root, hdr->merkle_root, 32) != 0) {
        free(computed_leaf_hashes);
        fclose(f);
        report->status = ZCC_TENSOR_ERR_SHA256_MISMATCH;
        snprintf(report->error_msg, sizeof(report->error_msg), "GGUF Merkle root mismatch");
        return -1;
    }

    if (hdr->leaf_hashes_size > 0 && hdr->leaf_hashes_offset > 0) {
        const uint8_t *embedded_leaf_hashes = (const uint8_t *)elf_attest_base + hdr->leaf_hashes_offset;
        if (hdr->leaf_hashes_offset + hdr->leaf_hashes_size > elf_attest_size) {
            free(computed_leaf_hashes);
            fclose(f);
            report->status = ZCC_TENSOR_ERR_LAYOUT_NONCANONICAL;
            snprintf(report->error_msg, sizeof(report->error_msg), "Embedded leaf hashes offset/size out of bounds");
            return -1;
        }
        if (hdr->leaf_hashes_size != expected_leaf_count * 32) {
            free(computed_leaf_hashes);
            fclose(f);
            report->status = ZCC_TENSOR_ERR_LAYOUT_NONCANONICAL;
            snprintf(report->error_msg, sizeof(report->error_msg), "Embedded leaf hashes size mismatch");
            return -1;
        }
        if (memcmp(computed_leaf_hashes, embedded_leaf_hashes, expected_leaf_count * 32) != 0) {
            free(computed_leaf_hashes);
            fclose(f);
            report->status = ZCC_TENSOR_ERR_SHA256_MISMATCH;
            snprintf(report->error_msg, sizeof(report->error_msg), "Individual leaf hash mismatch against embedded attestation");
            return -1;
        }
    }

    free(computed_leaf_hashes);

    /* Rewind GGUF to read metadata & tensor headers */
    fseek(f, 0, SEEK_SET);
    uint32_t magic_val = 0;
    uint32_t version_val = 0;
    uint64_t tensor_count_val = 0;
    uint64_t metadata_kv_count_val = 0;
    if (fread(&magic_val, 1, 4, f) != 4 ||
        fread(&version_val, 1, 4, f) != 4 ||
        fread(&tensor_count_val, 1, 8, f) != 8 ||
        fread(&metadata_kv_count_val, 1, 8, f) != 8) {
        fclose(f);
        report->status = ZCC_TENSOR_ERR_LAYOUT_NONCANONICAL;
        snprintf(report->error_msg, sizeof(report->error_msg), "Truncated GGUF header");
        return -1;
    }

    if (magic_val != 0x46554747) { // "GGUF" in LE
        fclose(f);
        report->status = ZCC_TENSOR_ERR_LAYOUT_NONCANONICAL;
        snprintf(report->error_msg, sizeof(report->error_msg), "Invalid GGUF magic");
        return -1;
    }

    if (version_val != hdr->gguf_version) {
        fclose(f);
        report->status = ZCC_TENSOR_ERR_LAYOUT_NONCANONICAL;
        snprintf(report->error_msg, sizeof(report->error_msg), "GGUF version mismatch: expected %u, got %u", hdr->gguf_version, version_val);
        return -1;
    }

    if (tensor_count_val != hdr->record_count) {
        fclose(f);
        report->status = ZCC_TENSOR_ERR_COUNT_MISMATCH;
        snprintf(report->error_msg, sizeof(report->error_msg), "Tensor count mismatch: expected %u, got %llu", hdr->record_count, (unsigned long long)tensor_count_val);
        return -1;
    }

    /* Skip metadata keys and values */
    uint64_t i;
    for (i = 0; i < metadata_kv_count_val; i++) {
        char key_buf[256];
        if (read_gguf_string(f, key_buf, sizeof(key_buf)) != 0) {
            fclose(f);
            report->status = ZCC_TENSOR_ERR_LAYOUT_NONCANONICAL;
            snprintf(report->error_msg, sizeof(report->error_msg), "Truncated metadata key");
            return -1;
        }
        uint32_t val_type = 0;
        if (fread(&val_type, 1, 4, f) != 4) {
            fclose(f);
            report->status = ZCC_TENSOR_ERR_LAYOUT_NONCANONICAL;
            snprintf(report->error_msg, sizeof(report->error_msg), "Truncated metadata value type");
            return -1;
        }
        if (skip_metadata_value(f, val_type) != 0) {
            fclose(f);
            report->status = ZCC_TENSOR_ERR_LAYOUT_NONCANONICAL;
            snprintf(report->error_msg, sizeof(report->error_msg), "Failed to skip metadata value");
            return -1;
        }
    }

    /* Read GGUF Tensor Infos */
    typedef struct {
        char name[128];
        uint32_t dtype;
        uint32_t rank;
        uint64_t shape[4];
        uint64_t offset;
    } TempGgufInfo;
    TempGgufInfo *gguf_tensors = calloc(tensor_count_val, sizeof(TempGgufInfo));

    for (i = 0; i < tensor_count_val; i++) {
        if (read_gguf_string(f, gguf_tensors[i].name, sizeof(gguf_tensors[i].name)) != 0) {
            free(gguf_tensors);
            fclose(f);
            report->status = ZCC_TENSOR_ERR_LAYOUT_NONCANONICAL;
            snprintf(report->error_msg, sizeof(report->error_msg), "Truncated tensor name");
            return -1;
        }
        uint32_t n_dims = 0;
        if (fread(&n_dims, 1, 4, f) != 4) {
            free(gguf_tensors);
            fclose(f);
            report->status = ZCC_TENSOR_ERR_LAYOUT_NONCANONICAL;
            snprintf(report->error_msg, sizeof(report->error_msg), "Truncated tensor dims count");
            return -1;
        }
        if (n_dims > 4) {
            free(gguf_tensors);
            fclose(f);
            report->status = ZCC_TENSOR_ERR_SHAPE_MISMATCH;
            snprintf(report->error_msg, sizeof(report->error_msg), "Unsupported tensor rank: %u", n_dims);
            return -1;
        }
        gguf_tensors[i].rank = n_dims;
        if (fread(gguf_tensors[i].shape, 1, 8 * n_dims, f) != 8 * n_dims) {
            free(gguf_tensors);
            fclose(f);
            report->status = ZCC_TENSOR_ERR_LAYOUT_NONCANONICAL;
            snprintf(report->error_msg, sizeof(report->error_msg), "Truncated tensor shape");
            return -1;
        }
        if (fread(&gguf_tensors[i].dtype, 1, 4, f) != 4 ||
            fread(&gguf_tensors[i].offset, 1, 8, f) != 8) {
            free(gguf_tensors);
            fclose(f);
            report->status = ZCC_TENSOR_ERR_LAYOUT_NONCANONICAL;
            snprintf(report->error_msg, sizeof(report->error_msg), "Truncated tensor type/offset");
            return -1;
        }
    }

    uint64_t tensor_data_start = (ftell(f) + 31) & ~31;

    /* Verify each ELF record against GGUF */
    const ZccTensorAttestRecord *records = (const ZccTensorAttestRecord *)((const uint8_t *)elf_attest_base + hdr->records_offset);
    uint32_t r;
    for (r = 0; r < hdr->record_count; r++) {
        const ZccTensorAttestRecord *rec = &records[r];
        /* Find matching tensor by name in GGUF */
        int found_idx = -1;
        for (i = 0; i < tensor_count_val; i++) {
            if (strcmp(gguf_tensors[i].name, rec->name) == 0) {
                found_idx = (int)i;
                break;
            }
        }
        if (found_idx == -1) {
            free(gguf_tensors);
            fclose(f);
            report->status = ZCC_TENSOR_ERR_MISSING;
            snprintf(report->error_msg, sizeof(report->error_msg), "Tensor '%s' missing in GGUF", rec->name);
            return -1;
        }

        const TempGgufInfo *g_tensor = &gguf_tensors[found_idx];

        /* Check dtype */
        if (g_tensor->dtype != rec->dtype) {
            free(gguf_tensors);
            fclose(f);
            report->status = ZCC_TENSOR_ERR_DTYPE_MISMATCH;
            snprintf(report->error_msg, sizeof(report->error_msg), "Tensor '%s' dtype mismatch", rec->name);
            return -1;
        }

        /* Check rank and shape */
        if (g_tensor->rank != rec->rank) {
            free(gguf_tensors);
            fclose(f);
            report->status = ZCC_TENSOR_ERR_SHAPE_MISMATCH;
            snprintf(report->error_msg, sizeof(report->error_msg), "Tensor '%s' rank mismatch", rec->name);
            return -1;
        }
        uint32_t d;
        for (d = 0; d < rec->rank; d++) {
            if (g_tensor->shape[d] != rec->shape[d]) {
                free(gguf_tensors);
                fclose(f);
                report->status = ZCC_TENSOR_ERR_SHAPE_MISMATCH;
                snprintf(report->error_msg, sizeof(report->error_msg), "Tensor '%s' dimension %u mismatch", rec->name, d);
                return -1;
            }
        }

        /* Check offset */
        if (g_tensor->offset != rec->offset) {
            free(gguf_tensors);
            fclose(f);
            report->status = ZCC_TENSOR_ERR_OFFSET_MISMATCH;
            snprintf(report->error_msg, sizeof(report->error_msg), "Tensor '%s' offset mismatch", rec->name);
            return -1;
        }

        /* Verify nbytes matches calculated nbytes */
        uint64_t calc_nbytes = compute_nbytes(rec->dtype, rec->rank, rec->shape);
        if (calc_nbytes != rec->nbytes) {
            free(gguf_tensors);
            fclose(f);
            report->status = ZCC_TENSOR_ERR_NBYTES_MISMATCH;
            snprintf(report->error_msg, sizeof(report->error_msg), "Tensor '%s' size/nbytes calculation mismatch", rec->name);
            return -1;
        }

        /* Seek and read tensor data to compute payload hash */
        uint64_t t_offset = tensor_data_start + rec->offset;
        if (fseek(f, t_offset, SEEK_SET) != 0) {
            free(gguf_tensors);
            fclose(f);
            report->status = ZCC_TENSOR_ERR_IO;
            snprintf(report->error_msg, sizeof(report->error_msg), "Failed to seek to tensor data for '%s'", rec->name);
            return -1;
        }

        ZccSHA256_CTX tensor_sha_ctx;
        zcc_sha256_init(&tensor_sha_ctx);
        uint64_t bytes_left = rec->nbytes;
        while (bytes_left > 0) {
            size_t to_read = bytes_left > sizeof(buf) ? sizeof(buf) : bytes_left;
            if (fread(buf, 1, to_read, f) != to_read) {
                free(gguf_tensors);
                fclose(f);
                report->status = ZCC_TENSOR_ERR_SHA256_MISMATCH;
                snprintf(report->error_msg, sizeof(report->error_msg), "Truncated data read for tensor '%s'", rec->name);
                return -1;
            }
            zcc_sha256_update(&tensor_sha_ctx, buf, to_read);
            bytes_left -= to_read;
        }
        uint8_t actual_tensor_hash[32];
        zcc_sha256_final(&tensor_sha_ctx, actual_tensor_hash);

        if (memcmp(actual_tensor_hash, rec->tensor_sha256, 32) != 0) {
            free(gguf_tensors);
            fclose(f);
            report->status = ZCC_TENSOR_ERR_SHA256_MISMATCH;
            snprintf(report->error_msg, sizeof(report->error_msg), "Tensor '%s' data hash mismatch", rec->name);
            return -1;
        }
    }

    free(gguf_tensors);
    fclose(f);
    report->status = ZCC_TENSOR_OK;
    return 0;
}
