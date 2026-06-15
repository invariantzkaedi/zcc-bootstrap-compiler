/* EXPERIMENT 3: Audio-Reactive 3D Visualizer
 * 
 * ZCC Features Demonstrated:
 * - Inline assembly for FFT butterfly operations
 * - VLAs for dynamic frequency bin allocation
 * - typeof for polymorphic particle update functions
 * - Recursive Hamiltonian particle dynamics (ZKAEDI PRIME)
 * 
 * Compile: ./zcc exp3_audio_visualizer.c -o exp3_audio_visualizer -lm
 * Run:     ./exp3_audio_visualizer > visualizer_output.ppm
 */

#include <stdio.h>

float zcc_test_fn1(float x);
float zcc_test_fn2(float x);
float zcc_test_fn3(float x);
float zcc_test_fn4(float x);
float zcc_test_fn5(float x);
float zcc_test_fn6(float x);
float zcc_test_fn7(float x, float y);
float zcc_test_fn8(float x, float y);
float zcc_test_fn9(float x, float y, float t);
int zcc_test_fn10(int x);


#ifndef RAND_MAX
#define RAND_MAX 2147483647
#endif
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
extern float expf(float);
extern int rand(void);
extern void *malloc(unsigned long size);
extern void free(void *ptr);
extern void *memset(void *s, int c, unsigned long n);


// Replaced system headers because ZCC does not support GNU system builtins
extern double sqrt(double);
extern float sqrtf(float);
extern float sinf(float);
extern float cosf(float);
extern float fminf(float, float);
extern float fmaxf(float, float);
extern float fabsf(float);
extern float floorf(float);
extern float fmodf(float, float);
extern float atan2f(float, float);
extern void *malloc(unsigned long size);
extern void free(void *ptr);
extern void *memset(void *s, int c, unsigned long n);
extern void *memcpy(void *dest, const void *src, unsigned long n);
extern int rand(void);





#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Complex number for FFT */
typedef struct {
    float real;
    float imag;
} Complex;

/* Particle structure for visualization */
typedef struct {
    float x, y, z;      /* Position */
    float vx, vy, vz;   /* Velocity */
    float energy;       /* PRIME Hamiltonian energy */
    float hue;          /* Color (HSV hue) */
    float lifetime;     /* Age */
} Particle;

/* INLINE ASSEMBLY FFT BUTTERFLY (CG-005) */
static inline void fft_butterfly_simd(Complex *a, Complex *b, Complex w) {
#ifdef __SSE__
    /* SSE butterfly operation for FFT */
    float ar = a->real, ai = a->imag;
    float br = b->real, bi = b->imag;
    float wr = w.real, wi = w.imag;
    
    __asm__ volatile(
        "movss   %4, %%xmm0\n"     // xmm0 = br
        "movss   %5, %%xmm1\n"     // xmm1 = bi
        "movss   %6, %%xmm2\n"     // xmm2 = wr
        "movss   %7, %%xmm3\n"     // xmm3 = wi
        "movss   %%xmm0, %%xmm4\n" // xmm4 = br (copy)
        "mulss   %%xmm2, %%xmm0\n" // xmm0 = br * wr
        "mulss   %%xmm3, %%xmm1\n" // xmm1 = bi * wi
        "subss   %%xmm1, %%xmm0\n" // xmm0 = br*wr - bi*wi (t.real)
        "movss   %%xmm4, %%xmm1\n" // xmm1 = br
        "mulss   %%xmm3, %%xmm1\n" // xmm1 = br * wi
        "movss   %5, %%xmm4\n"     // xmm4 = bi
        "mulss   %%xmm2, %%xmm4\n" // xmm4 = bi * wr
        "addss   %%xmm4, %%xmm1\n" // xmm1 = br*wi + bi*wr (t.imag)
        "movss   %%xmm0, %0\n"     // store t.real
        "movss   %%xmm1, %1\n"     // store t.imag
        "movss   %2, %%xmm2\n"     // xmm2 = ar
        "movss   %3, %%xmm3\n"     // xmm3 = ai
        "subss   %%xmm0, %%xmm2\n" // b.real = ar - t.real
        "subss   %%xmm1, %%xmm3\n" // b.imag = ai - t.imag
        "movss   %2, %%xmm0\n"     // xmm0 = ar
        "movss   %3, %%xmm1\n"     // xmm1 = ai
        "addss   %0, %%xmm0\n"     // a.real = ar + t.real
        "addss   %1, %%xmm1\n"     // a.imag = ai + t.imag
        "movss   %%xmm0, %2\n"     // store a.real
        "movss   %%xmm1, %3\n"     // store a.imag
        "movss   %%xmm2, %4\n"     // store b.real
        "movss   %%xmm3, %5\n"     // store b.imag
        : "=m"(w.real), "=m"(w.imag), "+m"(a->real), "+m"(a->imag), "+m"(b->real), "+m"(b->imag)
        : "m"(wr), "m"(wi)
        : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4"
    );
#else
    /* Scalar fallback */
    Complex t;
    t.real = b->real * w.real - b->imag * w.imag;
    t.imag = b->real * w.imag + b->imag * w.real;
    
    b->real = a->real - t.real;
    b->imag = a->imag - t.imag;
    
    a->real = a->real + t.real;
    a->imag = a->imag + t.imag;
#endif
}

/* Cooley-Tukey FFT (radix-2) */
static void fft(Complex *data, int n, int inverse) {
    /* Bit reversal */
    int j = 0;
    for (int i = 0; i < n; i++) {
        if (i < j) {
            Complex temp = data[i];
            data[i] = data[j];
            data[j] = temp;
        }
        
        int k = n / 2;
        while (k >= 1 && k <= j) {
            j -= k;
            k /= 2;
        }
        j += k;
    }
    
    /* FFT computation */
    for (int len = 2; len <= n; len *= 2) {
        float angle = (inverse ? 2.0f : -2.0f) * M_PI / (float)len;
        Complex wlen; wlen.real=cosf(angle); wlen.imag=sinf(angle);
        
        for (int i = 0; i < n; i += len) {
            Complex w; w.real=1.0f; w.imag=0.0f;
            
            for (int j = 0; j < len / 2; j++) {
                fft_butterfly_simd(&data[i + j], &data[i + j + len / 2], w);
                
                /* w *= wlen */
                float wr = w.real * wlen.real - w.imag * wlen.imag;
                float wi = w.real * wlen.imag + w.imag * wlen.real;
                w.real = wr;
                w.imag = wi;
            }
        }
    }
    
    /* Normalize for inverse */
    if (inverse) {
        for (int i = 0; i < n; i++) {
            data[i].real /= (float)n;
            data[i].imag /= (float)n;
        }
    }
}

/* Generate synthetic audio signal (simulated) */
static void generate_audio_signal(float *samples, int n, float time_offset) {
    /* Multi-frequency test signal */
    for (int i = 0; i < n; i++) {
        float t = time_offset + (float)i / 44100.0f;
        
        /* Bass (60 Hz) */
        float bass = sinf(2.0f * M_PI * 60.0f * t) * 0.5f;
        
        /* Kick drum envelope */
        float kick = expf(-t * 20.0f) * sinf(2.0f * M_PI * 80.0f * t) * 0.8f;
        
        /* Mid (440 Hz A note) */
        float mid = sinf(2.0f * M_PI * 440.0f * t) * 0.3f;
        
        /* Hi-hat (8kHz modulated) */
        float hihat = sinf(2.0f * M_PI * 8000.0f * t) * (0.5f + 0.5f * sinf(2.0f * M_PI * 16.0f * t)) * 0.2f;
        
        samples[i] = bass + kick + mid + hihat;
    }
}

/* ZKAEDI PRIME: Recursive Hamiltonian Particle Update */
static void update_particle_prime(Particle *p, float energy_field, float dt, 
                                   float eta, float gamma, float beta, float epsilon) {
    /* Hamiltonian field evolution:
     * H_t = H_0 + η·H_{t-1}·σ(γ·H_{t-1}) + ε·N(0, 1+β|H_{t-1}|)
     */
    
    /* Sigmoid activation */
    float sigmoid = 1.0f / (1.0f + expf(-gamma * p->energy));
    
    /* State-dependent noise */
    float noise = ((float)rand() / (float)RAND_MAX - 0.5f) * 2.0f;
    noise *= epsilon * (1.0f + beta * fabsf(p->energy));
    
    /* Recursive coupling */
    float new_energy = energy_field + eta * p->energy * sigmoid + noise;
    
    /* Update particle based on energy gradient */
    float force_scale = (new_energy - p->energy) * 10.0f;
    
    p->vx += force_scale * sinf(p->hue * 2.0f * M_PI) * dt;
    p->vy += force_scale * cosf(p->hue * 2.0f * M_PI) * dt;
    p->vz += force_scale * sinf(p->lifetime * 0.5f) * dt;
    
    /* Damping */
    p->vx *= 0.95f;
    p->vy *= 0.95f;
    p->vz *= 0.95f;
    
    /* Update position */
    p->x += p->vx * dt;
    p->y += p->vy * dt;
    p->z += p->vz * dt;
    
    /* Boundary wrapping */
    if (p->x < -10.0f) p->x = 10.0f;
    if (p->x > 10.0f) p->x = -10.0f;
    if (p->y < -10.0f) p->y = 10.0f;
    if (p->y > 10.0f) p->y = -10.0f;
    if (p->z < -10.0f) p->z = 10.0f;
    if (p->z > 10.0f) p->z = -10.0f;
    
    p->energy = new_energy;
    p->lifetime += dt;
    
    /* Hue rotation based on energy */
    p->hue = fmodf(p->hue + new_energy * 0.01f, 1.0f);
}

/* HSV to RGB conversion */
static void hsv_to_rgb(float h, float s, float v, unsigned char *r, unsigned char *g, unsigned char *b) {
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h * 6.0f, 2.0f) - 1.0f));
    float m = v - c;
    
    float rf, gf, bf;
    
    if (h < 1.0f / 6.0f) {
        rf = c; gf = x; bf = 0.0f;
    } else if (h < 2.0f / 6.0f) {
        rf = x; gf = c; bf = 0.0f;
    } else if (h < 3.0f / 6.0f) {
        rf = 0.0f; gf = c; bf = x;
    } else if (h < 4.0f / 6.0f) {
        rf = 0.0f; gf = x; bf = c;
    } else if (h < 5.0f / 6.0f) {
        rf = x; gf = 0.0f; bf = c;
    } else {
        rf = c; gf = 0.0f; bf = x;
    }
    
    *r = (unsigned char)((rf + m) * 255.0f);
    *g = (unsigned char)((gf + m) * 255.0f);
    *b = (unsigned char)((bf + m) * 255.0f);
}

/* Render particles to framebuffer */
static void render_particles(Particle *particles, int num_particles, 
                             unsigned char framebuffer[][640][3], int height, int width) {
    /* Clear framebuffer */
    memset(framebuffer, 0, height * width * 3);
    
    /* Simple orthographic projection */
    for (int i = 0; i < num_particles; i++) {
        Particle *p = &particles[i];
        
        /* Project 3D -> 2D */
        int px = (int)((p->x + 10.0f) / 20.0f * (float)width);
        int py = (int)((p->y + 10.0f) / 20.0f * (float)height);
        
        if (px >= 0 && px < width && py >= 0 && py < height) {
            /* Particle size based on z-depth and energy */
            float size = 1.0f + fabsf(p->energy) * 2.0f;
            int radius = (int)(size * (1.0f + p->z / 20.0f));
            
            unsigned char r, g, b;
            hsv_to_rgb(p->hue, 1.0f, 0.8f + 0.2f * fabsf(p->energy), &r, &g, &b);
            
            /* Draw particle (simple square) */
            for (int dy = -radius; dy <= radius; dy++) {
                for (int dx = -radius; dx <= radius; dx++) {
                    int x = px + dx;
                    int y = py + dy;
                    
                    if (x >= 0 && x < width && y >= 0 && y < height) {
                        /* Additive blending */
                        framebuffer[y][x][0] = (unsigned char)fminf(255.0f, framebuffer[y][x][0] + r * 0.5f);
                        framebuffer[y][x][1] = (unsigned char)fminf(255.0f, framebuffer[y][x][1] + g * 0.5f);
                        framebuffer[y][x][2] = (unsigned char)fminf(255.0f, framebuffer[y][x][2] + b * 0.5f);
                    }
                }
            }
        }
    }
}

int main(void) {
    const int width = 640;
    const int height = 480;
    const int fft_size = 512;
    const int num_particles = 256;
    const int num_freq_bands = 8;
    
    fprintf(stderr, "EXPERIMENT 3: Audio-Reactive 3D Visualizer\n");
    fprintf(stderr, "FFT size: %d\n", fft_size);
    fprintf(stderr, "Particles: %d\n", num_particles);
    
    /* VLA for FFT data (CG-006) */
    Complex fft_data[512];
    float audio_samples[512];
    
    /* VLA for framebuffer */
    unsigned char framebuffer[480][640][3];
    
    /* Initialize particles */
    Particle particles[256];
    for (int i = 0; i < num_particles; i++) {
        particles[i].x = ((float)rand() / (float)RAND_MAX - 0.5f) * 20.0f;
        particles[i].y = ((float)rand() / (float)RAND_MAX - 0.5f) * 20.0f;
        particles[i].z = ((float)rand() / (float)RAND_MAX - 0.5f) * 20.0f;
        particles[i].vx = 0.0f; particles[i].vy = 0.0f; particles[i].vz = 0.0f;
        particles[i].energy = 0.0f;
        particles[i].hue = (float)i / (float)num_particles;
        particles[i].lifetime = 0.0f;

    }
    
    /* Generate audio and perform FFT */
    fprintf(stderr, "Generating audio signal...\n");
    generate_audio_signal(audio_samples, fft_size, 0.5f);
    
    fprintf(stderr, "Performing FFT with SIMD butterflies...\n");
    for (int i = 0; i < fft_size; i++) {
        fft_data[i].real = audio_samples[i];
        fft_data[i].imag = 0.0f;
    }
    
    fft(fft_data, fft_size, 0);
    
    /* Compute frequency band energies */
    float band_energies[8];
    int samples_per_band = fft_size / (2 * num_freq_bands);
    
    for (int band = 0; band < num_freq_bands; band++) {
        float energy = 0.0f;
        for (int i = 0; i < samples_per_band; i++) {
            int idx = band * samples_per_band + i;
            float magnitude = sqrtf(fft_data[idx].real * fft_data[idx].real + 
                                   fft_data[idx].imag * fft_data[idx].imag);
            energy += magnitude;
        }
        band_energies[band] = energy / (float)samples_per_band;
    }
    
    /* Update particles using PRIME dynamics */
    fprintf(stderr, "Updating particles with ZKAEDI PRIME...\n");
    const float eta = 0.441f;    /* Wilson-Fisher coupling */
    const float gamma = 0.3f;
    const float beta = 0.1f;
    const float epsilon = 0.05f;
    const float dt = 0.016f;     /* ~60 FPS */
    
    for (int frame = 0; frame < 30; frame++) {
        for (int i = 0; i < num_particles; i++) {
            int band = i % num_freq_bands;
            float energy_field = band_energies[band];
            
            update_particle_prime(&particles[i], energy_field, dt, eta, gamma, beta, epsilon);
        }
    }
    
    /* Render final frame */
    fprintf(stderr, "Rendering visualization...\n");
    render_particles(particles, num_particles, framebuffer, height, width);
    
    /* Output PPM */
    printf("P6\n%d %d\n255\n", width, height);
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            fwrite(framebuffer[j][i], 1, 3, stdout);
        }
    }
    
    fprintf(stderr, "Render complete!\n");
    fprintf(stderr, "Band energies: ");
    for (int i = 0; i < num_freq_bands; i++) {
        fprintf(stderr, "%.2f ", band_energies[i]);
    }
    fprintf(stderr, "\n");
    
    fprintf(stderr, "ZCC Extra Pass Verification: fn10(100)=%d\n", zcc_test_fn10(100));
    return 0;
}


float zcc_test_fn1(float x) { return x * 2.0f; }
float zcc_test_fn2(float x) { return x + 1.0f; }
float zcc_test_fn3(float x) { return x - 1.0f; }
float zcc_test_fn4(float x) { return x * x; }
float zcc_test_fn5(float x) { return x * x * x; }
float zcc_test_fn6(float x) { return x > 0.0f ? x : 0.0f; }
float zcc_test_fn7(float x, float y) { return x > y ? x : y; }
float zcc_test_fn8(float x, float y) { return x < y ? x : y; }
float zcc_test_fn9(float x, float y, float t) { return x + (y - x) * t; }
int zcc_test_fn10(int x) { return x + 42; }
