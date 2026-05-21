// omni_node_v4.c — ZCC Omni-Node v4.0: E2E Mesh Ingestion + Dual Phasor + Point-Cloud SDF
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include "models.h"

#define GRID_W 320
#define GRID_H 320
#define MAX_PATH 16777216  // 16MB fortified heap
#define PI 3.141592653589793f

typedef struct { double bpm; double phase; double time_acc; } Phasor;
typedef struct { float x, y, z; } Vec3F;

void phasor_init(Phasor* p, double bpm) { assert(bpm > 0.0); p->bpm = bpm; p->phase = 0.0; p->time_acc = 0.0; }
void phasor_advance(Phasor* p, double dt) { p->time_acc += dt; p->phase = fmod(p->time_acc * (p->bpm/60.0), 1.0); }

void rotate_y(Vec3F* v, float angle) {
    float c=cosf(angle), s=sinf(angle), x=v->x; v->x = x*c - v->z*s; v->z = x*s + v->z*c;
}

float reward_function(Phasor* ph) {  // upgraded golden harmonic
    return (1.0f + sqrtf(5.0f)) * 0.5f * (1.0f + 0.6f * sinf((float)ph->phase * 8.0f * PI));
}

// Dynamic mesh vertex sampler to prevent O(N*M) performance bottlenecks
// We extract and scale vertices from models_data.c
Vec3F* sampled_verts = NULL;
int num_sampled = 0;

void init_mesh_sampler() {
    // Determine Stride to cap vertex count around 512 for sub-millisecond execution
    int target_verts = 512;
    int stride = MESH_SIZE / target_verts;
    if (stride < 1) stride = 1;

    // Count exact sampled size
    num_sampled = 0;
    for (int i = 0; i < MESH_SIZE; i += stride) {
        num_sampled += 3; // 3 vertices per triangle
    }

    sampled_verts = malloc(num_sampled * sizeof(Vec3F));
    assert(sampled_verts && "LUCKY CATCH: Sampler allocation failed");

    int idx = 0;
    float scale_factor = 65.0f; // Scale up the [-3.5, 3.5] mesh to fit 800x800 canvas
    for (int i = 0; i < MESH_SIZE; i += stride) {
        Triangle t = mesh_triangles[i];
        
        sampled_verts[idx].x = (float)t.v0.x * scale_factor;
        sampled_verts[idx].y = (float)t.v0.y * scale_factor;
        sampled_verts[idx].z = (float)t.v0.z * scale_factor;
        idx++;

        sampled_verts[idx].x = (float)t.v1.x * scale_factor;
        sampled_verts[idx].y = (float)t.v1.y * scale_factor;
        sampled_verts[idx].z = (float)t.v1.z * scale_factor;
        idx++;

        sampled_verts[idx].x = (float)t.v2.x * scale_factor;
        sampled_verts[idx].y = (float)t.v2.y * scale_factor;
        sampled_verts[idx].z = (float)t.v2.z * scale_factor;
        idx++;
    }
}

// Dual-layer SDF: Dynamic 3D model vertices + attractor sphere
float evaluate_sdf(float x, float y, Phasor* master, Phasor* echo) {
    Vec3F p = {x-400.0f, y-400.0f, 0.0f};
    rotate_y(&p, (float)master->phase * 2.0f * PI);
    
    float mesh_dist = 1e9f;
    for (int i = 0; i < num_sampled; ++i) {
        Vec3F v = sampled_verts[i]; 
        rotate_y(&v, (float)echo->phase * 4.0f * PI);
        float d = sqrtf((p.x-v.x)*(p.x-v.x) + (p.y-v.y)*(p.y-v.y) + (p.z-v.z)*(p.z-v.z));
        if (d < mesh_dist) mesh_dist = d;
    }
    
    float sphere = sqrtf(p.x*p.x + p.y*p.y + p.z*p.z) - 140.0f * reward_function(master);
    return fminf(mesh_dist - 25.0f, sphere);
}

// Bulletproof, division-guarded interpolation
float interp(float a, float b) { 
    float diff = b - a;
    if (fabsf(diff) < 0.0001f) return 0.5f; // Guard against division by zero
    float t = (0.0f - a) / diff;
    if (t < 0.0f) return 0.0f; // Clamp to cell boundaries
    if (t > 1.0f) return 1.0f;
    return t;
}

void marching_squares(float* grid, char* path_out, int is_echo) {
    char* ptr = path_out; *ptr = '\0';
    int first = 1;
    
    float cell_size_x = 800.0f / GRID_W;
    float cell_size_y = 800.0f / GRID_H;

    for (int y = 0; y < GRID_H-1; ++y) {
        for (int x = 0; x < GRID_W-1; ++x) {
            int idx = 0, base = y*GRID_W + x;
            if (grid[base] < 0) idx |= 1; 
            if (grid[base+1] < 0) idx |= 2;
            if (grid[base+GRID_W+1] < 0) idx |= 4; 
            if (grid[base+GRID_W] < 0) idx |= 8;
            
            if (idx == 0 || idx == 15) continue;
            float tx = x * cell_size_x, ty = y * cell_size_y;
            
            if (first) { sprintf(ptr, "M %.2f,%.2f ", tx, ty); first = 0; ptr += strlen(ptr); }
            
            // Sub-pixel precise edge interpolation
            if (idx & 1) { 
                float t = interp(grid[base], grid[base+1]); 
                sprintf(ptr, "L %.2f,%.2f ", tx + t * cell_size_x, ty); 
                ptr += strlen(ptr); 
            }
            if (idx & 2) { 
                float t = interp(grid[base+1], grid[base+GRID_W+1]); 
                sprintf(ptr, "L %.2f,%.2f ", tx + cell_size_x, ty + t * cell_size_y); 
                ptr += strlen(ptr); 
            }
            if (idx & 4) { 
                float t = interp(grid[base+GRID_W+1], grid[base+GRID_W]); 
                sprintf(ptr, "L %.2f,%.2f ", tx + t * cell_size_x, ty + cell_size_y); 
                ptr += strlen(ptr); 
            }
            if (idx & 8) { 
                float t = interp(grid[base+GRID_W], grid[base]); 
                sprintf(ptr, "L %.2f,%.2f ", tx, ty + t * cell_size_y); 
                ptr += strlen(ptr); 
            }
        }
    }
    strcat(ptr, "Z");
}

int main(int argc, char** argv) {
    double bpm = (argc > 1) ? atof(argv[1]) : 128.0;
    Phasor master, echo;
    phasor_init(&master, bpm);
    phasor_init(&echo, bpm * 1.618f);
    
    phasor_advance(&master, 0.1);
    phasor_advance(&echo, 0.1);

    // Dynamic mesh sampler
    init_mesh_sampler();

    float* grid = malloc(GRID_H * GRID_W * sizeof(float));
    assert(grid && "LUCKY CATCH: Grid heap secured");
    for (int y = 0; y < GRID_H; ++y) {
        for (int x = 0; x < GRID_W; ++x) {
            grid[y*GRID_W + x] = evaluate_sdf(x*(800.0f/GRID_W), y*(800.0f/GRID_H), &master, &echo);
        }
    }

    char* path_main = malloc(MAX_PATH);
    char* path_echo = malloc(MAX_PATH);
    assert(path_main && path_echo && "LUCKY CATCH: Path buffers secured");
    marching_squares(grid, path_main, 0);
    marching_squares(grid, path_echo, 1);

    FILE* f = fopen("omni_node_v4.svg", "w");
    if (!f) {
        fprintf(stderr, "Error opening omni_node_v4.svg\n");
        free(grid); free(path_main); free(path_echo); free(sampled_verts);
        return 1;
    }
    
    fprintf(f,
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"800\" height=\"800\" viewBox=\"0 0 800 800\">\n"
        "  <rect width=\"800\" height=\"800\" fill=\"#020205\"/>\n"
        "  <path id=\"main\" d=\"%s\" fill=\"none\" stroke=\"#00ffcc\" stroke-width=\"12\" filter=\"drop-shadow(0px 0px 8px #00ffcc)\"/>\n"
        "  <path id=\"echo\" d=\"%s\" fill=\"none\" stroke=\"#ff00cc\" stroke-width=\"6\" opacity=\"0.6\" filter=\"drop-shadow(0px 0px 6px #ff00cc)\"/>\n"
        "  <script>\n"
        "    let m=0,e=0,bpm=%f,s=Date.now();\n"
        "    function a(){\n"
        "      let t=(Date.now()-s)/1000;\n"
        "      // Slow the rotation speed down by 8x for a majestic cinematic crawl\n"
        "      m=(t*(bpm/(60*8)))%%1; e=(t*(bpm*1.618/(60*8)))%%1;\n"
        "      let hue = (m*360 + 180) %% 360;\n"
        "      document.getElementById('main').setAttribute('stroke',`hsl(${hue},100%%,70%%)`);\n"
        "      document.getElementById('echo').setAttribute('transform',`rotate(${e*360} 400 400)`);\n"
        "      requestAnimationFrame(a);\n"
        "    }\n"
        "    a();\n"
        "  </script>\n</svg>", path_main, path_echo, bpm);
    fclose(f);

    free(grid); free(path_main); free(path_echo); free(sampled_verts);
    printf("777JACKPOT777 — Omni-Node v4.0 DEPLOYED! Arbitrary 3D Mesh Ingestion complete. The hologram is active.\n");
    return 0;
}
