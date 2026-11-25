/*
 * ZKAEDI ZCC // Sovereign Win64 GPU GGUF Transformer Inference Engine (Track 9)
 * ===============================================================================
 * Compiled natively to Win64 PE32+ by ZCC with ZERO MSVC, ZERO MinGW,
 * ZERO GCC, ZERO binutils, ZERO nvcc, and ZERO CUDA Toolkit.
 * Direct PE32+ imports: C:\Windows\System32\{msvcrt,nvcuda}.dll ONLY.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#ifdef ZCC_EMBEDDED_AOT_PTX
#include "embedded_aot_ptx.h"
#else
#include "gpu_llm_kernels_ptx.h"
#endif

#ifdef ZCC_EMBEDDED_GGUF
#include "embedded_gguf_blob.h"
#endif

#ifdef ZCC_SEALED_OVERLAY_GGUF
#include "embedded_gguf_seal.h"
#endif

/* MSVCRT 64-bit file length/slice & module API (C:\Windows\System32\msvcrt.dll) */
extern int _open(const char *path, int oflag);
extern int _close(int fd);
extern int _read(int fd, void *buf, unsigned int count);
extern long long _lseeki64(int fd, long long offset, int origin);
extern int _get_pgmptr(char **pValue);

/* CUDA Driver API -> C:\Windows\System32\nvcuda.dll */
typedef struct {
    void *func;
    unsigned int gridDimX;
    unsigned int gridDimY;
    unsigned int gridDimZ;
    unsigned int blockDimX;
    unsigned int blockDimY;
    unsigned int blockDimZ;
    unsigned int sharedMemBytes;
    unsigned int _pad0;
    void **kernelParams;
    void **extra;
} ZccCudaKernelNodeParams;

extern int cuInit(unsigned int Flags);
extern int cuDriverGetVersion(int *driverVersion);
extern int cuDeviceGetCount(int *count);
extern int cuDeviceGet(int *device, int ordinal);
extern int cuDeviceGetName(char *name, int len, int dev);
extern int cuDeviceGetAttribute(int *pi, int attrib, int dev);
extern int cuDeviceTotalMem_v2(size_t *bytes, int dev);
extern int cuCtxCreate_v2(void **pctx, unsigned int flags, int dev);
extern int cuCtxDestroy_v2(void *ctx);
extern int cuCtxSynchronize(void);
extern int cuModuleLoadDataEx(void **module, const void *image, unsigned int numOptions, int *options, void **optionValues);
extern int cuModuleUnload(void *hmod);
extern int cuModuleGetFunction(void **hfunc, void *hmod, const char *name);
extern int cuMemAlloc_v2(uint64_t *dptr, size_t bytesize);
extern int cuMemFree_v2(uint64_t dptr);
extern int cuMemHostAlloc(void **pp, size_t bytesize, unsigned int Flags);
extern int cuMemHostGetDevicePointer_v2(uint64_t *pdptr, void *p, unsigned int Flags);
extern int cuMemFreeHost(void *p);
extern int cuMemGetInfo_v2(size_t *free_bytes, size_t *total_bytes);
extern int cuMemsetD8_v2(uint64_t dstDevice, unsigned char uc, size_t N);
extern int cuMemcpyHtoD_v2(uint64_t dstDevice, const void *srcHost, size_t ByteCount);
extern int cuMemcpyHtoDAsync_v2(uint64_t dstDevice, const void *srcHost, size_t ByteCount, void *hStream);
extern int cuMemcpyDtoH_v2(void *dstHost, uint64_t srcDevice, size_t ByteCount);
extern int cuStreamCreate(void **phStream, unsigned int Flags);
extern int cuStreamDestroy_v2(void *hStream);
extern int cuStreamWaitEvent(void *hStream, void *hEvent, unsigned int Flags);
extern int cuLaunchKernel(
    void *f,
    unsigned int gridDimX, unsigned int gridDimY, unsigned int gridDimZ,
    unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ,
    unsigned int sharedMemBytes,
    void *hStream,
    void **kernelParams,
    void **extra
);
extern int cuGraphCreate(void **phGraph, unsigned int flags);
extern int cuGraphAddKernelNode(
    void **phGraphNode,
    void *hGraph,
    void *const *dependencies,
    size_t numDependencies,
    const ZccCudaKernelNodeParams *nodeParams
);
extern int cuGraphInstantiateWithFlags(void **phGraphExec, void *hGraph, unsigned long long flags);
extern int cuGraphLaunch(void *hGraphExec, void *hStream);
extern int cuGraphDestroy(void *hGraph);
extern int cuGraphExecDestroy(void *hGraphExec);
extern int cuEventCreate(void **phEvent, unsigned int Flags);
extern int cuEventRecord(void *hEvent, void *hStream);
extern int cuEventSynchronize(void *hEvent);
extern int cuEventElapsedTime(float *pMilliseconds, void *hStart, void *hEnd);
extern int cuEventDestroy_v2(void *hEvent);
extern int cuGetErrorName(int error, const char **pStr);
extern int cuGetErrorString(int error, const char **pStr);

static char g_zcc_llm_err[512];
static int g_zcc_llm_failed = 0;

static void zcc_llm_set_err(const char *msg) {
    if (!msg) msg = "unknown error";
    snprintf(g_zcc_llm_err, sizeof(g_zcc_llm_err), "%s", msg);
    g_zcc_llm_failed = 1;
}

static void check_cu(int rc, const char *where) {
    if (rc != 0) {
        const char *ename = "UNKNOWN";
        const char *edesc = "";
        cuGetErrorName(rc, &ename);
        cuGetErrorString(rc, &edesc);
#ifdef ZCC_GPU_LLM_LIBRARY
        char msg[512];
        snprintf(msg, sizeof(msg), "CUDA Driver error in %s: rc=%d (%s: %s)",
                 where ? where : "?", rc, ename ? ename : "?", edesc ? edesc : "");
        zcc_llm_set_err(msg);
        return;
#else
        fprintf(stderr, "CUDA Driver error in %s: rc=%d (%s: %s)\n",
                where, rc, ename ? ename : "?", edesc ? edesc : "");
        exit(1);
#endif
    }
}

/* Compact FIPS 180-4 SHA-256 for streaming >4 GiB PE overlay verification */
typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t buf[64];
    size_t buf_len;
} ZccSha256Ctx;

#define ZCC_ROTR32(x, n) (((x) >> (n)) | ((x) << (32U - (n))))

static void zcc_sha256_transform(ZccSha256Ctx *ctx, const uint8_t block[64]) {
    static const uint32_t k[64] = {
        0x428A2F98U, 0x71374491U, 0xB5C0FBCFU, 0xE9B5DBA5U,
        0x3956C25BU, 0x59F111F1U, 0x923F82A4U, 0xAB1C5ED5U,
        0xD807AA98U, 0x12835B01U, 0x243185BEU, 0x550C7DC3U,
        0x72BE5D74U, 0x80DEB1FEU, 0x9BDC06A7U, 0xC19BF174U,
        0xE49B69C1U, 0xEFBE4786U, 0x0FC19DC6U, 0x240CA1CCU,
        0x2DE92C6FU, 0x4A7484AAU, 0x5CB0A9DCU, 0x76F988DAU,
        0x983E5152U, 0xA831C66DU, 0xB00327C8U, 0xBF597FC7U,
        0xC6E00BF3U, 0xD5A79147U, 0x06CA6351U, 0x14292967U,
        0x27B70A85U, 0x2E1B2138U, 0x4D2C6DFCU, 0x53380D13U,
        0x650A7354U, 0x766A0ABBU, 0x81C2C92EU, 0x92722C85U,
        0xA2BFE8A1U, 0xA81A664BU, 0xC24B8B70U, 0xC76C51A3U,
        0xD192E819U, 0xD6990624U, 0xF40E3585U, 0x106AA070U,
        0x19A4C116U, 0x1E376C08U, 0x2748774CU, 0x34B0BCB5U,
        0x391C0CB3U, 0x4ED8AA4AU, 0x5B9CCA4FU, 0x682E6FF3U,
        0x748F82EEU, 0x78A5636FU, 0x84C87814U, 0x8CC70208U,
        0x90BEFFFAU, 0xA4506CEBU, 0xBEF9A3F7U, 0xC67178F2U
    };
    uint32_t w[64];
    for (unsigned int i = 0U; i < 16U; i++) {
        size_t j = (size_t)i * 4U;
        w[i] = ((uint32_t)block[j] << 24) | ((uint32_t)block[j + 1U] << 16) |
               ((uint32_t)block[j + 2U] << 8) | (uint32_t)block[j + 3U];
    }
    for (unsigned int i = 16U; i < 64U; i++) {
        uint32_t s0 = ZCC_ROTR32(w[i - 15U], 7U) ^ ZCC_ROTR32(w[i - 15U], 18U) ^ (w[i - 15U] >> 3U);
        uint32_t s1 = ZCC_ROTR32(w[i - 2U], 17U) ^ ZCC_ROTR32(w[i - 2U], 19U) ^ (w[i - 2U] >> 10U);
        w[i] = w[i - 16U] + s0 + w[i - 7U] + s1;
    }
    uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3];
    uint32_t e = ctx->state[4], f = ctx->state[5], g = ctx->state[6], h = ctx->state[7];
    for (unsigned int i = 0U; i < 64U; i++) {
        uint32_t S1 = ZCC_ROTR32(e, 6U) ^ ZCC_ROTR32(e, 11U) ^ ZCC_ROTR32(e, 25U);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + S1 + ch + k[i] + w[i];
        uint32_t S0 = ZCC_ROTR32(a, 2U) ^ ZCC_ROTR32(a, 13U) ^ ZCC_ROTR32(a, 22U);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;
        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void zcc_sha256_init(ZccSha256Ctx *ctx) {
    ctx->state[0] = 0x6A09E667U; ctx->state[1] = 0xBB67AE85U;
    ctx->state[2] = 0x3C6EF372U; ctx->state[3] = 0xA54FF53AU;
    ctx->state[4] = 0x510E527FU; ctx->state[5] = 0x9B05688CU;
    ctx->state[6] = 0x1F83D9ABU; ctx->state[7] = 0x5BE0CD19U;
    ctx->bitlen = 0ULL;
    ctx->buf_len = 0U;
}

static void zcc_sha256_update(ZccSha256Ctx *ctx, const uint8_t *data, size_t len) {
    size_t i = 0U;
    if (ctx->buf_len > 0U) {
        while (i < len && ctx->buf_len < 64U) {
            ctx->buf[ctx->buf_len++] = data[i++];
        }
        if (ctx->buf_len == 64U) {
            zcc_sha256_transform(ctx, ctx->buf);
            ctx->bitlen += 512ULL;
            ctx->buf_len = 0U;
        }
    }
    while (i + 64U <= len) {
        zcc_sha256_transform(ctx, data + i);
        ctx->bitlen += 512ULL;
        i += 64U;
    }
    while (i < len) {
        ctx->buf[ctx->buf_len++] = data[i++];
    }
}

static void zcc_sha256_final(ZccSha256Ctx *ctx, uint8_t out_hash[32]) {
    size_t i = ctx->buf_len;
    if (i < 56U) {
        ctx->buf[i++] = 0x80U;
        while (i < 56U) ctx->buf[i++] = 0U;
    } else {
        ctx->buf[i++] = 0x80U;
        while (i < 64U) ctx->buf[i++] = 0U;
        zcc_sha256_transform(ctx, ctx->buf);
        memset(ctx->buf, 0, 56U);
    }
    ctx->bitlen += (uint64_t)ctx->buf_len * 8ULL;
    for (int j = 0; j < 8; j++) {
        ctx->buf[56 + j] = (uint8_t)((ctx->bitlen >> (56 - j * 8)) & 0xFFULL);
    }
    zcc_sha256_transform(ctx, ctx->buf);
    for (int j = 0; j < 8; j++) {
        out_hash[j * 4 + 0] = (uint8_t)((ctx->state[j] >> 24) & 0xFFU);
        out_hash[j * 4 + 1] = (uint8_t)((ctx->state[j] >> 16) & 0xFFU);
        out_hash[j * 4 + 2] = (uint8_t)((ctx->state[j] >> 8) & 0xFFU);
        out_hash[j * 4 + 3] = (uint8_t)(ctx->state[j] & 0xFFU);
    }
}

static void sha256_to_hex(const uint8_t h[32], char out[65]) {
    static const char hx[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        out[i * 2] = hx[(h[i] >> 4) & 0xFU];
        out[i * 2 + 1] = hx[h[i] & 0xFU];
    }
    out[64] = '\0';
}

static void fmt_u64_dec(uint64_t v, char *buf, size_t cap) {
    char tmp[32];
    size_t n = 0U;
    if (cap == 0U) return;
    if (v == 0ULL) {
        buf[0] = '0';
        if (cap > 1U) buf[1] = '\0';
        return;
    }
    while (v > 0ULL && n < sizeof(tmp)) {
        tmp[n++] = (char)('0' + (int)(v % 10ULL));
        v /= 10ULL;
    }
    size_t out = (n < cap - 1U) ? n : (cap - 1U);
    for (size_t i = 0U; i < out; i++) buf[i] = tmp[n - 1U - i];
    buf[out] = '\0';
}

/* GGUF / GGML Constants */
#define ZCC_GGUF_MAGIC 0x46554747U
#define ZCC_GGML_F32   0U
#define ZCC_GGML_F16   1U
#define ZCC_GGML_Q4_0  2U
#define ZCC_GGML_Q8_0  8U
#define ZCC_GGML_Q4_K  12U
#define ZCC_GGML_Q6_K  14U
#define ZCC_GGML_BF16  30U

#define ZCC_MAX_LAYERS  256U
#define ZCC_MAX_TENSORS 16384U

#define ZCC_AOT_MAGIC_V1 0x5A4343414F543031ULL
#define ZCC_AOT_MAGIC_V2 0x5A4343414F543032ULL

typedef struct {
    char name[128];
    uint32_t n_dims;
    uint64_t dims[4];
    uint32_t ggml_type;
    uint64_t offset;
    uint64_t nbytes;
    uint64_t d_ptr;
    uint64_t dst_off;
    uint8_t on_host;
} GgufTensor;

typedef struct {
    uint64_t d_attn_norm;  uint64_t off_attn_norm;  uint32_t t_attn_norm;
    uint64_t d_wq;         uint64_t off_wq;         uint32_t t_wq;
    uint64_t d_wk;         uint64_t off_wk;         uint32_t t_wk;
    uint64_t d_wv;         uint64_t off_wv;         uint32_t t_wv;
    uint64_t d_wo;         uint64_t off_wo;         uint32_t t_wo;
    uint64_t d_bq;         uint64_t off_bq;         int has_bq;
    uint64_t d_bk;         uint64_t off_bk;         int has_bk;
    uint64_t d_bv;         uint64_t off_bv;         int has_bv;
    uint64_t d_ffn_norm;   uint64_t off_ffn_norm;   uint32_t t_ffn_norm;
    uint64_t d_wgate;      uint64_t off_wgate;      uint32_t t_wgate;
    uint64_t d_wup;        uint64_t off_wup;        uint32_t t_wup;
    uint64_t d_wdown;      uint64_t off_wdown;      uint32_t t_wdown;
} LayerWeights;

typedef struct {
    uint32_t n_layers;
    uint32_t dim;
    uint32_t hidden_dim;
    uint32_t n_heads;
    uint32_t n_kv_heads;
    uint32_t max_seq_len;
    float rms_eps;
    float rope_theta;
} ArchCfg;

/* Tokenizer data structures (ASCII Raw + GPT-2 BPE + SentencePiece) */
#define ZCC_TOK_ASCII_RAW 0
#define ZCC_TOK_GPT2_BPE  1
#define ZCC_TOK_SPM       2

typedef struct {
    int mode;
    char model_name[64];
    uint32_t vocab_size;
    char *pool;
    size_t pool_len;
    size_t pool_cap;
    uint32_t *tok_off;
    uint32_t *tok_len;
    float *scores;
    uint32_t n_scores;
    uint8_t *tok_type;
    uint32_t n_types;

    char *merge_pool;
    size_t merge_pool_len;
    size_t merge_pool_cap;
    uint32_t *merge_off;
    uint32_t *merge_len;
    uint32_t n_merges;

    uint32_t *tok_ht;
    uint32_t tok_ht_mask;

    uint64_t *merge_keys;
    uint32_t *merge_ranks;
    uint32_t merge_ht_mask;

    uint32_t *special_ids;
    uint32_t n_specials;

    uint32_t bos_id;  int has_bos;
    uint32_t eos_id;  int has_eos;
    uint32_t eot_id;  int has_eot;
    int add_bos;      int add_bos_explicit;
} GgufTokenizer;

typedef struct {
    char arch[64];
    unsigned int n_layers;
    unsigned int dim;
    unsigned int hidden_dim;
    unsigned int n_heads;
    unsigned int n_kv_heads;
    unsigned int head_dim;
    unsigned int kv_dim;
    unsigned int max_seq_len;
    unsigned int vocab_size;
    unsigned int align;
    float rms_eps;
    float rope_theta;
    uint32_t weight_type_primary;
    uint64_t data_start;
    uint64_t wblob_bytes;
    uint64_t file_fingerprint;
    int has_any_bias;

    GgufTensor *tensors;
    unsigned int num_tensors;

    uint64_t d_wblob;
    void *h_spill_blob;
    uint64_t d_spill_blob;
    uint64_t vram_wblob_bytes;
    uint64_t spill_wblob_bytes;
    uint64_t d_tok_emb;    uint64_t off_tok_emb;    uint32_t t_tok_emb;
    uint64_t d_out_norm;   uint64_t off_out_norm;   uint32_t t_out_norm;
    uint64_t d_wout;       uint64_t off_wout;       uint32_t t_wout;
    LayerWeights *layers;
    GgufTokenizer tok;
} LlamaModel;

static int mul_u64_checked(uint64_t a, uint64_t b, uint64_t *out) {
    if (a != 0ULL && b > (0xFFFFFFFFFFFFFFFFULL / a)) return -1;
    *out = a * b;
    return 0;
}

static uint64_t fnv1a_span64(const char *s, size_t len) {
    uint64_t h = 0xCBF29CE484222325ULL;
    for (size_t i = 0U; i < len; i++) {
        h ^= (uint64_t)(unsigned char)s[i];
        h *= 0x00000100000001B3ULL;
    }
    return h;
}

static void free_tokenizer(GgufTokenizer *t) {
    if (!t) return;
    if (t->pool) free(t->pool);
    if (t->tok_off) free(t->tok_off);
    if (t->tok_len) free(t->tok_len);
    if (t->scores) free(t->scores);
    if (t->tok_type) free(t->tok_type);
    if (t->merge_pool) free(t->merge_pool);
    if (t->merge_off) free(t->merge_off);
    if (t->merge_len) free(t->merge_len);
    if (t->tok_ht) free(t->tok_ht);
    if (t->merge_keys) free(t->merge_keys);
    if (t->merge_ranks) free(t->merge_ranks);
    if (t->special_ids) free(t->special_ids);
    memset(t, 0, sizeof(*t));
}

static void free_model_gpu_mem(LlamaModel *m) {
    if (!m) return;
    if (m->d_wblob) { cuMemFree_v2(m->d_wblob); m->d_wblob = 0ULL; }
    if (m->h_spill_blob) { cuMemFreeHost(m->h_spill_blob); m->h_spill_blob = NULL; }
    m->d_spill_blob = 0ULL;
    m->vram_wblob_bytes = 0ULL;
    m->spill_wblob_bytes = 0ULL;
}

static void free_llama_model(LlamaModel *m) {
    if (!m) return;
    if (m->tensors) { free(m->tensors); m->tensors = NULL; }
    if (m->layers) { free(m->layers); m->layers = NULL; }
    free_tokenizer(&m->tok);
}

/* Unified 64-bit GGUF stream (file, 64-bit PE overlay slice, or embedded buffer) */
typedef struct {
    FILE *fp;
    int fd;
    int use_fd_slice;
    int do_sha256;
    uint64_t slice_rem;
    const uint8_t *mem;
    size_t mem_len;
    size_t mem_pos;
    uint8_t *buf;
    size_t cap;
    size_t fill;
    size_t idx;
    uint64_t pos;
    uint64_t fsz64;
    uint64_t fp64;
    ZccSha256Ctx sha_ctx;
} GgufStream;

static size_t zcc_fd_read_exact(int fd, void *dst, size_t n) {
    uint8_t *p = (uint8_t *)dst;
    size_t got = 0U;
    while (got < n) {
        size_t chunk = n - got;
        unsigned int ask = (chunk > 65536U) ? 65536U : (unsigned int)chunk;
        int r = _read(fd, p + got, ask);
        if (r <= 0) break;
        got += (size_t)r;
    }
    return got;
}

static uint32_t read_le32(const uint8_t *b) {
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static uint64_t read_le64(const uint8_t *b) {
    return ((uint64_t)read_le32(b + 4) << 32) | (uint64_t)read_le32(b);
}

static uint32_t fnv1a_bytes32(const uint8_t *data, size_t n) {
    uint32_t h = 2166136261U;
    for (size_t i = 0U; i < n; i++) {
        h ^= (uint32_t)data[i];
        h *= 16777619U;
    }
    return h;
}

static int probe_sealed_pe_overlay(
    const char *self_path,
    uint64_t expected_len,
    const uint8_t expected_sha256[32],
    uint64_t *out_off,
    uint64_t *out_len
) {
    if (!self_path || !self_path[0]) {
        fprintf(stderr, "TAMPER_DETECTED: missing executable path for PE overlay\n");
        return -1;
    }
    int fd = _open(self_path, 0x8000);
    if (fd < 0) {
        fprintf(stderr, "TAMPER_DETECTED: cannot open self binary '%s'\n", self_path);
        return -1;
    }
    long long exe_sz_ll = _lseeki64(fd, 0LL, 2);
    if (exe_sz_ll < 576LL) {
        _close(fd);
        fprintf(stderr, "TAMPER_DETECTED: sealed executable truncated\n");
        return -1;
    }
    uint64_t exe_sz = (uint64_t)exe_sz_ll;
    if (_lseeki64(fd, (long long)(exe_sz - 64ULL), 0) < 0LL) {
        _close(fd);
        fprintf(stderr, "TAMPER_DETECTED: footer seek failed\n");
        return -1;
    }
    uint8_t ftr[64];
    if (zcc_fd_read_exact(fd, ftr, 64U) != 64U) {
        _close(fd);
        fprintf(stderr, "TAMPER_DETECTED: cannot read ZCCGGUF1 footer\n");
        return -1;
    }
    _close(fd);

    if (memcmp(ftr, "ZCCGGUF1", 8U) != 0) {
        fprintf(stderr, "TAMPER_DETECTED: invalid PE overlay footer magic\n");
        return -1;
    }
    uint32_t calc_fnv = fnv1a_bytes32(ftr, 60U);
    uint32_t stored_fnv = read_le32(ftr + 60);
    if (calc_fnv != stored_fnv) {
        fprintf(stderr, "TAMPER_DETECTED: PE overlay footer FNV-1a checksum mismatch\n");
        return -1;
    }
    uint64_t ov_off = read_le64(ftr + 8);
    uint64_t ov_len = read_le64(ftr + 16);
    uint32_t ov_flags = read_le32(ftr + 56);
    if (ov_flags != 1U || ov_off < 512ULL || ov_off > exe_sz - 64ULL ||
        ov_len != (exe_sz - 64ULL) - ov_off || ov_len != expected_len) {
        fprintf(stderr, "TAMPER_DETECTED: PE overlay bounds/length mismatch\n");
        return -1;
    }
    if (memcmp(ftr + 24, expected_sha256, 32U) != 0) {
        fprintf(stderr, "TAMPER_DETECTED: PE overlay footer SHA-256 diverges from compiled seal\n");
        return -1;
    }
    *out_off = ov_off;
    *out_len = ov_len;
    return 0;
}

static int get_file_size64(const char *path, uint64_t *out_sz) {
    int fd = _open(path, 0x8000); /* _O_RDONLY | _O_BINARY */
    if (fd < 0) return -1;
    long long sz = _lseeki64(fd, 0LL, 2);
    _close(fd);
    if (sz < 0LL) return -1;
    *out_sz = (uint64_t)sz;
    return 0;
}

static int gs_open_file(GgufStream *s, const char *path) {
    memset(s, 0, sizeof(*s));
    s->fd = -1;
    if (get_file_size64(path, &s->fsz64) != 0) return -1;
    s->fp = fopen(path, "rb");
    if (!s->fp) return -1;
    s->cap = 262144U;
    s->buf = (uint8_t *)malloc(s->cap);
    if (!s->buf) { fclose(s->fp); s->fp = NULL; return -1; }
    s->fp64 = 0xCBF29CE484222325ULL;
    return 0;
}

static int gs_open_file_slice(GgufStream *s, const char *path, uint64_t offset, uint64_t length, int do_sha256) {
    memset(s, 0, sizeof(*s));
    s->fd = -1;
    int fd = _open(path, 0x8000);
    if (fd < 0) return -1;
    long long sk = _lseeki64(fd, (long long)offset, 0);
    if (sk < 0LL || (uint64_t)sk != offset) {
        _close(fd);
        return -1;
    }
    s->fd = fd;
    s->use_fd_slice = 1;
    s->do_sha256 = do_sha256;
    s->slice_rem = length;
    s->fsz64 = length;
    s->cap = 262144U;
    s->buf = (uint8_t *)malloc(s->cap);
    if (!s->buf) {
        _close(fd);
        s->fd = -1;
        return -1;
    }
    s->fp64 = 0xCBF29CE484222325ULL;
    if (do_sha256) zcc_sha256_init(&s->sha_ctx);
    return 0;
}

static int gs_open_mem(GgufStream *s, const uint8_t *data, size_t len) {
    memset(s, 0, sizeof(*s));
    s->fd = -1;
    s->mem = data;
    s->mem_len = len;
    s->fsz64 = (uint64_t)len;
    s->cap = 262144U;
    s->buf = (uint8_t *)malloc(s->cap);
    if (!s->buf) return -1;
    s->fp64 = 0xCBF29CE484222325ULL;
    return 0;
}

static void gs_close(GgufStream *s) {
    if (s->fp) { fclose(s->fp); s->fp = NULL; }
    if (s->use_fd_slice && s->fd >= 0) { _close(s->fd); s->fd = -1; }
    if (s->buf) { free(s->buf); s->buf = NULL; }
}

static int gs_refill(GgufStream *s) {
    size_t got = 0U;
    if (s->mem) {
        if (s->mem_pos >= s->mem_len) return -1;
        size_t avail = s->mem_len - s->mem_pos;
        got = (avail < s->cap) ? avail : s->cap;
        memcpy(s->buf, s->mem + s->mem_pos, got);
        s->mem_pos += got;
    } else if (s->use_fd_slice) {
        if (s->slice_rem == 0ULL) return -1;
        size_t want = (s->slice_rem < (uint64_t)s->cap) ? (size_t)s->slice_rem : s->cap;
        got = zcc_fd_read_exact(s->fd, s->buf, want);
        s->slice_rem -= (uint64_t)got;
    } else {
        got = fread(s->buf, 1U, s->cap, s->fp);
    }
    if (got == 0U) return -1;
    uint64_t h = s->fp64;
    const uint8_t *p = s->buf;
    size_t i = 0U;
    while (i + 4U <= got) {
        h ^= (uint64_t)p[i];      h *= 0x00000100000001B3ULL;
        h ^= (uint64_t)p[i + 1U]; h *= 0x00000100000001B3ULL;
        h ^= (uint64_t)p[i + 2U]; h *= 0x00000100000001B3ULL;
        h ^= (uint64_t)p[i + 3U]; h *= 0x00000100000001B3ULL;
        i += 4U;
    }
    while (i < got) {
        h ^= (uint64_t)p[i];
        h *= 0x00000100000001B3ULL;
        i++;
    }
    s->fp64 = h;
    if (s->do_sha256) zcc_sha256_update(&s->sha_ctx, s->buf, got);
    s->idx = 0U;
    s->fill = got;
    return 0;
}

static int gs_rd_bytes(GgufStream *s, void *dst, size_t n) {
    uint8_t *out = (uint8_t *)dst;
    size_t rem = n;
    while (rem > 0U) {
        if (s->idx >= s->fill) {
            if (gs_refill(s) != 0) return -1;
        }
        size_t avail = s->fill - s->idx;
        size_t take = (rem < avail) ? rem : avail;
        if (out) { memcpy(out, s->buf + s->idx, take); out += take; }
        s->idx += take;
        s->pos += (uint64_t)take;
        rem -= take;
    }
    return 0;
}

static int gs_skip_u64(GgufStream *s, uint64_t n) {
    while (n > 0ULL) {
        if (s->idx >= s->fill) {
            if (gs_refill(s) != 0) return -1;
        }
        uint64_t avail = (uint64_t)(s->fill - s->idx);
        uint64_t take = (n < avail) ? n : avail;
        s->idx += (size_t)take;
        s->pos += take;
        n -= take;
    }
    return 0;
}

static void gs_drain_eof(GgufStream *s) {
    if (s->idx < s->fill) {
        uint64_t avail = (uint64_t)(s->fill - s->idx);
        s->idx = s->fill;
        s->pos += avail;
    }
    while (gs_refill(s) == 0) {
        s->pos += (uint64_t)s->fill;
        s->idx = s->fill;
    }
}

static int gs_rd_u8(GgufStream *s, uint8_t *v) { return gs_rd_bytes(s, v, 1U); }

static int gs_rd_u32(GgufStream *s, uint32_t *v) {
    uint8_t b[4];
    if (gs_rd_bytes(s, b, 4U) != 0) return -1;
    *v = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    return 0;
}

static int gs_rd_u64(GgufStream *s, uint64_t *v) {
    uint32_t lo = 0U, hi = 0U;
    if (gs_rd_u32(s, &lo) != 0 || gs_rd_u32(s, &hi) != 0) return -1;
    *v = ((uint64_t)hi << 32) | (uint64_t)lo;
    return 0;
}

static int gs_rd_f32(GgufStream *s, float *v) {
    uint32_t u = 0U;
    if (gs_rd_u32(s, &u) != 0) return -1;
    memcpy(v, &u, 4U);
    return 0;
}

static int gs_rd_str_cap(GgufStream *s, char *dst, size_t cap) {
    uint64_t len = 0ULL;
    if (gs_rd_u64(s, &len) != 0) return -1;
    if (len > 16777216ULL) return -1;
    uint64_t cpy64 = (cap > 1U && len < (uint64_t)(cap - 1U)) ? len : ((cap > 1U) ? (uint64_t)(cap - 1U) : 0ULL);
    if (cpy64 > 0ULL) {
        if (gs_rd_bytes(s, dst, (size_t)cpy64) != 0) return -1;
    }
    if (cap > 0U) dst[(size_t)cpy64] = '\0';
    if (len > cpy64) {
        if (gs_skip_u64(s, len - cpy64) != 0) return -1;
    }
    return 0;
}

static int gs_skip_val(GgufStream *s, uint32_t vt, int depth) {
    uint8_t u8 = 0; uint16_t u16 = 0; uint32_t u32 = 0; uint64_t u64 = 0ULL;
    if (depth > 8) return -1;
    if (vt == 0U || vt == 1U || vt == 7U) return gs_rd_bytes(s, &u8, 1U);
    if (vt == 2U || vt == 3U) return gs_rd_bytes(s, &u16, 2U);
    if (vt == 4U || vt == 5U || vt == 6U) return gs_rd_u32(s, &u32);
    if (vt == 10U || vt == 11U || vt == 12U) return gs_rd_u64(s, &u64);
    if (vt == 8U) {
        if (gs_rd_u64(s, &u64) != 0) return -1;
        return gs_skip_u64(s, u64);
    }
    if (vt == 9U) {
        uint32_t et = 0U; uint64_t cnt = 0ULL;
        if (gs_rd_u32(s, &et) != 0 || gs_rd_u64(s, &cnt) != 0) return -1;
        if (cnt > 2097152ULL) return -1;
        for (uint64_t i = 0ULL; i < cnt; i++) {
            if (gs_skip_val(s, et, depth + 1) != 0) return -1;
        }
        return 0;
    }
    return -1;
}

static int parse_arch_kv(GgufStream *s, ArchCfg *ac, const char *sub, uint32_t vt) {
    if (strcmp(sub, "block_count") == 0 && vt == 4U) return gs_rd_u32(s, &ac->n_layers);
    if (strcmp(sub, "embedding_length") == 0 && vt == 4U) return gs_rd_u32(s, &ac->dim);
    if (strcmp(sub, "feed_forward_length") == 0 && vt == 4U) return gs_rd_u32(s, &ac->hidden_dim);
    if (strcmp(sub, "attention.head_count") == 0 && vt == 4U) return gs_rd_u32(s, &ac->n_heads);
    if (strcmp(sub, "attention.head_count_kv") == 0 && vt == 4U) return gs_rd_u32(s, &ac->n_kv_heads);
    if (strcmp(sub, "context_length") == 0 && vt == 4U) return gs_rd_u32(s, &ac->max_seq_len);
    if (strcmp(sub, "attention.layer_norm_rms_epsilon") == 0 && vt == 6U) return gs_rd_f32(s, &ac->rms_eps);
    if (strcmp(sub, "rope.freq_base") == 0 && vt == 6U) return gs_rd_f32(s, &ac->rope_theta);
    return 1;
}

static int pool_append(char **pool, size_t *len, size_t *cap, GgufStream *s, uint64_t slen, uint32_t *out_off, uint32_t *out_len) {
    if (slen > 16777216ULL) return -1;
    uint32_t eff = (slen <= 4096ULL) ? (uint32_t)slen : 4096U;
    size_t need = *len + (size_t)eff + 1U;
    if (need > *cap) {
        size_t ncap = (*cap == 0U) ? 65536U : *cap;
        while (ncap < need) ncap *= 2U;
        char *np = (char *)realloc(*pool, ncap);
        if (!np) return -1;
        *pool = np;
        *cap = ncap;
    }
    *out_off = (uint32_t)(*len);
    *out_len = eff;
    if (eff > 0U && gs_rd_bytes(s, *pool + *len, (size_t)eff) != 0) return -1;
    (*pool)[*len + (size_t)eff] = '\0';
    *len += (size_t)eff + 1U;
    if (slen > (uint64_t)eff && gs_skip_u64(s, slen - (uint64_t)eff) != 0) return -1;
    return 0;
}

static int tok_lookup(const GgufTokenizer *t, const char *s, size_t len) {
    if (!t->tok_ht || t->vocab_size == 0U || len == 0U) return -1;
    uint64_t h = fnv1a_span64(s, len);
    uint32_t idx = (uint32_t)h & t->tok_ht_mask;
    for (;;) {
        uint32_t slot = t->tok_ht[idx];
        if (slot == 0U) return -1;
        uint32_t tid = slot - 1U;
        if (t->tok_len[tid] == (uint32_t)len && memcmp(t->pool + t->tok_off[tid], s, len) == 0) {
            return (int)tid;
        }
        idx = (idx + 1U) & t->tok_ht_mask;
    }
}

static int merge_lookup_rank(const GgufTokenizer *t, uint32_t a, uint32_t b, uint32_t *out_rank) {
    if (!t->merge_keys || t->n_merges == 0U) return -1;
    uint64_t key = (((uint64_t)a << 32) | (uint64_t)b) + 1ULL;
    uint64_t h = (key ^ (key >> 30)) * 0xBF58476D1CE4E5B9ULL;
    uint32_t idx = (uint32_t)h & t->merge_ht_mask;
    for (;;) {
        uint64_t k = t->merge_keys[idx];
        if (k == 0ULL) return -1;
        if (k == key) {
            *out_rank = t->merge_ranks[idx];
            return 0;
        }
        idx = (idx + 1U) & t->merge_ht_mask;
    }
}

static const GgufTokenizer *g_sort_tok = NULL;
static int cmp_special_desc(const void *pa, const void *pb) {
    uint32_t a = *(const uint32_t *)pa;
    uint32_t b = *(const uint32_t *)pb;
    uint32_t la = g_sort_tok->tok_len[a];
    uint32_t lb = g_sort_tok->tok_len[b];
    if (la != lb) return (lb > la) ? 1 : -1;
    return (a < b) ? -1 : ((a > b) ? 1 : 0);
}

static int finalize_tokenizer(GgufTokenizer *t) {
    if (strcmp(t->model_name, "gpt2") == 0) t->mode = ZCC_TOK_GPT2_BPE;
    else if (strcmp(t->model_name, "llama") == 0) t->mode = ZCC_TOK_SPM;
    else t->mode = ZCC_TOK_ASCII_RAW;

    if (!t->add_bos_explicit) {
        t->add_bos = (t->mode == ZCC_TOK_SPM && t->has_bos) ? 1 : 0;
    }
    if (t->vocab_size == 0U || !t->pool || t->mode == ZCC_TOK_ASCII_RAW) return 0;

    uint32_t ht_sz = 64U;
    while (ht_sz < t->vocab_size * 2U + 16U) ht_sz *= 2U;
    t->tok_ht = (uint32_t *)calloc((size_t)ht_sz, sizeof(uint32_t));
    if (!t->tok_ht) return -1;
    t->tok_ht_mask = ht_sz - 1U;
    for (uint32_t i = 0U; i < t->vocab_size; i++) {
        if (t->tok_len[i] == 0U) continue;
        uint64_t h = fnv1a_span64(t->pool + t->tok_off[i], (size_t)t->tok_len[i]);
        uint32_t idx = (uint32_t)h & t->tok_ht_mask;
        for (;;) {
            uint32_t slot = t->tok_ht[idx];
            if (slot == 0U) {
                t->tok_ht[idx] = i + 1U;
                break;
            }
            uint32_t exist = slot - 1U;
            if (t->tok_len[exist] == t->tok_len[i] &&
                memcmp(t->pool + t->tok_off[exist], t->pool + t->tok_off[i], (size_t)t->tok_len[i]) == 0) {
                break;
            }
            idx = (idx + 1U) & t->tok_ht_mask;
        }
    }

    if (t->n_merges > 0U && t->merge_pool) {
        uint32_t msz = 64U;
        while (msz < t->n_merges * 2U + 16U) msz *= 2U;
        t->merge_keys = (uint64_t *)calloc((size_t)msz, sizeof(uint64_t));
        t->merge_ranks = (uint32_t *)calloc((size_t)msz, sizeof(uint32_t));
        if (!t->merge_keys || !t->merge_ranks) return -1;
        t->merge_ht_mask = msz - 1U;
        for (uint32_t r = 0U; r < t->n_merges; r++) {
            const char *ms = t->merge_pool + t->merge_off[r];
            uint32_t ml = t->merge_len[r];
            const char *sp = (const char *)memchr(ms, ' ', (size_t)ml);
            if (!sp || sp == ms || sp + 1 >= ms + ml) continue;
            size_t la = (size_t)(sp - ms);
            size_t lb = (size_t)(ml - la - 1U);
            int ida = tok_lookup(t, ms, la);
            int idb = tok_lookup(t, sp + 1, lb);
            if (ida < 0 || idb < 0) continue;
            uint64_t key = (((uint64_t)(uint32_t)ida << 32) | (uint64_t)(uint32_t)idb) + 1ULL;
            uint64_t h = (key ^ (key >> 30)) * 0xBF58476D1CE4E5B9ULL;
            uint32_t idx = (uint32_t)h & t->merge_ht_mask;
            for (;;) {
                if (t->merge_keys[idx] == 0ULL) {
                    t->merge_keys[idx] = key;
                    t->merge_ranks[idx] = r;
                    break;
                }
                if (t->merge_keys[idx] == key) break;
                idx = (idx + 1U) & t->merge_ht_mask;
            }
        }
        free(t->merge_pool); t->merge_pool = NULL;
        free(t->merge_off);  t->merge_off = NULL;
        free(t->merge_len);  t->merge_len = NULL;
    }

    if (!t->has_eot) {
        int id_im = tok_lookup(t, "<|im_end|>", 10U);
        int id_eo = tok_lookup(t, "<|endoftext|>", 13U);
        if (t->has_eos) {
            if (id_im >= 0 && (uint32_t)id_im == t->eos_id && id_eo >= 0 && (uint32_t)id_eo != t->eos_id) {
                t->eot_id = (uint32_t)id_eo;
                t->has_eot = 1;
            } else if (id_eo >= 0 && (uint32_t)id_eo == t->eos_id && id_im >= 0 && (uint32_t)id_im != t->eos_id) {
                t->eot_id = (uint32_t)id_im;
                t->has_eot = 1;
            }
        }
    }

    /* Collect special/control tokens (token_type == 3) sorted longest-first */
    t->special_ids = (uint32_t *)malloc((size_t)t->vocab_size * sizeof(uint32_t));
    if (!t->special_ids) return -1;
    uint32_t ns = 0U;
    for (uint32_t i = 0U; i < t->vocab_size; i++) {
        if (t->tok_len[i] == 0U) continue;
        int is_sp = (t->tok_type && i < t->n_types && t->tok_type[i] == 3U);
        if (!is_sp && ((t->has_bos && i == t->bos_id) || (t->has_eos && i == t->eos_id) || (t->has_eot && i == t->eot_id))) {
            is_sp = 1;
        }
        if (is_sp) t->special_ids[ns++] = i;
    }
    t->n_specials = ns;
    if (ns > 1U) {
        g_sort_tok = t;
        qsort(t->special_ids, (size_t)ns, sizeof(uint32_t), cmp_special_desc);
        g_sort_tok = NULL;
    }
    return 0;
}

/* OpenAI/HuggingFace GPT-2 byte <-> UTF-8 bijection */
static char g_b2u_str[256][4];
static uint8_t g_b2u_len[256];
static int g_cp2byte[512];
static int g_b2u_init = 0;

static void init_gpt2_byte_tables(void) {
    if (g_b2u_init) return;
    for (int i = 0; i < 512; i++) g_cp2byte[i] = -1;
    int extra = 0;
    for (int b = 0; b < 256; b++) {
        int cp = 0;
        if ((b >= 33 && b <= 126) || (b >= 161 && b <= 172) || (b >= 174 && b <= 255)) {
            cp = b;
        } else {
            cp = 256 + extra;
            extra++;
        }
        g_cp2byte[cp] = b;
        if (cp < 128) {
            g_b2u_str[b][0] = (char)cp;
            g_b2u_str[b][1] = '\0';
            g_b2u_len[b] = 1U;
        } else {
            g_b2u_str[b][0] = (char)(0xC0 | (cp >> 6));
            g_b2u_str[b][1] = (char)(0x80 | (cp & 0x3F));
            g_b2u_str[b][2] = '\0';
            g_b2u_len[b] = 2U;
        }
    }
    g_b2u_init = 1;
}

static int is_ascii_alpha(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static int is_ascii_digit(unsigned char c) {
    return c >= '0' && c <= '9';
}

static int is_ascii_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static int is_bpe_letter(unsigned char c) {
    return is_ascii_alpha(c) || c >= 0x80U;
}

static int tok_push_id(uint32_t **arr, uint32_t *cnt, uint32_t *cap, uint32_t id) {
    if (*cnt >= *cap) {
        uint32_t ncap = (*cap == 0U) ? 256U : (*cap * 2U);
        uint32_t *na = (uint32_t *)realloc(*arr, (size_t)ncap * sizeof(uint32_t));
        if (!na) return -1;
        *arr = na;
        *cap = ncap;
    }
    (*arr)[(*cnt)++] = id;
    return 0;
}

static int bpe_encode_word(const GgufTokenizer *t, const char *w, size_t wlen, uint32_t **out, uint32_t *ocnt, uint32_t *ocap) {
    init_gpt2_byte_tables();
    if (wlen == 0U) return 0;
    uint32_t *ids = (uint32_t *)malloc(wlen * sizeof(uint32_t));
    if (!ids) return -1;
    size_t n_ids = 0U;
    for (size_t i = 0U; i < wlen; i++) {
        unsigned char b = (unsigned char)w[i];
        int id = tok_lookup(t, g_b2u_str[b], (size_t)g_b2u_len[b]);
        if (id >= 0) ids[n_ids++] = (uint32_t)id;
    }
    char *c_buf = NULL;
    size_t c_cap = 0U;
    while (n_ids > 1U) {
        uint32_t best_rank = 0xFFFFFFFFU;
        size_t best_idx = 0U;
        int found = 0;
        for (size_t i = 0U; i + 1U < n_ids; i++) {
            uint32_t r = 0U;
            if (merge_lookup_rank(t, ids[i], ids[i + 1U], &r) == 0) {
                if (!found || r < best_rank) {
                    found = 1;
                    best_rank = r;
                    best_idx = i;
                }
            }
        }
        if (!found) break;
        uint32_t a = ids[best_idx];
        uint32_t b = ids[best_idx + 1U];
        size_t la = (size_t)t->tok_len[a];
        size_t lb = (size_t)t->tok_len[b];
        if (la + lb + 1U > c_cap) {
            size_t nc = (la + lb + 256U);
            char *nb = (char *)realloc(c_buf, nc);
            if (!nb) { if (c_buf) free(c_buf); free(ids); return -1; }
            c_buf = nb; c_cap = nc;
        }
        memcpy(c_buf, t->pool + t->tok_off[a], la);
        memcpy(c_buf + la, t->pool + t->tok_off[b], lb);
        int merged = tok_lookup(t, c_buf, la + lb);
        if (merged < 0) break;
        ids[best_idx] = (uint32_t)merged;
        for (size_t j = best_idx + 1U; j + 1U < n_ids; j++) ids[j] = ids[j + 1U];
        n_ids--;
    }
    if (c_buf) free(c_buf);
    for (size_t i = 0U; i < n_ids; i++) {
        if (tok_push_id(out, ocnt, ocap, ids[i]) != 0) { free(ids); return -1; }
    }
    free(ids);
    return 0;
}

static size_t next_bpe_pretoken_len(const char *s, size_t n) {
    if (n == 0U) return 0U;
    unsigned char c0 = (unsigned char)s[0];
    if (c0 == '\r' || c0 == '\n') {
        size_t p = 0U;
        while (p < n && (s[p] == '\r' || s[p] == '\n')) p++;
        return p;
    }
    if (c0 == '\'' && n >= 2U) {
        unsigned char c1 = (unsigned char)s[1];
        int l1 = (c1 >= 'A' && c1 <= 'Z') ? (int)(c1 + 32) : (int)c1;
        if ((l1 == 's' || l1 == 't' || l1 == 'm' || l1 == 'd') &&
            (n == 2U || !is_bpe_letter((unsigned char)s[2]))) {
            return 2U;
        }
        if (n >= 3U) {
            unsigned char c2 = (unsigned char)s[2];
            int l2 = (c2 >= 'A' && c2 <= 'Z') ? (int)(c2 + 32) : (int)c2;
            if (((l1 == 'r' && l2 == 'e') || (l1 == 'v' && l2 == 'e') || (l1 == 'l' && l2 == 'l')) &&
                (n == 3U || !is_bpe_letter((unsigned char)s[3]))) {
                return 3U;
            }
        }
    }
    if (is_bpe_letter(c0)) {
        size_t p = 1U;
        while (p < n && is_bpe_letter((unsigned char)s[p])) p++;
        return p;
    }
    if (c0 != '\r' && c0 != '\n' && !is_bpe_letter(c0) && !is_ascii_digit(c0) &&
        n >= 2U && is_bpe_letter((unsigned char)s[1])) {
        size_t p = 2U;
        while (p < n && is_bpe_letter((unsigned char)s[p])) p++;
        return p;
    }
    if (is_ascii_digit(c0)) return 1U;
    if (c0 == ' ' && n >= 2U) {
        unsigned char c1 = (unsigned char)s[1];
        if (!is_ascii_space(c1) && !is_ascii_digit(c1)) {
            size_t p = 2U;
            while (p < n && !is_ascii_space((unsigned char)s[p]) && !is_bpe_letter((unsigned char)s[p]) && !is_ascii_digit((unsigned char)s[p])) p++;
            while (p < n && (s[p] == '\r' || s[p] == '\n')) p++;
            return p;
        }
    }
    if (is_ascii_space(c0)) {
        size_t p = 0U;
        while (p < n && is_ascii_space((unsigned char)s[p]) && s[p] != '\r' && s[p] != '\n') p++;
        if (p < n && (s[p] == '\r' || s[p] == '\n')) {
            while (p < n && (s[p] == '\r' || s[p] == '\n')) p++;
            return p;
        }
        if (p > 1U && p < n && !is_ascii_digit((unsigned char)s[p])) return p - 1U;
        return p;
    }
    size_t p = 1U;
    while (p < n && !is_ascii_space((unsigned char)s[p]) && !is_bpe_letter((unsigned char)s[p]) && !is_ascii_digit((unsigned char)s[p])) p++;
    while (p < n && (s[p] == '\r' || s[p] == '\n')) p++;
    return p;
}

static size_t utf8_char_len(const char *s, size_t rem) {
    if (rem == 0U) return 0U;
    unsigned char c = (unsigned char)s[0];
    if (c < 0x80U) return 1U;
    if ((c & 0xE0U) == 0xC0U && rem >= 2U) return 2U;
    if ((c & 0xF0U) == 0xE0U && rem >= 3U) return 3U;
    if ((c & 0xF8U) == 0xF0U && rem >= 4U) return 4U;
    return 1U;
}

static int spm_encode_segment(const GgufTokenizer *t, const char *seg, size_t slen, uint32_t **out, uint32_t *ocnt, uint32_t *ocap) {
    if (slen == 0U) return 0;
    /* Prepend U+2581 (\xE2\x96\x81) and replace ASCII spaces with \xE2\x96\x81 */
    size_t tcap = slen * 3U + 4U;
    char *tbuf = (char *)malloc(tcap);
    if (!tbuf) return -1;
    size_t tlen = 0U;
    tbuf[tlen++] = (char)0xE2; tbuf[tlen++] = (char)0x96; tbuf[tlen++] = (char)0x81;
    for (size_t i = 0U; i < slen; i++) {
        if (seg[i] == ' ') {
            tbuf[tlen++] = (char)0xE2; tbuf[tlen++] = (char)0x96; tbuf[tlen++] = (char)0x81;
        } else {
            tbuf[tlen++] = seg[i];
        }
    }

    uint32_t *ids = (uint32_t *)malloc((tlen + 1U) * sizeof(uint32_t));
    if (!ids) { free(tbuf); return -1; }
    size_t n_ids = 0U;
    size_t pos = 0U;
    while (pos < tlen) {
        size_t cl = utf8_char_len(tbuf + pos, tlen - pos);
        int id = tok_lookup(t, tbuf + pos, cl);
        if (id >= 0) {
            ids[n_ids++] = (uint32_t)id;
        } else {
            for (size_t k = 0U; k < cl; k++) {
                char hex[8];
                snprintf(hex, sizeof(hex), "<0x%02X>", (unsigned int)(unsigned char)tbuf[pos + k]);
                int bid = tok_lookup(t, hex, 6U);
                if (bid < 0) bid = 0;
                ids[n_ids++] = (uint32_t)bid;
            }
        }
        pos += cl;
    }
    free(tbuf);

    char *c_buf = NULL;
    size_t c_cap = 0U;
    while (n_ids > 1U) {
        float best_score = -1e30f;
        size_t best_idx = 0U;
        uint32_t best_id = 0U;
        int found = 0;
        for (size_t i = 0U; i + 1U < n_ids; i++) {
            uint32_t a = ids[i];
            uint32_t b = ids[i + 1U];
            size_t la = (size_t)t->tok_len[a];
            size_t lb = (size_t)t->tok_len[b];
            if (la + lb + 1U > c_cap) {
                size_t nc = la + lb + 256U;
                char *nb = (char *)realloc(c_buf, nc);
                if (!nb) { if (c_buf) free(c_buf); free(ids); return -1; }
                c_buf = nb; c_cap = nc;
            }
            memcpy(c_buf, t->pool + t->tok_off[a], la);
            memcpy(c_buf + la, t->pool + t->tok_off[b], lb);
            int mid = tok_lookup(t, c_buf, la + lb);
            if (mid >= 0 && (!t->tok_type || (uint32_t)mid >= t->n_types || t->tok_type[mid] != 3U)) {
                float sc = (t->scores && (uint32_t)mid < t->n_scores) ? t->scores[mid] : 0.0f;
                if (!found || sc > best_score) {
                    found = 1;
                    best_score = sc;
                    best_idx = i;
                    best_id = (uint32_t)mid;
                }
            }
        }
        if (!found) break;
        ids[best_idx] = best_id;
        for (size_t j = best_idx + 1U; j + 1U < n_ids; j++) ids[j] = ids[j + 1U];
        n_ids--;
    }
    if (c_buf) free(c_buf);
    for (size_t i = 0U; i < n_ids; i++) {
        if (tok_push_id(out, ocnt, ocap, ids[i]) != 0) { free(ids); return -1; }
    }
    free(ids);
    return 0;
}

static int tokenize_prompt(
    const LlamaModel *m,
    const char *text,
    int no_bos,
    uint32_t **out_ids,
    uint32_t *out_cnt
) {
    const GgufTokenizer *t = &m->tok;
    *out_ids = NULL;
    *out_cnt = 0U;
    uint32_t cap = 0U;
    size_t len = strlen(text);
    if (t->mode == ZCC_TOK_ASCII_RAW) {
        if (len == 0U) return -1;
        for (size_t i = 0U; i < len; i++) {
            uint32_t id = (uint32_t)(unsigned char)text[i];
            if (id >= m->vocab_size) id = 32U;
            if (tok_push_id(out_ids, out_cnt, &cap, id) != 0) return -1;
        }
        return 0;
    }

    if (t->add_bos && !no_bos && t->has_bos) {
        if (tok_push_id(out_ids, out_cnt, &cap, t->bos_id) != 0) return -1;
    }

    size_t pos = 0U;
    size_t seg_start = 0U;
    while (pos <= len) {
        int sp_id = -1;
        uint32_t sp_len = 0U;
        if (pos < len) {
            for (uint32_t si = 0U; si < t->n_specials; si++) {
                uint32_t sid = t->special_ids[si];
                uint32_t sl = t->tok_len[sid];
                if (pos + (size_t)sl <= len && memcmp(text + pos, t->pool + t->tok_off[sid], (size_t)sl) == 0) {
                    sp_id = (int)sid;
                    sp_len = sl;
                    break;
                }
            }
        }
        if (pos == len || sp_id >= 0) {
            size_t seg_len = pos - seg_start;
            if (seg_len > 0U) {
                if (t->mode == ZCC_TOK_GPT2_BPE) {
                    size_t p = 0U;
                    while (p < seg_len) {
                        size_t wl = next_bpe_pretoken_len(text + seg_start + p, seg_len - p);
                        if (wl == 0U) wl = 1U;
                        if (bpe_encode_word(t, text + seg_start + p, wl, out_ids, out_cnt, &cap) != 0) return -1;
                        p += wl;
                    }
                } else {
                    if (spm_encode_segment(t, text + seg_start, seg_len, out_ids, out_cnt, &cap) != 0) return -1;
                }
            }
            if (sp_id >= 0) {
                if (tok_push_id(out_ids, out_cnt, &cap, (uint32_t)sp_id) != 0) return -1;
                pos += (size_t)sp_len;
                seg_start = pos;
                continue;
            }
            break;
        }
        pos++;
    }
    if (*out_cnt == 0U) return -1;
    return 0;
}

static int hex_digit_val(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    return -1;
}

static size_t detokenize_piece(const GgufTokenizer *t, uint32_t id, char *out, size_t cap) {
    if (t->mode == ZCC_TOK_ASCII_RAW) {
        if (cap == 0U) return 0U;
        int c = (int)id;
        out[0] = (c == 10 || (c >= 32 && c < 127)) ? (char)c : '?';
        if (cap > 1U) out[1] = '\0';
        return 1U;
    }
    if (id >= t->vocab_size || !t->pool) { if (cap > 0U) out[0] = '\0'; return 0U; }
    if (t->tok_type && id < t->n_types && t->tok_type[id] == 3U) {
        if (cap > 0U) out[0] = '\0';
        return 0U;
    }
    const char *s = t->pool + t->tok_off[id];
    uint32_t slen = t->tok_len[id];
    size_t n = 0U;

    if (t->mode == ZCC_TOK_SPM) {
        if (slen == 6U && s[0] == '<' && s[1] == '0' && (s[2] == 'x' || s[2] == 'X') && s[5] == '>') {
            int h0 = hex_digit_val((int)(unsigned char)s[3]);
            int h1 = hex_digit_val((int)(unsigned char)s[4]);
            if (h0 >= 0 && h1 >= 0) {
                if (n + 1U < cap) out[n++] = (char)((h0 << 4) | h1);
                if (cap > 0U) out[n] = '\0';
                return n;
            }
        }
        uint32_t p = 0U;
        while (p < slen) {
            if (p + 3U <= slen &&
                (unsigned char)s[p] == 0xE2U &&
                (unsigned char)s[p + 1U] == 0x96U &&
                (unsigned char)s[p + 2U] == 0x81U) {
                if (n + 1U < cap) out[n++] = ' ';
                p += 3U;
            } else {
                if (n + 1U < cap) out[n++] = s[p];
                p++;
            }
        }
        if (cap > 0U) out[n] = '\0';
        return n;
    }

    init_gpt2_byte_tables();
    uint32_t p = 0U;
    while (p < slen) {
        unsigned char c0 = (unsigned char)s[p];
        if (c0 < 0x80U) {
            int b = g_cp2byte[(int)c0];
            if (n + 1U < cap) out[n++] = (b >= 0) ? (char)b : (char)c0;
            p++;
        } else if ((c0 & 0xE0U) == 0xC0U && p + 1U < slen && ((unsigned char)s[p + 1U] & 0xC0U) == 0x80U) {
            int cp = (((int)(c0 & 0x1FU)) << 6) | (int)((unsigned char)s[p + 1U] & 0x3FU);
            if (cp < 512 && g_cp2byte[cp] >= 0) {
                if (n + 1U < cap) out[n++] = (char)g_cp2byte[cp];
            } else {
                if (n + 2U < cap) { out[n++] = s[p]; out[n++] = s[p + 1U]; }
            }
            p += 2U;
        } else {
            size_t cl = utf8_char_len(s + p, (size_t)(slen - p));
            for (size_t k = 0U; k < cl && n + 1U < cap; k++) out[n++] = s[p + k];
            p += (uint32_t)cl;
        }
    }
    if (cap > 0U) out[n] = '\0';
    return n;
}

static int find_tensor_idx(const LlamaModel *m, const char *name) {
    for (unsigned int i = 0U; i < m->num_tensors; i++) {
        if (strcmp(m->tensors[i].name, name) == 0) return (int)i;
    }
    return -1;
}

static const GgufTensor *find_tensor(const LlamaModel *m, const char *name) {
    int idx = find_tensor_idx(m, name);
    return (idx >= 0) ? &m->tensors[idx] : NULL;
}

static uint64_t align32_u64(uint64_t x) {
    return (x + 31ULL) & ~31ULL;
}

static void spill_mark_idx(LlamaModel *m, unsigned int ti, uint64_t *io_cur_vram) {
    GgufTensor *t = &m->tensors[ti];
    if (!t->on_host) {
        t->on_host = 1;
        uint64_t ab = align32_u64(t->nbytes);
        *io_cur_vram = (*io_cur_vram > ab) ? (*io_cur_vram - ab) : 0ULL;
    }
}

static void spill_mark_name(LlamaModel *m, const char *name, uint64_t *io_cur_vram) {
    int idx = find_tensor_idx(m, name);
    if (idx >= 0) spill_mark_idx(m, (unsigned int)idx, io_cur_vram);
}

static int require_tensor_2d(const LlamaModel *m, const char *name, unsigned int exp_k, unsigned int exp_n,
                             uint64_t *out_ptr, uint64_t *out_off, uint32_t *out_type) {
    const GgufTensor *t = find_tensor(m, name);
    if (!t || t->n_dims != 2U || t->dims[0] != (uint64_t)exp_k || t->dims[1] != (uint64_t)exp_n) {
        fprintf(stderr, "GGUF_PARSE_ERROR: missing or shape-mismatch tensor '%s' (expected [%u, %u])\n", name, exp_k, exp_n);
        return -1;
    }
    *out_ptr = t->d_ptr;
    *out_off = t->offset;
    *out_type = t->ggml_type;
    return 0;
}

static int require_tensor_1d(const LlamaModel *m, const char *name, unsigned int exp_n,
                             uint64_t *out_ptr, uint64_t *out_off, uint32_t *out_type) {
    const GgufTensor *t = find_tensor(m, name);
    if (!t || t->n_dims != 1U || t->dims[0] != (uint64_t)exp_n || t->ggml_type != ZCC_GGML_F32) {
        fprintf(stderr, "GGUF_PARSE_ERROR: missing or invalid 1D norm tensor '%s' (expected F32 [%u])\n", name, exp_n);
        return -1;
    }
    *out_ptr = t->d_ptr;
    *out_off = t->offset;
    *out_type = t->ggml_type;
    return 0;
}

static const char *wtype_name_str(uint32_t wtype) {
    if (wtype == ZCC_GGML_F16) return "F16";
    if (wtype == ZCC_GGML_Q4_0) return "Q4_0";
    if (wtype == ZCC_GGML_Q8_0) return "Q8_0";
    if (wtype == ZCC_GGML_Q4_K) return "Q4_K";
    if (wtype == ZCC_GGML_Q6_K) return "Q6_K";
    if (wtype == ZCC_GGML_BF16) return "BF16";
    return "F32";
}

static int check_aot_structural_match(const LlamaModel *m, const uint64_t *aot_meta) {
    if (aot_meta[9] != m->wblob_bytes ||
        aot_meta[10] != m->data_start ||
        aot_meta[2] != (uint64_t)m->n_layers ||
        aot_meta[3] != (uint64_t)m->dim ||
        aot_meta[4] != (uint64_t)m->hidden_dim ||
        aot_meta[5] != (uint64_t)m->n_heads ||
        aot_meta[6] != (uint64_t)m->n_kv_heads ||
        aot_meta[7] != (uint64_t)m->max_seq_len ||
        aot_meta[8] != (uint64_t)m->vocab_size) {
        fprintf(stderr, "AOT_FINGERPRINT_MISMATCH: structural model mismatch against compiled AOT PTX\n");
        return -1;
    }
    return 0;
}

static int parse_and_stream_gguf_ex(
    const char *path,
    const uint8_t *emb_mem,
    size_t emb_len,
    uint64_t slice_off,
    uint64_t slice_len,
    const uint8_t *expected_sha256,
    LlamaModel *m,
    int use_aot,
    const uint64_t *aot_meta,
    int allow_spill,
    uint64_t reserve_vram_bytes
) {
    memset(m, 0, sizeof(*m));
    m->align = 32U;
    m->rms_eps = 1e-5f;
    m->rope_theta = 10000.0f;
    strcpy(m->arch, "llama");

    ArchCfg arch_cfgs[2];
    memset(arch_cfgs, 0, sizeof(arch_cfgs));
    arch_cfgs[0].rms_eps = 1e-5f; arch_cfgs[0].rope_theta = 10000.0f;
    arch_cfgs[1].rms_eps = 1e-5f; arch_cfgs[1].rope_theta = 10000.0f;

    GgufStream s;
    if (emb_mem) {
        if (gs_open_mem(&s, emb_mem, emb_len) != 0) {
            fprintf(stderr, "GGUF_PARSE_ERROR: cannot initialize embedded GGUF\n");
            return -1;
        }
    } else if (slice_len > 0ULL) {
        if (gs_open_file_slice(&s, path, slice_off, slice_len, expected_sha256 != NULL) != 0) {
            fprintf(stderr, "TAMPER_DETECTED: cannot open sealed PE overlay slice in '%s'\n", path ? path : "");
            return -1;
        }
    } else {
        if (gs_open_file(&s, path) != 0) {
            char msg[320];
            snprintf(msg, sizeof(msg), "GGUF_PARSE_ERROR: cannot open '%s'", path ? path : "");
            fprintf(stderr, "%s\n", msg);
            zcc_llm_set_err(msg);
            return -1;
        }
    }
    if (s.fsz64 < 24ULL) {
        gs_close(&s);
        fprintf(stderr, "GGUF_PARSE_ERROR: file '%s' truncated (<24B)\n", path ? path : "<embedded>");
        return -1;
    }

    uint32_t magic = 0U, version = 0U;
    uint64_t n_tensors = 0ULL, n_kv = 0ULL;
    if (gs_rd_u32(&s, &magic) != 0 || gs_rd_u32(&s, &version) != 0 ||
        gs_rd_u64(&s, &n_tensors) != 0 || gs_rd_u64(&s, &n_kv) != 0) {
        gs_close(&s);
        fprintf(stderr, "GGUF_PARSE_ERROR: header truncated\n");
        return -1;
    }
    if (magic != ZCC_GGUF_MAGIC) {
        gs_close(&s);
        fprintf(stderr, "GGUF_PARSE_ERROR: invalid magic 0x%08x (expected 0x%08x)\n", magic, ZCC_GGUF_MAGIC);
        return -1;
    }
    if (version != 3U) {
        gs_close(&s);
        fprintf(stderr, "GGUF_PARSE_ERROR: unsupported GGUF version %u (expected 3)\n", version);
        return -1;
    }
    if (n_tensors == 0ULL || n_tensors > (uint64_t)ZCC_MAX_TENSORS || n_kv > 4096ULL) {
        gs_close(&s);
        fprintf(stderr, "GGUF_PARSE_ERROR: invalid tensor/kv counts (%u/%u)\n", (unsigned int)n_tensors, (unsigned int)n_kv);
        return -1;
    }

    char key[256];
    GgufTokenizer *tk = &m->tok;
    for (uint64_t i = 0ULL; i < n_kv; i++) {
        uint32_t vt = 0U;
        if (gs_rd_str_cap(&s, key, sizeof(key)) != 0 || gs_rd_u32(&s, &vt) != 0) {
            gs_close(&s); free_llama_model(m);
            fprintf(stderr, "GGUF_PARSE_ERROR: kv entry truncated\n");
            return -1;
        }
        if (strcmp(key, "general.architecture") == 0 && vt == 8U) {
            if (gs_rd_str_cap(&s, m->arch, sizeof(m->arch)) != 0) { gs_close(&s); free_llama_model(m); return -1; }
        } else if (strcmp(key, "general.alignment") == 0 && vt == 4U) {
            uint32_t av = 0U;
            if (gs_rd_u32(&s, &av) != 0) { gs_close(&s); free_llama_model(m); return -1; }
            if (av == 0U || (av & (av - 1U)) != 0U || av > (1U << 20)) {
                gs_close(&s); free_llama_model(m);
                fprintf(stderr, "GGUF_PARSE_ERROR: invalid non-power-of-two general.alignment %u\n", av);
                return -1;
            }
            m->align = av;
        } else if (strncmp(key, "llama.", 6) == 0) {
            int r = parse_arch_kv(&s, &arch_cfgs[0], key + 6, vt);
            if (r < 0 || (r > 0 && gs_skip_val(&s, vt, 0) != 0)) { gs_close(&s); free_llama_model(m); return -1; }
        } else if (strncmp(key, "qwen2.", 6) == 0) {
            int r = parse_arch_kv(&s, &arch_cfgs[1], key + 6, vt);
            if (r < 0 || (r > 0 && gs_skip_val(&s, vt, 0) != 0)) { gs_close(&s); free_llama_model(m); return -1; }
        } else if (strcmp(key, "tokenizer.ggml.model") == 0 && vt == 8U) {
            if (gs_rd_str_cap(&s, tk->model_name, sizeof(tk->model_name)) != 0) { gs_close(&s); free_llama_model(m); return -1; }
        } else if (strcmp(key, "tokenizer.ggml.tokens") == 0 && vt == 9U) {
            uint32_t et = 0U; uint64_t cnt = 0ULL;
            if (gs_rd_u32(&s, &et) != 0 || gs_rd_u64(&s, &cnt) != 0 || et != 8U || cnt == 0ULL || cnt > 1048576ULL) {
                gs_close(&s); free_llama_model(m);
                fprintf(stderr, "GGUF_PARSE_ERROR: invalid tokenizer array\n");
                return -1;
            }
            m->vocab_size = (unsigned int)cnt;
            tk->vocab_size = (uint32_t)cnt;
            tk->tok_off = (uint32_t *)calloc((size_t)cnt, sizeof(uint32_t));
            tk->tok_len = (uint32_t *)calloc((size_t)cnt, sizeof(uint32_t));
            if (!tk->tok_off || !tk->tok_len) { gs_close(&s); free_llama_model(m); return -1; }
            for (uint64_t ti = 0ULL; ti < cnt; ti++) {
                uint64_t slen = 0ULL;
                if (gs_rd_u64(&s, &slen) != 0 ||
                    pool_append(&tk->pool, &tk->pool_len, &tk->pool_cap, &s, slen,
                                &tk->tok_off[ti], &tk->tok_len[ti]) != 0) {
                    gs_close(&s); free_llama_model(m); return -1;
                }
            }
        } else if (strcmp(key, "tokenizer.ggml.scores") == 0 && vt == 9U) {
            uint32_t et = 0U; uint64_t cnt = 0ULL;
            if (gs_rd_u32(&s, &et) != 0 || gs_rd_u64(&s, &cnt) != 0 || et != 6U || cnt > 1048576ULL) {
                gs_close(&s); free_llama_model(m); return -1;
            }
            tk->n_scores = (uint32_t)cnt;
            tk->scores = (float *)calloc((size_t)cnt + 1U, sizeof(float));
            if (!tk->scores) { gs_close(&s); free_llama_model(m); return -1; }
            for (uint64_t ti = 0ULL; ti < cnt; ti++) {
                if (gs_rd_f32(&s, &tk->scores[ti]) != 0) { gs_close(&s); free_llama_model(m); return -1; }
            }
        } else if (strcmp(key, "tokenizer.ggml.token_type") == 0 && vt == 9U) {
            uint32_t et = 0U; uint64_t cnt = 0ULL;
            if (gs_rd_u32(&s, &et) != 0 || gs_rd_u64(&s, &cnt) != 0 || (et != 4U && et != 5U) || cnt > 1048576ULL) {
                gs_close(&s); free_llama_model(m); return -1;
            }
            tk->n_types = (uint32_t)cnt;
            tk->tok_type = (uint8_t *)calloc((size_t)cnt + 1U, 1U);
            if (!tk->tok_type) { gs_close(&s); free_llama_model(m); return -1; }
            for (uint64_t ti = 0ULL; ti < cnt; ti++) {
                uint32_t tv = 0U;
                if (gs_rd_u32(&s, &tv) != 0) { gs_close(&s); free_llama_model(m); return -1; }
                tk->tok_type[ti] = (uint8_t)(tv & 255U);
            }
        } else if (strcmp(key, "tokenizer.ggml.merges") == 0 && vt == 9U) {
            uint32_t et = 0U; uint64_t cnt = 0ULL;
            if (gs_rd_u32(&s, &et) != 0 || gs_rd_u64(&s, &cnt) != 0 || et != 8U || cnt > 2097152ULL) {
                gs_close(&s); free_llama_model(m); return -1;
            }
            tk->n_merges = (uint32_t)cnt;
            tk->merge_off = (uint32_t *)calloc((size_t)cnt + 1U, sizeof(uint32_t));
            tk->merge_len = (uint32_t *)calloc((size_t)cnt + 1U, sizeof(uint32_t));
            if (!tk->merge_off || !tk->merge_len) { gs_close(&s); free_llama_model(m); return -1; }
            for (uint64_t mi = 0ULL; mi < cnt; mi++) {
                uint64_t slen = 0ULL;
                if (gs_rd_u64(&s, &slen) != 0 ||
                    pool_append(&tk->merge_pool, &tk->merge_pool_len, &tk->merge_pool_cap, &s, slen,
                                &tk->merge_off[mi], &tk->merge_len[mi]) != 0) {
                    gs_close(&s); free_llama_model(m); return -1;
                }
            }
        } else if (strcmp(key, "tokenizer.ggml.bos_token_id") == 0 && (vt == 4U || vt == 5U)) {
            if (gs_rd_u32(&s, &tk->bos_id) != 0) { gs_close(&s); free_llama_model(m); return -1; }
            tk->has_bos = 1;
        } else if (strcmp(key, "tokenizer.ggml.eos_token_id") == 0 && (vt == 4U || vt == 5U)) {
            if (gs_rd_u32(&s, &tk->eos_id) != 0) { gs_close(&s); free_llama_model(m); return -1; }
            tk->has_eos = 1;
        } else if (strcmp(key, "tokenizer.ggml.eot_token_id") == 0 && (vt == 4U || vt == 5U)) {
            if (gs_rd_u32(&s, &tk->eot_id) != 0) { gs_close(&s); free_llama_model(m); return -1; }
            tk->has_eot = 1;
        } else if (strcmp(key, "tokenizer.ggml.add_bos_token") == 0 && vt == 7U) {
            uint8_t b = 0U;
            if (gs_rd_u8(&s, &b) != 0) { gs_close(&s); free_llama_model(m); return -1; }
            tk->add_bos = (b != 0U) ? 1 : 0;
            tk->add_bos_explicit = 1;
        } else {
            if (gs_skip_val(&s, vt, 0) != 0) {
                gs_close(&s); free_llama_model(m);
                fprintf(stderr, "GGUF_PARSE_ERROR: unsupported kv value '%s' (%u)\n", key, vt);
                return -1;
            }
        }
    }

    int arch_idx = -1;
    if (strcmp(m->arch, "llama") == 0) arch_idx = 0;
    else if (strcmp(m->arch, "qwen2") == 0) arch_idx = 1;
    if (arch_idx < 0) {
        gs_close(&s); free_llama_model(m);
        fprintf(stderr, "GGUF_PARSE_ERROR: unsupported architecture '%s'\n", m->arch);
        return -1;
    }

    m->n_layers = arch_cfgs[arch_idx].n_layers;
    m->dim = arch_cfgs[arch_idx].dim;
    m->hidden_dim = arch_cfgs[arch_idx].hidden_dim;
    m->n_heads = arch_cfgs[arch_idx].n_heads;
    m->n_kv_heads = arch_cfgs[arch_idx].n_kv_heads;
    m->max_seq_len = arch_cfgs[arch_idx].max_seq_len;
    m->rms_eps = arch_cfgs[arch_idx].rms_eps;
    m->rope_theta = arch_cfgs[arch_idx].rope_theta;

    if (m->n_layers == 0U || m->n_layers > ZCC_MAX_LAYERS ||
        m->dim == 0U || m->hidden_dim == 0U || m->n_heads == 0U || (m->dim % m->n_heads) != 0U) {
        gs_close(&s); free_llama_model(m);
        fprintf(stderr, "GGUF_PARSE_ERROR: invalid %s metadata configuration\n", m->arch);
        return -1;
    }
    if (m->n_kv_heads == 0U) m->n_kv_heads = m->n_heads;
    if ((m->n_heads % m->n_kv_heads) != 0U) {
        gs_close(&s); free_llama_model(m);
        fprintf(stderr, "GGUF_PARSE_ERROR: GQA n_heads not divisible by n_kv_heads\n");
        return -1;
    }
    if (m->max_seq_len == 0U) m->max_seq_len = 128U;
    m->head_dim = m->dim / m->n_heads;
    m->kv_dim = m->n_kv_heads * m->head_dim;
    m->num_tensors = (unsigned int)n_tensors;

    m->tensors = (GgufTensor *)calloc((size_t)m->num_tensors, sizeof(GgufTensor));
    m->layers = (LayerWeights *)calloc((size_t)m->n_layers, sizeof(LayerWeights));
    if (!m->tensors || !m->layers) { gs_close(&s); free_llama_model(m); return -1; }

    for (unsigned int ti = 0U; ti < m->num_tensors; ti++) {
        GgufTensor *t = &m->tensors[ti];
        if (gs_rd_str_cap(&s, t->name, sizeof(t->name)) != 0 || gs_rd_u32(&s, &t->n_dims) != 0) {
            gs_close(&s); free_llama_model(m);
            fprintf(stderr, "GGUF_PARSE_ERROR: tensor header truncated\n");
            return -1;
        }
        if (t->n_dims == 0U || t->n_dims > 4U) {
            gs_close(&s); free_llama_model(m);
            fprintf(stderr, "GGUF_PARSE_ERROR: bad n_dims %u on '%s'\n", t->n_dims, t->name);
            return -1;
        }
        uint64_t numel = 1ULL;
        for (uint32_t d = 0U; d < t->n_dims; d++) {
            if (gs_rd_u64(&s, &t->dims[d]) != 0 || t->dims[d] == 0ULL ||
                mul_u64_checked(numel, t->dims[d], &numel) != 0) {
                gs_close(&s);
                fprintf(stderr, "GGUF_PARSE_ERROR: tensor '%s' size overflow\n", t->name);
                free_llama_model(m);
                return -1;
            }
        }
        if (gs_rd_u32(&s, &t->ggml_type) != 0 || gs_rd_u64(&s, &t->offset) != 0) {
            gs_close(&s); free_llama_model(m); return -1;
        }
        uint64_t bytes64 = 0ULL;
        if (t->ggml_type == ZCC_GGML_F32) {
            if (mul_u64_checked(numel, 4ULL, &bytes64) != 0) { gs_close(&s); free_llama_model(m); return -1; }
        } else if (t->ggml_type == ZCC_GGML_F16 || t->ggml_type == ZCC_GGML_BF16) {
            if (mul_u64_checked(numel, 2ULL, &bytes64) != 0) { gs_close(&s); free_llama_model(m); return -1; }
        } else if (t->ggml_type == ZCC_GGML_Q4_0) {
            if ((t->dims[0] % 32ULL) != 0ULL || mul_u64_checked(numel / 32ULL, 18ULL, &bytes64) != 0) {
                gs_close(&s);
                fprintf(stderr, "GGUF_PARSE_ERROR: Q4_0 tensor '%s' alignment failure\n", t->name);
                free_llama_model(m);
                return -1;
            }
        } else if (t->ggml_type == ZCC_GGML_Q8_0) {
            if ((t->dims[0] % 32ULL) != 0ULL || mul_u64_checked(numel / 32ULL, 34ULL, &bytes64) != 0) {
                gs_close(&s);
                fprintf(stderr, "GGUF_PARSE_ERROR: Q8_0 tensor '%s' inner dim not multiple of 32\n", t->name);
                free_llama_model(m);
                return -1;
            }
        } else if (t->ggml_type == ZCC_GGML_Q4_K) {
            if ((t->dims[0] % 256ULL) != 0ULL || mul_u64_checked(numel / 256ULL, 144ULL, &bytes64) != 0) {
                gs_close(&s);
                fprintf(stderr, "GGUF_PARSE_ERROR: Q4_K tensor '%s' inner dim not multiple of 256\n", t->name);
                free_llama_model(m);
                return -1;
            }
        } else if (t->ggml_type == ZCC_GGML_Q6_K) {
            if ((t->dims[0] % 256ULL) != 0ULL || mul_u64_checked(numel / 256ULL, 210ULL, &bytes64) != 0) {
                gs_close(&s);
                fprintf(stderr, "GGUF_PARSE_ERROR: Q6_K tensor '%s' inner dim not multiple of 256\n", t->name);
                free_llama_model(m);
                return -1;
            }
        } else {
            gs_close(&s);
            fprintf(stderr, "GGUF_PARSE_ERROR: unsupported ggml_type %u on '%s'\n", t->ggml_type, t->name);
            free_llama_model(m);
            return -1;
        }
        if (bytes64 > s.fsz64) {
            gs_close(&s);
            fprintf(stderr, "GGUF_PARSE_ERROR: tensor '%s' size exceeds file\n", t->name);
            free_llama_model(m);
            return -1;
        }
        t->nbytes = bytes64;
    }

    uint64_t align_m1 = (uint64_t)(m->align - 1U);
    if (s.pos > 0xFFFFFFFFFFFFFFFFULL - align_m1) { gs_close(&s); free_llama_model(m); return -1; }
    uint64_t data_start = (s.pos + align_m1) & ~align_m1;
    if (data_start > s.fsz64 || (data_start > s.pos && gs_skip_u64(&s, data_start - s.pos) != 0)) {
        gs_close(&s); free_llama_model(m);
        fprintf(stderr, "GGUF_PARSE_ERROR: metadata extends past EOF\n");
        return -1;
    }
    m->data_start = data_start;
    m->wblob_bytes = s.fsz64 - data_start;

    for (unsigned int ti = 0U; ti < m->num_tensors; ti++) {
        GgufTensor *t = &m->tensors[ti];
        if (t->offset > m->wblob_bytes || t->nbytes > m->wblob_bytes - t->offset) {
            gs_close(&s);
            fprintf(stderr, "GGUF_PARSE_ERROR: tensor '%s' data exceeds EOF\n", t->name);
            free_llama_model(m);
            return -1;
        }
    }

    const GgufTensor *emb = find_tensor(m, "token_embd.weight");
    if (!emb || emb->n_dims != 2U || emb->dims[0] != (uint64_t)m->dim) {
        gs_close(&s); free_llama_model(m);
        fprintf(stderr, "GGUF_PARSE_ERROR: missing/invalid token_embd.weight\n");
        return -1;
    }
    if (m->vocab_size == 0U) {
        if (emb->dims[1] == 0ULL || emb->dims[1] > 1048576ULL) { gs_close(&s); free_llama_model(m); return -1; }
        m->vocab_size = (unsigned int)emb->dims[1];
    } else if (emb->dims[1] != (uint64_t)m->vocab_size) {
        gs_close(&s); free_llama_model(m);
        fprintf(stderr, "GGUF_PARSE_ERROR: token_embd.weight vocab dim mismatch\n");
        return -1;
    }

    if (use_aot && check_aot_structural_match(m, aot_meta) != 0) {
        gs_close(&s); free_llama_model(m); return -1;
    }

    size_t vram_free = 0U, vram_total = 0U;
    check_cu(cuMemGetInfo_v2(&vram_free, &vram_total), "cuMemGetInfo_v2");
    int do_spill = 0;
    uint64_t eff_vram_budget = 0ULL;
    if (allow_spill && m->num_tensors > 0U && m->wblob_bytes > 0ULL) {
        uint64_t want_vram = m->wblob_bytes;
        if (vram_free > 0U && want_vram + reserve_vram_bytes > (uint64_t)vram_free) {
            want_vram = ((uint64_t)vram_free > reserve_vram_bytes)
                        ? ((uint64_t)vram_free - reserve_vram_bytes) : 0ULL;
        }
        if (want_vram > 67108864ULL && reserve_vram_bytes > 0ULL) {
            uint64_t hi = want_vram, lo = 0ULL, best = 0ULL;
            uint64_t pbuf = 0ULL;
            int fits = 0;
            if (cuMemAlloc_v2(&pbuf, (size_t)hi) == 0) {
                if (cuMemsetD8_v2(pbuf, 0, (size_t)hi) == 0 && cuCtxSynchronize() == 0) {
                    size_t pf = 0U, pt = 0U;
                    if (cuMemGetInfo_v2(&pf, &pt) == 0 && (uint64_t)pf >= reserve_vram_bytes) fits = 1;
                }
                cuMemFree_v2(pbuf);
                cuCtxSynchronize();
            }
            if (fits) {
                best = hi;
            } else {
                for (int it = 0; it < 8; it++) {
                    uint64_t mid = lo + ((hi - lo) >> 1);
                    if (mid < 67108864ULL) { hi = mid; continue; }
                    pbuf = 0ULL;
                    int ok = 0;
                    if (cuMemAlloc_v2(&pbuf, (size_t)mid) == 0) {
                        if (cuMemsetD8_v2(pbuf, 0, (size_t)mid) == 0 && cuCtxSynchronize() == 0) {
                            size_t pf = 0U, pt = 0U;
                            if (cuMemGetInfo_v2(&pf, &pt) == 0 && (uint64_t)pf >= reserve_vram_bytes) ok = 1;
                        }
                        cuMemFree_v2(pbuf);
                        cuCtxSynchronize();
                    }
                    if (ok) { best = mid; lo = mid; }
                    else { hi = mid; }
                }
            }
            eff_vram_budget = best;
            if (eff_vram_budget < m->wblob_bytes) do_spill = 1;
        } else if (vram_free > 0U && m->wblob_bytes + reserve_vram_bytes > (uint64_t)vram_free) {
            eff_vram_budget = ((uint64_t)vram_free > reserve_vram_bytes)
                              ? ((uint64_t)vram_free - reserve_vram_bytes) : 0ULL;
            do_spill = 1;
        }
    }

    if (!do_spill && vram_free > 0U && m->wblob_bytes > (uint64_t)vram_free) {
        char req_s[32], free_s[32];
        fmt_u64_dec(m->wblob_bytes, req_s, sizeof(req_s));
        fmt_u64_dec((uint64_t)vram_free, free_s, sizeof(free_s));
        gs_close(&s); free_llama_model(m);
        fprintf(stderr, "VRAM_BUDGET_EXCEEDED: weights=%sB > free=%sB\n", req_s, free_s);
        return -1;
    }

    if (do_spill) {
        uint64_t vram_budget = eff_vram_budget;
        uint64_t cur_vram = 0ULL;
        for (unsigned int ti = 0U; ti < m->num_tensors; ti++) {
            m->tensors[ti].on_host = 0;
            m->tensors[ti].dst_off = 0ULL;
            cur_vram += align32_u64(m->tensors[ti].nbytes);
        }
        /* Pass 1: Move untied token_embd.weight (read 1 row/tok) to zero-copy pinned host first */
        if (cur_vram > vram_budget && find_tensor(m, "output.weight") != NULL) {
            spill_mark_name(m, "token_embd.weight", &cur_vram);
        }
        /* Pass 2a: Strided FFN Q4_K projection spilling across layers >= 2 so
         * max_spill_mat stays small (e.g. 38 MB vs 56 MB Q6_K ffn_down) and
         * async HtoD DMA transfers overlap 2-4 layers of GPU compute. */
        static const char *const k_strided_names[2] = { "ffn_gate.weight", "ffn_up.weight" };
        static const int k_stride_offs[4] = { 3, 1, 2, 0 };
        for (int ni = 0; ni < 2 && cur_vram > vram_budget; ni++) {
            for (int ph = 0; ph < 4 && cur_vram > vram_budget; ph++) {
                for (int l = (int)m->n_layers - 1; l >= 2 && cur_vram > vram_budget; l--) {
                    if ((l & 3) != k_stride_offs[ph]) continue;
                    char tn[128];
                    snprintf(tn, sizeof(tn), "blk.%d.%s", l, k_strided_names[ni]);
                    spill_mark_name(m, tn, &cur_vram);
                }
            }
        }
        /* Pass 2b: Fallback spill remaining tail layer matrices if still over budget */
        for (int l = (int)m->n_layers - 1; l >= 0 && cur_vram > vram_budget; l--) {
            static const char *const k_lw_names[7] = {
                "attn_q.weight", "attn_k.weight", "attn_v.weight", "attn_output.weight",
                "ffn_gate.weight", "ffn_up.weight", "ffn_down.weight"
            };
            char tn[128];
            for (int w = 0; w < 7 && cur_vram > vram_budget; w++) {
                snprintf(tn, sizeof(tn), "blk.%d.%s", l, k_lw_names[w]);
                spill_mark_name(m, tn, &cur_vram);
            }
        }
        /* Pass 3: Fallback spill largest remaining tensors if needed */
        while (cur_vram > vram_budget) {
            int best_i = -1;
            uint64_t best_sz = 0ULL;
            for (unsigned int ti = 0U; ti < m->num_tensors; ti++) {
                if (!m->tensors[ti].on_host && m->tensors[ti].nbytes > best_sz) {
                    best_sz = m->tensors[ti].nbytes;
                    best_i = (int)ti;
                }
            }
            if (best_i < 0) break;
            spill_mark_idx(m, (unsigned int)best_i, &cur_vram);
        }

        uint64_t vram_off = 0ULL, spill_off = 0ULL;
        for (unsigned int ti = 0U; ti < m->num_tensors; ti++) {
            uint64_t ab = align32_u64(m->tensors[ti].nbytes);
            if (m->tensors[ti].on_host) {
                m->tensors[ti].dst_off = spill_off;
                spill_off += ab;
            } else {
                m->tensors[ti].dst_off = vram_off;
                vram_off += ab;
            }
        }

        if (vram_off > 0ULL) {
            check_cu(cuMemAlloc_v2(&m->d_wblob, (size_t)vram_off), "cuMemAlloc(wblob_vram)");
        }
        if (spill_off > 0ULL && !g_zcc_llm_failed) {
            check_cu(cuMemHostAlloc(&m->h_spill_blob, (size_t)spill_off, 2U), "cuMemHostAlloc(wblob_zc)");
            if (!g_zcc_llm_failed) {
                check_cu(cuMemHostGetDevicePointer_v2(&m->d_spill_blob, m->h_spill_blob, 0U), "cuMemHostGetDevicePointer(wblob_zc)");
            }
        }
        if (g_zcc_llm_failed) {
            free_model_gpu_mem(m);
            gs_close(&s); free_llama_model(m);
            return -1;
        }

        unsigned int *order = (unsigned int *)malloc((size_t)m->num_tensors * sizeof(unsigned int));
        size_t stage_cap = 4194304U;
        uint8_t *stage = (uint8_t *)malloc(stage_cap);
        if (!order || !stage) {
            if (order) free(order);
            if (stage) free(stage);
            free_model_gpu_mem(m);
            gs_close(&s); free_llama_model(m);
            fprintf(stderr, "FATAL: OOM during hybrid weight streaming\n");
            return -1;
        }
        for (unsigned int ti = 0U; ti < m->num_tensors; ti++) order[ti] = ti;
        for (unsigned int i = 1U; i < m->num_tensors; i++) {
            unsigned int key = order[i];
            uint64_t koff = m->tensors[key].offset;
            unsigned int j = i;
            while (j > 0U && m->tensors[order[j - 1U]].offset > koff) {
                order[j] = order[j - 1U];
                j--;
            }
            order[j] = key;
        }

        uint64_t cur_off = 0ULL;
        for (unsigned int k = 0U; k < m->num_tensors; k++) {
            GgufTensor *t = &m->tensors[order[k]];
            if (t->offset < cur_off || (t->offset > cur_off && gs_skip_u64(&s, t->offset - cur_off) != 0)) {
                free(order); free(stage); free_model_gpu_mem(m);
                gs_close(&s); free_llama_model(m);
                fprintf(stderr, "GGUF_PARSE_ERROR: stream offset mismatch during hybrid upload\n");
                return -1;
            }
            cur_off = t->offset;
            uint64_t done = 0ULL;
            while (done < t->nbytes) {
                uint64_t rem = t->nbytes - done;
                size_t take = (rem < (uint64_t)stage_cap) ? (size_t)rem : stage_cap;
                if (gs_rd_bytes(&s, stage, take) != 0) {
                    free(order); free(stage); free_model_gpu_mem(m);
                    gs_close(&s); free_llama_model(m);
                    fprintf(stderr, "GGUF_PARSE_ERROR: stream truncated during hybrid weight upload\n");
                    return -1;
                }
                if (t->on_host) {
                    memcpy((uint8_t *)m->h_spill_blob + t->dst_off + done, stage, take);
                } else {
                    check_cu(cuMemcpyHtoD_v2(m->d_wblob + t->dst_off + done, stage, take), "cuMemcpyHtoD(wblob_chunk)");
                    if (g_zcc_llm_failed) {
                        free(order); free(stage); free_model_gpu_mem(m);
                        gs_close(&s); free_llama_model(m);
                        return -1;
                    }
                }
                done += (uint64_t)take;
            }
            cur_off += t->nbytes;
            t->d_ptr = t->on_host ? (m->d_spill_blob + t->dst_off) : (m->d_wblob + t->dst_off);
        }
        if (cur_off < m->wblob_bytes && gs_skip_u64(&s, m->wblob_bytes - cur_off) != 0) {
            free(order); free(stage); free_model_gpu_mem(m);
            gs_close(&s); free_llama_model(m);
            return -1;
        }
        free(order);
        free(stage);
        m->vram_wblob_bytes = vram_off;
        m->spill_wblob_bytes = spill_off;
    } else if (m->wblob_bytes > 0ULL) {
        /* Stream weights in 4 MiB chunks into one contiguous VRAM buffer while hashing */
        check_cu(cuMemAlloc_v2(&m->d_wblob, (size_t)m->wblob_bytes), "cuMemAlloc(wblob)");
        if (g_zcc_llm_failed) {
            free_model_gpu_mem(m);
            gs_close(&s); free_llama_model(m);
            return -1;
        }
        size_t stage_cap = 4194304U;
        uint8_t *stage = (uint8_t *)malloc(stage_cap);
        if (!stage) {
            free_model_gpu_mem(m);
            gs_close(&s); free_llama_model(m); return -1;
        }
        uint64_t uploaded = 0ULL;
        while (uploaded < m->wblob_bytes) {
            uint64_t rem = m->wblob_bytes - uploaded;
            size_t take = (rem < (uint64_t)stage_cap) ? (size_t)rem : stage_cap;
            if (gs_rd_bytes(&s, stage, take) != 0) {
                free(stage);
                free_model_gpu_mem(m);
                gs_close(&s); free_llama_model(m);
                fprintf(stderr, "GGUF_PARSE_ERROR: stream truncated during weight upload\n");
                return -1;
            }
            check_cu(cuMemcpyHtoD_v2(m->d_wblob + uploaded, stage, take), "cuMemcpyHtoD(wblob_chunk)");
            if (g_zcc_llm_failed) {
                free(stage);
                free_model_gpu_mem(m);
                gs_close(&s); free_llama_model(m);
                return -1;
            }
            uploaded += (uint64_t)take;
        }
        free(stage);
        m->vram_wblob_bytes = m->wblob_bytes;
        m->spill_wblob_bytes = 0ULL;
        for (unsigned int ti = 0U; ti < m->num_tensors; ti++) {
            m->tensors[ti].d_ptr = m->d_wblob + m->tensors[ti].offset;
        }
    }
    gs_drain_eof(&s);
    if (expected_sha256 != NULL) {
        uint8_t actual_sha[32];
        zcc_sha256_final(&s.sha_ctx, actual_sha);
        if ((slice_len > 0ULL && s.pos != slice_len) || memcmp(actual_sha, expected_sha256, 32U) != 0) {
            char exp_hex[65], got_hex[65];
            sha256_to_hex(expected_sha256, exp_hex);
            sha256_to_hex(actual_sha, got_hex);
            fprintf(stderr, "TAMPER_DETECTED: GGUF SHA-256 mismatch (expected=%s got=%s)\n", exp_hex, got_hex);
            gs_close(&s);
            free_model_gpu_mem(m);
            free_llama_model(m);
            return -1;
        }
    }
    m->file_fingerprint = s.fp64;
    gs_close(&s);

    if (use_aot && aot_meta[1] != m->file_fingerprint) {
        fprintf(stderr, "AOT_FINGERPRINT_MISMATCH: expected=0x%08x%08x got=0x%08x%08x\n",
                (unsigned int)(aot_meta[1] >> 32), (unsigned int)(aot_meta[1] & 0xFFFFFFFFU),
                (unsigned int)(m->file_fingerprint >> 32), (unsigned int)(m->file_fingerprint & 0xFFFFFFFFU));
        free_model_gpu_mem(m);
        free_llama_model(m);
        return -1;
    }

    m->d_tok_emb = emb->d_ptr;
    m->off_tok_emb = emb->offset;
    m->t_tok_emb = emb->ggml_type;

    if (require_tensor_1d(m, "output_norm.weight", m->dim, &m->d_out_norm, &m->off_out_norm, &m->t_out_norm) != 0) {
        free_model_gpu_mem(m);
        free_llama_model(m); return -1;
    }
    const GgufTensor *wout = find_tensor(m, "output.weight");
    if (wout) {
        if (wout->n_dims != 2U || wout->dims[0] != (uint64_t)m->dim || wout->dims[1] != (uint64_t)m->vocab_size) {
            free_model_gpu_mem(m);
            free_llama_model(m);
            fprintf(stderr, "GGUF_PARSE_ERROR: output.weight shape mismatch\n");
            return -1;
        }
        m->d_wout = wout->d_ptr;
        m->off_wout = wout->offset;
        m->t_wout = wout->ggml_type;
    } else {
        m->d_wout = m->d_tok_emb;
        m->off_wout = m->off_tok_emb;
        m->t_wout = m->t_tok_emb;
    }

    for (unsigned int l = 0U; l < m->n_layers; l++) {
        char tn[128];
        LayerWeights *lw = &m->layers[l];
        snprintf(tn, sizeof(tn), "blk.%u.attn_norm.weight", l);
        if (require_tensor_1d(m, tn, m->dim, &lw->d_attn_norm, &lw->off_attn_norm, &lw->t_attn_norm) != 0) goto fail_tensors;
        snprintf(tn, sizeof(tn), "blk.%u.attn_q.weight", l);
        if (require_tensor_2d(m, tn, m->dim, m->dim, &lw->d_wq, &lw->off_wq, &lw->t_wq) != 0) goto fail_tensors;
        snprintf(tn, sizeof(tn), "blk.%u.attn_k.weight", l);
        if (require_tensor_2d(m, tn, m->dim, m->kv_dim, &lw->d_wk, &lw->off_wk, &lw->t_wk) != 0) goto fail_tensors;
        snprintf(tn, sizeof(tn), "blk.%u.attn_v.weight", l);
        if (require_tensor_2d(m, tn, m->dim, m->kv_dim, &lw->d_wv, &lw->off_wv, &lw->t_wv) != 0) goto fail_tensors;
        snprintf(tn, sizeof(tn), "blk.%u.attn_output.weight", l);
        if (require_tensor_2d(m, tn, m->dim, m->dim, &lw->d_wo, &lw->off_wo, &lw->t_wo) != 0) goto fail_tensors;
        snprintf(tn, sizeof(tn), "blk.%u.ffn_norm.weight", l);
        if (require_tensor_1d(m, tn, m->dim, &lw->d_ffn_norm, &lw->off_ffn_norm, &lw->t_ffn_norm) != 0) goto fail_tensors;
        snprintf(tn, sizeof(tn), "blk.%u.ffn_gate.weight", l);
        if (require_tensor_2d(m, tn, m->dim, m->hidden_dim, &lw->d_wgate, &lw->off_wgate, &lw->t_wgate) != 0) goto fail_tensors;
        snprintf(tn, sizeof(tn), "blk.%u.ffn_up.weight", l);
        if (require_tensor_2d(m, tn, m->dim, m->hidden_dim, &lw->d_wup, &lw->off_wup, &lw->t_wup) != 0) goto fail_tensors;
        snprintf(tn, sizeof(tn), "blk.%u.ffn_down.weight", l);
        if (require_tensor_2d(m, tn, m->hidden_dim, m->dim, &lw->d_wdown, &lw->off_wdown, &lw->t_wdown) != 0) goto fail_tensors;

        uint32_t bt = 0U;
        snprintf(tn, sizeof(tn), "blk.%u.attn_q.bias", l);
        if (find_tensor(m, tn)) {
            if (require_tensor_1d(m, tn, m->dim, &lw->d_bq, &lw->off_bq, &bt) != 0) goto fail_tensors;
            lw->has_bq = 1; m->has_any_bias = 1;
        }
        snprintf(tn, sizeof(tn), "blk.%u.attn_k.bias", l);
        if (find_tensor(m, tn)) {
            if (require_tensor_1d(m, tn, m->kv_dim, &lw->d_bk, &lw->off_bk, &bt) != 0) goto fail_tensors;
            lw->has_bk = 1; m->has_any_bias = 1;
        }
        snprintf(tn, sizeof(tn), "blk.%u.attn_v.bias", l);
        if (find_tensor(m, tn)) {
            if (require_tensor_1d(m, tn, m->kv_dim, &lw->d_bv, &lw->off_bv, &bt) != 0) goto fail_tensors;
            lw->has_bv = 1; m->has_any_bias = 1;
        }
    }
    m->weight_type_primary = m->layers[0].t_wq;

    if (finalize_tokenizer(&m->tok) != 0) {
        goto fail_tensors;
    }
    return 0;

fail_tensors:
    free_model_gpu_mem(m);
    free_llama_model(m);
    return -1;
}

static int parse_and_stream_gguf(
    const char *path,
    const uint8_t *emb_mem,
    size_t emb_len,
    LlamaModel *m,
    int use_aot,
    const uint64_t *aot_meta,
    int allow_spill,
    uint64_t reserve_vram_bytes
) {
    return parse_and_stream_gguf_ex(path, emb_mem, emb_len, 0ULL, 0ULL, NULL,
                                    m, use_aot, aot_meta, allow_spill, reserve_vram_bytes);
}

#define ZCC_KV_F32  0
#define ZCC_KV_F16  1
#define ZCC_KV_Q8_0 2

static const char *kv_type_name_str(int kv_type) {
    if (kv_type == ZCC_KV_F16) return "f16";
    if (kv_type == ZCC_KV_Q8_0) return "q8_0";
    return "f32";
}

/* Generic mode kernel handles (47 kernels in win64_gpu_llm_kernels.c) */
static void *fn_embed_f32 = NULL, *fn_embed_f16 = NULL, *fn_embed_q8_0 = NULL;
static void *fn_embed_q4_0 = NULL, *fn_embed_bf16 = NULL, *fn_embed_q4_k = NULL, *fn_embed_q6_k = NULL;
static void *fn_rmsnorm = NULL, *fn_f32_to_f16 = NULL, *fn_gemm_tc = NULL;
static void *fn_matvec_f32 = NULL, *fn_matvec_f16 = NULL, *fn_matvec_q8_0 = NULL;
static void *fn_matvec_q4_0 = NULL, *fn_matvec_bf16 = NULL, *fn_matvec_q4_k = NULL, *fn_matvec_q6_k = NULL;
static void *fn_rope = NULL, *fn_rope_neox = NULL, *fn_kv_store = NULL, *fn_attn_gqa = NULL, *fn_attn_flash = NULL;
static void *fn_swiglu = NULL, *fn_add = NULL, *fn_add_bias = NULL, *fn_softmax = NULL;
static void *fn_kv_store_f16 = NULL, *fn_kv_store_q8_0 = NULL;
static void *fn_attn_flash_f16 = NULL, *fn_attn_flash_q8_0 = NULL;
static void *fn_dequant_f32_to_f16 = NULL, *fn_dequant_bf16_to_f16 = NULL, *fn_dequant_q8_0_to_f16 = NULL;
static void *fn_dequant_q4_0_to_f16 = NULL, *fn_dequant_q4_k_to_f16 = NULL, *fn_dequant_q6_k_to_f16 = NULL;
static void *fn_scatter_tile = NULL;
static void *fn_rmsnorm_batch = NULL, *fn_add_bias_batch = NULL, *fn_rope_batch = NULL, *fn_rope_neox_batch = NULL;
static void *fn_kv_store_batch_f32 = NULL, *fn_kv_store_batch_f16 = NULL, *fn_kv_store_batch_q8_0 = NULL;
static void *fn_attn_flash_batch_f32 = NULL, *fn_attn_flash_batch_f16 = NULL, *fn_attn_flash_batch_q8_0 = NULL;

typedef struct {
    uint64_t x, xb, xb2, x_f16;
    uint64_t q, k, v, att_scratch;
    uint64_t k_cache, v_cache;
    uint64_t hb, hb2;
    uint64_t logits, probs;
    uint64_t step_args;
    uint64_t w_f16_tmp, prefill_tmp, batch_toks;
    uint64_t argmax_ids, argmax_vals, draft_ids;
    uint64_t spill_stage[2];
    size_t spill_stage_bytes;
    int spill_slots;
    void *dma_stream;
    void *dma_ev[2];
    void *comp_ev[2];
} GpuActivations;

static void gpu_linear_gen(
    uint64_t w_dptr,
    uint32_t w_type,
    uint64_t x_dptr,
    uint64_t x_f16_dptr,
    uint64_t y_dptr,
    unsigned int N,
    unsigned int K,
    int use_tensor_core
) {
    if (w_type == ZCC_GGML_F16 && use_tensor_core && (K % 16U) == 0U) {
        unsigned int blk = 256U;
        unsigned int grd = (K + blk - 1U) / blk;
        void *p_cvt[3] = { &x_dptr, &x_f16_dptr, &K };
        check_cu(cuLaunchKernel(fn_f32_to_f16, grd, 1U, 1U, blk, 1U, 1U, 0U, NULL, p_cvt, NULL), "f32_to_f16");
        unsigned int one = 1U;
        unsigned int gx = (N + 7U) / 8U;
        void *p_tc[6] = { &x_f16_dptr, &w_dptr, &y_dptr, &one, &N, &K };
        check_cu(cuLaunchKernel(fn_gemm_tc, gx, 1U, 1U, 32U, 1U, 1U, 0U, NULL, p_tc, NULL), "gemm_tensor_f16");
        return;
    }
    void *fn = fn_matvec_f32;
    if (w_type == ZCC_GGML_F16) fn = fn_matvec_f16;
    else if (w_type == ZCC_GGML_Q4_0) fn = fn_matvec_q4_0;
    else if (w_type == ZCC_GGML_Q8_0) fn = fn_matvec_q8_0;
    else if (w_type == ZCC_GGML_Q4_K) fn = fn_matvec_q4_k;
    else if (w_type == ZCC_GGML_Q6_K) fn = fn_matvec_q6_k;
    else if (w_type == ZCC_GGML_BF16) fn = fn_matvec_bf16;
    void *p_mv[5] = { &w_dptr, &x_dptr, &y_dptr, &N, &K };
    check_cu(cuLaunchKernel(fn, N, 1U, 1U, 32U, 1U, 1U, 0U, NULL, p_mv, NULL), "matvec");
}

static void gpu_forward_token(
    const LlamaModel *m,
    const GpuActivations *act,
    unsigned int tok,
    unsigned int pos,
    unsigned int eff_max_seq,
    int use_tensor_core,
    int kv_type
) {
    unsigned int dim = m->dim;
    unsigned int hidden = m->hidden_dim;
    unsigned int kv_dim = m->kv_dim;
    unsigned int vocab = m->vocab_size;
    float eps = m->rms_eps;
    float theta = m->rope_theta;

    unsigned int blk_dim = 256U;
    unsigned int grd_dim = (dim + blk_dim - 1U) / blk_dim;
    unsigned int grd_hid = (hidden + blk_dim - 1U) / blk_dim;
    unsigned int grd_kv = (kv_dim + blk_dim - 1U) / blk_dim;
    unsigned int total_pairs = (m->n_heads + m->n_kv_heads) * (m->head_dim >> 1);
    unsigned int grd_rope = (total_pairs + blk_dim - 1U) / blk_dim;

    void *emb_fn = fn_embed_f32;
    if (m->t_tok_emb == ZCC_GGML_F16) emb_fn = fn_embed_f16;
    else if (m->t_tok_emb == ZCC_GGML_Q4_0) emb_fn = fn_embed_q4_0;
    else if (m->t_tok_emb == ZCC_GGML_Q8_0) emb_fn = fn_embed_q8_0;
    else if (m->t_tok_emb == ZCC_GGML_Q4_K) emb_fn = fn_embed_q4_k;
    else if (m->t_tok_emb == ZCC_GGML_Q6_K) emb_fn = fn_embed_q6_k;
    else if (m->t_tok_emb == ZCC_GGML_BF16) emb_fn = fn_embed_bf16;
    uint64_t emb_ptr = m->d_tok_emb;
    uint64_t x_ptr = act->x;
    void *p_emb[4] = { &emb_ptr, &x_ptr, &tok, &dim };
    check_cu(cuLaunchKernel(emb_fn, grd_dim, 1U, 1U, blk_dim, 1U, 1U, 0U, NULL, p_emb, NULL), "embed");

    for (unsigned int l = 0U; l < m->n_layers; l++) {
        const LayerWeights *lw = &m->layers[l];
        uint64_t xb_ptr = act->xb, xb2_ptr = act->xb2, xf16_ptr = act->x_f16;
        uint64_t q_ptr = act->q, k_ptr = act->k, v_ptr = act->v;
        uint64_t kc_ptr = act->k_cache, vc_ptr = act->v_cache, att_ptr = act->att_scratch;
        uint64_t hb_ptr = act->hb, hb2_ptr = act->hb2;

        uint64_t an_ptr = lw->d_attn_norm;
        void *p_rn1[5] = { &xb_ptr, &x_ptr, &an_ptr, &dim, &eps };
        check_cu(cuLaunchKernel(fn_rmsnorm, 1U, 1U, 1U, 32U, 1U, 1U, 0U, NULL, p_rn1, NULL), "rmsnorm_attn");

        gpu_linear_gen(lw->d_wq, lw->t_wq, xb_ptr, xf16_ptr, q_ptr, dim, dim, use_tensor_core);
        gpu_linear_gen(lw->d_wk, lw->t_wk, xb_ptr, xf16_ptr, k_ptr, kv_dim, dim, use_tensor_core);
        gpu_linear_gen(lw->d_wv, lw->t_wv, xb_ptr, xf16_ptr, v_ptr, kv_dim, dim, use_tensor_core);

        if (lw->has_bq) {
            uint64_t bq = lw->d_bq;
            void *p_bq[3] = { &q_ptr, &bq, &dim };
            check_cu(cuLaunchKernel(fn_add_bias, grd_dim, 1U, 1U, blk_dim, 1U, 1U, 0U, NULL, p_bq, NULL), "add_bq");
        }
        if (lw->has_bk) {
            uint64_t bk = lw->d_bk;
            void *p_bk[3] = { &k_ptr, &bk, &kv_dim };
            check_cu(cuLaunchKernel(fn_add_bias, grd_kv, 1U, 1U, blk_dim, 1U, 1U, 0U, NULL, p_bk, NULL), "add_bk");
        }
        if (lw->has_bv) {
            uint64_t bv = lw->d_bv;
            void *p_bv[3] = { &v_ptr, &bv, &kv_dim };
            check_cu(cuLaunchKernel(fn_add_bias, grd_kv, 1U, 1U, blk_dim, 1U, 1U, 0U, NULL, p_bv, NULL), "add_bv");
        }

        unsigned int nh = m->n_heads, nkv = m->n_kv_heads, hd = m->head_dim, mseq = eff_max_seq;
        void *rfn = (strcmp(m->arch, "qwen2") == 0) ? fn_rope_neox : fn_rope;
        void *p_rope[7] = { &q_ptr, &k_ptr, &pos, &nh, &nkv, &hd, &theta };
        check_cu(cuLaunchKernel(rfn, grd_rope, 1U, 1U, blk_dim, 1U, 1U, 0U, NULL, p_rope, NULL), "rope");

        void *p_kvs[8] = { &k_ptr, &v_ptr, &kc_ptr, &vc_ptr, &l, &pos, &mseq, &kv_dim };
        void *p_fa[10] = { &q_ptr, &kc_ptr, &vc_ptr, &xb_ptr, &l, &pos, &mseq, &nh, &nkv, &hd };
        if (kv_type == ZCC_KV_F16) {
            check_cu(cuLaunchKernel(fn_kv_store_f16, grd_kv, 1U, 1U, blk_dim, 1U, 1U, 0U, NULL, p_kvs, NULL), "kv_store_f16");
            check_cu(cuLaunchKernel(fn_attn_flash_f16, nh, 1U, 1U, 32U, 1U, 1U, 0U, NULL, p_fa, NULL), "attn_flash_f16");
        } else if (kv_type == ZCC_KV_Q8_0) {
            unsigned int nb_kv2 = (kv_dim >> 5) * 2U;
            check_cu(cuLaunchKernel(fn_kv_store_q8_0, nb_kv2, 1U, 1U, 32U, 1U, 1U, 0U, NULL, p_kvs, NULL), "kv_store_q8_0");
            check_cu(cuLaunchKernel(fn_attn_flash_q8_0, nh, 1U, 1U, 32U, 1U, 1U, 0U, NULL, p_fa, NULL), "attn_flash_q8_0");
        } else {
            check_cu(cuLaunchKernel(fn_kv_store, grd_kv, 1U, 1U, blk_dim, 1U, 1U, 0U, NULL, p_kvs, NULL), "kv_store");
            if (hd <= 256U) {
                check_cu(cuLaunchKernel(fn_attn_flash, nh, 1U, 1U, 32U, 1U, 1U, 0U, NULL, p_fa, NULL), "attn_flash");
            } else {
                void *p_att[11] = { &q_ptr, &kc_ptr, &vc_ptr, &att_ptr, &xb_ptr, &l, &pos, &mseq, &nh, &nkv, &hd };
                check_cu(cuLaunchKernel(fn_attn_gqa, nh, 1U, 1U, 32U, 1U, 1U, 0U, NULL, p_att, NULL), "attn_gqa");
            }
        }

        gpu_linear_gen(lw->d_wo, lw->t_wo, xb_ptr, xf16_ptr, xb2_ptr, dim, dim, use_tensor_core);

        void *p_add1[3] = { &x_ptr, &xb2_ptr, &dim };
        check_cu(cuLaunchKernel(fn_add, grd_dim, 1U, 1U, blk_dim, 1U, 1U, 0U, NULL, p_add1, NULL), "add_attn");

        uint64_t fn_ptr = lw->d_ffn_norm;
        void *p_rn2[5] = { &xb_ptr, &x_ptr, &fn_ptr, &dim, &eps };
        check_cu(cuLaunchKernel(fn_rmsnorm, 1U, 1U, 1U, 32U, 1U, 1U, 0U, NULL, p_rn2, NULL), "rmsnorm_ffn");

        gpu_linear_gen(lw->d_wgate, lw->t_wgate, xb_ptr, xf16_ptr, hb_ptr, hidden, dim, use_tensor_core);
        gpu_linear_gen(lw->d_wup, lw->t_wup, xb_ptr, xf16_ptr, hb2_ptr, hidden, dim, use_tensor_core);

        void *p_sw[3] = { &hb_ptr, &hb2_ptr, &hidden };
        check_cu(cuLaunchKernel(fn_swiglu, grd_hid, 1U, 1U, blk_dim, 1U, 1U, 0U, NULL, p_sw, NULL), "swiglu");

        gpu_linear_gen(lw->d_wdown, lw->t_wdown, hb_ptr, xf16_ptr, xb2_ptr, dim, hidden, use_tensor_core);

        void *p_add2[3] = { &x_ptr, &xb2_ptr, &dim };
        check_cu(cuLaunchKernel(fn_add, grd_dim, 1U, 1U, blk_dim, 1U, 1U, 0U, NULL, p_add2, NULL), "add_ffn");
    }

    uint64_t on_ptr = m->d_out_norm, xb_ptr = act->xb, xf16_ptr = act->x_f16;
    uint64_t log_ptr = act->logits;
    void *p_rnf[5] = { &xb_ptr, &x_ptr, &on_ptr, &dim, &eps };
    check_cu(cuLaunchKernel(fn_rmsnorm, 1U, 1U, 1U, 32U, 1U, 1U, 0U, NULL, p_rnf, NULL), "rmsnorm_out");

    gpu_linear_gen(m->d_wout, m->t_wout, xb_ptr, xf16_ptr, log_ptr, vocab, dim, use_tensor_core);
}

/* ========================================================================= */
/* Tier B Multi-CTA AOT (ZCCAOT02) + CUDA Graph Execution Plan               */
/* ========================================================================= */
typedef struct {
    void *fn;
    unsigned int gx, gy, gz;
    unsigned int bx, by, bz;
    uint64_t u64_a[8];
    uint32_t u32_a[4];
    void *kparams[12];
    const void *dma_src_host;
    size_t dma_bytes;
    int dma_slot;
} McNode;

typedef struct {
    void *fn_embed;
    void *fn_rmsnorm;
    void *fn_f32_to_f16;
    void *fn_gemm_tc;
    void *fn_matvec[32];
    void *fn_matvec_multi[32];
    void *fn_matvec_multi4[32];
    void *fn_argmax;
    void *fn_draft_chain;
    void *fn_add_bias;
    void *fn_rope_kv;
    void *fn_rope_inplace;
    void *fn_kv_store_q8_0;
    void *fn_attn_gqa;
    void *fn_swiglu;
    int kv_type;
    int has_argmax;
    int has_draft_chain;
    McNode *nodes;
    unsigned int num_nodes;
    unsigned int cap_nodes;
    unsigned int graph_nodes;
    uint64_t spill_stage;
    void *graph_exec;
    int use_graph;
} McPlan;

typedef struct {
    int ready;
    void *fn_rmsnorm_batch;
    void *fn_add_bias_batch;
    void *fn_prefill_tc_gemm;
    void *fn_rope_kv_batch;
    void *fn_rope_inplace_batch;
    void *fn_kv_store_batch_q8_0;
    void *fn_attn_batch;
    void *fn_swiglu_batch;
    void *fn_dequant[32];
} McPrefillFns;

typedef struct {
    unsigned int start;
    unsigned int count;
    int is_dma;
    void *graph_exec;
} McVerifySeg;

typedef struct {
    int ready;
    unsigned int M;
    McNode *nodes;
    unsigned int num_nodes;
    unsigned int cap_nodes;
    McVerifySeg *segs;
    unsigned int num_segs;
    uint64_t spill_stage[2];
    int spill_slots;
    void *dma_stream;
    void *dma_ev[2];
    void *comp_ev[2];
    unsigned int *dma_order;
    unsigned int num_dma;
    int use_graph;
} McVerifyPlan;

typedef struct {
    int cycles;
    unsigned int drafts_proposed;
    unsigned int drafts_accepted;
    unsigned int target_tokens;
    unsigned int verify_launches;
    unsigned int final_k;
} SpecStats;

static int find_spill_host_span(const LlamaModel *m, uint64_t dptr, const void **out_hptr, size_t *out_nbytes) {
    if (!m || !m->h_spill_blob || m->spill_wblob_bytes == 0ULL || dptr == 0ULL) return 0;
    if (dptr < m->d_spill_blob || dptr >= m->d_spill_blob + m->spill_wblob_bytes) return 0;
    for (unsigned int i = 0U; i < m->num_tensors; i++) {
        const GgufTensor *t = &m->tensors[i];
        if (t->on_host && t->d_ptr == dptr) {
            *out_hptr = (const uint8_t *)m->h_spill_blob + t->dst_off;
            *out_nbytes = (size_t)t->nbytes;
            return 1;
        }
    }
    return 0;
}

static void *mc_resolve_matvec(void *mod, McPlan *p, uint32_t fmt, uint32_t k_dim) {
    if (fmt == ZCC_GGML_F16 && (k_dim & 15U) == 0U) {
        if (!p->fn_f32_to_f16) check_cu(cuModuleGetFunction(&p->fn_f32_to_f16, mod, "zcc_aot_mc_f32_to_f16"), "get(mc_f32_to_f16)");
        if (!p->fn_gemm_tc) check_cu(cuModuleGetFunction(&p->fn_gemm_tc, mod, "zcc_aot_mc_gemm_tc_f16"), "get(mc_gemm_tc)");
        return p->fn_gemm_tc;
    }
    if (fmt < 32U && p->fn_matvec[fmt]) return p->fn_matvec[fmt];
    char kname[64];
    snprintf(kname, sizeof(kname), "zcc_aot_mc_matvec_%s",
             (fmt == ZCC_GGML_F16) ? "f16" :
             (fmt == ZCC_GGML_BF16) ? "bf16" :
             (fmt == ZCC_GGML_Q8_0) ? "q8_0" :
             (fmt == ZCC_GGML_Q4_0) ? "q4_0" :
             (fmt == ZCC_GGML_Q4_K) ? "q4_k" :
             (fmt == ZCC_GGML_Q6_K) ? "q6_k" : "f32");
    void *fn = NULL;
    check_cu(cuModuleGetFunction(&fn, mod, kname), kname);
    if (fmt < 32U) p->fn_matvec[fmt] = fn;
    return fn;
}

static McNode *mc_add_node(McPlan *p, void *fn, unsigned int gx, unsigned int bx) {
    if (p->num_nodes >= p->cap_nodes) {
        fprintf(stderr, "FATAL: McPlan node capacity exceeded\n");
        exit(1);
    }
    McNode *nd = &p->nodes[p->num_nodes++];
    memset(nd, 0, sizeof(*nd));
    nd->fn = fn;
    nd->gx = gx; nd->gy = 1U; nd->gz = 1U;
    nd->bx = bx; nd->by = 1U; nd->bz = 1U;
    return nd;
}

static void mc_emit_linear(
    McPlan *p, void *mod, const LlamaModel *m, const GpuActivations *act,
    uint64_t wblob, uint64_t woff, uint32_t wtype,
    uint64_t x_ptr, uint64_t xf16_ptr, uint64_t y_ptr,
    unsigned int N, unsigned int K, unsigned int add_flag, int xf16_ready
) {
    const void *h_dma = NULL;
    size_t n_dma = 0U;
    int do_dma = (act && act->spill_stage[0] && find_spill_host_span(m, wblob, &h_dma, &n_dma) && n_dma <= act->spill_stage_bytes) ? 1 : 0;
    if (do_dma) wblob = act->spill_stage[0];
    int use_tc = (wtype == ZCC_GGML_F16 && (K & 15U) == 0U);
    void *fn = mc_resolve_matvec(mod, p, wtype, K);
    if (use_tc && !xf16_ready) {
        McNode *cv = mc_add_node(p, p->fn_f32_to_f16, (K + 255U) / 256U, 256U);
        cv->u64_a[0] = x_ptr; cv->u64_a[1] = xf16_ptr; cv->u32_a[0] = K;
        cv->kparams[0] = &cv->u64_a[0]; cv->kparams[1] = &cv->u64_a[1]; cv->kparams[2] = &cv->u32_a[0];
    }
    unsigned int gx = use_tc ? ((N + 7U) / 8U) : ((N + 3U) / 4U);
    unsigned int bx = use_tc ? 32U : 128U;
    McNode *nd = mc_add_node(p, fn, gx, bx);
    if (do_dma) {
        nd->dma_src_host = h_dma;
        nd->dma_bytes = n_dma;
    }
    nd->u64_a[0] = use_tc ? xf16_ptr : wblob;
    nd->u64_a[1] = use_tc ? wblob : woff;
    nd->u64_a[2] = use_tc ? woff : x_ptr;
    nd->u64_a[3] = y_ptr;
    nd->u32_a[0] = N; nd->u32_a[1] = K; nd->u32_a[2] = add_flag;
    nd->kparams[0] = &nd->u64_a[0]; nd->kparams[1] = &nd->u64_a[1];
    nd->kparams[2] = &nd->u64_a[2]; nd->kparams[3] = &nd->u64_a[3];
    nd->kparams[4] = &nd->u32_a[0]; nd->kparams[5] = &nd->u32_a[1];
    nd->kparams[6] = &nd->u32_a[2];
}

static void mc_emit_bias(McPlan *p, uint64_t vec_ptr, uint64_t wblob, uint64_t boff, unsigned int N) {
    McNode *nd = mc_add_node(p, p->fn_add_bias, (N + 255U) / 256U, 256U);
    nd->u64_a[0] = vec_ptr; nd->u64_a[1] = wblob; nd->u64_a[2] = boff;
    nd->u32_a[0] = N;
    nd->kparams[0] = &nd->u64_a[0]; nd->kparams[1] = &nd->u64_a[1];
    nd->kparams[2] = &nd->u64_a[2]; nd->kparams[3] = &nd->u32_a[0];
}

static void mc_emit_rmsnorm(McPlan *p, uint64_t dst, uint64_t src, uint64_t wblob, uint64_t noff) {
    McNode *nd = mc_add_node(p, p->fn_rmsnorm, 1U, 128U);
    nd->u64_a[0] = dst; nd->u64_a[1] = src; nd->u64_a[2] = wblob; nd->u64_a[3] = noff;
    nd->kparams[0] = &nd->u64_a[0]; nd->kparams[1] = &nd->u64_a[1];
    nd->kparams[2] = &nd->u64_a[2]; nd->kparams[3] = &nd->u64_a[3];
}

static void *mc_resolve_matvec_multi(void *mod, McPlan *mp, McPrefillFns *pf, uint32_t fmt, uint32_t k_dim, unsigned int M) {
    if (fmt == ZCC_GGML_F16 && (k_dim & 15U) == 0U) {
        if (!mp->fn_f32_to_f16) check_cu(cuModuleGetFunction(&mp->fn_f32_to_f16, mod, "zcc_aot_mc_f32_to_f16"), "get(mc_f32_to_f16)");
        if (!pf->fn_prefill_tc_gemm) check_cu(cuModuleGetFunction(&pf->fn_prefill_tc_gemm, mod, "zcc_aot_mc_prefill_tc_gemm"), "get(mc_prefill_tc_gemm)");
        return pf->fn_prefill_tc_gemm;
    }
    const char *suf = (fmt == ZCC_GGML_F16) ? "f16" :
                      (fmt == ZCC_GGML_BF16) ? "bf16" :
                      (fmt == ZCC_GGML_Q8_0) ? "q8_0" :
                      (fmt == ZCC_GGML_Q4_0) ? "q4_0" :
                      (fmt == ZCC_GGML_Q4_K) ? "q4_k" :
                      (fmt == ZCC_GGML_Q6_K) ? "q6_k" : "f32";
    if (M <= 4U && fmt < 32U) {
        if (mp->fn_matvec_multi4[fmt]) return mp->fn_matvec_multi4[fmt];
        char kname4[64];
        snprintf(kname4, sizeof(kname4), "zcc_aot_mc_matvec_multi4_%s", suf);
        void *fn4 = NULL;
        if (cuModuleGetFunction(&fn4, mod, kname4) == 0 && fn4 != NULL) {
            mp->fn_matvec_multi4[fmt] = fn4;
            return fn4;
        }
    }
    if (fmt < 32U && mp->fn_matvec_multi[fmt]) return mp->fn_matvec_multi[fmt];
    char kname[64];
    snprintf(kname, sizeof(kname), "zcc_aot_mc_matvec_multi_%s", suf);
    void *fn = NULL;
    check_cu(cuModuleGetFunction(&fn, mod, kname), kname);
    if (fmt < 32U) mp->fn_matvec_multi[fmt] = fn;
    return fn;
}

static void mc_instantiate_graph(McNode *nodes, unsigned int num_nodes, void **out_exec) {
    void *graph = NULL, *prev = NULL, *cur = NULL;
    check_cu(cuGraphCreate(&graph, 0U), "cuGraphCreate");
    for (unsigned int i = 0U; i < num_nodes; i++) {
        McNode *nd = &nodes[i];
        ZccCudaKernelNodeParams kp;
        memset(&kp, 0, sizeof(kp));
        kp.func = nd->fn;
        kp.gridDimX = nd->gx; kp.gridDimY = nd->gy; kp.gridDimZ = nd->gz;
        kp.blockDimX = nd->bx; kp.blockDimY = nd->by; kp.blockDimZ = nd->bz;
        kp.kernelParams = nd->kparams;
        check_cu(cuGraphAddKernelNode(&cur, graph, prev ? &prev : NULL, prev ? 1U : 0U, &kp), "cuGraphAddKernelNode");
        prev = cur;
    }
    check_cu(cuGraphInstantiateWithFlags(out_exec, graph, 0ULL), "cuGraphInstantiateWithFlags");
    check_cu(cuGraphDestroy(graph), "cuGraphDestroy");
}

static void mc_plan_init(McPlan *p, void *mod, const LlamaModel *m, const GpuActivations *act, int no_cuda_graph, int kv_type, int append_mode) {
    memset(p, 0, sizeof(*p));
    p->use_graph = !no_cuda_graph;
    p->kv_type = kv_type;

    char ename[64];
    snprintf(ename, sizeof(ename), "zcc_aot_mc_embed_%s",
             (m->t_tok_emb == ZCC_GGML_F16) ? "f16" :
             (m->t_tok_emb == ZCC_GGML_BF16) ? "bf16" :
             (m->t_tok_emb == ZCC_GGML_Q8_0) ? "q8_0" :
             (m->t_tok_emb == ZCC_GGML_Q4_0) ? "q4_0" :
             (m->t_tok_emb == ZCC_GGML_Q4_K) ? "q4_k" :
             (m->t_tok_emb == ZCC_GGML_Q6_K) ? "q6_k" : "f32");
    check_cu(cuModuleGetFunction(&p->fn_embed, mod, ename), ename);
    check_cu(cuModuleGetFunction(&p->fn_rmsnorm, mod, "zcc_aot_mc_rmsnorm"), "get(mc_rmsnorm)");
    if (kv_type == ZCC_KV_F16) {
        check_cu(cuModuleGetFunction(&p->fn_rope_kv, mod, "zcc_aot_mc_rope_kv_f16"), "get(mc_rope_kv_f16)");
        check_cu(cuModuleGetFunction(&p->fn_attn_gqa, mod, "zcc_aot_mc_attn_gqa_f16"), "get(mc_attn_gqa_f16)");
    } else if (kv_type == ZCC_KV_Q8_0) {
        check_cu(cuModuleGetFunction(&p->fn_rope_inplace, mod, "zcc_aot_mc_rope_inplace"), "get(mc_rope_inplace)");
        check_cu(cuModuleGetFunction(&p->fn_kv_store_q8_0, mod, "zcc_aot_mc_kv_store_q8_0"), "get(mc_kv_store_q8_0)");
        check_cu(cuModuleGetFunction(&p->fn_attn_gqa, mod, "zcc_aot_mc_attn_gqa_q8_0"), "get(mc_attn_gqa_q8_0)");
    } else {
        check_cu(cuModuleGetFunction(&p->fn_rope_kv, mod, "zcc_aot_mc_rope_kv"), "get(mc_rope_kv)");
        check_cu(cuModuleGetFunction(&p->fn_attn_gqa, mod, "zcc_aot_mc_attn_gqa"), "get(mc_attn_gqa)");
    }
    check_cu(cuModuleGetFunction(&p->fn_swiglu, mod, "zcc_aot_mc_swiglu"), "get(mc_swiglu)");
    if (m->has_any_bias) check_cu(cuModuleGetFunction(&p->fn_add_bias, mod, "zcc_aot_mc_add_bias"), "get(mc_add_bias)");

    p->cap_nodes = m->n_layers * 24U + 24U;
    p->nodes = (McNode *)calloc((size_t)p->cap_nodes, sizeof(McNode));
    if (!p->nodes) { fprintf(stderr, "FATAL: OOM for McPlan\n"); exit(1); }

    unsigned int dim = m->dim, hid = m->hidden_dim, kv_dim = m->kv_dim;
    unsigned int total_pairs = (m->n_heads + m->n_kv_heads) * (m->head_dim >> 1);
    unsigned int grd_rope = (total_pairs + 127U) / 128U;

    {
        McNode *em = mc_add_node(p, p->fn_embed, (dim + 255U) / 256U, 256U);
        em->u64_a[0] = m->d_tok_emb; em->u64_a[1] = 0ULL; em->u64_a[2] = act->x; em->u64_a[3] = act->step_args;
        em->kparams[0] = &em->u64_a[0]; em->kparams[1] = &em->u64_a[1];
        em->kparams[2] = &em->u64_a[2]; em->kparams[3] = &em->u64_a[3];
    }

    for (unsigned int l = 0U; l < m->n_layers; l++) {
        const LayerWeights *lw = &m->layers[l];
        mc_emit_rmsnorm(p, act->xb, act->x, lw->d_attn_norm, 0ULL);

        int qkv_tc_ready = 0;
        if ((lw->t_wq == ZCC_GGML_F16 || lw->t_wk == ZCC_GGML_F16 || lw->t_wv == ZCC_GGML_F16) && (dim & 15U) == 0U) {
            mc_resolve_matvec(mod, p, ZCC_GGML_F16, dim);
            McNode *cv = mc_add_node(p, p->fn_f32_to_f16, (dim + 255U) / 256U, 256U);
            cv->u64_a[0] = act->xb; cv->u64_a[1] = act->x_f16; cv->u32_a[0] = dim;
            cv->kparams[0] = &cv->u64_a[0]; cv->kparams[1] = &cv->u64_a[1]; cv->kparams[2] = &cv->u32_a[0];
            qkv_tc_ready = 1;
        }
        mc_emit_linear(p, mod, m, act, lw->d_wq, 0ULL, lw->t_wq, act->xb, act->x_f16, act->q, dim, dim, 0U, qkv_tc_ready);
        mc_emit_linear(p, mod, m, act, lw->d_wk, 0ULL, lw->t_wk, act->xb, act->x_f16, act->k, kv_dim, dim, 0U, qkv_tc_ready);
        mc_emit_linear(p, mod, m, act, lw->d_wv, 0ULL, lw->t_wv, act->xb, act->x_f16, act->v, kv_dim, dim, 0U, qkv_tc_ready);
        if (lw->has_bq) mc_emit_bias(p, act->q, lw->d_bq, 0ULL, dim);
        if (lw->has_bk) mc_emit_bias(p, act->k, lw->d_bk, 0ULL, kv_dim);
        if (lw->has_bv) mc_emit_bias(p, act->v, lw->d_bv, 0ULL, kv_dim);

        if (kv_type == ZCC_KV_Q8_0) {
            McNode *ri = mc_add_node(p, p->fn_rope_inplace, grd_rope, 128U);
            ri->u64_a[0] = act->q; ri->u64_a[1] = act->k; ri->u64_a[2] = act->step_args;
            ri->kparams[0] = &ri->u64_a[0]; ri->kparams[1] = &ri->u64_a[1]; ri->kparams[2] = &ri->u64_a[2];

            McNode *ks = mc_add_node(p, p->fn_kv_store_q8_0, (kv_dim >> 5) * 2U, 32U);
            ks->u64_a[0] = act->k; ks->u64_a[1] = act->v;
            ks->u64_a[2] = act->k_cache; ks->u64_a[3] = act->v_cache; ks->u64_a[4] = act->step_args;
            ks->u32_a[0] = l;
            for (int k = 0; k < 5; k++) ks->kparams[k] = &ks->u64_a[k];
            ks->kparams[5] = &ks->u32_a[0];
        } else {
            McNode *rk = mc_add_node(p, p->fn_rope_kv, grd_rope, 128U);
            rk->u64_a[0] = act->q; rk->u64_a[1] = act->k; rk->u64_a[2] = act->v;
            rk->u64_a[3] = act->k_cache; rk->u64_a[4] = act->v_cache; rk->u64_a[5] = act->step_args;
            rk->u32_a[0] = l;
            for (int k = 0; k < 6; k++) rk->kparams[k] = &rk->u64_a[k];
            rk->kparams[6] = &rk->u32_a[0];
        }
        {
            McNode *at = mc_add_node(p, p->fn_attn_gqa, m->n_heads, 32U);
            at->u64_a[0] = act->q; at->u64_a[1] = act->k_cache; at->u64_a[2] = act->v_cache;
            at->u64_a[3] = act->xb; at->u64_a[4] = act->step_args; at->u32_a[0] = l;
            for (int k = 0; k < 5; k++) at->kparams[k] = &at->u64_a[k];
            at->kparams[5] = &at->u32_a[0];
        }

        mc_emit_linear(p, mod, m, act, lw->d_wo, 0ULL, lw->t_wo, act->xb, act->x_f16, act->x, dim, dim, 1U, 0);
        mc_emit_rmsnorm(p, act->xb, act->x, lw->d_ffn_norm, 0ULL);

        int ffn_tc_ready = 0;
        if ((lw->t_wgate == ZCC_GGML_F16 || lw->t_wup == ZCC_GGML_F16) && (dim & 15U) == 0U) {
            mc_resolve_matvec(mod, p, ZCC_GGML_F16, dim);
            McNode *cv = mc_add_node(p, p->fn_f32_to_f16, (dim + 255U) / 256U, 256U);
            cv->u64_a[0] = act->xb; cv->u64_a[1] = act->x_f16; cv->u32_a[0] = dim;
            cv->kparams[0] = &cv->u64_a[0]; cv->kparams[1] = &cv->u64_a[1]; cv->kparams[2] = &cv->u32_a[0];
            ffn_tc_ready = 1;
        }
        mc_emit_linear(p, mod, m, act, lw->d_wgate, 0ULL, lw->t_wgate, act->xb, act->x_f16, act->hb, hid, dim, 0U, ffn_tc_ready);
        mc_emit_linear(p, mod, m, act, lw->d_wup, 0ULL, lw->t_wup, act->xb, act->x_f16, act->hb2, hid, dim, 0U, ffn_tc_ready);

        {
            McNode *sw = mc_add_node(p, p->fn_swiglu, (hid + 255U) / 256U, 256U);
            sw->u64_a[0] = act->hb; sw->u64_a[1] = act->hb2;
            sw->kparams[0] = &sw->u64_a[0]; sw->kparams[1] = &sw->u64_a[1];
        }
        mc_emit_linear(p, mod, m, act, lw->d_wdown, 0ULL, lw->t_wdown, act->hb, act->x_f16, act->x, dim, hid, 1U, 0);
    }

    mc_emit_rmsnorm(p, act->xb, act->x, m->d_out_norm, 0ULL);
    mc_emit_linear(p, mod, m, act, m->d_wout, 0ULL, m->t_wout, act->xb, act->x_f16, act->logits, m->vocab_size, dim, 0U, 0);

    if (append_mode == 1 && act->argmax_ids && act->argmax_vals) {
        if (cuModuleGetFunction(&p->fn_argmax, mod, "zcc_aot_mc_argmax_f32") == 0 && p->fn_argmax) {
            p->has_argmax = 1;
            McNode *am = mc_add_node(p, p->fn_argmax, 1U, 256U);
            am->u64_a[0] = act->logits; am->u64_a[1] = act->argmax_ids; am->u64_a[2] = act->argmax_vals;
            am->u32_a[0] = m->vocab_size;
            am->kparams[0] = &am->u64_a[0]; am->kparams[1] = &am->u64_a[1];
            am->kparams[2] = &am->u64_a[2]; am->kparams[3] = &am->u32_a[0];
        }
    } else if (append_mode == 2) {
        check_cu(cuModuleGetFunction(&p->fn_draft_chain, mod, "zcc_aot_mc_draft_chain_step"), "get(mc_draft_chain_step)");
        p->has_draft_chain = 1;
        McNode *dc = mc_add_node(p, p->fn_draft_chain, 1U, 256U);
        dc->u64_a[0] = act->logits; dc->u64_a[1] = act->step_args; dc->u64_a[2] = act->draft_ids;
        dc->u32_a[0] = m->vocab_size;
        dc->kparams[0] = &dc->u64_a[0]; dc->kparams[1] = &dc->u64_a[1];
        dc->kparams[2] = &dc->u64_a[2]; dc->kparams[3] = &dc->u32_a[0];
    }

    p->spill_stage = act->spill_stage[0];
    unsigned int first_dma = p->num_nodes;
    for (unsigned int i = 0U; i < p->num_nodes; i++) {
        if (p->nodes[i].dma_src_host != NULL) { first_dma = i; break; }
    }
    p->graph_nodes = first_dma;
    if (p->use_graph && p->graph_nodes > 0U) {
        mc_instantiate_graph(p->nodes, p->graph_nodes, &p->graph_exec);
    }
}

static void mc_plan_launch(const McPlan *p) {
    unsigned int start_i = 0U;
    if (p->use_graph && p->graph_exec && p->graph_nodes > 0U) {
        check_cu(cuGraphLaunch(p->graph_exec, NULL), "cuGraphLaunch");
        start_i = p->graph_nodes;
    }
    for (unsigned int i = start_i; i < p->num_nodes; i++) {
        const McNode *nd = &p->nodes[i];
        if (nd->dma_src_host != NULL) {
            check_cu(cuMemcpyHtoD_v2(p->spill_stage, nd->dma_src_host, nd->dma_bytes), "HtoD(mc_spill)");
        }
        check_cu(cuLaunchKernel(nd->fn, nd->gx, nd->gy, nd->gz, nd->bx, nd->by, nd->bz, 0U, NULL, nd->kparams, NULL), "mc_direct");
    }
}

static void mc_plan_step(McPlan *p, uint64_t d_step_args, unsigned int tok, unsigned int pos, unsigned int eff_max_seq) {
    uint32_t sa[4];
    sa[0] = tok; sa[1] = pos; sa[2] = eff_max_seq; sa[3] = 0U;
    check_cu(cuMemcpyHtoD_v2(d_step_args, sa, 16U), "HtoD(step_args)");
    mc_plan_launch(p);
}

static void mc_plan_free(McPlan *p) {
    if (p->graph_exec) { cuGraphExecDestroy(p->graph_exec); p->graph_exec = NULL; }
    if (p->nodes) { free(p->nodes); p->nodes = NULL; }
}

static void mc_init_prefill_fns(void *mod, McPlan *mp, McPrefillFns *pf, const LlamaModel *m, int kv_type) {
    if (pf->ready) return;
    if (!mp->fn_f32_to_f16) check_cu(cuModuleGetFunction(&mp->fn_f32_to_f16, mod, "zcc_aot_mc_f32_to_f16"), "get(mc_f32_to_f16)");
    check_cu(cuModuleGetFunction(&pf->fn_rmsnorm_batch, mod, "zcc_aot_mc_rmsnorm_batch"), "get(mc_rmsnorm_batch)");
    check_cu(cuModuleGetFunction(&pf->fn_prefill_tc_gemm, mod, "zcc_aot_mc_prefill_tc_gemm"), "get(mc_prefill_tc_gemm)");
    check_cu(cuModuleGetFunction(&pf->fn_swiglu_batch, mod, "zcc_aot_mc_swiglu_batch"), "get(mc_swiglu_batch)");
    if (m->has_any_bias) check_cu(cuModuleGetFunction(&pf->fn_add_bias_batch, mod, "zcc_aot_mc_add_bias_batch"), "get(mc_add_bias_batch)");
    if (kv_type == ZCC_KV_F16) {
        check_cu(cuModuleGetFunction(&pf->fn_rope_kv_batch, mod, "zcc_aot_mc_rope_kv_batch_f16"), "get(mc_rope_kv_batch_f16)");
        check_cu(cuModuleGetFunction(&pf->fn_attn_batch, mod, "zcc_aot_mc_attn_gqa_batch_f16"), "get(mc_attn_gqa_batch_f16)");
    } else if (kv_type == ZCC_KV_Q8_0) {
        check_cu(cuModuleGetFunction(&pf->fn_rope_inplace_batch, mod, "zcc_aot_mc_rope_inplace_batch"), "get(mc_rope_inplace_batch)");
        check_cu(cuModuleGetFunction(&pf->fn_kv_store_batch_q8_0, mod, "zcc_aot_mc_kv_store_batch_q8_0"), "get(mc_kv_store_batch_q8_0)");
        check_cu(cuModuleGetFunction(&pf->fn_attn_batch, mod, "zcc_aot_mc_attn_gqa_batch_q8_0"), "get(mc_attn_gqa_batch_q8_0)");
    } else {
        check_cu(cuModuleGetFunction(&pf->fn_rope_kv_batch, mod, "zcc_aot_mc_rope_kv_batch"), "get(mc_rope_kv_batch)");
        check_cu(cuModuleGetFunction(&pf->fn_attn_batch, mod, "zcc_aot_mc_attn_gqa_batch"), "get(mc_attn_gqa_batch)");
    }
    pf->ready = 1;
}

static McNode *mc_v_add(McVerifyPlan *vp, void *fn, unsigned int gx, unsigned int gy, unsigned int bx) {
    if (vp->num_nodes >= vp->cap_nodes) { fprintf(stderr, "FATAL: McVerifyPlan node capacity exceeded\n"); exit(1); }
    McNode *nd = &vp->nodes[vp->num_nodes++];
    memset(nd, 0, sizeof(*nd));
    nd->fn = fn;
    nd->gx = gx; nd->gy = gy; nd->gz = 1U;
    nd->bx = bx; nd->by = 1U; nd->bz = 1U;
    return nd;
}

static void mc_v_emit_linear(
    McVerifyPlan *vp, void *mod, McPlan *mp, McPrefillFns *pf,
    const LlamaModel *m, const GpuActivations *act,
    uint64_t wblob, uint64_t woff, uint32_t wtype,
    uint64_t x_f32, uint64_t x_f16, uint64_t y_ptr,
    unsigned int M, unsigned int N, unsigned int K, unsigned int add_flag, int xf16_ready
) {
    const void *h_dma = NULL;
    size_t n_dma = 0U;
    int do_dma = (act && act->spill_stage[0] && find_spill_host_span(m, wblob, &h_dma, &n_dma) && n_dma <= act->spill_stage_bytes) ? 1 : 0;
    int slot = 0;
    if (do_dma) {
        slot = (act->spill_slots >= 2) ? (int)(vp->num_dma & 1U) : 0;
        wblob = act->spill_stage[slot];
        vp->num_dma++;
    }
    int use_tc = (wtype == ZCC_GGML_F16 && (K & 15U) == 0U);
    void *fn = mc_resolve_matvec_multi(mod, mp, pf, wtype, K, M);
    if (use_tc) {
        if (!xf16_ready) {
            unsigned int mk = M * K;
            McNode *cv = mc_v_add(vp, mp->fn_f32_to_f16, (mk + 255U) / 256U, 1U, 256U);
            cv->u64_a[0] = x_f32; cv->u64_a[1] = x_f16; cv->u32_a[0] = mk;
            cv->kparams[0] = &cv->u64_a[0]; cv->kparams[1] = &cv->u64_a[1]; cv->kparams[2] = &cv->u32_a[0];
        }
        McNode *nd = mc_v_add(vp, fn, (N + 31U) / 32U, (M + 15U) / 16U, 128U);
        if (do_dma) { nd->dma_src_host = h_dma; nd->dma_bytes = n_dma; nd->dma_slot = slot; }
        nd->u64_a[0] = x_f16; nd->u64_a[1] = wblob; nd->u64_a[2] = woff; nd->u64_a[3] = y_ptr;
        nd->u32_a[0] = M; nd->u32_a[1] = N; nd->u32_a[2] = K; nd->u32_a[3] = add_flag;
        nd->kparams[0] = &nd->u64_a[0]; nd->kparams[1] = &nd->u64_a[1];
        nd->kparams[2] = &nd->u64_a[2]; nd->kparams[3] = &nd->u64_a[3];
        nd->kparams[4] = &nd->u32_a[0]; nd->kparams[5] = &nd->u32_a[1];
        nd->kparams[6] = &nd->u32_a[2]; nd->kparams[7] = &nd->u32_a[3];
    } else {
        McNode *nd = mc_v_add(vp, fn, (N + 3U) / 4U, 1U, 128U);
        if (do_dma) { nd->dma_src_host = h_dma; nd->dma_bytes = n_dma; nd->dma_slot = slot; }
        nd->u64_a[0] = wblob; nd->u64_a[1] = woff; nd->u64_a[2] = x_f32; nd->u64_a[3] = y_ptr;
        nd->u32_a[0] = N; nd->u32_a[1] = K; nd->u32_a[2] = M; nd->u32_a[3] = add_flag;
        nd->kparams[0] = &nd->u64_a[0]; nd->kparams[1] = &nd->u64_a[1];
        nd->kparams[2] = &nd->u64_a[2]; nd->kparams[3] = &nd->u64_a[3];
        nd->kparams[4] = &nd->u32_a[0]; nd->kparams[5] = &nd->u32_a[1];
        nd->kparams[6] = &nd->u32_a[2]; nd->kparams[7] = &nd->u32_a[3];
    }
}

static void mc_verify_plan_init(
    McVerifyPlan *vp, void *mod, McPlan *mp, McPrefillFns *pf,
    const LlamaModel *m, const GpuActivations *act, unsigned int M, int no_cuda_graph, int kv_type
) {
    memset(vp, 0, sizeof(*vp));
    mc_init_prefill_fns(mod, mp, pf, m, kv_type);
    if (!mp->fn_argmax) check_cu(cuModuleGetFunction(&mp->fn_argmax, mod, "zcc_aot_mc_argmax_f32"), "get(mc_argmax)");
    vp->M = M;
    vp->use_graph = !no_cuda_graph;
    vp->cap_nodes = m->n_layers * 24U + M + 24U;
    vp->nodes = (McNode *)calloc((size_t)vp->cap_nodes, sizeof(McNode));
    if (!vp->nodes) { fprintf(stderr, "FATAL: OOM for McVerifyPlan\n"); exit(1); }

    unsigned int dim = m->dim, hid = m->hidden_dim, kv_dim = m->kv_dim;
    unsigned int total_pairs = (m->n_heads + m->n_kv_heads) * (m->head_dim >> 1);
    unsigned int pos0_dyn = 0xFFFFFFFFU;

    for (unsigned int ti = 0U; ti < M; ti++) {
        McNode *em = mc_v_add(vp, mp->fn_embed, (dim + 255U) / 256U, 1U, 256U);
        em->u64_a[0] = m->d_tok_emb; em->u64_a[1] = 0ULL;
        em->u64_a[2] = act->x + (uint64_t)ti * (uint64_t)dim * 4ULL;
        em->u64_a[3] = act->batch_toks + (uint64_t)ti * 4ULL;
        em->kparams[0] = &em->u64_a[0]; em->kparams[1] = &em->u64_a[1];
        em->kparams[2] = &em->u64_a[2]; em->kparams[3] = &em->u64_a[3];
    }

    for (unsigned int l = 0U; l < m->n_layers; l++) {
        const LayerWeights *lw = &m->layers[l];
        {
            McNode *rn = mc_v_add(vp, pf->fn_rmsnorm_batch, M, 1U, 128U);
            rn->u64_a[0] = act->xb; rn->u64_a[1] = act->x; rn->u64_a[2] = lw->d_attn_norm; rn->u64_a[3] = 0ULL;
            rn->kparams[0] = &rn->u64_a[0]; rn->kparams[1] = &rn->u64_a[1];
            rn->kparams[2] = &rn->u64_a[2]; rn->kparams[3] = &rn->u64_a[3];
        }
        int qkv_tc_ready = 0;
        if ((lw->t_wq == ZCC_GGML_F16 || lw->t_wk == ZCC_GGML_F16 || lw->t_wv == ZCC_GGML_F16) && (dim & 15U) == 0U) {
            unsigned int m_dim = M * dim;
            mc_resolve_matvec_multi(mod, mp, pf, ZCC_GGML_F16, dim, M);
            McNode *cv = mc_v_add(vp, mp->fn_f32_to_f16, (m_dim + 255U) / 256U, 1U, 256U);
            cv->u64_a[0] = act->xb; cv->u64_a[1] = act->x_f16; cv->u32_a[0] = m_dim;
            cv->kparams[0] = &cv->u64_a[0]; cv->kparams[1] = &cv->u64_a[1]; cv->kparams[2] = &cv->u32_a[0];
            qkv_tc_ready = 1;
        }
        mc_v_emit_linear(vp, mod, mp, pf, m, act, lw->d_wq, 0ULL, lw->t_wq, act->xb, act->x_f16, act->q, M, dim, dim, 0U, qkv_tc_ready);
        mc_v_emit_linear(vp, mod, mp, pf, m, act, lw->d_wk, 0ULL, lw->t_wk, act->xb, act->x_f16, act->k, M, kv_dim, dim, 0U, qkv_tc_ready);
        mc_v_emit_linear(vp, mod, mp, pf, m, act, lw->d_wv, 0ULL, lw->t_wv, act->xb, act->x_f16, act->v, M, kv_dim, dim, 0U, qkv_tc_ready);

        if (lw->has_bq) {
            McNode *ab = mc_v_add(vp, pf->fn_add_bias_batch, (dim + 255U) / 256U, M, 256U);
            ab->u64_a[0] = act->q; ab->u64_a[1] = lw->d_bq; ab->u64_a[2] = 0ULL; ab->u32_a[0] = dim;
            ab->kparams[0] = &ab->u64_a[0]; ab->kparams[1] = &ab->u64_a[1]; ab->kparams[2] = &ab->u64_a[2]; ab->kparams[3] = &ab->u32_a[0];
        }
        if (lw->has_bk) {
            McNode *ab = mc_v_add(vp, pf->fn_add_bias_batch, (kv_dim + 255U) / 256U, M, 256U);
            ab->u64_a[0] = act->k; ab->u64_a[1] = lw->d_bk; ab->u64_a[2] = 0ULL; ab->u32_a[0] = kv_dim;
            ab->kparams[0] = &ab->u64_a[0]; ab->kparams[1] = &ab->u64_a[1]; ab->kparams[2] = &ab->u64_a[2]; ab->kparams[3] = &ab->u32_a[0];
        }
        if (lw->has_bv) {
            McNode *ab = mc_v_add(vp, pf->fn_add_bias_batch, (kv_dim + 255U) / 256U, M, 256U);
            ab->u64_a[0] = act->v; ab->u64_a[1] = lw->d_bv; ab->u64_a[2] = 0ULL; ab->u32_a[0] = kv_dim;
            ab->kparams[0] = &ab->u64_a[0]; ab->kparams[1] = &ab->u64_a[1]; ab->kparams[2] = &ab->u64_a[2]; ab->kparams[3] = &ab->u32_a[0];
        }

        if (kv_type == ZCC_KV_Q8_0) {
            McNode *ri = mc_v_add(vp, pf->fn_rope_inplace_batch, (total_pairs + 127U) / 128U, M, 128U);
            ri->u64_a[0] = act->q; ri->u64_a[1] = act->k; ri->u64_a[2] = act->step_args;
            ri->u32_a[0] = pos0_dyn; ri->u32_a[1] = M;
            ri->kparams[0] = &ri->u64_a[0]; ri->kparams[1] = &ri->u64_a[1]; ri->kparams[2] = &ri->u64_a[2];
            ri->kparams[3] = &ri->u32_a[0]; ri->kparams[4] = &ri->u32_a[1];

            McNode *ks = mc_v_add(vp, pf->fn_kv_store_batch_q8_0, (kv_dim >> 5) * 2U, M, 32U);
            ks->u64_a[0] = act->k; ks->u64_a[1] = act->v;
            ks->u64_a[2] = act->k_cache; ks->u64_a[3] = act->v_cache; ks->u64_a[4] = act->step_args;
            ks->u32_a[0] = l; ks->u32_a[1] = pos0_dyn; ks->u32_a[2] = M;
            for (int k = 0; k < 5; k++) ks->kparams[k] = &ks->u64_a[k];
            for (int k = 0; k < 3; k++) ks->kparams[5 + k] = &ks->u32_a[k];
        } else {
            unsigned int mx = (total_pairs > kv_dim) ? total_pairs : kv_dim;
            McNode *rk = mc_v_add(vp, pf->fn_rope_kv_batch, (mx + 127U) / 128U, M, 128U);
            rk->u64_a[0] = act->q; rk->u64_a[1] = act->k; rk->u64_a[2] = act->v;
            rk->u64_a[3] = act->k_cache; rk->u64_a[4] = act->v_cache; rk->u64_a[5] = act->step_args;
            rk->u32_a[0] = l; rk->u32_a[1] = pos0_dyn; rk->u32_a[2] = M;
            for (int k = 0; k < 6; k++) rk->kparams[k] = &rk->u64_a[k];
            for (int k = 0; k < 3; k++) rk->kparams[6 + k] = &rk->u32_a[k];
        }
        {
            McNode *at = mc_v_add(vp, pf->fn_attn_batch, m->n_heads, M, 32U);
            at->u64_a[0] = act->q; at->u64_a[1] = act->k_cache; at->u64_a[2] = act->v_cache;
            at->u64_a[3] = act->xb; at->u64_a[4] = act->step_args;
            at->u32_a[0] = l; at->u32_a[1] = pos0_dyn; at->u32_a[2] = M;
            for (int k = 0; k < 5; k++) at->kparams[k] = &at->u64_a[k];
            for (int k = 0; k < 3; k++) at->kparams[5 + k] = &at->u32_a[k];
        }

        mc_v_emit_linear(vp, mod, mp, pf, m, act, lw->d_wo, 0ULL, lw->t_wo, act->xb, act->x_f16, act->x, M, dim, dim, 1U, 0);
        {
            McNode *rn = mc_v_add(vp, pf->fn_rmsnorm_batch, M, 1U, 128U);
            rn->u64_a[0] = act->xb; rn->u64_a[1] = act->x; rn->u64_a[2] = lw->d_ffn_norm; rn->u64_a[3] = 0ULL;
            rn->kparams[0] = &rn->u64_a[0]; rn->kparams[1] = &rn->u64_a[1];
            rn->kparams[2] = &rn->u64_a[2]; rn->kparams[3] = &rn->u64_a[3];
        }
        int ffn_tc_ready = 0;
        if ((lw->t_wgate == ZCC_GGML_F16 || lw->t_wup == ZCC_GGML_F16) && (dim & 15U) == 0U) {
            unsigned int m_dim = M * dim;
            mc_resolve_matvec_multi(mod, mp, pf, ZCC_GGML_F16, dim, M);
            McNode *cv = mc_v_add(vp, mp->fn_f32_to_f16, (m_dim + 255U) / 256U, 1U, 256U);
            cv->u64_a[0] = act->xb; cv->u64_a[1] = act->x_f16; cv->u32_a[0] = m_dim;
            cv->kparams[0] = &cv->u64_a[0]; cv->kparams[1] = &cv->u64_a[1]; cv->kparams[2] = &cv->u32_a[0];
            ffn_tc_ready = 1;
        }
        mc_v_emit_linear(vp, mod, mp, pf, m, act, lw->d_wgate, 0ULL, lw->t_wgate, act->xb, act->x_f16, act->hb, M, hid, dim, 0U, ffn_tc_ready);
        mc_v_emit_linear(vp, mod, mp, pf, m, act, lw->d_wup, 0ULL, lw->t_wup, act->xb, act->x_f16, act->hb2, M, hid, dim, 0U, ffn_tc_ready);
        {
            unsigned int m_hid = M * hid;
            McNode *sw = mc_v_add(vp, pf->fn_swiglu_batch, (m_hid + 255U) / 256U, 1U, 256U);
            sw->u64_a[0] = act->hb; sw->u64_a[1] = act->hb2; sw->u32_a[0] = m_hid;
            sw->kparams[0] = &sw->u64_a[0]; sw->kparams[1] = &sw->u64_a[1]; sw->kparams[2] = &sw->u32_a[0];
        }
        mc_v_emit_linear(vp, mod, mp, pf, m, act, lw->d_wdown, 0ULL, lw->t_wdown, act->hb, act->x_f16, act->x, M, dim, hid, 1U, 0);
    }

    {
        McNode *rn = mc_v_add(vp, pf->fn_rmsnorm_batch, M, 1U, 128U);
        rn->u64_a[0] = act->xb; rn->u64_a[1] = act->x; rn->u64_a[2] = m->d_out_norm; rn->u64_a[3] = 0ULL;
        rn->kparams[0] = &rn->u64_a[0]; rn->kparams[1] = &rn->u64_a[1];
        rn->kparams[2] = &rn->u64_a[2]; rn->kparams[3] = &rn->u64_a[3];
    }
    mc_v_emit_linear(vp, mod, mp, pf, m, act, m->d_wout, 0ULL, m->t_wout, act->xb, act->x_f16, act->logits, M, m->vocab_size, dim, 0U, 0);
    {
        McNode *am = mc_v_add(vp, mp->fn_argmax, M, 1U, 256U);
        am->u64_a[0] = act->logits; am->u64_a[1] = act->argmax_ids; am->u64_a[2] = act->argmax_vals;
        am->u32_a[0] = m->vocab_size;
        am->kparams[0] = &am->u64_a[0]; am->kparams[1] = &am->u64_a[1];
        am->kparams[2] = &am->u64_a[2]; am->kparams[3] = &am->u32_a[0];
    }

    vp->spill_stage[0] = act->spill_stage[0];
    vp->spill_stage[1] = act->spill_stage[1];
    vp->spill_slots = act->spill_slots;
    vp->dma_stream = act->dma_stream;
    vp->dma_ev[0] = act->dma_ev[0];
    vp->dma_ev[1] = act->dma_ev[1];
    vp->comp_ev[0] = act->comp_ev[0];
    vp->comp_ev[1] = act->comp_ev[1];
    if (vp->num_dma > 0U) {
        vp->dma_order = (unsigned int *)malloc((size_t)vp->num_dma * sizeof(unsigned int));
        if (!vp->dma_order) { fprintf(stderr, "FATAL: OOM for dma_order\n"); exit(1); }
        unsigned int di = 0U;
        for (unsigned int i = 0U; i < vp->num_nodes; i++) {
            if (vp->nodes[i].dma_src_host != NULL) vp->dma_order[di++] = i;
        }
        vp->num_dma = di;
    }
    vp->segs = (McVerifySeg *)calloc((size_t)(vp->num_dma * 2U + 2U), sizeof(McVerifySeg));
    if (!vp->segs) { fprintf(stderr, "FATAL: OOM for McVerifySeg\n"); exit(1); }
    vp->num_segs = 0U;
    unsigned int si = 0U;
    while (si < vp->num_nodes) {
        if (vp->nodes[si].dma_src_host != NULL) {
            McVerifySeg *sg = &vp->segs[vp->num_segs++];
            sg->start = si; sg->count = 1U; sg->is_dma = 1; sg->graph_exec = NULL;
            si++;
        } else {
            unsigned int s0 = si;
            while (si < vp->num_nodes && vp->nodes[si].dma_src_host == NULL) si++;
            McVerifySeg *sg = &vp->segs[vp->num_segs++];
            sg->start = s0; sg->count = si - s0; sg->is_dma = 0; sg->graph_exec = NULL;
            if (vp->use_graph && sg->count > 0U) {
                mc_instantiate_graph(&vp->nodes[s0], sg->count, &sg->graph_exec);
            }
        }
    }
    vp->ready = 1;
}

static void mc_verify_plan_step(
    McVerifyPlan *vp, const GpuActivations *act,
    const uint32_t *toks, unsigned int M, unsigned int pos0, unsigned int eff_max_seq
) {
    check_cu(cuMemcpyHtoD_v2(act->batch_toks, toks, (size_t)M * 4U), "HtoD(v_toks)");
    uint32_t sa[4];
    sa[0] = 0U; sa[1] = pos0; sa[2] = eff_max_seq; sa[3] = M;
    check_cu(cuMemcpyHtoD_v2(act->step_args, sa, 16U), "HtoD(v_sa)");
    if (vp->num_dma > 0U && vp->spill_slots >= 2 && vp->dma_stream != NULL) {
        unsigned int pre = (vp->num_dma < 2U) ? vp->num_dma : 2U;
        for (unsigned int d = 0U; d < pre; d++) {
            const McNode *dn = &vp->nodes[vp->dma_order[d]];
            int sl = dn->dma_slot;
            check_cu(cuEventRecord(vp->comp_ev[sl], NULL), "rec(pre_comp)");
            check_cu(cuStreamWaitEvent(vp->dma_stream, vp->comp_ev[sl], 0U), "dma_wait(pre_comp)");
            check_cu(cuMemcpyHtoDAsync_v2(vp->spill_stage[sl], dn->dma_src_host, dn->dma_bytes, vp->dma_stream), "HtoDAsync(pre_spill)");
            check_cu(cuEventRecord(vp->dma_ev[sl], vp->dma_stream), "rec(pre_dma)");
        }
        unsigned int dma_k = 0U;
        for (unsigned int s = 0U; s < vp->num_segs; s++) {
            const McVerifySeg *sg = &vp->segs[s];
            if (!sg->is_dma) {
                if (vp->use_graph && sg->graph_exec) {
                    check_cu(cuGraphLaunch(sg->graph_exec, NULL), "cuGraphLaunch(v)");
                } else {
                    for (unsigned int i = sg->start; i < sg->start + sg->count; i++) {
                        McNode *nd = &vp->nodes[i];
                        check_cu(cuLaunchKernel(nd->fn, nd->gx, nd->gy, nd->gz, nd->bx, nd->by, nd->bz, 0U, NULL, nd->kparams, NULL), "v_direct");
                    }
                }
            } else {
                McNode *nd = &vp->nodes[sg->start];
                int sl = nd->dma_slot;
                check_cu(cuStreamWaitEvent(NULL, vp->dma_ev[sl], 0U), "wait(dma_ev)");
                check_cu(cuLaunchKernel(nd->fn, nd->gx, nd->gy, nd->gz, nd->bx, nd->by, nd->bz, 0U, NULL, nd->kparams, NULL), "v_direct");
                check_cu(cuEventRecord(vp->comp_ev[sl], NULL), "rec(comp_ev)");
                if (dma_k + 2U < vp->num_dma) {
                    const McNode *nxt = &vp->nodes[vp->dma_order[dma_k + 2U]];
                    check_cu(cuStreamWaitEvent(vp->dma_stream, vp->comp_ev[sl], 0U), "dma_wait(comp_ev)");
                    check_cu(cuMemcpyHtoDAsync_v2(vp->spill_stage[sl], nxt->dma_src_host, nxt->dma_bytes, vp->dma_stream), "HtoDAsync(v_spill)");
                    check_cu(cuEventRecord(vp->dma_ev[sl], vp->dma_stream), "rec(dma_ev)");
                }
                dma_k++;
            }
        }
    } else {
        for (unsigned int s = 0U; s < vp->num_segs; s++) {
            const McVerifySeg *sg = &vp->segs[s];
            if (!sg->is_dma) {
                if (vp->use_graph && sg->graph_exec) {
                    check_cu(cuGraphLaunch(sg->graph_exec, NULL), "cuGraphLaunch(v)");
                } else {
                    for (unsigned int i = sg->start; i < sg->start + sg->count; i++) {
                        McNode *nd = &vp->nodes[i];
                        check_cu(cuLaunchKernel(nd->fn, nd->gx, nd->gy, nd->gz, nd->bx, nd->by, nd->bz, 0U, NULL, nd->kparams, NULL), "v_direct");
                    }
                }
            } else {
                McNode *nd = &vp->nodes[sg->start];
                check_cu(cuMemcpyHtoD_v2(vp->spill_stage[0], nd->dma_src_host, nd->dma_bytes), "HtoD(v_spill)");
                check_cu(cuLaunchKernel(nd->fn, nd->gx, nd->gy, nd->gz, nd->bx, nd->by, nd->bz, 0U, NULL, nd->kparams, NULL), "v_direct");
            }
        }
    }
}

static void mc_verify_plan_free(McVerifyPlan *vp) {
    if (vp->segs) {
        for (unsigned int s = 0U; s < vp->num_segs; s++) {
            if (vp->segs[s].graph_exec) {
                cuGraphExecDestroy(vp->segs[s].graph_exec);
                vp->segs[s].graph_exec = NULL;
            }
        }
        free(vp->segs);
        vp->segs = NULL;
    }
    vp->num_segs = 0U;
    if (vp->nodes) { free(vp->nodes); vp->nodes = NULL; }
    if (vp->dma_order) { free(vp->dma_order); vp->dma_order = NULL; }
    vp->num_dma = 0U;
    vp->ready = 0;
}

static void *mc_resolve_dequant(void *mod, McPrefillFns *pf, uint32_t fmt) {
    if (fmt < 32U && pf->fn_dequant[fmt]) return pf->fn_dequant[fmt];
    char kname[64];
    snprintf(kname, sizeof(kname), "zcc_aot_mc_dequant_%s_to_f16",
             (fmt == ZCC_GGML_BF16) ? "bf16" :
             (fmt == ZCC_GGML_Q8_0) ? "q8_0" :
             (fmt == ZCC_GGML_Q4_0) ? "q4_0" :
             (fmt == ZCC_GGML_Q4_K) ? "q4_k" :
             (fmt == ZCC_GGML_Q6_K) ? "q6_k" : "f32");
    void *fn = NULL;
    check_cu(cuModuleGetFunction(&fn, mod, kname), kname);
    if (fmt < 32U) pf->fn_dequant[fmt] = fn;
    return fn;
}

static void gpu_prefill_linear_tc(
    int aot_tier,
    void *mod,
    McPlan *mp,
    McPrefillFns *pf,
    const LlamaModel *m,
    const GpuActivations *act,
    uint64_t w_dptr_or_blob,
    uint64_t w_off,
    uint32_t w_type,
    uint64_t x_f32_ptr,
    uint64_t y_dptr,
    unsigned int M,
    unsigned int N,
    unsigned int K,
    unsigned int add_flag
) {
    {
        const void *h_dma = NULL;
        size_t n_dma = 0U;
        if (act && act->spill_stage[0] && find_spill_host_span(m, w_dptr_or_blob, &h_dma, &n_dma) && n_dma <= act->spill_stage_bytes) {
            check_cu(cuMemcpyHtoD_v2(act->spill_stage[0], h_dma, n_dma), "HtoD(pf_spill)");
            w_dptr_or_blob = act->spill_stage[0];
        }
    }
    if (aot_tier == 2 && w_type != ZCC_GGML_F16 && M <= 8U) {
        void *fn_mm = mc_resolve_matvec_multi(mod, mp, pf, w_type, K, M);
        void *pmm[8] = { &w_dptr_or_blob, &w_off, &x_f32_ptr, &y_dptr, &N, &K, &M, &add_flag };
        check_cu(cuLaunchKernel(fn_mm, (N + 3U) / 4U, 1U, 1U, 128U, 1U, 1U, 0U, NULL, pmm, NULL), "mc_pf_multi");
        return;
    }

    uint64_t wf16_ptr = (w_type == ZCC_GGML_F16) ? w_dptr_or_blob : act->w_f16_tmp;
    uint64_t wf16_off = (w_type == ZCC_GGML_F16) ? w_off : 0ULL;

    if (w_type != ZCC_GGML_F16) {
        unsigned int gx_dq = (K + 255U) / 256U;
        if (aot_tier == 2) {
            void *df = mc_resolve_dequant(mod, pf, w_type);
            void *pdq[5] = { &w_dptr_or_blob, &w_off, &act->w_f16_tmp, &N, &K };
            check_cu(cuLaunchKernel(df, gx_dq, N, 1U, 256U, 1U, 1U, 0U, NULL, pdq, NULL), "mc_dequant");
        } else {
            void *df = fn_dequant_f32_to_f16;
            if (w_type == ZCC_GGML_BF16) df = fn_dequant_bf16_to_f16;
            else if (w_type == ZCC_GGML_Q8_0) df = fn_dequant_q8_0_to_f16;
            else if (w_type == ZCC_GGML_Q4_0) df = fn_dequant_q4_0_to_f16;
            else if (w_type == ZCC_GGML_Q4_K) df = fn_dequant_q4_k_to_f16;
            else if (w_type == ZCC_GGML_Q6_K) df = fn_dequant_q6_k_to_f16;
            void *pdq[4] = { &w_dptr_or_blob, &act->w_f16_tmp, &N, &K };
            check_cu(cuLaunchKernel(df, gx_dq, N, 1U, 256U, 1U, 1U, 0U, NULL, pdq, NULL), "gen_dequant");
        }
    }

    if (aot_tier == 2) {
        unsigned int gx = (N + 31U) / 32U;
        unsigned int gy = (M + 15U) / 16U;
        uint64_t xf16 = act->x_f16;
        void *pg[8] = { &xf16, &wf16_ptr, &wf16_off, &y_dptr, &M, &N, &K, &add_flag };
        check_cu(cuLaunchKernel(pf->fn_prefill_tc_gemm, gx, gy, 1U, 128U, 1U, 1U, 0U, NULL, pg, NULL), "mc_prefill_tc");
    } else {
        unsigned int gx = (N + 7U) / 8U;
        unsigned int gy = (M + 15U) / 16U;
        uint64_t xf16 = act->x_f16;
        uint64_t dst_ptr = add_flag ? act->prefill_tmp : y_dptr;
        void *pg[6] = { &xf16, &wf16_ptr, &dst_ptr, &M, &N, &K };
        check_cu(cuLaunchKernel(fn_gemm_tc, gx, gy, 1U, 32U, 1U, 1U, 0U, NULL, pg, NULL), "gen_prefill_tc");
        if (add_flag) {
            unsigned int tot = M * N;
            unsigned int one = 1U;
            void *ps[4] = { &y_dptr, &dst_ptr, &tot, &one };
            check_cu(cuLaunchKernel(fn_scatter_tile, (tot + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, ps, NULL), "scatter_add");
        }
    }
}

static unsigned int run_prefill_tokens(
    int aot_tier,
    void *fn_aot_fwd,
    unsigned int aot_block_dim,
    void *mod,
    McPlan *mc_plan,
    McPrefillFns *pf,
    const LlamaModel *m,
    const GpuActivations *act,
    const uint32_t *toks,
    unsigned int n_toks,
    unsigned int start_pos,
    unsigned int eff_max_seq,
    int use_tensor_core,
    int kv_type,
    int prefill_batch,
    unsigned int *out_chunks,
    int *out_tc_used
) {
    int can_batch = (prefill_batch > 1) && (aot_tier != 1) &&
                    ((m->dim & 15U) == 0U) && ((m->hidden_dim & 15U) == 0U) && ((m->head_dim & 15U) == 0U);
    if (aot_tier == 2 && can_batch) {
        mc_init_prefill_fns(mod, mc_plan, pf, m, kv_type);
    }

    unsigned int pos = start_pos;
    unsigned int chunks = 0U;
    int tc_used = 0;
    unsigned int idx = 0U;

    while (idx < n_toks) {
        unsigned int B = can_batch ? (unsigned int)prefill_batch : 1U;
        if (aot_tier == 2 && act->w_f16_tmp == 0ULL && B > 8U) B = 8U;
        if (B > n_toks - idx) B = n_toks - idx;
        if (!can_batch || B < 2U) {
            for (unsigned int i = 0U; i < B; i++) {
                unsigned int tok = (unsigned int)toks[idx + i];
                if (tok >= m->vocab_size) tok = 0U;
                if (aot_tier == 1) {
                    void *p_aot[6] = { &m->d_wblob, &act->k_cache, &act->v_cache, &act->logits, &tok, &pos };
                    check_cu(cuLaunchKernel(fn_aot_fwd, 1U, 1U, 1U, aot_block_dim, 1U, 1U, 0U, NULL, p_aot, NULL), "aot_fwd");
                } else if (aot_tier == 2) {
                    mc_plan_step(mc_plan, act->step_args, tok, pos, eff_max_seq);
                } else {
                    gpu_forward_token(m, act, tok, pos, eff_max_seq, use_tensor_core, kv_type);
                }
                pos++;
                chunks++;
            }
            idx += B;
            continue;
        }

        tc_used = 1;
        chunks++;
        unsigned int dim = m->dim, hid = m->hidden_dim, kv_dim = m->kv_dim;
        float eps = m->rms_eps, theta = m->rope_theta;

        if (aot_tier == 2) {
            uint32_t sa[3];
            sa[0] = 0U; sa[1] = pos; sa[2] = eff_max_seq;
            check_cu(cuMemcpyHtoD_v2(act->step_args, sa, 12U), "HtoD(pf_step_args)");
            uint32_t tmp_toks[128];
            for (unsigned int ti = 0U; ti < B; ti++) {
                uint32_t tk = toks[idx + ti];
                tmp_toks[ti] = (tk < m->vocab_size) ? tk : 0U;
            }
            check_cu(cuMemcpyHtoD_v2(act->batch_toks, tmp_toks, (size_t)B * 4U), "HtoD(batch_toks)");
            uint64_t zero64 = 0ULL;
            for (unsigned int ti = 0U; ti < B; ti++) {
                uint64_t row_x = act->x + (uint64_t)ti * (uint64_t)dim * 4ULL;
                uint64_t targ = act->batch_toks + (uint64_t)ti * 4ULL;
                uint64_t temb = m->d_tok_emb;
                void *pe[4] = { &temb, &zero64, &row_x, &targ };
                check_cu(cuLaunchKernel(mc_plan->fn_embed, (dim + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pe, NULL), "mc_pf_emb");
            }
        } else {
            void *emb_fn = fn_embed_f32;
            if (m->t_tok_emb == ZCC_GGML_F16) emb_fn = fn_embed_f16;
            else if (m->t_tok_emb == ZCC_GGML_Q4_0) emb_fn = fn_embed_q4_0;
            else if (m->t_tok_emb == ZCC_GGML_Q8_0) emb_fn = fn_embed_q8_0;
            else if (m->t_tok_emb == ZCC_GGML_Q4_K) emb_fn = fn_embed_q4_k;
            else if (m->t_tok_emb == ZCC_GGML_Q6_K) emb_fn = fn_embed_q6_k;
            else if (m->t_tok_emb == ZCC_GGML_BF16) emb_fn = fn_embed_bf16;
            for (unsigned int ti = 0U; ti < B; ti++) {
                unsigned int tok = (unsigned int)toks[idx + ti];
                if (tok >= m->vocab_size) tok = 0U;
                uint64_t row_x = act->x + (uint64_t)ti * (uint64_t)dim * 4ULL;
                void *pe[4] = { &m->d_tok_emb, &row_x, &tok, &dim };
                check_cu(cuLaunchKernel(emb_fn, (dim + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pe, NULL), "gen_pf_emb");
            }
        }

        unsigned int b_dim = B * dim;
        unsigned int b_hid = B * hid;
        unsigned int grd_bdim = (b_dim + 255U) / 256U;
        unsigned int total_pairs = (m->n_heads + m->n_kv_heads) * (m->head_dim >> 1);
        void *cvt_fn = (aot_tier == 2) ? mc_plan->fn_f32_to_f16 : fn_f32_to_f16;

        uint64_t zero64 = 0ULL;
        for (unsigned int l = 0U; l < m->n_layers; l++) {
            const LayerWeights *lw = &m->layers[l];
            if (aot_tier == 2) {
                uint64_t an_d = lw->d_attn_norm;
                void *pr[4] = { &act->xb, &act->x, &an_d, &zero64 };
                check_cu(cuLaunchKernel(pf->fn_rmsnorm_batch, B, 1U, 1U, 128U, 1U, 1U, 0U, NULL, pr, NULL), "mc_pf_rn1");
            } else {
                uint64_t an = lw->d_attn_norm;
                void *pr[5] = { &act->xb, &act->x, &an, &dim, &eps };
                check_cu(cuLaunchKernel(fn_rmsnorm_batch, B, 1U, 1U, 32U, 1U, 1U, 0U, NULL, pr, NULL), "gen_pf_rn1");
            }

            void *pcv1[3] = { &act->xb, &act->x_f16, &b_dim };
            check_cu(cuLaunchKernel(cvt_fn, grd_bdim, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pcv1, NULL), "pf_cvt1");

            gpu_prefill_linear_tc(aot_tier, mod, mc_plan, pf, m, act, lw->d_wq, 0ULL, lw->t_wq, act->xb, act->q, B, dim, dim, 0U);
            gpu_prefill_linear_tc(aot_tier, mod, mc_plan, pf, m, act, lw->d_wk, 0ULL, lw->t_wk, act->xb, act->k, B, kv_dim, dim, 0U);
            gpu_prefill_linear_tc(aot_tier, mod, mc_plan, pf, m, act, lw->d_wv, 0ULL, lw->t_wv, act->xb, act->v, B, kv_dim, dim, 0U);

            if (lw->has_bq) {
                if (aot_tier == 2) {
                    uint64_t db = lw->d_bq;
                    void *pb[4] = { &act->q, &db, &zero64, &dim };
                    check_cu(cuLaunchKernel(pf->fn_add_bias_batch, (dim + 255U) / 256U, B, 1U, 256U, 1U, 1U, 0U, NULL, pb, NULL), "mc_pf_bq");
                } else {
                    uint64_t db = lw->d_bq;
                    void *pb[3] = { &act->q, &db, &dim };
                    check_cu(cuLaunchKernel(fn_add_bias_batch, (dim + 255U) / 256U, B, 1U, 256U, 1U, 1U, 0U, NULL, pb, NULL), "gen_pf_bq");
                }
            }
            if (lw->has_bk) {
                if (aot_tier == 2) {
                    uint64_t db = lw->d_bk;
                    void *pb[4] = { &act->k, &db, &zero64, &kv_dim };
                    check_cu(cuLaunchKernel(pf->fn_add_bias_batch, (kv_dim + 255U) / 256U, B, 1U, 256U, 1U, 1U, 0U, NULL, pb, NULL), "mc_pf_bk");
                } else {
                    uint64_t db = lw->d_bk;
                    void *pb[3] = { &act->k, &db, &kv_dim };
                    check_cu(cuLaunchKernel(fn_add_bias_batch, (kv_dim + 255U) / 256U, B, 1U, 256U, 1U, 1U, 0U, NULL, pb, NULL), "gen_pf_bk");
                }
            }
            if (lw->has_bv) {
                if (aot_tier == 2) {
                    uint64_t db = lw->d_bv;
                    void *pb[4] = { &act->v, &db, &zero64, &kv_dim };
                    check_cu(cuLaunchKernel(pf->fn_add_bias_batch, (kv_dim + 255U) / 256U, B, 1U, 256U, 1U, 1U, 0U, NULL, pb, NULL), "mc_pf_bv");
                } else {
                    uint64_t db = lw->d_bv;
                    void *pb[3] = { &act->v, &db, &kv_dim };
                    check_cu(cuLaunchKernel(fn_add_bias_batch, (kv_dim + 255U) / 256U, B, 1U, 256U, 1U, 1U, 0U, NULL, pb, NULL), "gen_pf_bv");
                }
            }

            if (aot_tier == 2) {
                if (kv_type == ZCC_KV_Q8_0) {
                    void *pri[5] = { &act->q, &act->k, &act->step_args, &pos, &B };
                    check_cu(cuLaunchKernel(pf->fn_rope_inplace_batch, (total_pairs + 127U) / 128U, B, 1U, 128U, 1U, 1U, 0U, NULL, pri, NULL), "mc_pf_ri");
                    void *pkv[8] = { &act->k, &act->v, &act->k_cache, &act->v_cache, &act->step_args, &l, &pos, &B };
                    check_cu(cuLaunchKernel(pf->fn_kv_store_batch_q8_0, (kv_dim >> 5) * 2U, B, 1U, 32U, 1U, 1U, 0U, NULL, pkv, NULL), "mc_pf_kvq8");
                } else {
                    unsigned int mx = (total_pairs > kv_dim) ? total_pairs : kv_dim;
                    void *prk[9] = { &act->q, &act->k, &act->v, &act->k_cache, &act->v_cache, &act->step_args, &l, &pos, &B };
                    check_cu(cuLaunchKernel(pf->fn_rope_kv_batch, (mx + 127U) / 128U, B, 1U, 128U, 1U, 1U, 0U, NULL, prk, NULL), "mc_pf_rkv");
                }
                void *pa[8] = { &act->q, &act->k_cache, &act->v_cache, &act->xb, &act->step_args, &l, &pos, &B };
                check_cu(cuLaunchKernel(pf->fn_attn_batch, m->n_heads, B, 1U, 32U, 1U, 1U, 0U, NULL, pa, NULL), "mc_pf_attn");
            } else {
                unsigned int nh = m->n_heads, nkv = m->n_kv_heads, hd = m->head_dim, mseq = eff_max_seq;
                void *rfn = (strcmp(m->arch, "qwen2") == 0) ? fn_rope_neox_batch : fn_rope_batch;
                void *prp[7] = { &act->q, &act->k, &pos, &nh, &nkv, &hd, &theta };
                check_cu(cuLaunchKernel(rfn, (total_pairs + 255U) / 256U, B, 1U, 256U, 1U, 1U, 0U, NULL, prp, NULL), "gen_pf_rope");
                void *pkv[8] = { &act->k, &act->v, &act->k_cache, &act->v_cache, &l, &pos, &mseq, &kv_dim };
                void *pa[10] = { &act->q, &act->k_cache, &act->v_cache, &act->xb, &l, &pos, &mseq, &nh, &nkv, &hd };
                if (kv_type == ZCC_KV_F16) {
                    check_cu(cuLaunchKernel(fn_kv_store_batch_f16, (kv_dim + 255U) / 256U, B, 1U, 256U, 1U, 1U, 0U, NULL, pkv, NULL), "gen_pf_kv16");
                    check_cu(cuLaunchKernel(fn_attn_flash_batch_f16, nh, B, 1U, 32U, 1U, 1U, 0U, NULL, pa, NULL), "gen_pf_at16");
                } else if (kv_type == ZCC_KV_Q8_0) {
                    check_cu(cuLaunchKernel(fn_kv_store_batch_q8_0, (kv_dim >> 5) * 2U, B, 1U, 32U, 1U, 1U, 0U, NULL, pkv, NULL), "gen_pf_kvq8");
                    check_cu(cuLaunchKernel(fn_attn_flash_batch_q8_0, nh, B, 1U, 32U, 1U, 1U, 0U, NULL, pa, NULL), "gen_pf_atq8");
                } else {
                    check_cu(cuLaunchKernel(fn_kv_store_batch_f32, (kv_dim + 255U) / 256U, B, 1U, 256U, 1U, 1U, 0U, NULL, pkv, NULL), "gen_pf_kv32");
                    check_cu(cuLaunchKernel(fn_attn_flash_batch_f32, nh, B, 1U, 32U, 1U, 1U, 0U, NULL, pa, NULL), "gen_pf_at32");
                }
            }

            void *pcv2[3] = { &act->xb, &act->x_f16, &b_dim };
            check_cu(cuLaunchKernel(cvt_fn, grd_bdim, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pcv2, NULL), "pf_cvt2");
            gpu_prefill_linear_tc(aot_tier, mod, mc_plan, pf, m, act, lw->d_wo, 0ULL, lw->t_wo, act->xb, act->x, B, dim, dim, 1U);

            if (aot_tier == 2) {
                uint64_t fn_d = lw->d_ffn_norm;
                void *pr[4] = { &act->xb, &act->x, &fn_d, &zero64 };
                check_cu(cuLaunchKernel(pf->fn_rmsnorm_batch, B, 1U, 1U, 128U, 1U, 1U, 0U, NULL, pr, NULL), "mc_pf_rn2");
            } else {
                uint64_t fn = lw->d_ffn_norm;
                void *pr[5] = { &act->xb, &act->x, &fn, &dim, &eps };
                check_cu(cuLaunchKernel(fn_rmsnorm_batch, B, 1U, 1U, 32U, 1U, 1U, 0U, NULL, pr, NULL), "gen_pf_rn2");
            }

            void *pcv3[3] = { &act->xb, &act->x_f16, &b_dim };
            check_cu(cuLaunchKernel(cvt_fn, grd_bdim, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pcv3, NULL), "pf_cvt3");
            gpu_prefill_linear_tc(aot_tier, mod, mc_plan, pf, m, act, lw->d_wgate, 0ULL, lw->t_wgate, act->xb, act->hb, B, hid, dim, 0U);
            gpu_prefill_linear_tc(aot_tier, mod, mc_plan, pf, m, act, lw->d_wup, 0ULL, lw->t_wup, act->xb, act->hb2, B, hid, dim, 0U);

            void *psw[3] = { &act->hb, &act->hb2, &b_hid };
            void *sw_fn = (aot_tier == 2) ? pf->fn_swiglu_batch : fn_swiglu;
            check_cu(cuLaunchKernel(sw_fn, (b_hid + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, psw, NULL), "pf_swiglu");

            void *pcv4[3] = { &act->hb, &act->x_f16, &b_hid };
            check_cu(cuLaunchKernel(cvt_fn, (b_hid + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pcv4, NULL), "pf_cvt4");
            gpu_prefill_linear_tc(aot_tier, mod, mc_plan, pf, m, act, lw->d_wdown, 0ULL, lw->t_wdown, act->hb, act->x, B, dim, hid, 1U);
        }

        if (idx + B == n_toks) {
            uint64_t x_last = act->x + (uint64_t)(B - 1U) * (uint64_t)dim * 4ULL;
            if (aot_tier == 2) {
                uint64_t on_d = m->d_out_norm;
                void *pr[4] = { &act->xb, &x_last, &on_d, &zero64 };
                check_cu(cuLaunchKernel(mc_plan->fn_rmsnorm, 1U, 1U, 1U, 128U, 1U, 1U, 0U, NULL, pr, NULL), "mc_pf_rnf");
                int wout_tc = (m->t_wout == ZCC_GGML_F16 && (dim & 15U) == 0U);
                if (wout_tc) {
                    void *pcv[3] = { &act->xb, &act->x_f16, &dim };
                    check_cu(cuLaunchKernel(mc_plan->fn_f32_to_f16, (dim + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pcv, NULL), "mc_pf_cvto");
                }
                void *lfn = mc_resolve_matvec(mod, mc_plan, m->t_wout, dim);
                unsigned int gx_w = wout_tc ? ((m->vocab_size + 7U) / 8U) : ((m->vocab_size + 3U) / 4U);
                unsigned int bx_w = wout_tc ? 32U : 128U;
                uint32_t zero_f = 0U;
                uint64_t wout_d = m->d_wout;
                {
                    const void *h_dma = NULL;
                    size_t n_dma = 0U;
                    if (act->spill_stage[0] && find_spill_host_span(m, wout_d, &h_dma, &n_dma) && n_dma <= act->spill_stage_bytes) {
                        check_cu(cuMemcpyHtoD_v2(act->spill_stage[0], h_dma, n_dma), "HtoD(pf_wout_spill)");
                        wout_d = act->spill_stage[0];
                    }
                }
                uint64_t a0 = wout_tc ? act->x_f16 : wout_d;
                uint64_t a1 = wout_tc ? wout_d : zero64;
                uint64_t a2 = wout_tc ? zero64 : act->xb;
                void *pl[7] = { &a0, &a1, &a2, &act->logits, &m->vocab_size, &dim, &zero_f };
                check_cu(cuLaunchKernel(lfn, gx_w, 1U, 1U, bx_w, 1U, 1U, 0U, NULL, pl, NULL), "mc_pf_wout");
            } else {
                uint64_t on = m->d_out_norm;
                void *pr[5] = { &act->xb, &x_last, &on, &dim, &eps };
                check_cu(cuLaunchKernel(fn_rmsnorm, 1U, 1U, 1U, 32U, 1U, 1U, 0U, NULL, pr, NULL), "gen_pf_rnf");
                gpu_linear_gen(m->d_wout, m->t_wout, act->xb, act->x_f16, act->logits, m->vocab_size, dim, use_tensor_core);
            }
        }
        pos += B;
        idx += B;
    }

    if (out_chunks) *out_chunks = chunks;
    if (out_tc_used) *out_tc_used = tc_used;
    return pos;
}

/* ========================================================================= */
/* Helpers, 512^3 TC Bench, FNV-1a, and Production Sampler                   */
/* ========================================================================= */
static uint16_t f32_to_f16_host(float f) {
    uint32_t x = 0U; memcpy(&x, &f, 4U);
    uint32_t sign = (x >> 16) & 0x8000U;
    uint32_t exp = (x >> 23) & 0xFFU;
    uint32_t mant = x & 0x7FFFFFU;
    if (exp == 0U) return (uint16_t)sign;
    if (exp == 0xFFU) return (uint16_t)(sign | 0x7C00U | (mant ? 0x0200U : 0U));
    int new_exp = (int)exp - 127 + 15;
    if (new_exp >= 31) return (uint16_t)(sign | 0x7C00U);
    if (new_exp <= 0) return (uint16_t)sign;
    return (uint16_t)(sign | ((uint32_t)new_exp << 10) | (mant >> 13));
}

static float f16_to_f32_host(uint16_t h) {
    uint32_t sign = ((uint32_t)h & 0x8000U) << 16;
    uint32_t exp = (h >> 10) & 0x1FU;
    uint32_t mant = h & 0x3FFU;
    uint32_t out = 0U;
    if (exp == 0U) out = sign;
    else if (exp == 31U) out = sign | 0x7F800000U | (mant << 13);
    else out = sign | ((exp - 15U + 127U) << 23) | (mant << 13);
    float rf = 0.0f; memcpy(&rf, &out, 4U);
    return rf;
}

static void run_gemm_benchmark(void *ev0, void *ev1) {
    unsigned int M = 512U, N = 512U, K = 512U;
    size_t sz_a = (size_t)M * K * sizeof(uint16_t);
    size_t sz_b = (size_t)N * K * sizeof(uint16_t);
    size_t sz_d = (size_t)M * N * sizeof(float);
    uint16_t *ha = (uint16_t *)malloc(sz_a);
    uint16_t *hb = (uint16_t *)malloc(sz_b);
    float *hd = (float *)malloc(sz_d);
    for (unsigned int i = 0U; i < M * K; i++) ha[i] = f32_to_f16_host((float)((int)(i % 13U) - 6) * 0.0625f);
    for (unsigned int i = 0U; i < N * K; i++) hb[i] = f32_to_f16_host((float)((int)(i % 17U) - 8) * 0.0625f);
    uint64_t da = 0ULL, db = 0ULL, dd = 0ULL;
    check_cu(cuMemAlloc_v2(&da, sz_a), "bench_a");
    check_cu(cuMemAlloc_v2(&db, sz_b), "bench_b");
    check_cu(cuMemAlloc_v2(&dd, sz_d), "bench_d");
    check_cu(cuMemcpyHtoD_v2(da, ha, sz_a), "bench_HtoD_a");
    check_cu(cuMemcpyHtoD_v2(db, hb, sz_b), "bench_HtoD_b");

    unsigned int gx = (N + 7U) / 8U, gy = (M + 15U) / 16U;
    void *params[6] = { &da, &db, &dd, &M, &N, &K };
    check_cu(cuLaunchKernel(fn_gemm_tc, gx, gy, 1U, 32U, 1U, 1U, 0U, NULL, params, NULL), "bench_warmup");
    check_cu(cuCtxSynchronize(), "bench_warmup_sync");

    check_cu(cuEventRecord(ev0, NULL), "bench_ev0");
    check_cu(cuLaunchKernel(fn_gemm_tc, gx, gy, 1U, 32U, 1U, 1U, 0U, NULL, params, NULL), "bench_run");
    check_cu(cuEventRecord(ev1, NULL), "bench_ev1");
    check_cu(cuEventSynchronize(ev1), "bench_sync");
    float ms = 0.0f;
    check_cu(cuEventElapsedTime(&ms, ev0, ev1), "bench_elapsed");
    check_cu(cuMemcpyDtoH_v2(hd, dd, sz_d), "bench_DtoH");

    double max_err = 0.0;
    for (unsigned int r = 0U; r < M; r += 31U) {
        for (unsigned int c = 0U; c < N; c += 29U) {
            double ref = 0.0;
            for (unsigned int k = 0U; k < K; k++) {
                ref += (double)f16_to_f32_host(ha[r * K + k]) * (double)f16_to_f32_host(hb[c * K + k]);
            }
            double e = fabs((double)hd[r * N + c] - ref);
            if (e > max_err) max_err = e;
        }
    }
    if (max_err > 5e-2) {
        fprintf(stderr, "GEMM benchmark accuracy failure: max_err=%.8f\n", max_err);
        exit(1);
    }
    double flops = 2.0 * (double)M * (double)N * (double)K;
    double tflops = (ms > 0.0f) ? (flops / ((double)ms * 1e9)) : 0.0;
    printf("GPU_LLM_GEMM_BENCH_OK M=%u N=%u K=%u gpu_ms=%.4f tflops=%.4f max_err=%.8f\n",
           M, N, K, (double)ms, tflops, max_err);
    cuMemFree_v2(da); cuMemFree_v2(db); cuMemFree_v2(dd);
    free(ha); free(hb); free(hd);
}

static uint32_t fnv1a_floats(const float *arr, unsigned int n) {
    uint32_t h = 2166136261U;
    for (unsigned int i = 0U; i < n; i++) {
        uint32_t u = 0U; memcpy(&u, &arr[i], 4U);
        h ^= (u & 0xFFU); h *= 16777619U;
        h ^= ((u >> 8) & 0xFFU); h *= 16777619U;
        h ^= ((u >> 16) & 0xFFU); h *= 16777619U;
        h ^= ((u >> 24) & 0xFFU); h *= 16777619U;
    }
    return h;
}

static void print_sanitized(const char *s) {
    for (size_t i = 0U; s[i]; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\n') printf("\\n");
        else if (c == '\r') printf("\\r");
        else if (c == '"') printf("\\\"");
        else if (c >= 32U && c < 127U) putchar((int)c);
        else printf("\\x%02x", (unsigned int)c);
    }
}

static char *read_text_file_alloc(const char *path, size_t *out_len) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    if (fseek(fp, 0L, SEEK_END) != 0) { fclose(fp); return NULL; }
    long sz_l = ftell(fp);
    if (sz_l <= 0L) { fclose(fp); return NULL; }
    size_t sz = (size_t)sz_l;
    fseek(fp, 0L, SEEK_SET);
    char *buf = (char *)malloc(sz + 1U);
    if (!buf || fread(buf, 1U, sz, fp) != sz) {
        if (buf) free(buf);
        fclose(fp);
        return NULL;
    }
    buf[sz] = '\0';
    fclose(fp);
    if (out_len) *out_len = sz;
    return buf;
}

static void check_finite_logits(const float *logits, unsigned int vocab_size, int step_idx) {
    for (unsigned int i = 0U; i < vocab_size; i++) {
        float lv = logits[i];
        if (!(lv >= -1e38f && lv <= 1e38f)) {
#ifdef ZCC_GPU_LLM_LIBRARY
            char msg[160];
            if (step_idx < 0) snprintf(msg, sizeof(msg), "Non-finite GPU logit detected at idx %u", i);
            else snprintf(msg, sizeof(msg), "Non-finite GPU logit at step %d idx %u", step_idx, i);
            zcc_llm_set_err(msg);
            return;
#else
            if (step_idx < 0) fprintf(stderr, "FATAL: Non-finite GPU logit detected at idx %u\n", i);
            else fprintf(stderr, "FATAL: Non-finite GPU logit at step %d idx %u\n", step_idx, i);
            exit(1);
#endif
        }
    }
}

typedef struct {
    float prob;
    uint32_t id;
} SampleCand;

static int cmp_cand_desc(const void *pa, const void *pb) {
    const SampleCand *a = (const SampleCand *)pa;
    const SampleCand *b = (const SampleCand *)pb;
    if (a->prob > b->prob) return -1;
    if (a->prob < b->prob) return 1;
    return (a->id < b->id) ? -1 : ((a->id > b->id) ? 1 : 0);
}

static int prepare_filtered_cands(
    const float *h_logits,
    float *work_logits,
    float *h_probs,
    SampleCand *cands,
    unsigned int vocab_size,
    const uint32_t *history,
    unsigned int hist_len,
    float temp,
    int top_k,
    float top_p,
    float min_p,
    float repeat_penalty,
    int *out_mode,
    int *out_argmax,
    double *out_sum_p,
    unsigned int *out_active
) {
    memcpy(work_logits, h_logits, (size_t)vocab_size * sizeof(float));
    if (repeat_penalty != 1.0f && hist_len > 0U) {
        unsigned int start = (hist_len > 64U) ? (hist_len - 64U) : 0U;
        for (unsigned int h = start; h < hist_len; h++) {
            uint32_t tid = history[h];
            if (tid >= vocab_size) continue;
            int seen = 0;
            for (unsigned int q = start; q < h; q++) {
                if (history[q] == tid) { seen = 1; break; }
            }
            if (seen) continue;
            float lv = work_logits[tid];
            work_logits[tid] = (lv > 0.0f) ? (lv / repeat_penalty) : (lv * repeat_penalty);
        }
    }

    if (temp <= 0.0f) {
        int nxt = 0;
        float best = work_logits[0];
        for (unsigned int i = 1U; i < vocab_size; i++) {
            if (work_logits[i] > best) { best = work_logits[i]; nxt = (int)i; }
        }
        *out_mode = 0;
        *out_argmax = nxt;
        *out_sum_p = 1.0;
        *out_active = 1U;
        return 0;
    }

    double max_l = (double)work_logits[0];
    for (unsigned int i = 1U; i < vocab_size; i++) {
        if ((double)work_logits[i] > max_l) max_l = (double)work_logits[i];
    }
    double inv_t = 1.0 / (double)temp;
    double sum_p = 0.0;
    for (unsigned int i = 0U; i < vocab_size; i++) {
        double w = exp(((double)work_logits[i] - max_l) * inv_t);
        h_probs[i] = (float)w;
        sum_p += w;
    }

    if (top_k == 0 && top_p >= 1.0f && min_p <= 0.0f) {
        *out_mode = 1;
        *out_argmax = 0;
        *out_sum_p = sum_p;
        *out_active = vocab_size;
        return 0;
    }

    double inv_sum = (sum_p > 0.0) ? (1.0 / sum_p) : 0.0;
    unsigned int n_cands = vocab_size;
    if (vocab_size > 4096U) {
        double cutoff = (min_p > 0.0f) ? ((double)min_p * 0.99) : 1e-6;
        unsigned int nc = 0U;
        double kept_w = 0.0;
        for (unsigned int i = 0U; i < vocab_size; i++) {
            double w = (double)h_probs[i];
            if (w >= cutoff) {
                cands[nc].prob = (float)(w * inv_sum);
                cands[nc].id = i;
                nc++;
                kept_w += w;
            }
        }
        if (nc > 0U &&
            !(top_k > 0 && nc < (unsigned int)top_k && min_p <= 0.0f) &&
            !(top_p < 1.0f && min_p <= 0.0f && kept_w < (double)top_p * sum_p)) {
            n_cands = nc;
        }
    }
    if (n_cands == vocab_size) {
        for (unsigned int i = 0U; i < vocab_size; i++) {
            cands[i].prob = (float)((double)h_probs[i] * inv_sum);
            cands[i].id = i;
        }
    }
    qsort(cands, (size_t)n_cands, sizeof(SampleCand), cmp_cand_desc);

    unsigned int active = n_cands;
    if (top_k > 0 && (unsigned int)top_k < active) active = (unsigned int)top_k;
    if (min_p > 0.0f && active > 1U) {
        float thresh = cands[0].prob * min_p;
        unsigned int kept = 1U;
        while (kept < active && cands[kept].prob >= thresh) kept++;
        active = kept;
    }
    if (top_p < 1.0f && active > 1U) {
        double cum = 0.0;
        unsigned int kept = 0U;
        for (unsigned int i = 0U; i < active; i++) {
            cum += (double)cands[i].prob;
            kept = i + 1U;
            if (cum >= (double)top_p) break;
        }
        active = (kept > 0U) ? kept : 1U;
    }
    *out_mode = 2;
    *out_argmax = (int)cands[0].id;
    *out_sum_p = sum_p;
    *out_active = active;
    return 0;
}

static void fill_filtered_probs(
    const float *h_logits,
    float *work_logits,
    float *h_probs,
    SampleCand *cands,
    unsigned int vocab_size,
    const uint32_t *history,
    unsigned int hist_len,
    float temp,
    int top_k,
    float top_p,
    float min_p,
    float repeat_penalty,
    float *out_probs
) {
    int mode = 0, argmax_id = 0;
    double sum_p = 0.0;
    unsigned int active = 0U;
    prepare_filtered_cands(h_logits, work_logits, h_probs, cands, vocab_size, history, hist_len,
                           temp, top_k, top_p, min_p, repeat_penalty, &mode, &argmax_id, &sum_p, &active);
    memset(out_probs, 0, (size_t)vocab_size * sizeof(float));
    if (mode == 0) {
        out_probs[(unsigned int)argmax_id] = 1.0f;
    } else if (mode == 1) {
        double inv = (sum_p > 0.0) ? (1.0 / sum_p) : 0.0;
        for (unsigned int i = 0U; i < vocab_size; i++) out_probs[i] = (float)((double)h_probs[i] * inv);
    } else {
        double fsum = 0.0;
        for (unsigned int i = 0U; i < active; i++) fsum += (double)cands[i].prob;
        double inv = (fsum > 0.0) ? (1.0 / fsum) : 0.0;
        for (unsigned int i = 0U; i < active; i++) {
            if (cands[i].id < vocab_size) out_probs[cands[i].id] = (float)((double)cands[i].prob * inv);
        }
    }
}

static int sample_from_prob_vec(const float *probs, unsigned int vocab_size, uint32_t *io_seed) {
    double sum = 0.0;
    unsigned int best_id = 0U;
    float best_p = probs[0];
    for (unsigned int i = 0U; i < vocab_size; i++) {
        sum += (double)probs[i];
        if (probs[i] > best_p) { best_p = probs[i]; best_id = i; }
    }
    if (!(sum > 0.0)) return (int)best_id;
    *io_seed = (*io_seed) * 1664525U + 1013904223U;
    double u = ((double)(((*io_seed) >> 8) & 0x00FFFFFFU) / 16777216.0) * sum;
    double cdf = 0.0;
    for (unsigned int i = 0U; i < vocab_size; i++) {
        cdf += (double)probs[i];
        if (u <= cdf) return (int)i;
    }
    return (int)best_id;
}

static int sample_next_token(
    const float *h_logits,
    float *work_logits,
    float *h_probs,
    SampleCand *cands,
    unsigned int vocab_size,
    const uint32_t *history,
    unsigned int hist_len,
    float temp,
    int top_k,
    float top_p,
    float min_p,
    float repeat_penalty,
    uint32_t *io_seed
) {
    int mode = 0, argmax_id = 0;
    double sum_p = 0.0;
    unsigned int active = 0U;
    prepare_filtered_cands(h_logits, work_logits, h_probs, cands, vocab_size, history, hist_len,
                           temp, top_k, top_p, min_p, repeat_penalty, &mode, &argmax_id, &sum_p, &active);
    if (mode == 0) return argmax_id;
    if (mode == 1) {
        *io_seed = (*io_seed) * 1664525U + 1013904223U;
        double u = ((double)(((*io_seed) >> 8) & 0x00FFFFFFU) / 16777216.0) * sum_p;
        double cdf = 0.0;
        for (unsigned int i = 0U; i < vocab_size; i++) {
            cdf += (double)h_probs[i];
            if (u <= cdf) return (int)i;
        }
        return (int)(vocab_size - 1U);
    }
    if (active == 1U) return (int)cands[0].id;

    double fsum = 0.0;
    for (unsigned int i = 0U; i < active; i++) fsum += (double)cands[i].prob;
    *io_seed = (*io_seed) * 1664525U + 1013904223U;
    double u = ((double)(((*io_seed) >> 8) & 0x00FFFFFFU) / 16777216.0) * fsum;
    double cdf = 0.0;
    for (unsigned int i = 0U; i < active; i++) {
        cdf += (double)cands[i].prob;
        if (u <= cdf) return (int)cands[i].id;
    }
    return (int)cands[active - 1U].id;
}

static int parse_finite_float_arg(const char *val, const char *flag, float lo, int lo_incl, float hi, int hi_incl, float *out) {
    char *endp = NULL;
    double v = strtod(val, &endp);
    if (!val[0] || (endp && *endp != '\0') || !(v >= -1e38 && v <= 1e38)) {
        fprintf(stderr, "INVALID_ARG: %s must be finite\n", flag);
        return -1;
    }
    if ((lo_incl ? (v < (double)lo) : (v <= (double)lo)) ||
        (hi_incl ? (v > (double)hi) : (v >= (double)hi))) {
        fprintf(stderr, "INVALID_ARG: %s out of valid range\n", flag);
        return -1;
    }
    *out = (float)v;
    return 0;
}

static int parse_int_arg(const char *val, const char *flag, int min_v, int max_v, int *out) {
    char *endp = NULL;
    long v = strtol(val, &endp, 10);
    if (!val[0] || (endp && *endp != '\0') || v < (long)min_v || v > (long)max_v) {
        fprintf(stderr, "INVALID_ARG: %s must be an integer in [%d, %d]\n", flag, min_v, max_v);
        return -1;
    }
    *out = (int)v;
    return 0;
}

static char *format_chat_prompt_alloc(const LlamaModel *m, const char *text, const char *sys_text, int cont_turn) {
    int is_chatml = (strcmp(m->arch, "qwen2") == 0) ||
                    (tok_lookup(&m->tok, "<|im_start|>", 12U) >= 0);
    size_t tlen = text ? strlen(text) : 0U;
    size_t slen = sys_text ? strlen(sys_text) : 0U;
    size_t cap = tlen + slen + 280U;
    char *buf = (char *)malloc(cap);
#ifdef ZCC_GPU_LLM_LIBRARY
    if (!buf) { zcc_llm_set_err("OOM for chat buffer"); return NULL; }
#else
    if (!buf) { fprintf(stderr, "FATAL: OOM for chat buffer\n"); exit(1); }
#endif
    buf[0] = '\0';
    size_t off = 0U;
    if (cont_turn && text) {
        if (is_chatml) {
            off += (size_t)snprintf(buf + off, cap - off, "<|im_end|>\n");
        } else {
            off += (size_t)snprintf(buf + off, cap - off, " </s>");
        }
    }
    if (sys_text && sys_text[0] != '\0') {
        if (is_chatml) {
            off += (size_t)snprintf(buf + off, cap - off, "<|im_start|>system\n%s<|im_end|>\n", sys_text);
        } else {
            off += (size_t)snprintf(buf + off, cap - off, "[INST] <<SYS>>\n%s\n<</SYS>> [/INST] ", sys_text);
        }
    }
    if (text) {
        if (is_chatml) {
            snprintf(buf + off, cap - off, "<|im_start|>user\n%s<|im_end|>\n<|im_start|>assistant\n", text);
        } else {
            snprintf(buf + off, cap - off, "[INST] %s [/INST]", text);
        }
    }
    return buf;
}

static int is_stop_token(const LlamaModel *m, int ignore_eos, uint32_t tok) {
    if (m->tok.mode == ZCC_TOK_ASCII_RAW || ignore_eos) return 0;
    return (m->tok.has_eos && tok == m->tok.eos_id) ||
           (m->tok.has_eot && tok == m->tok.eot_id);
}

static void record_generated_token_ex(
    const LlamaModel *m,
    uint32_t tok,
    int *gen_ids,
    int *io_gen_count,
    uint32_t *hist,
    unsigned int *io_hist_len,
    char **io_gen_text,
    size_t *io_gen_cap,
    size_t *io_gen_text_len,
    int stream_out,
    void (*stream_cb)(const char *, int, void *),
    void *stream_cb_ud
) {
    gen_ids[(*io_gen_count)++] = (int)tok;
    hist[(*io_hist_len)++] = tok;
    char piece[256];
    size_t plen = detokenize_piece(&m->tok, tok, piece, sizeof(piece));
    if (plen >= sizeof(piece)) plen = sizeof(piece) - 1U;
    piece[plen] = '\0';
    if (*io_gen_text_len + plen + 1U > *io_gen_cap) {
        *io_gen_cap = *io_gen_text_len + plen + 256U;
        *io_gen_text = (char *)realloc(*io_gen_text, *io_gen_cap);
    }
    if (plen > 0U) {
        memcpy((*io_gen_text) + *io_gen_text_len, piece, plen);
        *io_gen_text_len += plen;
        (*io_gen_text)[*io_gen_text_len] = '\0';
        if (stream_out) { fwrite(piece, 1U, plen, stdout); fflush(stdout); }
        if (stream_cb) stream_cb(piece, (int)tok, stream_cb_ud);
    }
}

static void record_generated_token(
    const LlamaModel *m,
    uint32_t tok,
    int *gen_ids,
    int *io_gen_count,
    uint32_t *hist,
    unsigned int *io_hist_len,
    char **io_gen_text,
    size_t *io_gen_cap,
    size_t *io_gen_text_len,
    int stream_out
) {
    record_generated_token_ex(m, tok, gen_ids, io_gen_count, hist, io_hist_len,
                              io_gen_text, io_gen_cap, io_gen_text_len,
                              stream_out, NULL, NULL);
}

static int spec_check_vocab_compat(const LlamaModel *draft, const LlamaModel *target) {
    if (draft->tok.mode != target->tok.mode) {
        fprintf(stderr, "SPEC_VOCAB_MISMATCH: tokenizer mode mismatch (%d vs %d)\n",
                draft->tok.mode, target->tok.mode);
        return -1;
    }
    if (draft->tok.has_eos != target->tok.has_eos ||
        (draft->tok.has_eos && draft->tok.eos_id != target->tok.eos_id) ||
        draft->tok.has_eot != target->tok.has_eot ||
        (draft->tok.has_eot && draft->tok.eot_id != target->tok.eot_id) ||
        draft->tok.has_bos != target->tok.has_bos ||
        (draft->tok.has_bos && draft->tok.bos_id != target->tok.bos_id)) {
        fprintf(stderr, "SPEC_VOCAB_MISMATCH: special control token IDs do not match\n");
        return -1;
    }
    if (draft->tok.mode != ZCC_TOK_ASCII_RAW) {
        unsigned int min_v = (draft->vocab_size < target->vocab_size) ? draft->vocab_size : target->vocab_size;
        if (!draft->tok.tok_off || !draft->tok.tok_len || !draft->tok.pool ||
            !target->tok.tok_off || !target->tok.tok_len || !target->tok.pool ||
            draft->tok.vocab_size < min_v || target->tok.vocab_size < min_v) {
            fprintf(stderr, "SPEC_VOCAB_MISMATCH: missing tokenizer tables\n");
            return -1;
        }
        for (unsigned int i = 0U; i < min_v; i++) {
            uint32_t dl = draft->tok.tok_len[i];
            uint32_t tl = target->tok.tok_len[i];
            if (dl != tl || (dl > 0U && memcmp(draft->tok.pool + draft->tok.tok_off[i],
                                               target->tok.pool + target->tok.tok_off[i], (size_t)dl) != 0)) {
                fprintf(stderr, "SPEC_VOCAB_MISMATCH: token %u differs between draft and target\n", i);
                return -1;
            }
        }
    }
    return 0;
}

static int load_ptx_module(
    const char *path,
    const void *fallback_ptx,
    unsigned int fallback_bytes,
    int query_aot_meta,
    int *out_aot_tier,
    void **out_mod,
    uint64_t out_meta[16],
    unsigned int *out_bytes
) {
    char *ptx_buf = NULL;
    size_t ptx_len = 0U;
    const void *ptx_image = fallback_ptx;
    unsigned int byte_cnt = fallback_bytes;
    if (path) {
        ptx_buf = read_text_file_alloc(path, &ptx_len);
        if (!ptx_buf) {
            char msg[320];
            snprintf(msg, sizeof(msg), "AOT_PTX_ERROR: cannot read '%s'", path);
            fprintf(stderr, "%s\n", msg);
            zcc_llm_set_err(msg);
            return -1;
        }
        ptx_image = (const void *)ptx_buf;
        byte_cnt = (unsigned int)ptx_len;
    }

    void *mod = NULL;
    char jit_err[4096];
    memset(jit_err, 0, sizeof(jit_err));
    int jit_opts[2] = { 5, 6 };
    void *jit_vals[2] = { (void *)jit_err, (void *)(uintptr_t)sizeof(jit_err) };
    int load_rc = cuModuleLoadDataEx(&mod, ptx_image, 2U, jit_opts, jit_vals);
    if (ptx_buf) { free(ptx_buf); ptx_buf = NULL; }
    if (load_rc != 0) {
        if (jit_err[0] != '\0') fprintf(stderr, "NV PTX JIT error log:\n%s\n", jit_err);
        check_cu(load_rc, "cuModuleLoadDataEx");
        return -1;
    }

    int aot_tier = 0;
    memset(out_meta, 0, 16U * sizeof(uint64_t));
    if (query_aot_meta) {
        void *fn_meta = NULL;
        check_cu(cuModuleGetFunction(&fn_meta, mod, "zcc_aot_model_meta"), "get(zcc_aot_model_meta)");
        uint64_t d_meta = 0ULL;
        if (!g_zcc_llm_failed) check_cu(cuMemAlloc_v2(&d_meta, 16U * 8U), "cuMemAlloc(aot_meta)");
        void *p_meta[1] = { &d_meta };
        if (!g_zcc_llm_failed) check_cu(cuLaunchKernel(fn_meta, 1U, 1U, 1U, 1U, 1U, 1U, 0U, NULL, p_meta, NULL), "aot_model_meta");
        if (!g_zcc_llm_failed) check_cu(cuCtxSynchronize(), "aot_model_meta_sync");
        if (!g_zcc_llm_failed) check_cu(cuMemcpyDtoH_v2(out_meta, d_meta, 16U * 8U), "DtoH(aot_meta)");
        if (d_meta) check_cu(cuMemFree_v2(d_meta), "cuMemFree(aot_meta)");
        if (g_zcc_llm_failed) {
            cuModuleUnload(mod);
            return -1;
        }
        if ((out_meta[0] != ZCC_AOT_MAGIC_V1 && out_meta[0] != ZCC_AOT_MAGIC_V2) ||
            out_meta[14] == 0ULL || out_meta[14] > 1024ULL) {
            char msg[256];
            snprintf(msg, sizeof(msg), "AOT_PTX_ERROR: invalid zcc_aot_model_meta header (magic=0x%08x%08x block=%u)",
                     (unsigned int)(out_meta[0] >> 32), (unsigned int)(out_meta[0] & 0xFFFFFFFFU),
                     (unsigned int)out_meta[14]);
            fprintf(stderr, "%s\n", msg);
            zcc_llm_set_err(msg);
            cuModuleUnload(mod);
            return -1;
        }
        aot_tier = (out_meta[0] == ZCC_AOT_MAGIC_V2) ? 2 : 1;
    }

    *out_aot_tier = aot_tier;
    *out_mod = mod;
    *out_bytes = byte_cnt;
    return 0;
}

static int load_generic_kernels(void *mod) {
    check_cu(cuModuleGetFunction(&fn_embed_f32, mod, "llm_embed_f32"), "get(llm_embed_f32)");
    check_cu(cuModuleGetFunction(&fn_embed_f16, mod, "llm_embed_f16"), "get(llm_embed_f16)");
    check_cu(cuModuleGetFunction(&fn_embed_q4_0, mod, "llm_embed_q4_0"), "get(llm_embed_q4_0)");
    check_cu(cuModuleGetFunction(&fn_embed_q8_0, mod, "llm_embed_q8_0"), "get(llm_embed_q8_0)");
    check_cu(cuModuleGetFunction(&fn_embed_q4_k, mod, "llm_embed_q4_k"), "get(llm_embed_q4_k)");
    check_cu(cuModuleGetFunction(&fn_embed_q6_k, mod, "llm_embed_q6_k"), "get(llm_embed_q6_k)");
    check_cu(cuModuleGetFunction(&fn_embed_bf16, mod, "llm_embed_bf16"), "get(llm_embed_bf16)");
    check_cu(cuModuleGetFunction(&fn_rmsnorm, mod, "llm_rmsnorm_f32"), "get(llm_rmsnorm_f32)");
    check_cu(cuModuleGetFunction(&fn_f32_to_f16, mod, "llm_f32_to_f16"), "get(llm_f32_to_f16)");
    check_cu(cuModuleGetFunction(&fn_gemm_tc, mod, "llm_gemm_tensor_f16"), "get(llm_gemm_tensor_f16)");
    check_cu(cuModuleGetFunction(&fn_matvec_f32, mod, "llm_matvec_f32"), "get(llm_matvec_f32)");
    check_cu(cuModuleGetFunction(&fn_matvec_f16, mod, "llm_matvec_f16"), "get(llm_matvec_f16)");
    check_cu(cuModuleGetFunction(&fn_matvec_q4_0, mod, "llm_matvec_q4_0"), "get(llm_matvec_q4_0)");
    check_cu(cuModuleGetFunction(&fn_matvec_q8_0, mod, "llm_matvec_q8_0"), "get(llm_matvec_q8_0)");
    check_cu(cuModuleGetFunction(&fn_matvec_q4_k, mod, "llm_matvec_q4_k"), "get(llm_matvec_q4_k)");
    check_cu(cuModuleGetFunction(&fn_matvec_q6_k, mod, "llm_matvec_q6_k"), "get(llm_matvec_q6_k)");
    check_cu(cuModuleGetFunction(&fn_matvec_bf16, mod, "llm_matvec_bf16"), "get(llm_matvec_bf16)");
    check_cu(cuModuleGetFunction(&fn_rope, mod, "llm_rope_f32"), "get(llm_rope_f32)");
    check_cu(cuModuleGetFunction(&fn_rope_neox, mod, "llm_rope_neox_f32"), "get(llm_rope_neox_f32)");
    check_cu(cuModuleGetFunction(&fn_kv_store, mod, "llm_kv_store_f32"), "get(llm_kv_store_f32)");
    check_cu(cuModuleGetFunction(&fn_attn_gqa, mod, "llm_attn_gqa_f32"), "get(llm_attn_gqa_f32)");
    check_cu(cuModuleGetFunction(&fn_attn_flash, mod, "llm_attn_flash_gqa_f32"), "get(llm_attn_flash_gqa_f32)");
    check_cu(cuModuleGetFunction(&fn_swiglu, mod, "llm_swiglu_f32"), "get(llm_swiglu_f32)");
    check_cu(cuModuleGetFunction(&fn_add, mod, "llm_add_f32"), "get(llm_add_f32)");
    check_cu(cuModuleGetFunction(&fn_add_bias, mod, "llm_add_bias_f32"), "get(llm_add_bias_f32)");
    check_cu(cuModuleGetFunction(&fn_softmax, mod, "llm_softmax_f32"), "get(llm_softmax_f32)");
    check_cu(cuModuleGetFunction(&fn_kv_store_f16, mod, "llm_kv_store_f16"), "get(llm_kv_store_f16)");
    check_cu(cuModuleGetFunction(&fn_kv_store_q8_0, mod, "llm_kv_store_q8_0"), "get(llm_kv_store_q8_0)");
    check_cu(cuModuleGetFunction(&fn_attn_flash_f16, mod, "llm_attn_flash_gqa_f16"), "get(llm_attn_flash_gqa_f16)");
    check_cu(cuModuleGetFunction(&fn_attn_flash_q8_0, mod, "llm_attn_flash_gqa_q8_0"), "get(llm_attn_flash_gqa_q8_0)");
    check_cu(cuModuleGetFunction(&fn_dequant_f32_to_f16, mod, "llm_dequant_f32_to_f16"), "get(llm_dequant_f32_to_f16)");
    check_cu(cuModuleGetFunction(&fn_dequant_bf16_to_f16, mod, "llm_dequant_bf16_to_f16"), "get(llm_dequant_bf16_to_f16)");
    check_cu(cuModuleGetFunction(&fn_dequant_q8_0_to_f16, mod, "llm_dequant_q8_0_to_f16"), "get(llm_dequant_q8_0_to_f16)");
    check_cu(cuModuleGetFunction(&fn_dequant_q4_0_to_f16, mod, "llm_dequant_q4_0_to_f16"), "get(llm_dequant_q4_0_to_f16)");
    check_cu(cuModuleGetFunction(&fn_dequant_q4_k_to_f16, mod, "llm_dequant_q4_k_to_f16"), "get(llm_dequant_q4_k_to_f16)");
    check_cu(cuModuleGetFunction(&fn_dequant_q6_k_to_f16, mod, "llm_dequant_q6_k_to_f16"), "get(llm_dequant_q6_k_to_f16)");
    check_cu(cuModuleGetFunction(&fn_scatter_tile, mod, "llm_scatter_tile_f32"), "get(llm_scatter_tile_f32)");
    check_cu(cuModuleGetFunction(&fn_rmsnorm_batch, mod, "llm_rmsnorm_batch_f32"), "get(llm_rmsnorm_batch_f32)");
    check_cu(cuModuleGetFunction(&fn_add_bias_batch, mod, "llm_add_bias_batch_f32"), "get(llm_add_bias_batch_f32)");
    check_cu(cuModuleGetFunction(&fn_rope_batch, mod, "llm_rope_batch_f32"), "get(llm_rope_batch_f32)");
    check_cu(cuModuleGetFunction(&fn_rope_neox_batch, mod, "llm_rope_neox_batch_f32"), "get(llm_rope_neox_batch_f32)");
    check_cu(cuModuleGetFunction(&fn_kv_store_batch_f32, mod, "llm_kv_store_batch_f32"), "get(llm_kv_store_batch_f32)");
    check_cu(cuModuleGetFunction(&fn_kv_store_batch_f16, mod, "llm_kv_store_batch_f16"), "get(llm_kv_store_batch_f16)");
    check_cu(cuModuleGetFunction(&fn_kv_store_batch_q8_0, mod, "llm_kv_store_batch_q8_0"), "get(llm_kv_store_batch_q8_0)");
    check_cu(cuModuleGetFunction(&fn_attn_flash_batch_f32, mod, "llm_attn_flash_gqa_batch_f32"), "get(llm_attn_flash_gqa_batch_f32)");
    check_cu(cuModuleGetFunction(&fn_attn_flash_batch_f16, mod, "llm_attn_flash_gqa_batch_f16"), "get(llm_attn_flash_gqa_batch_f16)");
    check_cu(cuModuleGetFunction(&fn_attn_flash_batch_q8_0, mod, "llm_attn_flash_gqa_batch_q8_0"), "get(llm_attn_flash_gqa_batch_q8_0)");
    return g_zcc_llm_failed ? -1 : 0;
}

static unsigned int run_decode_sequence_ex(
    int aot_tier,
    void *fn_aot_fwd,
    unsigned int aot_block_dim,
    McPlan *mc_plan,
    const LlamaModel *m,
    const GpuActivations *act,
    int use_tensor_core,
    int kv_type,
    int max_steps,
    unsigned int pos,
    unsigned int eff_max_seq,
    float temp,
    int top_k,
    float top_p,
    float min_p,
    float repeat_penalty,
    int ignore_eos,
    int stream_out,
    const char *stream_prefix,
    void (*stream_cb)(const char *, int, void *),
    void *stream_cb_ud,
    uint32_t *io_seed,
    float *h_logits,
    float *work_logits,
    float *h_probs,
    SampleCand *cands,
    uint32_t *hist,
    unsigned int *io_hist_len,
    int *gen_ids,
    char **io_gen_text,
    size_t *io_gen_cap,
    int *out_gen_count
) {
    int gen_count = 0;
    size_t gen_text_len = 0U;
    (*io_gen_text)[0] = '\0';
    if (stream_out && stream_prefix) { printf("%s", stream_prefix); fflush(stdout); }
    int fast_gpu_argmax = (aot_tier == 2 && mc_plan && mc_plan->has_argmax &&
                           temp <= 0.0f && repeat_penalty == 1.0f &&
                           act->argmax_ids && act->argmax_vals);
    int have_gpu_nxt = 0;
    uint32_t gpu_nxt_id = 0U;
    for (int s = 0; s < max_steps; s++) {
        if (g_zcc_llm_failed) break;
        int nxt = have_gpu_nxt
            ? (int)gpu_nxt_id
            : sample_next_token(h_logits, work_logits, h_probs, cands, m->vocab_size,
                                hist, *io_hist_len, temp, top_k, top_p, min_p, repeat_penalty, io_seed);
        if (is_stop_token(m, ignore_eos, (uint32_t)nxt)) break;
        record_generated_token_ex(m, (uint32_t)nxt, gen_ids, &gen_count, hist, io_hist_len,
                                  io_gen_text, io_gen_cap, &gen_text_len, stream_out,
                                  stream_cb, stream_cb_ud);
        unsigned int utok = (unsigned int)nxt;
        if (aot_tier == 1) {
            void *p_aot[6] = { &m->d_wblob, &act->k_cache, &act->v_cache, &act->logits, &utok, &pos };
            check_cu(cuLaunchKernel(fn_aot_fwd, 1U, 1U, 1U, aot_block_dim, 1U, 1U, 0U, NULL, p_aot, NULL), "aot_fwd");
        } else if (aot_tier == 2) {
            mc_plan_step(mc_plan, act->step_args, utok, pos, eff_max_seq);
        } else {
            gpu_forward_token(m, act, utok, pos, eff_max_seq, use_tensor_core, kv_type);
        }
        if (g_zcc_llm_failed) break;
        pos++;
        if (fast_gpu_argmax) {
            float top1_val = 0.0f;
            check_cu(cuMemcpyDtoH_v2(&gpu_nxt_id, act->argmax_ids, 4U), "DtoH_argmax_id");
            check_cu(cuMemcpyDtoH_v2(&top1_val, act->argmax_vals, 4U), "DtoH_argmax_val");
            if (g_zcc_llm_failed) break;
            if (!(top1_val >= -1e38f && top1_val <= 1e38f)) {
#ifdef ZCC_GPU_LLM_LIBRARY
                char msg[128];
                snprintf(msg, sizeof(msg), "NAN_LOGITS step=%d idx=%u", s, gpu_nxt_id);
                zcc_llm_set_err(msg);
                break;
#else
                fprintf(stderr, "NAN_LOGITS step=%d idx=%u\n", s, gpu_nxt_id);
                exit(1);
#endif
            }
            have_gpu_nxt = 1;
        } else {
            check_cu(cuMemcpyDtoH_v2(h_logits, act->logits, (size_t)m->vocab_size * 4U), "DtoH_step_logits");
            if (g_zcc_llm_failed) break;
            check_finite_logits(h_logits, m->vocab_size, s);
            if (g_zcc_llm_failed) break;
        }
    }
    if (stream_out) { putchar('\n'); fflush(stdout); }
    *out_gen_count = gen_count;
    return pos;
}

static unsigned int run_decode_sequence(
    int aot_tier,
    void *fn_aot_fwd,
    unsigned int aot_block_dim,
    McPlan *mc_plan,
    const LlamaModel *m,
    const GpuActivations *act,
    int use_tensor_core,
    int kv_type,
    int max_steps,
    unsigned int pos,
    unsigned int eff_max_seq,
    float temp,
    int top_k,
    float top_p,
    float min_p,
    float repeat_penalty,
    int ignore_eos,
    int stream_out,
    const char *stream_prefix,
    uint32_t *io_seed,
    float *h_logits,
    float *work_logits,
    float *h_probs,
    SampleCand *cands,
    uint32_t *hist,
    unsigned int *io_hist_len,
    int *gen_ids,
    char **io_gen_text,
    size_t *io_gen_cap,
    int *out_gen_count
) {
    return run_decode_sequence_ex(aot_tier, fn_aot_fwd, aot_block_dim, mc_plan, m, act,
                                  use_tensor_core, kv_type, max_steps, pos, eff_max_seq,
                                  temp, top_k, top_p, min_p, repeat_penalty, ignore_eos,
                                  stream_out, stream_prefix, NULL, NULL,
                                  io_seed, h_logits, work_logits, h_probs, cands,
                                  hist, io_hist_len, gen_ids, io_gen_text, io_gen_cap, out_gen_count);
}

typedef struct {
    McPlan *target_plan;
    McVerifyPlan *verify_plans;
    McPrefillFns *target_pf;
    void *target_mod;
    const LlamaModel *target;
    const GpuActivations *act_t;
    McPlan *draft_plan;
    McPlan *draft_chain_plan;
    const LlamaModel *draft;
    const GpuActivations *act_d;
    int no_cuda_graph;
    int kv_type;
    int draft_k;
    int adaptive_k;
    int max_steps;
    unsigned int eff_max_seq;
    float temp;
    int top_k;
    float top_p;
    float min_p;
    float repeat_penalty;
    int ignore_eos;
    int stream_out;
    const char *stream_prefix;
    uint32_t *io_seed;
    float *h_logits;
    float *work_logits;
    float *h_probs;
    SampleCand *cands;
    uint32_t *hist;
    unsigned int *io_hist_len;
    int *gen_ids;
    char **io_gen_text;
    size_t *io_gen_cap;
    SpecStats *stats;
    int *io_draft_pending_catchup;
    uint32_t *io_draft_catchup_tok;
    int keep_kv_tail;
} SpecCtx;

static unsigned int run_speculative_sequence(SpecCtx *sc, unsigned int pos, int *out_gen_count) {
    int gen_count = 0;
    size_t gen_text_len = 0U;
    (*sc->io_gen_text)[0] = '\0';
    if (sc->stream_out && sc->stream_prefix) { printf("%s", sc->stream_prefix); fflush(stdout); }
    if (sc->max_steps <= 0) {
        sc->stats->final_k = (unsigned int)sc->draft_k;
        if (sc->stream_out) { putchar('\n'); fflush(stdout); }
        *out_gen_count = 0;
        return pos;
    }

    const LlamaModel *target = sc->target;
    const LlamaModel *draft = sc->draft;
    unsigned int V_t = target->vocab_size;
    unsigned int V_d = draft->vocab_size;

    int g0 = sample_next_token(sc->h_logits, sc->work_logits, sc->h_probs, sc->cands, V_t,
                               sc->hist, *sc->io_hist_len, sc->temp, sc->top_k, sc->top_p,
                               sc->min_p, sc->repeat_penalty, sc->io_seed);
    if (is_stop_token(target, sc->ignore_eos, (uint32_t)g0)) {
        sc->stats->final_k = (unsigned int)sc->draft_k;
        if (sc->stream_out) { putchar('\n'); fflush(stdout); }
        *out_gen_count = 0;
        return pos;
    }
    record_generated_token(target, (uint32_t)g0, sc->gen_ids, &gen_count, sc->hist, sc->io_hist_len,
                           sc->io_gen_text, sc->io_gen_cap, &gen_text_len, sc->stream_out);
    sc->stats->target_tokens++;

    int greedy_fast = (sc->temp <= 0.0f && sc->repeat_penalty == 1.0f &&
                       sc->target_plan->has_argmax && sc->act_t->argmax_ids &&
                       sc->act_t->argmax_vals && sc->draft_chain_plan->has_draft_chain);
    float *h_q_probs = NULL, *h_v_logits = NULL, *p_probs = NULL, *res_probs = NULL;
    if (!greedy_fast) {
        h_q_probs = (float *)malloc((size_t)sc->draft_k * (size_t)V_d * sizeof(float));
        h_v_logits = (float *)malloc((size_t)(sc->draft_k + 1) * (size_t)V_t * sizeof(float));
        p_probs = (float *)malloc((size_t)V_t * sizeof(float));
        res_probs = (float *)malloc((size_t)V_t * sizeof(float));
        if (!h_q_probs || !h_v_logits || !p_probs || !res_probs) {
            fprintf(stderr, "FATAL: OOM for speculative sampling buffers\n");
            exit(1);
        }
    }

    uint32_t cur_tok = (uint32_t)g0;
    int hit_eos = 0;
    int k_cur = sc->draft_k;
    sc->stats->final_k = (unsigned int)k_cur;

    while (gen_count < sc->max_steps && pos < sc->eff_max_seq) {
        if (*sc->io_draft_pending_catchup) {
            uint32_t ct = (*sc->io_draft_catchup_tok < V_d) ? *sc->io_draft_catchup_tok : 0U;
            mc_plan_step(sc->draft_plan, sc->act_d->step_args, ct, pos - 1U, sc->eff_max_seq);
            *sc->io_draft_pending_catchup = 0;
        }

        int rem_steps = sc->max_steps - gen_count;
        int rem_ctx = (sc->eff_max_seq > pos + 1U) ? (int)(sc->eff_max_seq - pos - 1U) : 0;
        int k_step = k_cur;
        if (k_step > rem_steps - 1) k_step = rem_steps - 1;
        if (k_step > rem_ctx) k_step = rem_ctx;
        if (k_step > 7) k_step = 7;

        if (k_step < 1) {
            uint32_t cur_tok_t = (cur_tok < V_t) ? cur_tok : 0U;
            uint32_t cur_tok_d = (cur_tok < V_d) ? cur_tok : 0U;
            mc_plan_step(sc->target_plan, sc->act_t->step_args, cur_tok_t, pos, sc->eff_max_seq);
            mc_plan_step(sc->draft_plan, sc->act_d->step_args, cur_tok_d, pos, sc->eff_max_seq);
            pos++;
            int nxt = 0;
            if (greedy_fast) {
                uint32_t tid = 0U;
                float tval = 0.0f;
                check_cu(cuMemcpyDtoH_v2(&tid, sc->act_t->argmax_ids, 4U), "DtoH_fb_id");
                check_cu(cuMemcpyDtoH_v2(&tval, sc->act_t->argmax_vals, 4U), "DtoH_fb_val");
                if (!(tval >= -1e38f && tval <= 1e38f)) {
                    fprintf(stderr, "NAN_LOGITS step=%d idx=%u\n", gen_count, tid);
                    exit(1);
                }
                nxt = (int)tid;
            } else {
                check_cu(cuMemcpyDtoH_v2(sc->h_logits, sc->act_t->logits, (size_t)V_t * 4U), "DtoH_fb_logits");
                check_finite_logits(sc->h_logits, V_t, gen_count);
                nxt = sample_next_token(sc->h_logits, sc->work_logits, sc->h_probs, sc->cands, V_t,
                                        sc->hist, *sc->io_hist_len, sc->temp, sc->top_k, sc->top_p,
                                        sc->min_p, sc->repeat_penalty, sc->io_seed);
            }
            if (is_stop_token(target, sc->ignore_eos, (uint32_t)nxt)) { hit_eos = 1; break; }
            record_generated_token(target, (uint32_t)nxt, sc->gen_ids, &gen_count, sc->hist, sc->io_hist_len,
                                   sc->io_gen_text, sc->io_gen_cap, &gen_text_len, sc->stream_out);
            sc->stats->target_tokens++;
            cur_tok = (uint32_t)nxt;
            continue;
        }

        uint32_t draft_toks[8];
        uint32_t cur_tok_d = (cur_tok < V_d) ? cur_tok : 0U;
        if (greedy_fast) {
            uint32_t sa[4];
            sa[0] = cur_tok_d; sa[1] = pos; sa[2] = sc->eff_max_seq; sa[3] = 0U;
            check_cu(cuMemcpyHtoD_v2(sc->act_d->step_args, sa, 16U), "HtoD(draft_chain_sa)");
            for (int i = 0; i < k_step; i++) mc_plan_launch(sc->draft_chain_plan);
            check_cu(cuMemcpyDtoH_v2(draft_toks, sc->act_d->draft_ids, (size_t)k_step * 4U), "DtoH(draft_ids)");
            for (int i = 0; i < k_step; i++) {
                if (draft_toks[i] == 0xFFFFFFFFU || draft_toks[i] >= V_d) {
                    fprintf(stderr, "NAN_LOGITS step=%d idx=%u\n", gen_count + i, draft_toks[i]);
                    exit(1);
                }
            }
        } else {
            uint32_t win_hist[72];
            unsigned int hl = *sc->io_hist_len;
            unsigned int tail = (hl > 64U) ? 64U : hl;
            if (tail > 0U) memcpy(win_hist, sc->hist + (hl - tail), (size_t)tail * sizeof(uint32_t));
            unsigned int wh_len = tail;
            uint32_t d_in = cur_tok_d;
            for (int i = 0; i < k_step; i++) {
                mc_plan_step(sc->draft_plan, sc->act_d->step_args, d_in, pos + (unsigned int)i, sc->eff_max_seq);
                check_cu(cuMemcpyDtoH_v2(sc->h_logits, sc->act_d->logits, (size_t)V_d * 4U), "DtoH(draft_logits)");
                check_finite_logits(sc->h_logits, V_d, gen_count + i);
                float *qi = h_q_probs + (size_t)i * V_d;
                fill_filtered_probs(sc->h_logits, sc->work_logits, sc->h_probs, sc->cands, V_d,
                                    win_hist, wh_len, sc->temp, sc->top_k, sc->top_p, sc->min_p, sc->repeat_penalty, qi);
                draft_toks[i] = (uint32_t)sample_from_prob_vec(qi, V_d, sc->io_seed);
                if (wh_len < 72U) win_hist[wh_len++] = draft_toks[i];
                d_in = draft_toks[i];
            }
        }

        unsigned int M = (unsigned int)k_step + 1U;
        if (!sc->verify_plans[M].ready) {
            mc_verify_plan_init(&sc->verify_plans[M], sc->target_mod, sc->target_plan, sc->target_pf,
                                target, sc->act_t, M, sc->no_cuda_graph, sc->kv_type);
        }
        uint32_t v_toks[8];
        v_toks[0] = (cur_tok < V_t) ? cur_tok : 0U;
        for (int i = 0; i < k_step; i++) v_toks[i + 1] = (draft_toks[i] < V_t) ? draft_toks[i] : 0U;
        mc_verify_plan_step(&sc->verify_plans[M], sc->act_t, v_toks, M, pos, sc->eff_max_seq);
        sc->stats->verify_launches++;
        sc->stats->cycles++;
        sc->stats->drafts_proposed += (unsigned int)k_step;

        unsigned int accepted = 0U;
        uint32_t nxt = 0U;
        if (greedy_fast) {
            uint32_t v_ids[8];
            float v_vals[8];
            check_cu(cuMemcpyDtoH_v2(v_ids, sc->act_t->argmax_ids, (size_t)M * 4U), "DtoH(v_ids)");
            check_cu(cuMemcpyDtoH_v2(v_vals, sc->act_t->argmax_vals, (size_t)M * 4U), "DtoH(v_vals)");
            for (unsigned int j = 0U; j < M; j++) {
                if (!(v_vals[j] >= -1e38f && v_vals[j] <= 1e38f)) {
                    fprintf(stderr, "NAN_LOGITS step=%d idx=%u\n", gen_count + (int)j, v_ids[j]);
                    exit(1);
                }
            }
            for (int i = 0; i < k_step; i++) {
                if (draft_toks[i] < V_t && v_ids[i] == draft_toks[i]) {
                    if (is_stop_token(target, sc->ignore_eos, draft_toks[i])) {
                        pos += (unsigned int)i + 1U;
                        sc->stats->drafts_accepted += (unsigned int)i;
                        hit_eos = 1;
                        break;
                    }
                    record_generated_token(target, draft_toks[i], sc->gen_ids, &gen_count, sc->hist, sc->io_hist_len,
                                           sc->io_gen_text, sc->io_gen_cap, &gen_text_len, sc->stream_out);
                    sc->stats->target_tokens++;
                    accepted++;
                } else {
                    break;
                }
            }
            if (!hit_eos) nxt = v_ids[accepted];
        } else {
            check_cu(cuMemcpyDtoH_v2(h_v_logits, sc->act_t->logits, (size_t)M * (size_t)V_t * 4U), "DtoH(v_logits)");
            for (unsigned int j = 0U; j < M; j++) {
                check_finite_logits(h_v_logits + (size_t)j * V_t, V_t, gen_count + (int)j);
            }
            int rejected = 0;
            for (int i = 0; i < k_step; i++) {
                fill_filtered_probs(h_v_logits + (size_t)i * V_t, sc->work_logits, sc->h_probs, sc->cands, V_t,
                                    sc->hist, *sc->io_hist_len, sc->temp, sc->top_k, sc->top_p,
                                    sc->min_p, sc->repeat_penalty, p_probs);
                *sc->io_seed = (*sc->io_seed) * 1664525U + 1013904223U;
                double u = ((double)(((*sc->io_seed) >> 8) & 0x00FFFFFFU) / 16777216.0);
                float pd = (draft_toks[i] < V_t) ? p_probs[draft_toks[i]] : 0.0f;
                float qd = h_q_probs[(size_t)i * V_d + draft_toks[i]];
                if (draft_toks[i] < V_t && (pd >= qd || u * (double)qd <= (double)pd)) {
                    if (is_stop_token(target, sc->ignore_eos, draft_toks[i])) {
                        pos += (unsigned int)i + 1U;
                        sc->stats->drafts_accepted += (unsigned int)i;
                        hit_eos = 1;
                        break;
                    }
                    record_generated_token(target, draft_toks[i], sc->gen_ids, &gen_count, sc->hist, sc->io_hist_len,
                                           sc->io_gen_text, sc->io_gen_cap, &gen_text_len, sc->stream_out);
                    sc->stats->target_tokens++;
                    accepted++;
                } else {
                    const float *qi = h_q_probs + (size_t)i * V_d;
                    double rsum = 0.0;
                    for (unsigned int v = 0U; v < V_t; v++) {
                        float qv = (v < V_d) ? qi[v] : 0.0f;
                        float rv = (p_probs[v] > qv) ? (p_probs[v] - qv) : 0.0f;
                        res_probs[v] = rv;
                        rsum += (double)rv;
                    }
                    nxt = (uint32_t)sample_from_prob_vec((rsum > 0.0) ? res_probs : p_probs, V_t, sc->io_seed);
                    rejected = 1;
                    break;
                }
            }
            if (!hit_eos && !rejected) {
                fill_filtered_probs(h_v_logits + (size_t)k_step * V_t, sc->work_logits, sc->h_probs, sc->cands, V_t,
                                    sc->hist, *sc->io_hist_len, sc->temp, sc->top_k, sc->top_p,
                                    sc->min_p, sc->repeat_penalty, p_probs);
                nxt = (uint32_t)sample_from_prob_vec(p_probs, V_t, sc->io_seed);
            }
        }

        if (!hit_eos) {
            pos += accepted + 1U;
            sc->stats->drafts_accepted += accepted;
            if ((int)accepted == k_step) {
                *sc->io_draft_pending_catchup = 1;
                *sc->io_draft_catchup_tok = draft_toks[k_step - 1];
            } else {
                *sc->io_draft_pending_catchup = 0;
            }
            if (is_stop_token(target, sc->ignore_eos, nxt)) {
                hit_eos = 1;
            } else {
                record_generated_token(target, nxt, sc->gen_ids, &gen_count, sc->hist, sc->io_hist_len,
                                       sc->io_gen_text, sc->io_gen_cap, &gen_text_len, sc->stream_out);
                sc->stats->target_tokens++;
                cur_tok = nxt;
            }
        }

        if (sc->adaptive_k) {
            if ((int)accepted == k_step && k_cur < sc->draft_k) k_cur++;
            else if (accepted * 2U < (unsigned int)k_step && k_cur > 1) k_cur--;
        }
        sc->stats->final_k = (unsigned int)k_cur;
        if (hit_eos) break;
    }

    if (sc->keep_kv_tail) {
        if (*sc->io_draft_pending_catchup) {
            uint32_t ct = (*sc->io_draft_catchup_tok < V_d) ? *sc->io_draft_catchup_tok : 0U;
            mc_plan_step(sc->draft_plan, sc->act_d->step_args, ct, pos - 1U, sc->eff_max_seq);
            *sc->io_draft_pending_catchup = 0;
        }
        if (!hit_eos && pos < sc->eff_max_seq) {
            uint32_t cur_tok_t = (cur_tok < V_t) ? cur_tok : 0U;
            uint32_t cur_tok_d = (cur_tok < V_d) ? cur_tok : 0U;
            mc_plan_step(sc->target_plan, sc->act_t->step_args, cur_tok_t, pos, sc->eff_max_seq);
            mc_plan_step(sc->draft_plan, sc->act_d->step_args, cur_tok_d, pos, sc->eff_max_seq);
            pos++;
        }
    }

    if (sc->stream_out) { putchar('\n'); fflush(stdout); }
    if (h_q_probs) free(h_q_probs);
    if (h_v_logits) free(h_v_logits);
    if (p_probs) free(p_probs);
    if (res_probs) free(res_probs);
    *out_gen_count = gen_count;
    return pos;
}

static unsigned int run_prefill_active(
    int aot_tier,
    void *fn_aot_fwd,
    unsigned int aot_block_dim,
    void *mod,
    McPlan *mc_plan,
    McPrefillFns *mc_pf,
    const LlamaModel *model,
    const GpuActivations *act,
    int use_spec,
    void *draft_mod,
    unsigned int draft_block_dim,
    McPlan *draft_plan,
    McPrefillFns *draft_pf,
    const LlamaModel *draft,
    const GpuActivations *act_d,
    const uint32_t *toks,
    unsigned int n_toks,
    unsigned int start_pos,
    unsigned int eff_max_seq,
    int use_tensor_core,
    int kv_type,
    int prefill_batch,
    unsigned int *out_chunks,
    int *out_tc_used
) {
    if (use_spec) {
        run_prefill_tokens(2, NULL, draft_block_dim, draft_mod, draft_plan, draft_pf,
                           draft, act_d, toks, n_toks, start_pos, eff_max_seq,
                           use_tensor_core, kv_type, prefill_batch, NULL, NULL);
    }
    return run_prefill_tokens(aot_tier, fn_aot_fwd, aot_block_dim, mod, mc_plan, mc_pf,
                              model, act, toks, n_toks, start_pos, eff_max_seq,
                              use_tensor_core, kv_type, prefill_batch, out_chunks, out_tc_used);
}

#ifndef ZCC_GPU_LLM_LIBRARY

int main(int argc, char **argv) {
    const char *model_path = "build_win64\\zcc_tiny_llama_f16.gguf";
    int user_gave_model = 0;
    const char *aot_ptx_path = NULL;
    const char *draft_model_path = NULL;
    const char *draft_ptx_path = NULL;
    const char *prompt = "ZCC compiles ";
    int user_gave_prompt = 0;
    const char *system_prompt = NULL;
    const char *dump_logits_path = NULL;
    int steps = 48;
    int draft_k = 5;
    int spec_adaptive = 1;
    float temp = 0.0f;
    int top_k = 0;
    float top_p = 1.0f;
    float min_p = 0.0f;
    float repeat_penalty = 1.0f;
    int ctx_override = 0;
    int stream_out = 0;
    int chat_mode = 0;
    int no_bos = 0;
    int ignore_eos = 0;
    int no_cuda_graph = 0;
    int kv_type = ZCC_KV_F32;
    int prefill_batch = 1;
    int user_gave_prefill_batch = 0;
    int interactive = 0;
    int clean_mode = 0;
    uint32_t seed = 2026U;
    int use_tensor_core = 1;
    int bench_gemm = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "INVALID_ARG: missing value for --model\n"); return 1; }
            model_path = argv[++i]; user_gave_model = 1;
        } else if (strcmp(argv[i], "--aot-ptx") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "INVALID_ARG: missing value for --aot-ptx\n"); return 1; }
            aot_ptx_path = argv[++i];
        } else if (strcmp(argv[i], "--draft-model") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "INVALID_ARG: missing value for --draft-model\n"); return 1; }
            draft_model_path = argv[++i];
        } else if (strcmp(argv[i], "--draft-ptx") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "INVALID_ARG: missing value for --draft-ptx\n"); return 1; }
            draft_ptx_path = argv[++i];
        } else if (strcmp(argv[i], "--draft-k") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "INVALID_ARG: missing value for --draft-k\n"); return 1; }
            if (parse_int_arg(argv[++i], "--draft-k", 1, 7, &draft_k) != 0) return 1;
        } else if (strcmp(argv[i], "--spec-fixed-k") == 0) {
            spec_adaptive = 0;
        } else if (strcmp(argv[i], "--prompt") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "INVALID_ARG: missing value for --prompt\n"); return 1; }
            prompt = argv[++i]; user_gave_prompt = 1;
        } else if (strcmp(argv[i], "--system") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "INVALID_ARG: missing value for --system\n"); return 1; }
            system_prompt = argv[++i];
        } else if (strcmp(argv[i], "--kv-type") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "INVALID_ARG: missing value for --kv-type\n"); return 1; }
            const char *kt = argv[++i];
            if (strcmp(kt, "f32") == 0) kv_type = ZCC_KV_F32;
            else if (strcmp(kt, "f16") == 0) kv_type = ZCC_KV_F16;
            else if (strcmp(kt, "q8_0") == 0) kv_type = ZCC_KV_Q8_0;
            else { fprintf(stderr, "INVALID_ARG: --kv-type must be f32, f16, or q8_0\n"); return 1; }
        } else if (strcmp(argv[i], "--prefill-batch") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "INVALID_ARG: missing value for --prefill-batch\n"); return 1; }
            if (parse_int_arg(argv[++i], "--prefill-batch", 1, 128, &prefill_batch) != 0) return 1;
            user_gave_prefill_batch = 1;
        } else if (strcmp(argv[i], "--steps") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "INVALID_ARG: missing value for --steps\n"); return 1; }
            if (parse_int_arg(argv[++i], "--steps", 0, 1048576, &steps) != 0) return 1;
        } else if (strcmp(argv[i], "--temp") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "INVALID_ARG: missing value for --temp\n"); return 1; }
            if (parse_finite_float_arg(argv[++i], "--temp", 0.0f, 1, 1000000.0f, 1, &temp) != 0) return 1;
        } else if (strcmp(argv[i], "--top-k") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "INVALID_ARG: missing value for --top-k\n"); return 1; }
            if (parse_int_arg(argv[++i], "--top-k", 0, 1048576, &top_k) != 0) return 1;
        } else if (strcmp(argv[i], "--top-p") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "INVALID_ARG: missing value for --top-p\n"); return 1; }
            if (parse_finite_float_arg(argv[++i], "--top-p", 0.0f, 0, 1.0f, 1, &top_p) != 0) return 1;
        } else if (strcmp(argv[i], "--min-p") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "INVALID_ARG: missing value for --min-p\n"); return 1; }
            if (parse_finite_float_arg(argv[++i], "--min-p", 0.0f, 1, 1.0f, 1, &min_p) != 0) return 1;
        } else if (strcmp(argv[i], "--repeat-penalty") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "INVALID_ARG: missing value for --repeat-penalty\n"); return 1; }
            if (parse_finite_float_arg(argv[++i], "--repeat-penalty", 0.0f, 0, 1000000.0f, 1, &repeat_penalty) != 0) return 1;
        } else if (strcmp(argv[i], "--ctx") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "INVALID_ARG: missing value for --ctx\n"); return 1; }
            if (parse_int_arg(argv[++i], "--ctx", 1, 1048576, &ctx_override) != 0) return 1;
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) seed = (uint32_t)atoi(argv[++i]);
        else if (strcmp(argv[i], "--dump-logits") == 0 && i + 1 < argc) dump_logits_path = argv[++i];
        else if (strcmp(argv[i], "--no-tensor-core") == 0) use_tensor_core = 0;
        else if (strcmp(argv[i], "--bench-gemm") == 0) bench_gemm = 1;
        else if (strcmp(argv[i], "--stream") == 0) stream_out = 1;
        else if (strcmp(argv[i], "--chat") == 0) chat_mode = 1;
        else if (strcmp(argv[i], "--no-bos") == 0) no_bos = 1;
        else if (strcmp(argv[i], "--ignore-eos") == 0) ignore_eos = 1;
        else if (strcmp(argv[i], "--no-cuda-graph") == 0) no_cuda_graph = 1;
        else if (strcmp(argv[i], "--interactive") == 0) interactive = 1;
        else if (strcmp(argv[i], "--clean") == 0) { clean_mode = 1; stream_out = 1; }
    }

    if ((draft_model_path != NULL) != (draft_ptx_path != NULL)) {
        fprintf(stderr, "INVALID_ARG: --draft-model and --draft-ptx must be specified together\n");
        return 1;
    }
    int use_spec = (draft_model_path != NULL);

    int use_embedded_aot = 0;
#ifdef ZCC_EMBEDDED_AOT_PTX
    if (!aot_ptx_path) use_embedded_aot = 1;
#endif
    int use_embedded_gguf = 0;
#ifdef ZCC_EMBEDDED_GGUF
    if (!user_gave_model) use_embedded_gguf = 1;
#endif
    int use_sealed_overlay = 0;
#ifdef ZCC_SEALED_OVERLAY_GGUF
    if (!user_gave_model && !use_embedded_gguf) use_sealed_overlay = 1;
    if (argc == 1) {
        interactive = 1;
        chat_mode = 1;
        clean_mode = 1;
        stream_out = 1;
        steps = 256;
    }
#endif
    int use_aot = (aot_ptx_path != NULL) || use_embedded_aot;

    if (use_aot && bench_gemm) {
        fprintf(stderr, "INVALID_ARG: --bench-gemm is not supported in --aot-ptx mode\n");
        return 1;
    }
    if (use_spec && (!use_aot || bench_gemm)) {
        fprintf(stderr, "SPEC_REQUIRES_TIER_B: speculative decoding requires --aot-ptx Tier B and cannot use --bench-gemm\n");
        return 1;
    }

    check_cu(cuInit(0U), "cuInit");
    int drv_ver = 0, dev_cnt = 0, dev = 0;
    check_cu(cuDriverGetVersion(&drv_ver), "cuDriverGetVersion");
    check_cu(cuDeviceGetCount(&dev_cnt), "cuDeviceGetCount");
    if (dev_cnt <= 0) { fprintf(stderr, "No CUDA device detected\n"); return 1; }
    check_cu(cuDeviceGet(&dev, 0), "cuDeviceGet");
    char dev_name[256]; memset(dev_name, 0, sizeof(dev_name));
    check_cu(cuDeviceGetName(dev_name, (int)sizeof(dev_name) - 1, dev), "cuDeviceGetName");
    int cc_maj = 0, cc_min = 0;
    check_cu(cuDeviceGetAttribute(&cc_maj, 75, dev), "cuDeviceGetAttribute(CC_MAJOR)");
    check_cu(cuDeviceGetAttribute(&cc_min, 76, dev), "cuDeviceGetAttribute(CC_MINOR)");
    size_t vram_bytes = 0U;
    check_cu(cuDeviceTotalMem_v2(&vram_bytes, dev), "cuDeviceTotalMem_v2");

    void *ctx = NULL;
    check_cu(cuCtxCreate_v2(&ctx, 8U, dev), "cuCtxCreate_v2");

    const void *ptx_fallback = NULL;
    unsigned int ptx_fallback_bytes = 0U;
#ifdef ZCC_EMBEDDED_AOT_PTX
    ptx_fallback = (const void *)g_embedded_aot_ptx;
    ptx_fallback_bytes = (unsigned int)(sizeof(g_embedded_aot_ptx) - 1U);
#else
    ptx_fallback = (const void *)g_gpu_llm_kernels_ptx;
    ptx_fallback_bytes = (unsigned int)(sizeof(g_gpu_llm_kernels_ptx) - 1U);
#endif

    void *mod = NULL;
    void *fn_aot_fwd = NULL;
    uint64_t aot_meta[16];
    unsigned int ptx_byte_count = 0U;
    unsigned int aot_block_dim = 128U;
    int aot_tier = 0;

    if (load_ptx_module(aot_ptx_path, ptx_fallback, ptx_fallback_bytes, use_aot,
                        &aot_tier, &mod, aot_meta, &ptx_byte_count) != 0) {
        cuCtxDestroy_v2(ctx);
        return 1;
    }

    if (use_aot) {
        aot_block_dim = (unsigned int)aot_meta[14];
        if (aot_tier == 1) {
            if (use_spec) {
                fprintf(stderr, "SPEC_REQUIRES_TIER_B: speculative decoding requires Tier B AOT PTX for target model\n");
                cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
                return 1;
            }
            check_cu(cuModuleGetFunction(&fn_aot_fwd, mod, "zcc_aot_forward_token"), "get(zcc_aot_forward_token)");
        }
    } else {
        if (load_generic_kernels(mod) != 0) {
            cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
            return 1;
        }
    }

    void *draft_mod = NULL;
    uint64_t draft_meta[16];
    memset(draft_meta, 0, sizeof(draft_meta));
    unsigned int draft_ptx_bytes = 0U, draft_block_dim = 128U;
    int draft_aot_tier = 0;
    if (use_spec) {
        if (load_ptx_module(draft_ptx_path, NULL, 0U, 1, &draft_aot_tier, &draft_mod, draft_meta, &draft_ptx_bytes) != 0) {
            cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
            return 1;
        }
        if (draft_aot_tier != 2) {
            fprintf(stderr, "SPEC_REQUIRES_TIER_B: speculative decoding requires Tier B AOT PTX for draft model\n");
            cuModuleUnload(draft_mod); cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
            return 1;
        }
        draft_block_dim = (unsigned int)draft_meta[14];
    }

    fprintf(clean_mode ? stderr : stdout, "GPU_LLM_DEVICE name=\"%s\" cc=%d.%d vram_mib=%u driver=%d ptx_bytes=%u\n",
            dev_name, cc_maj, cc_min, (unsigned int)(vram_bytes >> 20), drv_ver, ptx_byte_count);

    LlamaModel *model = NULL;
    LlamaModel *draft = NULL;
    if (use_spec) {
        draft = (LlamaModel *)calloc(1U, sizeof(LlamaModel));
        if (!draft || parse_and_stream_gguf(draft_model_path, NULL, 0U, draft, 1, draft_meta,
                                            1, 268435456ULL) != 0) {
            if (draft) { free_model_gpu_mem(draft); free_llama_model(draft); free(draft); }
            cuModuleUnload(draft_mod); cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
            return 1;
        }
        if ((kv_type == ZCC_KV_Q8_0 && ((draft->head_dim & 31U) != 0U || (draft->kv_dim & 31U) != 0U)) ||
            (kv_type != ZCC_KV_F32 && draft->head_dim > 256U) ||
            (draft->dim & 15U) != 0U || (draft->hidden_dim & 15U) != 0U || (draft->head_dim & 15U) != 0U) {
            if (kv_type == ZCC_KV_Q8_0 && ((draft->head_dim & 31U) != 0U || (draft->kv_dim & 31U) != 0U)) {
                fprintf(stderr, "INVALID_ARG: draft model head_dim (%u) incompatible with --kv-type q8_0\n", draft->head_dim);
            } else if ((draft->dim & 15U) != 0U || (draft->hidden_dim & 15U) != 0U || (draft->head_dim & 15U) != 0U) {
                fprintf(stderr, "SPEC_REQUIRES_DIM16: draft dimensions must be multiples of 16\n");
            } else {
                fprintf(stderr, "INVALID_ARG: draft head_dim (%u) exceeds 256 for --kv-type %s\n", draft->head_dim, kv_type_name_str(kv_type));
            }
            free_model_gpu_mem(draft);
            free_llama_model(draft); free(draft);
            cuModuleUnload(draft_mod); cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
            return 1;
        }
    }

    model = (LlamaModel *)calloc(1U, sizeof(LlamaModel));
    const uint8_t *emb_ptr = NULL;
    size_t emb_sz = 0U;
    uint64_t ov_off = 0ULL, ov_len = 0ULL;
    const uint8_t *ov_sha = NULL;
    int seal_verified = 0;
#ifdef ZCC_EMBEDDED_GGUF
    if (use_embedded_gguf) {
        emb_ptr = (const uint8_t *)g_embedded_gguf_u32;
        emb_sz = g_embedded_gguf_len;
    }
#endif
#ifdef ZCC_SEALED_OVERLAY_GGUF
    if (use_sealed_overlay) {
        char *pgm = NULL;
        const char *self_path = argv[0];
        if (_get_pgmptr(&pgm) == 0 && pgm && pgm[0]) self_path = pgm;
        if (probe_sealed_pe_overlay(self_path, g_sealed_gguf_len, g_sealed_gguf_sha256,
                                    &ov_off, &ov_len) != 0) {
            if (model) free(model);
            if (draft) { free_model_gpu_mem(draft); free_llama_model(draft); free(draft); }
            if (draft_mod) cuModuleUnload(draft_mod);
            cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
            return 1;
        }
        model_path = self_path;
        ov_sha = g_sealed_gguf_sha256;
    }
#endif
    if (!model || parse_and_stream_gguf_ex(model_path, emb_ptr, emb_sz, ov_off, ov_len, ov_sha,
                                           model, use_aot, aot_meta,
                                           (use_spec && aot_tier == 2) ? 1 : 0,
                                           use_spec ? 503316480ULL : 0ULL) != 0) {
        if (model) { free_model_gpu_mem(model); free_llama_model(model); free(model); }
        if (draft) { free_model_gpu_mem(draft); free_llama_model(draft); free(draft); }
        if (draft_mod) cuModuleUnload(draft_mod);
        cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
        return 1;
    }
    if (ov_sha != NULL) seal_verified = 1;

    if (kv_type != ZCC_KV_F32 && aot_tier == 1) {
        fprintf(stderr, "INVALID_ARG: --kv-type %s is not supported in Tier A aot_fused mode\n", kv_type_name_str(kv_type));
        if (draft) { free_model_gpu_mem(draft); free_llama_model(draft); free(draft); }
        free_model_gpu_mem(model);
        free_llama_model(model); free(model);
        if (draft_mod) cuModuleUnload(draft_mod);
        cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
        return 1;
    }
    if (kv_type == ZCC_KV_Q8_0 && ((model->head_dim & 31U) != 0U || (model->kv_dim & 31U) != 0U)) {
        fprintf(stderr, "INVALID_ARG: --kv-type q8_0 requires head_dim divisible by 32 (got %u)\n", model->head_dim);
        if (draft) { free_model_gpu_mem(draft); free_llama_model(draft); free(draft); }
        free_model_gpu_mem(model);
        free_llama_model(model); free(model);
        if (draft_mod) cuModuleUnload(draft_mod);
        cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
        return 1;
    }
    if (kv_type != ZCC_KV_F32 && model->head_dim > 256U) {
        fprintf(stderr, "INVALID_ARG: --kv-type %s requires head_dim <= 256 (got %u)\n", kv_type_name_str(kv_type), model->head_dim);
        if (draft) { free_model_gpu_mem(draft); free_llama_model(draft); free(draft); }
        free_model_gpu_mem(model);
        free_llama_model(model); free(model);
        if (draft_mod) cuModuleUnload(draft_mod);
        cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
        return 1;
    }

    if (use_spec) {
        if ((model->dim & 15U) != 0U || (model->hidden_dim & 15U) != 0U || (model->head_dim & 15U) != 0U) {
            fprintf(stderr, "SPEC_REQUIRES_DIM16: target dimensions must be multiples of 16\n");
            free_model_gpu_mem(draft);
            free_llama_model(draft); free(draft);
            free_model_gpu_mem(model);
            free_llama_model(model); free(model);
            cuModuleUnload(draft_mod); cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
            return 1;
        }
        if (spec_check_vocab_compat(draft, model) != 0) {
            free_model_gpu_mem(draft);
            free_llama_model(draft); free(draft);
            free_model_gpu_mem(model);
            free_llama_model(model); free(model);
            cuModuleUnload(draft_mod); cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
            return 1;
        }
    }

    unsigned int eff_max_seq = model->max_seq_len;
    if (ctx_override == 0 && draft && draft->max_seq_len < eff_max_seq) eff_max_seq = draft->max_seq_len;
    if (ctx_override > 0) {
        if (aot_tier == 1 && (unsigned int)ctx_override > model->max_seq_len) {
            fprintf(stderr, "INVALID_ARG: --ctx %d exceeds compiled Tier A max_seq_len %u\n", ctx_override, model->max_seq_len);
            if (draft) { free_model_gpu_mem(draft); free_llama_model(draft); free(draft); }
            free_model_gpu_mem(model);
            free_llama_model(model); free(model);
            if (draft_mod) cuModuleUnload(draft_mod);
            cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
            return 1;
        }
        eff_max_seq = (unsigned int)ctx_override;
    }

    uint64_t row_bytes_kv = (uint64_t)model->kv_dim * 4ULL;
    if (kv_type == ZCC_KV_F16) row_bytes_kv = (uint64_t)model->kv_dim * 2ULL;
    else if (kv_type == ZCC_KV_Q8_0) row_bytes_kv = (uint64_t)(model->kv_dim >> 5) * 34ULL;

    uint64_t row_bytes_kv_d = 0ULL;
    if (use_spec) {
        row_bytes_kv_d = (uint64_t)draft->kv_dim * 4ULL;
        if (kv_type == ZCC_KV_F16) row_bytes_kv_d = (uint64_t)draft->kv_dim * 2ULL;
        else if (kv_type == ZCC_KV_Q8_0) row_bytes_kv_d = (uint64_t)(draft->kv_dim >> 5) * 34ULL;
    }

    unsigned int max_dim = (draft && draft->dim > model->dim) ? draft->dim : model->dim;
    unsigned int max_hid = (draft && draft->hidden_dim > model->hidden_dim) ? draft->hidden_dim : model->hidden_dim;
    unsigned int max_kv_dim = (draft && draft->kv_dim > model->kv_dim) ? draft->kv_dim : model->kv_dim;
    unsigned int max_vocab = (draft && draft->vocab_size > model->vocab_size) ? draft->vocab_size : model->vocab_size;

    unsigned int act_rows = use_spec ? 8U : (unsigned int)prefill_batch;
    int alloc_w_f16_tmp = (!use_spec && ((aot_tier == 2 && prefill_batch > 8) || (aot_tier == 0 && prefill_batch > 1))) ? 1 : 0;

    size_t free_vram = 0U, total_vram = 0U;
    check_cu(cuMemGetInfo_v2(&free_vram, &total_vram), "cuMemGetInfo_v2");
    size_t max_spill_mat = 0U;
    for (unsigned int ti = 0U; ti < model->num_tensors; ti++) {
        const GgufTensor *gt = &model->tensors[ti];
        if (gt->on_host && strcmp(gt->name, "token_embd.weight") != 0 && (size_t)gt->nbytes > max_spill_mat) {
            max_spill_mat = (size_t)gt->nbytes;
        }
    }
    if (draft) {
        for (unsigned int ti = 0U; ti < draft->num_tensors; ti++) {
            const GgufTensor *gt = &draft->tensors[ti];
            if (gt->on_host && strcmp(gt->name, "token_embd.weight") != 0 && (size_t)gt->nbytes > max_spill_mat) {
                max_spill_mat = (size_t)gt->nbytes;
            }
        }
    }
    uint64_t pb_u64 = (uint64_t)act_rows;
    uint64_t ws_est = pb_u64 * ((uint64_t)max_dim * 24ULL + (uint64_t)max_hid * 12ULL + (uint64_t)max_kv_dim * 8ULL) +
                      (uint64_t)(use_spec ? 8ULL : 1ULL) * (uint64_t)max_vocab * 4ULL + 65536ULL +
                      (uint64_t)max_spill_mat;
    if (alloc_w_f16_tmp) {
        uint64_t max_mat = ((uint64_t)max_hid > (uint64_t)max_dim)
                           ? (uint64_t)max_hid * (uint64_t)max_dim
                           : (uint64_t)max_dim * (uint64_t)max_dim;
        ws_est += max_mat * 2ULL;
    }

    uint64_t kv_each64 = 0ULL, f32_each64 = 0ULL;
    uint64_t kv_each_d64 = 0ULL, f32_each_d64 = 0ULL;
    uint64_t kv_total64 = 0ULL, f32_total64 = 0ULL;
    for (;;) {
        int ov = (mul_u64_checked((uint64_t)model->n_layers, (uint64_t)eff_max_seq, &kv_each64) != 0 ||
                  mul_u64_checked(kv_each64, row_bytes_kv, &kv_each64) != 0 ||
                  mul_u64_checked((uint64_t)model->n_layers, (uint64_t)eff_max_seq, &f32_each64) != 0 ||
                  mul_u64_checked(f32_each64, (uint64_t)model->kv_dim * 4ULL, &f32_each64) != 0);
        if (!ov && use_spec) {
            ov = (mul_u64_checked((uint64_t)draft->n_layers, (uint64_t)eff_max_seq, &kv_each_d64) != 0 ||
                  mul_u64_checked(kv_each_d64, row_bytes_kv_d, &kv_each_d64) != 0 ||
                  mul_u64_checked((uint64_t)draft->n_layers, (uint64_t)eff_max_seq, &f32_each_d64) != 0 ||
                  mul_u64_checked(f32_each_d64, (uint64_t)draft->kv_dim * 4ULL, &f32_each_d64) != 0);
        }
        if (!ov) {
            kv_total64 = (kv_each64 + kv_each_d64) * 2ULL;
            f32_total64 = (f32_each64 + f32_each_d64) * 2ULL;
        }
        if (ctx_override == 0 && aot_tier != 1 && eff_max_seq > 256U && free_vram > 0U &&
            (ov || kv_total64 > (uint64_t)free_vram || kv_total64 + ws_est > ((uint64_t)free_vram * 85ULL) / 100ULL)) {
            eff_max_seq >>= 1;
            continue;
        }
        if (ov) {
            fprintf(stderr, "VRAM_BUDGET_EXCEEDED: KV cache byte size overflow\n");
            if (draft) { free_model_gpu_mem(draft); free_llama_model(draft); free(draft); }
            free_model_gpu_mem(model);
            free_llama_model(model); free(model);
            if (draft_mod) cuModuleUnload(draft_mod);
            cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
            return 1;
        }
        break;
    }
    unsigned int auto_ctx_orig = (draft && draft->max_seq_len < model->max_seq_len) ? draft->max_seq_len : model->max_seq_len;
    if (ctx_override == 0 && eff_max_seq != auto_ctx_orig) {
        fprintf(stderr, "AUTO_CTX: clamped context %u -> %u to fit free VRAM\n",
                auto_ctx_orig, eff_max_seq);
    }

    if (kv_total64 > (uint64_t)free_vram || kv_total64 + ws_est > (uint64_t)free_vram) {
        char req_s[32], free_s[32];
        fmt_u64_dec(kv_total64 + ws_est, req_s, sizeof(req_s));
        fmt_u64_dec((uint64_t)free_vram, free_s, sizeof(free_s));
        fprintf(stderr, "VRAM_BUDGET_EXCEEDED: required=%s free=%s\n", req_s, free_s);
        if (draft) { free_model_gpu_mem(draft); free_llama_model(draft); free(draft); }
        free_model_gpu_mem(model);
        free_llama_model(model); free(model);
        if (draft_mod) cuModuleUnload(draft_mod);
        cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
        return 1;
    }
    size_t kv_bytes = (size_t)kv_each64;
    size_t kv_bytes_d = (size_t)kv_each_d64;

    GpuActivations act, act_d;
    memset(&act, 0, sizeof(act));
    memset(&act_d, 0, sizeof(act_d));
    check_cu(cuMemAlloc_v2(&act.k_cache, kv_bytes), "act.k_cache");
    check_cu(cuMemAlloc_v2(&act.v_cache, kv_bytes), "act.v_cache");
    check_cu(cuMemAlloc_v2(&act.logits, (size_t)(use_spec ? 8U : 1U) * (size_t)max_vocab * 4U), "act.logits");
    if (aot_tier != 1) {
        unsigned int max_k = (max_hid > max_dim) ? max_hid : max_dim;
        size_t pb = (size_t)act_rows;
        check_cu(cuMemAlloc_v2(&act.x, pb * (size_t)max_dim * 4U), "act.x");
        check_cu(cuMemAlloc_v2(&act.xb, pb * (size_t)max_dim * 4U), "act.xb");
        check_cu(cuMemAlloc_v2(&act.xb2, pb * (size_t)max_dim * 4U), "act.xb2");
        check_cu(cuMemAlloc_v2(&act.x_f16, pb * (size_t)max_k * 2U), "act.x_f16");
        check_cu(cuMemAlloc_v2(&act.q, pb * (size_t)max_dim * 4U), "act.q");
        check_cu(cuMemAlloc_v2(&act.k, pb * (size_t)max_kv_dim * 4U), "act.k");
        check_cu(cuMemAlloc_v2(&act.v, pb * (size_t)max_kv_dim * 4U), "act.v");
        if (!use_aot && model->head_dim > 256U) {
            check_cu(cuMemAlloc_v2(&act.att_scratch, (size_t)model->n_heads * eff_max_seq * 4U), "act.att");
        }
        check_cu(cuMemAlloc_v2(&act.hb, pb * (size_t)max_hid * 4U), "act.hb");
        check_cu(cuMemAlloc_v2(&act.hb2, pb * (size_t)max_hid * 4U), "act.hb2");
        if (!use_aot) check_cu(cuMemAlloc_v2(&act.probs, (size_t)model->vocab_size * 4U), "act.probs");
        if (aot_tier == 2) {
            size_t am_bytes = (size_t)(use_spec ? 8U : 1U) * 4U;
            check_cu(cuMemAlloc_v2(&act.step_args, 16U), "act.step_args");
            check_cu(cuMemAlloc_v2(&act.argmax_ids, am_bytes), "act.argmax_ids");
            check_cu(cuMemAlloc_v2(&act.argmax_vals, am_bytes), "act.argmax_vals");
        }
        if (alloc_w_f16_tmp) {
            size_t max_mat = ((size_t)max_hid > (size_t)max_dim)
                             ? (size_t)max_hid * (size_t)max_dim
                             : (size_t)max_dim * (size_t)max_dim;
            check_cu(cuMemAlloc_v2(&act.w_f16_tmp, max_mat * 2U), "act.w_f16_tmp");
            if (aot_tier == 0) check_cu(cuMemAlloc_v2(&act.prefill_tmp, pb * (size_t)max_dim * 4U), "act.prefill_tmp");
        }
        if ((aot_tier == 2 && prefill_batch > 1) || use_spec) {
            check_cu(cuMemAlloc_v2(&act.batch_toks, pb * 4U), "act.batch_toks");
        }
    }
    if (max_spill_mat > 0U) {
        check_cu(cuMemAlloc_v2(&act.spill_stage[0], max_spill_mat), "act.spill_stage0");
        act.spill_stage_bytes = max_spill_mat;
        act.spill_slots = 1;
        if (kv_total64 + ws_est + (uint64_t)max_spill_mat + 33554432ULL <= (uint64_t)free_vram) {
            if (cuMemAlloc_v2(&act.spill_stage[1], max_spill_mat) == 0) {
                act.spill_slots = 2;
                check_cu(cuStreamCreate(&act.dma_stream, 1U), "cuStreamCreate(dma)");
                check_cu(cuEventCreate(&act.dma_ev[0], 2U), "dma_ev0");
                check_cu(cuEventCreate(&act.dma_ev[1], 2U), "dma_ev1");
                check_cu(cuEventCreate(&act.comp_ev[0], 2U), "comp_ev0");
                check_cu(cuEventCreate(&act.comp_ev[1], 2U), "comp_ev1");
            }
        }
    }
    if (use_spec) {
        check_cu(cuMemAlloc_v2(&act.draft_ids, 32U), "act.draft_ids");
        act_d = act;
        check_cu(cuMemAlloc_v2(&act_d.k_cache, kv_bytes_d), "act_d.k_cache");
        check_cu(cuMemAlloc_v2(&act_d.v_cache, kv_bytes_d), "act_d.v_cache");
        check_cu(cuMemAlloc_v2(&act_d.step_args, 16U), "act_d.step_args");
    }

    McPlan mc_plan, draft_plan, draft_chain_plan;
    McPrefillFns mc_pf, draft_pf;
    McVerifyPlan verify_plans[9];
    memset(&mc_plan, 0, sizeof(mc_plan));
    memset(&draft_plan, 0, sizeof(draft_plan));
    memset(&draft_chain_plan, 0, sizeof(draft_chain_plan));
    memset(&mc_pf, 0, sizeof(mc_pf));
    memset(&draft_pf, 0, sizeof(draft_pf));
    memset(verify_plans, 0, sizeof(verify_plans));
    if (aot_tier == 2) {
        mc_plan_init(&mc_plan, mod, model, &act, no_cuda_graph, kv_type, 1);
    }
    if (use_spec) {
        mc_plan_init(&draft_plan, draft_mod, draft, &act_d, no_cuda_graph, kv_type, 0);
        mc_plan_init(&draft_chain_plan, draft_mod, draft, &act_d, no_cuda_graph, kv_type, 2);
        for (int m_idx = 2; m_idx <= draft_k + 1 && m_idx <= 8; m_idx++) {
            mc_verify_plan_init(&verify_plans[m_idx], mod, &mc_plan, &mc_pf, model, &act, (unsigned int)m_idx, no_cuda_graph, kv_type);
        }
    }

    FILE *info_fp = clean_mode ? stderr : stdout;
    if (use_aot) {
        char wb_s[32];
        fmt_u64_dec(aot_meta[9], wb_s, sizeof(wb_s));
        unsigned int launches_pt = (aot_tier == 2 && no_cuda_graph) ? mc_plan.num_nodes : 1U;
        const char *ptx_lbl = aot_ptx_path ? aot_ptx_path : "<embedded>";
        if (aot_tier == 2) {
            fprintf(info_fp, "GPU_LLM_AOT_META ptx_path=\"%s\" fingerprint=%08x%08x smem_bytes=%u wblob_bytes=%s block_dim=%u launches_per_token=%u graph_nodes=%u\n",
                    ptx_lbl,
                    (unsigned int)(aot_meta[1] >> 32), (unsigned int)(aot_meta[1] & 0xFFFFFFFFU),
                    (unsigned int)aot_meta[11], wb_s, aot_block_dim, launches_pt, mc_plan.num_nodes);
        } else {
            fprintf(info_fp, "GPU_LLM_AOT_META ptx_path=\"%s\" fingerprint=%08x%08x smem_bytes=%u wblob_bytes=%s block_dim=%u launches_per_token=%u\n",
                    ptx_lbl,
                    (unsigned int)(aot_meta[1] >> 32), (unsigned int)(aot_meta[1] & 0xFFFFFFFFU),
                    (unsigned int)aot_meta[11], wb_s, aot_block_dim, (unsigned int)aot_meta[15]);
        }
    }

    const char *wtype_str = wtype_name_str(model->weight_type_primary);
    int tc_active = use_aot
        ? (int)aot_meta[12]
        : ((model->weight_type_primary == ZCC_GGML_F16 && use_tensor_core && (model->dim % 16U) == 0U) ? 1 : 0);
    const char *mode_str = "generic";
    if (use_embedded_gguf && use_aot) mode_str = "aot_embedded";
    else if (use_sealed_overlay && use_aot) mode_str = "aot_overlay";
    else if (aot_tier == 1) mode_str = "aot_fused";
    else if (aot_tier == 2) mode_str = no_cuda_graph ? "aot_multi_cta" : "aot_graph";
    const char *mdl_lbl = use_embedded_gguf ? "<embedded>" : (use_sealed_overlay ? "<pe_overlay>" : model_path);

    fprintf(info_fp, "GPU_LLM_MODEL path=\"%s\" arch=%s layers=%u dim=%u hidden=%u heads=%u kv_heads=%u vocab=%u wtype=%s tensor_core=%d mode=%s\n",
            mdl_lbl, model->arch, model->n_layers, model->dim, model->hidden_dim,
            model->n_heads, model->n_kv_heads, model->vocab_size, wtype_str, tc_active, mode_str);
    if (seal_verified && ov_sha) {
        char sha_hex[65], len_s[32];
        sha256_to_hex(ov_sha, sha_hex);
        fmt_u64_dec(ov_len, len_s, sizeof(len_s));
        fprintf(info_fp, "GPU_LLM_SEAL verified=1 mode=pe_overlay sha256=%s bytes=%s\n", sha_hex, len_s);
    }

    if (use_spec) {
        char dwb_s[32];
        fmt_u64_dec(draft_meta[9], dwb_s, sizeof(dwb_s));
        fprintf(info_fp, "GPU_LLM_DRAFT_META ptx_path=\"%s\" fingerprint=%08x%08x wblob_bytes=%s block_dim=%u graph_nodes=%u chain_nodes=%u\n",
                draft_ptx_path,
                (unsigned int)(draft_meta[1] >> 32), (unsigned int)(draft_meta[1] & 0xFFFFFFFFU),
                dwb_s, draft_block_dim, draft_plan.num_nodes, draft_chain_plan.num_nodes);
        fprintf(info_fp, "GPU_LLM_DRAFT_MODEL path=\"%s\" arch=%s layers=%u dim=%u hidden=%u heads=%u kv_heads=%u vocab=%u wtype=%s draft_k=%d adaptive=%d\n",
                draft_model_path, draft->arch, draft->n_layers, draft->dim, draft->hidden_dim,
                draft->n_heads, draft->n_kv_heads, draft->vocab_size, wtype_name_str(draft->weight_type_primary),
                draft_k, spec_adaptive);
    }

    {
        char kv_s[32], base_s[32];
        fmt_u64_dec(kv_total64, kv_s, sizeof(kv_s));
        fmt_u64_dec(f32_total64, base_s, sizeof(base_s));
        double kv_ratio = (kv_total64 > 0ULL) ? ((double)f32_total64 / (double)kv_total64) : 1.0;
        fprintf(info_fp, "GPU_LLM_KV type=%s bytes=%s baseline_f32=%s ratio=%.2fx\n",
                kv_type_name_str(kv_type), kv_s, base_s, kv_ratio);
    }
    if (model->spill_wblob_bytes > 0ULL || (draft && draft->spill_wblob_bytes > 0ULL)) {
        char tv_s[32], th_s[32], dv_s[32], dh_s[32], ss_s[32];
        fmt_u64_dec(model->vram_wblob_bytes, tv_s, sizeof(tv_s));
        fmt_u64_dec(model->spill_wblob_bytes, th_s, sizeof(th_s));
        fmt_u64_dec(draft ? draft->vram_wblob_bytes : 0ULL, dv_s, sizeof(dv_s));
        fmt_u64_dec(draft ? draft->spill_wblob_bytes : 0ULL, dh_s, sizeof(dh_s));
        fmt_u64_dec((uint64_t)act.spill_stage_bytes, ss_s, sizeof(ss_s));
        fprintf(info_fp, "GPU_LLM_WEIGHT_MEM target_vram_bytes=%s target_host_zc_bytes=%s draft_vram_bytes=%s draft_host_zc_bytes=%s spill_slots=%d spill_stage_bytes=%s\n",
                tv_s, th_s, dv_s, dh_s, act.spill_slots, ss_s);
    }

    void *ev0 = NULL, *ev1 = NULL;
    check_cu(cuEventCreate(&ev0, 0U), "ev0");
    check_cu(cuEventCreate(&ev1, 0U), "ev1");
    if (bench_gemm) run_gemm_benchmark(ev0, ev1);

    float *h_logits = (float *)malloc((size_t)max_vocab * 4U);
    float *work_logits = (float *)malloc((size_t)max_vocab * 4U);
    float *h_probs = (float *)malloc((size_t)max_vocab * 4U);
    SampleCand *cands = (SampleCand *)malloc((size_t)max_vocab * sizeof(SampleCand));
    size_t hist_cap = (size_t)eff_max_seq + 128U;
    uint32_t *hist = (uint32_t *)malloc(hist_cap * sizeof(uint32_t));
    int *gen_ids = (int *)malloc((size_t)(steps + 2) * sizeof(int));
    size_t gen_text_cap = (size_t)(steps + 1) * 64U + 256U;
    char *gen_text = (char *)malloc(gen_text_cap);
    if (gen_text) gen_text[0] = '\0';

    SpecStats spec_stats;
    memset(&spec_stats, 0, sizeof(spec_stats));
    int draft_pending_catchup = 0;
    uint32_t draft_catchup_tok = 0U;
    SpecCtx spec_ctx;
    memset(&spec_ctx, 0, sizeof(spec_ctx));
    spec_ctx.target_plan = &mc_plan;
    spec_ctx.verify_plans = verify_plans;
    spec_ctx.target_pf = &mc_pf;
    spec_ctx.target_mod = mod;
    spec_ctx.target = model;
    spec_ctx.act_t = &act;
    spec_ctx.draft_plan = &draft_plan;
    spec_ctx.draft_chain_plan = &draft_chain_plan;
    spec_ctx.draft = draft;
    spec_ctx.act_d = &act_d;
    spec_ctx.no_cuda_graph = no_cuda_graph;
    spec_ctx.kv_type = kv_type;
    spec_ctx.draft_k = draft_k;
    spec_ctx.adaptive_k = spec_adaptive;
    spec_ctx.max_steps = steps;
    spec_ctx.eff_max_seq = eff_max_seq;
    spec_ctx.temp = temp;
    spec_ctx.top_k = top_k;
    spec_ctx.top_p = top_p;
    spec_ctx.min_p = min_p;
    spec_ctx.repeat_penalty = repeat_penalty;
    spec_ctx.ignore_eos = ignore_eos;
    spec_ctx.stream_out = clean_mode ? 1 : stream_out;
    spec_ctx.stream_prefix = clean_mode ? NULL : "GPU_LLM_STREAM ";
    spec_ctx.io_seed = &seed;
    spec_ctx.h_logits = h_logits;
    spec_ctx.work_logits = work_logits;
    spec_ctx.h_probs = h_probs;
    spec_ctx.cands = cands;
    spec_ctx.hist = hist;
    spec_ctx.gen_ids = gen_ids;
    spec_ctx.io_gen_text = &gen_text;
    spec_ctx.io_gen_cap = &gen_text_cap;
    spec_ctx.stats = &spec_stats;
    spec_ctx.io_draft_pending_catchup = &draft_pending_catchup;
    spec_ctx.io_draft_catchup_tok = &draft_catchup_tok;
    spec_ctx.keep_kv_tail = interactive;

    if (interactive) {
        unsigned int pos = 0U, hist_len = 0U;
        int turn = 0;
        if (system_prompt && system_prompt[0] != '\0') {
            char *sys_fmt = format_chat_prompt_alloc(model, NULL, system_prompt, 0);
            uint32_t *sys_ids = NULL; uint32_t sys_len = 0U;
            if (tokenize_prompt(model, sys_fmt, no_bos, &sys_ids, &sys_len) == 0) {
                if ((uint64_t)sys_len >= (uint64_t)eff_max_seq) {
                    printf("GPU_LLM_CTX_FULL pos=0 needed=%u max_seq=%u\n", sys_len, eff_max_seq);
                    free(sys_ids); free(sys_fmt);
                    goto cleanup_all;
                }
                for (uint32_t i = 0U; i < sys_len; i++) hist[hist_len++] = sys_ids[i];
                pos = run_prefill_active(aot_tier, fn_aot_fwd, aot_block_dim, mod, &mc_plan, &mc_pf,
                                         model, &act, use_spec, draft_mod, draft_block_dim, &draft_plan, &draft_pf,
                                         draft, &act_d, sys_ids, sys_len, pos, eff_max_seq,
                                         use_tensor_core, kv_type, prefill_batch, NULL, NULL);
                check_cu(cuCtxSynchronize(), "repl_sys_sync");
                free(sys_ids);
            }
            free(sys_fmt);
        }

        if (clean_mode) {
            fprintf(stderr, "[Interactive ready: max_seq=%u kv=%s mode=%s%s pos=%u | type /reset or /quit]\n",
                    eff_max_seq, kv_type_name_str(kv_type), mode_str, use_spec ? "+spec" : "", pos);
            fflush(stderr);
        } else {
            printf("GPU_LLM_INTERACTIVE_READY max_seq=%u kv_type=%s mode=%s pos=%u\n",
                   eff_max_seq, kv_type_name_str(kv_type), mode_str, pos);
            fflush(stdout);
        }

        char line_buf[4096];
        int pending_cli_turn = user_gave_prompt ? 1 : 0;
        while (1) {
            const char *raw_line = NULL;
            if (pending_cli_turn) {
                pending_cli_turn = 0;
                raw_line = prompt;
            } else {
                fprintf(stderr, "\nYou> ");
                fflush(stderr);
                if (fgets(line_buf, (int)sizeof(line_buf), stdin) == NULL) break;
                if ((unsigned char)line_buf[0] == 0xEFU && (unsigned char)line_buf[1] == 0xBBU && (unsigned char)line_buf[2] == 0xBFU) {
                    memmove(line_buf, line_buf + 3, strlen(line_buf + 3) + 1U);
                }
                size_t llen = strlen(line_buf);
                while (llen > 0U && (line_buf[llen - 1U] == '\n' || line_buf[llen - 1U] == '\r')) {
                    line_buf[--llen] = '\0';
                }
                if (llen == 0U) continue;
                if (strcmp(line_buf, "/quit") == 0 || strcmp(line_buf, "/exit") == 0) break;
                if (strcmp(line_buf, "/reset") == 0) {
                    check_cu(cuMemsetD8_v2(act.k_cache, 0, kv_bytes), "reset_k");
                    check_cu(cuMemsetD8_v2(act.v_cache, 0, kv_bytes), "reset_v");
                    if (use_spec) {
                        check_cu(cuMemsetD8_v2(act_d.k_cache, 0, kv_bytes_d), "reset_dk");
                        check_cu(cuMemsetD8_v2(act_d.v_cache, 0, kv_bytes_d), "reset_dv");
                        draft_pending_catchup = 0;
                    }
                    pos = 0U; hist_len = 0U; turn = 0;
                    if (system_prompt && system_prompt[0] != '\0') {
                        char *sys_fmt = format_chat_prompt_alloc(model, NULL, system_prompt, 0);
                        uint32_t *sys_ids = NULL; uint32_t sys_len = 0U;
                        if (tokenize_prompt(model, sys_fmt, no_bos, &sys_ids, &sys_len) == 0 && (uint64_t)sys_len < (uint64_t)eff_max_seq) {
                            for (uint32_t i = 0U; i < sys_len; i++) hist[hist_len++] = sys_ids[i];
                            pos = run_prefill_active(aot_tier, fn_aot_fwd, aot_block_dim, mod, &mc_plan, &mc_pf,
                                                     model, &act, use_spec, draft_mod, draft_block_dim, &draft_plan, &draft_pf,
                                                     draft, &act_d, sys_ids, sys_len, pos, eff_max_seq,
                                                     use_tensor_core, kv_type, prefill_batch, NULL, NULL);
                            check_cu(cuCtxSynchronize(), "repl_reset_sys_sync");
                            free(sys_ids);
                        }
                        free(sys_fmt);
                    }
                    if (clean_mode) {
                        fprintf(stderr, "[KV cache cleared, pos=%u/%u]\n", pos, eff_max_seq);
                        fflush(stderr);
                    } else {
                        printf("GPU_LLM_REPL_RESET kv_cleared=1 pos=%u\n", pos);
                        fflush(stdout);
                    }
                    continue;
                }
                raw_line = line_buf;
            }

            char *fmt_turn = chat_mode ? format_chat_prompt_alloc(model, raw_line, NULL, (turn > 0) ? 1 : 0) : NULL;
            const char *tok_turn = fmt_turn ? fmt_turn : raw_line;
            int turn_no_bos = (pos > 0U) ? 1 : no_bos;
            uint32_t *t_ids = NULL; uint32_t t_len = 0U;
            if (tokenize_prompt(model, tok_turn, turn_no_bos, &t_ids, &t_len) != 0) {
                if (fmt_turn) free(fmt_turn);
                continue;
            }
            if (fmt_turn) free(fmt_turn);
            if ((uint64_t)pos + (uint64_t)t_len >= (uint64_t)eff_max_seq) {
                if (clean_mode) {
                    fprintf(stderr, "[Context full: pos=%u needed=%u max_seq=%u; type /reset or /quit]\n", pos, t_len, eff_max_seq);
                    fflush(stderr);
                } else {
                    printf("GPU_LLM_CTX_FULL pos=%u needed=%u max_seq=%u\n", pos, t_len, eff_max_seq);
                    fflush(stdout);
                }
                free(t_ids);
                continue;
            }
            for (uint32_t i = 0U; i < t_len; i++) {
                uint32_t tok = t_ids[i];
                if (tok >= model->vocab_size) tok = 0U;
                hist[hist_len++] = tok;
            }
            check_cu(cuEventRecord(ev0, NULL), "repl_ev0");
            pos = run_prefill_active(aot_tier, fn_aot_fwd, aot_block_dim, mod, &mc_plan, &mc_pf,
                                     model, &act, use_spec, draft_mod, draft_block_dim, &draft_plan, &draft_pf,
                                     draft, &act_d, t_ids, t_len, pos, eff_max_seq,
                                     use_tensor_core, kv_type, prefill_batch, NULL, NULL);
            check_cu(cuCtxSynchronize(), "repl_prefill_sync");
            check_cu(cuMemcpyDtoH_v2(h_logits, act.logits, (size_t)model->vocab_size * 4U), "DtoH_turn_logits");
            check_finite_logits(h_logits, model->vocab_size, -1);

            int t_top1 = 0; float t_best = h_logits[0];
            for (unsigned int i = 1U; i < model->vocab_size; i++) {
                if (h_logits[i] > t_best) { t_best = h_logits[i]; t_top1 = (int)i; }
            }
            uint32_t t_fnv = fnv1a_floats(h_logits, model->vocab_size);

            int turn_steps = steps;
            int room = (int)(eff_max_seq - pos);
            if (turn_steps > room) turn_steps = room;
            if (turn_steps < 0) turn_steps = 0;

            int tgen_count = 0;
            if (use_spec) {
                memset(&spec_stats, 0, sizeof(spec_stats));
                spec_ctx.max_steps = turn_steps;
                spec_ctx.stream_out = clean_mode ? 1 : stream_out;
                spec_ctx.stream_prefix = clean_mode ? "Assistant> " : "GPU_LLM_STREAM ";
                spec_ctx.io_hist_len = &hist_len;
                pos = run_speculative_sequence(&spec_ctx, pos, &tgen_count);
            } else {
                pos = run_decode_sequence(aot_tier, fn_aot_fwd, aot_block_dim, &mc_plan, model, &act,
                                          use_tensor_core, kv_type, turn_steps, pos, eff_max_seq,
                                          temp, top_k, top_p, min_p, repeat_penalty, ignore_eos,
                                          clean_mode ? 1 : stream_out,
                                          clean_mode ? "Assistant> " : "GPU_LLM_STREAM ",
                                          &seed, h_logits, work_logits,
                                          h_probs, cands, hist, &hist_len, gen_ids, &gen_text, &gen_text_cap, &tgen_count);
            }
            check_cu(cuEventRecord(ev1, NULL), "repl_ev1");
            check_cu(cuEventSynchronize(ev1), "repl_ev1_sync");
            float turn_ms = 0.0f;
            check_cu(cuEventElapsedTime(&turn_ms, ev0, ev1), "repl_elapsed");
            double turn_tps = (turn_ms > 0.0f) ? (((double)(t_len + (uint32_t)tgen_count) * 1000.0) / (double)turn_ms) : 0.0;

            if (clean_mode) {
                if (use_spec) {
                    double acc_pct = (spec_stats.drafts_proposed > 0U)
                        ? (100.0 * (double)spec_stats.drafts_accepted / (double)spec_stats.drafts_proposed) : 0.0;
                    fprintf(stderr, "[%d gen tok | %.1f tok/s | acc=%.0f%% k=%u | pos=%u/%u]\n",
                            tgen_count, turn_tps, acc_pct, spec_stats.final_k, pos, eff_max_seq);
                } else {
                    fprintf(stderr, "[%d gen tok | %.1f tok/s | pos=%u/%u]\n",
                            tgen_count, turn_tps, pos, eff_max_seq);
                }
                fflush(stderr);
            } else {
                printf("GPU_LLM_TURN turn=%d prompt_tokens=%u gen_tokens=%d pos=%u top1_id=%d top1_logit=%.6f logits_fnv=%08x tokens=",
                       turn, t_len, tgen_count, pos, t_top1, (double)t_best, t_fnv);
                for (int s = 0; s < tgen_count; s++) printf("%s%d", (s > 0) ? "," : "", gen_ids[s]);
                if (tgen_count == 0) printf("none");
                printf(" text=\""); print_sanitized(gen_text); printf("\"\n");
                if (use_spec) {
                    double acc_rate = (spec_stats.drafts_proposed > 0U)
                        ? ((double)spec_stats.drafts_accepted / (double)spec_stats.drafts_proposed) : 0.0;
                    printf("GPU_LLM_SPEC draft_k=%d final_k=%u adaptive=%d cycles=%d proposed=%u accepted=%u accept_rate=%.4f verify_launches=%u\n",
                           draft_k, spec_stats.final_k, spec_adaptive, spec_stats.cycles,
                           spec_stats.drafts_proposed, spec_stats.drafts_accepted, acc_rate, spec_stats.verify_launches);
                }
                fflush(stdout);
            }
            free(t_ids);
            turn++;
        }

        if (clean_mode) {
            fprintf(stderr, "\n[Session ended: turns=%d final_pos=%u]\n", turn, pos);
            fflush(stderr);
        } else {
            printf("GPU_LLM_INTERACTIVE_OK turns=%d final_pos=%u\n", turn, pos);
            fflush(stdout);
        }
        goto cleanup_all;
    }

    char *effective_prompt = NULL;
    const char *tok_in = prompt;
    if (chat_mode || (system_prompt && system_prompt[0] != '\0')) {
        effective_prompt = format_chat_prompt_alloc(model, prompt, system_prompt, 0);
        tok_in = effective_prompt;
    }

    uint32_t *prompt_ids = NULL;
    uint32_t p_len = 0U;
    if (tokenize_prompt(model, tok_in, no_bos, &prompt_ids, &p_len) != 0) {
        fprintf(stderr, "Empty prompt unsupported\n");
        if (effective_prompt) free(effective_prompt);
        return 1;
    }
    if ((uint64_t)p_len + (uint64_t)steps >= (uint64_t)eff_max_seq) {
        fprintf(stderr, "Prompt + steps exceeds context length %u\n", eff_max_seq);
        free(prompt_ids);
        if (effective_prompt) free(effective_prompt);
        return 1;
    }

    unsigned int hist_len = 0U;
    for (uint32_t pi = 0U; pi < p_len; pi++) {
        uint32_t tok = prompt_ids[pi];
        if (tok >= model->vocab_size) tok = 0U;
        hist[hist_len++] = tok;
    }

    unsigned int pf_chunks = 0U;
    int pf_tc_used = 0;
    check_cu(cuEventRecord(ev0, NULL), "fwd_ev0");
    unsigned int pos = run_prefill_active(aot_tier, fn_aot_fwd, aot_block_dim, mod, &mc_plan, &mc_pf,
                                          model, &act, use_spec, draft_mod, draft_block_dim, &draft_plan, &draft_pf,
                                          draft, &act_d, prompt_ids, p_len, 0U, eff_max_seq,
                                          use_tensor_core, kv_type, prefill_batch, &pf_chunks, &pf_tc_used);
    check_cu(cuEventRecord(ev1, NULL), "fwd_ev1");
    check_cu(cuEventSynchronize(ev1), "fwd_sync_prompt");
    float prompt_ms = 0.0f;
    check_cu(cuEventElapsedTime(&prompt_ms, ev0, ev1), "prompt_elapsed");

    if (!clean_mode && (user_gave_prefill_batch || pf_tc_used)) {
        double pf_tps = (prompt_ms > 0.0f) ? (((double)p_len * 1000.0) / (double)prompt_ms) : 0.0;
        printf("GPU_LLM_PREFILL batch=%d prompt_tokens=%u chunk_count=%u tc_prefill=%d prefill_ms=%.4f prefill_tok_per_sec=%.2f\n",
               prefill_batch, p_len, pf_chunks, pf_tc_used, (double)prompt_ms, pf_tps);
    }

    check_cu(cuMemcpyDtoH_v2(h_logits, act.logits, (size_t)model->vocab_size * 4U), "DtoH_prompt_logits");
    check_finite_logits(h_logits, model->vocab_size, -1);
    if (dump_logits_path) {
        FILE *dfp = fopen(dump_logits_path, "wb");
        if (!dfp || fwrite(h_logits, 4U, (size_t)model->vocab_size, dfp) != (size_t)model->vocab_size) {
            fprintf(stderr, "Failed writing logits to '%s'\n", dump_logits_path);
            return 1;
        }
        fclose(dfp);
    }

    int top1_id = 0;
    float top1_logit = h_logits[0];
    for (unsigned int i = 1U; i < model->vocab_size; i++) {
        if (h_logits[i] > top1_logit) { top1_logit = h_logits[i]; top1_id = (int)i; }
    }
    uint32_t logits_fnv = fnv1a_floats(h_logits, model->vocab_size);

    int gen_count = 0;
    float gen_ms = 0.0f;
    if (steps > 0) {
        check_cu(cuEventRecord(ev0, NULL), "gen_ev0");
        if (use_spec) {
            memset(&spec_stats, 0, sizeof(spec_stats));
            spec_ctx.max_steps = steps;
            spec_ctx.stream_out = clean_mode ? 1 : stream_out;
            spec_ctx.stream_prefix = clean_mode ? NULL : "GPU_LLM_STREAM ";
            spec_ctx.io_hist_len = &hist_len;
            pos = run_speculative_sequence(&spec_ctx, pos, &gen_count);
        } else {
            pos = run_decode_sequence(aot_tier, fn_aot_fwd, aot_block_dim, &mc_plan, model, &act,
                                      use_tensor_core, kv_type, steps, pos, eff_max_seq,
                                      temp, top_k, top_p, min_p, repeat_penalty, ignore_eos,
                                      clean_mode ? 1 : stream_out,
                                      clean_mode ? NULL : "GPU_LLM_STREAM ",
                                      &seed, h_logits, work_logits,
                                      h_probs, cands, hist, &hist_len, gen_ids, &gen_text, &gen_text_cap, &gen_count);
        }
        check_cu(cuEventRecord(ev1, NULL), "gen_ev1");
        check_cu(cuEventSynchronize(ev1), "gen_sync");
        check_cu(cuEventElapsedTime(&gen_ms, ev0, ev1), "gen_elapsed");
    } else if (stream_out && !clean_mode) {
        printf("GPU_LLM_STREAM \n");
        fflush(stdout);
    }

    float total_ms = prompt_ms + gen_ms;
    double tok_per_sec = (total_ms > 0.0f) ? (((double)(p_len + (uint32_t)gen_count) * 1000.0) / (double)total_ms) : 0.0;

    if (clean_mode) {
        if (use_spec) {
            double acc_pct = (spec_stats.drafts_proposed > 0U)
                ? (100.0 * (double)spec_stats.drafts_accepted / (double)spec_stats.drafts_proposed) : 0.0;
            fprintf(stderr, "[prompt=%u gen=%d tok | %.1f tok/s | acc=%.0f%% k=%u | %.2f ms]\n",
                    p_len, gen_count, tok_per_sec, acc_pct, spec_stats.final_k, (double)total_ms);
        } else {
            fprintf(stderr, "[prompt=%u gen=%d tok | %.1f tok/s | %.2f ms]\n",
                    p_len, gen_count, tok_per_sec, (double)total_ms);
        }
        fflush(stderr);
    } else {
        printf("GPU_LLM_TEXT prompt=\"");
        print_sanitized(prompt);
        printf("\" generated=\"");
        print_sanitized(gen_text);
        printf("\"\n");

        printf("GPU_LLM_TOKENS ids=");
        for (int s = 0; s < gen_count; s++) printf("%s%d", (s > 0) ? "," : "", gen_ids[s]);
        if (gen_count == 0) printf("none");
        printf("\n");

        printf("GPU_LLM_OK steps=%d total_gpu_ms=%.4f tok_per_sec=%.2f top1_id=%d top1_logit=%.6f logits_fnv=%08x\n",
               gen_count, (double)total_ms, tok_per_sec, top1_id, (double)top1_logit, logits_fnv);
        if (use_spec) {
            double acc_rate = (spec_stats.drafts_proposed > 0U)
                ? ((double)spec_stats.drafts_accepted / (double)spec_stats.drafts_proposed) : 0.0;
            printf("GPU_LLM_SPEC draft_k=%d final_k=%u adaptive=%d cycles=%d proposed=%u accepted=%u accept_rate=%.4f verify_launches=%u\n",
                   draft_k, spec_stats.final_k, spec_adaptive, spec_stats.cycles,
                   spec_stats.drafts_proposed, spec_stats.drafts_accepted, acc_rate, spec_stats.verify_launches);
        }
        fflush(stdout);
    }

    free(prompt_ids);
    if (effective_prompt) free(effective_prompt);

cleanup_all:
    for (int m_idx = 2; m_idx <= 8; m_idx++) mc_verify_plan_free(&verify_plans[m_idx]);
    if (use_spec) {
        mc_plan_free(&draft_chain_plan);
        mc_plan_free(&draft_plan);
    }
    if (aot_tier == 2) mc_plan_free(&mc_plan);
    if (draft) {
        free_model_gpu_mem(draft);
        free_llama_model(draft); free(draft);
    }
    free_model_gpu_mem(model);
    if (act.x) cuMemFree_v2(act.x);
    if (act.xb) cuMemFree_v2(act.xb);
    if (act.xb2) cuMemFree_v2(act.xb2);
    if (act.x_f16) cuMemFree_v2(act.x_f16);
    if (act.q) cuMemFree_v2(act.q);
    if (act.k) cuMemFree_v2(act.k);
    if (act.v) cuMemFree_v2(act.v);
    if (act.att_scratch) cuMemFree_v2(act.att_scratch);
    if (act.hb) cuMemFree_v2(act.hb);
    if (act.hb2) cuMemFree_v2(act.hb2);
    if (act.probs) cuMemFree_v2(act.probs);
    if (act.step_args) cuMemFree_v2(act.step_args);
    if (act.argmax_ids) cuMemFree_v2(act.argmax_ids);
    if (act.argmax_vals) cuMemFree_v2(act.argmax_vals);
    if (act.draft_ids) cuMemFree_v2(act.draft_ids);
    if (act.w_f16_tmp) cuMemFree_v2(act.w_f16_tmp);
    if (act.prefill_tmp) cuMemFree_v2(act.prefill_tmp);
    if (act.batch_toks) cuMemFree_v2(act.batch_toks);
    if (act.spill_stage[0]) cuMemFree_v2(act.spill_stage[0]);
    if (act.spill_stage[1]) cuMemFree_v2(act.spill_stage[1]);
    if (act.dma_ev[0]) cuEventDestroy_v2(act.dma_ev[0]);
    if (act.dma_ev[1]) cuEventDestroy_v2(act.dma_ev[1]);
    if (act.comp_ev[0]) cuEventDestroy_v2(act.comp_ev[0]);
    if (act.comp_ev[1]) cuEventDestroy_v2(act.comp_ev[1]);
    if (act.dma_stream) cuStreamDestroy_v2(act.dma_stream);
    if (use_spec) {
        if (act_d.k_cache) cuMemFree_v2(act_d.k_cache);
        if (act_d.v_cache) cuMemFree_v2(act_d.v_cache);
        if (act_d.step_args) cuMemFree_v2(act_d.step_args);
    }
    cuMemFree_v2(act.k_cache); cuMemFree_v2(act.v_cache); cuMemFree_v2(act.logits);
    free(h_logits); free(work_logits); free(h_probs);
    free(cands); free(hist); free(gen_ids); free(gen_text);
    free_llama_model(model); free(model);
    cuEventDestroy_v2(ev0); cuEventDestroy_v2(ev1);
    if (draft_mod) cuModuleUnload(draft_mod);
    cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
    return 0;
}
#endif /* !ZCC_GPU_LLM_LIBRARY */
