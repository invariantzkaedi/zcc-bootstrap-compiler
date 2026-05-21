#include "zcc_svg.h"
#include "zcc_anim.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

// Custom override for closing anim paths since zcc_anim.h lacks a macro mapping
#undef svg_path_close
#define svg_path_close(p) do { \
    strcat((p)->d, "Z "); \
} while(0)

// Helper to generate a morphing wave path string for a given phase
static void make_wave_path(char* buf, float phase, float width, float height, float amplitude, float frequency) {
    SVGPath* p = svg_path_create();
    float mid_y = height / 2.0f;
    
    // Start at bottom left, go up to wave start
    svg_path_move_to(p, 0.0f, height);
    svg_path_line_to(p, 0.0f, mid_y + amplitude * sinf(-phase * 2.0f * M_PI));
    
    for (float x = 4.0f; x <= width; x += 4.0f) {
        float angle = (x / width) * frequency * 2.0f * M_PI - phase * 2.0f * M_PI;
        float y = mid_y + amplitude * sinf(angle);
        svg_path_line_to(p, x, y);
    }
    
    // Close the shape to the bottom right and left for a beautiful liquid-fill effect
    svg_path_line_to(p, width, height);
    svg_path_line_to(p, 0.0f, height);
    
    strcpy(buf, p->d);
    svg_path_free(p);
}

// Helper to generate a morphing star/fractal path string
static void make_star_path(char* buf, float phase, float cx, float cy, float r_in, float r_out, int points) {
    SVGPath* p = svg_path_create();
    float angle_step = M_PI / (float)points;
    
    for (int i = 0; i < points * 2; i++) {
        float angle = (float)i * angle_step + phase * 2.0f * M_PI;
        float r = (i % 2 == 0) ? r_out : r_in;
        // Modulate outer radius based on phase for breathing effect
        if (i % 2 == 0) {
            r += 10.0f * sinf(phase * 4.0f * M_PI + (float)i);
        }
        float x = cx + r * cosf(angle);
        float y = cy + r * sinf(angle);
        
        if (i == 0) {
            svg_path_move_to(p, x, y);
        } else {
            svg_path_line_to(p, x, y);
        }
    }
    svg_path_close(p);
    strcpy(buf, p->d);
    svg_path_free(p);
}

int main() {
    ZccSvgNode* svg = svg_svg();
    svg_set_width(svg, "800");
    svg_set_height(svg, "800");
    svg_set_viewBox(svg, "0 0 800 800");
    svg_set_xmlns(svg, "http://www.w3.org/2000/svg");

    ZccSvgNode* bg = svg_rect();
    svg_set_attr(bg, "width", "100%");
    svg_set_attr(bg, "height", "100%");
    svg_set_fill(bg, "#030206");
    svg_add_child(svg, bg);

    // Glowing Neon Filters and Gradients
    ZccSvgNode* defs = svg_defs();
    
    // Neon Cyan glow
    ZccSvgNode* filter_cyan = svg_create_node("filter");
    svg_set_id(filter_cyan, "cyanGlow");
    ZccSvgNode* blur1 = svg_create_node("feGaussianBlur");
    svg_set_attr(blur1, "stdDeviation", "5");
    svg_set_attr(blur1, "result", "coloredBlur");
    ZccSvgNode* merge1 = svg_create_node("feMerge");
    ZccSvgNode* node1 = svg_create_node("feMergeNode");
    svg_set_attr(node1, "in", "coloredBlur");
    ZccSvgNode* node2 = svg_create_node("feMergeNode");
    svg_set_attr(node2, "in", "SourceGraphic");
    svg_add_child(merge1, node1);
    svg_add_child(merge1, node2);
    svg_add_child(filter_cyan, blur1);
    svg_add_child(filter_cyan, merge1);
    svg_add_child(defs, filter_cyan);

    // Neon Pink glow
    ZccSvgNode* filter_pink = svg_create_node("filter");
    svg_set_id(filter_pink, "pinkGlow");
    ZccSvgNode* blur2 = svg_create_node("feGaussianBlur");
    svg_set_attr(blur2, "stdDeviation", "6");
    svg_set_attr(blur2, "result", "coloredBlur");
    ZccSvgNode* merge2 = svg_create_node("feMerge");
    ZccSvgNode* node3 = svg_create_node("feMergeNode");
    svg_set_attr(node3, "in", "coloredBlur");
    ZccSvgNode* node4 = svg_create_node("feMergeNode");
    svg_set_attr(node4, "in", "SourceGraphic");
    svg_add_child(merge2, node3);
    svg_add_child(merge2, node4);
    svg_add_child(filter_pink, blur2);
    svg_add_child(filter_pink, merge2);
    svg_add_child(defs, filter_pink);

    // Gradients
    ZccSvgNode* cyanGrad = svg_linearGradient();
    svg_set_id(cyanGrad, "cyanGrad");
    svg_set_attr(cyanGrad, "x1", "0%"); svg_set_attr(cyanGrad, "y1", "0%");
    svg_set_attr(cyanGrad, "x2", "100%"); svg_set_attr(cyanGrad, "y2", "0%");
    ZccSvgNode* cstop1 = svg_stop(); svg_set_offset(cstop1, "0%"); svg_set_stop_color(cstop1, "rgba(0, 255, 204, 0.1)"); svg_add_child(cyanGrad, cstop1);
    ZccSvgNode* cstop2 = svg_stop(); svg_set_offset(cstop2, "100%"); svg_set_stop_color(cstop2, "rgba(0, 255, 204, 0.95)"); svg_add_child(cyanGrad, cstop2);
    svg_add_child(defs, cyanGrad);

    ZccSvgNode* pinkGrad = svg_linearGradient();
    svg_set_id(pinkGrad, "pinkGrad");
    svg_set_attr(pinkGrad, "x1", "0%"); svg_set_attr(pinkGrad, "y1", "0%");
    svg_set_attr(pinkGrad, "x2", "100%"); svg_set_attr(pinkGrad, "y2", "100%");
    ZccSvgNode* pstop1 = svg_stop(); svg_set_offset(pstop1, "0%"); svg_set_stop_color(pstop1, "#ff007f"); svg_add_child(pinkGrad, pstop1);
    ZccSvgNode* pstop2 = svg_stop(); svg_set_offset(pstop2, "100%"); svg_set_stop_color(pstop2, "#00ffcc"); svg_add_child(pinkGrad, pstop2);
    svg_add_child(defs, pinkGrad);

    svg_add_child(svg, defs);

    // -------------------------------------------------------------
    // LOADER 1: PGO LIQUID WAVE SCANNER (Sine wave morphing filled bar)
    // -------------------------------------------------------------
    ZccSvgNode* l1_group = svg_create_node("g");
    svg_set_attr(l1_group, "transform", "translate(100, 100)");
    
    // Border shell
    ZccSvgNode* l1_border = svg_rect();
    svg_set_attr(l1_border, "width", "600");
    svg_set_attr(l1_border, "height", "60");
    svg_set_attr(l1_border, "rx", "12");
    svg_set_fill(l1_border, "none");
    svg_set_attr(l1_border, "stroke", "#1a1426");
    svg_set_attr(l1_border, "stroke-width", "2");
    svg_add_child(l1_group, l1_border);

    // Clip path for liquid fill boundary
    ZccSvgNode* clip = svg_create_node("clipPath");
    svg_set_id(clip, "l1_clip");
    ZccSvgNode* clip_rect = svg_rect();
    svg_set_attr(clip_rect, "width", "596");
    svg_set_attr(clip_rect, "height", "56");
    svg_set_attr(clip_rect, "x", "2");
    svg_set_attr(clip_rect, "y", "2");
    svg_set_attr(clip_rect, "rx", "10");
    svg_add_child(clip, clip_rect);
    svg_add_child(l1_group, clip);

    // Liquid fill with morphing sine waves
    int wave_frames = 20;
    char* wave_values = (char*)malloc(1024 * 1024);
    wave_values[0] = '\0';
    char* first_wave = (char*)malloc(131072);
    first_wave[0] = '\0';

    for (int f = 0; f < wave_frames; f++) {
        float phase = (float)f / (float)wave_frames;
        char temp[32768];
        make_wave_path(temp, phase, 600.0f, 60.0f, 12.0f, 2.5f);
        if (f == 0) strcpy(first_wave, temp);
        strcat(wave_values, temp);
        if (f < wave_frames - 1) strcat(wave_values, ";");
    }

    ZccSvgNode* liquid = svg_path();
    svg_set_attr(liquid, "clip-path", "url(#l1_clip)");
    svg_set_fill(liquid, "url(#cyanGrad)");
    svg_set_attr(liquid, "filter", "url(#cyanGlow)");
    svg_set_attr(liquid, "d", first_wave);

    ZccSvgNode* l1_anim = svg_create_node("animate");
    svg_set_attr(l1_anim, "attributeName", "d");
    svg_set_attr(l1_anim, "dur", "2.5s");
    svg_set_attr(l1_anim, "repeatCount", "indefinite");
    svg_set_attr(l1_anim, "values", wave_values);
    svg_add_child(liquid, l1_anim);
    svg_add_child(l1_group, liquid);

    // Dynamic scanning dot over top
    ZccSvgNode* scan_dot = svg_circle();
    svg_set_attr(scan_dot, "r", "6");
    svg_set_attr(scan_dot, "fill", "#00ffcc");
    svg_set_attr(scan_dot, "filter", "url(#cyanGlow)");
    svg_set_attr(scan_dot, "cy", "30");
    ZccSvgNode* scan_anim = svg_create_node("animate");
    svg_set_attr(scan_anim, "attributeName", "cx");
    svg_set_attr(scan_anim, "from", "20");
    svg_set_attr(scan_anim, "to", "580");
    svg_set_attr(scan_anim, "dur", "2.0s");
    svg_set_attr(scan_anim, "repeatCount", "indefinite");
    svg_set_attr(scan_anim, "keyTimes", "0;0.5;1");
    // Feature engineered custom easing back-and-forth swing
    svg_set_attr(scan_anim, "values", "20;580;20");
    svg_add_child(scan_dot, scan_anim);
    svg_add_child(l1_group, scan_dot);

    ZccSvgNode* l1_text = svg_text();
    svg_set_attr(l1_text, "x", "20");
    svg_set_attr(l1_text, "y", "-15");
    svg_set_attr(l1_text, "font-family", "'Space Mono', monospace");
    svg_set_attr(l1_text, "font-size", "12px");
    svg_set_attr(l1_text, "fill", "#8b8599");
    svg_set_content(l1_text, "STAGE_01 // PGO_PROFILE_OPTIMIZATION_FLOW");
    svg_add_child(l1_group, l1_text);

    ZccSvgNode* l1_pct = svg_text();
    svg_set_attr(l1_pct, "x", "580");
    svg_set_attr(l1_pct, "y", "-15");
    svg_set_attr(l1_pct, "font-family", "'Space Mono', monospace");
    svg_set_attr(l1_pct, "font-size", "12px");
    svg_set_attr(l1_pct, "fill", "#00ffcc");
    svg_set_attr(l1_pct, "text-anchor", "end");
    svg_set_content(l1_pct, "74.82% OPTIMIZED");
    svg_add_child(l1_group, l1_pct);

    svg_add_child(svg, l1_group);

    // -------------------------------------------------------------
    // LOADER 2: REGISTER ALLOCATION CONCENTRIC SPIN SPINNER
    // -------------------------------------------------------------
    ZccSvgNode* l2_group = svg_create_node("g");
    svg_set_attr(l2_group, "transform", "translate(220, 360)");

    // Outer Orbit Ring
    ZccSvgNode* outer_ring = svg_circle();
    svg_set_attr(outer_ring, "cx", "0");
    svg_set_attr(outer_ring, "cy", "0");
    svg_set_attr(outer_ring, "r", "80");
    svg_set_fill(outer_ring, "none");
    svg_set_attr(outer_ring, "stroke", "#ff007f");
    svg_set_attr(outer_ring, "stroke-width", "2");
    svg_set_attr(outer_ring, "stroke-dasharray", "120 40 20 40");
    svg_set_attr(outer_ring, "filter", "url(#pinkGlow)");
    ZccSvgNode* rot_anim1 = svg_create_node("animateTransform");
    svg_set_attr(rot_anim1, "attributeName", "transform");
    svg_set_attr(rot_anim1, "type", "rotate");
    svg_set_attr(rot_anim1, "from", "0");
    svg_set_attr(rot_anim1, "to", "360");
    svg_set_attr(rot_anim1, "dur", "4.0s");
    svg_set_attr(rot_anim1, "repeatCount", "indefinite");
    svg_add_child(outer_ring, rot_anim1);
    svg_add_child(l2_group, outer_ring);

    // Inner Counter-rotating Orbit Ring
    ZccSvgNode* inner_ring = svg_circle();
    svg_set_attr(inner_ring, "cx", "0");
    svg_set_attr(inner_ring, "cy", "0");
    svg_set_attr(inner_ring, "r", "56");
    svg_set_fill(inner_ring, "none");
    svg_set_attr(inner_ring, "stroke", "#00ffcc");
    svg_set_attr(inner_ring, "stroke-width", "1.5");
    svg_set_attr(inner_ring, "stroke-dasharray", "80 20 40 20");
    svg_set_attr(inner_ring, "filter", "url(#cyanGlow)");
    ZccSvgNode* rot_anim2 = svg_create_node("animateTransform");
    svg_set_attr(rot_anim2, "attributeName", "transform");
    svg_set_attr(rot_anim2, "type", "rotate");
    svg_set_attr(rot_anim2, "from", "360");
    svg_set_attr(rot_anim2, "to", "0");
    svg_set_attr(rot_anim2, "dur", "3.0s");
    svg_set_attr(rot_anim2, "repeatCount", "indefinite");
    svg_add_child(inner_ring, rot_anim2);
    svg_add_child(l2_group, inner_ring);

    // Central pulsing reactor node
    ZccSvgNode* reactor = svg_circle();
    svg_set_attr(reactor, "cx", "0");
    svg_set_attr(reactor, "cy", "0");
    svg_set_attr(reactor, "r", "25");
    svg_set_fill(reactor, "#ff007f");
    svg_set_attr(reactor, "filter", "url(#pinkGlow)");
    ZccSvgNode* pulse_anim = svg_create_node("animate");
    svg_set_attr(pulse_anim, "attributeName", "r");
    svg_set_attr(pulse_anim, "values", "25;32;25");
    svg_set_attr(pulse_anim, "dur", "1.8s");
    svg_set_attr(pulse_anim, "repeatCount", "indefinite");
    svg_add_child(reactor, pulse_anim);
    svg_add_child(l2_group, reactor);

    ZccSvgNode* l2_text = svg_text();
    svg_set_attr(l2_text, "x", "-120");
    svg_set_attr(l2_text, "y", "-20");
    svg_set_attr(l2_text, "font-family", "'Space Mono', monospace");
    svg_set_attr(l2_text, "font-size", "12px");
    svg_set_attr(l2_text, "fill", "#8b8599");
    svg_set_attr(l2_text, "text-anchor", "end");
    svg_set_content(l2_text, "STAGE_02 // REGALLOC_CONCENTRIC_CHIP_LOCK");
    svg_add_child(l2_group, l2_text);

    ZccSvgNode* l2_desc1 = svg_text();
    svg_set_attr(l2_desc1, "x", "-120");
    svg_set_attr(l2_desc1, "y", "5");
    svg_set_attr(l2_desc1, "font-family", "'Space Mono', monospace");
    svg_set_attr(l2_desc1, "font-size", "11px");
    svg_set_attr(l2_desc1, "fill", "#ff007f");
    svg_set_attr(l2_desc1, "text-anchor", "end");
    svg_set_content(l2_desc1, "CHAITIN_BRIGGS: ACTIVE (INTERVALS RESOLVED)");
    svg_add_child(l2_group, l2_desc1);

    ZccSvgNode* l2_desc2 = svg_text();
    svg_set_attr(l2_desc2, "x", "-120");
    svg_set_attr(l2_desc2, "y", "25");
    svg_set_attr(l2_desc2, "font-family", "'Space Mono', monospace");
    svg_set_attr(l2_desc2, "font-size", "10px");
    svg_set_attr(l2_desc2, "fill", "#00ffcc");
    svg_set_attr(l2_desc2, "text-anchor", "end");
    svg_set_content(l2_desc2, "VIRTUAL_REGISTERS: 124,760 Lowered");
    svg_add_child(l2_group, l2_desc2);

    svg_add_child(svg, l2_group);

    // -------------------------------------------------------------
    // LOADER 3: SEGMENTED DECAY BAR (Symbolic execution tracer)
    // -------------------------------------------------------------
    ZccSvgNode* l3_group = svg_create_node("g");
    svg_set_attr(l3_group, "transform", "translate(100, 560)");

    // Define 15 horizontal block segments
    int segments = 15;
    float seg_width = 34.0f;
    float seg_gap = 6.0f;
    float start_x = 0.0f;

    for (int i = 0; i < segments; i++) {
        ZccSvgNode* seg = svg_rect();
        char x_str[32], y_str[32];
        sprintf(x_str, "%.2f", start_x + (float)i * (seg_width + seg_gap));
        svg_set_attr(seg, "x", x_str);
        svg_set_attr(seg, "y", "0");
        svg_set_attr(seg, "width", "34");
        svg_set_attr(seg, "height", "36");
        svg_set_attr(seg, "rx", "5");
        
        // Cycle colors from Cyan to Pink
        if (i < 8) {
            svg_set_fill(seg, "#00ffcc");
            svg_set_attr(seg, "filter", "url(#cyanGlow)");
        } else {
            svg_set_fill(seg, "#ff007f");
            svg_set_attr(seg, "filter", "url(#pinkGlow)");
        }

        // Add staggered pulse animation
        ZccSvgNode* seg_anim = svg_create_node("animate");
        svg_set_attr(seg_anim, "attributeName", "opacity");
        svg_set_attr(seg_anim, "values", "0.25;1.0;0.25");
        svg_set_attr(seg_anim, "dur", "2.2s");
        char begin_str[32];
        sprintf(begin_str, "%.2fs", (float)i * 0.12f);
        svg_set_attr(seg_anim, "begin", begin_str);
        svg_set_attr(seg_anim, "repeatCount", "indefinite");
        svg_add_child(seg, seg_anim);

        svg_add_child(l3_group, seg);
    }

    ZccSvgNode* l3_text = svg_text();
    svg_set_attr(l3_text, "x", "20");
    svg_set_attr(l3_text, "y", "-20");
    svg_set_attr(l3_text, "font-family", "'Space Mono', monospace");
    svg_set_attr(l3_text, "font-size", "12px");
    svg_set_attr(l3_text, "fill", "#8b8599");
    svg_set_content(l3_text, "STAGE_03 // SEGMENTED_TELEMETRY_PIPELINE");
    svg_add_child(l3_group, l3_text);

    ZccSvgNode* l3_pct = svg_text();
    svg_set_attr(l3_pct, "x", "580");
    svg_set_attr(l3_pct, "y", "-20");
    svg_set_attr(l3_pct, "font-family", "'Space Mono', monospace");
    svg_set_attr(l3_pct, "font-size", "12px");
    svg_set_attr(l3_pct, "fill", "#ff007f");
    svg_set_attr(l3_pct, "text-anchor", "end");
    svg_set_content(l3_pct, "ACTIVE RUNNING");
    svg_add_child(l3_group, l3_pct);

    svg_add_child(svg, l3_group);

    // -------------------------------------------------------------
    // LOADER 4: FRACTAL BREATHING GATEWAYS spinner
    // -------------------------------------------------------------
    ZccSvgNode* l4_group = svg_create_node("g");
    svg_set_attr(l4_group, "transform", "translate(580, 360)");

    // Morphing star fractal
    int star_frames = 20;
    char* star_values = (char*)malloc(1024 * 1024);
    star_values[0] = '\0';
    char* first_star = (char*)malloc(131072);
    first_star[0] = '\0';

    for (int f = 0; f < star_frames; f++) {
        float phase = (float)f / (float)star_frames;
        char temp[32768];
        make_star_path(temp, phase, 0.0f, 0.0f, 32.0f, 64.0f, 8);
        if (f == 0) strcpy(first_star, temp);
        strcat(star_values, temp);
        if (f < star_frames - 1) strcat(star_values, ";");
    }

    ZccSvgNode* star_shape = svg_path();
    svg_set_fill(star_shape, "none");
    svg_set_attr(star_shape, "stroke", "url(#pinkGrad)");
    svg_set_attr(star_shape, "stroke-width", "3.0");
    svg_set_attr(star_shape, "filter", "url(#pinkGlow)");
    svg_set_attr(star_shape, "d", first_star);

    ZccSvgNode* l4_anim = svg_create_node("animate");
    svg_set_attr(l4_anim, "attributeName", "d");
    svg_set_attr(l4_anim, "dur", "4.0s");
    svg_set_attr(l4_anim, "repeatCount", "indefinite");
    svg_set_attr(l4_anim, "values", star_values);
    svg_add_child(star_shape, l4_anim);
    svg_add_child(l4_group, star_shape);

    ZccSvgNode* l4_desc = svg_text();
    svg_set_attr(l4_desc, "x", "120");
    svg_set_attr(l4_desc, "y", "5");
    svg_set_attr(l4_desc, "font-family", "'Space Mono', monospace");
    svg_set_attr(l4_desc, "font-size", "11px");
    svg_set_attr(l4_desc, "fill", "#ff007f");
    svg_set_attr(l4_desc, "text-anchor", "start");
    svg_set_content(l4_desc, "FRACTAL_GATE: OPENING");
    svg_add_child(l4_group, l4_desc);

    ZccSvgNode* l4_sub = svg_text();
    svg_set_attr(l4_sub, "x", "120");
    svg_set_attr(l4_sub, "y", "25");
    svg_set_attr(l4_sub, "font-family", "'Space Mono', monospace");
    svg_set_attr(l4_sub, "font-size", "10px");
    svg_set_attr(l4_sub, "fill", "#00ffcc");
    svg_set_attr(l4_sub, "text-anchor", "start");
    svg_set_content(l4_sub, "SELF_HOSTING_VERIFIED");
    svg_add_child(l4_group, l4_sub);

    svg_add_child(svg, l4_group);

    // Decorative dashboard grids and frames
    ZccSvgNode* border = svg_rect();
    svg_set_attr(border, "x", "15");
    svg_set_attr(border, "y", "15");
    svg_set_attr(border, "width", "770");
    svg_set_attr(border, "height", "770");
    svg_set_fill(border, "none");
    svg_set_attr(border, "stroke", "#1a1426");
    svg_set_attr(border, "stroke-width", "2");
    svg_add_child(svg, border);

    ZccSvgNode* main_title = svg_text();
    svg_set_attr(main_title, "x", "30");
    svg_set_attr(main_title, "y", "40");
    svg_set_attr(main_title, "font-family", "'Space Mono', monospace");
    svg_set_attr(main_title, "font-weight", "bold");
    svg_set_attr(main_title, "font-size", "14px");
    svg_set_attr(main_title, "fill", "#00ffcc");
    svg_set_content(main_title, "🔱 ZKAEDI VMAX: NEXT GENERATION LOADERS & OPTIMIZATION PROGRESS TRACKERS");
    svg_add_child(svg, main_title);

    // Save final loaders output svg
    char* xml = svg_to_string(svg);
    FILE* fp = fopen("test_loading_bars.svg", "w");
    if (fp) {
        fputs(xml, fp);
        fclose(fp);
        printf("SUCCESS: Generated next-generation test_loading_bars.svg!\n");
    }

    free(first_star); free(star_values);
    free(first_wave); free(wave_values);
    free(xml);
    return 0;
}
