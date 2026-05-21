/* Auto-generated ZCC SVG Master Library */
#ifndef ZCC_SVG_H
#define ZCC_SVG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct SvgAttr {
    char* name;
    char* value;
    struct SvgAttr* next;
} SvgAttr;

typedef struct ZccSvgNode {
    const char* tag;
    SvgAttr* attrs;
    char* content;
    struct ZccSvgNode* next;
    struct ZccSvgNode* children;
} ZccSvgNode;

typedef struct SvgPathBuilder {
    char data[8192];
} SvgPathBuilder;

static inline ZccSvgNode* svg_create_node(const char* tag) {
    ZccSvgNode* n = (ZccSvgNode*)calloc(1, sizeof(ZccSvgNode));
    n->tag = tag;
    return n;
}

static inline void svg_add_child(ZccSvgNode* parent, ZccSvgNode* child) {
    if (!parent->children) {
        parent->children = child;
    } else {
        ZccSvgNode* curr = parent->children;
        while (curr->next) curr = curr->next;
        curr->next = child;
    }
}

static inline void svg_set_attr(ZccSvgNode* node, const char* name, const char* value) {
#ifdef _MSC_VER
    char* name_dup = _strdup(name);
    char* value_dup = _strdup(value);
#else
    char* name_dup = strdup(name);
    char* value_dup = strdup(value);
#endif
    SvgAttr* a = (SvgAttr*)calloc(1, sizeof(SvgAttr));
    a->name = name_dup;
    a->value = value_dup;
    a->next = node->attrs;
    node->attrs = a;
}

static inline SvgPathBuilder* svg_path_begin() {
    SvgPathBuilder* pb = (SvgPathBuilder*)calloc(1, sizeof(SvgPathBuilder));
    pb->data[0] = '\0';
    return pb;
}
static inline void svg_path_move_to(SvgPathBuilder* pb, int x, int y) {
    char buf[64];
    sprintf(buf, "M%d %d ", x, y);
    strcat(pb->data, buf);
}
static inline void svg_path_cubic_to(SvgPathBuilder* pb, int x1, int y1, int x2, int y2, int x, int y) {
    char buf[128];
    sprintf(buf, "C%d %d, %d %d, %d %d ", x1, y1, x2, y2, x, y);
    strcat(pb->data, buf);
}
static inline void svg_path_close(SvgPathBuilder* pb) {
    strcat(pb->data, "Z ");
}
static inline void svg_apply_path(ZccSvgNode* node, SvgPathBuilder* pb) {
    svg_set_attr(node, "d", pb->data);
    free(pb);
}

static inline void svg_to_string_impl(ZccSvgNode* node, char* out, int indent) {
    if (!node) return;
    char ind[64] = {0};
    for(int i=0; i<indent && i<63; i++) ind[i] = ' ';
    
    char buf[8192];
    sprintf(buf, "%s<%s", ind, node->tag);
    strcat(out, buf);
    
    SvgAttr* a = node->attrs;
    while(a) {
        sprintf(buf, " %s=\"%s\"", a->name, a->value);
        strcat(out, buf);
        a = a->next;
    }
    
    if (!node->children && !node->content) {
        strcat(out, " />\n");
    } else {
        strcat(out, ">\n");
        if (node->content) {
            sprintf(buf, "%s  %s\n", ind, node->content);
            strcat(out, buf);
        }
        ZccSvgNode* c = node->children;
        while(c) {
            svg_to_string_impl(c, out, indent + 2);
            c = c->next;
        }
        sprintf(buf, "%s</%s>\n", ind, node->tag);
        strcat(out, buf);
    }
}

static inline char* svg_to_string(ZccSvgNode* root) {
    char* out = (char*)calloc(1, 1024 * 1024); // 1MB buffer
    svg_to_string_impl(root, out, 0);
    return out;
}

static inline ZccSvgNode* svg_animate() {
    return svg_create_node("animate");
}

static inline ZccSvgNode* svg_animateTransform() {
    return svg_create_node("animateTransform");
}

static inline ZccSvgNode* svg_circle() {
    return svg_create_node("circle");
}

static inline ZccSvgNode* svg_defs() {
    return svg_create_node("defs");
}

static inline ZccSvgNode* svg_ellipse() {
    return svg_create_node("ellipse");
}

static inline ZccSvgNode* svg_filter() {
    return svg_create_node("filter");
}

static inline ZccSvgNode* svg_g() {
    return svg_create_node("g");
}

static inline ZccSvgNode* svg_line() {
    return svg_create_node("line");
}

static inline ZccSvgNode* svg_linearGradient() {
    return svg_create_node("linearGradient");
}

static inline ZccSvgNode* svg_path() {
    return svg_create_node("path");
}

static inline ZccSvgNode* svg_polygon() {
    return svg_create_node("polygon");
}

static inline ZccSvgNode* svg_polyline() {
    return svg_create_node("polyline");
}

static inline ZccSvgNode* svg_rect() {
    return svg_create_node("rect");
}

static inline ZccSvgNode* svg_stop() {
    return svg_create_node("stop");
}

static inline ZccSvgNode* svg_svg() {
    return svg_create_node("svg");
}

static inline ZccSvgNode* svg_text() {
    return svg_create_node("text");
}

static inline ZccSvgNode* svg_use() {
    return svg_create_node("use");
}

static inline void svg_set_attributeName(ZccSvgNode* n, const char* v) {
    svg_set_attr(n, "attributeName", v);
}

static inline void svg_set_attributeType(ZccSvgNode* n, const char* v) {
    svg_set_attr(n, "attributeType", v);
}

static inline void svg_set_dur(ZccSvgNode* n, const char* v) {
    svg_set_attr(n, "dur", v);
}

static inline void svg_set_fill(ZccSvgNode* n, const char* v) {
    svg_set_attr(n, "fill", v);
}

static inline void svg_set_from(ZccSvgNode* n, const char* v) {
    svg_set_attr(n, "from", v);
}

static inline void svg_set_height(ZccSvgNode* n, const char* v) {
    svg_set_attr(n, "height", v);
}

static inline void svg_set_id(ZccSvgNode* n, const char* v) {
    svg_set_attr(n, "id", v);
}

static inline void svg_set_offset(ZccSvgNode* n, const char* v) {
    svg_set_attr(n, "offset", v);
}

static inline void svg_set_repeatCount(ZccSvgNode* n, const char* v) {
    svg_set_attr(n, "repeatCount", v);
}

static inline void svg_set_stop_color(ZccSvgNode* n, const char* v) {
    svg_set_attr(n, "stop-color", v);
}

static inline void svg_set_stop_opacity(ZccSvgNode* n, const char* v) {
    svg_set_attr(n, "stop-opacity", v);
}

static inline void svg_set_stroke(ZccSvgNode* n, const char* v) {
    svg_set_attr(n, "stroke", v);
}

static inline void svg_set_stroke_width(ZccSvgNode* n, const char* v) {
    svg_set_attr(n, "stroke-width", v);
}

static inline void svg_set_to(ZccSvgNode* n, const char* v) {
    svg_set_attr(n, "to", v);
}

static inline void svg_set_type(ZccSvgNode* n, const char* v) {
    svg_set_attr(n, "type", v);
}

static inline void svg_set_viewBox(ZccSvgNode* n, const char* v) {
    svg_set_attr(n, "viewBox", v);
}

static inline void svg_set_width(ZccSvgNode* n, const char* v) {
    svg_set_attr(n, "width", v);
}

static inline void svg_set_x1(ZccSvgNode* n, const char* v) {
    svg_set_attr(n, "x1", v);
}

static inline void svg_set_x2(ZccSvgNode* n, const char* v) {
    svg_set_attr(n, "x2", v);
}

static inline void svg_set_y1(ZccSvgNode* n, const char* v) {
    svg_set_attr(n, "y1", v);
}

static inline void svg_set_y2(ZccSvgNode* n, const char* v) {
    svg_set_attr(n, "y2", v);
}

#endif
