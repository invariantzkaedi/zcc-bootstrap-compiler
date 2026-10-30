/* Auto-generated ZCC SVG Deep Knowledge Library */
#ifndef ZCC_SVG_H
#define ZCC_SVG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct ZccSvgNode {
    const char* tag;
    char* attributes;
    char* content;
    struct ZccSvgNode* next;
    struct ZccSvgNode* children;
} ZccSvgNode;

ZccSvgNode* svg_create_node(const char* tag);
void svg_add_child(ZccSvgNode* parent, ZccSvgNode* child);
void svg_set_attr(ZccSvgNode* node, const char* attr_name, const char* attr_val);
void svg_set_content(ZccSvgNode* node, const char* text);
char* svg_to_string(ZccSvgNode* root);
void svg_free_node_tree(ZccSvgNode* node);

/* Base64 & ASCII Utility Functions */
char* base64_encode(const unsigned char* data, size_t input_length);
unsigned char* base64_decode(const char* data, size_t input_length, size_t* output_length);
char* svg_to_base64(ZccSvgNode* root);
char* svg_to_data_uri(ZccSvgNode* root);
char* svg_to_html_uri(ZccSvgNode* root);
char* hexdump_to_ascii(const unsigned char* data, size_t len);
char* hexdump_to_svg(const unsigned char* data, size_t len);

/* Path Builder Utility — Full SVG 1.1 / SVG 2 Command Set */
typedef struct SvgPathBuilder {
    char* d;
    int cap;
    int len;
} SvgPathBuilder;
SvgPathBuilder* svg_path_begin(void);
void svg_path_move_to(SvgPathBuilder* pb, float x, float y);
void svg_path_move_rel(SvgPathBuilder* pb, float dx, float dy);
void svg_path_line_to(SvgPathBuilder* pb, float x, float y);
void svg_path_line_rel(SvgPathBuilder* pb, float dx, float dy);
void svg_path_horiz_to(SvgPathBuilder* pb, float x);
void svg_path_horiz_rel(SvgPathBuilder* pb, float dx);
void svg_path_vert_to(SvgPathBuilder* pb, float y);
void svg_path_vert_rel(SvgPathBuilder* pb, float dy);
void svg_path_cubic_to(SvgPathBuilder* pb, float x1, float y1, float x2, float y2, float x, float y);
void svg_path_cubic_rel(SvgPathBuilder* pb, float dx1, float dy1, float dx2, float dy2, float dx, float dy);
void svg_path_smooth_cubic_to(SvgPathBuilder* pb, float x2, float y2, float x, float y);
void svg_path_smooth_cubic_rel(SvgPathBuilder* pb, float dx2, float dy2, float dx, float dy);
void svg_path_quad_to(SvgPathBuilder* pb, float x1, float y1, float x, float y);
void svg_path_quad_rel(SvgPathBuilder* pb, float dx1, float dy1, float dx, float dy);
void svg_path_smooth_quad_to(SvgPathBuilder* pb, float x, float y);
void svg_path_smooth_quad_rel(SvgPathBuilder* pb, float dx, float dy);
void svg_path_arc_to(SvgPathBuilder* pb, float rx, float ry, float x_axis_rot, int large_arc, int sweep, float x, float y);
void svg_path_arc_rel(SvgPathBuilder* pb, float rx, float ry, float x_axis_rot, int large_arc, int sweep, float dx, float dy);
void svg_path_close(SvgPathBuilder* pb);
void svg_path_close_rel(SvgPathBuilder* pb);
void svg_path_circle(SvgPathBuilder* pb, float cx, float cy, float r);
void svg_path_ellipse(SvgPathBuilder* pb, float cx, float cy, float rx, float ry);
void svg_path_rect_rounded(SvgPathBuilder* pb, float x, float y, float w, float h, float rx, float ry);
void svg_path_regular_polygon(SvgPathBuilder* pb, float cx, float cy, float r, int sides, float start_angle);
void svg_path_star(SvgPathBuilder* pb, float cx, float cy, float r_outer, float r_inner, int points, float start_angle);
void svg_path_harmonic_rose(SvgPathBuilder* pb, float cx, float cy, float base_r, float amp1, int k1, float amp2, int k2, int segments);
char* svg_path_compact_d(const char* d_in);
void svg_apply_path(ZccSvgNode* node, SvgPathBuilder* pb);

/* ZCC AST Visualizer Utilities */
struct ZCCNode;
char* svg_render_ast(struct ZCCNode* root);
char* svg_render_ast_html_uri(struct ZCCNode* root);
char* zcc_ast_to_ascii(struct ZCCNode* root);

/* Node Generators */
ZccSvgNode* svg_animate(void);
ZccSvgNode* svg_animateMotion(void);
ZccSvgNode* svg_animateTransform(void);
ZccSvgNode* svg_circle(void);
ZccSvgNode* svg_clipPath(void);
ZccSvgNode* svg_defs(void);
ZccSvgNode* svg_desc(void);
ZccSvgNode* svg_ellipse(void);
ZccSvgNode* svg_feBlend(void);
ZccSvgNode* svg_feColorMatrix(void);
ZccSvgNode* svg_feComponentTransfer(void);
ZccSvgNode* svg_feComposite(void);
ZccSvgNode* svg_feConvolveMatrix(void);
ZccSvgNode* svg_feDiffuseLighting(void);
ZccSvgNode* svg_feDisplacementMap(void);
ZccSvgNode* svg_feFlood(void);
ZccSvgNode* svg_feGaussianBlur(void);
ZccSvgNode* svg_feImage(void);
ZccSvgNode* svg_feMerge(void);
ZccSvgNode* svg_feMergeNode(void);
ZccSvgNode* svg_feMorphology(void);
ZccSvgNode* svg_feOffset(void);
ZccSvgNode* svg_feSpecularLighting(void);
ZccSvgNode* svg_feTile(void);
ZccSvgNode* svg_feTurbulence(void);
ZccSvgNode* svg_filter(void);
ZccSvgNode* svg_g(void);
ZccSvgNode* svg_image(void);
ZccSvgNode* svg_line(void);
ZccSvgNode* svg_linearGradient(void);
ZccSvgNode* svg_mask(void);
ZccSvgNode* svg_mpath(void);
ZccSvgNode* svg_path(void);
ZccSvgNode* svg_pattern(void);
ZccSvgNode* svg_polygon(void);
ZccSvgNode* svg_polyline(void);
ZccSvgNode* svg_radialGradient(void);
ZccSvgNode* svg_rect(void);
ZccSvgNode* svg_script(void);
ZccSvgNode* svg_set(void);
ZccSvgNode* svg_stop(void);
ZccSvgNode* svg_style(void);
ZccSvgNode* svg_svg(void);
ZccSvgNode* svg_switch(void);
ZccSvgNode* svg_symbol(void);
ZccSvgNode* svg_text(void);
ZccSvgNode* svg_textPath(void);
ZccSvgNode* svg_title(void);
ZccSvgNode* svg_tspan(void);
ZccSvgNode* svg_use(void);

/* Attribute Setters */
void svg_set_accumulate(ZccSvgNode* node, const char* val);
void svg_set_additive(ZccSvgNode* node, const char* val);
void svg_set_attributeName(ZccSvgNode* node, const char* val);
void svg_set_attributeType(ZccSvgNode* node, const char* val);
void svg_set_begin(ZccSvgNode* node, const char* val);
void svg_set_by(ZccSvgNode* node, const char* val);
void svg_set_calcMode(ZccSvgNode* node, const char* val);
void svg_set_class(ZccSvgNode* node, const char* val);
void svg_set_clip_path(ZccSvgNode* node, const char* val);
void svg_set_color(ZccSvgNode* node, const char* val);
void svg_set_cx(ZccSvgNode* node, const char* val);
void svg_set_cy(ZccSvgNode* node, const char* val);
void svg_set_d(ZccSvgNode* node, const char* val);
void svg_set_dominant_baseline(ZccSvgNode* node, const char* val);
void svg_set_dur(ZccSvgNode* node, const char* val);
void svg_set_end(ZccSvgNode* node, const char* val);
void svg_set_fill(ZccSvgNode* node, const char* val);
void svg_set_fill_opacity(ZccSvgNode* node, const char* val);
void svg_set_fill_rule(ZccSvgNode* node, const char* val);
void svg_set_filter(ZccSvgNode* node, const char* val);
void svg_set_font_family(ZccSvgNode* node, const char* val);
void svg_set_font_size(ZccSvgNode* node, const char* val);
void svg_set_font_style(ZccSvgNode* node, const char* val);
void svg_set_font_weight(ZccSvgNode* node, const char* val);
void svg_set_from(ZccSvgNode* node, const char* val);
void svg_set_gradientTransform(ZccSvgNode* node, const char* val);
void svg_set_gradientUnits(ZccSvgNode* node, const char* val);
void svg_set_height(ZccSvgNode* node, const char* val);
void svg_set_href(ZccSvgNode* node, const char* val);
void svg_set_id(ZccSvgNode* node, const char* val);
void svg_set_keyPoints(ZccSvgNode* node, const char* val);
void svg_set_keySplines(ZccSvgNode* node, const char* val);
void svg_set_keyTimes(ZccSvgNode* node, const char* val);
void svg_set_mask(ZccSvgNode* node, const char* val);
void svg_set_max(ZccSvgNode* node, const char* val);
void svg_set_min(ZccSvgNode* node, const char* val);
void svg_set_offset(ZccSvgNode* node, const char* val);
void svg_set_onbegin(ZccSvgNode* node, const char* val);
void svg_set_onend(ZccSvgNode* node, const char* val);
void svg_set_onrepeat(ZccSvgNode* node, const char* val);
void svg_set_opacity(ZccSvgNode* node, const char* val);
void svg_set_path(ZccSvgNode* node, const char* val);
void svg_set_patternTransform(ZccSvgNode* node, const char* val);
void svg_set_patternUnits(ZccSvgNode* node, const char* val);
void svg_set_points(ZccSvgNode* node, const char* val);
void svg_set_preserveAspectRatio(ZccSvgNode* node, const char* val);
void svg_set_r(ZccSvgNode* node, const char* val);
void svg_set_repeatCount(ZccSvgNode* node, const char* val);
void svg_set_repeatDur(ZccSvgNode* node, const char* val);
void svg_set_restart(ZccSvgNode* node, const char* val);
void svg_set_rotate(ZccSvgNode* node, const char* val);
void svg_set_rx(ZccSvgNode* node, const char* val);
void svg_set_ry(ZccSvgNode* node, const char* val);
void svg_set_stop_color(ZccSvgNode* node, const char* val);
void svg_set_stop_opacity(ZccSvgNode* node, const char* val);
void svg_set_stroke(ZccSvgNode* node, const char* val);
void svg_set_stroke_dasharray(ZccSvgNode* node, const char* val);
void svg_set_stroke_dashoffset(ZccSvgNode* node, const char* val);
void svg_set_stroke_linecap(ZccSvgNode* node, const char* val);
void svg_set_stroke_linejoin(ZccSvgNode* node, const char* val);
void svg_set_stroke_miterlimit(ZccSvgNode* node, const char* val);
void svg_set_stroke_opacity(ZccSvgNode* node, const char* val);
void svg_set_stroke_width(ZccSvgNode* node, const char* val);
void svg_set_style(ZccSvgNode* node, const char* val);
void svg_set_systemLanguage(ZccSvgNode* node, const char* val);
void svg_set_target(ZccSvgNode* node, const char* val);
void svg_set_text_anchor(ZccSvgNode* node, const char* val);
void svg_set_to(ZccSvgNode* node, const char* val);
void svg_set_transform(ZccSvgNode* node, const char* val);
void svg_set_type(ZccSvgNode* node, const char* val);
void svg_set_values(ZccSvgNode* node, const char* val);
void svg_set_viewBox(ZccSvgNode* node, const char* val);
void svg_set_width(ZccSvgNode* node, const char* val);
void svg_set_x(ZccSvgNode* node, const char* val);
void svg_set_x1(ZccSvgNode* node, const char* val);
void svg_set_x2(ZccSvgNode* node, const char* val);
void svg_set_xlink_href(ZccSvgNode* node, const char* val);
void svg_set_xmlns(ZccSvgNode* node, const char* val);
void svg_set_y(ZccSvgNode* node, const char* val);
void svg_set_y1(ZccSvgNode* node, const char* val);
void svg_set_y2(ZccSvgNode* node, const char* val);

#endif
