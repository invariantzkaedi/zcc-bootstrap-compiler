/* SPDX-License-Identifier: MIT */
#define _POSIX_C_SOURCE 200809L
#include "zcc_compute.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
typedef HMODULE t21_lib_t;
#define T21_DLOPEN(p) LoadLibraryA(p)
#define T21_DLSYM(h, s) ((void *)GetProcAddress((h), (s)))
#define T21_DLCLOSE(h) FreeLibrary(h)
#else
#include <dlfcn.h>
typedef void *t21_lib_t;
#define T21_DLOPEN(p) dlopen((p), RTLD_LAZY | RTLD_LOCAL)
#define T21_DLSYM(h, s) dlsym((h), (s))
#define T21_DLCLOSE(h) dlclose(h)
#endif

typedef int (*cuInit_t)(unsigned int);
typedef int (*cuDriverGetVersion_t)(int *);
typedef int (*cuDeviceGetCount_t)(int *);
typedef int (*cuDeviceGet_t)(int *, int);
typedef int (*cuDeviceGetName_t)(char *, int, int);
typedef int (*cuDeviceGetAttribute_t)(int *, int, int);
typedef int (*cuDeviceTotalMem_v2_t)(size_t *, int);
typedef int (*cuCtxCreate_v2_t)(void **, unsigned int, int);
typedef int (*cuCtxSetCurrent_t)(void *);
typedef int (*cuCtxDestroy_v2_t)(void *);
typedef int (*cuModuleLoadDataEx_t)(void **, const void *, unsigned int, int *, void **);
typedef int (*cuModuleUnload_t)(void *);
typedef int (*cuModuleGetFunction_t)(void **, void *, const char *);
typedef int (*cuMemAlloc_v2_t)(uint64_t *, size_t);
typedef int (*cuMemFree_v2_t)(uint64_t);
typedef int (*cuMemcpyHtoD_v2_t)(uint64_t, const void *, size_t);
typedef int (*cuMemcpyDtoH_v2_t)(void *, uint64_t, size_t);
typedef int (*cuLaunchKernel_t)(void *, unsigned int, unsigned int, unsigned int,
                                unsigned int, unsigned int, unsigned int,
                                unsigned int, void *, void **, void **);
typedef int (*cuCtxSynchronize_t)(void);
typedef int (*cuGetErrorName_t)(int, const char **);

typedef struct {
    int initialized;
    int available;
    t21_lib_t lib;
    void *ctx;
    void *mod;
    int driver_ver;
    int dev_count;
    int dev;
    int cc_maj;
    int cc_min;
    size_t vram_bytes;
    char dev_name[128];
    char ptx_path[512];
    char ptx_sha256[65];

    cuInit_t fn_cuInit;
    cuDriverGetVersion_t fn_cuDriverGetVersion;
    cuDeviceGetCount_t fn_cuDeviceGetCount;
    cuDeviceGet_t fn_cuDeviceGet;
    cuDeviceGetName_t fn_cuDeviceGetName;
    cuDeviceGetAttribute_t fn_cuDeviceGetAttribute;
    cuDeviceTotalMem_v2_t fn_cuDeviceTotalMem;
    cuCtxCreate_v2_t fn_cuCtxCreate;
    cuCtxSetCurrent_t fn_cuCtxSetCurrent;
    cuCtxDestroy_v2_t fn_cuCtxDestroy;
    cuModuleLoadDataEx_t fn_cuModuleLoadDataEx;
    cuModuleUnload_t fn_cuModuleUnload;
    cuModuleGetFunction_t fn_cuModuleGetFunction;
    cuMemAlloc_v2_t fn_cuMemAlloc;
    cuMemFree_v2_t fn_cuMemFree;
    cuMemcpyHtoD_v2_t fn_cuMemcpyHtoD;
    cuMemcpyDtoH_v2_t fn_cuMemcpyDtoH;
    cuLaunchKernel_t fn_cuLaunchKernel;
    cuCtxSynchronize_t fn_cuCtxSynchronize;
    cuGetErrorName_t fn_cuGetErrorName;

    void *k_vec_add;
    void *k_vec_mul;
    void *k_vec_fma;
    void *k_dot;
    void *k_gemm;
    void *k_rmsnorm;
    void *k_softmax;
    void *k_dequant_q8_0;
    void *k_matvec_q8_0;
    void *k_dequant_q4_0;
    void *k_matvec_q4_0;
    void *k_dequant_q4_k;
    void *k_matvec_q4_k;
} t21_ptx_state_t;

static t21_ptx_state_t g_ptx = {0};

static void *t21_host_alloc(const zcc_exec_options_t *opts, size_t nbytes) {
    if (opts && opts->alloc_fn) return opts->alloc_fn(nbytes, opts->alloc_ctx);
    return malloc(nbytes);
}

static void t21_host_free(const zcc_exec_options_t *opts, void *ptr) {
    if (!ptr) return;
    if (opts && opts->free_fn) {
        opts->free_fn(ptr, opts->alloc_ctx);
    } else {
        free(ptr);
    }
}

static const char *resolve_ptx_path(const char *override_path) {
    if (override_path && override_path[0]) return override_path;
    const char *envp = getenv("ZCC_T21A_PTX_PATH");
    if (envp && envp[0]) return envp;
    return "runtime/kernels_t21a_sm120.ptx";
}

static char *read_file_text(const char *path, size_t *out_len) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return NULL; }
    long sz = ftell(fp);
    if (sz <= 0 || fseek(fp, 0, SEEK_SET) != 0) { fclose(fp); return NULL; }
    char *buf = (char *)malloc((size_t)sz + 1U);
    if (!buf) { fclose(fp); return NULL; }
    size_t got = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    if (got != (size_t)sz) { free(buf); return NULL; }
    buf[got] = '\0';
    if (out_len) *out_len = got;
    return buf;
}

static int resolve_cuda_symbols(t21_ptx_state_t *s) {
#if defined(_WIN32)
    s->lib = T21_DLOPEN("nvcuda.dll");
#else
    s->lib = T21_DLOPEN("/usr/lib/wsl/lib/libcuda.so.1");
    if (!s->lib) s->lib = T21_DLOPEN("libcuda.so.1");
    if (!s->lib) s->lib = T21_DLOPEN("libcuda.so");
#endif
    if (!s->lib) return -1;

#define LOAD_SYM(field, type, name) do { \
    void *sym = T21_DLSYM(s->lib, (name)); \
    if (!sym) return -1; \
    memcpy(&(s->field), &sym, sizeof(sym)); \
} while (0)

    LOAD_SYM(fn_cuInit, cuInit_t, "cuInit");
    LOAD_SYM(fn_cuDriverGetVersion, cuDriverGetVersion_t, "cuDriverGetVersion");
    LOAD_SYM(fn_cuDeviceGetCount, cuDeviceGetCount_t, "cuDeviceGetCount");
    LOAD_SYM(fn_cuDeviceGet, cuDeviceGet_t, "cuDeviceGet");
    LOAD_SYM(fn_cuDeviceGetName, cuDeviceGetName_t, "cuDeviceGetName");
    LOAD_SYM(fn_cuDeviceGetAttribute, cuDeviceGetAttribute_t, "cuDeviceGetAttribute");
    LOAD_SYM(fn_cuDeviceTotalMem, cuDeviceTotalMem_v2_t, "cuDeviceTotalMem_v2");
    LOAD_SYM(fn_cuCtxCreate, cuCtxCreate_v2_t, "cuCtxCreate_v2");
    LOAD_SYM(fn_cuCtxSetCurrent, cuCtxSetCurrent_t, "cuCtxSetCurrent");
    LOAD_SYM(fn_cuCtxDestroy, cuCtxDestroy_v2_t, "cuCtxDestroy_v2");
    LOAD_SYM(fn_cuModuleLoadDataEx, cuModuleLoadDataEx_t, "cuModuleLoadDataEx");
    LOAD_SYM(fn_cuModuleUnload, cuModuleUnload_t, "cuModuleUnload");
    LOAD_SYM(fn_cuModuleGetFunction, cuModuleGetFunction_t, "cuModuleGetFunction");
    LOAD_SYM(fn_cuMemAlloc, cuMemAlloc_v2_t, "cuMemAlloc_v2");
    LOAD_SYM(fn_cuMemFree, cuMemFree_v2_t, "cuMemFree_v2");
    LOAD_SYM(fn_cuMemcpyHtoD, cuMemcpyHtoD_v2_t, "cuMemcpyHtoD_v2");
    LOAD_SYM(fn_cuMemcpyDtoH, cuMemcpyDtoH_v2_t, "cuMemcpyDtoH_v2");
    LOAD_SYM(fn_cuLaunchKernel, cuLaunchKernel_t, "cuLaunchKernel");
    LOAD_SYM(fn_cuCtxSynchronize, cuCtxSynchronize_t, "cuCtxSynchronize");
    LOAD_SYM(fn_cuGetErrorName, cuGetErrorName_t, "cuGetErrorName");
#undef LOAD_SYM
    return 0;
}

static int ensure_ptx_ready(const char *ptx_path_override, char *err_buf, size_t err_cap) {
    const char *path = resolve_ptx_path(ptx_path_override);
    if (g_ptx.initialized && g_ptx.available && strcmp(g_ptx.ptx_path, path) == 0) {
        if (g_ptx.fn_cuCtxSetCurrent) g_ptx.fn_cuCtxSetCurrent(g_ptx.ctx);
        return 0;
    }

    if (g_ptx.initialized && strcmp(g_ptx.ptx_path, path) != 0) {
        zcc_ptx_shutdown();
    }

    memset(&g_ptx, 0, sizeof(g_ptx));
    g_ptx.initialized = 1;
    snprintf(g_ptx.ptx_path, sizeof(g_ptx.ptx_path), "%s", path);

    if (resolve_cuda_symbols(&g_ptx) != 0) {
        if (err_buf && err_cap > 0U) snprintf(err_buf, err_cap, "CUDA Driver library unavailable");
        return -1;
    }
    if (g_ptx.fn_cuInit(0U) != 0) {
        if (err_buf && err_cap > 0U) snprintf(err_buf, err_cap, "cuInit failed");
        return -1;
    }
    if (g_ptx.fn_cuDeviceGetCount(&g_ptx.dev_count) != 0 || g_ptx.dev_count <= 0) {
        if (err_buf && err_cap > 0U) snprintf(err_buf, err_cap, "No CUDA device present");
        return -1;
    }
    if (g_ptx.fn_cuDeviceGet(&g_ptx.dev, 0) != 0) {
        if (err_buf && err_cap > 0U) snprintf(err_buf, err_cap, "cuDeviceGet(0) failed");
        return -1;
    }
    g_ptx.fn_cuDriverGetVersion(&g_ptx.driver_ver);
    g_ptx.fn_cuDeviceGetName(g_ptx.dev_name, (int)sizeof(g_ptx.dev_name) - 1, g_ptx.dev);
    g_ptx.fn_cuDeviceGetAttribute(&g_ptx.cc_maj, 75, g_ptx.dev);
    g_ptx.fn_cuDeviceGetAttribute(&g_ptx.cc_min, 76, g_ptx.dev);
    g_ptx.fn_cuDeviceTotalMem(&g_ptx.vram_bytes, g_ptx.dev);

    if (g_ptx.cc_maj < 8) {
        if (err_buf && err_cap > 0U) snprintf(err_buf, err_cap, "Device CC %d.%d < 8.0", g_ptx.cc_maj, g_ptx.cc_min);
        return -1;
    }

    size_t ptx_len = 0U;
    char *ptx_src = read_file_text(path, &ptx_len);
    if (!ptx_src) {
        if (err_buf && err_cap > 0U) snprintf(err_buf, err_cap, "Unable to read PTX module: %s", path);
        return -1;
    }
    zcc_sha256_hex(ptx_src, ptx_len, g_ptx.ptx_sha256);

    if (g_ptx.fn_cuCtxCreate(&g_ptx.ctx, 0U, g_ptx.dev) != 0) {
        free(ptx_src);
        if (err_buf && err_cap > 0U) snprintf(err_buf, err_cap, "cuCtxCreate_v2 failed");
        return -1;
    }

    char jit_log[2048];
    memset(jit_log, 0, sizeof(jit_log));
    int jit_opts[2] = { 5, 6 };
    void *jit_vals[2] = { (void *)jit_log, (void *)(uintptr_t)sizeof(jit_log) };
    int load_rc = g_ptx.fn_cuModuleLoadDataEx(&g_ptx.mod, ptx_src, 2U, jit_opts, jit_vals);
    free(ptx_src);
    if (load_rc != 0) {
        const char *ename = "CUDA_ERR";
        g_ptx.fn_cuGetErrorName(load_rc, &ename);
        if (err_buf && err_cap > 0U) {
            snprintf(err_buf, err_cap, "cuModuleLoadDataEx failed (%s rc=%d): %.96s",
                     ename ? ename : "?", load_rc, jit_log);
        }
        g_ptx.fn_cuCtxDestroy(g_ptx.ctx);
        g_ptx.ctx = NULL;
        return -1;
    }

#define LOAD_K(field, name) do { \
    if (g_ptx.fn_cuModuleGetFunction(&(g_ptx.field), g_ptx.mod, (name)) != 0) { \
        if (err_buf && err_cap > 0U) snprintf(err_buf, err_cap, "Missing PTX kernel: %s", (name)); \
        g_ptx.fn_cuModuleUnload(g_ptx.mod); g_ptx.mod = NULL; \
        g_ptx.fn_cuCtxDestroy(g_ptx.ctx); g_ptx.ctx = NULL; \
        return -1; \
    } \
} while (0)

    LOAD_K(k_vec_add, "t21_vec_add_f32");
    LOAD_K(k_vec_mul, "t21_vec_mul_f32");
    LOAD_K(k_vec_fma, "t21_vec_fma_f32");
    LOAD_K(k_dot, "t21_dot_f32");
    LOAD_K(k_gemm, "t21_gemm_f32");
    LOAD_K(k_rmsnorm, "t21_rmsnorm_f32");
    LOAD_K(k_softmax, "t21_softmax_f32");
    LOAD_K(k_dequant_q8_0, "t21_dequant_q8_0");
    LOAD_K(k_matvec_q8_0, "t21_matvec_q8_0");
    LOAD_K(k_dequant_q4_0, "t21_dequant_q4_0");
    LOAD_K(k_matvec_q4_0, "t21_matvec_q4_0");
    LOAD_K(k_dequant_q4_k, "t21_dequant_q4_k");
    LOAD_K(k_matvec_q4_k, "t21_matvec_q4_k");
#undef LOAD_K

    g_ptx.available = 1;
    return 0;
}

zcc_status_t zcc_probe_ptx_caps(zcc_ptx_caps_t *out_caps, const char *ptx_path_override) {
    if (!out_caps) return ZCC_STATUS_ERR_NULL_PTR;
    memset(out_caps, 0, sizeof(*out_caps));
    out_caps->struct_size = (uint32_t)sizeof(*out_caps);
    char err_dummy[128];
    if (ensure_ptx_ready(ptx_path_override, err_dummy, sizeof(err_dummy)) != 0) {
        return ZCC_STATUS_OK;
    }
    out_caps->cuda_available = 1U;
    out_caps->driver_version = g_ptx.driver_ver;
    out_caps->device_count = g_ptx.dev_count;
    out_caps->cc_major = g_ptx.cc_maj;
    out_caps->cc_minor = g_ptx.cc_min;
    out_caps->vram_bytes = (uint64_t)g_ptx.vram_bytes;
    snprintf(out_caps->device_name, sizeof(out_caps->device_name), "%s", g_ptx.dev_name);
    return ZCC_STATUS_OK;
}

void zcc_ptx_shutdown(void) {
    if (g_ptx.mod && g_ptx.fn_cuModuleUnload) {
        g_ptx.fn_cuModuleUnload(g_ptx.mod);
        g_ptx.mod = NULL;
    }
    if (g_ptx.ctx && g_ptx.fn_cuCtxDestroy) {
        g_ptx.fn_cuCtxDestroy(g_ptx.ctx);
        g_ptx.ctx = NULL;
    }
    if (g_ptx.lib) {
        T21_DLCLOSE(g_ptx.lib);
        g_ptx.lib = NULL;
    }
    memset(&g_ptx, 0, sizeof(g_ptx));
}

static int64_t desc_total_elems(const zcc_tensor_desc_t *t) {
    int64_t n = 1;
    for (uint32_t d = 0U; d < t->rank; d++) n *= t->shape[d];
    return n;
}

static uint64_t desc_byte_off(const zcc_tensor_desc_t *t, int64_t idx) {
    if (t->rank == 1U) {
        int64_t s0 = (t->strides[0] > 0) ? t->strides[0] : 4;
        return (uint64_t)idx * (uint64_t)s0;
    }
    uint64_t off = 0ULL;
    int64_t rem = idx;
    uint64_t exp_st = 4ULL;
    for (int d = (int)t->rank - 1; d >= 0; d--) {
        int64_t dim = t->shape[d];
        int64_t coord = rem % dim;
        rem /= dim;
        uint64_t st = (t->strides[d] > 0) ? (uint64_t)t->strides[d] : exp_st;
        off += (uint64_t)coord * st;
        exp_st = st * (uint64_t)dim;
    }
    return off;
}

static float *pack_f32_contiguous(const zcc_tensor_desc_t *t, const zcc_exec_options_t *opts, size_t *out_bytes) {
    int64_t n = desc_total_elems(t);
    size_t sz = (size_t)n * sizeof(float);
    float *buf = (float *)t21_host_alloc(opts, sz);
    if (!buf) return NULL;
    for (int64_t i = 0; i < n; i++) {
        memcpy(&buf[i], (const uint8_t *)t->data + desc_byte_off(t, i), sizeof(float));
    }
    if (out_bytes) *out_bytes = sz;
    return buf;
}

static void unpack_f32_contiguous(zcc_tensor_desc_t *t, const float *buf) {
    int64_t n = desc_total_elems(t);
    for (int64_t i = 0; i < n; i++) {
        memcpy((uint8_t *)t->data + desc_byte_off(t, i), &buf[i], sizeof(float));
    }
}

zcc_status_t zcc_ptx_exec(
    zcc_op_t op,
    const zcc_tensor_desc_t *inputs,
    uint32_t num_inputs,
    zcc_tensor_desc_t *output,
    const zcc_exec_options_t *opts,
    const zcc_ptx_caps_t *caps,
    char *ptx_sha_out,
    char *err_buf,
    size_t err_cap
) {
    (void)caps;
    if (opts && opts->accum_policy != ZCC_ACCUM_FP32_FMA) {
        if (err_buf && err_cap > 0U) {
            snprintf(err_buf, err_cap, "PTX_SM120 supports ZCC_ACCUM_FP32_FMA accumulation policy only");
        }
        return ZCC_STATUS_ERR_INVALID_ARG;
    }

    const char *ptx_override = opts ? opts->ptx_path_override : NULL;
    if (ensure_ptx_ready(ptx_override, err_buf, err_cap) != 0) {
        return ZCC_STATUS_ERR_BACKEND_UNAVAILABLE;
    }
    if (ptx_sha_out) {
        snprintf(ptx_sha_out, 65, "%s", g_ptx.ptx_sha256);
    }

    float *h_in[3] = { NULL, NULL, NULL };
    size_t h_in_bytes[3] = { 0U, 0U, 0U };
    uint64_t d_in[3] = { 0ULL, 0ULL, 0ULL };
    float *h_out = NULL;
    size_t h_out_bytes = 0U;
    uint64_t d_out = 0ULL;
    zcc_status_t rc = ZCC_STATUS_OK;

    /* Prepare host staged contiguous buffers + device buffers */
    for (uint32_t i = 0U; i < num_inputs; i++) {
        if (inputs[i].dtype == ZCC_DTYPE_F32) {
            h_in[i] = pack_f32_contiguous(&inputs[i], opts, &h_in_bytes[i]);
            if (!h_in[i]) { rc = ZCC_STATUS_ERR_OOM; goto cleanup; }
            if (g_ptx.fn_cuMemAlloc(&d_in[i], h_in_bytes[i]) != 0) { rc = ZCC_STATUS_ERR_OOM; goto cleanup; }
            if (g_ptx.fn_cuMemcpyHtoD(d_in[i], h_in[i], h_in_bytes[i]) != 0) {
                rc = ZCC_STATUS_ERR_BACKEND_RUNTIME; goto cleanup;
            }
        } else {
            /* Quantized Q8_0 / Q4_0 / Q4_K */
            size_t qbytes = (size_t)inputs[i].nbytes;
            if (opts && opts->alloc_fn) {
                h_in[i] = (float *)t21_host_alloc(opts, qbytes);
                if (!h_in[i]) { rc = ZCC_STATUS_ERR_OOM; goto cleanup; }
                memcpy(h_in[i], inputs[i].data, qbytes);
            }
            if (g_ptx.fn_cuMemAlloc(&d_in[i], qbytes) != 0) { rc = ZCC_STATUS_ERR_OOM; goto cleanup; }
            if (g_ptx.fn_cuMemcpyHtoD(d_in[i], h_in[i] ? (const void *)h_in[i] : inputs[i].data, qbytes) != 0) {
                rc = ZCC_STATUS_ERR_BACKEND_RUNTIME; goto cleanup;
            }
        }
    }

    int64_t out_elems = desc_total_elems(output);
    h_out_bytes = (size_t)out_elems * sizeof(float);
    h_out = (float *)t21_host_alloc(opts, h_out_bytes);
    if (!h_out) { rc = ZCC_STATUS_ERR_OOM; goto cleanup; }
    if (g_ptx.fn_cuMemAlloc(&d_out, h_out_bytes) != 0) { rc = ZCC_STATUS_ERR_OOM; goto cleanup; }

    int cu_rc = 0;
    if (op == ZCC_OP_VEC_ADD_F32 || op == ZCC_OP_VEC_MUL_F32) {
        unsigned int n = (unsigned int)desc_total_elems(&inputs[0]);
        unsigned int gx = (n + 255U) / 256U;
        void *fn = (op == ZCC_OP_VEC_ADD_F32) ? g_ptx.k_vec_add : g_ptx.k_vec_mul;
        void *params[4] = { &d_in[0], &d_in[1], &d_out, &n };
        cu_rc = g_ptx.fn_cuLaunchKernel(fn, gx, 1U, 1U, 256U, 1U, 1U, 0U, NULL, params, NULL);
    } else if (op == ZCC_OP_VEC_FMA_F32) {
        unsigned int n = (unsigned int)desc_total_elems(&inputs[0]);
        unsigned int gx = (n + 255U) / 256U;
        void *params[5] = { &d_in[0], &d_in[1], &d_in[2], &d_out, &n };
        cu_rc = g_ptx.fn_cuLaunchKernel(g_ptx.k_vec_fma, gx, 1U, 1U, 256U, 1U, 1U, 0U, NULL, params, NULL);
    } else if (op == ZCC_OP_DOT_F32) {
        unsigned int n = (unsigned int)desc_total_elems(&inputs[0]);
        void *params[4] = { &d_in[0], &d_in[1], &d_out, &n };
        cu_rc = g_ptx.fn_cuLaunchKernel(g_ptx.k_dot, 1U, 1U, 1U, 32U, 1U, 1U, 0U, NULL, params, NULL);
    } else if (op == ZCC_OP_GEMM_F32) {
        unsigned int M = (unsigned int)inputs[0].shape[0];
        unsigned int K = (unsigned int)inputs[0].shape[1];
        unsigned int N = (unsigned int)inputs[1].shape[1];
        unsigned int lda = K, ldb = N, ldc = N;
        unsigned int gx = (N + 31U) / 32U;
        void *params[9] = { &d_in[0], &d_in[1], &d_out, &M, &N, &K, &lda, &ldb, &ldc };
        cu_rc = g_ptx.fn_cuLaunchKernel(g_ptx.k_gemm, gx, M, 1U, 32U, 1U, 1U, 0U, NULL, params, NULL);
    } else if (op == ZCC_OP_RMSNORM_F32) {
        unsigned int rows = (inputs[0].rank == 1U) ? 1U : (unsigned int)inputs[0].shape[0];
        unsigned int dim  = (inputs[0].rank == 1U) ? (unsigned int)inputs[0].shape[0] : (unsigned int)inputs[0].shape[1];
        unsigned int ld = dim;
        float eps = (opts && opts->rmsnorm_eps > 0.0f) ? opts->rmsnorm_eps : 1e-5f;
        void *params[8] = { &d_in[0], &d_in[1], &d_out, &rows, &dim, &ld, &ld, &eps };
        cu_rc = g_ptx.fn_cuLaunchKernel(g_ptx.k_rmsnorm, rows, 1U, 1U, 32U, 1U, 1U, 0U, NULL, params, NULL);
    } else if (op == ZCC_OP_SOFTMAX_F32) {
        unsigned int rows = (inputs[0].rank == 1U) ? 1U : (unsigned int)inputs[0].shape[0];
        unsigned int dim  = (inputs[0].rank == 1U) ? (unsigned int)inputs[0].shape[0] : (unsigned int)inputs[0].shape[1];
        unsigned int ld = dim;
        void *params[6] = { &d_in[0], &d_out, &rows, &dim, &ld, &ld };
        cu_rc = g_ptx.fn_cuLaunchKernel(g_ptx.k_softmax, rows, 1U, 1U, 32U, 1U, 1U, 0U, NULL, params, NULL);
    } else if (op == ZCC_OP_DEQUANT_Q8_0 || op == ZCC_OP_DEQUANT_Q4_0 || op == ZCC_OP_DEQUANT_Q4_K) {
        unsigned int N = (inputs[0].rank == 1U) ? 1U : (unsigned int)inputs[0].shape[0];
        unsigned int K = (inputs[0].rank == 1U) ? (unsigned int)inputs[0].shape[0] : (unsigned int)inputs[0].shape[1];
        unsigned int ldy = K;
        unsigned int gx = (K + 31U) / 32U;
        void *fn = (op == ZCC_OP_DEQUANT_Q8_0) ? g_ptx.k_dequant_q8_0
                 : (op == ZCC_OP_DEQUANT_Q4_0) ? g_ptx.k_dequant_q4_0 : g_ptx.k_dequant_q4_k;
        void *params[5] = { &d_in[0], &d_out, &N, &K, &ldy };
        cu_rc = g_ptx.fn_cuLaunchKernel(fn, gx, N, 1U, 32U, 1U, 1U, 0U, NULL, params, NULL);
    } else if (op == ZCC_OP_MATVEC_Q8_0 || op == ZCC_OP_MATVEC_Q4_0 || op == ZCC_OP_MATVEC_Q4_K) {
        unsigned int N = (unsigned int)inputs[0].shape[0];
        unsigned int K = (unsigned int)inputs[0].shape[1];
        void *fn = (op == ZCC_OP_MATVEC_Q8_0) ? g_ptx.k_matvec_q8_0
                 : (op == ZCC_OP_MATVEC_Q4_0) ? g_ptx.k_matvec_q4_0 : g_ptx.k_matvec_q4_k;
        void *params[5] = { &d_in[0], &d_in[1], &d_out, &N, &K };
        cu_rc = g_ptx.fn_cuLaunchKernel(fn, N, 1U, 1U, 32U, 1U, 1U, 0U, NULL, params, NULL);
    } else {
        rc = ZCC_STATUS_ERR_INVALID_OP;
        goto cleanup;
    }

    if (cu_rc != 0) {
        const char *ename = "CUDA_LAUNCH_ERR";
        g_ptx.fn_cuGetErrorName(cu_rc, &ename);
        if (err_buf && err_cap > 0U) snprintf(err_buf, err_cap, "cuLaunchKernel failed (%s rc=%d)", ename ? ename : "?", cu_rc);
        rc = ZCC_STATUS_ERR_BACKEND_RUNTIME;
        goto cleanup;
    }
    cu_rc = g_ptx.fn_cuCtxSynchronize();
    if (cu_rc != 0) {
        const char *ename = "CUDA_SYNC_ERR";
        g_ptx.fn_cuGetErrorName(cu_rc, &ename);
        if (err_buf && err_cap > 0U) snprintf(err_buf, err_cap, "cuCtxSynchronize failed (%s rc=%d)", ename ? ename : "?", cu_rc);
        rc = ZCC_STATUS_ERR_BACKEND_RUNTIME;
        goto cleanup;
    }
    cu_rc = g_ptx.fn_cuMemcpyDtoH(h_out, d_out, h_out_bytes);
    if (cu_rc != 0) {
        if (err_buf && err_cap > 0U) snprintf(err_buf, err_cap, "cuMemcpyDtoH_v2 failed (rc=%d)", cu_rc);
        rc = ZCC_STATUS_ERR_BACKEND_RUNTIME;
        goto cleanup;
    }

    unpack_f32_contiguous(output, h_out);

cleanup:
    if (d_out && g_ptx.fn_cuMemFree) g_ptx.fn_cuMemFree(d_out);
    for (uint32_t i = 0U; i < 3U; i++) {
        if (d_in[i] && g_ptx.fn_cuMemFree) g_ptx.fn_cuMemFree(d_in[i]);
        t21_host_free(opts, h_in[i]);
    }
    t21_host_free(opts, h_out);
    return rc;
}
