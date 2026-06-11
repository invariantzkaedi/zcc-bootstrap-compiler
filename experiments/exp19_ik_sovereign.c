/* EXPERIMENT 19: Inverse Kinematics Skeleton Solver (Sovereign)
 * 
 * - Parallel execution of 16 independent IK skeleton chains using AVX-512 structs.
 * - Iterative numeric solvers (Cyclic Coordinate Descent) vectorized.
 * - Zero dependency.
 */

#include "zcaedi_avx512_math.h"

// Define custom abs function
static inline int zcaedi_abs(int x) {
    return x < 0 ? -x : x;
}

#define WIDTH 512
#define HEIGHT 512
#define NUM_BONES 5
#define NUM_CHAINS 16

static inline v16f v16_len(v16f x, v16f y) {
    return _mm512_sqrt_ps(_mm512_fmadd_ps(x, x, _mm512_mul_ps(y, y)));
}

// SoA for 16 chains
typedef struct {
    v16f length;
    v16f angle; // relative to parent
} Bone16;

Bone16 bones[NUM_BONES];
v16f joint_x[NUM_BONES + 1]; // global positions
v16f joint_y[NUM_BONES + 1];

void forward_kinematics_avx512(v16f root_x, v16f root_y) {
    joint_x[0] = root_x;
    joint_y[0] = root_y;
    
    v16f global_angle = _mm512_setzero_ps();
    for (int i = 0; i < NUM_BONES; i++) {
        global_angle = _mm512_add_ps(global_angle, bones[i].angle);
        
        v16f dx = _mm512_cos_ps(global_angle);
        v16f dy = _mm512_sin_ps(global_angle);
        
        joint_x[i+1] = _mm512_fmadd_ps(dx, bones[i].length, joint_x[i]);
        joint_y[i+1] = _mm512_fmadd_ps(dy, bones[i].length, joint_y[i]);
    }
}

void solve_ik_ccd_avx512(v16f root_x, v16f root_y, v16f target_x, v16f target_y) {
    for (int iter = 0; iter < 10; iter++) {
        for (int i = NUM_BONES - 1; i >= 0; i--) {
            v16f eff_x = joint_x[NUM_BONES];
            v16f eff_y = joint_y[NUM_BONES];
            
            v16f cur_x = joint_x[i];
            v16f cur_y = joint_y[i];
            
            v16f dir_eff_x = _mm512_sub_ps(eff_x, cur_x);
            v16f dir_eff_y = _mm512_sub_ps(eff_y, cur_y);
            v16f len_eff = v16_len(dir_eff_x, dir_eff_y);
            
            
            // Wait, we can implement manual _CMP_GT_OQ inline or use a loop for now, or just add it to math.h. Let's use loop.
            v16i len_eff_mask_struct;
            for(int j=0; j<16; j++) len_eff_mask_struct.v[j] = len_eff.v[j] > 0.0001f ? 1 : 0;

            v16f rcp_eff = _mm512_rsqrt_ps(_mm512_mul_ps(len_eff, len_eff)); // approximation to 1/len
            dir_eff_x = _mm512_mask_mul_ps(dir_eff_x, len_eff_mask_struct, dir_eff_x, rcp_eff);
            dir_eff_y = _mm512_mask_mul_ps(dir_eff_y, len_eff_mask_struct, dir_eff_y, rcp_eff);
            
            v16f dir_tgt_x = _mm512_sub_ps(target_x, cur_x);
            v16f dir_tgt_y = _mm512_sub_ps(target_y, cur_y);
            v16f len_tgt = v16_len(dir_tgt_x, dir_tgt_y);
            
            v16i len_tgt_mask_struct;
            for(int j=0; j<16; j++) len_tgt_mask_struct.v[j] = len_tgt.v[j] > 0.0001f ? 1 : 0;

            v16f rcp_tgt = _mm512_rsqrt_ps(_mm512_mul_ps(len_tgt, len_tgt));
            dir_tgt_x = _mm512_mask_mul_ps(dir_tgt_x, len_tgt_mask_struct, dir_tgt_x, rcp_tgt);
            dir_tgt_y = _mm512_mask_mul_ps(dir_tgt_y, len_tgt_mask_struct, dir_tgt_y, rcp_tgt);
            
            v16f angle_e = _mm512_atan2_ps(dir_eff_y, dir_eff_x);
            v16f angle_t = _mm512_atan2_ps(dir_tgt_y, dir_tgt_x);
            v16f delta = _mm512_sub_ps(angle_t, angle_e);
            
            bones[i].angle = _mm512_add_ps(bones[i].angle, delta);
            
            // Constrain limits (-1.5 to 1.5)
            bones[i].angle = _mm512_min_ps(bones[i].angle, _mm512_set1_ps(1.5f));
            bones[i].angle = _mm512_max_ps(bones[i].angle, _mm512_set1_ps(-1.5f));
            
            forward_kinematics_avx512(root_x, root_y);
        }
    }
}

typedef struct { float x, y; } vec2;

void draw_line(unsigned char fb[HEIGHT][WIDTH][3], vec2 p1, vec2 p2, unsigned char r, unsigned char g, unsigned char b) {
    int x0 = (int)p1.x, y0 = (int)p1.y;
    int x1 = (int)p2.x, y1 = (int)p2.y;
    int dx = zcaedi_abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -zcaedi_abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    
    while (1) {
        if (x0 >= 0 && x0 < WIDTH && y0 >= 0 && y0 < HEIGHT) {
            fb[y0][x0][0] = r; fb[y0][x0][1] = g; fb[y0][x0][2] = b;
            if (x0+1 < WIDTH) { fb[y0][x0+1][0] = r; fb[y0][x0+1][1] = g; fb[y0][x0+1][2] = b; }
            if (y0+1 < HEIGHT){ fb[y0+1][x0][0] = r; fb[y0+1][x0][1] = g; fb[y0+1][x0][2] = b; }
        }
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

unsigned char framebuffer[HEIGHT][WIDTH][3];

int main(void) {
    printf("ZKAEDI PRIME SOVEREIGN IK (Exp19) initializing...\n");
    
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            framebuffer[y][x][0] = 20;
            framebuffer[y][x][1] = 20;
            framebuffer[y][x][2] = 20;
        }
    }
    
    for (int i = 0; i < NUM_BONES; i++) {
        bones[i].length = _mm512_set1_ps(40.0f); // shorter bones so 16 fit
        
        float ang_init[16];
        for(int k=0; k<16; k++) ang_init[k] = 0.2f + (float)k*0.01f;
        bones[i].angle = _mm512_loadu_ps(ang_init);
    }
    
    // Spread 16 roots across the screen
    float root_x_arr[16], root_y_arr[16];
    for(int k=0; k<16; k++) {
        root_x_arr[k] = ((float)WIDTH / 16.0f) * (float)k + 16.0f;
        root_y_arr[k] = (float)HEIGHT - 50.0f;
    }
    
    v16f root_x = _mm512_loadu_ps(root_x_arr);
    v16f root_y = _mm512_loadu_ps(root_y_arr);
    
    forward_kinematics_avx512(root_x, root_y);
    
    printf("Solving 16 CCD IK Chains Exp19...\n");
    for(int frame = 0; frame < 30; frame++) {
        float t = (float)frame * 0.2f;
        
        // 16 different targets
        float tx_a[16], ty_a[16];
        for(int k=0; k<16; k++) {
            float phase = t + (float)k*0.5f;
            tx_a[k] = root_x_arr[k] + sovereign_cosf(phase)*100.0f;
            ty_a[k] = (float)HEIGHT/2.0f + sovereign_sinf(phase)*100.0f - 50.0f;
        }
        
        v16f target_x = _mm512_loadu_ps(tx_a);
        v16f target_y = _mm512_loadu_ps(ty_a);
        
        solve_ik_ccd_avx512(root_x, root_y, target_x, target_y);
        
        // Draw bones for all 16 chains
        for(int k=0; k<16; k++) {
            for(int i=0; i<NUM_BONES; i++) {
                float jx0_a[16], jy0_a[16];
                float jx1_a[16], jy1_a[16];
                
                _mm512_storeu_ps(jx0_a, joint_x[i]);
                _mm512_storeu_ps(jy0_a, joint_y[i]);
                _mm512_storeu_ps(jx1_a, joint_x[i+1]);
                _mm512_storeu_ps(jy1_a, joint_y[i+1]);
                
                unsigned char c = 50 + (i * 40);
                unsigned char r_c = (k*15) % 255;
                vec2 p1; p1.x = jx0_a[k]; p1.y = jy0_a[k];
                vec2 p2; p2.x = jx1_a[k]; p2.y = jy1_a[k];
                draw_line(framebuffer, p1, p2, r_c, 255 - c, 150);
            }
            // Draw target
            int tx = (int)tx_a[k], ty = (int)ty_a[k];
            if(tx >= 0 && tx < WIDTH && ty >= 0 && ty < HEIGHT) {
                framebuffer[ty][tx][0] = 255; framebuffer[ty][tx][1] = 0; framebuffer[ty][tx][2] = 0;
            }
        }
    }
    
    unsigned int hash = 0;
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            hash ^= (framebuffer[y][x][0] << 16) | (framebuffer[y][x][1] << 8) | framebuffer[y][x][2];
            hash = (hash << 1) | (hash >> 31);
        }
    }
    
    printf("Exp19 IK Verification Hash: 0x%08X\n", hash);
    printf("Sovereign IK Established. Zero External Dependencies linked.\n");
    return 0;
}
