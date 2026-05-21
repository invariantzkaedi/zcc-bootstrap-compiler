#include "zcc_svg.h"
#include "zcc_anim.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

// EVM Exploit Topology basic block representation
typedef struct {
    Vec3 pos;
    float radius;
    const char* label;
    const char* opcode;
    const char* color;
} EvmNode;

typedef struct {
    int from;
    int to;
    float radius;
    const char* color;
} EvmEdge;

#define NUM_NODES 6
static EvmNode nodes[NUM_NODES] = {
    {{0.0f, 15.0f, 0.0f}, 4.2f, "0x00_ENTRY", "PUSH1 0x80 / MSTORE", "#ff00ff"},       // Entry
    {{-15.0f, 5.0f, -5.0f}, 3.8f, "0x15_SELECTOR", "CALLDATALOAD / EQ", "#00ffff"},    // Selector
    {{15.0f, 5.0f, 5.0f}, 4.5f, "0x42_REENTRANCY", "CALL (REENTRANT)", "#ff007f"},      // Exploit Target (Hot Pink)
    {{5.0f, -8.0f, -8.0f}, 3.6f, "0x60_MUTATION", "SSTORE (BALANCES)", "#ffaa00"},      // Delayed Mutation (Orange)
    {{-10.0f, -15.0f, 10.0f}, 4.0f, "0x88_DRAIN", "TRANSFER / GAS", "#ff0033"},         // Drain Payload (Red)
    {{18.0f, -12.0f, -5.0f}, 3.5f, "0x99_SAFE_EXIT", "REVERT / RETURN", "#00ff66"}      // Safe Exit (Green)
};

#define NUM_EDGES 6
static EvmEdge edges[NUM_EDGES] = {
    {0, 1, 0.7f, "url(#glowGrad)"},   // ENTRY -> SELECTOR
    {1, 2, 0.7f, "url(#glowGrad)"},   // SELECTOR -> REENTRANCY
    {1, 5, 0.6f, "url(#greenGrad)"},  // SELECTOR -> SAFE_EXIT
    {2, 3, 0.7f, "url(#glowGrad)"},   // REENTRANCY -> MUTATION
    {3, 4, 0.7f, "url(#glowGrad)"},   // MUTATION -> DRAIN
    {4, 2, 0.8f, "url(#exploitGrad)"} // DRAIN -> REENTRANCY (recursive reentrancy loop!)
};

// Computes dynamic, orbiting, and pulsing node positions at phase t
static void get_node_pos(int index, float phase, Vec3* pos, float* radius) {
    EvmNode base = nodes[index];
    *radius = base.radius;
    *pos = base.pos;

    // Apply multi-axis orbital mechanics and beat-synced expansion
    if (index == 2) { // REENTRANCY (Violently pulsing)
        *radius = base.radius * (1.0f + 0.32f * sinf(phase * 4.0f * M_PI));
        float r = 16.0f + 3.0f * sinf(phase * 2.0f * M_PI);
        pos->x = nodes[1].pos.x + r * cosf(phase * 2.0f * M_PI);
        pos->z = nodes[1].pos.z + r * sinf(phase * 2.0f * M_PI);
    } else if (index == 4) { // DRAIN (Vortex spiral)
        float decay = 1.0f - 0.25f * sinf(phase * M_PI);
        *radius = base.radius * decay;
        pos->y = base.pos.y + 5.0f * sinf(phase * 6.0f * M_PI);
        pos->x = base.pos.x + 3.0f * cosf(phase * 4.0f * M_PI);
    } else if (index == 3) { // MUTATION (Orbiting the exploit hub)
        float r = 7.5f;
        Vec3 target;
        float target_rad;
        get_node_pos(2, phase, &target, &target_rad);
        pos->x = target.x + r * cosf(phase * 4.0f * M_PI);
        pos->y = target.y + r * sinf(phase * 4.0f * M_PI);
    }
}

// 3D Rotated scene SDF evaluator with smooth blending
static float evm_scene_sdf(Vec3 p, ChaosPhasor* ph) {
    Vec3 pr = p;
    // Master scene rotation
    rotate_y(&pr, (float)ph->phase * 2.0f * M_PI);

    float d = 1e9f;

    // Dynamic animated node positions for this frame
    Vec3 anim_pos[NUM_NODES];
    float anim_rad[NUM_NODES];
    for (int i = 0; i < NUM_NODES; i++) {
        get_node_pos(i, (float)ph->phase, &anim_pos[i], &anim_rad[i]);
    }

    // 1. Evaluate basic block nodes (metaball smooth union blend!)
    for (int i = 0; i < NUM_NODES; i++) {
        float dx = pr.x - anim_pos[i].x;
        float dy = pr.y - anim_pos[i].y;
        float dz = pr.z - anim_pos[i].z;
        float d_node = sqrtf(dx*dx + dy*dy + dz*dz) - anim_rad[i];
        d = sdSmoothUnion(d, d_node, 3.2f);
    }

    // 2. Evaluate execution flow edges (capsules)
    for (int i = 0; i < NUM_EDGES; i++) {
        Vec3 a = anim_pos[edges[i].from];
        Vec3 b = anim_pos[edges[i].to];
        float d_edge = sdCapsule(pr, a, b, edges[i].radius);
        d = sdSmoothUnion(d, d_edge, 2.5f);
    }

    // 3. Extruded Koch Snowflake shield around Entry
    float koch = sdKochSnowflake((Vec3){pr.x - anim_pos[0].x, pr.y - anim_pos[0].y, pr.z - anim_pos[0].z}, 2.5f, 4, 0.4f);
    d = sdSmoothUnion(d, koch * 2.0f, 1.8f);

    // Ripple noise
    float ripple = anim_fbm_3d(pr.x * 0.2f, pr.y * 0.2f, pr.z * 0.2f, 4);
    d += ripple * 0.15f;

    return d;
}

static Vec3 evm_get_normal(Vec3 p, ChaosPhasor* ph) {
    Vec3 e = {EPSILON, 0.0f, 0.0f};
    Vec3 n = {
        evm_scene_sdf((Vec3){p.x + e.x, p.y, p.z}, ph) - evm_scene_sdf((Vec3){p.x - e.x, p.y, p.z}, ph),
        evm_scene_sdf((Vec3){p.x, p.y + e.x, p.z}, ph) - evm_scene_sdf((Vec3){p.x, p.y - e.x, p.z}, ph),
        evm_scene_sdf((Vec3){p.x, p.y, p.z + e.x}, ph) - evm_scene_sdf((Vec3){p.x, p.y, p.z - e.x}, ph)
    };
    normalize(&n);
    return n;
}

static float evm_ray_march(Ray r, ChaosPhasor* ph, Vec3* hit, Vec3* normal) {
    float depth = 0.0f;
    for (int i = 0; i < MAX_STEPS; ++i) {
        Vec3 p = {r.ro.x + r.rd.x * depth, r.ro.y + r.rd.y * depth, r.ro.z + r.rd.z * depth};
        float dist = evm_scene_sdf(p, ph);
        depth += dist;
        if (dist < SURF_DIST) {
            *hit = p;
            *normal = evm_get_normal(p, ph);
            return depth;
        }
        if (depth > MAX_DIST) break;
    }
    return MAX_DIST;
}

int main() {
    // 1. Initialize root SVG document
    ZccSvgNode* svg = svg_svg();
    svg_set_width(svg, "800");
    svg_set_height(svg, "800");
    svg_set_viewBox(svg, "0 0 800 800");
    svg_set_xmlns(svg, "http://www.w3.org/2000/svg");

    // Add space black obsidian background
    ZccSvgNode* bg = svg_rect();
    svg_set_attr(bg, "width", "100%");
    svg_set_attr(bg, "height", "100%");
    svg_set_fill(bg, "#020205");
    svg_add_child(svg, bg);

    // Setup linear gradients
    ZccSvgNode* defs = svg_defs();
    
    ZccSvgNode* glowGrad = svg_linearGradient();
    svg_set_id(glowGrad, "glowGrad");
    svg_set_attr(glowGrad, "x1", "0%"); svg_set_attr(glowGrad, "y1", "0%");
    svg_set_attr(glowGrad, "x2", "100%"); svg_set_attr(glowGrad, "y2", "100%");
    ZccSvgNode* stop1 = svg_stop(); svg_set_offset(stop1, "0%"); svg_set_stop_color(stop1, "#ff007f"); svg_add_child(glowGrad, stop1);
    ZccSvgNode* stop2 = svg_stop(); svg_set_offset(stop2, "100%"); svg_set_stop_color(stop2, "#00ffff"); svg_add_child(glowGrad, stop2);
    svg_add_child(defs, glowGrad);

    ZccSvgNode* greenGrad = svg_linearGradient();
    svg_set_id(greenGrad, "greenGrad");
    svg_set_attr(greenGrad, "x1", "0%"); svg_set_attr(greenGrad, "y1", "0%");
    svg_set_attr(greenGrad, "x2", "100%"); svg_set_attr(greenGrad, "y2", "100%");
    ZccSvgNode* stop3 = svg_stop(); svg_set_offset(stop3, "0%"); svg_set_stop_color(stop3, "#00ffff"); svg_add_child(greenGrad, stop3);
    ZccSvgNode* stop4 = svg_stop(); svg_set_offset(stop4, "100%"); svg_set_stop_color(stop4, "#00ff66"); svg_add_child(greenGrad, stop4);
    svg_add_child(defs, greenGrad);

    ZccSvgNode* exploitGrad = svg_linearGradient();
    svg_set_id(exploitGrad, "exploitGrad");
    svg_set_attr(exploitGrad, "x1", "0%"); svg_set_attr(exploitGrad, "y1", "0%");
    svg_set_attr(exploitGrad, "x2", "100%"); svg_set_attr(exploitGrad, "y2", "100%");
    ZccSvgNode* stop5 = svg_stop(); svg_set_offset(stop5, "0%"); svg_set_stop_color(stop5, "#ff0011"); svg_add_child(exploitGrad, stop5);
    ZccSvgNode* stop6 = svg_stop(); svg_set_offset(stop6, "100%"); svg_set_stop_color(stop6, "#ff00ff"); svg_add_child(exploitGrad, stop6);
    svg_add_child(defs, exploitGrad);

    svg_add_child(svg, defs);

    // Setup 30 frames for ultra-fluid 60fps-like loop motion
    int num_frames = 30;
    char* values_list = (char*)malloc(3 * 1024 * 1024);
    values_list[0] = '\0';

    char* first_frame_path = (char*)malloc(512 * 1024);
    first_frame_path[0] = '\0';

    // Buffers to hold projected HUD coordinates across all 30 frames for SMIL positional lock-on!
    char* circle_cx[NUM_NODES];
    char* circle_cy[NUM_NODES];
    char* text_x[NUM_NODES];
    char* text_y[NUM_NODES];
    
    for (int i = 0; i < NUM_NODES; i++) {
        circle_cx[i] = (char*)malloc(32768); circle_cx[i][0] = '\0';
        circle_cy[i] = (char*)malloc(32768); circle_cy[i][0] = '\0';
        text_x[i] = (char*)malloc(32768); text_x[i][0] = '\0';
        text_y[i] = (char*)malloc(32768); text_y[i][0] = '\0';
    }

    // Set up camera matrix
    AnimVec3 eye = {0.0f, 0.0f, 52.0f};
    AnimVec3 target = {0.0f, 0.0f, 0.0f};
    AnimVec3 up = {0.0f, 1.0f, 0.0f};
    AnimMatrix4x4 view = anim_matrix_look_at(eye, target, up);
    AnimMatrix4x4 proj = anim_matrix_perspective(M_PI / 3.0f, 1.0f, 0.1f, 100.0f);
    AnimMatrix4x4 vp;
    anim_matrix_mul(&vp, &view, &proj);

    ChaosPhasor ph;
    chaos_phasor_init(&ph, 120.0);

    printf("Starting CPU Ray Marcher for BUTTERY-SMOOTH EVM Exploit Topology visualizer...\n");

    for (int f = 0; f < num_frames; f++) {
        printf("  Processing Frame %d/%d...\n", f + 1, num_frames);

        ph.phase = (double)f / (double)num_frames;

        SVGPath* path = svg_path_create();
        Vec3 light_dir = {0.5f, 0.7f, -0.5f};
        normalize(&light_dir);

        int width = 800, height = 800;
        int grid_step = 16; 

        for (int y = 0; y < height; y += grid_step) {
            for (int x = 0; x < width; x += grid_step) {
                Vec3 ro = {0.0f, 0.0f, 52.0f};
                Vec3 rd = {(x - width / 2.0f) / (float)width * 1.1f, (y - height / 2.0f) / (float)height * -1.1f, -1.0f};
                normalize(&rd);

                Vec3 hit, normal;
                float d = evm_ray_march((Ray){ro, rd}, &ph, &hit, &normal);

                if (d < MAX_DIST) {
                    float shade = fmaxf(0.18f, dot(normal, light_dir));
                    Vec3 view_dir = {-rd.x, -rd.y, -rd.z};
                    Vec3 half_dir = {light_dir.x + view_dir.x, light_dir.y + view_dir.y, light_dir.z + view_dir.z};
                    normalize(&half_dir);
                    float spec = powf(fmaxf(0.0f, dot(normal, half_dir)), 12.0f);
                    float intensity = shade + spec * 0.8f;

                    float sx = (float)x;
                    float sy = (float)y;
                    float ex = sx + normal.x * intensity * 14.0f;
                    float ey = sy - normal.y * intensity * 14.0f;

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

        // Accumulate projected 2D coordinates for lock-on tracking text overlays!
        for (int i = 0; i < NUM_NODES; i++) {
            Vec3 rpos;
            float rad;
            get_node_pos(i, (float)f / (float)num_frames, &rpos, &rad);
            
            // Apply master scene rotation to match raymarch perspective
            rotate_y(&rpos, (float)f / (float)num_frames * 2.0f * M_PI);
            
            AnimVec3 screen = anim_matrix_project(vp, rpos, 800.0f, 800.0f);
            
            char tmp[64];
            sprintf(tmp, "%.2f", screen.x);
            strcat(circle_cx[i], tmp);
            
            sprintf(tmp, "%.2f", screen.y);
            strcat(circle_cy[i], tmp);
            
            sprintf(tmp, "%.2f", screen.x - 45.0f);
            strcat(text_x[i], tmp);
            
            sprintf(tmp, "%.2f", screen.y - 15.0f);
            strcat(text_y[i], tmp);
            
            if (f < num_frames - 1) {
                strcat(circle_cx[i], ";");
                strcat(circle_cy[i], ";");
                strcat(text_x[i], ";");
                strcat(text_y[i], ";");
            }
        }

        svg_path_free(path);
    }

    // Deploy 5 trailing decaying ghost echo vector paths
    int num_ghosts = 5;
    for (int g = num_ghosts; g >= 0; g--) {
        ZccSvgNode* morph_shape = svg_path();
        svg_set_fill(morph_shape, "none");
        svg_set_stroke(morph_shape, "url(#glowGrad)");
        svg_set_stroke_linecap(morph_shape, "round");

        char sw_str[32], op_str[32];
        if (g == 0) {
            sprintf(sw_str, "2.4");
            sprintf(op_str, "0.95");
        } else {
            sprintf(sw_str, "%.2f", 2.4f / (float)(g + 1));
            sprintf(op_str, "%.3f", 0.75f / (float)(g + 1));
        }

        svg_set_stroke_width(morph_shape, sw_str);
        svg_set_opacity(morph_shape, op_str);
        svg_set_attr(morph_shape, "d", first_frame_path);

        // Inject morph animators stagger trigger start phases
        ZccSvgNode* anim = svg_animate_path_morph(morph_shape, values_list, 5.0f, "indefinite");
        char begin_str[32];
        sprintf(begin_str, "-%.3fs", (float)g * 0.166f); // 30-frame aligned delay
        svg_set_attr(anim, "begin", begin_str);

        svg_add_child(svg, morph_shape);
    }

    // Deploy projected HUD tracking overlay
    ZccSvgNode* hud_group = svg_create_node("g");
    svg_set_attr(hud_group, "font-family", "'Courier New', monospace");
    svg_set_attr(hud_group, "font-weight", "bold");
    svg_add_child(svg, hud_group);

    for (int i = 0; i < NUM_NODES; i++) {
        // 1. Lock-on indicator circle
        ZccSvgNode* node_circle = svg_circle();
        svg_set_attr(node_circle, "fill", nodes[i].color);
        svg_set_attr(node_circle, "opacity", "0.18");
        char r_str[32];
        sprintf(r_str, "%.2f", nodes[i].radius * 2.8f);
        svg_set_attr(node_circle, "r", r_str);
        
        ZccSvgNode* cx_anim = svg_create_node("animate");
        svg_set_attr(cx_anim, "attributeName", "cx");
        svg_set_attr(cx_anim, "dur", "5.0s");
        svg_set_attr(cx_anim, "values", circle_cx[i]);
        svg_set_attr(cx_anim, "repeatCount", "indefinite");
        svg_add_child(node_circle, cx_anim);
        
        ZccSvgNode* cy_anim = svg_create_node("animate");
        svg_set_attr(cy_anim, "attributeName", "cy");
        svg_set_attr(cy_anim, "dur", "5.0s");
        svg_set_attr(cy_anim, "values", circle_cy[i]);
        svg_set_attr(cy_anim, "repeatCount", "indefinite");
        svg_add_child(node_circle, cy_anim);
        
        svg_add_child(hud_group, node_circle);
        
        // 2. Lock-on primary label
        ZccSvgNode* text_node = svg_text();
        svg_set_attr(text_node, "font-size", "11px");
        svg_set_attr(text_node, "fill", nodes[i].color);
        svg_set_content(text_node, nodes[i].label);
        
        ZccSvgNode* tx_anim = svg_create_node("animate");
        svg_set_attr(tx_anim, "attributeName", "x");
        svg_set_attr(tx_anim, "dur", "5.0s");
        svg_set_attr(tx_anim, "values", text_x[i]);
        svg_set_attr(tx_anim, "repeatCount", "indefinite");
        svg_add_child(text_node, tx_anim);
        
        ZccSvgNode* ty_anim = svg_create_node("animate");
        svg_set_attr(ty_anim, "attributeName", "y");
        svg_set_attr(ty_anim, "dur", "5.0s");
        svg_set_attr(ty_anim, "values", text_y[i]);
        svg_set_attr(ty_anim, "repeatCount", "indefinite");
        svg_add_child(text_node, ty_anim);
        
        svg_add_child(hud_group, text_node);
        
        // 3. Lock-on opcode sub-label
        ZccSvgNode* sub_text = svg_text();
        svg_set_attr(sub_text, "font-size", "8.5px");
        svg_set_attr(sub_text, "fill", "#8888aa");
        svg_set_content(sub_text, nodes[i].opcode);
        
        // Slightly offset the lock-on tracking sub-label downwards
        char* sub_y_list = (char*)malloc(32768);
        sub_y_list[0] = '\0';
        char* text_y_copy = strdup(text_y[i]);
        char* token = strtok(text_y_copy, ";");
        while (token) {
            float py = atof(token);
            char tmp[64];
            sprintf(tmp, "%.2f", py + 13.0f);
            strcat(sub_y_list, tmp);
            token = strtok(NULL, ";");
            if (token) strcat(sub_y_list, ";");
        }
        
        ZccSvgNode* stx_anim = svg_create_node("animate");
        svg_set_attr(stx_anim, "attributeName", "x");
        svg_set_attr(stx_anim, "dur", "5.0s");
        svg_set_attr(stx_anim, "values", text_x[i]);
        svg_set_attr(stx_anim, "repeatCount", "indefinite");
        svg_add_child(sub_text, stx_anim);
        
        ZccSvgNode* sty_anim = svg_create_node("animate");
        svg_set_attr(sty_anim, "attributeName", "y");
        svg_set_attr(sty_anim, "dur", "5.0s");
        svg_set_attr(sty_anim, "values", sub_y_list);
        svg_set_attr(sty_anim, "repeatCount", "indefinite");
        svg_add_child(sub_text, sty_anim);
        
        svg_add_child(hud_group, sub_text);
        
        free(text_y_copy);
        free(sub_y_list);
    }

    // Static forensic metadata decorations
    ZccSvgNode* border = svg_rect();
    svg_set_attr(border, "x", "15");
    svg_set_attr(border, "y", "15");
    svg_set_attr(border, "width", "770");
    svg_set_attr(border, "height", "770");
    svg_set_fill(border, "none");
    svg_set_attr(border, "stroke", "#333355");
    svg_set_attr(border, "stroke-width", "1.5");
    svg_set_attr(border, "opacity", "0.4");
    svg_add_child(svg, border);

    ZccSvgNode* corner_lbl = svg_text();
    svg_set_attr(corner_lbl, "x", "30");
    svg_set_attr(corner_lbl, "y", "40");
    svg_set_attr(corner_lbl, "font-size", "12px");
    svg_set_attr(corner_lbl, "fill", "#00ff7f");
    svg_set_content(corner_lbl, "🔱 ZKAEDI OMNI-FORENSICS: EVM SYMBOLIC CFG VMAX-EXPLODE-0.2");
    svg_add_child(svg, corner_lbl);

    ZccSvgNode* loop_lbl = svg_text();
    svg_set_attr(loop_lbl, "x", "30");
    svg_set_attr(loop_lbl, "y", "58");
    svg_set_attr(loop_lbl, "font-size", "9px");
    svg_set_attr(loop_lbl, "fill", "#ff0055");
    svg_set_content(loop_lbl, "CRITICAL DETECTED TRANSITION: REENTRANCY RECURSIVE ATTRACTOR LOOP [ACTIVE]");
    svg_add_child(svg, loop_lbl);

    // Save finalized vector asset
    char* xml = svg_to_string(svg);
    FILE* fp = fopen("test_evm_topology.svg", "w");
    if (fp) {
        fputs(xml, fp);
        fclose(fp);
        printf("SUCCESS: Deployed test_evm_topology and generated test_evm_topology.svg!\n");
    } else {
        printf("ERROR: Failed to write test_evm_topology.svg\n");
    }

    // clean up buffers cleanly to prevent any memory leaks
    for (int i = 0; i < NUM_NODES; i++) {
        free(circle_cx[i]);
        free(circle_cy[i]);
        free(text_x[i]);
        free(text_y[i]);
    }

    free(xml);
    free(first_frame_path);
    free(values_list);

    return 0;
}
