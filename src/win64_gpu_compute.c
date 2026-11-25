/*
 * ZKAEDI ZCC // Sovereign Win64 GPU Compute Oracle (nvcuda.dll + ZCC PTX Backend)
 * Compiled natively to Win64 PE32+ by ZCC with zero MSVC, zero MinGW, zero CUDA SDK.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "gpu_kernels_ptx.h"

/* CUDA Driver API declarations resolved directly via PE32+ IAT -> System32\nvcuda.dll */
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
extern int cuModuleLoadData(void **module, const void *image);
extern int cuModuleLoadDataEx(void **module, const void *image, unsigned int numOptions, int *options, void **optionValues);
extern int cuModuleUnload(void *hmod);
extern int cuModuleGetFunction(void **hfunc, void *hmod, const char *name);
extern int cuMemAlloc_v2(uint64_t *dptr, size_t bytesize);
extern int cuMemFree_v2(uint64_t dptr);
extern int cuMemcpyHtoD_v2(uint64_t dstDevice, const void *srcHost, size_t ByteCount);
extern int cuMemcpyDtoH_v2(void *dstHost, uint64_t srcDevice, size_t ByteCount);
extern int cuLaunchKernel(
    void *f,
    unsigned int gridDimX, unsigned int gridDimY, unsigned int gridDimZ,
    unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ,
    unsigned int sharedMemBytes,
    void *hStream,
    void **kernelParams,
    void **extra
);
extern int cuEventCreate(void **phEvent, unsigned int Flags);
extern int cuEventRecord(void *hEvent, void *hStream);
extern int cuEventSynchronize(void *hEvent);
extern int cuEventElapsedTime(float *pMilliseconds, void *hStart, void *hEnd);
extern int cuEventDestroy_v2(void *hEvent);
extern int cuGetErrorName(int error, const char **pStr);
extern int cuGetErrorString(int error, const char **pStr);

static void check_cu(int rc, const char *where) {
    if (rc != 0) {
        const char *ename = "UNKNOWN";
        const char *edesc = "";
        cuGetErrorName(rc, &ename);
        cuGetErrorString(rc, &edesc);
        fprintf(stderr, "CUDA Driver failure in %s: rc=%d (%s: %s)\n", where, rc, ename ? ename : "?", edesc ? edesc : "");
        exit(1);
    }
}

static uint16_t f32_to_f16_simple(float f) {
    uint32_t x = 0;
    memcpy(&x, &f, 4);
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

static float f16_to_f32_simple(uint16_t h) {
    uint32_t sign = ((uint32_t)h & 0x8000U) << 16;
    uint32_t exp = (h >> 10) & 0x1FU;
    uint32_t mant = h & 0x3FFU;
    uint32_t out = 0;
    if (exp == 0U) {
        out = sign;
    } else if (exp == 31U) {
        out = sign | 0x7F800000U | (mant << 13);
    } else {
        out = sign | ((exp - 15U + 127U) << 23) | (mant << 13);
    }
    float rf = 0.0f;
    memcpy(&rf, &out, 4);
    return rf;
}

static uint32_t cpu_rotr32(uint32_t x, uint32_t k) {
    k &= 31U;
    return k ? ((x >> k) | (x << (32U - k))) : x;
}

static uint32_t cpu_popc32(uint32_t x) {
    uint32_t c = 0;
    while (x) {
        c += (x & 1U);
        x >>= 1;
    }
    return c;
}

static uint32_t cpu_clz32(uint32_t x) {
    if (x == 0U) return 32U;
    uint32_t c = 0;
    while ((x & 0x80000000U) == 0U) {
        c++;
        x <<= 1;
    }
    return c;
}

static uint32_t fnv1a_u32(const uint32_t *buf, int n) {
    uint32_t h = 2166136261U;
    for (int i = 0; i < n; i++) {
        uint32_t v = buf[i];
        h ^= (v & 0xFFU); h *= 16777619U;
        h ^= ((v >> 8) & 0xFFU); h *= 16777619U;
        h ^= ((v >> 16) & 0xFFU); h *= 16777619U;
        h ^= ((v >> 24) & 0xFFU); h *= 16777619U;
    }
    return h;
}

static int count_unique_u32(const uint32_t *buf, int n) {
    static uint32_t seen[512];
    int used = 0;
    for (int i = 0; i < n; i += 3) {
        uint32_t v = buf[i] & 0x00FFFFFFU;
        int found = 0;
        for (int j = 0; j < used; j++) {
            if (seen[j] == v) { found = 1; break; }
        }
        if (!found) {
            if (used < 512) seen[used++] = v;
            else break;
        }
    }
    return used;
}

int main(int argc, char **argv) {
    int test_bad_ptx = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--test-bad-ptx") == 0) test_bad_ptx = 1;
    }

    check_cu(cuInit(0U), "cuInit");
    int drv_ver = 0, dev_cnt = 0, dev = 0;
    check_cu(cuDriverGetVersion(&drv_ver), "cuDriverGetVersion");
    check_cu(cuDeviceGetCount(&dev_cnt), "cuDeviceGetCount");
    if (dev_cnt <= 0) {
        fprintf(stderr, "No CUDA device detected\n");
        return 1;
    }
    check_cu(cuDeviceGet(&dev, 0), "cuDeviceGet");

    char dev_name[256];
    memset(dev_name, 0, sizeof(dev_name));
    check_cu(cuDeviceGetName(dev_name, (int)sizeof(dev_name) - 1, dev), "cuDeviceGetName");
    int cc_maj = 0, cc_min = 0;
    check_cu(cuDeviceGetAttribute(&cc_maj, 75, dev), "cuDeviceGetAttribute(CC_MAJOR)");
    check_cu(cuDeviceGetAttribute(&cc_min, 76, dev), "cuDeviceGetAttribute(CC_MINOR)");
    size_t vram_bytes = 0;
    check_cu(cuDeviceTotalMem_v2(&vram_bytes, dev), "cuDeviceTotalMem_v2");

    void *ctx = NULL;
    check_cu(cuCtxCreate_v2(&ctx, 0U, dev), "cuCtxCreate_v2");

    if (test_bad_ptx) {
        void *bad_mod = NULL;
        const char *bad_ptx =
            ".version 8.7\n.target sm_120\n.address_size 64\n"
            ".visible .entry bad_k() { BOGUS_BLACKWELL_OPCODE %r0; ret; }\n";
        int brc = cuModuleLoadData(&bad_mod, bad_ptx);
        const char *ename = "UNKNOWN";
        cuGetErrorName(brc, &ename);
        if (brc == 0) {
            fprintf(stderr, "EF-0 FAILURE: nvcuda accepted malformed PTX!\n");
            cuModuleUnload(bad_mod);
            cuCtxDestroy_v2(ctx);
            return 0;
        }
        printf("GPU_BAD_PTX_REJECTED rc=%d name=%s\n", brc, ename ? ename : "ERR");
        fflush(stdout);
        cuCtxDestroy_v2(ctx);
        return 1;
    }

    printf("GPU_DEVICE name=\"%s\" cc=%d.%d vram_mib=%u driver=%d ptx_bytes=%u\n",
           dev_name, cc_maj, cc_min, (unsigned int)(vram_bytes >> 20), drv_ver,
           (unsigned int)sizeof(g_gpu_kernels_ptx) - 1U);

    void *mod = NULL;
    char jit_err[4096];
    memset(jit_err, 0, sizeof(jit_err));
    int jit_opts[2] = { 5, 6 }; /* CU_JIT_ERROR_LOG_BUFFER, CU_JIT_ERROR_LOG_BUFFER_SIZE_BYTES */
    void *jit_vals[2] = { (void *)jit_err, (void *)(uintptr_t)sizeof(jit_err) };
    int load_rc = cuModuleLoadDataEx(&mod, (const void *)g_gpu_kernels_ptx, 2U, jit_opts, jit_vals);
    if (load_rc != 0) {
        if (jit_err[0] != '\0') {
            fprintf(stderr, "NV PTX JIT error log:\n%s\n", jit_err);
        }
        check_cu(load_rc, "cuModuleLoadDataEx");
    }

    void *fn_saxpy = NULL, *fn_matmul = NULL, *fn_bitops = NULL, *fn_mma = NULL, *fn_render = NULL;
    check_cu(cuModuleGetFunction(&fn_saxpy, mod, "gpu_saxpy_fma"), "cuModuleGetFunction(gpu_saxpy_fma)");
    check_cu(cuModuleGetFunction(&fn_matmul, mod, "gpu_matmul_f32"), "cuModuleGetFunction(gpu_matmul_f32)");
    check_cu(cuModuleGetFunction(&fn_bitops, mod, "gpu_blackwell_bitops"), "cuModuleGetFunction(gpu_blackwell_bitops)");
    check_cu(cuModuleGetFunction(&fn_mma, mod, "gpu_tensor_mma_f16"), "cuModuleGetFunction(gpu_tensor_mma_f16)");
    check_cu(cuModuleGetFunction(&fn_render, mod, "gpu_render_scene"), "cuModuleGetFunction(gpu_render_scene)");

    void *ev0 = NULL, *ev1 = NULL;
    check_cu(cuEventCreate(&ev0, 0U), "cuEventCreate(0)");
    check_cu(cuEventCreate(&ev1, 0U), "cuEventCreate(1)");

    /* 1. 1M-element SAXPY_FMA */
    {
        unsigned int n = 1048576U;
        size_t sz = (size_t)n * sizeof(float);
        float *hx = (float *)malloc(sz);
        float *hy = (float *)malloc(sz);
        float *href = (float *)malloc(sz);
        float a = 2.25f, b = -0.75f;
        for (unsigned int i = 0; i < n; i++) {
            hx[i] = (float)((i % 97U) + 1U) * 0.125f;
            hy[i] = (float)((i % 53U) + 1U) * 0.25f;
            href[i] = a * hx[i] + b * hy[i];
        }
        uint64_t dx = 0, dy = 0;
        check_cu(cuMemAlloc_v2(&dx, sz), "cuMemAlloc(dx)");
        check_cu(cuMemAlloc_v2(&dy, sz), "cuMemAlloc(dy)");
        check_cu(cuMemcpyHtoD_v2(dx, hx, sz), "HtoD(dx)");
        check_cu(cuMemcpyHtoD_v2(dy, hy, sz), "HtoD(dy)");
        void *params[5] = { &dx, &dy, &a, &b, &n };
        check_cu(cuEventRecord(ev0, NULL), "ev0");
        check_cu(cuLaunchKernel(fn_saxpy, n / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, params, NULL), "launch_saxpy");
        check_cu(cuEventRecord(ev1, NULL), "ev1");
        check_cu(cuEventSynchronize(ev1), "evSync");
        float ms = 0.0f;
        check_cu(cuEventElapsedTime(&ms, ev0, ev1), "evElapsed");
        check_cu(cuMemcpyDtoH_v2(hy, dy, sz), "DtoH(dy)");
        double max_err = 0.0;
        for (unsigned int i = 0; i < n; i++) {
            double d = fabs((double)hy[i] - (double)href[i]);
            if (d > max_err) max_err = d;
        }
        if (max_err > 1e-4) {
            fprintf(stderr, "SAXPY error too high: %.8f\n", max_err);
            return 1;
        }
        printf("GPU_SAXPY_OK n=%u gpu_ms=%.4f max_err=%.8f\n", n, (double)ms, max_err);
        cuMemFree_v2(dx); cuMemFree_v2(dy);
        free(hx); free(hy); free(href);
    }

    /* 2. 256x256 FP32 Matrix Multiply */
    {
        unsigned int N = 256U;
        size_t sz = (size_t)N * (size_t)N * sizeof(float);
        float *ha = (float *)malloc(sz);
        float *hb = (float *)malloc(sz);
        float *hc = (float *)malloc(sz);
        for (unsigned int i = 0; i < N * N; i++) {
            ha[i] = (float)((int)(i % 19U) - 9) * 0.125f;
            hb[i] = (float)((int)(i % 23U) - 11) * 0.0625f;
        }
        uint64_t da = 0, db = 0, dc = 0;
        check_cu(cuMemAlloc_v2(&da, sz), "alloc_da");
        check_cu(cuMemAlloc_v2(&db, sz), "alloc_db");
        check_cu(cuMemAlloc_v2(&dc, sz), "alloc_dc");
        check_cu(cuMemcpyHtoD_v2(da, ha, sz), "HtoD(da)");
        check_cu(cuMemcpyHtoD_v2(db, hb, sz), "HtoD(db)");
        void *params[4] = { &da, &db, &dc, &N };
        check_cu(cuEventRecord(ev0, NULL), "ev0");
        check_cu(cuLaunchKernel(fn_matmul, N / 16U, N / 16U, 1U, 16U, 16U, 1U, 0U, NULL, params, NULL), "launch_matmul");
        check_cu(cuEventRecord(ev1, NULL), "ev1");
        check_cu(cuEventSynchronize(ev1), "evSync");
        float ms = 0.0f;
        check_cu(cuEventElapsedTime(&ms, ev0, ev1), "evElapsed");
        check_cu(cuMemcpyDtoH_v2(hc, dc, sz), "DtoH(dc)");
        double max_err = 0.0;
        for (unsigned int r = 0; r < N; r += 7U) {
            for (unsigned int c = 0; c < N; c += 5U) {
                double ref = 0.0;
                for (unsigned int k = 0; k < N; k++) ref += (double)ha[r * N + k] * (double)hb[k * N + c];
                double d = fabs((double)hc[r * N + c] - ref);
                if (d > max_err) max_err = d;
            }
        }
        if (max_err > 1e-2) {
            fprintf(stderr, "MATMUL error too high: %.8f\n", max_err);
            return 1;
        }
        double flops = 2.0 * (double)N * (double)N * (double)N;
        double gflops = (ms > 0.0f) ? (flops / ((double)ms * 1e6)) : 0.0;
        printf("GPU_MATMUL_OK N=%u gpu_ms=%.4f gflops=%.2f max_err=%.8f\n", N, (double)ms, gflops, max_err);
        cuMemFree_v2(da); cuMemFree_v2(db); cuMemFree_v2(dc);
        free(ha); free(hb); free(hc);
    }

    /* 3. Blackwell Bitops (LOP3 + SHF.R.WRAP + POPC + CLZ) */
    {
        unsigned int n = 65536U;
        size_t sz = (size_t)n * sizeof(uint32_t);
        uint32_t *ha = (uint32_t *)malloc(sz);
        uint32_t *hb = (uint32_t *)malloc(sz);
        uint32_t *hc = (uint32_t *)malloc(sz);
        uint32_t *och = (uint32_t *)malloc(sz);
        uint32_t *omaj = (uint32_t *)malloc(sz);
        uint32_t *oxor = (uint32_t *)malloc(sz);
        uint32_t *orot = (uint32_t *)malloc(sz);
        uint32_t *opop = (uint32_t *)malloc(sz);
        uint32_t *oclz = (uint32_t *)malloc(sz);
        uint32_t s = 0x13579BDFU;
        for (unsigned int i = 0; i < n; i++) {
            s = s * 1664525U + 1013904223U; ha[i] = (i == 0U) ? 0U : s;
            s = s * 1664525U + 1013904223U; hb[i] = s;
            s = s * 1664525U + 1013904223U; hc[i] = s;
        }
        uint64_t da = 0, db = 0, dc = 0, dch = 0, dmaj = 0, dxor = 0, drot = 0, dpop = 0, dclz = 0;
        check_cu(cuMemAlloc_v2(&da, sz), "alloc"); check_cu(cuMemAlloc_v2(&db, sz), "alloc");
        check_cu(cuMemAlloc_v2(&dc, sz), "alloc"); check_cu(cuMemAlloc_v2(&dch, sz), "alloc");
        check_cu(cuMemAlloc_v2(&dmaj, sz), "alloc"); check_cu(cuMemAlloc_v2(&dxor, sz), "alloc");
        check_cu(cuMemAlloc_v2(&drot, sz), "alloc"); check_cu(cuMemAlloc_v2(&dpop, sz), "alloc");
        check_cu(cuMemAlloc_v2(&dclz, sz), "alloc");
        check_cu(cuMemcpyHtoD_v2(da, ha, sz), "HtoD");
        check_cu(cuMemcpyHtoD_v2(db, hb, sz), "HtoD");
        check_cu(cuMemcpyHtoD_v2(dc, hc, sz), "HtoD");
        void *params[10] = { &da, &db, &dc, &dch, &dmaj, &dxor, &drot, &dpop, &dclz, &n };
        check_cu(cuLaunchKernel(fn_bitops, n / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, params, NULL), "launch_bitops");
        check_cu(cuCtxSynchronize(), "sync_bitops");
        check_cu(cuMemcpyDtoH_v2(och, dch, sz), "DtoH");
        check_cu(cuMemcpyDtoH_v2(omaj, dmaj, sz), "DtoH");
        check_cu(cuMemcpyDtoH_v2(oxor, dxor, sz), "DtoH");
        check_cu(cuMemcpyDtoH_v2(orot, drot, sz), "DtoH");
        check_cu(cuMemcpyDtoH_v2(opop, dpop, sz), "DtoH");
        check_cu(cuMemcpyDtoH_v2(oclz, dclz, sz), "DtoH");
        int mismatches = 0;
        for (unsigned int i = 0; i < n; i++) {
            uint32_t va = ha[i], vb = hb[i], vc = hc[i];
            if (och[i] != ((va & vb) ^ (~va & vc))) mismatches++;
            if (omaj[i] != ((va & vb) ^ (va & vc) ^ (vb & vc))) mismatches++;
            if (oxor[i] != (va ^ vb ^ vc)) mismatches++;
            if (orot[i] != cpu_rotr32(va, 7U)) mismatches++;
            if (opop[i] != cpu_popc32(va)) mismatches++;
            if (oclz[i] != cpu_clz32(va)) mismatches++;
        }
        if (mismatches != 0) {
            fprintf(stderr, "BITOPS mismatches=%d\n", mismatches);
            return 1;
        }
        printf("GPU_BITOPS_OK n=%u mismatches=0\n", n);
        cuMemFree_v2(da); cuMemFree_v2(db); cuMemFree_v2(dc);
        cuMemFree_v2(dch); cuMemFree_v2(dmaj); cuMemFree_v2(dxor);
        cuMemFree_v2(drot); cuMemFree_v2(dpop); cuMemFree_v2(dclz);
        free(ha); free(hb); free(hc);
        free(och); free(omaj); free(oxor); free(orot); free(opop); free(oclz);
    }

    /* 4. Tensor Core SM 12.0 MMA (m16n8k16 f16 -> f32) */
    {
        uint16_t a_f16[16 * 16];
        uint16_t b_f16[8 * 16];
        float d_f32[16 * 8];
        for (int r = 0; r < 16; r++) {
            for (int k = 0; k < 16; k++) {
                float v = (float)(((r * 3 + k) % 7) - 3) * 0.5f;
                a_f16[r * 16 + k] = f32_to_f16_simple(v);
            }
        }
        for (int c = 0; c < 8; c++) {
            for (int k = 0; k < 16; k++) {
                float v = (float)(((c * 5 + k) % 5) - 2) * 0.5f;
                b_f16[c * 16 + k] = f32_to_f16_simple(v);
            }
        }
        memset(d_f32, 0, sizeof(d_f32));
        uint64_t da = 0, db = 0, dd = 0;
        check_cu(cuMemAlloc_v2(&da, sizeof(a_f16)), "alloc_mma_a");
        check_cu(cuMemAlloc_v2(&db, sizeof(b_f16)), "alloc_mma_b");
        check_cu(cuMemAlloc_v2(&dd, sizeof(d_f32)), "alloc_mma_d");
        check_cu(cuMemcpyHtoD_v2(da, a_f16, sizeof(a_f16)), "HtoD_mma_a");
        check_cu(cuMemcpyHtoD_v2(db, b_f16, sizeof(b_f16)), "HtoD_mma_b");
        void *params[3] = { &da, &db, &dd };
        check_cu(cuLaunchKernel(fn_mma, 1U, 1U, 1U, 32U, 1U, 1U, 0U, NULL, params, NULL), "launch_mma");
        check_cu(cuCtxSynchronize(), "sync_mma");
        check_cu(cuMemcpyDtoH_v2(d_f32, dd, sizeof(d_f32)), "DtoH_mma");
        double max_err = 0.0;
        for (int r = 0; r < 16; r++) {
            for (int c = 0; c < 8; c++) {
                double ref = 0.0;
                for (int k = 0; k < 16; k++) {
                    ref += (double)f16_to_f32_simple(a_f16[r * 16 + k]) *
                           (double)f16_to_f32_simple(b_f16[c * 16 + k]);
                }
                double e = fabs((double)d_f32[r * 8 + c] - ref);
                if (e > max_err) max_err = e;
            }
        }
        if (max_err > 1e-2) {
            fprintf(stderr, "TENSOR_MMA error too high: %.8f\n", max_err);
            return 1;
        }
        printf("GPU_TENSOR_MMA_OK shape=16x8x16 d00=%.4f max_err=%.8f\n", (double)d_f32[0], max_err);
        cuMemFree_v2(da); cuMemFree_v2(db); cuMemFree_v2(dd);
    }

    /* 5. Shader Modes Self-Check */
    {
        int w = 320, h = 200;
        size_t sz = (size_t)w * (size_t)h * sizeof(uint32_t);
        uint32_t *fb = (uint32_t *)malloc(sz);
        uint64_t dfb = 0;
        check_cu(cuMemAlloc_v2(&dfb, sz), "alloc_fb");
        int u[3];
        uint32_t fnv[3];
        for (int m = 0; m < 3; m++) {
            float t = 1.25f, cx = 0.0f, cy = 0.0f, zm = 1.0f;
            void *params[8] = { &dfb, &w, &h, &m, &t, &cx, &cy, &zm };
            check_cu(cuLaunchKernel(fn_render, (unsigned int)((w + 15) / 16), (unsigned int)((h + 15) / 16), 1U,
                                    16U, 16U, 1U, 0U, NULL, params, NULL), "launch_render");
            check_cu(cuCtxSynchronize(), "sync_render");
            check_cu(cuMemcpyDtoH_v2(fb, dfb, sz), "DtoH_fb");
            u[m] = count_unique_u32(fb, w * h);
            fnv[m] = fnv1a_u32(fb, w * h);
            if (u[m] < 32) {
                fprintf(stderr, "Shader mode %d low color diversity: %d\n", m, u[m]);
                return 1;
            }
        }
        printf("GPU_SHADERS_OK m0=(%d,%08x) m1=(%d,%08x) m2=(%d,%08x)\n",
               u[0], fnv[0], u[1], fnv[1], u[2], fnv[2]);
        cuMemFree_v2(dfb);
        free(fb);
    }

    cuEventDestroy_v2(ev0);
    cuEventDestroy_v2(ev1);
    cuModuleUnload(mod);
    cuCtxDestroy_v2(ctx);
    printf("GPU_COMPUTE_ALL_OK\n");
    fflush(stdout);
    return 0;
}
