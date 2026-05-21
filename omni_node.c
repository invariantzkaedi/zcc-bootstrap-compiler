// omni_node.c — ZCC Omni-Node: Phasor + 3D Projection + SDF Marching Squares → SVG
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>

#define GRID_W 128
#define GRID_H 128
#define MAX_PATH (1024 * 1024 * 4) // 4MB to prevent overflow
#define PI 3.141592653589793f

// ==================== LUCKY CODER CORE ====================
// 1. Phasor — FAUST-style audio master clock
typedef struct {
    double bpm;
    double phase;      // [0.0, 1.0)
    double time_acc;
} Phasor;

void phasor_init(Phasor* p, double bpm) {
    assert(bpm > 0.0 && "BPM must be positive — lucky auditor catches this");
    p->bpm = bpm;
    p->phase = 0.0;
    p->time_acc = 0.0;
}

void phasor_advance(Phasor* p, double delta_sec) {
    p->time_acc += delta_sec;
    double freq = p->bpm / 60.0;
    p->phase = fmod(p->time_acc * freq, 1.0);
}

// 2. 3D Vertices + Projection — OBJ style
typedef struct { float x, y, z; } Vec3;

void rotate_y(Vec3* v, float angle) {  // phasor-driven rotation
    float c = cosf(angle), s = sinf(angle);
    float x = v->x;
    v->x = x * c - v->z * s;
    v->z = x * s + v->z * c;
}

// 3. SDF — GLSL style
float sdSphere(Vec3 p, Vec3 center, float r, Phasor* ph) {
    Vec3 d = {p.x - center.x, p.y - center.y, p.z - center.z};
    // phasor morphs radius for breathing effect
    float morph = 1.0f + 0.3f * sinf((float)ph->phase * 2.0f * PI);
    return sqrtf(d.x*d.x + d.y*d.y + d.z*d.z) - r * morph;
}

float evaluate_sdf(float x, float y, Phasor* ph) {
    Vec3 p3d = {x - 400.0f, y - 400.0f, 0.0f};  // lift to 3D
    rotate_y(&p3d, (float)ph->phase * 2.0f * PI);  // 3D rotation driven by phasor
    Vec3 center = {0, 0, 0};
    return sdSphere(p3d, center, 180.0f, ph);
}

// Marching Squares with Edge Interpolation
float interp(float val1, float val2) {
    if (fabsf(val1 - val2) < 0.00001f) return 0.5f;
    return (0.0f - val1) / (val2 - val1);
}

void marching_squares(float grid[GRID_H][GRID_W], char* path_out) {
    char* ptr = path_out;
    ptr += sprintf(ptr, "M ");
    int first = 1;
    
    for (int y = 0; y < GRID_H - 1; ++y) {
        for (int x = 0; x < GRID_W - 1; ++x) {
            // Read cell values
            float v0 = grid[y][x];
            float v1 = grid[y][x+1];
            float v2 = grid[y+1][x+1];
            float v3 = grid[y+1][x];
            
            int idx = 0;
            if (v0 < 0) idx |= 1;
            if (v1 < 0) idx |= 2;
            if (v2 < 0) idx |= 4;
            if (v3 < 0) idx |= 8;
            
            if (idx == 0 || idx == 15) continue;
            
            float cell_size_x = 800.0f / GRID_W;
            float cell_size_y = 800.0f / GRID_H;
            
            float tx = (float)x * cell_size_x;
            float ty = (float)y * cell_size_y;
            
            // Basic edge midpoint calculation with interpolation
            float ix = tx + cell_size_x * interp(v0, v1);
            float iy = ty + cell_size_y * interp(v0, v3);
            
            if (first) {
                sprintf(ptr, "%.2f,%.2f ", ix, iy);
                first = 0;
                ptr += strlen(ptr);
            } else {
                sprintf(ptr, "L %.2f,%.2f ", ix, iy);
                ptr += strlen(ptr);
            }
        }
    }
    strcat(ptr, "Z");
}

int main(int argc, char** argv) {
    double bpm = (argc > 1) ? atof(argv[1]) : 128.0;
    Phasor ph;
    phasor_init(&ph, bpm);
    phasor_advance(&ph, 0.25); // Advance to represent a nice dynamic frame

    // Build SDF grid
    float grid[GRID_H][GRID_W];
    for (int y = 0; y < GRID_H; ++y) {
        for (int x = 0; x < GRID_W; ++x) {
            grid[y][x] = evaluate_sdf((float)x * (800.0f/GRID_W), (float)y * (800.0f/GRID_H), &ph);
        }
    }

    // Allocate massive buffer on heap to prevent stack overflow
    char* path_d = (char*)malloc(MAX_PATH);
    if (path_d == NULL) {
        fprintf(stderr, "LUCKY CATCH: Grid alloc failed — aborting before deployment disaster\n");
        exit(1);
    }
    
    marching_squares(grid, path_d);

    FILE* f = fopen("omni_node.svg", "w");
    if (!f) {
        fprintf(stderr, "Error opening omni_node.svg for writing\n");
        free(path_d);
        return 1;
    }
    
    fprintf(f,
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"800\" height=\"800\" viewBox=\"0 0 800 800\">\n"
        "  <defs>\n"
        "    <style>@import url('https://fonts.googleapis.com/css2?family=Press+Start+2P');</style>\n"
        "  </defs>\n"
        "  <rect width=\"800\" height=\"800\" fill=\"#020205\" />\n"
        "  <path id=\"omni\" d=\"%s\" fill=\"none\" stroke=\"#00ffcc\" stroke-width=\"6\" stroke-linejoin=\"round\" filter=\"drop-shadow(0px 0px 8px #00ffcc)\"/>\n"
        "  <script>\n"
        "    let phase = 0, bpm = %f, start = Date.now();\n"
        "    function animate() {\n"
        "      let elapsed = (Date.now() - start)/1000;\n"
        "      phase = (elapsed * (bpm/60)) %% 1;\n"
        "      document.getElementById('omni').setAttribute('transform', `rotate(${phase*360} 400 400)`);\n"
        "      requestAnimationFrame(animate);\n"
        "    }\n"
        "    animate();\n"
        "  </script>\n"
        "</svg>", path_d, bpm);
    fclose(f);

    free(path_d);
    printf("777JACKPOT777 — Omni-Node SVG generated! Open omni_node.svg and sync to your Suno track. Pure rhythm.\n");
    return 0;
}
