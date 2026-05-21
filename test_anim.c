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

// Draw a sphere-like morphed shape into AnimVec3 path
static inline void make_sphere_path(AnimVec3* path, int num_points) {
    for (int i = 0; i < num_points; i++) {
        float theta = (i * 2.0f * M_PI) / (num_points - 1);
        float r = 100.0f + 30.0f * sinf(5.0f * theta);
        path[i].x = r * cosf(theta);
        path[i].y = r * sinf(theta);
        path[i].z = 40.0f * cosf(5.0f * theta);
    }
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

    // Generate Morph coordinates
    int num_points = 64;
    AnimVec3* path_a = malloc(num_points * sizeof(AnimVec3));
    AnimVec3* path_b = malloc(num_points * sizeof(AnimVec3));
    AnimVec3* path_interp = malloc(num_points * sizeof(AnimVec3));

    make_sphere_path(path_a, num_points);
    make_torus_path(path_b, num_points);

    // Build the semicolon separated morph frames using easings
    char* values_list = malloc(256 * 1024);
    values_list[0] = '\0';

    int frames = 24;
    for (int f = 0; f <= frames; f++) {
        float t = (float)f / (float)frames;
        // Apply cubic ease-in-out to local morph interpolation phasor
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

    // Generate 5 trailing ghost echo layers + 1 main leading layer
    int num_ghosts = 5;
    for (int g = num_ghosts; g >= 0; g--) {
        ZccSvgNode* path = svg_path();
        svg_set_fill(path, "none");
        svg_set_stroke(path, "url(#animGrad)");
        
        // Scale stroke width and opacity for ghosting trail decay
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

        // Calculated phase delay string
        char begin_str[32];
        sprintf(begin_str, "-%.3fs", (float)g * 0.18f);

        // Attach animators and apply phase offset to prevent initialization lag
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
    return 0;
}
