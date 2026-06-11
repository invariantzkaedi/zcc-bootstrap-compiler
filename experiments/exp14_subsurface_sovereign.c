/* EXPERIMENT 14-SOVEREIGN: Subsurface Scattering Simulator
 * 
 * 777JACKPOT777 LUCKY UPGRADE
 * - Vectorized 1D Gaussian blur passes (processing 16 pixels per cycle).
 * - Fast unaligned loads for horizontal taps, aligned block loads for vertical taps.
 * - SSSS profile separation optimized for throughput.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zcaedi_avx512_math.h"

extern float sqrtf(float);

#define WIDTH 512
#define HEIGHT 512

void draw_circle(float *buf, int cx, int cy, int r, float intensity) {
    for(int y=-r; y<=r; y++) {
        for(int x=-r; x<=r; x++) {
            if(x*x + y*y <= r*r) {
                int px = cx + x;
                int py = cy + y;
                if(px>=0 && px<WIDTH && py>=0 && py<HEIGHT) {
                    buf[py * WIDTH + px] = intensity;
                }
            }
        }
    }
}

// 1D Gaussian Blur Pass Sovereign
void blur_pass_avx512(float *src, float *dst, int dx, int dy, float weight) {
    // 5-tap kernel
    float kernel[5] = {0.06136f, 0.24477f, 0.38774f, 0.24477f, 0.06136f};
    int offset = 2;
    
    v16f v_weight = _mm512_set1_ps(weight);
    v16f k0 = _mm512_set1_ps(kernel[0]);
    v16f k1 = _mm512_set1_ps(kernel[1]);
    v16f k2 = _mm512_set1_ps(kernel[2]);
    v16f k3 = _mm512_set1_ps(kernel[3]);
    v16f k4 = _mm512_set1_ps(kernel[4]);
    v16f k_arr[5] = {k0, k1, k2, k3, k4};
    
    // We process 16 pixels horizontally at once.
    // If dx != 0 (horizontal pass), we load src + y*WIDTH + x + i*dx
    // If dy != 0 (vertical pass), we load src + (y + i*dy)*WIDTH + x
    for(int y=0; y<HEIGHT; y++) {
        for(int x=0; x<WIDTH; x+=16) {
            v16f sum = _mm512_setzero_ps();
            
            for(int i=-offset; i<=offset; i++) {
                int py = y + i * dy;
                // clamp y
                if (py < 0) py = 0;
                if (py >= HEIGHT) py = HEIGHT - 1;
                
                int px_base = x + i * dx;
                v16f tap;
                
                // If the entire 16-wide load is within x bounds
                if (px_base >= 0 && px_base + 15 < WIDTH) {
                    tap = _mm512_loadu_ps(&src[py * WIDTH + px_base]);
                } else {
                    // Slow path for x edge cases (clamping)
                    float tmp[16];
                    for(int k=0; k<16; k++) {
                        int px = px_base + k;
                        if(px < 0) px = 0;
                        if(px >= WIDTH) px = WIDTH - 1;
                        tmp[k] = src[py * WIDTH + px];
                    }
                    tap = _mm512_loadu_ps(tmp);
                }
                
                sum = _mm512_fmadd_ps(tap, k_arr[i + offset], sum);
            }
            
            v16f out_val = _mm512_loadu_ps(&dst[y * WIDTH + x]);
            out_val = _mm512_fmadd_ps(sum, v_weight, out_val);
            _mm512_storeu_ps(&dst[y * WIDTH + x], out_val);
        }
    }
}

int main(void) {
    float *base_img = malloc(WIDTH * HEIGHT * sizeof(float));
    float *temp_buf = malloc(WIDTH * HEIGHT * sizeof(float));
    float *scatter_r = malloc(WIDTH * HEIGHT * sizeof(float));
    float *scatter_g = malloc(WIDTH * HEIGHT * sizeof(float));
    float *scatter_b = malloc(WIDTH * HEIGHT * sizeof(float));
    unsigned char (*fb)[WIDTH][3] = malloc(HEIGHT * WIDTH * 3);
    
    memset(base_img, 0, WIDTH * HEIGHT * sizeof(float));
    memset(scatter_r, 0, WIDTH * HEIGHT * sizeof(float));
    memset(scatter_g, 0, WIDTH * HEIGHT * sizeof(float));
    memset(scatter_b, 0, WIDTH * HEIGHT * sizeof(float));
    
    fprintf(stderr, "Simulating Subsurface Scattering Exp14 Sovereign...\n");
    // Draw hard shapes
    draw_circle(base_img, WIDTH/2, HEIGHT/2, 100, 1.0f);
    draw_circle(base_img, WIDTH/2 + 80, HEIGHT/2 + 80, 50, 0.8f);
    
    // Red Pass (Wide blur)
    memset(temp_buf, 0, WIDTH*HEIGHT*sizeof(float));
    blur_pass_avx512(base_img, temp_buf, 4, 0, 1.0f); // Horizontal
    blur_pass_avx512(temp_buf, scatter_r, 0, 4, 1.0f); // Vertical
    
    // Green Pass (Medium blur)
    memset(temp_buf, 0, WIDTH*HEIGHT*sizeof(float));
    blur_pass_avx512(base_img, temp_buf, 2, 0, 1.0f);
    blur_pass_avx512(temp_buf, scatter_g, 0, 2, 1.0f);
    
    // Blue Pass (Tight blur)
    memset(temp_buf, 0, WIDTH*HEIGHT*sizeof(float));
    blur_pass_avx512(base_img, temp_buf, 1, 0, 1.0f);
    blur_pass_avx512(temp_buf, scatter_b, 0, 1, 1.0f);
    
    v16f limit = _mm512_set1_ps(1.0f);
    v16f scale = _mm512_set1_ps(255.0f);
    v16f zero = _mm512_setzero_ps();
    
    for(int y=0; y<HEIGHT; y++) {
        for(int x=0; x<WIDTH; x+=16) {
            int idx = y * WIDTH + x;
            
            v16f r = _mm512_loadu_ps(&scatter_r[idx]);
            v16f g = _mm512_loadu_ps(&scatter_g[idx]);
            v16f b = _mm512_loadu_ps(&scatter_b[idx]);
            
            r = _mm512_min_ps(_mm512_max_ps(r, zero), limit);
            g = _mm512_min_ps(_mm512_max_ps(g, zero), limit);
            b = _mm512_min_ps(_mm512_max_ps(b, zero), limit);
            
            r = _mm512_mul_ps(r, scale);
            g = _mm512_mul_ps(g, scale);
            b = _mm512_mul_ps(b, scale);
            
            float r_a[16], g_a[16], b_a[16];
            _mm512_storeu_ps(r_a, r);
            _mm512_storeu_ps(g_a, g);
            _mm512_storeu_ps(b_a, b);
            
            for(int k=0; k<16; k++) {
                if(x+k < WIDTH) {
                    fb[y][x+k][0] = (unsigned char)r_a[k];
                    fb[y][x+k][1] = (unsigned char)g_a[k];
                    fb[y][x+k][2] = (unsigned char)b_a[k];
                }
            }
        }
    }
    
    // Hash calculations
    unsigned int hash = 0;
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            hash ^= (fb[y][x][0] << 16) | (fb[y][x][1] << 8) | fb[y][x][2];
            hash = (hash << 1) | (hash >> 31);
        }
    }
    fprintf(stderr, "Exp14 Subsurface Verification Hash: 0x%08X\n", hash);

    printf("P6\n%d %d\n255\n", WIDTH, HEIGHT);
    for (int y = 0; y < HEIGHT; y++) fwrite(fb[y], 1, WIDTH * 3, stdout);
    
    free(base_img); free(temp_buf); free(scatter_r); free(scatter_g); free(scatter_b); free(fb);
    fprintf(stderr, "Exp14 Sovereign done!\n");
    return 0;
}
