#include "zcc_svg.h"
#include "zcc_anim.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main() {
    // 1. Initialize our parent SVG document
    ZccSvgNode* svg = svg_svg();
    svg_set_width(svg, "800");
    svg_set_height(svg, "800");
    svg_set_viewBox(svg, "0 0 800 800");
    svg_set_xmlns(svg, "http://www.w3.org/2000/svg");
    
    // Add a dark space-grade glassmorphism background
    ZccSvgNode* rect = svg_rect();
    svg_set_attr(rect, "width", "100%");
    svg_set_attr(rect, "height", "100%");
    svg_set_fill(rect, "#030308");
    svg_add_child(svg, rect);

    // Setup linear gradients for high-fidelity glowing strokes
    ZccSvgNode* defs = svg_defs();
    ZccSvgNode* grad = svg_linearGradient();
    svg_set_id(grad, "glowGrad");
    svg_set_attr(grad, "x1", "0%");
    svg_set_attr(grad, "y1", "0%");
    svg_set_attr(grad, "x2", "100%");
    svg_set_attr(grad, "y2", "100%");
    
    ZccSvgNode* stop1 = svg_stop();
    svg_set_offset(stop1, "0%");
    svg_set_stop_color(stop1, "#ff007f"); // Hot Pink
    svg_add_child(grad, stop1);
    
    ZccSvgNode* stop2 = svg_stop();
    svg_set_offset(stop2, "100%");
    svg_set_stop_color(stop2, "#00ffcc"); // Neon Cyan
    svg_add_child(grad, stop2);
    
    svg_add_child(defs, grad);
    svg_add_child(svg, defs);

    // Load the torus OBJ mesh procedurally
    ObjMesh mesh;
    obj_load_torus(&mesh);

    // Allocate a large heap space for morph targets (values list for SMIL animation)
    char* values_list = (char*)malloc(2 * 1024 * 1024);
    values_list[0] = '\0';
    
    int num_frames = 15;
    ChaosPhasor ph;
    chaos_phasor_init(&ph, 120.0); // 120.0 BPM Suno beat!

    printf("Starting CPU 3D Hybrid SDF & OBJ Mesh Ray Marcher...\n");
    
    char* first_frame_path = (char*)malloc(256 * 1024);
    first_frame_path[0] = '\0';

    for (int f = 0; f < num_frames; f++) {
        printf("  Rendering frame %d/%d...\n", f + 1, num_frames);
        
        // Advance phasor stage
        ph.phase = (double)f / (double)num_frames;
        
        // Raymarch the entire SDF screen at this time slice
        SVGPath* path = svg_path_create();
        Vec3 light_dir = {0.6f, 0.8f, -0.4f};
        normalize(&light_dir);

        int width = 800, height = 800;
        int grid_step = 16; // Perfectly optimized visual spacing

        for (int y = 0; y < height; y += grid_step) {
            for (int x = 0; x < width; x += grid_step) {
                Vec3 ro = {0.0f, 0.0f, 60.0f}; // Position camera
                Vec3 rd = {(x - width / 2.0f) / (float)width, (y - height / 2.0f) / (float)height * -1.0f, -1.0f};
                normalize(&rd);
                
                Vec3 hit, normal;
                float d = ray_march((Ray){ro, rd}, &ph, &mesh, &hit, &normal);
                
                if (d < MAX_DIST) {
                    float shade = fmaxf(0.15f, dot(normal, light_dir));
                    // Dynamic highlight specular calculation
                    Vec3 view_dir = {-rd.x, -rd.y, -rd.z};
                    Vec3 half_dir = {light_dir.x + view_dir.x, light_dir.y + view_dir.y, light_dir.z + view_dir.z};
                    normalize(&half_dir);
                    float spec = powf(fmaxf(0.0f, dot(normal, half_dir)), 16.0f);
                    
                    float intensity = shade + spec * 0.7f;
                    
                    // Generate vector ticks aligned to the normal projection!
                    float sx = (float)x;
                    float sy = (float)y;
                    float ex = sx + normal.x * intensity * 12.0f;
                    float ey = sy - normal.y * intensity * 12.0f; // Flip y for screen coords
                    
                    svg_path_move_to(path, sx, sy);
                    svg_path_line_to(path, ex, ey);
                }
            }
        }
        
        if (f == 0) {
            strcpy(first_frame_path, path->d);
        }
        
        strcat(values_list, path->d);
        if (f < num_frames - 1) {
            strcat(values_list, ";");
        }
        
        svg_path_free(path);
    }

    // Generate 5 trailing ghost raymarched layers + 1 main leading layer
    int num_ghosts = 5;
    for (int g = num_ghosts; g >= 0; g--) {
        ZccSvgNode* morph_shape = svg_path();
        svg_set_fill(morph_shape, "none");
        svg_set_stroke(morph_shape, "url(#glowGrad)");
        svg_set_stroke_linecap(morph_shape, "round");
        
        // Scale stroke width and opacity for ghosting trail decay
        char sw_str[32], op_str[32];
        if (g == 0) {
            sprintf(sw_str, "2.2");
            sprintf(op_str, "0.9");
        } else {
            sprintf(sw_str, "%.2f", 2.2f / (float)(g + 1));
            sprintf(op_str, "%.3f", 0.70f / (float)(g + 1));
        }
        svg_set_stroke_width(morph_shape, sw_str);
        svg_set_opacity(morph_shape, op_str);
        svg_set_attr(morph_shape, "d", first_frame_path);

        // Inject morph animator and apply delayed start phase offset
        ZccSvgNode* anim = svg_animate_path_morph(morph_shape, values_list, 4.0f, "indefinite");
        char begin_str[32];
        sprintf(begin_str, "-%.3fs", (float)g * 0.20f);
        svg_set_attr(anim, "begin", begin_str);

        svg_add_child(svg, morph_shape);
    }

    // Growing botanical L-System branches framing the scene
    ZccSvgNode* l_system_group = svg_create_node("g");
    svg_set_attr(l_system_group, "stroke", "#9400d3");
    svg_set_attr(l_system_group, "stroke-width", "1.5");
    svg_set_attr(l_system_group, "opacity", "0.3");
    
    anim_draw_branch_recursive(l_system_group, 400.0f, 780.0f, 85.0f, -M_PI / 2.0f, 0, 7);
    svg_add_child(svg, l_system_group);

    // Save the fully compiled live raymarched SVG to disk
    char* xml = svg_to_string(svg);
    FILE* fp = fopen("test_raymarch.svg", "w");
    if (fp) {
        fputs(xml, fp);
        fclose(fp);
        printf("SUCCESS: Deployed test_raymarch and generated test_raymarch.svg!\n");
    } else {
        printf("ERROR: Failed to write test_raymarch.svg\n");
    }

    // Free all dynamic variables and heap-allocated objects cleanly
    obj_mesh_free(&mesh);
    free(xml);
    free(first_frame_path);
    free(values_list);
    return 0;
}
