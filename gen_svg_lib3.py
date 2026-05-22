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
tags = sorted(list([t for t in tags if len(t) >= 1 and not t.isdigit()]))
attrs = sorted(list([a for a in attrs if len(a) >= 1 and not a.isdigit()]))

with open('zcc_svg.h', 'w') as f:
    f.write('/* Auto-generated ZCC SVG Deep Knowledge Library */\n')
    f.write('#ifndef ZCC_SVG_H\n#define ZCC_SVG_H\n\n')
    f.write('#include <stdio.h>\n')
    f.write('#include <stdlib.h>\n')
    f.write('#include <string.h>\n')
    f.write('#include <stdint.h>\n\n')
    
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
    f.write('/* Base64 & ASCII Utility Functions */\n')
    f.write('char* base64_encode(const unsigned char* data, size_t input_length);\n')
    f.write('unsigned char* base64_decode(const char* data, size_t input_length, size_t* output_length);\n')
    f.write('char* svg_to_base64(ZccSvgNode* root);\n')
    f.write('char* svg_to_data_uri(ZccSvgNode* root);\n')
    f.write('char* svg_to_html_uri(ZccSvgNode* root);\n')
    f.write('char* hexdump_to_ascii(const unsigned char* data, size_t len);\n\n')
 
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
    f.write('/* ZCC AST Visualizer Utilities */\n')
    f.write('struct ZCCNode;\n')
    f.write('char* svg_render_ast(struct ZCCNode* root);\n')
    f.write('char* svg_render_ast_html_uri(struct ZCCNode* root);\n')
    f.write('char* zcc_ast_to_ascii(struct ZCCNode* root);\n\n')

    f.write('/* Node Generators */\n')
    for t in tags:
        fn_name = t.replace('-', '_').replace(':', '_')
        f.write(f'ZccSvgNode* svg_{fn_name}(void);\n')

    f.write('\n/* Attribute Setters */\n')
    for a in attrs:
        fn_name = a.replace('-', '_').replace(':', '_')
        f.write(f'void svg_set_{fn_name}(ZccSvgNode* node, const char* val);\n')

    f.write('\n#endif\n')

with open('zcc_svg.c', 'w') as f:
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
    f.write('    int tag_len = strlen(node->tag);\n')
    f.write('    int attrs_len = node->attributes ? strlen(node->attributes) : 0;\n')
    f.write('    int total_needed = tag_len + attrs_len + 3;\n')
    f.write('    while (*len + total_needed + 2048 > *cap) {\n')
    f.write('        *cap *= 2;\n')
    f.write('        *out = (char*)realloc(*out, *cap);\n')
    f.write('    }\n')
    f.write('    *len += sprintf(*out + *len, "<%s%s>", node->tag, node->attributes ? node->attributes : "");\n')
    
    f.write('    if (node->content) {\n')
    f.write('        int slen = strlen(node->content);\n')
    f.write('        while (*len + slen + 2048 > *cap) {\n')
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

    f.write('    int close_len = tag_len + 5;\n')
    f.write('    while (*len + close_len + 2048 > *cap) {\n')
    f.write('        *cap *= 2;\n')
    f.write('        *out = (char*)realloc(*out, *cap);\n')
    f.write('    }\n')
    f.write('    *len += sprintf(*out + *len, "</%s>\\n", node->tag);\n')
    f.write('}\n\n')

    f.write('char* svg_to_string(ZccSvgNode* root) {\n')
    f.write('    int cap = 8192;\n')
    f.write('    int len = 0;\n')
    f.write('    char* out = (char*)calloc(1, cap);\n')
    f.write('    _svg_to_string_rec(root, &out, &cap, &len);\n')
    f.write('    return out;\n')
    f.write('}\n\n')
    f.write('/* Base64 & ASCII Utility Implementations */\n')
    f.write('static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";\n\n')
    f.write('static int get_b64_char_value(char c) {\n')
    f.write('    if (c >= \'A\' && c <= \'Z\') return c - \'A\';\n')
    f.write('    if (c >= \'a\' && c <= \'z\') return c - \'a\' + 26;\n')
    f.write('    if (c >= \'0\' && c <= \'9\') return c - \'0\' + 52;\n')
    f.write('    if (c == \'+\') return 62;\n')
    f.write('    if (c == \'/\') return 63;\n')
    f.write('    return -1;\n')
    f.write('}\n\n')
    f.write('char* base64_encode(const unsigned char* data, size_t input_length) {\n')
    f.write('    size_t output_length = 4 * ((input_length + 2) / 3);\n')
    f.write('    char* encoded_data = (char*)malloc(output_length + 1);\n')
    f.write('    if (!encoded_data) return NULL;\n\n')
    f.write('    size_t i, j;\n')
    f.write('    for (i = 0, j = 0; i < input_length;) {\n')
    f.write('        uint32_t octet_a = i < input_length ? data[i++] : 0;\n')
    f.write('        uint32_t octet_b = i < input_length ? data[i++] : 0;\n')
    f.write('        uint32_t octet_c = i < input_length ? data[i++] : 0;\n\n')
    f.write('        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;\n\n')
    f.write('        encoded_data[j++] = b64_table[(triple >> 3 * 6) & 0x3F];\n')
    f.write('        encoded_data[j++] = b64_table[(triple >> 2 * 6) & 0x3F];\n')
    f.write('        encoded_data[j++] = b64_table[(triple >> 1 * 6) & 0x3F];\n')
    f.write('        encoded_data[j++] = b64_table[(triple >> 0 * 6) & 0x3F];\n')
    f.write('    }\n\n')
    f.write('    size_t mod = input_length % 3;\n')
    f.write('    if (mod == 1) {\n')
    f.write('        encoded_data[output_length - 2] = \'=\';\n')
    f.write('        encoded_data[output_length - 1] = \'=\';\n')
    f.write('    } else if (mod == 2) {\n')
    f.write('        encoded_data[output_length - 1] = \'=\';\n')
    f.write('    }\n\n')
    f.write('    encoded_data[output_length] = \'\\0\';\n')
    f.write('    return encoded_data;\n')
    f.write('}\n\n')
    f.write('unsigned char* base64_decode(const char* data, size_t input_length, size_t* output_length) {\n')
    f.write('    size_t clean_len = 0;\n')
    f.write('    for (size_t i = 0; i < input_length; i++) {\n')
    f.write('        char c = data[i];\n')
    f.write('        if (c != \' \' && c != \'\\t\' && c != \'\\r\' && c != \'\\n\') {\n')
    f.write('            clean_len++;\n')
    f.write('        }\n')
    f.write('    }\n\n')
    f.write('    if (clean_len % 4 != 0) return NULL;\n\n')
    f.write('    size_t padding = 0;\n')
    f.write('    int non_ws_count = 0;\n')
    f.write('    char last_chars[4] = {0};\n')
    f.write('    for (size_t i = input_length; i > 0; i--) {\n')
    f.write('        char c = data[i - 1];\n')
    f.write('        if (c != \' \' && c != \'\\t\' && c != \'\\r\' && c != \'\\n\') {\n')
    f.write('            if (non_ws_count < 4) {\n')
    f.write('                last_chars[3 - non_ws_count] = c;\n')
    f.write('                non_ws_count++;\n')
    f.write('            }\n')
    f.write('        }\n')
    f.write('    }\n')
    f.write('    if (non_ws_count >= 1 && last_chars[3] == \'=\') {\n')
    f.write('        padding++;\n')
    f.write('        if (non_ws_count >= 2 && last_chars[2] == \'=\') padding++;\n')
    f.write('    }\n\n')
    f.write('    size_t out_len = (clean_len / 4) * 3 - padding;\n')
    f.write('    unsigned char* decoded_data = (unsigned char*)malloc(out_len + 1);\n')
    f.write('    if (!decoded_data) return NULL;\n\n')
    f.write('    size_t i = 0, j = 0;\n')
    f.write('    while (i < input_length) {\n')
    f.write('        char chars[4] = {0};\n')
    f.write('        int filled = 0;\n')
    f.write('        while (filled < 4 && i < input_length) {\n')
    f.write('            char c = data[i++];\n')
    f.write('            if (c != \' \' && c != \'\\t\' && c != \'\\r\' && c != \'\\n\') {\n')
    f.write('                chars[filled++] = c;\n')
    f.write('            }\n')
    f.write('        }\n')
    f.write('        if (filled == 0) break;\n')
    f.write('        if (filled < 4) {\n')
    f.write('            free(decoded_data);\n')
    f.write('            return NULL;\n')
    f.write('        }\n\n')
    f.write('        int val0 = get_b64_char_value(chars[0]);\n')
    f.write('        int val1 = get_b64_char_value(chars[1]);\n')
    f.write('        int val2 = chars[2] != \'=\' ? get_b64_char_value(chars[2]) : -1;\n')
    f.write('        int val3 = chars[3] != \'=\' ? get_b64_char_value(chars[3]) : -1;\n\n')
    f.write('        if (val0 < 0 || val1 < 0) {\n')
    f.write('            free(decoded_data);\n')
    f.write('            return NULL;\n')
    f.write('        }\n\n')
    f.write('        uint32_t triple = (val0 << 18) + (val1 << 12);\n')
    f.write('        if (val2 >= 0) triple += (val2 << 6);\n')
    f.write('        if (val3 >= 0) triple += val3;\n\n')
    f.write('        if (j < out_len) decoded_data[j++] = (triple >> 16) & 0xFF;\n')
    f.write('        if (j < out_len) decoded_data[j++] = (triple >> 8) & 0xFF;\n')
    f.write('        if (j < out_len) decoded_data[j++] = triple & 0xFF;\n')
    f.write('    }\n')
    f.write('    decoded_data[out_len] = \'\\0\';\n\n')
    f.write('    if (output_length) *output_length = out_len;\n')
    f.write('    return decoded_data;\n')
    f.write('}\n\n')
    f.write('char* svg_to_base64(ZccSvgNode* root) {\n')
    f.write('    char* svg_str = svg_to_string(root);\n')
    f.write('    if (!svg_str) return NULL;\n')
    f.write('    char* b64 = base64_encode((const unsigned char*)svg_str, strlen(svg_str));\n')
    f.write('    free(svg_str);\n')
    f.write('    return b64;\n')
    f.write('}\n\n')
    f.write('char* svg_to_data_uri(ZccSvgNode* root) {\n')
    f.write('    char* b64 = svg_to_base64(root);\n')
    f.write('    if (!b64) return NULL;\n')
    f.write('    size_t b64_len = strlen(b64);\n')
    f.write('    const char* prefix = "data:image/svg+xml;base64,";\n')
    f.write('    size_t prefix_len = strlen(prefix);\n')
    f.write('    char* uri = (char*)malloc(prefix_len + b64_len + 1);\n')
    f.write('    if (!uri) {\n')
    f.write('        free(b64);\n')
    f.write('        return NULL;\n')
    f.write('    }\n')
    f.write('    strcpy(uri, prefix);\n')
    f.write('    strcpy(uri + prefix_len, b64);\n')
    f.write('    free(b64);\n')
    f.write('    return uri;\n')
    f.write('}\n\n')
    f.write('char* svg_to_html_uri(ZccSvgNode* root) {\n')
    f.write('    char* svg_str = svg_to_string(root);\n')
    f.write('    if (!svg_str) return NULL;\n\n')
    f.write('    size_t len = strlen(svg_str);\n')
    f.write('    char* svg_single = (char*)malloc(len + 1);\n')
    f.write('    if (!svg_single) {\n')
    f.write('        free(svg_str);\n')
    f.write('        return NULL;\n')
    f.write('    }\n')
    f.write('    for (size_t i = 0; i <= len; i++) {\n')
    f.write('        if (svg_str[i] == \'"\') {\n')
    f.write('            svg_single[i] = \'\\\'\';\n')
    f.write('        } else {\n')
    f.write('            svg_single[i] = svg_str[i];\n')
    f.write('        }\n')
    f.write('    }\n')
    f.write('    free(svg_str);\n\n')
    f.write('    const char* html_pre = "<!DOCTYPE html><html><body style=\'margin:0;background:#03010a;overflow:hidden;\'>";\n')
    f.write('    const char* html_post = "</body></html>";\n')
    f.write('    size_t pre_len = strlen(html_pre);\n')
    f.write('    size_t post_len = strlen(html_post);\n\n')
    f.write('    size_t html_len = pre_len + len + post_len;\n')
    f.write('    char* html_content = (char*)malloc(html_len + 1);\n')
    f.write('    if (!html_content) {\n')
    f.write('        free(svg_single);\n')
    f.write('        return NULL;\n')
    f.write('    }\n')
    f.write('    strcpy(html_content, html_pre);\n')
    f.write('    strcpy(html_content + pre_len, svg_single);\n')
    f.write('    strcpy(html_content + pre_len + len, html_post);\n')
    f.write('    free(svg_single);\n\n')
    f.write('    char* b64 = base64_encode((const unsigned char*)html_content, html_len);\n')
    f.write('    free(html_content);\n')
    f.write('    if (!b64) return NULL;\n\n')
    f.write('    const char* prefix = "data:text/html;base64,";\n')
    f.write('    size_t prefix_len = strlen(prefix);\n')
    f.write('    size_t b64_len = strlen(b64);\n')
    f.write('    char* uri = (char*)malloc(prefix_len + b64_len + 1);\n')
    f.write('    if (!uri) {\n')
    f.write('        free(b64);\n')
    f.write('        return NULL;\n')
    f.write('    }\n')
    f.write('    strcpy(uri, prefix);\n')
    f.write('    strcpy(uri + prefix_len, b64);\n')
    f.write('    free(b64);\n')
    f.write('    return uri;\n')
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

    # Emit the ZCC AST visualizer implementation
    f.write(r'''
#include "zcc_ast_bridge.h"

static const char* zcc_kind_to_str(int kind) {
    switch (kind) {
        case ZND_NUM: return "ZND_NUM";
        case ZND_STR: return "ZND_STR";
        case ZND_VAR: return "ZND_VAR";
        case ZND_ASSIGN: return "ZND_ASSIGN";
        case ZND_ADD: return "ZND_ADD";
        case ZND_SUB: return "ZND_SUB";
        case ZND_MOD: return "ZND_MOD";
        case ZND_MUL: return "ZND_MUL";
        case ZND_BAND: return "ZND_BAND";
        case ZND_SHL: return "ZND_SHL";
        case ZND_SHR: return "ZND_SHR";
        case ZND_LT: return "ZND_LT";
        case ZND_LE: return "ZND_LE";
        case ZND_GT: return "ZND_GT";
        case ZND_GE: return "ZND_GE";
        case ZND_EQ: return "ZND_EQ";
        case ZND_NE: return "ZND_NE";
        case ZND_IF: return "ZND_IF";
        case ZND_WHILE: return "ZND_WHILE";
        case ZND_FOR: return "ZND_FOR";
        case ZND_BREAK: return "ZND_BREAK";
        case ZND_CONTINUE: return "ZND_CONTINUE";
        case ZND_RETURN: return "ZND_RETURN";
        case ZND_BLOCK: return "ZND_BLOCK";
        case ZND_CAST: return "ZND_CAST";
        case ZND_CALL: return "ZND_CALL";
        case ZND_NOP: return "ZND_NOP";
        case ZND_POST_INC: return "ZND_POST_INC";
        case ZND_COMPOUND_ASSIGN: return "ZND_COMPOUND_ASSIGN";
        case ZND_ADDR: return "ZND_ADDR";
        case ZND_DEREF: return "ZND_DEREF";
        case ZND_MEMBER: return "ZND_MEMBER";
        case ZND_SWITCH: return "ZND_SWITCH";
        case ZND_DIV: return "ZND_DIV";
        case ZND_NEG: return "ZND_NEG";
        case ZND_LAND: return "ZND_LAND";
        case ZND_LOR: return "ZND_LOR";
        case ZND_LNOT: return "ZND_LNOT";
        case ZND_BOR: return "ZND_BOR";
        case ZND_BXOR: return "ZND_BXOR";
        case ZND_BNOT: return "ZND_BNOT";
        case ZND_TERNARY: return "ZND_TERNARY";
        case ZND_SIZEOF: return "ZND_SIZEOF";
        case ZND_POST_DEC: return "ZND_POST_DEC";
        case ZND_PRE_INC: return "ZND_PRE_INC";
        case ZND_PRE_DEC: return "ZND_PRE_DEC";
        case ZND_ASM: return "ZND_ASM";
        case ZND_VLA_ALLOC: return "ZND_VLA_ALLOC";
        case ZND_GOTO: return "ZND_GOTO";
        case ZND_LABEL: return "ZND_LABEL";
        default: return "ZND_UNKNOWN";
    }
}

typedef struct LayoutNode {
    ZCCNode* znode;
    float x, y;
    float subtree_width;
    struct LayoutNode* children[64];
    int child_count;
} LayoutNode;

static LayoutNode* build_layout_tree(ZCCNode* z, int depth) {
    if (!z) return NULL;

    LayoutNode* l = (LayoutNode*)calloc(1, sizeof(LayoutNode));
    if (!l) return NULL;

    l->znode = z;
    l->y = depth * 110.0f;

    #define ADD_CHILD(c) if (c) { \
        LayoutNode* cl = build_layout_tree(c, depth + 1); \
        if (cl && l->child_count < 64) l->children[l->child_count++] = cl; \
    }

    ADD_CHILD(z->lhs);
    ADD_CHILD(z->rhs);
    ADD_CHILD(z->cond);
    ADD_CHILD(z->then_body);
    ADD_CHILD(z->else_body);
    ADD_CHILD(z->body);
    ADD_CHILD(z->init);
    ADD_CHILD(z->inc);

    if (z->stmts && z->num_stmts > 0) {
        for (unsigned int i = 0; i < z->num_stmts; i++) {
            ADD_CHILD(z->stmts[i]);
        }
    }

    if (z->args && z->num_args > 0) {
        for (int i = 0; i < z->num_args; i++) {
            ADD_CHILD(z->args[i]);
        }
    }

    #undef ADD_CHILD

    if (l->child_count == 0) {
        l->subtree_width = 160.0f;
    } else {
        float sum = 0.0f;
        for (int i = 0; i < l->child_count; i++) {
            sum += l->children[i]->subtree_width;
        }
        sum += (l->child_count - 1) * 30.0f;
        l->subtree_width = sum > 160.0f ? sum : 160.0f;
    }

    return l;
}

static void assign_x_coordinates(LayoutNode* l, float left_boundary) {
    if (!l) return;

    if (l->child_count == 0) {
        l->x = left_boundary + l->subtree_width * 0.5f;
    } else {
        float current_left = left_boundary;
        for (int i = 0; i < l->child_count; i++) {
            assign_x_coordinates(l->children[i], current_left);
            current_left += l->children[i]->subtree_width + 30.0f;
        }
        l->x = (l->children[0]->x + l->children[l->child_count - 1]->x) * 0.5f;
    }
}

static void get_bounding_box(LayoutNode* l, float* min_x, float* max_x, float* min_y, float* max_y) {
    if (!l) return;
    if (l->x - 100.0f < *min_x) *min_x = l->x - 100.0f;
    if (l->x + 100.0f > *max_x) *max_x = l->x + 100.0f;
    if (l->y - 50.0f < *min_y) *min_y = l->y - 50.0f;
    if (l->y + 100.0f > *max_y) *max_y = l->y + 100.0f;

    for (int i = 0; i < l->child_count; i++) {
        get_bounding_box(l->children[i], min_x, max_x, min_y, max_y);
    }
}

static void render_layout_to_nodes(LayoutNode* l, ZccSvgNode* parent_svg) {
    if (!l) return;

    for (int i = 0; i < l->child_count; i++) {
        LayoutNode* child = l->children[i];
        ZccSvgNode* line = svg_path();
        svg_set_fill(line, "none");
        svg_set_stroke(line, "#390099");
        svg_set_stroke_width(line, "2.5");
        svg_set_opacity(line, "0.55");

        char path_d[256];
        float mid_y = (l->y + child->y) * 0.5f;
        sprintf(path_d, "M %.1f %.1f C %.1f %.1f, %.1f %.1f, %.1f %.1f",
                l->x, l->y + 25.0f,
                l->x, mid_y,
                child->x, mid_y,
                child->x, child->y - 25.0f);
        svg_set_attr(line, "d", path_d);
        svg_add_child(parent_svg, line);
    }

    ZccSvgNode* group = svg_g();
    ZccSvgNode* rect = svg_rect();
    char x_str[32], y_str[32];
    sprintf(x_str, "%.1f", l->x - 70.0f);
    sprintf(y_str, "%.1f", l->y - 25.0f);
    svg_set_x(rect, x_str);
    svg_set_y(rect, y_str);
    svg_set_width(rect, "140");
    svg_set_height(rect, "50");
    svg_set_attr(rect, "rx", "6");
    svg_set_attr(rect, "ry", "6");
    svg_set_fill(rect, "#0b0a1a");

    int k = l->znode->kind;
    if (k == ZND_IF || k == ZND_WHILE || k == ZND_FOR || k == ZND_BREAK || 
        k == ZND_CONTINUE || k == ZND_RETURN || k == ZND_SWITCH || k == ZND_GOTO || k == ZND_LABEL) {
        svg_set_stroke(rect, "#cc00ff");
    } else if (k == ZND_ASSIGN || k == ZND_ADD || k == ZND_SUB || k == ZND_MUL || 
               k == ZND_DIV || k == ZND_MOD || k == ZND_POST_INC || k == ZND_PRE_INC || 
               k == ZND_POST_DEC || k == ZND_PRE_DEC || k == ZND_COMPOUND_ASSIGN) {
        svg_set_stroke(rect, "#00ffcc");
    } else {
        svg_set_stroke(rect, "#3d30a2");
    }
    svg_set_stroke_width(rect, "2");
    svg_set_attr(rect, "style", "transition: all 0.2s ease-in-out; cursor: pointer;");
    svg_set_attr(rect, "onmouseover", "this.setAttribute('stroke-width', '3.5'); this.setAttribute('fill', '#14122d');");
    svg_set_attr(rect, "onmouseout", "this.setAttribute('stroke-width', '2'); this.setAttribute('fill', '#0b0a1a');");
    svg_add_child(group, rect);

    ZccSvgNode* txt1 = svg_text();
    char tx_str[32], ty_str1[32];
    sprintf(tx_str, "%.1f", l->x);
    sprintf(ty_str1, "%.1f", l->y - 4.0f);
    svg_set_x(txt1, tx_str);
    svg_set_y(txt1, ty_str1);
    svg_set_fill(txt1, "#ffffff");
    svg_set_attr(txt1, "font-size", "10");
    svg_set_attr(txt1, "font-family", "Inter, system-ui, sans-serif");
    svg_set_attr(txt1, "font-weight", "bold");
    svg_set_attr(txt1, "text-anchor", "middle");
    svg_set_content(txt1, zcc_kind_to_str(l->znode->kind));
    svg_add_child(group, txt1);

    ZccSvgNode* txt2 = svg_text();
    char ty_str2[32];
    sprintf(ty_str2, "%.1f", l->x);
    sprintf(ty_str2, "%.1f", l->y + 12.0f);
    svg_set_x(txt2, tx_str);
    svg_set_y(txt2, ty_str2);

    char details[64] = "";
    if (l->znode->name[0] != '\0') {
        sprintf(details, "sym: %s", l->znode->name);
    } else if (l->znode->int_val != 0 || l->znode->kind == ZND_NUM) {
        sprintf(details, "val: %lld", l->znode->int_val);
    } else if (l->znode->kind == ZND_VAR) {
        sprintf(details, "var: %s", l->znode->name);
    }

    if (details[0] != '\0') {
        svg_set_fill(txt2, "#8f8fb8");
        svg_set_attr(txt2, "font-size", "9");
        svg_set_attr(txt2, "font-family", "Inter, system-ui, sans-serif");
        svg_set_attr(txt2, "text-anchor", "middle");
        svg_set_content(txt2, details);
        svg_add_child(group, txt2);
    }

    svg_add_child(parent_svg, group);

    for (int i = 0; i < l->child_count; i++) {
        render_layout_to_nodes(l->children[i], parent_svg);
    }
}

static void free_layout_tree(LayoutNode* l) {
    if (!l) return;
    for (int i = 0; i < l->child_count; i++) {
        free_layout_tree(l->children[i]);
    }
    free(l);
}

char* svg_render_ast(struct ZCCNode* root) {
    if (!root) return NULL;

    LayoutNode* l = build_layout_tree((ZCCNode*)root, 0);
    if (!l) return NULL;

    assign_x_coordinates(l, 0.0f);

    float min_x = 1e9f, max_x = -1e9f, min_y = 1e9f, max_y = -1e9f;
    get_bounding_box(l, &min_x, &max_x, &min_y, &max_y);

    ZccSvgNode* svg = svg_svg();
    svg_set_xmlns(svg, "http://www.w3.org/2000/svg");

    char w_str[32], h_str[32], vb_str[128];
    float w = max_x - min_x;
    float h = max_y - min_y;
    sprintf(w_str, "%.1f", w);
    sprintf(h_str, "%.1f", h);
    sprintf(vb_str, "%.1f %.1f %.1f %.1f", min_x, min_y, w, h);
    svg_set_width(svg, w_str);
    svg_set_height(svg, h_str);
    svg_set_viewBox(svg, vb_str);

    ZccSvgNode* bg = svg_rect();
    char rx_str[32], ry_str[32];
    sprintf(rx_str, "%.1f", min_x);
    sprintf(ry_str, "%.1f", min_y);
    svg_set_x(bg, rx_str);
    svg_set_y(bg, ry_str);
    svg_set_width(bg, w_str);
    svg_set_height(bg, h_str);
    svg_set_fill(bg, "#020206");
    svg_add_child(svg, bg);

    render_layout_to_nodes(l, svg);

    char* svg_str = svg_to_string(svg);
    free_layout_tree(l);
    return svg_str;
}

char* svg_render_ast_html_uri(struct ZCCNode* root) {
    char* svg_str = svg_render_ast(root);
    if (!svg_str) return NULL;

    size_t len = strlen(svg_str);
    char* svg_single = (char*)malloc(len + 1);
    if (!svg_single) {
        free(svg_str);
        return NULL;
    }
    for (size_t i = 0; i <= len; i++) {
        if (svg_str[i] == '"') {
            svg_single[i] = '\'';
        } else {
            svg_single[i] = svg_str[i];
        }
    }
    free(svg_str);

    const char* html_pre = "<!DOCTYPE html><html><body style='margin:0;background:#020206;display:flex;justify-content:center;align-items:center;min-height:100vh;overflow:auto;'>";
    const char* html_post = "</body></html>";
    size_t pre_len = strlen(html_pre);
    size_t post_len = strlen(html_post);

    size_t html_len = pre_len + len + post_len;
    char* html_content = (char*)malloc(html_len + 1);
    if (!html_content) {
        free(svg_single);
        return NULL;
    }
    strcpy(html_content, html_pre);
    strcpy(html_content + pre_len, svg_single);
    strcpy(html_content + pre_len + len, html_post);
    free(svg_single);

    char* b64 = base64_encode((const unsigned char*)html_content, html_len);
    free(html_content);
    if (!b64) return NULL;

    const char* prefix = "data:text/html;base64,";
    size_t prefix_len = strlen(prefix);
    size_t b64_len = strlen(b64);
    char* uri = (char*)malloc(prefix_len + b64_len + 1);
    if (!uri) {
        free(b64);
        return NULL;
    }
    strcpy(uri, prefix);
    strcpy(uri + prefix_len, b64);
    free(b64);
    return uri;
}

static void _ast_to_ascii_rec(ZCCNode* z, char** out, int* cap, int* len, const char* prefix, int is_last) {
    if (!z) return;

    char details[128] = "";
    int k = z->kind;
    if (z->name[0] != '\0') {
        sprintf(details, " (sym: %s)", z->name);
    } else if (z->int_val != 0 || k == ZND_NUM) {
        sprintf(details, " (val: %lld)", z->int_val);
    } else if (k == ZND_VAR) {
        sprintf(details, " (var: %s)", z->name);
    }

    char node_line[256];
    sprintf(node_line, "%s%s%s%s\n", prefix, is_last ? "└── " : "├── ", zcc_kind_to_str(k), details);
    int line_len = strlen(node_line);

    while (*len + line_len + 512 > *cap) {
        *cap *= 2;
        *out = (char*)realloc(*out, *cap);
    }
    strcpy(*out + *len, node_line);
    *len += line_len;

    ZCCNode* children[64];
    int child_count = 0;
    #define PUSH_CHILD(c) if (c && child_count < 64) children[child_count++] = (ZCCNode*)(c);
    PUSH_CHILD(z->lhs);
    PUSH_CHILD(z->rhs);
    PUSH_CHILD(z->cond);
    PUSH_CHILD(z->then_body);
    PUSH_CHILD(z->else_body);
    PUSH_CHILD(z->body);
    PUSH_CHILD(z->init);
    PUSH_CHILD(z->inc);

    if (z->stmts && z->num_stmts > 0) {
        for (unsigned int i = 0; i < z->num_stmts; i++) {
            PUSH_CHILD(z->stmts[i]);
        }
    }
    if (z->args && z->num_args > 0) {
        for (int i = 0; i < z->num_args; i++) {
            PUSH_CHILD(z->args[i]);
        }
    }
    #undef PUSH_CHILD

    for (int i = 0; i < child_count; i++) {
        char new_prefix[256];
        sprintf(new_prefix, "%s%s", prefix, is_last ? "    " : "│   ");
        _ast_to_ascii_rec(children[i], out, cap, len, new_prefix, i == child_count - 1);
    }
}

char* zcc_ast_to_ascii(struct ZCCNode* root) {
    if (!root) return NULL;
    int cap = 4096;
    int len = 0;
    char* out = (char*)calloc(1, cap);

    char details[128] = "";
    int k = root->kind;
    if (root->name[0] != '\0') {
        sprintf(details, " (sym: %s)", root->name);
    } else if (root->int_val != 0 || k == ZND_NUM) {
        sprintf(details, " (val: %lld)", root->int_val);
    } else if (k == ZND_VAR) {
        sprintf(details, " (var: %s)", root->name);
    }

    len += sprintf(out, "%s%s\n", zcc_kind_to_str(k), details);

    ZCCNode* children[64];
    int child_count = 0;
    #define PUSH_CHILD(c) if (c && child_count < 64) children[child_count++] = (ZCCNode*)(c);
    PUSH_CHILD(root->lhs);
    PUSH_CHILD(root->rhs);
    PUSH_CHILD(root->cond);
    PUSH_CHILD(root->then_body);
    PUSH_CHILD(root->else_body);
    PUSH_CHILD(root->body);
    PUSH_CHILD(root->init);
    PUSH_CHILD(root->inc);

    if (root->stmts && root->num_stmts > 0) {
        for (unsigned int i = 0; i < root->num_stmts; i++) {
            PUSH_CHILD(root->stmts[i]);
        }
    }
    if (root->args && root->num_args > 0) {
        for (int i = 0; i < root->num_args; i++) {
            PUSH_CHILD(root->args[i]);
        }
    }
    #undef PUSH_CHILD

    for (int i = 0; i < child_count; i++) {
        _ast_to_ascii_rec(children[i], &out, &cap, &len, "", i == child_count - 1);
    }
    return out;
}

char* hexdump_to_ascii(const unsigned char* data, size_t len) {
    if (!data || len == 0) return NULL;
    size_t cap = 256 + len * 80;
    char* out = (char*)malloc(cap);
    if (!out) return NULL;
    size_t offset = 0;
    size_t out_len = 0;

    while (offset < len) {
        out_len += sprintf(out + out_len, "%08zX: ", offset);
        size_t chunk = len - offset;
        if (chunk > 16) chunk = 16;

        for (size_t i = 0; i < 16; i++) {
            if (i < chunk) {
                out_len += sprintf(out + out_len, "%02X ", data[offset + i]);
            } else {
                strcpy(out + out_len, "   ");
                out_len += 3;
            }
        }
        
        strcpy(out + out_len, " |");
        out_len += 2;

        for (size_t i = 0; i < chunk; i++) {
            unsigned char c = data[offset + i];
            if (c >= 32 && c <= 126) {
                out[out_len++] = c;
            } else {
                out[out_len++] = '.';
            }
        }
        out[out_len++] = '|';
        out[out_len++] = '\n';
        offset += chunk;
    }
    out[out_len] = '\0';
    return out;
}
''')

