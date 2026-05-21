#include "zcc_svg.h"
#include "midi_data.h"
#include <stdio.h>
#include <math.h>

int main() {
    // 1. Base Canvas
    ZccSvgNode* svg = svg_svg();
    svg_set_width(svg, "1920");
    svg_set_height(svg, "1080");
    svg_set_viewBox(svg, "0 0 1920 1080");
    svg_set_style(svg, "background-color: #020205;");
    
    // Total Duration of the track
    float dur = bass_count > 0 ? bass_times[bass_count-1] + 2.0 : 60.0;
    char dur_str[32];
    sprintf(dur_str, "%.2fs", dur);

    // 2. The Bass-Reactive Core (Strange Attractor)
    ZccSvgNode* p = svg_path();
    svg_set_fill(p, "none");
    svg_set_stroke(p, "magenta");
    svg_set_stroke_width(p, "1");
    svg_set_opacity(p, "0.9");
    
    // Generate a beautiful geometry (5000 points)
    SvgPathBuilder* pb = svg_path_begin();
    double a = -1.4, b = 1.6, c = 1.0, d = 0.7;
    double x = 0.0, y = 0.0;
    svg_path_move_to(pb, 960, 540);
    for (int i = 0; i < 8000; i++) {
        double nx = sin(a * y) + c * cos(a * x);
        double ny = sin(b * x) + d * cos(b * y);
        x = nx; y = ny;
        svg_path_line_to(pb, 960 + (x * 250), 540 + (y * 250));
    }
    svg_apply_path(p, pb);
    
    // We construct a massive keyTimes/values array for the Bass
    // To make the attractor violently scale on the 808 hits
    char* scale_vals = (char*)malloc(1024 * 1024);
    char* scale_times = (char*)malloc(1024 * 1024);
    strcpy(scale_vals, "1"); strcpy(scale_times, "0");
    int v_len = 1, t_len = 1;
    
    for (int i = 0; i < bass_count; i++) {
        float hit = bass_times[i];
        float norm_time = hit / dur;
        
        // Before hit
        char buf[128];
        int l = sprintf(buf, "; 1"); strcpy(scale_vals + v_len, buf); v_len += l;
        l = sprintf(buf, "; %.4f", norm_time - 0.001); strcpy(scale_times + t_len, buf); t_len += l;
        
        // On hit (scale up based on velocity)
        float s = 1.0 + (bass_vels[i] / 127.0) * 0.8;
        l = sprintf(buf, "; %.2f", s); strcpy(scale_vals + v_len, buf); v_len += l;
        l = sprintf(buf, "; %.4f", norm_time); strcpy(scale_times + t_len, buf); t_len += l;
        
        // Decay
        l = sprintf(buf, "; 1"); strcpy(scale_vals + v_len, buf); v_len += l;
        l = sprintf(buf, "; %.4f", norm_time + 0.05); strcpy(scale_times + t_len, buf); t_len += l;
    }
    // End frame
    strcpy(scale_vals + v_len, "; 1");
    strcpy(scale_times + t_len, "; 1");

    // Apply the SMIL Animation
    ZccSvgNode* anim = svg_animateTransform();
    svg_set_attributeName(anim, "transform");
    svg_set_attributeType(anim, "XML");
    svg_set_type(anim, "scale");
    svg_set_dur(anim, dur_str);
    svg_set_repeatCount(anim, "1");
    svg_set_values(anim, scale_vals);
    svg_set_keyTimes(anim, scale_times);
    
    // Transform origin fix (SMIL scale transforms around 0,0, so we wrap in a G)
    ZccSvgNode* g = svg_create_node("g");
    svg_set_transform(g, "translate(960, 540)"); // Move to center
    
    // Shift path to be centered at 0,0 for scaling
    ZccSvgNode* p_centered = svg_path();
    svg_set_fill(p_centered, "none");
    svg_set_stroke(p_centered, "magenta");
    svg_set_stroke_width(p_centered, "0.5");
    pb = svg_path_begin();
    x = 0; y = 0;
    svg_path_move_to(pb, 0, 0);
    for (int i = 0; i < 8000; i++) {
        double nx = sin(a * y) + c * cos(a * x);
        double ny = sin(b * x) + d * cos(b * y);
        x = nx; y = ny;
        svg_path_line_to(pb, x * 350, y * 350);
    }
    svg_apply_path(p_centered, pb);
    
    svg_add_child(p_centered, anim); // The path scales
    svg_add_child(g, p_centered);
    svg_add_child(svg, g);

    // 3. Drum Triggers (Flash Opacity)
    ZccSvgNode* flash_rect = svg_rect();
    svg_set_attr(flash_rect, "x", "0"); svg_set_attr(flash_rect, "y", "0");
    svg_set_width(flash_rect, "1920"); svg_set_height(flash_rect, "1080");
    svg_set_fill(flash_rect, "cyan");
    
    char* op_vals = (char*)malloc(1024 * 1024);
    char* op_times = (char*)malloc(1024 * 1024);
    strcpy(op_vals, "0"); strcpy(op_times, "0");
    v_len = 1; t_len = 1;
    
    for (int i = 0; i < drum_count; i++) {
        float hit = drum_times[i];
        float norm = hit / dur;
        char buf[128]; int l;
        l = sprintf(buf, "; 0"); strcpy(op_vals + v_len, buf); v_len += l;
        l = sprintf(buf, "; %.4f", norm - 0.001); strcpy(op_times + t_len, buf); t_len += l;
        
        float op = (drum_vels[i] / 127.0) * 0.3; // Max 30% flash
        l = sprintf(buf, "; %.2f", op); strcpy(op_vals + v_len, buf); v_len += l;
        l = sprintf(buf, "; %.4f", norm); strcpy(op_times + t_len, buf); t_len += l;
        
        l = sprintf(buf, "; 0"); strcpy(op_vals + v_len, buf); v_len += l;
        l = sprintf(buf, "; %.4f", norm + 0.02); strcpy(op_times + t_len, buf); t_len += l;
    }
    strcpy(op_vals + v_len, "; 0");
    strcpy(op_times + t_len, "; 1");
    
    ZccSvgNode* anim_op = svg_animate();
    svg_set_attributeName(anim_op, "opacity");
    svg_set_dur(anim_op, dur_str);
    svg_set_values(anim_op, op_vals);
    svg_set_keyTimes(anim_op, op_times);
    
    svg_add_child(flash_rect, anim_op);
    svg_add_child(svg, flash_rect);

    char* output = svg_to_string(svg);
    printf("%s\n", output);
    return 0;
}
