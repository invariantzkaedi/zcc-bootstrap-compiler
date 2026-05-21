#include "zcc_svg.h"

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
    int needed = tag_len + attrs_len + 3;
    
    if (*len + needed + 2048 > *cap) {
        while (*len + needed + 2048 > *cap) {
            *cap *= 2;
        }
        *out = (char*)realloc(*out, *cap);
    }
    
    (*out)[*len] = '<';
    strcpy(*out + *len + 1, node->tag);
    *len += 1 + tag_len;
    if (node->attributes) {
        strcpy(*out + *len, node->attributes);
        *len += attrs_len;
    }
    (*out)[*len] = '>';
    (*out)[*len + 1] = '\0';
    *len += 1;
    
    if (node->content) {
        int slen = strlen(node->content);
        if (*len + slen + 2048 > *cap) {
            while (*len + slen + 2048 > *cap) {
                *cap *= 2;
            }
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
    
    int close_needed = tag_len + 4;
    if (*len + close_needed + 2048 > *cap) {
        while (*len + close_needed + 2048 > *cap) {
            *cap *= 2;
        }
        *out = (char*)realloc(*out, *cap);
    }
    sprintf(*out + *len, "</%s>\n", node->tag);
    *len += close_needed;
}

char* svg_to_string(ZccSvgNode* root) {
    int cap = 8192;
    int len = 0;
    char* out = (char*)calloc(1, cap);
    _svg_to_string_rec(root, &out, &cap, &len);
    return out;
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
ZccSvgNode* svg_animatetransform(void) { return svg_create_node("animatetransform"); }
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
ZccSvgNode* svg_image(void) { return svg_create_node("image"); }
ZccSvgNode* svg_line(void) { return svg_create_node("line"); }
ZccSvgNode* svg_linearGradient(void) { return svg_create_node("linearGradient"); }
ZccSvgNode* svg_lineargradient(void) { return svg_create_node("lineargradient"); }
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
ZccSvgNode* svg_svg(void) { return svg_create_node("svg"); }
ZccSvgNode* svg_switch(void) { return svg_create_node("switch"); }
ZccSvgNode* svg_symbol(void) { return svg_create_node("symbol"); }
ZccSvgNode* svg_text(void) { return svg_create_node("text"); }
ZccSvgNode* svg_textPath(void) { return svg_create_node("textPath"); }
ZccSvgNode* svg_textpath(void) { return svg_create_node("textpath"); }
ZccSvgNode* svg_title(void) { return svg_create_node("title"); }
ZccSvgNode* svg_tspan(void) { return svg_create_node("tspan"); }
ZccSvgNode* svg_use(void) { return svg_create_node("use"); }
void svg_set_accumulate(ZccSvgNode* node, const char* val) { svg_set_attr(node, "accumulate", val); }
void svg_set_additive(ZccSvgNode* node, const char* val) { svg_set_attr(node, "additive", val); }
void svg_set_aria_describedby(ZccSvgNode* node, const char* val) { svg_set_attr(node, "aria-describedby", val); }
void svg_set_aria_labelledby(ZccSvgNode* node, const char* val) { svg_set_attr(node, "aria-labelledby", val); }
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
void svg_set_letter_spacing(ZccSvgNode* node, const char* val) { svg_set_attr(node, "letter-spacing", val); }
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
void svg_set_repeatCount(ZccSvgNode* node, const char* val) { svg_set_attr(node, "repeatCount", val); }
void svg_set_repeatDur(ZccSvgNode* node, const char* val) { svg_set_attr(node, "repeatDur", val); }
void svg_set_restart(ZccSvgNode* node, const char* val) { svg_set_attr(node, "restart", val); }
void svg_set_rotate(ZccSvgNode* node, const char* val) { svg_set_attr(node, "rotate", val); }
void svg_set_rx(ZccSvgNode* node, const char* val) { svg_set_attr(node, "rx", val); }
void svg_set_ry(ZccSvgNode* node, const char* val) { svg_set_attr(node, "ry", val); }
void svg_set_startOffset(ZccSvgNode* node, const char* val) { svg_set_attr(node, "startOffset", val); }
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
void svg_set_x1(ZccSvgNode* node, const char* val) { svg_set_attr(node, "x1", val); }
void svg_set_x2(ZccSvgNode* node, const char* val) { svg_set_attr(node, "x2", val); }
void svg_set_xlink_href(ZccSvgNode* node, const char* val) { svg_set_attr(node, "xlink:href", val); }
void svg_set_xmlns(ZccSvgNode* node, const char* val) { svg_set_attr(node, "xmlns", val); }
void svg_set_y1(ZccSvgNode* node, const char* val) { svg_set_attr(node, "y1", val); }
void svg_set_y2(ZccSvgNode* node, const char* val) { svg_set_attr(node, "y2", val); }
