#include <stdio.h>
#include <stdlib.h>
#include "zcaedi_avx512_math.h"

#define WIDTH 640
#define HEIGHT 480

static unsigned char framebuffer[HEIGHT][WIDTH][3];
static float zbuffer[HEIGHT * WIDTH];
int pixels_drawn = 0;

static inline float fminf_s(float a, float b) { return a < b ? a : b; }
static inline float fmaxf_s(float a, float b) { return a > b ? a : b; }



typedef struct { float x, y, z; } vec3f;
static inline vec3f v_create(float x, float y, float z) { vec3f r; r.x=x; r.y=y; r.z=z; return r; }

static inline float edge_func(vec3f a, vec3f b, vec3f c) {
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

typedef struct {
    unsigned char r, g, b;
} Color;

static inline v16f edge_func_avx512(v16f ax, v16f ay, v16f bx, v16f by, v16f px, v16f py) {
    return _mm512_sub_ps(
        _mm512_mul_ps(_mm512_sub_ps(px, ax), _mm512_sub_ps(by, ay)),
        _mm512_mul_ps(_mm512_sub_ps(py, ay), _mm512_sub_ps(bx, ax))
    );
}

static void draw_triangle_avx512(vec3f v0, vec3f v1, vec3f v2, Color col) {
    int minx = (int)fmaxf_s(0.0f, fminf_s(v0.x, fminf_s(v1.x, v2.x)));
    int miny = (int)fmaxf_s(0.0f, fminf_s(v0.y, fminf_s(v1.y, v2.y)));
    int maxx = (int)fminf_s(WIDTH - 1.0f, fmaxf_s(v0.x, fmaxf_s(v1.x, v2.x)));
    int maxy = (int)fminf_s(HEIGHT - 1.0f, fmaxf_s(v0.y, fmaxf_s(v1.y, v2.y)));

    float area = edge_func(v0, v1, v2);
    if (area <= 0.0f) { 
        printf("Debug: Triangle culled (area %f)\n", area);
        return;
    }
    printf("Debug: Drawing triangle, area=%f, bounds=(%d,%d)-(%d,%d)\n", area, minx, miny, maxx, maxy);

    v16f v0x = _mm512_set1_ps(v0.x);
    v16f v0y = _mm512_set1_ps(v0.y);
    v16f v1x = _mm512_set1_ps(v1.x);
    v16f v1y = _mm512_set1_ps(v1.y);
    v16f v2x = _mm512_set1_ps(v2.x);
    v16f v2y = _mm512_set1_ps(v2.y);
    v16f inv_area = _mm512_set1_ps(1.0f / area);
    v16f zero = _mm512_setzero_ps();

    v16f z0 = _mm512_set1_ps(v0.z);
    v16f z1 = _mm512_set1_ps(v1.z);
    v16f z2 = _mm512_set1_ps(v2.z);

    v16f x_offsets = _mm512_set_ps(15.f, 14.f, 13.f, 12.f, 11.f, 10.f, 9.f, 8.f, 7.f, 6.f, 5.f, 4.f, 3.f, 2.f, 1.f, 0.f);

    for (int y = miny; y <= maxy; y++) {
        v16f py = _mm512_set1_ps((float)y + 0.5f);
        
        for (int x = minx; x <= maxx; x += 16) {
            v16f px = _mm512_add_ps(_mm512_set1_ps((float)x + 0.5f), x_offsets);
            
            v16i x_mask = _mm512_cmp_ps_mask_v16i(px, _mm512_set1_ps((float)WIDTH), 1);
            if (_mm512_movepi32_mask(x_mask) == 0) continue;

            v16f w0 = edge_func_avx512(v1x, v1y, v2x, v2y, px, py);
            v16f w1 = edge_func_avx512(v2x, v2y, v0x, v0y, px, py);
            v16f w2 = edge_func_avx512(v0x, v0y, v1x, v1y, px, py);

            v16i m0 = _mm512_cmp_ps_mask_ge(w0, zero);
            v16i m1 = _mm512_cmp_ps_mask_ge(w1, zero);
            v16i m2 = _mm512_cmp_ps_mask_ge(w2, zero);
            
            v16i mask = _mm512_and_epi32(m0, _mm512_and_epi32(m1, _mm512_and_epi32(m2, x_mask)));

            if (_mm512_movepi32_mask(mask)) {
                w0 = _mm512_mul_ps(w0, inv_area);
                w1 = _mm512_mul_ps(w1, inv_area);
                w2 = _mm512_mul_ps(w2, inv_area);

                v16f z = _mm512_fmadd_ps(w0, z0, _mm512_fmadd_ps(w1, z1, _mm512_mul_ps(w2, z2)));

                int idx = y * WIDTH + x;
                
                v16f old_z = _mm512_maskz_loadu_ps(mask, &zbuffer[idx]);
                
                v16i z_mask = _mm512_mask_cmp_ps_mask(mask, z, old_z, 1);

                unsigned int m_val = _mm512_movepi32_mask(z_mask);
                if (m_val) {
                    _mm512_mask_storeu_ps(&zbuffer[idx], z_mask, z);
                    
                    while (m_val) {
                        int bit = 0;
                        unsigned int temp = m_val;
                        while ((temp & 1) == 0) {
                            temp >>= 1;
                            bit++;
                        }
                        
                        m_val &= ~(1U << bit);
                        if (x + bit < WIDTH) {
                            pixels_drawn++;
                            framebuffer[y][x + bit][0] = col.r;
                            framebuffer[y][x + bit][1] = col.g;
                            framebuffer[y][x + bit][2] = col.b;
                        }
                    }
                }
            }
        }
    }
}



int main(void) {
    printf("ZKAEDI PRIME SOVEREIGN RASTERIZER (Exp18) initializing...\n");

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            framebuffer[y][x][0] = 0;
            framebuffer[y][x][1] = 0;
            framebuffer[y][x][2] = 0;
            zbuffer[y * WIDTH + x] = 1000.0f;
        }
    }

    vec3f verts[8] = {
        {-1,-1,-1}, { 1,-1,-1}, { 1, 1,-1}, {-1, 1,-1},
        {-1,-1, 1}, { 1,-1, 1}, { 1, 1, 1}, {-1, 1, 1}
    };

    int indices[36] = {
        0,1,2, 0,2,3, 1,5,6, 1,6,2, 5,4,7, 5,7,6, 
        4,0,3, 4,3,7, 3,2,6, 3,6,7, 4,5,1, 4,1,0
    };

    Color colors[6] = {
        {255,0,0}, {0,255,0}, {0,0,255},
        {255,255,0}, {255,0,255}, {0,255,255}
    };

    // Precomputed camera angles for zero-dependency Sovereign engine
    float cx = 0.877582f, sx = 0.479425f;
    float cy = 0.764842f, sy = 0.644217f;
    printf("Debug: Camera projection matrix initialized.\n");

    vec3f proj_verts[8];
    for (int i = 0; i < 8; i++) {
        vec3f v = verts[i];
        float x1 = v.x * cy + v.z * sy;
        float z1 = -v.x * sy + v.z * cy;
        float y2 = v.y * cx - z1 * sx;
        float z2 = v.y * sx + z1 * cx + 4.0f;
        float px = (x1 / z2) * 400.0f + (WIDTH / 2.0f);
        float py = (y2 / z2) * 400.0f + (HEIGHT / 2.0f);
        proj_verts[i] = v_create(px, py, z2);
    }

    for (int i = 0; i < 36; i+=3) {
        vec3f v0 = proj_verts[indices[i]];
        vec3f v1 = proj_verts[indices[i+1]];
        vec3f v2 = proj_verts[indices[i+2]];
        
        Color c = colors[i/6];
        float depth = (v0.z + v1.z + v2.z) / 3.0f;
        float intensity = 1.0f / (depth * 0.2f);
        if(intensity > 1.0f) intensity = 1.0f;
        c.r = (unsigned char)(c.r * intensity);
        c.g = (unsigned char)(c.g * intensity);
        c.b = (unsigned char)(c.b * intensity);

        draw_triangle_avx512(v0, v1, v2, c);
    }

    unsigned int hash = 0;
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            hash ^= (framebuffer[y][x][0] << 16) | (framebuffer[y][x][1] << 8) | framebuffer[y][x][2];
            hash = (hash << 1) | (hash >> 31);
        }
    }

    printf("Exp18 Pixels Drawn: %d\n", pixels_drawn);
    printf("Exp18 Rasterizer Verification Hash: 0x%08X\n", hash);
    printf("Sovereign Rasterizer Established. Zero External Dependencies linked.\n");

    return 0;
}
