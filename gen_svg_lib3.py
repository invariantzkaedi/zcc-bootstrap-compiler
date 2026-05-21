import os, re

ds_dir = r'H:\agents\_svgzcc\ds'
tags = set()
attrs = set()
path_cmds = set()

# Deep regex parsers to mine SVG attributes, tags, and path commands.
# We will scan all .md files.
if os.path.exists(ds_dir):
    for f in os.listdir(ds_dir):
        if f.endswith('.md'):
            with open(os.path.join(ds_dir, f), 'r', encoding='utf-8', errors='ignore') as fp:
                text = fp.read()
                # Mine tags inside <...>
                for m in re.finditer(r'<([a-zA-Z0-9_-]+)(?:>| )', text):
                    t = m.group(1).lower()
                    if t not in ['br', 'div', 'span', 'html', 'head', 'body', 'a', 'img']:
                        tags.add(t)
                
                # Mine attributes inside code blocks or standard assignments
                for m in re.finditer(r' ([a-zA-Z0-9_-]+)="', text):
                    attrs.add(m.group(1))
                for m in re.finditer(r'`([a-zA-Z0-9_-]+)` attribute', text):
                    attrs.add(m.group(1))

                # Mine path commands (M, L, C, Z, etc.) from `d="..."` patterns
                for m in re.finditer(r'd="([^"]+)"', text):
                    path_str = m.group(1)
                    cmds = re.findall(r'[a-zA-Z]', path_str)
                    path_cmds.update(cmds)

# Known SVG 2.0 / SMIL filter elements to ensure full coverage
standard_tags = ['svg', 'g', 'defs', 'desc', 'title', 'symbol', 'use', 'image', 'switch', 'script', 'style', 
                 'path', 'rect', 'circle', 'ellipse', 'line', 'polyline', 'polygon', 
                 'text', 'tspan', 'textPath', 'linearGradient', 'radialGradient', 'stop', 
                 'pattern', 'clipPath', 'mask', 'filter', 'feBlend', 'feColorMatrix', 
                 'feComponentTransfer', 'feComposite', 'feConvolveMatrix', 'feDiffuseLighting', 
                 'feDisplacementMap', 'feFlood', 'feGaussianBlur', 'feImage', 'feMerge', 
                 'feMergeNode', 'feMorphology', 'feOffset', 'feSpecularLighting', 'feTile', 
                 'feTurbulence', 'animate', 'animateMotion', 'animateTransform', 'set', 'mpath']

standard_attrs = ['id', 'class', 'style', 'x', 'y', 'width', 'height', 'cx', 'cy', 'r', 'rx', 'ry', 
                  'x1', 'y1', 'x2', 'y2', 'd', 'points', 'transform', 'fill', 'fill-rule', 'fill-opacity', 
                  'stroke', 'stroke-width', 'stroke-linecap', 'stroke-linejoin', 'stroke-miterlimit', 
                  'stroke-dasharray', 'stroke-dashoffset', 'stroke-opacity', 'opacity', 'color', 
                  'font-family', 'font-size', 'font-weight', 'font-style', 'text-anchor', 'dominant-baseline', 
                  'viewBox', 'preserveAspectRatio', 'xmlns', 'href', 'xlink:href', 'clip-path', 'mask', 
                  'filter', 'gradientUnits', 'gradientTransform', 'patternUnits', 'patternTransform', 
                  'offset', 'stop-color', 'stop-opacity', 'attributeName', 'attributeType', 'from', 'to', 'by', 
                  'begin', 'dur', 'end', 'min', 'max', 'restart', 'repeatCount', 'repeatDur', 
                  'calcMode', 'values', 'keyTimes', 'keySplines', 'keyPoints', 'rotate', 'path', 'type', 
                  'additive', 'accumulate', 'target', 'systemLanguage', 'onbegin', 'onend', 'onrepeat']

tags.update(standard_tags)
attrs.update(standard_attrs)

# Clean up tags/attrs
tags = sorted(list([t for t in tags if len(t) > 1 and not t.isdigit()]))
attrs = sorted(list([a for a in attrs if len(a) > 1 and not a.isdigit()]))

with open(r'G:\zccMAIN\zcc\zcc_svg.h', 'w') as f:
    f.write('/* Auto-generated ZCC SVG Deep Knowledge Library */\n')
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

    f.write('/* Path Builder Utility */\n')
    f.write('typedef struct SvgPathBuilder {\n')
    f.write('    char* d;\n')
    f.write('    int cap;\n')
    f.write('    int len;\n')
    f.write('} SvgPathBuilder;\n')
    f.write('SvgPathBuilder* svg_path_begin();\n')
    f.write('void svg_path_move_to(SvgPathBuilder* pb, float x, float y);\n')
    f.write('void svg_path_line_to(SvgPathBuilder* pb, float x, float y);\n')
    f.write('void svg_path_cubic_to(SvgPathBuilder* pb, float x1, float y1, float x2, float y2, float x, float y);\n')
    f.write('void svg_path_close(SvgPathBuilder* pb);\n')
    f.write('void svg_apply_path(ZccSvgNode* node, SvgPathBuilder* pb);\n\n')

    f.write('/* Node Generators */\n')
    for t in tags:
        fn_name = t.replace('-', '_').replace(':', '_')
        f.write(f'ZccSvgNode* svg_{fn_name}(void);\n')

    f.write('\n/* Attribute Setters */\n')
    for a in attrs:
        fn_name = a.replace('-', '_').replace(':', '_')
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
    f.write('    char buf[4096];\n')
    f.write('    sprintf(buf, "<%s%s>", node->tag, node->attributes ? node->attributes : "");\n')
    f.write('    int slen = strlen(buf);\n')
    f.write('    if (*len + slen + 2048 > *cap) {\n')
    f.write('        *cap *= 2;\n')
    f.write('        *out = (char*)realloc(*out, *cap);\n')
    f.write('    }\n')
    f.write('    strcpy(*out + *len, buf);\n')
    f.write('    *len += slen;\n')
    
    f.write('    if (node->content) {\n')
    f.write('        slen = strlen(node->content);\n')
    f.write('        if (*len + slen + 2048 > *cap) {\n')
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
    f.write('    int cap = 8192;\n')
    f.write('    int len = 0;\n')
    f.write('    char* out = (char*)calloc(1, cap);\n')
    f.write('    _svg_to_string_rec(root, &out, &cap, &len);\n')
    f.write('    return out;\n')
    f.write('}\n\n')
    
    f.write('/* Path Builder Implementation */\n')
    f.write('SvgPathBuilder* svg_path_begin() {\n')
    f.write('    SvgPathBuilder* pb = (SvgPathBuilder*)calloc(1, sizeof(SvgPathBuilder));\n')
    f.write('    pb->cap = 256;\n')
    f.write('    pb->d = (char*)calloc(1, pb->cap);\n')
    f.write('    return pb;\n')
    f.write('}\n')
    f.write('static void _pb_append(SvgPathBuilder* pb, const char* str) {\n')
    f.write('    int slen = strlen(str);\n')
    f.write('    if (pb->len + slen + 64 > pb->cap) {\n')
    f.write('        pb->cap *= 2;\n')
    f.write('        pb->d = (char*)realloc(pb->d, pb->cap);\n')
    f.write('    }\n')
    f.write('    strcpy(pb->d + pb->len, str);\n')
    f.write('    pb->len += slen;\n')
    f.write('}\n')
    f.write('void svg_path_move_to(SvgPathBuilder* pb, float x, float y) {\n')
    f.write('    char buf[64]; sprintf(buf, "M%.2f,%.2f ", x, y); _pb_append(pb, buf);\n')
    f.write('}\n')
    f.write('void svg_path_line_to(SvgPathBuilder* pb, float x, float y) {\n')
    f.write('    char buf[64]; sprintf(buf, "L%.2f,%.2f ", x, y); _pb_append(pb, buf);\n')
    f.write('}\n')
    f.write('void svg_path_cubic_to(SvgPathBuilder* pb, float x1, float y1, float x2, float y2, float x, float y) {\n')
    f.write('    char buf[128]; sprintf(buf, "C%.2f,%.2f %.2f,%.2f %.2f,%.2f ", x1, y1, x2, y2, x, y); _pb_append(pb, buf);\n')
    f.write('}\n')
    f.write('void svg_path_close(SvgPathBuilder* pb) {\n')
    f.write('    _pb_append(pb, "Z ");\n')
    f.write('}\n')
    f.write('void svg_apply_path(ZccSvgNode* node, SvgPathBuilder* pb) {\n')
    f.write('    svg_set_attr(node, "d", pb->d);\n')
    f.write('    free(pb->d);\n')
    f.write('    free(pb);\n')
    f.write('}\n\n')

    for t in tags:
        fn_name = t.replace('-', '_').replace(':', '_')
        f.write(f'ZccSvgNode* svg_{fn_name}(void) {{ return svg_create_node("{t}"); }}\n')

    for a in attrs:
        fn_name = a.replace('-', '_').replace(':', '_')
        f.write(f'void svg_set_{fn_name}(ZccSvgNode* node, const char* val) {{ svg_set_attr(node, "{a}", val); }}\n')
