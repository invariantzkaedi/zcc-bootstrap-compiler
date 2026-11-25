/*
 * ZKAEDI ZCC // Sovereign Win64 Live NVIDIA RTX 5070 GPU Shader Window
 * Direct PE32+ imports: nvcuda.dll + USER32.dll + GDI32.dll + KERNEL32.dll + msvcrt.dll
 * Zero MSVC, zero MinGW, zero nvcc, zero CUDA Toolkit.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "gpu_kernels_ptx.h"

/* CUDA Driver API -> System32\nvcuda.dll */
extern int cuInit(unsigned int Flags);
extern int cuDeviceGetCount(int *count);
extern int cuDeviceGet(int *device, int ordinal);
extern int cuDeviceGetName(char *name, int len, int dev);
extern int cuDeviceGetAttribute(int *pi, int attrib, int dev);
extern int cuCtxCreate_v2(void **pctx, unsigned int flags, int dev);
extern int cuCtxDestroy_v2(void *ctx);
extern int cuCtxSynchronize(void);
extern int cuModuleLoadData(void **module, const void *image);
extern int cuModuleUnload(void *hmod);
extern int cuModuleGetFunction(void **hfunc, void *hmod, const char *name);
extern int cuMemAlloc_v2(uint64_t *dptr, size_t bytesize);
extern int cuMemFree_v2(uint64_t dptr);
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

/* Win32 API -> KERNEL32 / USER32 / GDI32 */
typedef long long (*WNDPROC_GPULIVE)(void *, unsigned int, unsigned long long, long long);

typedef struct {
    unsigned int cbSize;
    unsigned int style;
    WNDPROC_GPULIVE lpfnWndProc;
    int cbClsExtra;
    int cbWndExtra;
    void *hInstance;
    void *hIcon;
    void *hCursor;
    void *hbrBackground;
    const char *lpszMenuName;
    const char *lpszClassName;
    void *hIconSm;
} GPU_WNDCLASSEXA;

typedef struct {
    int left;
    int top;
    int right;
    int bottom;
} GPU_RECT;

typedef struct {
    int x;
    int y;
} GPU_POINT;

typedef struct {
    void *hwnd;
    unsigned int message;
    uint64_t wParam;
    int64_t lParam;
    unsigned int time;
    GPU_POINT pt;
} GPU_MSG;

typedef struct {
    void *hdc;
    int fErase;
    GPU_RECT rcPaint;
    int fRestore;
    int fIncUpdate;
    unsigned char rgbReserved[32];
} GPU_PAINTSTRUCT;

typedef struct {
    unsigned int biSize;
    int biWidth;
    int biHeight;
    unsigned short biPlanes;
    unsigned short biBitCount;
    unsigned int biCompression;
    unsigned int biSizeImage;
    int biXPelsPerMeter;
    int biYPelsPerMeter;
    unsigned int biClrUsed;
    unsigned int biClrImportant;
} GPU_BITMAPINFOHEADER;

typedef struct {
    GPU_BITMAPINFOHEADER bmiHeader;
    unsigned int bmiColors[1];
} GPU_BITMAPINFO;

extern void *GetModuleHandleA(const char *lpModuleName);
extern unsigned int GetTickCount(void);
extern void Sleep(unsigned int dwMilliseconds);
extern unsigned short RegisterClassExA(const GPU_WNDCLASSEXA *lpwcx);
extern void *CreateWindowExA(
    unsigned int dwExStyle, const char *lpClassName, const char *lpWindowName,
    unsigned int dwStyle, int X, int Y, int nWidth, int nHeight,
    void *hWndParent, void *hMenu, void *hInstance, void *lpParam
);
extern int ShowWindow(void *hWnd, int nCmdShow);
extern int UpdateWindow(void *hWnd);
extern int DestroyWindow(void *hWnd);
extern int64_t DefWindowProcA(void *hWnd, unsigned int Msg, uint64_t wParam, int64_t lParam);
extern void PostQuitMessage(int nExitCode);
extern int PeekMessageA(GPU_MSG *lpMsg, void *hWnd, unsigned int wMsgFilterMin, unsigned int wMsgFilterMax, unsigned int wRemoveMsg);
extern int TranslateMessage(const GPU_MSG *lpMsg);
extern int64_t DispatchMessageA(const GPU_MSG *lpMsg);
extern void *GetDC(void *hWnd);
extern int ReleaseDC(void *hWnd, void *hDC);
extern void *BeginPaint(void *hWnd, GPU_PAINTSTRUCT *lpPaint);
extern int EndPaint(void *hWnd, const GPU_PAINTSTRUCT *lpPaint);
extern int GetClientRect(void *hWnd, GPU_RECT *lpRect);
extern int SetWindowTextA(void *hWnd, const char *lpString);
extern void *LoadCursorA(void *hInstance, const char *lpCursorName);
extern void *GetStockObject(int i);
extern int StretchDIBits(
    void *hdc, int xDest, int yDest, int DestWidth, int DestHeight,
    int xSrc, int ySrc, int SrcWidth, int SrcHeight,
    const void *lpBits, const GPU_BITMAPINFO *lpbmi, unsigned int iUsage, unsigned int rop
);

static int g_live_quit = 0;
static int g_live_mode = 1;
static int g_live_paused = 0;
static float g_cam_x = 0.0f;
static float g_cam_y = 0.0f;
static float g_zoom = 1.0f;

static uint32_t *g_host_fb = NULL;
static int g_fb_w = 640;
static int g_fb_h = 400;
static GPU_BITMAPINFO g_bmi;
static int g_blit_ok = 0;

static void blit_fb_to_hdc(void *hwnd, void *hdc) {
    if (!g_host_fb || !hdc) return;
    GPU_RECT rc;
    int dst_w = 960, dst_h = 600;
    if (hwnd && GetClientRect(hwnd, &rc)) {
        if (rc.right - rc.left > 0) dst_w = rc.right - rc.left;
        if (rc.bottom - rc.top > 0) dst_h = rc.bottom - rc.top;
    }
    int r = StretchDIBits(hdc, 0, 0, dst_w, dst_h, 0, 0, g_fb_w, g_fb_h,
                          (const void *)g_host_fb, &g_bmi, 0U, 0x00CC0020U);
    if (r > 0) g_blit_ok = 1;
}

static long long gpu_wnd_proc(void *hwnd, unsigned int msg, unsigned long long wp, long long lp) {
    if (msg == 0x0010U || msg == 0x0002U) {
        g_live_quit = 1;
        PostQuitMessage(0);
        return 0;
    } else if (msg == 0x000FU) {
        GPU_PAINTSTRUCT ps;
        void *hdc = BeginPaint(hwnd, &ps);
        blit_fb_to_hdc(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    } else if (msg == 0x0100U) {
        if (wp == 0x1BU) { g_live_quit = 1; PostQuitMessage(0); }
        else if (wp == '1') { g_live_mode = 0; g_cam_x = 0.0f; g_cam_y = 0.0f; g_zoom = 1.0f; }
        else if (wp == '2') { g_live_mode = 1; g_cam_x = 0.0f; g_cam_y = 0.0f; g_zoom = 1.0f; }
        else if (wp == '3') { g_live_mode = 2; g_cam_x = 0.0f; g_cam_y = 0.0f; g_zoom = 1.0f; }
        else if (wp == ' ') { g_live_paused = !g_live_paused; }
        else if (wp == 'R') { g_cam_x = 0.0f; g_cam_y = 0.0f; g_zoom = 1.0f; }
        else if (wp == 'A' || wp == 0x25U) { g_cam_x -= 0.08f / g_zoom; }
        else if (wp == 'D' || wp == 0x27U) { g_cam_x += 0.08f / g_zoom; }
        else if (wp == 'W' || wp == 0x26U) { g_cam_y += 0.08f / g_zoom; }
        else if (wp == 'S' || wp == 0x28U) { g_cam_y -= 0.08f / g_zoom; }
        else if (wp == 'E' || wp == 0xBBU) { g_zoom *= 1.12f; }
        else if (wp == 'Q' || wp == 0xBDU) { g_zoom /= 1.12f; if (g_zoom < 0.25f) g_zoom = 0.25f; }
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static uint32_t fnv1a_rgb_dib(const uint32_t *fb, int n) {
    uint32_t h = 2166136261U;
    for (int i = 0; i < n; i++) {
        uint32_t px = fb[i];
        h ^= ((px >> 16) & 0xFFU); h *= 16777619U;
        h ^= ((px >> 8) & 0xFFU); h *= 16777619U;
        h ^= (px & 0xFFU); h *= 16777619U;
    }
    return h;
}

static int count_unique_colors(const uint32_t *fb, int n) {
    static uint32_t seen[512];
    int used = 0;
    for (int i = 0; i < n; i += 3) {
        uint32_t c = fb[i] & 0x00FFFFFFU;
        int f = 0;
        for (int j = 0; j < used; j++) if (seen[j] == c) { f = 1; break; }
        if (!f) {
            if (used < 512) seen[used++] = c;
            else break;
        }
    }
    return used;
}

static int dump_ppm(const char *path, const uint32_t *fb, int w, int h) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    fprintf(fp, "P6\n%d %d\n255\n", w, h);
    uint8_t *row = (uint8_t *)malloc((size_t)w * 3U);
    if (!row) { fclose(fp); return -1; }
    for (int y = 0; y < h; y++) {
        const uint32_t *src = fb + y * w;
        for (int x = 0; x < w; x++) {
            uint32_t px = src[x];
            row[x * 3 + 0] = (uint8_t)((px >> 16) & 0xFFU);
            row[x * 3 + 1] = (uint8_t)((px >> 8) & 0xFFU);
            row[x * 3 + 2] = (uint8_t)(px & 0xFFU);
        }
        fwrite(row, 1U, (size_t)w * 3U, fp);
    }
    free(row);
    fclose(fp);
    return 0;
}

int main(int argc, char **argv) {
    int headless = 0;
    int auto_close_frames = 0;
    int frames = 10;
    int w_arg = 0, h_arg = 0;
    const char *ppm_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--headless") == 0) headless = 1;
        else if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) g_live_mode = atoi(argv[++i]);
        else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) frames = atoi(argv[++i]);
        else if (strcmp(argv[i], "--auto-close-frames") == 0 && i + 1 < argc) auto_close_frames = atoi(argv[++i]);
        else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) w_arg = atoi(argv[++i]);
        else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) h_arg = atoi(argv[++i]);
        else if (strcmp(argv[i], "--dump-ppm") == 0 && i + 1 < argc) ppm_path = argv[++i];
    }
    if (g_live_mode < 0 || g_live_mode > 2) g_live_mode = 1;
    if (frames < 1) frames = 1;
    if (headless) {
        g_fb_w = (w_arg >= 64) ? w_arg : 320;
        g_fb_h = (h_arg >= 64) ? h_arg : 200;
    } else {
        g_fb_w = (w_arg >= 64) ? w_arg : 640;
        g_fb_h = (h_arg >= 64) ? h_arg : 400;
    }

    if (cuInit(0U) != 0) { fprintf(stderr, "cuInit failed\n"); return 1; }
    int dev_cnt = 0, dev = 0;
    cuDeviceGetCount(&dev_cnt);
    if (dev_cnt <= 0 || cuDeviceGet(&dev, 0) != 0) { fprintf(stderr, "No CUDA device\n"); return 1; }
    char gpu_name[128];
    memset(gpu_name, 0, sizeof(gpu_name));
    cuDeviceGetName(gpu_name, (int)sizeof(gpu_name) - 1, dev);
    int cc_maj = 0, cc_min = 0;
    cuDeviceGetAttribute(&cc_maj, 75, dev);
    cuDeviceGetAttribute(&cc_min, 76, dev);

    void *ctx = NULL, *mod = NULL, *kfn = NULL, *ev0 = NULL, *ev1 = NULL;
    if (cuCtxCreate_v2(&ctx, 0U, dev) != 0) { fprintf(stderr, "cuCtxCreate failed\n"); return 1; }
    if (cuModuleLoadData(&mod, (const void *)g_gpu_kernels_ptx) != 0) {
        fprintf(stderr, "cuModuleLoadData failed\n");
        cuCtxDestroy_v2(ctx);
        return 1;
    }
    if (cuModuleGetFunction(&kfn, mod, "gpu_render_scene") != 0) {
        fprintf(stderr, "cuModuleGetFunction(gpu_render_scene) failed\n");
        cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
        return 1;
    }
    cuEventCreate(&ev0, 0U);
    cuEventCreate(&ev1, 0U);

    size_t fb_bytes = (size_t)g_fb_w * (size_t)g_fb_h * sizeof(uint32_t);
    g_host_fb = (uint32_t *)malloc(fb_bytes);
    uint64_t d_fb = 0;
    if (!g_host_fb || cuMemAlloc_v2(&d_fb, fb_bytes) != 0) {
        fprintf(stderr, "GPU framebuffer alloc failed\n");
        return 1;
    }

    unsigned int gx = (unsigned int)((g_fb_w + 15) / 16);
    unsigned int gy = (unsigned int)((g_fb_h + 15) / 16);

    if (headless) {
        float last_ms = 0.0f;
        for (int f = 0; f < frames; f++) {
            float t = (float)(f + 1) * 0.08f;
            void *params[8] = { &d_fb, &g_fb_w, &g_fb_h, &g_live_mode, &t, &g_cam_x, &g_cam_y, &g_zoom };
            cuEventRecord(ev0, NULL);
            if (cuLaunchKernel(kfn, gx, gy, 1U, 16U, 16U, 1U, 0U, NULL, params, NULL) != 0) {
                fprintf(stderr, "cuLaunchKernel failed\n");
                return 1;
            }
            cuEventRecord(ev1, NULL);
            cuEventSynchronize(ev1);
            cuEventElapsedTime(&last_ms, ev0, ev1);
        }
        cuMemcpyDtoH_v2(g_host_fb, d_fb, fb_bytes);
        if (ppm_path && dump_ppm(ppm_path, g_host_fb, g_fb_w, g_fb_h) != 0) {
            fprintf(stderr, "Failed to write PPM %s\n", ppm_path);
            return 1;
        }
        int uniq = count_unique_colors(g_host_fb, g_fb_w * g_fb_h);
        uint32_t fnv = fnv1a_rgb_dib(g_host_fb, g_fb_w * g_fb_h);
        printf("GPU_LIVE_OK gpu=\"%s\" cc=%d.%d mode=%d frames=%d gpu_ms=%.4f unique_colors=%d fnv=%08x\n",
               gpu_name, cc_maj, cc_min, g_live_mode, frames, (double)last_ms, uniq, fnv);
        fflush(stdout);
        cuMemFree_v2(d_fb);
        cuEventDestroy_v2(ev0); cuEventDestroy_v2(ev1);
        cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
        free(g_host_fb);
        return 0;
    }

    memset(&g_bmi, 0, sizeof(g_bmi));
    g_bmi.bmiHeader.biSize = (unsigned int)sizeof(GPU_BITMAPINFOHEADER);
    g_bmi.bmiHeader.biWidth = g_fb_w;
    g_bmi.bmiHeader.biHeight = -g_fb_h;
    g_bmi.bmiHeader.biPlanes = 1;
    g_bmi.bmiHeader.biBitCount = 32;
    g_bmi.bmiHeader.biCompression = 0U;

    void *hinst = GetModuleHandleA(NULL);
    GPU_WNDCLASSEXA wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = (unsigned int)sizeof(GPU_WNDCLASSEXA);
    wc.style = 0x0023U;
    wc.lpfnWndProc = gpu_wnd_proc;
    wc.hInstance = hinst;
    wc.hCursor = LoadCursorA(NULL, (const char *)(uintptr_t)32512);
    wc.hbrBackground = GetStockObject(4);
    wc.lpszClassName = "ZCC_GPU_PTX_LIVE";
    RegisterClassExA(&wc);

    void *hwnd = CreateWindowExA(
        0U, "ZCC_GPU_PTX_LIVE",
        "ZCC Sovereign Win64 GPU Shader (Zero-NVCC / Direct nvcuda.dll)",
        0x10CF0000U, 100, 80, 976, 639,
        NULL, NULL, hinst, NULL
    );
    if (!hwnd) {
        fprintf(stderr, "CreateWindowExA failed\n");
        return 1;
    }
    ShowWindow(hwnd, 5);
    UpdateWindow(hwnd);

    int rendered = 0;
    float sim_t = 0.0f;
    float last_ms = 0.0f;
    unsigned int t_fps0 = GetTickCount();
    int fps_frames = 0;
    const char *mnames[3] = { "Julia Morph", "3D SDF Raymarch", "Hamiltonian Waves" };

    while (!g_live_quit) {
        GPU_MSG msg;
        while (PeekMessageA(&msg, NULL, 0U, 0U, 1U)) {
            if (msg.message == 0x0012U) g_live_quit = 1;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (g_live_quit) break;

        if (!g_live_paused) sim_t += 0.03f;
        void *params[8] = { &d_fb, &g_fb_w, &g_fb_h, &g_live_mode, &sim_t, &g_cam_x, &g_cam_y, &g_zoom };
        cuEventRecord(ev0, NULL);
        cuLaunchKernel(kfn, gx, gy, 1U, 16U, 16U, 1U, 0U, NULL, params, NULL);
        cuEventRecord(ev1, NULL);
        cuEventSynchronize(ev1);
        cuEventElapsedTime(&last_ms, ev0, ev1);
        cuMemcpyDtoH_v2(g_host_fb, d_fb, fb_bytes);

        void *hdc = GetDC(hwnd);
        if (hdc) {
            blit_fb_to_hdc(hwnd, hdc);
            ReleaseDC(hwnd, hdc);
        }
        rendered++;
        fps_frames++;

        unsigned int now = GetTickCount();
        if (now - t_fps0 >= 400U) {
            float fps = (float)fps_frames * 1000.0f / (float)(now - t_fps0);
            char title[256];
            snprintf(title, sizeof(title),
                     "ZCC Sovereign GPU [%s] | Mode %d: %s | GPU %.2f ms | %.1f FPS | Keys: 1-3/WASD/QE/Space/Esc",
                     gpu_name, g_live_mode + 1, mnames[g_live_mode], (double)last_ms, (double)fps);
            SetWindowTextA(hwnd, title);
            t_fps0 = now;
            fps_frames = 0;
        }

        if (auto_close_frames > 0 && rendered >= auto_close_frames) break;
        Sleep(1U);
    }

    if (ppm_path) dump_ppm(ppm_path, g_host_fb, g_fb_w, g_fb_h);
    int uniq = count_unique_colors(g_host_fb, g_fb_w * g_fb_h);
    uint32_t fnv = fnv1a_rgb_dib(g_host_fb, g_fb_w * g_fb_h);
    int hwnd_ok = hwnd ? 1 : 0;
    DestroyWindow(hwnd);
    cuMemFree_v2(d_fb);
    cuEventDestroy_v2(ev0); cuEventDestroy_v2(ev1);
    cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
    free(g_host_fb);
    printf("GPU_LIVE_GUI_OK hwnd_ok=%d blit_ok=%d frames=%d gpu_ms=%.4f unique_colors=%d fnv=%08x\n",
           hwnd_ok, g_blit_ok, rendered, (double)last_ms, uniq, fnv);
    fflush(stdout);
    return 0;
}
