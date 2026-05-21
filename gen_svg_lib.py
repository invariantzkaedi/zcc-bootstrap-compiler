import os, re

ds_dir = '/mnt/h/agents/_svgzcc/ds'
tags = set()
attrs = set()

if os.path.exists(ds_dir):
    for f in os.listdir(ds_dir):
        if f.endswith('.md'):
            with open(os.path.join(ds_dir, f), 'r', encoding='utf-8', errors='ignore') as fp:
                text = fp.read()
                for m in re.finditer(r'<([a-zA-Z0-9_-]+)[ >]', text):
                    tags.add(m.group(1))
                for m in re.finditer(r' ([a-zA-Z0-9_-]+)="', text):
                    attrs.add(m.group(1))

# Default essential SVG attrs just in case they aren't parsed
attrs.update(["width", "height", "viewBox", "id", "x1", "y1", "x2", "y2", "offset", "stop-color", "stop-opacity", "fill", "stroke", "stroke-width", "attributeName", "attributeType", "type", "from", "to", "dur", "repeatCount", "opacity"])
tags.update(["svg", "g", "path", "circle", "rect", "line", "polygon", "polyline", "ellipse", "text", "animate", "animateTransform", "filter", "defs", "linearGradient", "stop", "use"])

with open('zcc_svg.h', 'w') as f:
    f.write('''/* Auto-generated ZCC SVG Master Library */
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
    pb->data[0] = '\\0';
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
    
    char buf[128];
    sprintf(buf, "%s<%s", ind, node->tag);
    strcat(out, buf);
    
    SvgAttr* a = node->attrs;
    while(a) {
        strcat(out, " ");
        strcat(out, a->name);
        strcat(out, "=\\"");
        strcat(out, a->value);
        strcat(out, "\\\"");
        a = a->next;
    }
    
    if (!node->children && !node->content) {
        strcat(out, " />\\n");
    } else {
        strcat(out, ">\\n");
        if (node->content) {
            strcat(out, ind);
            strcat(out, "  ");
            strcat(out, node->content);
            strcat(out, "\\n");
        }
        ZccSvgNode* c = node->children;
        while(c) {
            svg_to_string_impl(c, out, indent + 2);
            c = c->next;
        }
        sprintf(buf, "%s</%s>\\n", ind, node->tag);
        strcat(out, buf);
    }
}

static inline char* svg_to_string(ZccSvgNode* root) {
    char* out = (char*)calloc(1, 1024 * 1024); // 1MB buffer
    svg_to_string_impl(root, out, 0);
    return out;
}

''')

    # Emit tag functions
    for t in sorted(list(tags)):
        t_clean = t.replace("-", "_")
        f.write(f'static inline ZccSvgNode* svg_{t_clean}() {{\n')
        f.write(f'    return svg_create_node("{t}");\n')
        f.write(f'}}\n\n')

    # Emit attr setter functions
    for a in sorted(list(attrs)):
        a_clean = a.replace("-", "_").replace(":", "_")
        f.write(f'static inline void svg_set_{a_clean}(ZccSvgNode* n, const char* v) {{\n')
        f.write(f'    svg_set_attr(n, "{a}", v);\n')
        f.write(f'}}\n\n')

    f.write('#endif\n')
