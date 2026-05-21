import os

tags = ['animate', 'animateTransform', 'circle', 'defs', 'desc', 'ellipse', 'line', 'linearGradient', 'path', 'polygon', 'polyline', 'rect', 'stop', 'style', 'svg', 'symbol', 'text', 'textPath', 'title', 'use', 'g']
attrs = ['aria-labelledby', 'attributeName', 'class', 'cx', 'cy', 'd', 'dur', 'fill', 'font-family', 'font-size', 'from', 'height', 'href', 'id', 'letter-spacing', 'offset', 'points', 'r', 'repeatCount', 'rx', 'ry', 'startOffset', 'stop-color', 'stroke', 'stroke-width', 'style', 'to', 'transform', 'type', 'values', 'viewBox', 'width', 'x', 'x1', 'x2', 'xmlns', 'y', 'y1', 'y2']

with open(r'G:\zccMAIN\zcc\zcc_svg.h', 'w') as f:
    f.write('/* Auto-generated ZCC SVG Master Library from H:\\agents\\_svgzcc\\ds */\n')
    f.write('#ifndef ZCC_SVG_H\n#define ZCC_SVG_H\n\n')
    f.write('#include <stdio.h>\n')
    f.write('#include <stdlib.h>\n')
    f.write('#include <string.h>\n\n')
    
    f.write('typedef struct ZccSvgNode {\n')
    f.write('    const char* tag;\n')
    f.write('    char* attributes;\n')
    f.write('    char* content;\n')
    f.write('    struct ZccSvgNode* next;\n')
    f.write('    struct ZccSvgNode* children;\n')
    f.write('} ZccSvgNode;\n\n')

    f.write('ZccSvgNode* svg_create_node(const char* tag);\n')
    f.write('void svg_add_child(ZccSvgNode* parent, ZccSvgNode* child);\n')
    f.write('void svg_set_attr(ZccSvgNode* node, const char* attr_name, const char* attr_val);\n')
    f.write('void svg_set_content(ZccSvgNode* node, const char* text);\n')
    f.write('char* svg_to_string(ZccSvgNode* root);\n\n')

    f.write('/* Node Generators */\n')
    for t in tags:
        fn_name = t.replace('-', '_')
        f.write(f'ZccSvgNode* svg_{fn_name}(void);\n')

    f.write('\n/* Attribute Setters */\n')
    for a in attrs:
        fn_name = a.replace('-', '_')
        f.write(f'void svg_set_{fn_name}(ZccSvgNode* node, const char* val);\n')

    f.write('\n#endif\n')

with open(r'G:\zccMAIN\zcc\zcc_svg.c', 'w') as f:
    f.write('#include "zcc_svg.h"\n\n')
    
    f.write('ZccSvgNode* svg_create_node(const char* tag) {\n')
    f.write('    ZccSvgNode* n = (ZccSvgNode*)calloc(1, sizeof(ZccSvgNode));\n')
    f.write('    n->tag = tag;\n')
    f.write('    return n;\n')
    f.write('}\n\n')

    f.write('void svg_add_child(ZccSvgNode* parent, ZccSvgNode* child) {\n')
    f.write('    if (!parent->children) {\n')
    f.write('        parent->children = child;\n')
    f.write('    } else {\n')
    f.write('        ZccSvgNode* cur = parent->children;\n')
    f.write('        while(cur->next) cur = cur->next;\n')
    f.write('        cur->next = child;\n')
    f.write('    }\n')
    f.write('}\n\n')

    f.write('void svg_set_attr(ZccSvgNode* node, const char* attr_name, const char* attr_val) {\n')
    f.write('    int name_len = strlen(attr_name);\n')
    f.write('    int val_len = strlen(attr_val);\n')
    f.write('    int new_len = name_len + val_len + 5;\n')
    f.write('    if (!node->attributes) {\n')
    f.write('        node->attributes = (char*)calloc(1, new_len);\n')
    f.write('        sprintf(node->attributes, " %s=\\"%s\\"", attr_name, attr_val);\n')
    f.write('    } else {\n')
    f.write('        int old_len = strlen(node->attributes);\n')
    f.write('        char* new_attrs = (char*)calloc(1, old_len + new_len);\n')
    f.write('        strcpy(new_attrs, node->attributes);\n')
    f.write('        sprintf(new_attrs + old_len, " %s=\\"%s\\"", attr_name, attr_val);\n')
    f.write('        free(node->attributes);\n')
    f.write('        node->attributes = new_attrs;\n')
    f.write('    }\n')
    f.write('}\n\n')

    f.write('void svg_set_content(ZccSvgNode* node, const char* text) {\n')
    f.write('    if (node->content) free(node->content);\n')
    f.write('    node->content = strdup(text);\n')
    f.write('}\n\n')

    f.write('static void _svg_to_string_rec(ZccSvgNode* node, char** out, int* cap, int* len) {\n')
    f.write('    if (!node) return;\n')
    f.write('    char buf[2048];\n')
    f.write('    sprintf(buf, "<%s%s>", node->tag, node->attributes ? node->attributes : "");\n')
    f.write('    int slen = strlen(buf);\n')
    f.write('    if (*len + slen + 1024 > *cap) {\n')
    f.write('        *cap *= 2;\n')
    f.write('        *out = (char*)realloc(*out, *cap);\n')
    f.write('    }\n')
    f.write('    strcpy(*out + *len, buf);\n')
    f.write('    *len += slen;\n')
    
    f.write('    if (node->content) {\n')
    f.write('        slen = strlen(node->content);\n')
    f.write('        if (*len + slen + 1024 > *cap) {\n')
    f.write('            *cap *= 2;\n')
    f.write('            *out = (char*)realloc(*out, *cap);\n')
    f.write('        }\n')
    f.write('        strcpy(*out + *len, node->content);\n')
    f.write('        *len += slen;\n')
    f.write('    }\n')

    f.write('    if (node->children) {\n')
    f.write('        ZccSvgNode* c = node->children;\n')
    f.write('        while(c) {\n')
    f.write('            _svg_to_string_rec(c, out, cap, len);\n')
    f.write('            c = c->next;\n')
    f.write('        }\n')
    f.write('    }\n')

    f.write('    sprintf(buf, "</%s>\\n", node->tag);\n')
    f.write('    slen = strlen(buf);\n')
    f.write('    strcpy(*out + *len, buf);\n')
    f.write('    *len += slen;\n')
    f.write('}\n\n')

    f.write('char* svg_to_string(ZccSvgNode* root) {\n')
    f.write('    int cap = 4096;\n')
    f.write('    int len = 0;\n')
    f.write('    char* out = (char*)calloc(1, cap);\n')
    f.write('    _svg_to_string_rec(root, &out, &cap, &len);\n')
    f.write('    return out;\n')
    f.write('}\n\n')

    for t in tags:
        fn_name = t.replace('-', '_')
        f.write(f'ZccSvgNode* svg_{fn_name}(void) {{ return svg_create_node("{t}"); }}\n')

    for a in attrs:
        fn_name = a.replace('-', '_')
        f.write(f'void svg_set_{fn_name}(ZccSvgNode* node, const char* val) {{ svg_set_attr(node, "{a}", val); }}\n')
