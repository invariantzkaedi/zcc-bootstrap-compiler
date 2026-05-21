#include "zcc_svg.h"
#include "zcc_anim.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

int main() {
    ZccSvgNode* svg = svg_svg();
    svg_set_xmlns(svg, "http://www.w3.org/2000/svg");
    svg_set_width(svg, "800");
    svg_set_height(svg, "800");
    svg_set_viewBox(svg, "0 0 800 800");

    // Add deep dark space background
    ZccSvgNode* bg = svg_rect();
    svg_set_width(bg, "800");
    svg_set_height(bg, "800");
    svg_set_fill(bg, "#020206");
    svg_add_child(svg, bg);

    // Dynamic gradient
    ZccSvgNode* defs = svg_defs();
    ZccSvgNode* grad = svg_linearGradient();
    svg_set_id(grad, "glowGrad");
    svg_set_x1(grad, "0%");
    svg_set_y1(grad, "0%");
    svg_set_x2(grad, "100%");
    svg_set_y2(grad, "100%");

    ZccSvgNode* stop1 = svg_stop();
    svg_set_offset(stop1, "0%");
    svg_set_stop_color(stop1, "#00ffcc"); // Neon Cyan

    ZccSvgNode* stop2 = svg_stop();
    svg_set_offset(stop2, "100%");
    svg_set_stop_color(stop2, "#cc00ff"); // Purple

    svg_add_child(grad, stop1);
    svg_add_child(grad, stop2);
    svg_add_child(defs, grad);
    svg_add_child(svg, defs);

    // Allocate coordinates for Lorenz attractor
    int num_points = 240;
    AnimVec3* lorenz_points = malloc(num_points * sizeof(AnimVec3));
    
    // Generate Lorenz Attractor coordinates: sigma=10, rho=28, beta=8/3, dt=0.015
    anim_generate_lorenz(lorenz_points, num_points, 10.0f, 28.0f, 8.0f/3.0f, 0.015f);

    // Apply fBm noise displacement to the lorenz points to make it organic and fluid!
    for (int i = 0; i < num_points; i++) {
        float disp_x = anim_fbm_3d(lorenz_points[i].x * 0.05f, lorenz_points[i].y * 0.05f, lorenz_points[i].z * 0.05f, 3);
        float disp_y = anim_fbm_3d(lorenz_points[i].y * 0.05f, lorenz_points[i].z * 0.05f, lorenz_points[i].x * 0.05f, 3);
        lorenz_points[i].x += disp_x * 8.0f;
        lorenz_points[i].y += disp_y * 8.0f;
    }

    // Build morph values semicolon separated list
    // Each frame rotates the lookat camera around the lorenz attractor!
    char* values_list = malloc(512 * 1024);
    values_list[0] = '\0';

    int frames = 20;
    for (int f = 0; f < frames; f++) {
        float angle = (f * 2.0f * M_PI) / frames;
        
        // Dynamic lookat camera orbiting around the attractor
        AnimVec3 eye = {
            250.0f * cosf(angle),
            120.0f * sinf(angle * 0.5f),
            250.0f * sinf(angle)
        };
        AnimVec3 target = { 0.0f, 0.0f, -40.0f };
        AnimVec3 up = { 0.0f, 1.0f, 0.0f };

        // Construct matrix view-perspective stack
        AnimMatrix4x4 view = anim_matrix_look_at(eye, target, up);
        AnimMatrix4x4 proj = anim_matrix_perspective(M_PI / 4.0f, 1.0f, 10.0f, 1000.0f);
        
        // Multiply view * projection matrix
        AnimMatrix4x4 view_proj = anim_matrix_identity();
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                float sum = 0.0f;
                for (int k = 0; k < 4; k++) {
                    sum += view.m[i][k] * proj.m[k][j];
                }
                view_proj.m[i][j] = sum;
            }
        }

        // Project and format all points into a path string
        char frame_path[32768];
        frame_path[0] = '\0';
        for (int i = 0; i < num_points; i++) {
            AnimVec3 proj_pt = anim_matrix_project(view_proj, lorenz_points[i], 800.0f, 800.0f);
            char pt_str[64];
            if (i == 0) {
                sprintf(pt_str, "M %.2f %.2f ", proj_pt.x, proj_pt.y);
            } else {
                sprintf(pt_str, "L %.2f %.2f ", proj_pt.x, proj_pt.y);
            }
            strcat(frame_path, pt_str);
        }
        
        strcat(values_list, frame_path);
        if (f < frames - 1) {
            strcat(values_list, ";");
        }
    }

    // Set initial path frame
    AnimVec3 eye_init = { 250.0f, 0.0f, 0.0f };
    AnimVec3 target_init = { 0.0f, 0.0f, -40.0f };
    AnimVec3 up_init = { 0.0f, 1.0f, 0.0f };
    AnimMatrix4x4 view_init = anim_matrix_look_at(eye_init, target_init, up_init);
    AnimMatrix4x4 proj_init = anim_matrix_perspective(M_PI / 4.0f, 1.0f, 10.0f, 1000.0f);
    
    AnimMatrix4x4 vp_init = anim_matrix_identity();
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++) {
                sum += view_init.m[i][k] * proj_init.m[k][j];
            }
            vp_init.m[i][j] = sum;
        }
    }

    char init_path[32768];
    init_path[0] = '\0';
    for (int i = 0; i < num_points; i++) {
        AnimVec3 proj_pt = anim_matrix_project(vp_init, lorenz_points[i], 800.0f, 800.0f);
        char pt_str[64];
        if (i == 0) {
            sprintf(pt_str, "M %.2f %.2f ", proj_pt.x, proj_pt.y);
        } else {
            sprintf(pt_str, "L %.2f %.2f ", proj_pt.x, proj_pt.y);
        }
        strcat(init_path, pt_str);
    }

    // Generate 5 trailing ghost attractor layers + 1 main leading layer
    int num_ghosts = 5;
    for (int g = num_ghosts; g >= 0; g--) {
        ZccSvgNode* path = svg_path();
        svg_set_fill(path, "none");
        svg_set_stroke(path, "url(#glowGrad)");
        
        // Scale stroke width and opacity for ghosting trail decay
        char sw_str[32], op_base_str[32];
        if (g == 0) {
            sprintf(sw_str, "3.0");
            sprintf(op_base_str, "0.95");
        } else {
            sprintf(sw_str, "%.2f", 3.0f / (float)(g + 1));
            sprintf(op_base_str, "%.3f", 0.75f / (float)(g + 1));
        }
        svg_set_stroke_width(path, sw_str);
        svg_set_opacity(path, op_base_str);

        svg_set_attr(path, "d", init_path);

        // Calculated phase delay string
        char begin_str[32];
        sprintf(begin_str, "-%.3fs", (float)g * 0.28f);

        // Attach animators and apply phase offset to prevent initialization lag
        ZccSvgNode* anim_morph = svg_animate_path_morph(path, values_list, 10.0f, "indefinite");
        svg_set_attr(anim_morph, "begin", begin_str);

        ZccSvgNode* anim_rot = svg_animate_transform_rotate(path, 0.0f, 360.0f, 400.0f, 400.0f, 18.0f, "indefinite");
        svg_set_attr(anim_rot, "begin", begin_str);

        float min_op = 0.15f / (float)(g + 1);
        float max_op = 0.95f / (float)(g + 1);
        ZccSvgNode* anim_op = svg_animate_opacity(path, min_op, max_op, 4.0f, "indefinite");
        svg_set_attr(anim_op, "begin", begin_str);

        svg_add_child(svg, path);
    }

    // Render string and write to file
    char* svg_str = svg_to_string(svg);
    FILE* fp = fopen("test_attractor.svg", "w");
    if (fp) {
        fprintf(fp, "%s", svg_str);
        fclose(fp);
    }

    printf("SUCCESS: Deployed test_attractor and generated test_attractor.svg!\n");

    free(lorenz_points);
    free(values_list);
    return 0;
}
