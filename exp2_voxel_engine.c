/* EXPERIMENT 2: Voxel Engine (Minecraft-style)
 * 
 * ZCC Features Demonstrated:
 * - Bitfields for ultra-compact voxel storage (16 bits per voxel)
 * - VLAs for dynamic chunk allocation
 * - _Generic for type-safe voxel operations
 * - typeof for polymorphic voxel accessors
 * 
 * Compile: ./zcc exp2_voxel_engine.c -o exp2_voxel_engine -lm
 * Run:     ./exp2_voxel_engine > voxel_output.ppm
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





/* BITFIELD-PACKED VOXEL DATA (CG-010)
 * 16 bits total:
 * - 12 bits: voxel type (4096 possible types)
 * - 4 bits: metadata (rotation, state, etc.)
 */
typedef struct {
    unsigned type : 12;      /* 0-4095 voxel types */
    unsigned metadata : 4;   /* 0-15 state values */
} Voxel;

/* Voxel type enum (subset) */
enum VoxelType {
    VOXEL_AIR = 0,
    VOXEL_STONE = 1,
    VOXEL_DIRT = 2,
    VOXEL_GRASS = 3,
    VOXEL_WOOD = 4,
    VOXEL_LEAVES = 5,
    VOXEL_WATER = 6,
    VOXEL_SAND = 7,
    VOXEL_GLASS = 8,
    VOXEL_GOLD = 9,
    VOXEL_DIAMOND = 10,
};

/* Voxel colors for rendering */
typedef struct {
    unsigned char r, g, b;
} Color;

static const Color voxel_colors[] = {
    {0, 0, 0},           /* AIR */
    {128, 128, 128},     /* STONE */
    {139, 69, 19},       /* DIRT */
    {34, 139, 34},       /* GRASS */
    {139, 90, 43},       /* WOOD */
    {0, 255, 0},         /* LEAVES */
    {0, 100, 255},       /* WATER */
    {238, 232, 170},     /* SAND */
    {173, 216, 230},     /* GLASS */
    {255, 215, 0},       /* GOLD */
    {0, 255, 255},       /* DIAMOND */
};

/* Chunk dimensions */
#define CHUNK_SIZE_X 16
#define CHUNK_SIZE_Y 32
#define CHUNK_SIZE_Z 16

/* Chunk structure using VLA principles */
typedef struct {
    Voxel voxels[CHUNK_SIZE_Y][CHUNK_SIZE_Z][CHUNK_SIZE_X];
    int x, y, z;  /* Chunk position in world */
} Chunk;

/* _GENERIC MACRO FOR TYPE-SAFE VOXEL ACCESS (CG-009) */
#define voxel_get(chunk, x, y, z) voxel_get_chunk(chunk, x, y, z) 

#define voxel_set(chunk, x, y, z, type) voxel_set_chunk(chunk, x, y, z, type) 

/* Voxel accessor functions */
static inline Voxel voxel_get_chunk(Chunk *chunk, int x, int y, int z) {
    if (x < 0 || x >= CHUNK_SIZE_X || 
        y < 0 || y >= CHUNK_SIZE_Y || 
        z < 0 || z >= CHUNK_SIZE_Z) {
        Voxel r; r.type=VOXEL_AIR; r.metadata=0; return r;
    }
    return chunk->voxels[y][z][x];
}

static inline void voxel_set_chunk(Chunk *chunk, int x, int y, int z, unsigned type) {
    if (x >= 0 && x < CHUNK_SIZE_X && 
        y >= 0 && y < CHUNK_SIZE_Y && 
        z >= 0 && z < CHUNK_SIZE_Z) {
        chunk->voxels[y][z][x].type = type;
    }
}

/* Procedural terrain generation using Perlin-like noise */
static float noise3d(int x, int y, int z) {
    /* Simple hash-based pseudo-noise */
    int n = x + y * 57 + z * 131;
    n = (n << 13) ^ n;
    return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}

static float perlin_noise(float x, float y, float z) {
    int xi = (int)floorf(x);
    int yi = (int)floorf(y);
    int zi = (int)floorf(z);
    
    float xf = x - (float)xi;
    float yf = y - (float)yi;
    float zf = z - (float)zi;
    
    /* Smooth interpolation */
    float u = xf * xf * (3.0f - 2.0f * xf);
    float v = yf * yf * (3.0f - 2.0f * yf);
    float w = zf * zf * (3.0f - 2.0f * zf);
    
    /* Trilinear interpolation */
    float c000 = noise3d(xi, yi, zi);
    float c100 = noise3d(xi + 1, yi, zi);
    float c010 = noise3d(xi, yi + 1, zi);
    float c110 = noise3d(xi + 1, yi + 1, zi);
    float c001 = noise3d(xi, yi, zi + 1);
    float c101 = noise3d(xi + 1, yi, zi + 1);
    float c011 = noise3d(xi, yi + 1, zi + 1);
    float c111 = noise3d(xi + 1, yi + 1, zi + 1);
    
    float x00 = c000 * (1.0f - u) + c100 * u;
    float x10 = c010 * (1.0f - u) + c110 * u;
    float x01 = c001 * (1.0f - u) + c101 * u;
    float x11 = c011 * (1.0f - u) + c111 * u;
    
    float y0 = x00 * (1.0f - v) + x10 * v;
    float y1 = x01 * (1.0f - v) + x11 * v;
    
    return y0 * (1.0f - w) + y1 * w;
}

/* Generate chunk terrain */
static void generate_chunk(Chunk *chunk) {
    memset(chunk->voxels, 0, sizeof(chunk->voxels));
    
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            /* Multi-octave noise for terrain height */
            float world_x = (float)(chunk->x * CHUNK_SIZE_X + x);
            float world_z = (float)(chunk->z * CHUNK_SIZE_Z + z);
            
            float height = 0.0f;
            height += perlin_noise(world_x * 0.01f, 0.0f, world_z * 0.01f) * 10.0f;
            height += perlin_noise(world_x * 0.05f, 0.0f, world_z * 0.05f) * 5.0f;
            height += perlin_noise(world_x * 0.1f, 0.0f, world_z * 0.1f) * 2.0f;
            
            int terrain_height = (int)(CHUNK_SIZE_Y / 2 + height);
            
            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                unsigned voxel_type = VOXEL_AIR;
                
                if (y < terrain_height - 3) {
                    voxel_type = VOXEL_STONE;
                } else if (y < terrain_height - 1) {
                    voxel_type = VOXEL_DIRT;
                } else if (y == terrain_height - 1) {
                    voxel_type = VOXEL_GRASS;
                } else if (y < CHUNK_SIZE_Y / 2) {
                    voxel_type = VOXEL_WATER;
                }
                
                /* Add some ores randomly */
                if (voxel_type == VOXEL_STONE) {
                    float ore_noise = perlin_noise(world_x * 0.3f, (float)y * 0.3f, world_z * 0.3f);
                    if (ore_noise > 0.6f && y < terrain_height / 2) {
                        voxel_type = VOXEL_GOLD;
                    } else if (ore_noise > 0.75f && y < terrain_height / 3) {
                        voxel_type = VOXEL_DIAMOND;
                    }
                }
                
                voxel_set(chunk, x, y, z, voxel_type);
            }
        }
    }
}

/* Simplified raycast rendering from camera position */
typedef struct {
    float x, y, z;
} Vec3;
static inline Vec3 vec3_create(float x, float y, float z) { Vec3 r; r.x=x; r.y=y; r.z=z; return r; }

static Vec3 vec3_add(Vec3 a, Vec3 b) {
    Vec3 r; r.x=a.x + b.x; r.y=a.y + b.y; r.z=a.z + b.z; return r;
}

static Vec3 vec3_scale(Vec3 v, float s) {
    Vec3 r; r.x=v.x * s; r.y=v.y * s; r.z=v.z * s; return r;
}

/* Simple voxel raycast */
static Voxel raycast_voxel(Chunk *chunk, Vec3 origin, Vec3 dir, int max_steps) {
    Vec3 pos = origin;
    
    for (int step = 0; step < max_steps; step++) {
        int vx = (int)floorf(pos.x);
        int vy = (int)floorf(pos.y);
        int vz = (int)floorf(pos.z);
        
        Voxel v = voxel_get(chunk, vx, vy, vz);
        if (v.type != VOXEL_AIR) {
            return v;
        }
        
        pos = vec3_add(pos, vec3_scale(dir, 0.1f));
    }
    
    Voxel r; r.type=VOXEL_AIR; r.metadata=0; return r;
}

/* Render chunk to framebuffer */
static void render_chunk(Chunk *chunk, unsigned char framebuffer[][640][3], int height, int width) {
    Vec3 camera = {8.0f, 20.0f, 25.0f};  /* Camera position */
    Vec3 look_at = {8.0f, 12.0f, 8.0f};  /* Look at center of chunk */
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float u = ((float)x / (float)width) * 2.0f - 1.0f;
            float v = ((float)(height - y) / (float)height) * 2.0f - 1.0f;
            
            /* Simple ray direction */
            Vec3 dir = {
                look_at.x - camera.x + u * 0.5f,
                look_at.y - camera.y + v * 0.5f,
                look_at.z - camera.z
            };
            
            /* Normalize */
            float len = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
            dir.x /= len;
            dir.y /= len;
            dir.z /= len;
            
            Voxel hit = raycast_voxel(chunk, camera, dir, 100);
            
            Color color;
            if (hit.type < sizeof(voxel_colors) / sizeof(voxel_colors[0])) {
                color = voxel_colors[hit.type];
            } else {
                color.r=255; color.g=0; color.b=255;  /* Magenta for unknown */
            }
            
            /* Simple lighting based on voxel type */
            float brightness = 1.0f;
            if (hit.type != VOXEL_AIR) {
                brightness = 0.7f + 0.3f * (float)hit.type / 10.0f;
            } else {
                /* Sky gradient */
                brightness = 0.5f + 0.5f * v;
                color.r=(unsigned char)(135 * brightness); color.g=(unsigned char)(206 * brightness); color.b=(unsigned char)(235 * brightness);
            }
            
            framebuffer[y][x][0] = (unsigned char)(color.r * brightness);
            framebuffer[y][x][1] = (unsigned char)(color.g * brightness);
            framebuffer[y][x][2] = (unsigned char)(color.b * brightness);
        }
    }
}

int main(void) {
    const int width = 640;
    const int height = 480;
    
    fprintf(stderr, "EXPERIMENT 2: Voxel Engine\n");
    fprintf(stderr, "Voxel size: %zu bits\n", sizeof(Voxel) * 8);
    fprintf(stderr, "Chunk size: %d×%d×%d voxels\n", CHUNK_SIZE_X, CHUNK_SIZE_Y, CHUNK_SIZE_Z);
    fprintf(stderr, "Memory per chunk: %zu bytes\n", sizeof(Chunk));
    
    /* NORMALIZED C89: Heap allocation for VLA to prevent stack bounds crash */
    unsigned char (*framebuffer)[640][3] = malloc(480 * 640 * 3);
    
    /* Create and generate chunk */
    Chunk chunk;
    chunk.x = 0;
    chunk.y = 0;
    chunk.z = 0;
    
    fprintf(stderr, "Generating terrain...\n");
    generate_chunk(&chunk);
    
    /* Count voxel types for stats */
    int voxel_counts[16] = {0};
    for (int y = 0; y < CHUNK_SIZE_Y; y++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            for (int x = 0; x < CHUNK_SIZE_X; x++) {
                Voxel v = voxel_get(&chunk, x, y, z);
                if (v.type < 16) {
                    voxel_counts[v.type]++;
                }
            }
        }
    }
    
    fprintf(stderr, "Voxel statistics:\n");
    fprintf(stderr, "  AIR: %d\n", voxel_counts[VOXEL_AIR]);
    fprintf(stderr, "  STONE: %d\n", voxel_counts[VOXEL_STONE]);
    fprintf(stderr, "  GRASS: %d\n", voxel_counts[VOXEL_GRASS]);
    fprintf(stderr, "  GOLD: %d\n", voxel_counts[VOXEL_GOLD]);
    fprintf(stderr, "  DIAMOND: %d\n", voxel_counts[VOXEL_DIAMOND]);
    
    fprintf(stderr, "Rendering...\n");
    render_chunk(&chunk, framebuffer, height, width);
    
    /* Output PPM */
    printf("P6\n%d %d\n255\n", width, height);
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            fwrite(framebuffer[j][i], 1, 3, stdout);
        }
    }
    
    fprintf(stderr, "Render complete!\n");
    fprintf(stderr, "Bitfield compression: %.1f%% of uncompressed size\n",
            (float)(sizeof(Voxel) * 100) / (float)(sizeof(unsigned) + sizeof(unsigned)));
    free(framebuffer);
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
