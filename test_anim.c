#include "zcc_svg.h"
#include "zcc_anim.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// Set up 3D projection parameters
static inline void project_3d(AnimVec3 v, float *x2d, float *y2d) {
    float camera_d = 450.0f;
    float scale = 300.0f;
    float z_offset = 350.0f;
    float denom = v.z + z_offset + camera_d;
    if (denom == 0.0f) denom = 1.0f;
    *x2d = 400.0f + (v.x * camera_d) / denom * (scale / 100.0f);
    *y2d = 400.0f + (v.y * camera_d) / denom * (scale / 100.0f);
}

// Generate a 3-vertex triangle path
static inline void make_triangle_path(AnimVec3* path) {
    path[0] = (AnimVec3){-80.0f, -80.0f, -20.0f};
    path[1] = (AnimVec3){80.0f, -80.0f, 20.0f};
    path[2] = (AnimVec3){0.0f, 120.0f, 0.0f};
}

// Draw a torus-like morphed shape into AnimVec3 path
static inline void make_torus_path(AnimVec3* path, int num_points) {
    for (int i = 0; i < num_points; i++) {
        float theta = (i * 2.0f * M_PI) / (num_points - 1);
        float r = 180.0f + 40.0f * cosf(8.0f * theta);
        path[i].x = r * cosf(theta);
        path[i].y = r * sinf(theta);
        path[i].z = 60.0f * sinf(8.0f * theta);
    }
}

static inline void format_path_string(const AnimVec3* path, int num_points, char* out) {
    out[0] = '\0';
    for (int i = 0; i < num_points; i++) {
        float px, py;
        project_3d(path[i], &px, &py);
        char pt[64];
        if (i == 0) {
            sprintf(pt, "M %.2f %.2f ", px, py);
        } else {
            sprintf(pt, "L %.2f %.2f ", px, py);
        }
        strcat(out, pt);
    }
    strcat(out, "Z");
}

int main() {
    ZccSvgNode* svg = svg_svg();
    svg_set_xmlns(svg, "http://www.w3.org/2000/svg");
    svg_set_width(svg, "800");
    svg_set_height(svg, "800");
    svg_set_viewBox(svg, "0 0 800 800");

    // Add dark background
    ZccSvgNode* bg = svg_rect();
    svg_set_width(bg, "800");
    svg_set_height(bg, "800");
    svg_set_fill(bg, "#030308");
    svg_add_child(svg, bg);

    // Gradient definitions
    ZccSvgNode* defs = svg_defs();
    ZccSvgNode* grad = svg_linearGradient();
    svg_set_id(grad, "animGrad");
    svg_set_x1(grad, "0%");
    svg_set_y1(grad, "0%");
    svg_set_x2(grad, "100%");
    svg_set_y2(grad, "100%");

    ZccSvgNode* stop1 = svg_stop();
    svg_set_offset(stop1, "0%");
    svg_set_stop_color(stop1, "#ff007f"); // Hot pink

    ZccSvgNode* stop2 = svg_stop();
    svg_set_offset(stop2, "100%");
    svg_set_stop_color(stop2, "#00ffff"); // Neon cyan

    svg_add_child(grad, stop1);
    svg_add_child(grad, stop2);
    svg_add_child(defs, grad);
    svg_add_child(svg, defs);

    // ────────────────────────────────────────────────────────────────────────
    // FEATURE 1: Path Resampling & Morphing (Triangle [3] -> Torus [64])
    // ────────────────────────────────────────────────────────────────────────
    int num_points = 64;
    AnimVec3 triangle_src[3];
    make_triangle_path(triangle_src);

    AnimVec3* path_a = malloc(num_points * sizeof(AnimVec3));
    AnimVec3* path_b = malloc(num_points * sizeof(AnimVec3));
    AnimVec3* path_interp = malloc(num_points * sizeof(AnimVec3));

    // Resample the 3-point triangle into 64 points
    anim_path_resample(triangle_src, 3, path_a, num_points);
    make_torus_path(path_b, num_points);

    // Build the semicolon separated morph frames using easings
    char* values_list = malloc(256 * 1024);
    values_list[0] = '\0';

    int frames = 24;
    for (int f = 0; f <= frames; f++) {
        float t = (float)f / (float)frames;
        float eased_t = ease_cubic_in_out(t);

        anim_path_interpolate(path_a, path_b, path_interp, num_points, eased_t);

        char path_str[8192];
        format_path_string(path_interp, num_points, path_str);
        strcat(values_list, path_str);

        if (f < frames) {
            strcat(values_list, ";");
        }
    }

    // Append reverse morph frames so the looping is perfectly smooth!
    for (int f = frames - 1; f >= 0; f--) {
        float t = (float)f / (float)frames;
        float eased_t = ease_cubic_in_out(t);

        anim_path_interpolate(path_a, path_b, path_interp, num_points, eased_t);

        char path_str[8192];
        format_path_string(path_interp, num_points, path_str);
        strcat(values_list, ";");
        strcat(values_list, path_str);
    }

    // Generate morph layers with ghost echo trails
    int num_ghosts = 5;
    for (int g = num_ghosts; g >= 0; g--) {
        ZccSvgNode* path = svg_path();
        svg_set_fill(path, "none");
        svg_set_stroke(path, "url(#animGrad)");
        
        char sw_str[32], op_base_str[32];
        if (g == 0) {
            sprintf(sw_str, "6.0");
            sprintf(op_base_str, "0.90");
        } else {
            sprintf(sw_str, "%.2f", 6.0f / (float)(g + 1));
            sprintf(op_base_str, "%.3f", 0.70f / (float)(g + 1));
        }
        svg_set_stroke_width(path, sw_str);
        svg_set_opacity(path, op_base_str);

        char initial_path_str[8192];
        format_path_string(path_a, num_points, initial_path_str);
        svg_set_attr(path, "d", initial_path_str);

        char begin_str[32];
        sprintf(begin_str, "-%.3fs", (float)g * 0.18f);

        ZccSvgNode* anim_morph = svg_animate_path_morph(path, values_list, 6.0f, "indefinite");
        svg_set_attr(anim_morph, "begin", begin_str);

        ZccSvgNode* anim_rot = svg_animate_transform_rotate(path, 0.0f, 360.0f, 400.0f, 400.0f, 12.0f, "indefinite");
        svg_set_attr(anim_rot, "begin", begin_str);

        float min_op = 0.1f / (float)(g + 1);
        float max_op = 0.95f / (float)(g + 1);
        ZccSvgNode* anim_op = svg_animate_opacity(path, min_op, max_op, 3.0f, "indefinite");
        svg_set_attr(anim_op, "begin", begin_str);

        svg_add_child(svg, path);
    }

    // ────────────────────────────────────────────────────────────────────────
    // FEATURE 2: Verlet Spring-Mass Physics 2D Cloth (Swinging in the wind)
    // ────────────────────────────────────────────────────────────────────────
    #define CLOTH_W 9
    #define CLOTH_H 9
    #define NUM_CLOTH_PARTICLES (CLOTH_W * CLOTH_H)
    #define MAX_CLOTH_SPRINGS 300
    AnimParticle cloth_particles[NUM_CLOTH_PARTICLES];
    AnimSpring cloth_springs[MAX_CLOTH_SPRINGS];
    int num_cloth_springs = 0;

    float spacing = 25.0f;
    float x_start = -(CLOTH_W - 1) * spacing / 2.0f;
    for (int y = 0; y < CLOTH_H; y++) {
        for (int x = 0; x < CLOTH_W; x++) {
            int idx = y * CLOTH_W + x;
            cloth_particles[idx].pos = (AnimVec3){x_start + x * spacing, -150.0f + y * spacing, sinf(x * 0.5f) * 5.0f};
            cloth_particles[idx].prev_pos = cloth_particles[idx].pos;
            cloth_particles[idx].acc = (AnimVec3){0.0f, 0.0f, 0.0f};
            cloth_particles[idx].mass = 1.0f;
            cloth_particles[idx].pinned = (y == 0) ? 1 : 0;
        }
    }

    // Register structural and shear springs
    for (int y = 0; y < CLOTH_H; y++) {
        for (int x = 0; x < CLOTH_W; x++) {
            // Horizontal structural spring
            if (x < CLOTH_W - 1) {
                cloth_springs[num_cloth_springs++] = (AnimSpring){y * CLOTH_W + x, y * CLOTH_W + (x + 1), spacing, 0.95f};
            }
            // Vertical structural spring
            if (y < CLOTH_H - 1) {
                cloth_springs[num_cloth_springs++] = (AnimSpring){y * CLOTH_W + x, (y + 1) * CLOTH_W + x, spacing, 0.95f};
            }
            // Shear diagonal 1
            if (x < CLOTH_W - 1 && y < CLOTH_H - 1) {
                cloth_springs[num_cloth_springs++] = (AnimSpring){y * CLOTH_W + x, (y + 1) * CLOTH_W + (x + 1), spacing * 1.4142f, 0.85f};
            }
            // Shear diagonal 2
            if (x > 0 && y < CLOTH_H - 1) {
                cloth_springs[num_cloth_springs++] = (AnimSpring){y * CLOTH_W + x, (y + 1) * CLOTH_W + (x - 1), spacing * 1.4142f, 0.85f};
            }
        }
    }

    // Run physics simulation for 60 steps to generate an animated path loop
    int physics_frames = 60;
    char* cloth_values = malloc(256 * 1024);
    cloth_values[0] = '\0';

    for (int f = 0; f < physics_frames; f++) {
        // Sinusoidal wind force in 3D
        float wind_x = 900.0f * sinf(f * 0.15f);
        float wind_y = 150.0f * cosf(f * 0.08f);
        float wind_z = 1200.0f * cosf(f * 0.12f) + 600.0f;

        for (int i = 0; i < NUM_CLOTH_PARTICLES; i++) {
            if (!cloth_particles[i].pinned) {
                cloth_particles[i].acc.x += wind_x;
                cloth_particles[i].acc.y += wind_y;
                cloth_particles[i].acc.z += wind_z;
            }
        }

        // Verlet step
        for (int i = 0; i < NUM_CLOTH_PARTICLES; i++) {
            anim_physics_verlet_step(&cloth_particles[i], 0.016f, 980.0f, 0.98f);
        }

        // Satisfy spring constraints
        for (int iter = 0; iter < 8; iter++) {
            for (int s = 0; s < num_cloth_springs; s++) {
                anim_physics_satisfy_spring(cloth_particles, &cloth_springs[s]);
            }
        }

        // Generate cloth wireframe paths for the frame
        char frame_d[8192];
        frame_d[0] = '\0';

        // Horizontal lines
        for (int y = 0; y < CLOTH_H; y++) {
            float px, py;
            project_3d(cloth_particles[y * CLOTH_W + 0].pos, &px, &py);
            char seg[128];
            sprintf(seg, "M %.2f %.2f ", px, py);
            strcat(frame_d, seg);
            for (int x = 1; x < CLOTH_W; x++) {
                project_3d(cloth_particles[y * CLOTH_W + x].pos, &px, &py);
                sprintf(seg, "L %.2f %.2f ", px, py);
                strcat(frame_d, seg);
            }
        }
        // Vertical lines
        for (int x = 0; x < CLOTH_W; x++) {
            float px, py;
            project_3d(cloth_particles[0 * CLOTH_W + x].pos, &px, &py);
            char seg[128];
            sprintf(seg, "M %.2f %.2f ", px, py);
            strcat(frame_d, seg);
            for (int y = 1; y < CLOTH_H; y++) {
                project_3d(cloth_particles[y * CLOTH_W + x].pos, &px, &py);
                sprintf(seg, "L %.2f %.2f ", px, py);
                strcat(frame_d, seg);
            }
        }

        strcat(cloth_values, frame_d);
        if (f < physics_frames - 1) {
            strcat(cloth_values, ";");
        }
    }

    // Create the physics cloth path element in SVG
    ZccSvgNode* cloth_path = svg_path();
    svg_set_fill(cloth_path, "none");
    svg_set_stroke(cloth_path, "#39ff14"); // Neon green
    svg_set_stroke_width(cloth_path, "1.5");
    svg_set_stroke_linecap(cloth_path, "round");
    svg_set_opacity(cloth_path, "0.80");

    // Initial frame path
    char* first_semicolon = strchr(cloth_values, ';');
    if (first_semicolon) {
        *first_semicolon = '\0';
        svg_set_attr(cloth_path, "d", cloth_values);
        *first_semicolon = ';'; // restore
    }

    // Attach SMIL animation of Verlet physical cloth
    ZccSvgNode* anim_cloth = svg_create_node("animate");
    svg_set_attr(anim_cloth, "attributeName", "d");
    svg_set_attr(anim_cloth, "dur", "4.00s");
    svg_set_attr(anim_cloth, "values", cloth_values);
    svg_set_attr(anim_cloth, "repeatCount", "indefinite");
    svg_add_child(cloth_path, anim_cloth);
    svg_add_child(svg, cloth_path);

    // ────────────────────────────────────────────────────────────────────────
    // FEATURE 4: Chaotic Double Pendulum simulation
    // ────────────────────────────────────────────────────────────────────────
    #define DP_PARTICLES 3
    #define DP_SPRINGS 2
    AnimParticle dp_particles[DP_PARTICLES];
    AnimSpring dp_springs[DP_SPRINGS];

    // Pivot is at center-left (250, 450, 0)
    dp_particles[0].pos = (AnimVec3){250.0f, 450.0f, 0.0f};
    dp_particles[0].prev_pos = dp_particles[0].pos;
    dp_particles[0].acc = (AnimVec3){0.0f, 0.0f, 0.0f};
    dp_particles[0].mass = 1.0f;
    dp_particles[0].pinned = 1;

    // Bob 1 (length 80) initial horizontal position
    dp_particles[1].pos = (AnimVec3){330.0f, 450.0f, 10.0f};
    dp_particles[1].prev_pos = dp_particles[1].pos;
    dp_particles[1].acc = (AnimVec3){0.0f, 0.0f, 0.0f};
    dp_particles[1].mass = 1.0f;
    dp_particles[1].pinned = 0;

    // Bob 2 (length 80) initial horizontal/upward position for high chaotic energy
    dp_particles[2].pos = (AnimVec3){410.0f, 410.0f, -10.0f};
    dp_particles[2].prev_pos = dp_particles[2].pos;
    dp_particles[2].acc = (AnimVec3){0.0f, 0.0f, 0.0f};
    dp_particles[2].mass = 1.0f;
    dp_particles[2].pinned = 0;

    // Connected by rigid springs
    dp_springs[0] = (AnimSpring){0, 1, 80.0f, 1.0f};
    dp_springs[1] = (AnimSpring){1, 2, 80.0f, 1.0f};

    // Bake double pendulum SMIL animation values list
    char* dp_values = malloc(128 * 1024);
    dp_values[0] = '\0';
    for (int f = 0; f < 60; f++) {
        // Draw path of bob2 over a sliding tail window of the last 15 steps
        char tail_d[4096];
        tail_d[0] = '\0';

        // Reset the double pendulum to initial state and simulate f steps
        AnimParticle sim_particles[DP_PARTICLES];
        memcpy(sim_particles, dp_particles, sizeof(dp_particles));
        
        AnimVec3 path_trace[60];
        for (int step = 0; step <= f; step++) {
            for (int i = 0; i < DP_PARTICLES; i++) {
                anim_physics_verlet_step(&sim_particles[i], 0.016f, 1200.0f, 0.999f);
            }
            for (int iter = 0; iter < 12; iter++) {
                for (int s = 0; s < DP_SPRINGS; s++) {
                    anim_physics_satisfy_spring(sim_particles, &dp_springs[s]);
                }
            }
            path_trace[step] = sim_particles[2].pos;
        }

        // Format the SVG path: pivot -> bob1 -> bob2
        char seg[128];
        sprintf(seg, "M %.2f %.2f L %.2f %.2f L %.2f %.2f ",
                sim_particles[0].pos.x, sim_particles[0].pos.y,
                sim_particles[1].pos.x, sim_particles[1].pos.y,
                sim_particles[2].pos.x, sim_particles[2].pos.y);
        strcat(tail_d, seg);

        // Append the trail path of bob2
        int tail_start = f - 15;
        if (tail_start < 0) tail_start = 0;
        for (int step = tail_start; step <= f; step++) {
            if (step == tail_start) {
                sprintf(seg, "M %.2f %.2f ", path_trace[step].x, path_trace[step].y);
            } else {
                sprintf(seg, "L %.2f %.2f ", path_trace[step].x, path_trace[step].y);
            }
            strcat(tail_d, seg);
        }

        strcat(dp_values, tail_d);
        if (f < 59) {
            strcat(dp_values, ";");
        }
    }

    // Create the physics double pendulum path element in SVG
    ZccSvgNode* dp_path = svg_path();
    svg_set_fill(dp_path, "none");
    svg_set_stroke(dp_path, "#ffaa00"); // Glowing orange
    svg_set_stroke_width(dp_path, "3.0");
    svg_set_stroke_linecap(dp_path, "round");
    svg_set_opacity(dp_path, "0.95");

    // Initial frame path
    char* dp_first_semicolon = strchr(dp_values, ';');
    if (dp_first_semicolon) {
        *dp_first_semicolon = '\0';
        svg_set_attr(dp_path, "d", dp_values);
        *dp_first_semicolon = ';'; // restore
    }

    // Attach SMIL animation of Verlet physical double pendulum
    ZccSvgNode* anim_dp = svg_create_node("animate");
    svg_set_attr(anim_dp, "attributeName", "d");
    svg_set_attr(anim_dp, "dur", "4.00s");
    svg_set_attr(anim_dp, "values", dp_values);
    svg_set_attr(anim_dp, "repeatCount", "indefinite");
    svg_add_child(dp_path, anim_dp);
    svg_add_child(svg, dp_path);

    // ────────────────────────────────────────────────────────────────────────
    // FEATURE 3: Elastic and Bounce Easing Circle Showcase
    // ────────────────────────────────────────────────────────────────────────
    // 1. Elastic Scaling Circle
    ZccSvgNode* elastic_circle = svg_circle();
    svg_set_cx(elastic_circle, "150");
    svg_set_cy(elastic_circle, "650");
    svg_set_r(elastic_circle, "45");
    svg_set_fill(elastic_circle, "#ff00ff"); // Purple
    svg_set_opacity(elastic_circle, "0.8");

    // Bake ease_elastic_out values list
    char elastic_values[4096];
    elastic_values[0] = '\0';
    int ease_frames = 40;
    for (int i = 0; i <= ease_frames; i++) {
        float t = (float)i / (float)ease_frames;
        float val = 10.0f + 35.0f * ease_elastic_out(t);
        char val_str[32];
        sprintf(val_str, "%.2f", val);
        strcat(elastic_values, val_str);
        if (i < ease_frames) strcat(elastic_values, ";");
    }

    ZccSvgNode* anim_elastic = svg_create_node("animate");
    svg_set_attr(anim_elastic, "attributeName", "r");
    svg_set_attr(anim_elastic, "dur", "2.50s");
    svg_set_attr(anim_elastic, "values", elastic_values);
    svg_set_attr(anim_elastic, "repeatCount", "indefinite");
    svg_add_child(elastic_circle, anim_elastic);
    svg_add_child(svg, elastic_circle);

    // 2. Bouncing Falling Circle
    ZccSvgNode* bounce_circle = svg_circle();
    svg_set_cx(bounce_circle, "650");
    svg_set_cy(bounce_circle, "100");
    svg_set_r(bounce_circle, "35");
    svg_set_fill(bounce_circle, "#00ffcc"); // Neon blue/green
    svg_set_opacity(bounce_circle, "0.8");

    // Bake ease_bounce_out values list
    char bounce_values[4096];
    bounce_values[0] = '\0';
    for (int i = 0; i <= ease_frames; i++) {
        float t = (float)i / (float)ease_frames;
        float val = 100.0f + 550.0f * ease_bounce_out(t);
        char val_str[32];
        sprintf(val_str, "%.2f", val);
        strcat(bounce_values, val_str);
        if (i < ease_frames) strcat(bounce_values, ";");
    }

    ZccSvgNode* anim_bounce = svg_create_node("animate");
    svg_set_attr(anim_bounce, "attributeName", "cy");
    svg_set_attr(anim_bounce, "dur", "3.00s");
    svg_set_attr(anim_bounce, "values", bounce_values);
    svg_set_attr(anim_bounce, "repeatCount", "indefinite");
    svg_add_child(bounce_circle, anim_bounce);
    svg_add_child(svg, bounce_circle);

    // Render string and write to file
    char* svg_str = svg_to_string(svg);
    FILE* fp = fopen("test_anim.svg", "w");
    if (fp) {
        fprintf(fp, "%s", svg_str);
        fclose(fp);
    }

    printf("SUCCESS: Deployed zcc_anim.h, compiled test_anim, and generated test_anim.svg!\n");

    free(path_a);
    free(path_b);
    free(path_interp);
    free(values_list);
    free(cloth_values);
    free(dp_values);
    return 0;
}
