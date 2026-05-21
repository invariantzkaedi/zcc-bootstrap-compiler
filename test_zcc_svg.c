#include "zcc_svg.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    float x, y, z;
} Vec3;

static inline Vec3 rotate_x(Vec3 v, float angle) {
    float c = cos(angle);
    float s = sin(angle);
    Vec3 r = {
        v.x,
        v.y * c - v.z * s,
        v.y * s + v.z * c
    };
    return r;
}

static inline Vec3 rotate_y(Vec3 v, float angle) {
    float c = cos(angle);
    float s = sin(angle);
    Vec3 r = {
        v.x * c + v.z * s,
        v.y,
        -v.x * s + v.z * c
    };
    return r;
}

static inline void project_perspective(Vec3 v, float *x2d, float *y2d, float width, float height) {
    float camera_d = 400.0f;
    float scale = 250.0f;
    float z_offset = 300.0f;
    float denom = v.z + z_offset + camera_d;
    if (denom == 0.0f) denom = 1.0f;
    *x2d = (width / 2.0f) + (v.x * camera_d) / denom * (scale / 100.0f);
    *y2d = (height / 2.0f) + (v.y * camera_d) / denom * (scale / 100.0f);
}

// Generate one frame of path data at phasor t
static inline void generate_frame_path(float t, float width, float height, char* out_buf) {
    // 1. Phasor loops t from 0.0 to 1.0
    // 2. 3D coordinates based on morphed sphere (SDF-like modulation)
    int steps = 72; // fine resolution
    out_buf[0] = '\0';
    
    // Rotation angles driven by phasor
    float rot_x_ang = t * 2.0f * M_PI;
    float rot_y_ang = t * 2.0f * M_PI * 2.0f;
    
    for (int i = 0; i <= steps; i++) {
        float theta = (i * 2.0f * M_PI) / steps;
        
        // Signed Distance Function-like radial modulation (frequency = 5)
        float r = 100.0f + 25.0f * sin(5.0f * theta + t * 2.0f * M_PI);
        
        // 3D coordinates
        Vec3 v;
        v.x = r * cos(theta);
        v.y = r * sin(theta);
        v.z = 30.0f * cos(5.0f * theta - t * 2.0f * M_PI);
        
        // Apply 3D rotations
        v = rotate_x(v, rot_x_ang);
        v = rotate_y(v, rot_y_ang);
        
        // Project to 2D
        float px, py;
        project_perspective(v, &px, &py, width, height);
        
        char pt[64];
        if (i == 0) {
            sprintf(pt, "M %.2f %.2f ", px, py);
        } else {
            sprintf(pt, "L %.2f %.2f ", px, py);
        }
        strcat(out_buf, pt);
    }
    strcat(out_buf, "Z");
}

int main() {
    ZccSvgNode* svg = svg_svg();
    svg_set_xmlns(svg, "http://www.w3.org/2000/svg");
    svg_set_width(svg, "800");
    svg_set_height(svg, "800");
    svg_set_viewBox(svg, "0 0 800 800");
    
    // Add dark cyber background
    ZccSvgNode* bg = svg_rect();
    svg_set_width(bg, "800");
    svg_set_height(bg, "800");
    svg_set_fill(bg, "#050508");
    svg_add_child(svg, bg);
    
    // Defs & Gradients
    ZccSvgNode* defs = svg_defs();
    ZccSvgNode* grad = svg_linearGradient();
    svg_set_id(grad, "cyberGrad");
    svg_set_x1(grad, "0%");
    svg_set_y1(grad, "0%");
    svg_set_x2(grad, "100%");
    svg_set_y2(grad, "100%");
    
    ZccSvgNode* stop1 = svg_stop();
    svg_set_offset(stop1, "0%");
    svg_set_stop_color(stop1, "#00ffcc"); // Neon cyan
    
    ZccSvgNode* stop2 = svg_stop();
    svg_set_offset(stop2, "50%");
    svg_set_stop_color(stop2, "#ff007f"); // Neon pink
    
    ZccSvgNode* stop3 = svg_stop();
    svg_set_offset(stop3, "100%");
    svg_set_stop_color(stop3, "#7f00ff"); // Neon purple
    
    svg_add_child(grad, stop1);
    svg_add_child(grad, stop2);
    svg_add_child(grad, stop3);
    svg_add_child(defs, grad);
    svg_add_child(svg, defs);
    
    // Create paths for morph animation
    // We will generate 12 keyframes across the phasor
    char* values_attr = (char*)malloc(128 * 1024);
    values_attr[0] = '\0';
    
    int num_frames = 12;
    for (int f = 0; f < num_frames; f++) {
        float phasor = (float)f / (float)num_frames;
        char frame_path[8192];
        generate_frame_path(phasor, 800.0f, 800.0f, frame_path);
        
        strcat(values_attr, frame_path);
        if (f < num_frames - 1) {
            strcat(values_attr, ";");
        }
    }
    
    // Add path
    ZccSvgNode* path = svg_path();
    svg_set_fill(path, "none");
    svg_set_stroke(path, "url(#cyberGrad)");
    svg_set_stroke_width(path, "4");
    svg_set_opacity(path, "0.9");
    
    // Initial path shape
    char initial_path[8192];
    generate_frame_path(0.0f, 800.0f, 800.0f, initial_path);
    svg_set_attr(path, "d", initial_path);
    
    // SMIL Morph Animation driven by the compiled C phasor
    ZccSvgNode* anim = svg_animate();
    svg_set_attributeName(anim, "d");
    svg_set_dur(anim, "4s");
    svg_set_repeatCount(anim, "indefinite");
    svg_set_attr(anim, "values", values_attr);
    
    svg_add_child(path, anim);
    svg_add_child(svg, path);
    
    // Render and output
    char* svg_str = svg_to_string(svg);
    
    // Write out directly to a file so it can be previewed
    FILE* fp = fopen("hybrid_omnified.svg", "w");
    if (fp) {
        fprintf(fp, "%s", svg_str);
        fclose(fp);
    }
    
    printf("Successfully compiled and generated hybrid_omnified.svg\n");

    // Upgrade verification: Test the new Base64 ASCII capabilities
    char* b64_uri = svg_to_data_uri(svg);
    if (b64_uri) {
        FILE* fp_b64 = fopen("hybrid_omnified_b64.txt", "w");
        if (fp_b64) {
            fprintf(fp_b64, "%s", b64_uri);
            fclose(fp_b64);
            printf("Successfully generated Base64 Data URI: hybrid_omnified_b64.txt (length: %zu)\n", strlen(b64_uri));
        }

        // Test symmetric decoding to verify absolute correctness
        const char* prefix = "data:image/svg+xml;base64,";
        size_t prefix_len = strlen(prefix);
        if (strncmp(b64_uri, prefix, prefix_len) == 0) {
            size_t dec_len = 0;
            unsigned char* dec_str = base64_decode(b64_uri + prefix_len, strlen(b64_uri + prefix_len), &dec_len);
            if (dec_str) {
                if (strcmp((char*)dec_str, svg_str) == 0) {
                    printf("Symmetric Base64 Decode validation: SUCCESS (decoded data matches original exact SVG string!)\n");
                } else {
                    printf("Symmetric Base64 Decode validation: FAILED (mismatch between original and decoded)\n");
                }
                free(dec_str);
            } else {
                printf("Symmetric Base64 Decode validation: FAILED (decode returned NULL)\n");
            }
        }
        free(b64_uri);
    }

    char* html_uri = svg_to_html_uri(svg);
    if (html_uri) {
        FILE* fp_html = fopen("hybrid_omnified_html_b64.txt", "w");
        if (fp_html) {
            fprintf(fp_html, "%s", html_uri);
            fclose(fp_html);
            printf("Successfully generated HTML Base64 Data URI: hybrid_omnified_html_b64.txt (length: %zu)\n", strlen(html_uri));
        }
        free(html_uri);
    }
    
    free(svg_str);
    free(values_attr);
    return 0;
}
