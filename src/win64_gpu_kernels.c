/*
 * ZKAEDI ZCC // Sovereign C99 GPU Kernels (Compiled via `./zcc --target=ptx`)
 * Zero CUDA headers, zero nvcc. Lowered directly to NVIDIA PTX ISA 8.7 (sm_120).
 */

unsigned int __ptx_tid_x(void);
unsigned int __ptx_tid_y(void);
unsigned int __ptx_ntid_x(void);
unsigned int __ptx_ntid_y(void);
unsigned int __ptx_ctaid_x(void);
unsigned int __ptx_ctaid_y(void);
float __ptx_sqrtf(float x);
float __ptx_rsqrtf(float x);
float __ptx_sinf(float x);
float __ptx_cosf(float x);
float __ptx_ex2f(float x);
float __ptx_fabsf(float x);
float __ptx_minf(float a, float b);
float __ptx_maxf(float a, float b);
float __ptx_fmaf(float a, float b, float c);
float __ptx_floorf(float x);
unsigned int __ptx_lop3(unsigned int a, unsigned int b, unsigned int c, int imm);
unsigned int __ptx_rotr32(unsigned int x, unsigned int k);
unsigned int __ptx_popc32(unsigned int x);
unsigned int __ptx_clz32(unsigned int x);
void __ptx_mma_m16n8k16_f16(const unsigned short *a, const unsigned short *b, float *d);

static float clamp01(float x) {
    return __ptx_minf(1.0f, __ptx_maxf(0.0f, x));
}

static unsigned int pack_rgb(float r, float g, float b) {
    int ri = (int)(clamp01(r) * 255.0f + 0.5f);
    int gi = (int)(clamp01(g) * 255.0f + 0.5f);
    int bi = (int)(clamp01(b) * 255.0f + 0.5f);
    return ((unsigned int)ri << 16) | ((unsigned int)gi << 8) | (unsigned int)bi;
}

static float sd_sphere(float px, float py, float pz, float rad) {
    return __ptx_sqrtf(px * px + py * py + pz * pz) - rad;
}

static float sd_torus(float px, float py, float pz, float rmaj, float rmin) {
    float qx = __ptx_sqrtf(px * px + pz * pz) - rmaj;
    return __ptx_sqrtf(qx * qx + py * py) - rmin;
}

static float map_scene(float px, float py, float pz, float t, int *out_mat) {
    float cz = pz - 3.2f;
    float ox = __ptx_cosf(t * 0.9f) * 1.15f;
    float oz = __ptx_sinf(t * 0.9f) * 0.85f;
    float d_sph = sd_sphere(px - ox * 0.35f, py - 0.12f * __ptx_sinf(t * 1.7f), cz - oz * 0.35f, 0.58f);

    float ca = __ptx_cosf(t * 0.8f);
    float sa = __ptx_sinf(t * 0.8f);
    float rx = px;
    float ry = py * ca - cz * sa;
    float rz = py * sa + cz * ca;
    float d_tor = sd_torus(rx, ry, rz, 1.02f, 0.22f);

    float d_flr = py + 0.95f;

    float d = d_sph;
    int mat = 1;
    if (d_tor < d) {
        d = d_tor;
        mat = 2;
    }
    if (d_flr < d) {
        d = d_flr;
        mat = 3;
    }
    if (out_mat) *out_mat = mat;
    return d;
}

void gpu_saxpy_fma(const float *x, float *y, float a, float b, unsigned int n) {
    unsigned int idx = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (idx < n) {
        y[idx] = __ptx_fmaf(a, x[idx], b * y[idx]);
    }
}

void gpu_matmul_f32(const float *A, const float *B, float *C, unsigned int N) {
    unsigned int col = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    unsigned int row = __ptx_ctaid_y() * __ptx_ntid_y() + __ptx_tid_y();
    if (row < N && col < N) {
        float acc = 0.0f;
        unsigned int rbase = row * N;
        for (unsigned int k = 0; k < N; k++) {
            acc = __ptx_fmaf(A[rbase + k], B[k * N + col], acc);
        }
        C[rbase + col] = acc;
    }
}

void gpu_blackwell_bitops(
    const unsigned int *a,
    const unsigned int *b,
    const unsigned int *c,
    unsigned int *out_ch,
    unsigned int *out_maj,
    unsigned int *out_xor,
    unsigned int *out_rot,
    unsigned int *out_popc,
    unsigned int *out_clz,
    unsigned int n
) {
    unsigned int idx = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (idx < n) {
        unsigned int va = a[idx];
        unsigned int vb = b[idx];
        unsigned int vc = c[idx];
        out_ch[idx] = __ptx_lop3(va, vb, vc, 0xCA);
        out_maj[idx] = __ptx_lop3(va, vb, vc, 0xE8);
        out_xor[idx] = __ptx_lop3(va, vb, vc, 0x96);
        out_rot[idx] = __ptx_rotr32(va, 7U);
        out_popc[idx] = __ptx_popc32(va);
        out_clz[idx] = __ptx_clz32(va);
    }
}

void gpu_tensor_mma_f16(const unsigned short *a_f16, const unsigned short *b_f16, float *d_f32) {
    if (__ptx_ctaid_x() == 0 && __ptx_tid_x() < 32U) {
        __ptx_mma_m16n8k16_f16(a_f16, b_f16, d_f32);
    }
}

void gpu_render_scene(
    unsigned int *fb,
    int width,
    int height,
    int mode,
    float t,
    float cam_x,
    float cam_y,
    float zoom
) {
    int x = (int)(__ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x());
    int y = (int)(__ptx_ctaid_y() * __ptx_ntid_y() + __ptx_tid_y());
    if (x >= width || y >= height) return;

    float aspect = (float)width / (float)height;
    float u = (((float)x + 0.5f) / (float)width) * 2.0f - 1.0f;
    float v = 1.0f - (((float)y + 0.5f) / (float)height) * 2.0f;
    u *= aspect;

    float r = 0.0f, g = 0.0f, b = 0.0f;

    if (mode == 0) {
        /* Neon Animated Julia / Mandelbrot Morph */
        float scale = 1.45f / zoom;
        float zx = u * scale + cam_x;
        float zy = v * scale + cam_y;
        float cx = -0.745f + 0.14f * __ptx_cosf(t * 0.55f);
        float cy = 0.186f + 0.14f * __ptx_sinf(t * 0.72f);
        int iter = 0;
        float m2 = 0.0f;
        float trap = 100.0f;
        while (iter < 64) {
            float x2 = zx * zx;
            float y2 = zy * zy;
            m2 = x2 + y2;
            if (m2 > 16.0f) break;
            float nx = x2 - y2 + cx;
            float ny = 2.0f * zx * zy + cy;
            zx = nx;
            zy = ny;
            float td = __ptx_fabsf(zx * zx - zy * zy);
            if (td < trap) trap = td;
            iter++;
        }
        if (iter >= 64) {
            float glow = __ptx_ex2f(-trap * 6.0f);
            r = 0.05f + 0.35f * glow;
            g = 0.12f + 0.70f * glow;
            b = 0.25f + 0.95f * glow;
        } else {
            float phase = (float)iter * 0.16f + t * 0.6f;
            r = 0.5f + 0.5f * __ptx_cosf(phase + 0.0f);
            g = 0.5f + 0.5f * __ptx_cosf(phase + 2.094f);
            b = 0.5f + 0.5f * __ptx_cosf(phase + 4.188f);
        }
    } else if (mode == 1) {
        /* Real-Time 3D Raymarched SDF Neon Scene */
        float rox = cam_x * 1.4f + 0.45f * __ptx_sinf(t * 0.35f);
        float roy = 0.35f + cam_y * 1.1f;
        float roz = 1.2f - 0.8f * (zoom - 1.0f);
        float inv_len = __ptx_rsqrtf(u * u + v * v + 2.25f);
        float rdx = u * inv_len;
        float rdy = v * inv_len;
        float rdz = 1.5f * inv_len;

        float dist = 0.0f;
        int hit = 0;
        int mat = 0;
        for (int s = 0; s < 42; s++) {
            float px = rox + rdx * dist;
            float py = roy + rdy * dist;
            float pz = roz + rdz * dist;
            int m = 0;
            float d = map_scene(px, py, pz, t, &m);
            if (d < 0.004f) {
                hit = 1;
                mat = m;
                break;
            }
            dist += d;
            if (dist > 18.0f) break;
        }

        if (hit) {
            float hx = rox + rdx * dist;
            float hy = roy + rdy * dist;
            float hz = roz + rdz * dist;
            float eps = 0.004f;
            float nx = map_scene(hx + eps, hy, hz, t, (int *)0) - map_scene(hx - eps, hy, hz, t, (int *)0);
            float ny = map_scene(hx, hy + eps, hz, t, (int *)0) - map_scene(hx, hy - eps, hz, t, (int *)0);
            float nz = map_scene(hx, hy, hz + eps, t, (int *)0) - map_scene(hx, hy, hz - eps, t, (int *)0);
            float ninv = __ptx_rsqrtf(nx * nx + ny * ny + nz * nz + 1e-8f);
            nx *= ninv; ny *= ninv; nz *= ninv;

            /* Key directional light */
            float lx = 0.577f, ly = 0.72f, lz = -0.39f;
            float diff = __ptx_maxf(0.0f, nx * lx + ny * ly + nz * lz);

            /* Hard shadow check */
            float sh = 1.0f;
            float sd_t = 0.04f;
            for (int k = 0; k < 16; k++) {
                float dd = map_scene(hx + lx * sd_t, hy + ly * sd_t, hz + lz * sd_t, t, (int *)0);
                if (dd < 0.005f) { sh = 0.22f; break; }
                sd_t += dd;
                if (sd_t > 5.0f) break;
            }

            float rim = 1.0f - __ptx_maxf(0.0f, -(nx * rdx + ny * rdy + nz * rdz));
            float rim2 = rim * rim * rim;
            float hvx = lx - rdx, hvy = ly - rdy, hvz = lz - rdz;
            float hinv = __ptx_rsqrtf(hvx * hvx + hvy * hvy + hvz * hvz + 1e-8f);
            float spec = __ptx_maxf(0.0f, (nx * hvx + ny * hvy + nz * hvz) * hinv);
            spec = spec * spec;
            spec = spec * spec;
            spec = spec * spec;

            float ar = 0.0f, ag = 0.0f, ab = 0.0f;
            if (mat == 1) {
                ar = 0.95f; ag = 0.22f; ab = 0.72f;
            } else if (mat == 2) {
                ar = 0.12f; ag = 0.92f; ab = 0.98f;
            } else {
                int cx = (int)__ptx_floorf(hx * 1.4f + 64.0f);
                int cz = (int)__ptx_floorf(hz * 1.4f + 64.0f);
                int chk = (cx + cz) & 1;
                ar = chk ? 0.24f : 0.07f;
                ag = chk ? 0.28f : 0.10f;
                ab = chk ? 0.40f : 0.16f;
            }

            float shade = (0.16f + 0.84f * diff * sh);
            r = ar * shade + 0.85f * spec * sh + 0.20f * rim2;
            g = ag * shade + 0.90f * spec * sh + 0.65f * rim2;
            b = ab * shade + 1.00f * spec * sh + 0.95f * rim2;

            float fog = __ptx_ex2f(-0.045f * dist * dist);
            r = r * fog + 0.04f * (1.0f - fog);
            g = g * fog + 0.07f * (1.0f - fog);
            b = b * fog + 0.15f * (1.0f - fog);
        } else {
            float sky = clamp01(v * 0.5f + 0.5f);
            r = 0.03f + 0.06f * (1.0f - sky);
            g = 0.05f + 0.10f * sky;
            b = 0.14f + 0.26f * sky;
        }
    } else {
        /* ZKAEDI PRIME Coupled 2D Hamiltonian Vortex Field */
        float x0 = u / zoom + cam_x;
        float y0 = v / zoom + cam_y;
        float r1 = __ptx_sqrtf((x0 - 0.6f * __ptx_cosf(t)) * (x0 - 0.6f * __ptx_cosf(t)) +
                               (y0 - 0.5f * __ptx_sinf(t * 1.1f)) * (y0 - 0.5f * __ptx_sinf(t * 1.1f)) + 0.01f);
        float r2 = __ptx_sqrtf((x0 + 0.6f * __ptx_cosf(t * 0.8f)) * (x0 + 0.6f * __ptx_cosf(t * 0.8f)) +
                               (y0 + 0.5f * __ptx_sinf(t * 0.9f)) * (y0 + 0.5f * __ptx_sinf(t * 0.9f)) + 0.01f);
        float w = __ptx_sinf(r1 * 12.0f - t * 3.0f) / (1.0f + r1)
                + __ptx_cosf(r2 * 12.0f - t * 2.4f) / (1.0f + r2)
                + 0.45f * __ptx_sinf((x0 * 4.5f + y0 * 3.2f) + t * 1.8f);
        float e = 0.5f + 0.5f * __ptx_sinf(w * 2.5f + 0.4f * __ptx_cosf(x0 * 6.0f));
        float ridge = __ptx_ex2f(-__ptx_fabsf(w) * 4.0f);
        r = 0.10f + 0.85f * e * e + 0.85f * ridge;
        g = 0.08f + 0.65f * __ptx_sinf(e * 3.14159f) + 0.90f * ridge;
        b = 0.30f + 0.70f * (1.0f - e) + 0.55f * ridge;
    }

    fb[y * width + x] = pack_rgb(r, g, b);
}
