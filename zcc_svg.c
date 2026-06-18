#include "zcc_svg.h"
#include <math.h>

ZccSvgNode* svg_create_node(const char* tag) {
    ZccSvgNode* n = (ZccSvgNode*)calloc(1, sizeof(ZccSvgNode));
    n->tag = tag;
    return n;
}

void svg_add_child(ZccSvgNode* parent, ZccSvgNode* child) {
    if (!parent->children) {
        parent->children = child;
    } else {
        ZccSvgNode* cur = parent->children;
        while(cur->next) cur = cur->next;
        cur->next = child;
    }
}

void svg_set_attr(ZccSvgNode* node, const char* attr_name, const char* attr_val) {
    int name_len = strlen(attr_name);
    int val_len = strlen(attr_val);
    int new_len = name_len + val_len + 5;
    if (!node->attributes) {
        node->attributes = (char*)calloc(1, new_len);
        sprintf(node->attributes, " %s=\"%s\"", attr_name, attr_val);
    } else {
        int old_len = strlen(node->attributes);
        char* new_attrs = (char*)calloc(1, old_len + new_len);
        strcpy(new_attrs, node->attributes);
        sprintf(new_attrs + old_len, " %s=\"%s\"", attr_name, attr_val);
        free(node->attributes);
        node->attributes = new_attrs;
    }
}

void svg_set_content(ZccSvgNode* node, const char* text) {
    if (node->content) free(node->content);
    node->content = strdup(text);
}

static void _svg_to_string_rec(ZccSvgNode* node, char** out, int* cap, int* len) {
    if (!node) return;
    int tag_len = strlen(node->tag);
    int attrs_len = node->attributes ? strlen(node->attributes) : 0;
    int total_needed = tag_len + attrs_len + 3;
    while (*len + total_needed + 2048 > *cap) {
        *cap *= 2;
        *out = (char*)realloc(*out, *cap);
    }
    *len += sprintf(*out + *len, "<%s%s>", node->tag, node->attributes ? node->attributes : "");
    if (node->content) {
        int slen = strlen(node->content);
        while (*len + slen + 2048 > *cap) {
            *cap *= 2;
            *out = (char*)realloc(*out, *cap);
        }
        strcpy(*out + *len, node->content);
        *len += slen;
    }
    if (node->children) {
        ZccSvgNode* c = node->children;
        while(c) {
            _svg_to_string_rec(c, out, cap, len);
            c = c->next;
        }
    }
    int close_len = tag_len + 5;
    while (*len + close_len + 2048 > *cap) {
        *cap *= 2;
        *out = (char*)realloc(*out, *cap);
    }
    *len += sprintf(*out + *len, "</%s>\n", node->tag);
}

char* svg_to_string(ZccSvgNode* root) {
    int cap = 8192;
    int len = 0;
    char* out = (char*)calloc(1, cap);
    _svg_to_string_rec(root, &out, &cap, &len);
    return out;
}

/* Recursively frees all nodes in a ZccSvgNode tree, including
 * their siblings (next chain) and children, plus heap-allocated
 * attributes and content strings.  Callers are responsible for
 * calling this on any root they own after they are done with it. */
void svg_free_node_tree(ZccSvgNode* node) {
    if (!node) return;
    svg_free_node_tree(node->children);
    svg_free_node_tree(node->next);
    if (node->attributes) free(node->attributes);
    if (node->content) free(node->content);
    free(node);
}

/* Base64 & ASCII Utility Implementations */
static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int get_b64_char_value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

char* base64_encode(const unsigned char* data, size_t input_length) {
    size_t output_length = 4 * ((input_length + 2) / 3);
    char* encoded_data = (char*)malloc(output_length + 1);
    if (!encoded_data) return NULL;

    size_t i, j;
    for (i = 0, j = 0; i < input_length;) {
        uint32_t octet_a = i < input_length ? data[i++] : 0;
        uint32_t octet_b = i < input_length ? data[i++] : 0;
        uint32_t octet_c = i < input_length ? data[i++] : 0;

        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        encoded_data[j++] = b64_table[(triple >> 3 * 6) & 0x3F];
        encoded_data[j++] = b64_table[(triple >> 2 * 6) & 0x3F];
        encoded_data[j++] = b64_table[(triple >> 1 * 6) & 0x3F];
        encoded_data[j++] = b64_table[(triple >> 0 * 6) & 0x3F];
    }

    size_t mod = input_length % 3;
    if (mod == 1) {
        encoded_data[output_length - 2] = '=';
        encoded_data[output_length - 1] = '=';
    } else if (mod == 2) {
        encoded_data[output_length - 1] = '=';
    }

    encoded_data[output_length] = '\0';
    return encoded_data;
}

unsigned char* base64_decode(const char* data, size_t input_length, size_t* output_length) {
    size_t clean_len = 0;
    for (size_t i = 0; i < input_length; i++) {
        char c = data[i];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            clean_len++;
        }
    }

    if (clean_len % 4 != 0) return NULL;

    size_t padding = 0;
    int non_ws_count = 0;
    char last_chars[4] = {0};
    for (size_t i = input_length; i > 0; i--) {
        char c = data[i - 1];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            if (non_ws_count < 4) {
                last_chars[3 - non_ws_count] = c;
                non_ws_count++;
            }
        }
    }
    if (non_ws_count >= 1 && last_chars[3] == '=') {
        padding++;
        if (non_ws_count >= 2 && last_chars[2] == '=') padding++;
    }

    size_t out_len = (clean_len / 4) * 3 - padding;
    unsigned char* decoded_data = (unsigned char*)malloc(out_len + 1);
    if (!decoded_data) return NULL;

    size_t i = 0, j = 0;
    while (i < input_length) {
        char chars[4] = {0};
        int filled = 0;
        while (filled < 4 && i < input_length) {
            char c = data[i++];
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
                chars[filled++] = c;
            }
        }
        if (filled == 0) break;
        if (filled < 4) {
            free(decoded_data);
            return NULL;
        }

        int val0 = get_b64_char_value(chars[0]);
        int val1 = get_b64_char_value(chars[1]);
        int val2 = chars[2] != '=' ? get_b64_char_value(chars[2]) : -1;
        int val3 = chars[3] != '=' ? get_b64_char_value(chars[3]) : -1;

        if (val0 < 0 || val1 < 0) {
            free(decoded_data);
            return NULL;
        }

        uint32_t triple = (val0 << 18) + (val1 << 12);
        if (val2 >= 0) triple += (val2 << 6);
        if (val3 >= 0) triple += val3;

        if (j < out_len) decoded_data[j++] = (triple >> 16) & 0xFF;
        if (j < out_len) decoded_data[j++] = (triple >> 8) & 0xFF;
        if (j < out_len) decoded_data[j++] = triple & 0xFF;
    }
    decoded_data[out_len] = '\0';

    if (output_length) *output_length = out_len;
    return decoded_data;
}

char* svg_to_base64(ZccSvgNode* root) {
    char* svg_str = svg_to_string(root);
    if (!svg_str) return NULL;
    char* b64 = base64_encode((const unsigned char*)svg_str, strlen(svg_str));
    free(svg_str);
    return b64;
}

char* svg_to_data_uri(ZccSvgNode* root) {
    char* b64 = svg_to_base64(root);
    if (!b64) return NULL;
    size_t b64_len = strlen(b64);
    const char* prefix = "data:image/svg+xml;base64,";
    size_t prefix_len = strlen(prefix);
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

char* svg_to_html_uri(ZccSvgNode* root) {
    char* svg_str = svg_to_string(root);
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

    const char* html_pre = "<!DOCTYPE html><html><body style='margin:0;background:#03010a;overflow:hidden;'>";
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

/* Path Builder Implementation */
SvgPathBuilder* svg_path_begin() {
    SvgPathBuilder* pb = (SvgPathBuilder*)calloc(1, sizeof(SvgPathBuilder));
    pb->cap = 256;
    pb->d = (char*)calloc(1, pb->cap);
    return pb;
}
static void _pb_append(SvgPathBuilder* pb, const char* str) {
    int slen = strlen(str);
    if (pb->len + slen + 64 > pb->cap) {
        pb->cap *= 2;
        pb->d = (char*)realloc(pb->d, pb->cap);
    }
    strcpy(pb->d + pb->len, str);
    pb->len += slen;
}
void svg_path_move_to(SvgPathBuilder* pb, float x, float y) {
    char buf[64]; sprintf(buf, "M%.2f,%.2f ", x, y); _pb_append(pb, buf);
}
void svg_path_line_to(SvgPathBuilder* pb, float x, float y) {
    char buf[64]; sprintf(buf, "L%.2f,%.2f ", x, y); _pb_append(pb, buf);
}
void svg_path_cubic_to(SvgPathBuilder* pb, float x1, float y1, float x2, float y2, float x, float y) {
    char buf[128]; sprintf(buf, "C%.2f,%.2f %.2f,%.2f %.2f,%.2f ", x1, y1, x2, y2, x, y); _pb_append(pb, buf);
}
void svg_path_close(SvgPathBuilder* pb) {
    _pb_append(pb, "Z ");
}
void svg_apply_path(ZccSvgNode* node, SvgPathBuilder* pb) {
    svg_set_attr(node, "d", pb->d);
    free(pb->d);
    free(pb);
}

ZccSvgNode* svg_animate(void) { return svg_create_node("animate"); }
ZccSvgNode* svg_animateMotion(void) { return svg_create_node("animateMotion"); }
ZccSvgNode* svg_animateTransform(void) { return svg_create_node("animateTransform"); }
ZccSvgNode* svg_circle(void) { return svg_create_node("circle"); }
ZccSvgNode* svg_clipPath(void) { return svg_create_node("clipPath"); }
ZccSvgNode* svg_defs(void) { return svg_create_node("defs"); }
ZccSvgNode* svg_desc(void) { return svg_create_node("desc"); }
ZccSvgNode* svg_ellipse(void) { return svg_create_node("ellipse"); }
ZccSvgNode* svg_feBlend(void) { return svg_create_node("feBlend"); }
ZccSvgNode* svg_feColorMatrix(void) { return svg_create_node("feColorMatrix"); }
ZccSvgNode* svg_feComponentTransfer(void) { return svg_create_node("feComponentTransfer"); }
ZccSvgNode* svg_feComposite(void) { return svg_create_node("feComposite"); }
ZccSvgNode* svg_feConvolveMatrix(void) { return svg_create_node("feConvolveMatrix"); }
ZccSvgNode* svg_feDiffuseLighting(void) { return svg_create_node("feDiffuseLighting"); }
ZccSvgNode* svg_feDisplacementMap(void) { return svg_create_node("feDisplacementMap"); }
ZccSvgNode* svg_feFlood(void) { return svg_create_node("feFlood"); }
ZccSvgNode* svg_feGaussianBlur(void) { return svg_create_node("feGaussianBlur"); }
ZccSvgNode* svg_feImage(void) { return svg_create_node("feImage"); }
ZccSvgNode* svg_feMerge(void) { return svg_create_node("feMerge"); }
ZccSvgNode* svg_feMergeNode(void) { return svg_create_node("feMergeNode"); }
ZccSvgNode* svg_feMorphology(void) { return svg_create_node("feMorphology"); }
ZccSvgNode* svg_feOffset(void) { return svg_create_node("feOffset"); }
ZccSvgNode* svg_feSpecularLighting(void) { return svg_create_node("feSpecularLighting"); }
ZccSvgNode* svg_feTile(void) { return svg_create_node("feTile"); }
ZccSvgNode* svg_feTurbulence(void) { return svg_create_node("feTurbulence"); }
ZccSvgNode* svg_filter(void) { return svg_create_node("filter"); }
ZccSvgNode* svg_g(void) { return svg_create_node("g"); }
ZccSvgNode* svg_image(void) { return svg_create_node("image"); }
ZccSvgNode* svg_line(void) { return svg_create_node("line"); }
ZccSvgNode* svg_linearGradient(void) { return svg_create_node("linearGradient"); }
ZccSvgNode* svg_mask(void) { return svg_create_node("mask"); }
ZccSvgNode* svg_mpath(void) { return svg_create_node("mpath"); }
ZccSvgNode* svg_path(void) { return svg_create_node("path"); }
ZccSvgNode* svg_pattern(void) { return svg_create_node("pattern"); }
ZccSvgNode* svg_polygon(void) { return svg_create_node("polygon"); }
ZccSvgNode* svg_polyline(void) { return svg_create_node("polyline"); }
ZccSvgNode* svg_radialGradient(void) { return svg_create_node("radialGradient"); }
ZccSvgNode* svg_rect(void) { return svg_create_node("rect"); }
ZccSvgNode* svg_script(void) { return svg_create_node("script"); }
ZccSvgNode* svg_set(void) { return svg_create_node("set"); }
ZccSvgNode* svg_stop(void) { return svg_create_node("stop"); }
ZccSvgNode* svg_style(void) { return svg_create_node("style"); }
ZccSvgNode* svg_svg(void) {
    ZccSvgNode* n = svg_create_node("svg");
    svg_set_attr(n, "xmlns", "http://www.w3.org/2000/svg");
    return n;
}
ZccSvgNode* svg_switch(void) { return svg_create_node("switch"); }
ZccSvgNode* svg_symbol(void) { return svg_create_node("symbol"); }
ZccSvgNode* svg_text(void) { return svg_create_node("text"); }
ZccSvgNode* svg_textPath(void) { return svg_create_node("textPath"); }
ZccSvgNode* svg_title(void) { return svg_create_node("title"); }
ZccSvgNode* svg_tspan(void) { return svg_create_node("tspan"); }
ZccSvgNode* svg_use(void) { return svg_create_node("use"); }
void svg_set_accumulate(ZccSvgNode* node, const char* val) { svg_set_attr(node, "accumulate", val); }
void svg_set_additive(ZccSvgNode* node, const char* val) { svg_set_attr(node, "additive", val); }
void svg_set_attributeName(ZccSvgNode* node, const char* val) { svg_set_attr(node, "attributeName", val); }
void svg_set_attributeType(ZccSvgNode* node, const char* val) { svg_set_attr(node, "attributeType", val); }
void svg_set_begin(ZccSvgNode* node, const char* val) { svg_set_attr(node, "begin", val); }
void svg_set_by(ZccSvgNode* node, const char* val) { svg_set_attr(node, "by", val); }
void svg_set_calcMode(ZccSvgNode* node, const char* val) { svg_set_attr(node, "calcMode", val); }
void svg_set_class(ZccSvgNode* node, const char* val) { svg_set_attr(node, "class", val); }
void svg_set_clip_path(ZccSvgNode* node, const char* val) { svg_set_attr(node, "clip-path", val); }
void svg_set_color(ZccSvgNode* node, const char* val) { svg_set_attr(node, "color", val); }
void svg_set_cx(ZccSvgNode* node, const char* val) { svg_set_attr(node, "cx", val); }
void svg_set_cy(ZccSvgNode* node, const char* val) { svg_set_attr(node, "cy", val); }
void svg_set_d(ZccSvgNode* node, const char* val) { svg_set_attr(node, "d", val); }
void svg_set_dominant_baseline(ZccSvgNode* node, const char* val) { svg_set_attr(node, "dominant-baseline", val); }
void svg_set_dur(ZccSvgNode* node, const char* val) { svg_set_attr(node, "dur", val); }
void svg_set_end(ZccSvgNode* node, const char* val) { svg_set_attr(node, "end", val); }
void svg_set_fill(ZccSvgNode* node, const char* val) { svg_set_attr(node, "fill", val); }
void svg_set_fill_opacity(ZccSvgNode* node, const char* val) { svg_set_attr(node, "fill-opacity", val); }
void svg_set_fill_rule(ZccSvgNode* node, const char* val) { svg_set_attr(node, "fill-rule", val); }
void svg_set_filter(ZccSvgNode* node, const char* val) { svg_set_attr(node, "filter", val); }
void svg_set_font_family(ZccSvgNode* node, const char* val) { svg_set_attr(node, "font-family", val); }
void svg_set_font_size(ZccSvgNode* node, const char* val) { svg_set_attr(node, "font-size", val); }
void svg_set_font_style(ZccSvgNode* node, const char* val) { svg_set_attr(node, "font-style", val); }
void svg_set_font_weight(ZccSvgNode* node, const char* val) { svg_set_attr(node, "font-weight", val); }
void svg_set_from(ZccSvgNode* node, const char* val) { svg_set_attr(node, "from", val); }
void svg_set_gradientTransform(ZccSvgNode* node, const char* val) { svg_set_attr(node, "gradientTransform", val); }
void svg_set_gradientUnits(ZccSvgNode* node, const char* val) { svg_set_attr(node, "gradientUnits", val); }
void svg_set_height(ZccSvgNode* node, const char* val) { svg_set_attr(node, "height", val); }
void svg_set_href(ZccSvgNode* node, const char* val) { svg_set_attr(node, "href", val); }
void svg_set_id(ZccSvgNode* node, const char* val) { svg_set_attr(node, "id", val); }
void svg_set_keyPoints(ZccSvgNode* node, const char* val) { svg_set_attr(node, "keyPoints", val); }
void svg_set_keySplines(ZccSvgNode* node, const char* val) { svg_set_attr(node, "keySplines", val); }
void svg_set_keyTimes(ZccSvgNode* node, const char* val) { svg_set_attr(node, "keyTimes", val); }
void svg_set_mask(ZccSvgNode* node, const char* val) { svg_set_attr(node, "mask", val); }
void svg_set_max(ZccSvgNode* node, const char* val) { svg_set_attr(node, "max", val); }
void svg_set_min(ZccSvgNode* node, const char* val) { svg_set_attr(node, "min", val); }
void svg_set_offset(ZccSvgNode* node, const char* val) { svg_set_attr(node, "offset", val); }
void svg_set_onbegin(ZccSvgNode* node, const char* val) { svg_set_attr(node, "onbegin", val); }
void svg_set_onend(ZccSvgNode* node, const char* val) { svg_set_attr(node, "onend", val); }
void svg_set_onrepeat(ZccSvgNode* node, const char* val) { svg_set_attr(node, "onrepeat", val); }
void svg_set_opacity(ZccSvgNode* node, const char* val) { svg_set_attr(node, "opacity", val); }
void svg_set_path(ZccSvgNode* node, const char* val) { svg_set_attr(node, "path", val); }
void svg_set_patternTransform(ZccSvgNode* node, const char* val) { svg_set_attr(node, "patternTransform", val); }
void svg_set_patternUnits(ZccSvgNode* node, const char* val) { svg_set_attr(node, "patternUnits", val); }
void svg_set_points(ZccSvgNode* node, const char* val) { svg_set_attr(node, "points", val); }
void svg_set_preserveAspectRatio(ZccSvgNode* node, const char* val) { svg_set_attr(node, "preserveAspectRatio", val); }
void svg_set_r(ZccSvgNode* node, const char* val) { svg_set_attr(node, "r", val); }
void svg_set_repeatCount(ZccSvgNode* node, const char* val) { svg_set_attr(node, "repeatCount", val); }
void svg_set_repeatDur(ZccSvgNode* node, const char* val) { svg_set_attr(node, "repeatDur", val); }
void svg_set_restart(ZccSvgNode* node, const char* val) { svg_set_attr(node, "restart", val); }
void svg_set_rotate(ZccSvgNode* node, const char* val) { svg_set_attr(node, "rotate", val); }
void svg_set_rx(ZccSvgNode* node, const char* val) { svg_set_attr(node, "rx", val); }
void svg_set_ry(ZccSvgNode* node, const char* val) { svg_set_attr(node, "ry", val); }
void svg_set_stop_color(ZccSvgNode* node, const char* val) { svg_set_attr(node, "stop-color", val); }
void svg_set_stop_opacity(ZccSvgNode* node, const char* val) { svg_set_attr(node, "stop-opacity", val); }
void svg_set_stroke(ZccSvgNode* node, const char* val) { svg_set_attr(node, "stroke", val); }
void svg_set_stroke_dasharray(ZccSvgNode* node, const char* val) { svg_set_attr(node, "stroke-dasharray", val); }
void svg_set_stroke_dashoffset(ZccSvgNode* node, const char* val) { svg_set_attr(node, "stroke-dashoffset", val); }
void svg_set_stroke_linecap(ZccSvgNode* node, const char* val) { svg_set_attr(node, "stroke-linecap", val); }
void svg_set_stroke_linejoin(ZccSvgNode* node, const char* val) { svg_set_attr(node, "stroke-linejoin", val); }
void svg_set_stroke_miterlimit(ZccSvgNode* node, const char* val) { svg_set_attr(node, "stroke-miterlimit", val); }
void svg_set_stroke_opacity(ZccSvgNode* node, const char* val) { svg_set_attr(node, "stroke-opacity", val); }
void svg_set_stroke_width(ZccSvgNode* node, const char* val) { svg_set_attr(node, "stroke-width", val); }
void svg_set_style(ZccSvgNode* node, const char* val) { svg_set_attr(node, "style", val); }
void svg_set_systemLanguage(ZccSvgNode* node, const char* val) { svg_set_attr(node, "systemLanguage", val); }
void svg_set_target(ZccSvgNode* node, const char* val) { svg_set_attr(node, "target", val); }
void svg_set_text_anchor(ZccSvgNode* node, const char* val) { svg_set_attr(node, "text-anchor", val); }
void svg_set_to(ZccSvgNode* node, const char* val) { svg_set_attr(node, "to", val); }
void svg_set_transform(ZccSvgNode* node, const char* val) { svg_set_attr(node, "transform", val); }
void svg_set_type(ZccSvgNode* node, const char* val) { svg_set_attr(node, "type", val); }
void svg_set_values(ZccSvgNode* node, const char* val) { svg_set_attr(node, "values", val); }
void svg_set_viewBox(ZccSvgNode* node, const char* val) { svg_set_attr(node, "viewBox", val); }
void svg_set_width(ZccSvgNode* node, const char* val) { svg_set_attr(node, "width", val); }
void svg_set_x(ZccSvgNode* node, const char* val) { svg_set_attr(node, "x", val); }
void svg_set_x1(ZccSvgNode* node, const char* val) { svg_set_attr(node, "x1", val); }
void svg_set_x2(ZccSvgNode* node, const char* val) { svg_set_attr(node, "x2", val); }
void svg_set_xlink_href(ZccSvgNode* node, const char* val) { svg_set_attr(node, "xlink:href", val); }
void svg_set_xmlns(ZccSvgNode* node, const char* val) { svg_set_attr(node, "xmlns", val); }
void svg_set_y(ZccSvgNode* node, const char* val) { svg_set_attr(node, "y", val); }
void svg_set_y1(ZccSvgNode* node, const char* val) { svg_set_attr(node, "y1", val); }
void svg_set_y2(ZccSvgNode* node, const char* val) { svg_set_attr(node, "y2", val); }

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
    } else {
        /* txt2 was allocated but there is no detail text for this
         * node — free it here rather than leaving a phantom. */
        svg_free_node_tree(txt2);
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
    /* Free the internally-constructed ZccSvgNode DOM; the serialised
     * string (svg_str) is what the caller owns and must free. */
    svg_free_node_tree(svg);
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

static double zcc_log2(double x) {
    if (x <= 0.0) return 0.0;
    uint64_t u = *(uint64_t*)&x;
    int exp = ((u >> 52) & 0x7FF) - 1023;
    u = (u & 0xFFFFFFFFFFFFFULL) | 0x3FF0000000000000ULL;
    double m = *(double*)&u;
    double z = m - 1.0;
    double log2_m = z * (1.4426950408889634 + z * (-0.7213475204444817 + z * (0.4808983469629878 - 0.3606737602222409 * z)));
    return (double)exp + log2_m;
}

char* hexdump_to_ascii(const unsigned char* data, size_t len) {
    if (!data || len == 0) return NULL;
    
    size_t nulls = 0;
    size_t printable = 0;
    size_t byte_counts[256];
    for (int i = 0; i < 256; i++) {
        byte_counts[i] = 0;
    }
    for (size_t i = 0; i < len; i++) {
        unsigned char b = data[i];
        byte_counts[b]++;
        if (b == 0) nulls++;
        if (b >= 32 && b <= 126) printable++;
    }
    
    double entropy = 0.0;
    for (int i = 0; i < 256; i++) {
        if (byte_counts[i] > 0) {
            double p = (double)byte_counts[i] / len;
            entropy -= p * zcc_log2(p);
        }
    }
    
    size_t cap = 1024 + (len / 16 + 1) * 100;
    char* out = (char*)malloc(cap);
    if (!out) return NULL;
    size_t out_len = 0;
    
    out_len += sprintf(out + out_len, 
        "[HEXDUMP: Size = %zu bytes, Nulls = %zu, Printable = %zu, Entropy = %.4f bits/byte]\n",
        len, nulls, printable, entropy);
        
    out_len += sprintf(out + out_len, 
        "Offset    00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F  ASCII Text\n"
        "--------  -----------------------  -----------------------  ----------------\n");
        
    size_t offset = 0;
    while (offset < len) {
        out_len += sprintf(out + out_len, "%08X  ", (unsigned int)offset);
        
        size_t chunk = len - offset;
        if (chunk > 16) chunk = 16;
        
        for (size_t i = 0; i < 8; i++) {
            if (offset + i < len) {
                out_len += sprintf(out + out_len, "%02X ", data[offset + i]);
            } else {
                strcpy(out + out_len, "   ");
                out_len += 3;
            }
        }
        
        strcpy(out + out_len, " ");
        out_len += 1;
        
        for (size_t i = 8; i < 16; i++) {
            if (offset + i < len) {
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
        
        if (chunk < 16) {
            for (size_t i = chunk; i < 16; i++) {
                out[out_len++] = ' ';
            }
        }
        
        out[out_len++] = '|';
        out[out_len++] = '\n';
        offset += 16;
    }
    out[out_len] = '\0';
    return out;
}

char* hexdump_to_svg(const unsigned char* data, size_t len) {
    if (!data || len == 0) return NULL;

    size_t nulls = 0;
    size_t printable = 0;
    size_t byte_counts[256];
    for (int i = 0; i < 256; i++) {
        byte_counts[i] = 0;
    }
    for (size_t i = 0; i < len; i++) {
        unsigned char b = data[i];
        byte_counts[b]++;
        if (b == 0) nulls++;
        if (b >= 32 && b <= 126) printable++;
    }

    double entropy = 0.0;
    for (int i = 0; i < 256; i++) {
        if (byte_counts[i] > 0) {
            double p = (double)byte_counts[i] / len;
            entropy -= p * zcc_log2(p);
        }
    }

    size_t cap = 8192 + len * 400;
    char* out = (char*)malloc(cap);
    if (!out) return NULL;
    size_t out_len = 0;

    out_len += sprintf(out + out_len,
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"800\" height=\"600\" viewBox=\"0 0 800 600\">\n"
        "  <defs>\n"
        "    <linearGradient id=\"cyberGrad\" x1=\"0%%\" y1=\"0%%\" x2=\"100%%\" y2=\"100%%\">\n"
        "      <stop offset=\"0%%\" stop-color=\"#00ffcc\"/>\n"
        "      <stop offset=\"50%%\" stop-color=\"#ff007f\"/>\n"
        "      <stop offset=\"100%%\" stop-color=\"#7f00ff\"/>\n"
        "    </linearGradient>\n"
        "    <filter id=\"glow\">\n"
        "      <feGaussianBlur stdDeviation=\"3\" result=\"coloredBlur\"/>\n"
        "      <feMerge>\n"
        "        <feMergeNode in=\"coloredBlur\"/>\n"
        "        <feMergeNode in=\"SourceGraphic\"/>\n"
        "      </feMerge>\n"
        "    </filter>\n"
        "  </defs>\n"
        "  <!-- Background -->\n"
        "  <rect width=\"800\" height=\"600\" fill=\"#05040d\" rx=\"8\" ry=\"8\"/>\n"
        "  <!-- Grid overlay -->\n"
        "  <g stroke=\"#15112e\" stroke-width=\"1\">\n");

    for (int x = 20; x < 800; x += 20) {
        out_len += sprintf(out + out_len, "    <line x1=\"%d\" y1=\"0\" x2=\"%d\" y2=\"600\" opacity=\"0.4\"/>\n", x, x);
    }
    for (int y = 20; y < 600; y += 20) {
        out_len += sprintf(out + out_len, "    <line x1=\"0\" y1=\"%d\" x2=\"800\" y2=\"%d\" opacity=\"0.4\"/>\n", y, y);
    }

    out_len += sprintf(out + out_len,
        "  </g>\n"
        "  <!-- Outer Border -->\n"
        "  <rect x=\"2\" y=\"2\" width=\"796\" height=\"596\" fill=\"none\" stroke=\"url(#cyberGrad)\" stroke-width=\"2\" rx=\"8\" ry=\"8\"/>\n"
        "  <!-- Header Panel -->\n"
        "  <rect x=\"30\" y=\"25\" width=\"740\" height=\"45\" rx=\"6\" ry=\"6\" fill=\"#0c0a1a\" stroke=\"#1c163a\" stroke-width=\"1.5\"/>\n"
        "  <text x=\"50\" y=\"52\" font-family=\"monospace\" font-size=\"15\" font-weight=\"bold\" fill=\"#00ffcc\" letter-spacing=\"2\" filter=\"url(#glow)\">ZCC SOVEREIGN BINARY RESOURCE METRIC ANALYZER</text>\n"
        "  <!-- Left Panel: Memory Matrix -->\n"
        "  <rect x=\"30\" y=\"90\" width=\"420\" height=\"480\" rx=\"6\" ry=\"6\" fill=\"#080613\" stroke=\"#1c163a\" stroke-width=\"1.5\"/>\n"
        "  <text x=\"50\" y=\"116\" font-family=\"monospace\" font-size=\"11\" font-weight=\"bold\" fill=\"#7f84b6\">MEMORY MATRIX MAP (16-BYTE ALIGNED)</text>\n");

    size_t num_rows = (len + 15) / 16;
    if (num_rows > 14) num_rows = 14;

    for (size_t r = 0; r < num_rows; r++) {
        out_len += sprintf(out + out_len,
            "  <text x=\"50\" y=\"%d\" font-family=\"monospace\" font-size=\"10\" fill=\"#50508a\">0x%08X</text>\n",
            (int)(146 + r * 28), (unsigned int)(r * 16));

        for (size_t c = 0; c < 16; c++) {
            size_t idx = r * 16 + c;
            if (idx >= len) break;

            unsigned char b = data[idx];
            int x_pos = 120 + c * 19;
            if (c >= 8) x_pos += 8;
            int y_pos = 132 + r * 28;

            const char* color_fill = "";
            const char* color_stroke = "";
            const char* text_color = "#ffffff";

            if (b == 0) {
                color_fill = "#141124";
                color_stroke = "#231f3c";
                text_color = "#4c486a";
            } else if (b >= 32 && b <= 126) {
                color_fill = "#00e5ff";
                color_stroke = "#0088aa";
                text_color = "#000000";
            } else if (b >= 128) {
                color_fill = "#8b2ff5";
                color_stroke = "#4b0082";
                text_color = "#ffffff";
            } else {
                color_fill = "#ff007f";
                color_stroke = "#aa0055";
                text_color = "#ffffff";
            }

            out_len += sprintf(out + out_len,
                "  <rect x=\"%d\" y=\"%d\" width=\"15\" height=\"20\" rx=\"2\" ry=\"2\" fill=\"%s\" stroke=\"%s\" stroke-width=\"1\">\n"
                "    <title>Offset: 0x%zX | Byte: 0x%02X (%d)</title>\n"
                "  </rect>\n",
                x_pos, y_pos, color_fill, color_stroke, idx, b, (int)b);

            out_len += sprintf(out + out_len,
                "  <text x=\"%d\" y=\"%d\" font-family=\"monospace\" font-size=\"7.5\" font-weight=\"bold\" fill=\"%s\" text-anchor=\"middle\">%02X</text>\n",
                x_pos + 7, y_pos + 13, text_color, b);
        }
    }

    out_len += sprintf(out + out_len,
        "  <rect x=\"470\" y=\"90\" width=\"300\" height=\"480\" rx=\"6\" ry=\"6\" fill=\"#080613\" stroke=\"#1c163a\" stroke-width=\"1.5\"/>\n"
        "  <text x=\"490\" y=\"116\" font-family=\"monospace\" font-size=\"11\" font-weight=\"bold\" fill=\"#7f84b6\">SHANNON ENTROPY PROFILE</text>\n");

    double dash_offset = 314.159 * (1.0 - (entropy > 8.0 ? 8.0 : entropy) / 8.0);
    out_len += sprintf(out + out_len,
        "  <!-- Radial Gauge -->\n"
        "  <circle cx=\"620\" cy=\"210\" r=\"50\" fill=\"none\" stroke=\"#121024\" stroke-width=\"10\"/>\n"
        "  <circle cx=\"620\" cy=\"210\" r=\"50\" fill=\"none\" stroke=\"url(#cyberGrad)\" stroke-width=\"10\"\n"
        "          stroke-dasharray=\"314.159\" stroke-dashoffset=\"%.3f\" transform=\"rotate(-90 620 210)\" filter=\"url(#glow)\"/>\n"
        "  <text x=\"620\" y=\"214\" font-family=\"monospace\" font-size=\"14\" font-weight=\"bold\" fill=\"#ffffff\" text-anchor=\"middle\">%.4f</text>\n"
        "  <text x=\"620\" y=\"228\" font-family=\"monospace\" font-size=\"9\" fill=\"#7f84b6\" text-anchor=\"middle\">bits/byte</text>\n",
        dash_offset, entropy);

    double null_pct = len > 0 ? (double)nulls / len * 100.0 : 0.0;
    double print_pct = len > 0 ? (double)printable / len * 100.0 : 0.0;
    double other_pct = 100.0 - null_pct - print_pct;
    if (other_pct < 0.0) other_pct = 0.0;

    int null_bar_w = (int)(240 * (null_pct / 100.0));
    int print_bar_w = (int)(240 * (print_pct / 100.0));
    int other_bar_w = (int)(240 * (other_pct / 100.0));

    out_len += sprintf(out + out_len,
        "  <!-- Progress Bars -->\n"
        "  <text x=\"490\" y=\"285\" font-family=\"monospace\" font-size=\"10\" fill=\"#ffffff\">TOTAL DATA SIZE: %zu bytes</text>\n"
        "  <rect x=\"490\" y=\"293\" width=\"260\" height=\"8\" rx=\"2\" ry=\"2\" fill=\"#121024\"/>\n"
        "  <rect x=\"490\" y=\"293\" width=\"260\" height=\"8\" rx=\"2\" ry=\"2\" fill=\"#7f00ff\" filter=\"url(#glow)\"/>\n",
        len);

    out_len += sprintf(out + out_len,
        "  <text x=\"490\" y=\"335\" font-family=\"monospace\" font-size=\"10\" fill=\"#ffffff\">NULL BYTES: %zu (%.1f%%)</text>\n"
        "  <rect x=\"490\" y=\"343\" width=\"260\" height=\"8\" rx=\"2\" ry=\"2\" fill=\"#121024\"/>\n"
        "  <rect x=\"490\" y=\"343\" width=\"%d\" height=\"8\" rx=\"2\" ry=\"2\" fill=\"#ff007f\" filter=\"url(#glow)\"/>\n",
        nulls, null_pct, null_bar_w);

    out_len += sprintf(out + out_len,
        "  <text x=\"490\" y=\"385\" font-family=\"monospace\" font-size=\"10\" fill=\"#ffffff\">PRINTABLE ASCII: %zu (%.1f%%)</text>\n"
        "  <rect x=\"490\" y=\"393\" width=\"260\" height=\"8\" rx=\"2\" ry=\"2\" fill=\"#121024\"/>\n"
        "  <rect x=\"490\" y=\"393\" width=\"%d\" height=\"8\" rx=\"2\" ry=\"2\" fill=\"#00ffcc\" filter=\"url(#glow)\"/>\n",
        printable, print_pct, print_bar_w);

    out_len += sprintf(out + out_len,
        "  <text x=\"490\" y=\"435\" font-family=\"monospace\" font-size=\"10\" fill=\"#ffffff\">CONTROL/EXTENDED: %zu (%.1f%%)</text>\n"
        "  <rect x=\"490\" y=\"443\" width=\"260\" height=\"8\" rx=\"2\" ry=\"2\" fill=\"#121024\"/>\n"
        "  <rect x=\"490\" y=\"443\" width=\"%d\" height=\"8\" rx=\"2\" ry=\"2\" fill=\"#8b2ff5\" filter=\"url(#glow)\"/>\n",
        len - nulls - printable, other_pct, other_bar_w);

    out_len += sprintf(out + out_len,
        "  <!-- Footer Status -->\n"
        "  <line x1=\"490\" y1=\"485\" x2=\"750\" y2=\"485\" stroke=\"#1c163a\" stroke-width=\"1\"/>\n"
        "  <text x=\"490\" y=\"515\" font-family=\"monospace\" font-size=\"10\" fill=\"#50508a\">SYSTEM STATUS: SECURE</text>\n"
        "  <text x=\"490\" y=\"535\" font-family=\"monospace\" font-size=\"10\" fill=\"#50508a\">ABI LANE: X86-64 SYSTEM V</text>\n"
        "  <text x=\"490\" y=\"555\" font-family=\"monospace\" font-size=\"10\" fill=\"#00ffcc\" filter=\"url(#glow)\">ZCC SOVEREIGN COMPILED</text>\n"
        "</svg>\n");

    out[out_len] = '\0';
    return out;
}
