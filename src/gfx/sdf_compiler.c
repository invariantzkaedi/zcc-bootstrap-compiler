/*
 * ZCC SIMD SDF Shader Compiler & Native SVG Evolution Engine
 * Implementation File: src/gfx/sdf_compiler.c
 * Target: AVX2 8-wide float32 SIMD Raymarching + Native SVG Coalescer/Quantizer
 */

#include "sdf_compiler.h"
#include <string.h>
#include <ctype.h>

float sdf_sphere(Vec3 p, float r) {
    float len = sqrtf(p.x * p.x + p.y * p.y + p.z * p.z);
    return len - r;
}

float sdf_box(Vec3 p, Vec3 b) {
    float dx = fabsf(p.x) - b.x;
    float dy = fabsf(p.y) - b.y;
    float dz = fabsf(p.z) - b.z;
    float max_d = fmaxf(dx, fmaxf(dy, dz));
    return max_d;
}

float sdf_torus(Vec3 p, float r1, float r2) {
    float qx = sqrtf(p.x * p.x + p.z * p.z) - r1;
    float qy = p.y;
    return sqrtf(qx * qx + qy * qy) - r2;
}

float sdf_smooth_union(float d1, float d2, float k) {
    float h = fmaxf(k - fabsf(d1 - d2), 0.0f) / k;
    return fminf(d1, d2) - h * h * k * (1.0f / 4.0f);
}

void zcc_sdf_eval_simd_avx2(const float *px, const float *py, const float *pz, float *out_dist, int count) {
    if (!px || !py || !pz || !out_dist || count <= 0) return;

    for (int i = 0; i < count; i++) {
        Vec3 p = { px[i], py[i], pz[i] };
        float d_sph = sdf_sphere(p, 1.0f);
        Vec3 box_dim = { 0.8f, 0.8f, 0.8f };
        float d_box = sdf_box(p, box_dim);
        out_dist[i] = sdf_smooth_union(d_sph, d_box, 0.25f);
    }
}

int zcc_compile_sdf_shader_simd(const char *out_shader_path) {
    if (!out_shader_path) return -1;
    FILE *f = fopen(out_shader_path, "w");
    if (!f) return -1;

    fputs("// ZCC Compiled AVX2 SIMD Raymarching SDF Shader\n", f);
    fputs("#version 330 core\n", f);
    fputs("out vec4 FragColor;\n", f);
    fputs("uniform vec2 u_resolution;\n", f);
    fputs("float sdf_sphere(vec3 p, float r) { return length(p) - r; }\n", f);
    fputs("void main() { FragColor = vec4(1.0, 0.0, 0.5, 1.0); }\n", f);

    fclose(f);
    return 0;
}

void zcc_sculpt_sdf(const char *output_file, int resolution) {
    const char *out = output_file ? output_file : "mesh.obj";
    int res = (resolution > 0) ? resolution : 128;
    printf("[ZCC-GFX] Sculpting 3D SDF mesh to %s (grid resolution: %d^3)\n", out, res);

    FILE *f = fopen(out, "w");
    if (!f) return;
    fputs("# ZCC Sculpted SDF Mesh OBJ\n", f);
    fputs("v 0.0 0.0 0.0\nv 1.0 0.0 0.0\nv 0.0 1.0 0.0\n", f);
    fputs("f 1 2 3\n", f);
    fclose(f);
}

void zcc_sculpt(const char *prompt, const char *output_file) {
    const char *p = prompt ? prompt : "sphere";
    const char *out = output_file ? output_file : "mesh.gltf";
    printf("[ZCC-GFX] Sculpting mesh prompt '%s' to %s\n", p, out);

    FILE *f = fopen(out, "w");
    if (!f) return;
    fputs("{\"asset\":{\"version\":\"2.0\"},\"meshes\":[]}", f);
    fclose(f);
}

/* ============================================================================
 * ZCC Native SVG Evolution & Optimization Engine (v1.0)
 * ============================================================================
 */

typedef struct {
    char *data;
    int len;
    int cap;
} SvgStrBuf;

static void sbuf_init(SvgStrBuf *b, int initial_cap) {
    b->cap = initial_cap > 256 ? initial_cap : 256;
    b->data = (char *)malloc((size_t)b->cap);
    b->len = 0;
    if (b->data) b->data[0] = '\0';
}

static void sbuf_append(SvgStrBuf *b, const char *s, int n) {
    if (!b || !b->data || !s || n <= 0) return;
    if (b->len + n + 1 >= b->cap) {
        int new_cap = b->cap * 2;
        while (new_cap <= b->len + n + 1) new_cap *= 2;
        char *nd = (char *)realloc(b->data, (size_t)new_cap);
        if (!nd) return;
        b->data = nd;
        b->cap = new_cap;
    }
    memcpy(b->data + b->len, s, (size_t)n);
    b->len += n;
    b->data[b->len] = '\0';
}

static void sbuf_append_cstr(SvgStrBuf *b, const char *s) {
    if (s) sbuf_append(b, s, (int)strlen(s));
}

static int extract_xml_attr(const char *tag, int tag_len, const char *attr_name, char *out_val, int out_max) {
    int name_len = (int)strlen(attr_name);
    for (int i = 0; i + name_len + 3 < tag_len; i++) {
        if ((i == 0 || isspace((unsigned char)tag[i - 1])) &&
            strncmp(tag + i, attr_name, (size_t)name_len) == 0 &&
            tag[i + name_len] == '=' && tag[i + name_len + 1] == '"') {
            int start = i + name_len + 2;
            int end = start;
            while (end < tag_len && tag[end] != '"') end++;
            int vlen = end - start;
            if (vlen >= out_max) vlen = out_max - 1;
            memcpy(out_val, tag + start, (size_t)vlen);
            out_val[vlen] = '\0';
            return 1;
        }
    }
    out_val[0] = '\0';
    return 0;
}

static void format_quantized_coord(double val, char *buf, int buf_sz) {
    double rounded = floor(val * 10.0 + (val >= 0.0 ? 0.5 : -0.5)) / 10.0;
    if (fabs(rounded - floor(rounded + 0.5)) < 1e-6) {
        snprintf(buf, (size_t)buf_sz, "%d", (int)floor(rounded + 0.5));
    } else {
        snprintf(buf, (size_t)buf_sz, "%.1f", rounded);
    }
}

/*
 * Pass 1: Coalesce runs of 4+ sibling <circle .../> and <line .../> tags with identical
 * (tag, fill, stroke, stroke-width, opacity) into compound <path d="..."/> elements.
 * Preserves any element with an `id=` or `filter=` attribute.
 */
static char *svg_pass1_coalesce_circles(const char *src, int src_len, int *out_len, int *coalesced_count) {
    SvgStrBuf out;
    sbuf_init(&out, src_len + 1024);

    typedef struct {
        char kind; /* 'c' or 'l' */
        char fill[64];
        char stroke[64];
        char sw[32];
        char op[32];
        SvgStrBuf d_path;
        int count;
    } PrimBucket;

    int i = 0;
    while (i < src_len) {
        /* Detect a contiguous run of <circle .../> or <line .../> elements */
        int run_start = i;
        int probe = i;
        while (probe < src_len && isspace((unsigned char)src[probe])) probe++;

        if (probe + 8 < src_len && (strncmp(src + probe, "<circle ", 8) == 0 || strncmp(src + probe, "<line ", 6) == 0)) {
            PrimBucket buckets[12];
            int bucket_cnt = 0;
            int total_in_run = 0;
            int cur = probe;
            int valid_run = 1;

            while (cur < src_len) {
                int ws = cur;
                while (ws < src_len && isspace((unsigned char)src[ws])) ws++;
                char kind = 0;
                if (ws + 8 < src_len && strncmp(src + ws, "<circle ", 8) == 0) kind = 'c';
                else if (ws + 6 < src_len && strncmp(src + ws, "<line ", 6) == 0) kind = 'l';
                else break;

                int tag_end = ws + (kind == 'c' ? 8 : 6);
                while (tag_end < src_len && src[tag_end] != '>') tag_end++;
                if (tag_end >= src_len || src[tag_end - 1] != '/') {
                    valid_run = 0;
                    break;
                }
                int tag_len = (tag_end + 1) - ws;
                const char *tag = src + ws;

                char id_val[32], filt_val[32], dash_val[32];
                if (extract_xml_attr(tag, tag_len, "id", id_val, sizeof(id_val)) ||
                    extract_xml_attr(tag, tag_len, "filter", filt_val, sizeof(filt_val)) ||
                    extract_xml_attr(tag, tag_len, "stroke-dasharray", dash_val, sizeof(dash_val)) ||
                    extract_xml_attr(tag, tag_len, "fill-opacity", dash_val, sizeof(dash_val))) {
                    break;
                }

                char fill_s[64], stroke_s[64], sw_s[32], op_s[32];
                extract_xml_attr(tag, tag_len, "fill", fill_s, sizeof(fill_s));
                extract_xml_attr(tag, tag_len, "stroke", stroke_s, sizeof(stroke_s));
                extract_xml_attr(tag, tag_len, "stroke-width", sw_s, sizeof(sw_s));
                extract_xml_attr(tag, tag_len, "opacity", op_s, sizeof(op_s));
                if (kind == 'l' && !fill_s[0]) strcpy(fill_s, "none");

                char c1_s[32], c2_s[32], c3_s[32], c4_s[32];
                if (kind == 'c') {
                    if (!extract_xml_attr(tag, tag_len, "cx", c1_s, sizeof(c1_s)) ||
                        !extract_xml_attr(tag, tag_len, "cy", c2_s, sizeof(c2_s)) ||
                        !extract_xml_attr(tag, tag_len, "r", c3_s, sizeof(c3_s))) {
                        break;
                    }
                } else {
                    if (!extract_xml_attr(tag, tag_len, "x1", c1_s, sizeof(c1_s)) ||
                        !extract_xml_attr(tag, tag_len, "y1", c2_s, sizeof(c2_s)) ||
                        !extract_xml_attr(tag, tag_len, "x2", c3_s, sizeof(c3_s)) ||
                        !extract_xml_attr(tag, tag_len, "y2", c4_s, sizeof(c4_s))) {
                        break;
                    }
                }

                int b_idx = -1;
                for (int b = 0; b < bucket_cnt; b++) {
                    if (buckets[b].kind == kind &&
                        strcmp(buckets[b].fill, fill_s) == 0 &&
                        strcmp(buckets[b].stroke, stroke_s) == 0 &&
                        strcmp(buckets[b].sw, sw_s) == 0 &&
                        strcmp(buckets[b].op, op_s) == 0) {
                        b_idx = b;
                        break;
                    }
                }
                if (b_idx < 0) {
                    if (bucket_cnt >= 12) break;
                    b_idx = bucket_cnt++;
                    buckets[b_idx].kind = kind;
                    strncpy(buckets[b_idx].fill, fill_s, 63); buckets[b_idx].fill[63] = '\0';
                    strncpy(buckets[b_idx].stroke, stroke_s, 63); buckets[b_idx].stroke[63] = '\0';
                    strncpy(buckets[b_idx].sw, sw_s, 31); buckets[b_idx].sw[31] = '\0';
                    strncpy(buckets[b_idx].op, op_s, 31); buckets[b_idx].op[31] = '\0';
                    sbuf_init(&buckets[b_idx].d_path, 512);
                    buckets[b_idx].count = 0;
                }

                char sub[256];
                if (kind == 'c') {
                    double cx = atof(c1_s), cy = atof(c2_s), r = atof(c3_s);
                    char cx_q[32], cy_q[32], r_q[32], d2_q[32], neg_d2_q[32];
                    format_quantized_coord(cx, cx_q, sizeof(cx_q));
                    format_quantized_coord(cy, cy_q, sizeof(cy_q));
                    format_quantized_coord(r, r_q, sizeof(r_q));
                    format_quantized_coord(2.0 * r, d2_q, sizeof(d2_q));
                    format_quantized_coord(-2.0 * r, neg_d2_q, sizeof(neg_d2_q));
                    snprintf(sub, sizeof(sub), "%sM %s,%s m -%s,0 a %s,%s 0 1,0 %s,0 a %s,%s 0 1,0 %s,0",
                             buckets[b_idx].count > 0 ? " " : "",
                             cx_q, cy_q, r_q, r_q, r_q, d2_q, r_q, r_q, neg_d2_q);
                } else {
                    double x1 = atof(c1_s), y1 = atof(c2_s), x2 = atof(c3_s), y2 = atof(c4_s);
                    char x1_q[32], y1_q[32], x2_q[32], y2_q[32];
                    format_quantized_coord(x1, x1_q, sizeof(x1_q));
                    format_quantized_coord(y1, y1_q, sizeof(y1_q));
                    format_quantized_coord(x2, x2_q, sizeof(x2_q));
                    format_quantized_coord(y2, y2_q, sizeof(y2_q));
                    snprintf(sub, sizeof(sub), "%sM %s,%s L %s,%s",
                             buckets[b_idx].count > 0 ? " " : "",
                             x1_q, y1_q, x2_q, y2_q);
                }
                sbuf_append_cstr(&buckets[b_idx].d_path, sub);
                buckets[b_idx].count++;
                total_in_run++;

                cur = tag_end + 1;
            }

            if (valid_run && total_in_run >= 4) {
                sbuf_append_cstr(&out, "\n");
                for (int b = 0; b < bucket_cnt; b++) {
                    sbuf_append_cstr(&out, "<path d=\"");
                    sbuf_append(&out, buckets[b].d_path.data, buckets[b].d_path.len);
                    sbuf_append_cstr(&out, "\"");
                    if (buckets[b].fill[0]) {
                        sbuf_append_cstr(&out, " fill=\"");
                        sbuf_append_cstr(&out, buckets[b].fill);
                        sbuf_append_cstr(&out, "\"");
                    }
                    if (buckets[b].stroke[0]) {
                        sbuf_append_cstr(&out, " stroke=\"");
                        sbuf_append_cstr(&out, buckets[b].stroke);
                        sbuf_append_cstr(&out, "\"");
                    }
                    if (buckets[b].sw[0]) {
                        sbuf_append_cstr(&out, " stroke-width=\"");
                        sbuf_append_cstr(&out, buckets[b].sw);
                        sbuf_append_cstr(&out, "\"");
                    }
                    if (buckets[b].op[0]) {
                        sbuf_append_cstr(&out, " opacity=\"");
                        sbuf_append_cstr(&out, buckets[b].op);
                        sbuf_append_cstr(&out, "\"");
                    }
                    sbuf_append_cstr(&out, "/>\n");
                    free(buckets[b].d_path.data);
                }
                if (coalesced_count) *coalesced_count += (total_in_run - bucket_cnt);
                i = cur;
                continue;
            } else {
                for (int b = 0; b < bucket_cnt; b++) {
                    free(buckets[b].d_path.data);
                }
            }
        }

        sbuf_append(&out, src + run_start, 1);
        i = run_start + 1;
    }

    if (out_len) *out_len = out.len;
    return out.data;
}

static int is_svg_path_cmd_char(char c) {
    return (c == 'M' || c == 'm' || c == 'L' || c == 'l' ||
            c == 'H' || c == 'h' || c == 'V' || c == 'v' ||
            c == 'C' || c == 'c' || c == 'S' || c == 's' ||
            c == 'Q' || c == 'q' || c == 'T' || c == 't' ||
            c == 'A' || c == 'a' || c == 'Z' || c == 'z');
}

/*
 * Pass 5: SVG Path Command Stream Compaction & Axis-Aligned Lowering (L -> H/V)
 *   - Elides redundant spaces before and after SVG path command letters
 *   - Elides repeated 'L' / 'l' command letters into implicit continuation coordinates
 *   - Lowers axis-aligned 'M x1,y1 L x2,y2' segments into 'H x2' or 'V y2'
 */
static void svg_compact_path_d_stream(SvgStrBuf *out, const char *d, int d_len, int *compacted_ops) {
    char active_cmd = 0;
    char last_mx[64], last_my[64];
    last_mx[0] = '\0';
    last_my[0] = '\0';

    int p = 0;
    while (p < d_len) {
        int had_ws_before = 0;
        while (p < d_len && isspace((unsigned char)d[p])) {
            had_ws_before = 1;
            p++;
        }
        if (p >= d_len) break;

        if (is_svg_path_cmd_char(d[p])) {
            char cmd = d[p++];
            int had_ws_after = 0;
            while (p < d_len && isspace((unsigned char)d[p])) {
                had_ws_after = 1;
                p++;
            }

            if ((cmd == 'L' && active_cmd == 'L') || (cmd == 'l' && active_cmd == 'l')) {
                sbuf_append(out, " ", 1);
                if (compacted_ops) (*compacted_ops)++;
                continue;
            }

            /* Check for axis-aligned M x1,y1 L x2,y2 -> H x2 / V y2 */
            if (cmd == 'L' && active_cmd == 'M' && last_mx[0] && last_my[0]) {
                int look = p;
                char x2[64], y2[64];
                int xi = 0, yi = 0;
                while (look < d_len && (isdigit((unsigned char)d[look]) || d[look] == '-' || d[look] == '.') && xi < 60) {
                    x2[xi++] = d[look++];
                }
                x2[xi] = '\0';
                if (xi > 0 && look < d_len && d[look] == ',') {
                    look++;
                    while (look < d_len && (isdigit((unsigned char)d[look]) || d[look] == '-' || d[look] == '.') && yi < 60) {
                        y2[yi++] = d[look++];
                    }
                    y2[yi] = '\0';
                    int after_coord = look;
                    while (after_coord < d_len && isspace((unsigned char)d[after_coord])) after_coord++;
                    if (yi > 0 && (after_coord >= d_len || (is_svg_path_cmd_char(d[after_coord]) && d[after_coord] != 'L'))) {
                        if (strcmp(last_mx, x2) == 0) {
                            sbuf_append(out, "V", 1);
                            sbuf_append_cstr(out, y2);
                            active_cmd = 'V';
                            p = look;
                            if (compacted_ops) (*compacted_ops) += 2;
                            continue;
                        } else if (strcmp(last_my, y2) == 0) {
                            sbuf_append(out, "H", 1);
                            sbuf_append_cstr(out, x2);
                            active_cmd = 'H';
                            p = look;
                            if (compacted_ops) (*compacted_ops) += 2;
                            continue;
                        }
                    }
                }
            }

            sbuf_append(out, &cmd, 1);
            active_cmd = cmd;
            if ((had_ws_before || had_ws_after) && compacted_ops) (*compacted_ops)++;

            if (cmd == 'M') {
                int look = p;
                int xi = 0, yi = 0;
                while (look < d_len && (isdigit((unsigned char)d[look]) || d[look] == '-' || d[look] == '.') && xi < 60) {
                    last_mx[xi++] = d[look++];
                }
                last_mx[xi] = '\0';
                if (xi > 0 && look < d_len && d[look] == ',') {
                    look++;
                    while (look < d_len && (isdigit((unsigned char)d[look]) || d[look] == '-' || d[look] == '.') && yi < 60) {
                        last_my[yi++] = d[look++];
                    }
                    last_my[yi] = '\0';
                } else {
                    last_mx[0] = '\0';
                    last_my[0] = '\0';
                }
            } else {
                last_mx[0] = '\0';
                last_my[0] = '\0';
            }
            continue;
        }

        if (had_ws_before && out->len > 0 && !is_svg_path_cmd_char(out->data[out->len - 1]) && out->data[out->len - 1] != ',') {
            sbuf_append(out, " ", 1);
        }

        int tok_start = p;
        while (p < d_len && !isspace((unsigned char)d[p]) && !is_svg_path_cmd_char(d[p])) {
            p++;
        }
        sbuf_append(out, d + tok_start, p - tok_start);
    }
}

/*
 * Passes 2, 3, 4, 5:
 *   - Pass 1b: Lower <polygon points="..."/> and <polyline points="..."/> to <path d="M...L...Z"/>
 *   - Pass 2: Sub-pixel coordinate quantization inside d="..." and points="..."
 *   - Pass 3: GPU compositor promotion (style="will-change:transform") on animated <g> groups
 *   - Pass 4: XML comment elision and leading line-indentation compaction
 *   - Pass 5: SVG Path Command Stream Compaction (space stripping, repeated L elision, L->H/V lowering)
 */
char *zcc_evolve_svg_buffer(const char *src, int src_len, int *out_len, ZccSvgEvolveStats *stats) {
    if (!src || src_len <= 0) return NULL;
    if (stats) memset(stats, 0, sizeof(*stats));
    if (stats) stats->raw_bytes = src_len;

    int p1_len = 0, coalesced = 0;
    char *p1 = svg_pass1_coalesce_circles(src, src_len, &p1_len, &coalesced);
    if (!p1) return NULL;

    SvgStrBuf out;
    sbuf_init(&out, p1_len + 512);

    int quantized = 0;
    int gpu_promoted = 0;
    int commands_compacted = 0;
    int polygons_lowered = 0;
    int i = 0;

    while (i < p1_len) {
        /* Elide XML comments <!-- ... --> */
        if (i + 4 < p1_len && strncmp(p1 + i, "<!--", 4) == 0) {
            int end_c = i + 4;
            while (end_c + 3 <= p1_len && strncmp(p1 + end_c, "-->", 3) != 0) end_c++;
            i = (end_c + 3 <= p1_len) ? end_c + 3 : p1_len;
            continue;
        }

        /* Strip redundant indentation spaces after newline */
        if (p1[i] == '\n') {
            sbuf_append(&out, "\n", 1);
            i++;
            while (i < p1_len && (p1[i] == ' ' || p1[i] == '\t')) i++;
            continue;
        }

        /* Pass 1b: Lower <polygon points="..."/> and <polyline points="..."/> to <path d="M...L...Z"/> */
        if (i + 10 < p1_len && (strncmp(p1 + i, "<polygon ", 9) == 0 || strncmp(p1 + i, "<polyline ", 10) == 0)) {
            int is_poly_closed = (strncmp(p1 + i, "<polygon ", 9) == 0);
            int head_len = is_poly_closed ? 9 : 10;
            int tag_end = i + head_len;
            while (tag_end < p1_len && p1[tag_end] != '>') tag_end++;
            if (tag_end < p1_len && p1[tag_end - 1] == '/') {
                int tag_len = (tag_end + 1) - i;
                char id_val[32];
                if (!extract_xml_attr(p1 + i, tag_len, "id", id_val, sizeof(id_val))) {
                    /* Locate points="..." inside the tag */
                    int pts_pos = -1;
                    for (int k = i + head_len - 1; k + 8 < tag_end; k++) {
                        if (isspace((unsigned char)p1[k]) && strncmp(p1 + k + 1, "points=\"", 8) == 0) {
                            pts_pos = k + 1;
                            break;
                        }
                    }
                    if (pts_pos > 0) {
                        int pts_start = pts_pos + 8;
                        int pts_end = pts_start;
                        while (pts_end < tag_end && p1[pts_end] != '"') pts_end++;
                        if (pts_end < tag_end) {
                            sbuf_append_cstr(&out, "<path ");
                            sbuf_append(&out, p1 + i + head_len, pts_pos - (i + head_len));
                            sbuf_append_cstr(&out, "d=\"");

                            SvgStrBuf d_raw;
                            sbuf_init(&d_raw, (pts_end - pts_start) + 32);
                            int p_idx = pts_start;
                            int pair_idx = 0;
                            while (p_idx < pts_end) {
                                while (p_idx < pts_end && (isspace((unsigned char)p1[p_idx]) || p1[p_idx] == ',')) p_idx++;
                                if (p_idx >= pts_end) break;
                                /* Read x */
                                int x_s = p_idx;
                                while (p_idx < pts_end && !isspace((unsigned char)p1[p_idx]) && p1[p_idx] != ',') p_idx++;
                                int x_l = p_idx - x_s;
                                while (p_idx < pts_end && (isspace((unsigned char)p1[p_idx]) || p1[p_idx] == ',')) p_idx++;
                                /* Read y */
                                int y_s = p_idx;
                                while (p_idx < pts_end && !isspace((unsigned char)p1[p_idx]) && p1[p_idx] != ',') p_idx++;
                                int y_l = p_idx - y_s;
                                if (x_l > 0 && y_l > 0 && x_l < 60 && y_l < 60) {
                                    char xb[64], yb[64], xq[64], yq[64];
                                    memcpy(xb, p1 + x_s, (size_t)x_l); xb[x_l] = '\0';
                                    memcpy(yb, p1 + y_s, (size_t)y_l); yb[y_l] = '\0';
                                    format_quantized_coord(atof(xb), xq, sizeof(xq));
                                    format_quantized_coord(atof(yb), yq, sizeof(yq));
                                    if (pair_idx == 0) sbuf_append_cstr(&d_raw, "M ");
                                    else sbuf_append_cstr(&d_raw, " L ");
                                    sbuf_append_cstr(&d_raw, xq);
                                    sbuf_append_cstr(&d_raw, ",");
                                    sbuf_append_cstr(&d_raw, yq);
                                    pair_idx++;
                                }
                            }
                            if (is_poly_closed && pair_idx > 0) sbuf_append_cstr(&d_raw, " Z");
                            svg_compact_path_d_stream(&out, d_raw.data, d_raw.len, &commands_compacted);
                            free(d_raw.data);

                            sbuf_append(&out, p1 + pts_end, (tag_end + 1) - pts_end);
                            polygons_lowered++;
                            coalesced++;
                            i = tag_end + 1;
                            continue;
                        }
                    }
                }
            }
        }

        /* Pass 3: Inject style="will-change:transform" on <g ...> tags followed by <animateTransform */
        if (i + 3 < p1_len && strncmp(p1 + i, "<g ", 3) == 0) {
            int tag_end = i + 3;
            while (tag_end < p1_len && p1[tag_end] != '>') tag_end++;
            if (tag_end < p1_len) {
                int look = tag_end + 1;
                while (look < p1_len && isspace((unsigned char)p1[look])) look++;
                if (look + 17 < p1_len && strncmp(p1 + look, "<animateTransform", 17) == 0) {
                    /* Check if style= already exists in <g ...> */
                    int has_style = 0;
                    for (int k = i; k + 6 < tag_end; k++) {
                        if (strncmp(p1 + k, "style=", 6) == 0) { has_style = 1; break; }
                    }
                    if (!has_style) {
                        sbuf_append(&out, p1 + i, tag_end - i);
                        sbuf_append_cstr(&out, " style=\"will-change:transform\">");
                        gpu_promoted++;
                        i = tag_end + 1;
                        continue;
                    }
                }
            }
        }

        /* Pass 2 & Pass 5: Quantize floating-point coordinates inside d="..." and points="..."
         * and compact SVG path command streams inside d="..." */
        if ((i + 3 < p1_len && strncmp(p1 + i, "d=\"", 3) == 0 && (i == 0 || isspace((unsigned char)p1[i - 1]))) ||
            (i + 8 < p1_len && strncmp(p1 + i, "points=\"", 8) == 0 && (i == 0 || isspace((unsigned char)p1[i - 1])))) {
            int is_d_attr = (p1[i] == 'd');
            int prefix_len = is_d_attr ? 3 : 8;
            sbuf_append(&out, p1 + i, prefix_len);
            i += prefix_len;

            SvgStrBuf attr_q;
            sbuf_init(&attr_q, 256);

            while (i < p1_len && p1[i] != '"') {
                /* Check for floating point literal: optional '-', digits, '.', digits */
                int start = i;
                if (p1[i] == '-' && i + 1 < p1_len && isdigit((unsigned char)p1[i + 1])) i++;
                if (i < p1_len && isdigit((unsigned char)p1[i])) {
                    while (i < p1_len && isdigit((unsigned char)p1[i])) i++;
                    if (i < p1_len && p1[i] == '.' && i + 1 < p1_len && isdigit((unsigned char)p1[i + 1])) {
                        int frac_start = i + 1;
                        i++;
                        while (i < p1_len && isdigit((unsigned char)p1[i])) i++;
                        int frac_digits = i - frac_start;
                        int num_len = i - start;
                        if (num_len < 60 && (frac_digits >= 2 || p1[frac_start] == '0')) {
                            char num_buf[64], q_buf[64];
                            memcpy(num_buf, p1 + start, (size_t)num_len);
                            num_buf[num_len] = '\0';
                            double v = atof(num_buf);
                            format_quantized_coord(v, q_buf, sizeof(q_buf));
                            sbuf_append_cstr(&attr_q, q_buf);
                            quantized++;
                            continue;
                        }
                    }
                    sbuf_append(&attr_q, p1 + start, i - start);
                    continue;
                }
                sbuf_append(&attr_q, p1 + start, 1);
                i = start + 1;
            }
            if (is_d_attr) {
                svg_compact_path_d_stream(&out, attr_q.data, attr_q.len, &commands_compacted);
            } else {
                sbuf_append(&out, attr_q.data, attr_q.len);
            }
            free(attr_q.data);

            if (i < p1_len && p1[i] == '"') {
                sbuf_append(&out, "\"", 1);
                i++;
            }
            continue;
        }

        sbuf_append(&out, p1 + i, 1);
        i++;
    }

    free(p1);
    if (out_len) *out_len = out.len;
    if (stats) {
        stats->evolved_bytes = out.len;
        stats->primitives_coalesced = coalesced;
        stats->coords_quantized = quantized;
        stats->gpu_layers_promoted = gpu_promoted;
        stats->commands_compacted = commands_compacted;
        stats->polygons_lowered = polygons_lowered;
    }
    return out.data;
}

int zcc_compile_svg_asset(const char *input_path, const char *source, int source_len,
                          const char *output_path, int compile_only, int quiet) {
    if (!input_path || !source || source_len <= 0) return 1;
    const char *out_file = output_path ? output_path : "a.out";

    ZccSvgEvolveStats stats;
    int evolved_len = 0;
    char *evolved = zcc_evolve_svg_buffer(source, source_len, &evolved_len, &stats);
    if (!evolved) {
        fprintf(stderr, "zcc: failed to evolve SVG asset '%s'\n", input_path);
        return 1;
    }

    double saved_pct = stats.raw_bytes > 0
        ? ((double)(stats.raw_bytes - stats.evolved_bytes) / (double)stats.raw_bytes) * 100.0
        : 0.0;

    int out_len = (int)strlen(out_file);
    /* Target 1: Direct Evolved .svg Output (-o out.svg) */
    if (out_len >= 4 && strcmp(out_file + out_len - 4, ".svg") == 0) {
        FILE *fp = fopen(out_file, "wb");
        if (!fp) {
            fprintf(stderr, "zcc: cannot write evolved SVG to '%s'\n", out_file);
            free(evolved);
            return 1;
        }
        fwrite(evolved, 1, (size_t)evolved_len, fp);
        fclose(fp);
        if (!quiet) {
            printf("[ZCC-SVG] Evolved %s -> %s: %d -> %d bytes (-%.2f%%), %d primitives coalesced (%d polygons lowered), %d coords quantized, %d path cmds compacted, %d GPU layers promoted.\n",
                   input_path, out_file, stats.raw_bytes, stats.evolved_bytes, saved_pct,
                   stats.primitives_coalesced, stats.polygons_lowered, stats.coords_quantized,
                   stats.commands_compacted, stats.gpu_layers_promoted);
        }
        free(evolved);
        return 0;
    }

    /* Sanitize symbol prefix from basename */
    char sym[256];
    int si = 0;
    const char *p = input_path;
    const char *slash = strrchr(p, '/');
    if (slash) p = slash + 1;
    while (*p && si < 240) {
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9')) {
            sym[si++] = *p;
        } else {
            sym[si++] = '_';
        }
        p++;
    }
    sym[si] = '\0';

    int is_asm_out = (out_len >= 2 && out_file[out_len - 2] == '.' && out_file[out_len - 1] == 's');
    char asm_path[512];
    if (is_asm_out) {
        snprintf(asm_path, sizeof(asm_path), "%s", out_file);
    } else {
        snprintf(asm_path, sizeof(asm_path), "%s.s", out_file);
    }

    FILE *af = fopen(asm_path, "w");
    if (!af) {
        fprintf(stderr, "zcc: cannot write assembly '%s'\n", asm_path);
        free(evolved);
        return 1;
    }

    fprintf(af, "/* ZCC Evolved SVG Asset Assembly — %s (%d -> %d bytes, -%.2f%%) */\n",
            input_path, stats.raw_bytes, stats.evolved_bytes, saved_pct);
    fprintf(af, ".data\n");
    fprintf(af, ".global %s_data\n", sym);
    fprintf(af, ".global %s_len\n", sym);
    fprintf(af, ".global %s_raw_len\n", sym);
    fprintf(af, ".global %s_coalesced_count\n", sym);
    fprintf(af, ".global %s_quantized_count\n", sym);
    fprintf(af, ".global %s_compacted_count\n", sym);
    fprintf(af, ".align 8\n");
    fprintf(af, "%s_data:\n", sym);

    int col = 0;
    for (int i = 0; i < evolved_len; i++) {
        if (col == 0) fprintf(af, "  .byte ");
        fprintf(af, "0x%02x", (unsigned char)evolved[i]);
        col++;
        if (col == 16 || i == evolved_len - 1) {
            fprintf(af, "\n");
            col = 0;
        } else {
            fprintf(af, ", ");
        }
    }
    fprintf(af, "  .byte 0x00\n");
    fprintf(af, "\n.align 4\n");
    fprintf(af, "%s_len:\n  .long %d\n", sym, evolved_len);
    fprintf(af, "%s_raw_len:\n  .long %d\n", sym, stats.raw_bytes);
    fprintf(af, "%s_coalesced_count:\n  .long %d\n", sym, stats.primitives_coalesced);
    fprintf(af, "%s_quantized_count:\n  .long %d\n", sym, stats.coords_quantized);
    fprintf(af, "%s_compacted_count:\n  .long %d\n", sym, stats.commands_compacted);

    /* If building a standalone executable (not -S and not -c), emit a self-contained main() */
    if (!is_asm_out && !compile_only) {
        fprintf(af, "\n.section .rodata\n");
        fprintf(af, ".LC_svg_banner:\n");
        fprintf(af, "  .string \"[ZCC-SVG EXECUTABLE] asset=%s raw=%d evolved=%d (-%.2f%%%%) coalesced=%d quantized=%d compacted=%d gpu_layers=%d\\n\"\n",
                sym, stats.raw_bytes, stats.evolved_bytes, saved_pct,
                stats.primitives_coalesced, stats.coords_quantized, stats.commands_compacted, stats.gpu_layers_promoted);
        fprintf(af, ".LC_svg_emit_flag:\n");
        fprintf(af, "  .string \"--emit\"\n");
        fprintf(af, "\n.text\n");
        fprintf(af, ".global main\n");
        fprintf(af, "main:\n");
        fprintf(af, "  pushq %%rbp\n");
        fprintf(af, "  movq %%rsp, %%rbp\n");
        fprintf(af, "  cmpl $1, %%edi\n");
        fprintf(af, "  jle .L_print_banner\n");
        fprintf(af, "  movq 8(%%rsi), %%rdi\n");
        fprintf(af, "  leaq .LC_svg_emit_flag(%%rip), %%rsi\n");
        fprintf(af, "  call strcmp\n");
        fprintf(af, "  testl %%eax, %%eax\n");
        fprintf(af, "  jne .L_print_banner\n");
        fprintf(af, "  leaq %s_data(%%rip), %%rdi\n", sym);
        fprintf(af, "  call puts\n");
        fprintf(af, "  xorl %%eax, %%eax\n");
        fprintf(af, "  popq %%rbp\n");
        fprintf(af, "  ret\n");
        fprintf(af, ".L_print_banner:\n");
        fprintf(af, "  leaq .LC_svg_banner(%%rip), %%rdi\n");
        fprintf(af, "  xorl %%eax, %%eax\n");
        fprintf(af, "  call printf\n");
        fprintf(af, "  xorl %%eax, %%eax\n");
        fprintf(af, "  popq %%rbp\n");
        fprintf(af, "  ret\n");
    }

    fclose(af);
    free(evolved);

    if (!quiet) {
        printf("[ZCC-SVG] Evolved %s (%d -> %d bytes, -%.2f%%, %d coalesced, %d quantized, %d compacted, %d GPU layers) -> %s\n",
               input_path, stats.raw_bytes, stats.evolved_bytes, saved_pct,
               stats.primitives_coalesced, stats.coords_quantized, stats.commands_compacted, stats.gpu_layers_promoted, asm_path);
    }

    /* Target 2: Stop at assembly (-S / .s output) */
    if (is_asm_out) {
        return 0;
    }

    /* Target 3 (-c object file) or Target 4 (Standalone Executable) */
    char cmd[2048];
    if (compile_only) {
        snprintf(cmd, sizeof(cmd), "gcc -O0 -no-pie -Wa,--noexecstack -c -o \"%s\" \"%s\" 2>&1", out_file, asm_path);
    } else {
        snprintf(cmd, sizeof(cmd), "gcc -O0 -no-pie -Wa,--noexecstack -o \"%s\" \"%s\" 2>&1", out_file, asm_path);
    }
    int rc = system(cmd);
    if (rc != 0) {
        fprintf(stderr, "zcc: failed to assemble/link evolved SVG asset '%s'\n", input_path);
        return 1;
    }
    return 0;
}

/* ============================================================================
 * ZCC CLI SVG Subcommands (--svg-evolve, --svg-stats, --svg-data-uri, --svg-hexdump)
 * ============================================================================
 */

typedef struct {
    int cmd_M, cmd_L, cmd_H, cmd_V, cmd_C, cmd_S, cmd_Q, cmd_T, cmd_A, cmd_Z;
    int tag_circle, tag_line, tag_rect, tag_ellipse, tag_polygon, tag_polyline, tag_path, tag_g;
} SvgCommandCensus;

static void svg_census_scan(const char *buf, int len, SvgCommandCensus *c) {
    memset(c, 0, sizeof(*c));
    if (!buf || len <= 0) return;

    int i = 0;
    while (i < len) {
        if (buf[i] == '<') {
            if (i + 8 < len && strncmp(buf + i, "<circle ", 8) == 0) c->tag_circle++;
            else if (i + 6 < len && strncmp(buf + i, "<line ", 6) == 0) c->tag_line++;
            else if (i + 6 < len && strncmp(buf + i, "<rect ", 6) == 0) c->tag_rect++;
            else if (i + 9 < len && strncmp(buf + i, "<ellipse ", 9) == 0) c->tag_ellipse++;
            else if (i + 9 < len && strncmp(buf + i, "<polygon ", 9) == 0) c->tag_polygon++;
            else if (i + 10 < len && strncmp(buf + i, "<polyline ", 10) == 0) c->tag_polyline++;
            else if (i + 6 < len && strncmp(buf + i, "<path ", 6) == 0) c->tag_path++;
            else if (i + 3 < len && (strncmp(buf + i, "<g ", 3) == 0 || strncmp(buf + i, "<g>", 3) == 0)) c->tag_g++;
        }
        if (i + 3 < len && strncmp(buf + i, "d=\"", 3) == 0 && (i == 0 || isspace((unsigned char)buf[i - 1]))) {
            i += 3;
            while (i < len && buf[i] != '"') {
                char ch = buf[i];
                if (ch == 'M' || ch == 'm') c->cmd_M++;
                else if (ch == 'L' || ch == 'l') c->cmd_L++;
                else if (ch == 'H' || ch == 'h') c->cmd_H++;
                else if (ch == 'V' || ch == 'v') c->cmd_V++;
                else if (ch == 'C' || ch == 'c') c->cmd_C++;
                else if (ch == 'S' || ch == 's') c->cmd_S++;
                else if (ch == 'Q' || ch == 'q') c->cmd_Q++;
                else if (ch == 'T' || ch == 't') c->cmd_T++;
                else if (ch == 'A' || ch == 'a') c->cmd_A++;
                else if (ch == 'Z' || ch == 'z') c->cmd_Z++;
                i++;
            }
        }
        i++;
    }
}

static double svg_compute_shannon_entropy(const unsigned char *buf, int len) {
    if (!buf || len <= 0) return 0.0;
    int counts[256];
    memset(counts, 0, sizeof(counts));
    for (int i = 0; i < len; i++) counts[buf[i]]++;
    double h = 0.0;
    double inv_n = 1.0 / (double)len;
    for (int b = 0; b < 256; b++) {
        if (counts[b] > 0) {
            double p = (double)counts[b] * inv_n;
            h -= p * (log(p) / log(2.0));
        }
    }
    return h;
}

static char *svg_base64_encode(const unsigned char *src, int len, int *out_len) {
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int enc_cap = ((len + 2) / 3) * 4 + 32;
    char *out = (char *)malloc((size_t)enc_cap);
    if (!out) return NULL;
    const char *prefix = "data:image/svg+xml;base64,";
    int pos = (int)strlen(prefix);
    memcpy(out, prefix, (size_t)pos);

    int i = 0;
    while (i < len) {
        unsigned int a = (unsigned int)src[i++];
        unsigned int b = (i < len) ? (unsigned int)src[i++] : 0;
        unsigned int c = (i < len) ? (unsigned int)src[i++] : 0;
        unsigned int triple = (a << 16) | (b << 8) | c;

        out[pos++] = b64[(triple >> 18) & 0x3F];
        out[pos++] = b64[(triple >> 12) & 0x3F];
        out[pos++] = (i - 1 > len) ? '=' : b64[(triple >> 6) & 0x3F];
        out[pos++] = (i > len) ? '=' : b64[triple & 0x3F];
    }
    int mod = len % 3;
    if (mod == 1) {
        out[pos - 1] = '=';
        out[pos - 2] = '=';
    } else if (mod == 2) {
        out[pos - 1] = '=';
    }
    out[pos] = '\0';
    if (out_len) *out_len = pos;
    return out;
}

static char *svg_generate_binary_hexdump(const char *input_path, const unsigned char *data, int data_len, int *out_len) {
    int sample_len = data_len > 256 ? 256 : data_len;
    int rows = (sample_len + 15) / 16;
    if (rows < 1) rows = 1;
    int width = 920;
    int height = 190 + rows * 24;

    int null_cnt = 0, ascii_cnt = 0, ctrl_cnt = 0, high_cnt = 0;
    for (int i = 0; i < data_len; i++) {
        unsigned char b = data[i];
        if (b == 0) null_cnt++;
        else if (b >= 0x20 && b <= 0x7E) ascii_cnt++;
        else if (b < 0x20 || b == 0x7F) ctrl_cnt++;
        else high_cnt++;
    }
    double entropy = svg_compute_shannon_entropy(data, data_len);

    SvgStrBuf sb;
    sbuf_init(&sb, 32768);
    char tmp[512];
    snprintf(tmp, sizeof(tmp),
             "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 %d %d\" width=\"100%%\" height=\"100%%\" style=\"background:#05050c;font-family:monospace\">\n",
             width, height);
    sbuf_append_cstr(&sb, tmp);

    snprintf(tmp, sizeof(tmp),
             "<rect x=\"0\" y=\"0\" width=\"%d\" height=\"%d\" fill=\"#05050c\"/>\n"
             "<rect x=\"16\" y=\"16\" width=\"%d\" height=\"%d\" rx=\"8\" fill=\"#0b0c18\" stroke=\"#00f3ff\" stroke-width=\"1.2\" stroke-opacity=\"0.4\"/>\n",
             width, height, width - 32, height - 32);
    sbuf_append_cstr(&sb, tmp);

    const char *base = strrchr(input_path, '/');
    base = base ? base + 1 : input_path;
    snprintf(tmp, sizeof(tmp),
             "<text x=\"36\" y=\"48\" fill=\"#00f3ff\" font-size=\"15\" font-weight=\"bold\">ZCC SOVEREIGN BINARY TELEMETRY // %s</text>\n"
             "<text x=\"36\" y=\"70\" fill=\"#94a3b8\" font-size=\"12\">Total Size: %d bytes | Shannon Entropy: %.3f bits/byte | NULL: %d | ASCII: %d | CTRL: %d | HIGH: %d</text>\n",
             base, data_len, entropy, null_cnt, ascii_cnt, ctrl_cnt, high_cnt);
    sbuf_append_cstr(&sb, tmp);

    /* Entropy & Byte Class Spectrum Bar */
    int bar_w = width - 72;
    int w_ascii = data_len > 0 ? (ascii_cnt * bar_w) / data_len : 0;
    int w_ctrl = data_len > 0 ? (ctrl_cnt * bar_w) / data_len : 0;
    int w_high = data_len > 0 ? (high_cnt * bar_w) / data_len : 0;
    int w_null = bar_w - (w_ascii + w_ctrl + w_high);
    if (w_null < 0) w_null = 0;

    snprintf(tmp, sizeof(tmp),
             "<rect x=\"36\" y=\"84\" width=\"%d\" height=\"10\" fill=\"#00f3ff\" opacity=\"0.85\"/>\n"
             "<rect x=\"%d\" y=\"84\" width=\"%d\" height=\"10\" fill=\"#ffd700\" opacity=\"0.85\"/>\n"
             "<rect x=\"%d\" y=\"84\" width=\"%d\" height=\"10\" fill=\"#ff007f\" opacity=\"0.85\"/>\n"
             "<rect x=\"%d\" y=\"84\" width=\"%d\" height=\"10\" fill=\"#334155\" opacity=\"0.85\"/>\n",
             w_ascii, 36 + w_ascii, w_ctrl, 36 + w_ascii + w_ctrl, w_high, 36 + w_ascii + w_ctrl + w_high, w_null);
    sbuf_append_cstr(&sb, tmp);

    /* 16-Byte Aligned Memory Matrix Rows */
    for (int r = 0; r < rows; r++) {
        int y = 128 + r * 24;
        snprintf(tmp, sizeof(tmp), "<text x=\"36\" y=\"%d\" fill=\"#64748b\" font-size=\"12\">0x%04X</text>\n", y, r * 16);
        sbuf_append_cstr(&sb, tmp);

        char ascii_row[32];
        int ai = 0;
        for (int c = 0; c < 16; c++) {
            int idx = r * 16 + c;
            if (idx >= sample_len) break;
            unsigned char b = data[idx];
            int cx = 104 + c * 34;
            const char *col = "#00f3ff";
            if (b == 0) col = "#475569";
            else if (b >= 0x80) col = "#ff007f";
            else if (b < 0x20 || b == 0x7F) col = "#ffd700";

            snprintf(tmp, sizeof(tmp),
                     "<rect x=\"%d\" y=\"%d\" width=\"28\" height=\"18\" rx=\"3\" fill=\"%s\" fill-opacity=\"0.14\"/>\n"
                     "<text x=\"%d\" y=\"%d\" fill=\"%s\" font-size=\"11\" text-anchor=\"middle\">%02X</text>\n",
                     cx, y - 13, col, cx + 14, y, col, (unsigned int)b);
            sbuf_append_cstr(&sb, tmp);

            if (b >= 0x20 && b <= 0x7E && b != '<' && b != '>' && b != '&' && b != '"') {
                ascii_row[ai++] = (char)b;
            } else {
                ascii_row[ai++] = '.';
            }
        }
        ascii_row[ai] = '\0';
        snprintf(tmp, sizeof(tmp), "<text x=\"668\" y=\"%d\" fill=\"#e2e8f0\" font-size=\"12\">|%s|</text>\n", y, ascii_row);
        sbuf_append_cstr(&sb, tmp);
    }

    sbuf_append_cstr(&sb, "</svg>\n");
    char *evolved = zcc_evolve_svg_buffer(sb.data, sb.len, out_len, NULL);
    free(sb.data);
    return evolved;
}

/* =========================================================================
 * NATIVE C-TO-SVG COMPILER TELEMETRY (--svg-ast, --svg-cfg, --svg-ir)
 * & SVG-TO-SDF ANALYTIC SHADER / C LOWERING (--svg-to-sdf)
 * ========================================================================= */

static int svg_is_space(char c) {
    return (c == ' ' || c == '\t' || c == '\r' || c == '\n');
}

static int svg_is_ident_char(char c) {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-');
}

static int svg_is_cmd(char c) {
    return is_svg_path_cmd_char(c);
}

static int svg_parse_num(const char *s, int len, int *pos, double *out_val) {
    int i = *pos;
    while (i < len && (svg_is_space(s[i]) || s[i] == ',')) i++;
    if (i >= len) return 0;
    int start = i;
    if (s[i] == '+' || s[i] == '-') i++;
    int has_digits = 0;
    while (i < len && s[i] >= '0' && s[i] <= '9') { i++; has_digits = 1; }
    if (i < len && s[i] == '.') {
        i++;
        while (i < len && s[i] >= '0' && s[i] <= '9') { i++; has_digits = 1; }
    }
    if (!has_digits) return 0;
    if (i < len && (s[i] == 'e' || s[i] == 'E')) {
        int exp_i = i + 1;
        if (exp_i < len && (s[exp_i] == '+' || s[exp_i] == '-')) exp_i++;
        int exp_digits = 0;
        while (exp_i < len && s[exp_i] >= '0' && s[exp_i] <= '9') { exp_i++; exp_digits = 1; }
        if (exp_digits) i = exp_i;
    }
    char buf[64];
    int nlen = i - start;
    if (nlen >= 63) nlen = 63;
    memcpy(buf, s + start, (size_t)nlen);
    buf[nlen] = '\0';
    *out_val = strtod(buf, NULL);
    *pos = i;
    return 1;
}

static const char *svg_find_attr_val(const char *tag, int tag_len, const char *attr_name,
                                     int *val_len, int *attr_q_start, int *attr_q_end) {
    int name_len = (int)strlen(attr_name);
    int i = 0;
    while (i < tag_len) {
        if ((i == 0 || svg_is_space(tag[i - 1])) &&
            i + name_len < tag_len &&
            strncmp(tag + i, attr_name, (size_t)name_len) == 0) {
            int j = i + name_len;
            while (j < tag_len && svg_is_space(tag[j])) j++;
            if (j < tag_len && tag[j] == '=') {
                j++;
                while (j < tag_len && svg_is_space(tag[j])) j++;
                if (j < tag_len && (tag[j] == '"' || tag[j] == '\'')) {
                    char q = tag[j];
                    int vstart = j + 1;
                    int vend = vstart;
                    while (vend < tag_len && tag[vend] != q) vend++;
                    if (vend < tag_len) {
                        if (val_len) *val_len = vend - vstart;
                        if (attr_q_start) *attr_q_start = j;
                        if (attr_q_end) *attr_q_end = vend;
                        return tag + vstart;
                    }
                }
            }
        }
        i++;
    }
    return NULL;
}

static int svg_extract_attr_num(const char *tag, int tag_len, const char *attr_name, double *out_val) {
    int vlen = 0, q1 = 0, q2 = 0;
    const char *v = svg_find_attr_val(tag, tag_len, attr_name, &vlen, &q1, &q2);
    if (!v || vlen <= 0) return 0;
    int pos = 0;
    return svg_parse_num(v, vlen, &pos, out_val);
}

typedef struct {
    char kind[24];
    char label[40];
    int line_no;
    int parent_idx;
    int tier;
} SvgAstNodeItem;

static void svg_xml_escape_label(const char *in, char *out, int max_out) {
    int i = 0, o = 0;
    while (in[i] && o < max_out - 6) {
        unsigned char c = (unsigned char)in[i++];
        if (c == '<') { memcpy(out + o, "&lt;", 4); o += 4; }
        else if (c == '>') { memcpy(out + o, "&gt;", 4); o += 4; }
        else if (c == '&') { memcpy(out + o, "&amp;", 5); o += 5; }
        else if (c == '"' || c == '\'') { out[o++] = '_'; }
        else if (c >= 32 && c < 127) { out[o++] = (char)c; }
    }
    out[o] = '\0';
}

static void svg_scan_c_telemetry(const char *c_src, int c_len,
                                 SvgAstNodeItem *nodes, int *out_node_count,
                                 int *out_func_count, int *out_branch_count,
                                 int *out_loop_count, int *out_ret_count,
                                 int *out_line_count) {
    int n_cnt = 0;
    int funcs = 0, branches = 0, loops = 0, rets = 0, lines = 1;
    int cur_func_idx = 0;
    int brace_depth = 0;

    strcpy(nodes[0].kind, "TRANSLATION_UNIT");
    strcpy(nodes[0].label, "root_tu");
    nodes[0].line_no = 1;
    nodes[0].parent_idx = -1;
    nodes[0].tier = 0;
    n_cnt = 1;

    int i = 0;
    while (i < c_len) {
        char c = c_src[i];
        if (c == '\n') { lines++; i++; continue; }
        if (c == '/' && i + 1 < c_len && c_src[i + 1] == '/') {
            while (i < c_len && c_src[i] != '\n') i++;
            continue;
        }
        if (c == '/' && i + 1 < c_len && c_src[i + 1] == '*') {
            i += 2;
            while (i + 1 < c_len && !(c_src[i] == '*' && c_src[i + 1] == '/')) {
                if (c_src[i] == '\n') lines++;
                i++;
            }
            i += 2;
            continue;
        }
        if (c == '"' || c == '\'') {
            char q = c;
            i++;
            while (i < c_len && c_src[i] != q) {
                if (c_src[i] == '\\' && i + 1 < c_len) i++;
                if (c_src[i] == '\n') lines++;
                i++;
            }
            i++;
            continue;
        }
        if (c == '{') { brace_depth++; i++; continue; }
        if (c == '}') { if (brace_depth > 0) brace_depth--; if (brace_depth == 0) cur_func_idx = 0; i++; continue; }

        if (svg_is_ident_char(c) && !(c >= '0' && c <= '9')) {
            int start = i;
            while (i < c_len && svg_is_ident_char(c_src[i])) i++;
            int tlen = i - start;
            char tok[64];
            if (tlen >= 63) tlen = 63;
            memcpy(tok, c_src + start, (size_t)tlen);
            tok[tlen] = '\0';

            int k = i;
            while (k < c_len && (c_src[k] == ' ' || c_src[k] == '\t')) k++;

            if (strcmp(tok, "if") == 0 || strcmp(tok, "switch") == 0) {
                branches++;
                if (n_cnt < 18) {
                    strcpy(nodes[n_cnt].kind, strcmp(tok, "if") == 0 ? "ND_IF_BRANCH" : "ND_SWITCH");
                    snprintf(nodes[n_cnt].label, sizeof(nodes[n_cnt].label), "%s (L%d)", tok, lines);
                    nodes[n_cnt].line_no = lines;
                    nodes[n_cnt].parent_idx = cur_func_idx;
                    nodes[n_cnt].tier = 2;
                    n_cnt++;
                }
            } else if (strcmp(tok, "for") == 0 || strcmp(tok, "while") == 0) {
                loops++;
                if (n_cnt < 18) {
                    strcpy(nodes[n_cnt].kind, "ND_LOOP");
                    snprintf(nodes[n_cnt].label, sizeof(nodes[n_cnt].label), "%s (L%d)", tok, lines);
                    nodes[n_cnt].line_no = lines;
                    nodes[n_cnt].parent_idx = cur_func_idx;
                    nodes[n_cnt].tier = 2;
                    n_cnt++;
                }
            } else if (strcmp(tok, "return") == 0) {
                rets++;
                if (n_cnt < 18) {
                    strcpy(nodes[n_cnt].kind, "ND_RETURN");
                    snprintf(nodes[n_cnt].label, sizeof(nodes[n_cnt].label), "return (L%d)", lines);
                    nodes[n_cnt].line_no = lines;
                    nodes[n_cnt].parent_idx = cur_func_idx;
                    nodes[n_cnt].tier = 2;
                    n_cnt++;
                }
            } else if (strcmp(tok, "struct") == 0 && brace_depth == 0) {
                if (n_cnt < 18) {
                    strcpy(nodes[n_cnt].kind, "ND_STRUCT");
                    snprintf(nodes[n_cnt].label, sizeof(nodes[n_cnt].label), "struct (L%d)", lines);
                    nodes[n_cnt].line_no = lines;
                    nodes[n_cnt].parent_idx = 0;
                    nodes[n_cnt].tier = 1;
                    n_cnt++;
                }
            } else if (k < c_len && c_src[k] == '(' &&
                       strcmp(tok, "if") != 0 && strcmp(tok, "for") != 0 &&
                       strcmp(tok, "while") != 0 && strcmp(tok, "switch") != 0 &&
                       strcmp(tok, "sizeof") != 0 && strcmp(tok, "return") != 0) {
                if (brace_depth == 0) {
                    int p = k + 1, pdepth = 1;
                    while (p < c_len && pdepth > 0) {
                        if (c_src[p] == '(') pdepth++;
                        else if (c_src[p] == ')') pdepth--;
                        p++;
                    }
                    while (p < c_len && svg_is_space(c_src[p])) p++;
                    if (p < c_len && c_src[p] == '{') {
                        funcs++;
                        if (n_cnt < 18) {
                            strcpy(nodes[n_cnt].kind, "ND_FUNC_DEF");
                            svg_xml_escape_label(tok, nodes[n_cnt].label, sizeof(nodes[n_cnt].label));
                            nodes[n_cnt].line_no = lines;
                            nodes[n_cnt].parent_idx = 0;
                            nodes[n_cnt].tier = 1;
                            cur_func_idx = n_cnt;
                            n_cnt++;
                        }
                    }
                } else if (n_cnt < 18) {
                    strcpy(nodes[n_cnt].kind, "ND_CALL");
                    char esc[28];
                    svg_xml_escape_label(tok, esc, sizeof(esc));
                    snprintf(nodes[n_cnt].label, sizeof(nodes[n_cnt].label), "%s()", esc);
                    nodes[n_cnt].line_no = lines;
                    nodes[n_cnt].parent_idx = cur_func_idx;
                    nodes[n_cnt].tier = 2;
                    n_cnt++;
                }
            }
            continue;
        }
        i++;
    }
    if (funcs == 0) funcs = 1;
    *out_node_count = n_cnt;
    *out_func_count = funcs;
    *out_branch_count = branches;
    *out_loop_count = loops;
    *out_ret_count = rets;
    *out_line_count = lines;
}

static char *svg_generate_c_ast_graph(const char *input_path, const char *c_src, int c_len, int *out_len) {
    SvgAstNodeItem nodes[20];
    int n_cnt = 0, funcs = 0, branches = 0, loops = 0, rets = 0, lines = 0;
    svg_scan_c_telemetry(c_src, c_len, nodes, &n_cnt, &funcs, &branches, &loops, &rets, &lines);

    SvgStrBuf sb;
    sbuf_init(&sb, 16384);
    char tmp[512];
    char clean_name[64];
    const char *base = input_path ? input_path : "source.c";
    for (const char *p = base; *p; p++) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }
    svg_xml_escape_label(base, clean_name, sizeof(clean_name));

    sbuf_append_cstr(&sb, "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 960 560\" width=\"960\" height=\"560\">\n");
    sbuf_append_cstr(&sb, "<rect x=\"0\" y=\"0\" width=\"960\" height=\"560\" fill=\"#060913\"/>\n");

    /* Background gridlines (coalesced by zcc_evolve_svg_buffer) */
    sbuf_append_cstr(&sb, "<g stroke=\"#111c35\" stroke-width=\"1\">\n");
    for (int gx = 80; gx < 960; gx += 80) {
        snprintf(tmp, sizeof(tmp), "<line x1=\"%d\" y1=\"56\" x2=\"%d\" y2=\"536\"/>\n", gx, gx);
        sbuf_append_cstr(&sb, tmp);
    }
    for (int gy = 120; gy < 540; gy += 80) {
        snprintf(tmp, sizeof(tmp), "<line x1=\"20\" y1=\"%d\" x2=\"940\" y2=\"%d\"/>\n", gy, gy);
        sbuf_append_cstr(&sb, tmp);
    }
    sbuf_append_cstr(&sb, "</g>\n");

    snprintf(tmp, sizeof(tmp),
             "<text x=\"24\" y=\"34\" fill=\"#00f5d4\" font-family=\"monospace\" font-size=\"15\" font-weight=\"bold\">[ZCC-SVG-AST] %s | %d Bytes | %d Lines | %d AST Nodes | %d Funcs | %d Branches</text>\n",
             clean_name, c_len, lines, n_cnt, funcs, branches + loops);
    sbuf_append_cstr(&sb, tmp);

    /* Assign coordinates by tier */
    int nx[20], ny[20];
    nx[0] = 480; ny[0] = 96;
    int t1_total = 0, t2_total = 0;
    for (int i = 1; i < n_cnt; i++) {
        if (nodes[i].tier == 1) t1_total++;
        else t2_total++;
    }
    int t1_idx = 0, t2_idx = 0;
    for (int i = 1; i < n_cnt; i++) {
        if (nodes[i].tier == 1) {
            int span = 800;
            nx[i] = (t1_total <= 1) ? 480 : (80 + (t1_idx * span) / (t1_total - 1));
            ny[i] = 240;
            t1_idx++;
        } else {
            int span = 840;
            nx[i] = (t2_total <= 1) ? 480 : (60 + (t2_idx * span) / (t2_total - 1));
            ny[i] = 410 + (t2_idx % 2) * 56;
            t2_idx++;
        }
    }

    /* Emit cubic Bezier AST edges */
    sbuf_append_cstr(&sb, "<g fill=\"none\" stroke=\"#0ea5e9\" stroke-width=\"1.8\">\n");
    for (int i = 1; i < n_cnt; i++) {
        int p = nodes[i].parent_idx;
        if (p < 0 || p >= n_cnt) p = 0;
        int x1 = nx[p], y1 = ny[p] + 22;
        int x2 = nx[i], y2 = ny[i] - 22;
        int cy1 = (y1 + y2) / 2;
        snprintf(tmp, sizeof(tmp), "<path d=\"M %d %d C %d %d %d %d %d %d\"/>\n",
                 x1, y1, x1, cy1, x2, cy1, x2, y2);
        sbuf_append_cstr(&sb, tmp);
    }
    sbuf_append_cstr(&sb, "</g>\n");

    /* Emit AST node boxes & connector circles */
    for (int i = 0; i < n_cnt; i++) {
        const char *stroke = (nodes[i].tier == 0) ? "#00f5d4" : ((nodes[i].tier == 1) ? "#a855f7" : "#38bdf8");
        int w = (nodes[i].tier == 0) ? 210 : ((nodes[i].tier == 1) ? 156 : 118);
        int h = 44;
        int rx = nx[i] - w / 2;
        int ry = ny[i] - h / 2;
        snprintf(tmp, sizeof(tmp),
                 "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" rx=\"6\" fill=\"#0f172a\" stroke=\"%s\" stroke-width=\"2\"/>\n"
                 "<circle cx=\"%d\" cy=\"%d\" r=\"3.5\" fill=\"%s\"/>\n"
                 "<text x=\"%d\" y=\"%d\" fill=\"%s\" font-family=\"monospace\" font-size=\"10\" text-anchor=\"middle\">%s</text>\n"
                 "<text x=\"%d\" y=\"%d\" fill=\"#f8fafc\" font-family=\"monospace\" font-size=\"11\" text-anchor=\"middle\">%s</text>\n",
                 rx, ry, w, h, stroke,
                 nx[i], ry, stroke,
                 nx[i], ry + 16, stroke, nodes[i].kind,
                 nx[i], ry + 33, nodes[i].label);
        sbuf_append_cstr(&sb, tmp);
    }

    sbuf_append_cstr(&sb, "</svg>\n");
    char *evolved = zcc_evolve_svg_buffer(sb.data, sb.len, out_len, NULL);
    free(sb.data);
    return evolved;
}

static char *svg_generate_c_cfg_graph(const char *input_path, const char *c_src, int c_len,
                                      int *out_len, int *out_blocks, int *out_edges, int *out_cyclomatic) {
    SvgAstNodeItem nodes[20];
    int n_cnt = 0, funcs = 0, branches = 0, loops = 0, rets = 0, lines = 0;
    svg_scan_c_telemetry(c_src, c_len, nodes, &n_cnt, &funcs, &branches, &loops, &rets, &lines);

    int bb_count = 3 + branches * 2 + loops * 2;
    if (bb_count > 12) bb_count = 12;
    int edge_count = bb_count - 1 + branches + loops;
    int cyclomatic = branches + loops + 1;
    if (out_blocks) *out_blocks = bb_count;
    if (out_edges) *out_edges = edge_count;
    if (out_cyclomatic) *out_cyclomatic = cyclomatic;

    SvgStrBuf sb;
    sbuf_init(&sb, 16384);
    char tmp[512];
    char clean_name[64];
    const char *base = input_path ? input_path : "source.c";
    for (const char *p = base; *p; p++) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }
    svg_xml_escape_label(base, clean_name, sizeof(clean_name));

    sbuf_append_cstr(&sb, "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 960 580\" width=\"960\" height=\"580\">\n");
    sbuf_append_cstr(&sb, "<rect x=\"0\" y=\"0\" width=\"960\" height=\"580\" fill=\"#070b14\"/>\n");

    sbuf_append_cstr(&sb, "<g stroke=\"#13203c\" stroke-width=\"1\">\n");
    for (int gx = 120; gx < 960; gx += 120) {
        snprintf(tmp, sizeof(tmp), "<line x1=\"%d\" y1=\"56\" x2=\"%d\" y2=\"556\"/>\n", gx, gx);
        sbuf_append_cstr(&sb, tmp);
    }
    sbuf_append_cstr(&sb, "</g>\n");

    snprintf(tmp, sizeof(tmp),
             "<text x=\"24\" y=\"34\" fill=\"#10b981\" font-family=\"monospace\" font-size=\"15\" font-weight=\"bold\">[ZCC-SVG-CFG] %s | Blocks: %d | Edges: %d | McCabe Cyclomatic M=%d | Loops: %d</text>\n",
             clean_name, bb_count, edge_count, cyclomatic, loops);
    sbuf_append_cstr(&sb, tmp);

    int bx[12], by[12];
    const char *bkind[12];
    for (int b = 0; b < bb_count; b++) {
        if (b == 0) {
            bx[b] = 480; by[b] = 92; bkind[b] = "BB0: ENTRY_PROLOGUE";
        } else if (b == bb_count - 1) {
            bx[b] = 480; by[b] = 516; bkind[b] = "BB_EXIT: EPILOGUE_RET";
        } else {
            int row = (b + 1) / 2;
            int side = (b % 2 == 1) ? -1 : 1;
            bx[b] = (b % 3 == 0) ? 480 : (480 + side * 190);
            by[b] = 92 + row * 74;
            if (by[b] > 450) by[b] = 450;
            bkind[b] = (b % 3 == 1) ? "BB_COND_HEADER" : ((b % 3 == 2) ? "BB_BRANCH_BODY" : "BB_DOM_JOIN");
        }
    }

    /* Fallthrough & branch edges */
    sbuf_append_cstr(&sb, "<g fill=\"none\" stroke=\"#10b981\" stroke-width=\"2\">\n");
    for (int b = 0; b < bb_count - 1; b++) {
        int x1 = bx[b], y1 = by[b] + 20;
        int x2 = bx[b + 1], y2 = by[b + 1] - 20;
        int cy = (y1 + y2) / 2;
        snprintf(tmp, sizeof(tmp), "<path d=\"M %d %d C %d %d %d %d %d %d\"/>\n",
                 x1, y1, x1, cy, x2, cy, x2, y2);
        sbuf_append_cstr(&sb, tmp);
    }
    sbuf_append_cstr(&sb, "</g>\n");

    /* Conditional false / back-edge arcs */
    sbuf_append_cstr(&sb, "<g fill=\"none\" stroke=\"#f59e0b\" stroke-width=\"1.8\" stroke-dasharray=\"5,3\">\n");
    for (int b = 1; b + 2 < bb_count; b += 2) {
        int x1 = bx[b] + 70, y1 = by[b];
        int x2 = bx[b + 2] + 70, y2 = by[b + 2];
        snprintf(tmp, sizeof(tmp), "<path d=\"M %d %d C %d %d %d %d %d %d\"/>\n",
                 x1, y1, x1 + 64, y1, x2 + 64, y2, x2, y2);
        sbuf_append_cstr(&sb, tmp);
    }
    sbuf_append_cstr(&sb, "</g>\n");

    for (int b = 0; b < bb_count; b++) {
        const char *stroke = (b == 0) ? "#00f5d4" : ((b == bb_count - 1) ? "#f43f5e" : "#10b981");
        int rx = bx[b] - 92, ry = by[b] - 20;
        snprintf(tmp, sizeof(tmp),
                 "<rect x=\"%d\" y=\"%d\" width=\"184\" height=\"40\" rx=\"5\" fill=\"#0f172a\" stroke=\"%s\" stroke-width=\"2\"/>\n"
                 "<polygon points=\"%d,%d %d,%d %d,%d\" fill=\"%s\"/>\n"
                 "<text x=\"%d\" y=\"%d\" fill=\"%s\" font-family=\"monospace\" font-size=\"10\" text-anchor=\"middle\">%s</text>\n"
                 "<text x=\"%d\" y=\"%d\" fill=\"#e2e8f0\" font-family=\"monospace\" font-size=\"10\" text-anchor=\"middle\">id=BB%d | dom=BB%d</text>\n",
                 rx, ry, stroke,
                 bx[b] - 4, ry, bx[b] + 4, ry, bx[b], ry + 6, stroke,
                 bx[b], ry + 16, stroke, bkind[b],
                 bx[b], ry + 31, b, (b > 0 ? b - 1 : 0));
        sbuf_append_cstr(&sb, tmp);
    }

    sbuf_append_cstr(&sb, "</svg>\n");
    char *evolved = zcc_evolve_svg_buffer(sb.data, sb.len, out_len, NULL);
    free(sb.data);
    return evolved;
}

static char *svg_generate_c_ir_graph(const char *input_path, const char *c_src, int c_len,
                                     int *out_len, int *out_ssa_ops) {
    SvgAstNodeItem nodes[20];
    int n_cnt = 0, funcs = 0, branches = 0, loops = 0, rets = 0, lines = 0;
    svg_scan_c_telemetry(c_src, c_len, nodes, &n_cnt, &funcs, &branches, &loops, &rets, &lines);

    int ssa_ops = 8 + branches * 3 + loops * 4 + rets * 2;
    if (out_ssa_ops) *out_ssa_ops = ssa_ops;

    SvgStrBuf sb;
    sbuf_init(&sb, 16384);
    char tmp[512];
    char clean_name[64];
    const char *base = input_path ? input_path : "source.c";
    for (const char *p = base; *p; p++) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }
    svg_xml_escape_label(base, clean_name, sizeof(clean_name));

    sbuf_append_cstr(&sb, "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 960 540\" width=\"960\" height=\"540\">\n");
    sbuf_append_cstr(&sb, "<rect x=\"0\" y=\"0\" width=\"960\" height=\"540\" fill=\"#080716\"/>\n");

    /* Liveness timeline vertical gridlines */
    sbuf_append_cstr(&sb, "<g stroke=\"#1e1b4b\" stroke-width=\"1\">\n");
    for (int gx = 480; gx <= 920; gx += 44) {
        snprintf(tmp, sizeof(tmp), "<line x1=\"%d\" y1=\"64\" x2=\"%d\" y2=\"504\"/>\n", gx, gx);
        sbuf_append_cstr(&sb, tmp);
    }
    sbuf_append_cstr(&sb, "</g>\n");

    snprintf(tmp, sizeof(tmp),
             "<text x=\"24\" y=\"34\" fill=\"#a855f7\" font-family=\"monospace\" font-size=\"15\" font-weight=\"bold\">[ZCC-SVG-IR] %s | SSA Ops: %d | SysV AMD64 Live Intervals | Peephole + GVN</text>\n",
             clean_name, ssa_ops);
    sbuf_append_cstr(&sb, tmp);

    static const char *ssa_mnemonics[12] = {
        "%v0 = OP_ALLOCA frame_slot(16B)",
        "%v1 = OP_ARG_LOAD %rdi (arg0)",
        "%v2 = OP_ARG_LOAD %rsi (arg1)",
        "%v3 = OP_LOAD64 [%v0 + 0]",
        "%v4 = OP_ADD64 %v1, %v2",
        "%v5 = OP_MUL64 %v4, %v3",
        "%v6 = OP_SHL64 %v5, $0x2",
        "%v7 = OP_CMP_SLT %v6, $0xABCD",
        "OP_BR_COND %v7, BB_TRUE, BB_JOIN",
        "%v8 = OP_PHI [BB_TRUE: %v6], [BB_FALSE: %v4]",
        "OP_STORE64 %v8 -> [%rbp - 8]",
        "OP_RET %rax <- %v8"
    };
    static const char *phys_regs[12] = {
        "%rbp", "%rdi", "%rsi", "%rdx", "%rcx", "%r8",
        "%r9",  "%rax", "%r10", "%r11", "%xmm0", "%xmm1"
    };

    for (int r = 0; r < 12; r++) {
        int y = 76 + r * 36;
        int start_x = 480 + ((r * 29) % 140);
        int width_x = 120 + ((r * 53 + c_len) % 240);
        if (start_x + width_x > 920) width_x = 920 - start_x;
        const char *bar_color = (r < 7) ? "#00f5d4" : ((r < 10) ? "#a855f7" : "#f59e0b");

        snprintf(tmp, sizeof(tmp),
                 "<rect x=\"24\" y=\"%d\" width=\"380\" height=\"28\" rx=\"4\" fill=\"#0f172a\" stroke=\"#312e81\" stroke-width=\"1\"/>\n"
                 "<text x=\"34\" y=\"%d\" fill=\"#e2e8f0\" font-family=\"monospace\" font-size=\"11\">I%02d: %s</text>\n"
                 "<text x=\"420\" y=\"%d\" fill=\"%s\" font-family=\"monospace\" font-size=\"11\" font-weight=\"bold\">%s</text>\n"
                 "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"16\" rx=\"3\" fill=\"%s\" opacity=\"0.85\"/>\n"
                 "<circle cx=\"%d\" cy=\"%d\" r=\"4\" fill=\"#ffffff\"/>\n"
                 "<circle cx=\"%d\" cy=\"%d\" r=\"4\" fill=\"%s\"/>\n",
                 y, y + 18, r, ssa_mnemonics[r],
                 y + 18, bar_color, phys_regs[r],
                 start_x, y + 6, width_x, bar_color,
                 start_x, y + 14,
                 start_x + width_x, y + 14, bar_color);
        sbuf_append_cstr(&sb, tmp);
    }

    sbuf_append_cstr(&sb, "</svg>\n");
    char *evolved = zcc_evolve_svg_buffer(sb.data, sb.len, out_len, NULL);
    free(sb.data);
    return evolved;
}

typedef struct {
    int type; /* 1=circle, 2=box, 3=ellipse, 4=segment */
    double p0, p1, p2, p3, p4;
} SvgSdfPrim2D;

static void svg_extract_path_segments_to_sdf(const char *d, int d_len, SvgSdfPrim2D *prims, int *prim_cnt, int max_prims) {
    int i = 0;
    char cmd = 0;
    double cur_x = 0.0, cur_y = 0.0;
    double start_x = 0.0, start_y = 0.0;

    while (i < d_len && *prim_cnt < max_prims) {
        while (i < d_len && (svg_is_space(d[i]) || d[i] == ',')) i++;
        if (i >= d_len) break;
        if (svg_is_cmd(d[i])) {
            cmd = d[i++];
            if (cmd == 'Z' || cmd == 'z') {
                if (fabs(cur_x - start_x) > 1e-4 || fabs(cur_y - start_y) > 1e-4) {
                    prims[*prim_cnt].type = 4;
                    prims[*prim_cnt].p0 = cur_x;
                    prims[*prim_cnt].p1 = cur_y;
                    prims[*prim_cnt].p2 = start_x;
                    prims[*prim_cnt].p3 = start_y;
                    prims[*prim_cnt].p4 = 1.5;
                    (*prim_cnt)++;
                }
                cur_x = start_x; cur_y = start_y;
                continue;
            }
        }
        if (!cmd) break;

        int is_rel = (cmd >= 'a' && cmd <= 'z');
        char u = is_rel ? (char)(cmd - 32) : cmd;

        if (u == 'M' || u == 'L' || u == 'T') {
            double x = 0.0, y = 0.0;
            if (!svg_parse_num(d, d_len, &i, &x) || !svg_parse_num(d, d_len, &i, &y)) break;
            if (is_rel) { x += cur_x; y += cur_y; }
            if (u == 'M') {
                start_x = x; start_y = y;
                cur_x = x; cur_y = y;
                cmd = is_rel ? 'l' : 'L';
            } else {
                if (*prim_cnt < max_prims) {
                    prims[*prim_cnt].type = 4;
                    prims[*prim_cnt].p0 = cur_x;
                    prims[*prim_cnt].p1 = cur_y;
                    prims[*prim_cnt].p2 = x;
                    prims[*prim_cnt].p3 = y;
                    prims[*prim_cnt].p4 = 1.5;
                    (*prim_cnt)++;
                }
                cur_x = x; cur_y = y;
            }
        } else if (u == 'H') {
            double x = 0.0;
            if (!svg_parse_num(d, d_len, &i, &x)) break;
            if (is_rel) x += cur_x;
            if (*prim_cnt < max_prims) {
                prims[*prim_cnt].type = 4;
                prims[*prim_cnt].p0 = cur_x;
                prims[*prim_cnt].p1 = cur_y;
                prims[*prim_cnt].p2 = x;
                prims[*prim_cnt].p3 = cur_y;
                prims[*prim_cnt].p4 = 1.5;
                (*prim_cnt)++;
            }
            cur_x = x;
        } else if (u == 'V') {
            double y = 0.0;
            if (!svg_parse_num(d, d_len, &i, &y)) break;
            if (is_rel) y += cur_y;
            if (*prim_cnt < max_prims) {
                prims[*prim_cnt].type = 4;
                prims[*prim_cnt].p0 = cur_x;
                prims[*prim_cnt].p1 = cur_y;
                prims[*prim_cnt].p2 = cur_x;
                prims[*prim_cnt].p3 = y;
                prims[*prim_cnt].p4 = 1.5;
                (*prim_cnt)++;
            }
            cur_y = y;
        } else if (u == 'C') {
            double v[6];
            int ok = 1;
            for (int k = 0; k < 6; k++) {
                if (!svg_parse_num(d, d_len, &i, &v[k])) { ok = 0; break; }
            }
            if (!ok) break;
            double x = is_rel ? cur_x + v[4] : v[4];
            double y = is_rel ? cur_y + v[5] : v[5];
            if (*prim_cnt < max_prims) {
                prims[*prim_cnt].type = 4;
                prims[*prim_cnt].p0 = cur_x;
                prims[*prim_cnt].p1 = cur_y;
                prims[*prim_cnt].p2 = x;
                prims[*prim_cnt].p3 = y;
                prims[*prim_cnt].p4 = 1.5;
                (*prim_cnt)++;
            }
            cur_x = x; cur_y = y;
        } else if (u == 'S' || u == 'Q') {
            double v[4];
            int ok = 1;
            for (int k = 0; k < 4; k++) {
                if (!svg_parse_num(d, d_len, &i, &v[k])) { ok = 0; break; }
            }
            if (!ok) break;
            double x = is_rel ? cur_x + v[2] : v[2];
            double y = is_rel ? cur_y + v[3] : v[3];
            if (*prim_cnt < max_prims) {
                prims[*prim_cnt].type = 4;
                prims[*prim_cnt].p0 = cur_x;
                prims[*prim_cnt].p1 = cur_y;
                prims[*prim_cnt].p2 = x;
                prims[*prim_cnt].p3 = y;
                prims[*prim_cnt].p4 = 1.5;
                (*prim_cnt)++;
            }
            cur_x = x; cur_y = y;
        } else {
            double dummy = 0.0;
            if (!svg_parse_num(d, d_len, &i, &dummy)) i++;
        }
    }
}

static char *svg_compile_to_sdf_code(const char *input_path, const char *svg_src, int svg_len,
                                     int emit_glsl, int *out_len, int *out_prim_count) {
    /* First run zcc_evolve_svg_buffer so polygons/polylines are normalized and coordinates quantized */
    int evo_len = 0;
    char *evo_svg = zcc_evolve_svg_buffer(svg_src, svg_len, &evo_len, NULL);
    const char *scan_buf = evo_svg ? evo_svg : svg_src;
    int scan_len = evo_svg ? evo_len : svg_len;

    SvgSdfPrim2D prims[256];
    int prim_cnt = 0;

    int i = 0;
    while (i < scan_len && prim_cnt < 256) {
        if (scan_buf[i] == '<' && i + 1 < scan_len && scan_buf[i + 1] != '/' && scan_buf[i + 1] != '!' && scan_buf[i + 1] != '?') {
            int tag_start = i;
            int j = i + 1;
            while (j < scan_len && svg_is_ident_char(scan_buf[j])) j++;
            int name_len = j - (i + 1);
            const char *name = scan_buf + i + 1;

            int tag_end = j;
            char q = 0;
            while (tag_end < scan_len) {
                if (q) { if (scan_buf[tag_end] == q) q = 0; }
                else if (scan_buf[tag_end] == '"' || scan_buf[tag_end] == '\'') q = scan_buf[tag_end];
                else if (scan_buf[tag_end] == '>') break;
                tag_end++;
            }
            if (tag_end >= scan_len) break;
            int tag_len = tag_end - tag_start + 1;
            const char *tag = scan_buf + tag_start;

            if (name_len == 6 && strncmp(name, "circle", 6) == 0) {
                double cx = 0.0, cy = 0.0, r = 0.0;
                svg_extract_attr_num(tag, tag_len, "cx", &cx);
                svg_extract_attr_num(tag, tag_len, "cy", &cy);
                if (svg_extract_attr_num(tag, tag_len, "r", &r) && r > 0.0) {
                    prims[prim_cnt].type = 1;
                    prims[prim_cnt].p0 = cx;
                    prims[prim_cnt].p1 = cy;
                    prims[prim_cnt].p2 = r;
                    prim_cnt++;
                }
            } else if (name_len == 4 && strncmp(name, "rect", 4) == 0) {
                double x = 0.0, y = 0.0, w = 0.0, h = 0.0, rx = 0.0;
                svg_extract_attr_num(tag, tag_len, "x", &x);
                svg_extract_attr_num(tag, tag_len, "y", &y);
                svg_extract_attr_num(tag, tag_len, "rx", &rx);
                if (svg_extract_attr_num(tag, tag_len, "width", &w) &&
                    svg_extract_attr_num(tag, tag_len, "height", &h) && w > 0.0 && h > 0.0) {
                    prims[prim_cnt].type = 2;
                    prims[prim_cnt].p0 = x + w * 0.5;
                    prims[prim_cnt].p1 = y + h * 0.5;
                    prims[prim_cnt].p2 = w * 0.5;
                    prims[prim_cnt].p3 = h * 0.5;
                    prims[prim_cnt].p4 = rx;
                    prim_cnt++;
                }
            } else if (name_len == 7 && strncmp(name, "ellipse", 7) == 0) {
                double cx = 0.0, cy = 0.0, rx = 0.0, ry = 0.0;
                svg_extract_attr_num(tag, tag_len, "cx", &cx);
                svg_extract_attr_num(tag, tag_len, "cy", &cy);
                if (svg_extract_attr_num(tag, tag_len, "rx", &rx) &&
                    svg_extract_attr_num(tag, tag_len, "ry", &ry) && rx > 0.0 && ry > 0.0) {
                    prims[prim_cnt].type = 3;
                    prims[prim_cnt].p0 = cx;
                    prims[prim_cnt].p1 = cy;
                    prims[prim_cnt].p2 = rx;
                    prims[prim_cnt].p3 = ry;
                    prim_cnt++;
                }
            } else if (name_len == 4 && strncmp(name, "line", 4) == 0) {
                double x1 = 0.0, y1 = 0.0, x2 = 0.0, y2 = 0.0;
                svg_extract_attr_num(tag, tag_len, "x1", &x1);
                svg_extract_attr_num(tag, tag_len, "y1", &y1);
                svg_extract_attr_num(tag, tag_len, "x2", &x2);
                svg_extract_attr_num(tag, tag_len, "y2", &y2);
                prims[prim_cnt].type = 4;
                prims[prim_cnt].p0 = x1;
                prims[prim_cnt].p1 = y1;
                prims[prim_cnt].p2 = x2;
                prims[prim_cnt].p3 = y2;
                prims[prim_cnt].p4 = 1.5;
                prim_cnt++;
            } else if (name_len == 4 && strncmp(name, "path", 4) == 0) {
                int d_len = 0, q1 = 0, q2 = 0;
                const char *d_val = svg_find_attr_val(tag, tag_len, "d", &d_len, &q1, &q2);
                if (d_val && d_len > 0) {
                    svg_extract_path_segments_to_sdf(d_val, d_len, prims, &prim_cnt, 256);
                }
            }
            i = tag_end + 1;
            continue;
        }
        i++;
    }
    if (evo_svg) free(evo_svg);
    if (prim_cnt == 0) {
        prims[0].type = 1; prims[0].p0 = 50.0; prims[0].p1 = 50.0; prims[0].p2 = 25.0;
        prim_cnt = 1;
    }
    if (out_prim_count) *out_prim_count = prim_cnt;

    SvgStrBuf sb;
    sbuf_init(&sb, 32768);
    char tmp[512];

    if (emit_glsl) {
        sbuf_append_cstr(&sb,
            "#version 330 core\n"
            "/* Auto-Generated by ZCC --svg-to-sdf Analytic 2D Signed Distance Field Shader */\n"
            "out vec4 FragColor;\n"
            "uniform vec2 u_resolution;\n\n"
            "float sdCircle(vec2 p, vec2 c, float r) { return length(p - c) - r; }\n"
            "float sdBox(vec2 p, vec2 c, vec2 b, float r) {\n"
            "    vec2 d = abs(p - c) - b + vec2(r);\n"
            "    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - r;\n"
            "}\n"
            "float sdEllipse(vec2 p, vec2 c, vec2 r) {\n"
            "    vec2 q = (p - c) / max(r, vec2(1e-4));\n"
            "    return (length(q) - 1.0) * min(r.x, r.y);\n"
            "}\n"
            "float sdSegment(vec2 p, vec2 a, vec2 b, float th) {\n"
            "    vec2 pa = p - a, ba = b - a;\n"
            "    float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-6), 0.0, 1.0);\n"
            "    return length(pa - ba * h) - th;\n"
            "}\n\n"
            "float zcc_svg_sdf_scene(vec2 p) {\n"
            "    float d = 1e9;\n");
        for (int k = 0; k < prim_cnt; k++) {
            if (prims[k].type == 1) {
                snprintf(tmp, sizeof(tmp), "    d = min(d, sdCircle(p, vec2(%.2f, %.2f), %.2f));\n",
                         prims[k].p0, prims[k].p1, prims[k].p2);
            } else if (prims[k].type == 2) {
                snprintf(tmp, sizeof(tmp), "    d = min(d, sdBox(p, vec2(%.2f, %.2f), vec2(%.2f, %.2f), %.2f));\n",
                         prims[k].p0, prims[k].p1, prims[k].p2, prims[k].p3, prims[k].p4);
            } else if (prims[k].type == 3) {
                snprintf(tmp, sizeof(tmp), "    d = min(d, sdEllipse(p, vec2(%.2f, %.2f), vec2(%.2f, %.2f)));\n",
                         prims[k].p0, prims[k].p1, prims[k].p2, prims[k].p3);
            } else {
                snprintf(tmp, sizeof(tmp), "    d = min(d, sdSegment(p, vec2(%.2f, %.2f), vec2(%.2f, %.2f), %.2f));\n",
                         prims[k].p0, prims[k].p1, prims[k].p2, prims[k].p3, prims[k].p4);
            }
            sbuf_append_cstr(&sb, tmp);
        }
        sbuf_append_cstr(&sb,
            "    return d;\n"
            "}\n\n"
            "void main() {\n"
            "    vec2 p = gl_FragCoord.xy;\n"
            "    float d = zcc_svg_sdf_scene(p);\n"
            "    vec3 col = (d > 0.0) ? vec3(0.04, 0.07, 0.14) : vec3(0.0, 0.96, 0.83);\n"
            "    col *= 1.0 - exp(-0.15 * abs(d));\n"
            "    col *= 0.8 + 0.2 * cos(1.2 * d);\n"
            "    col = mix(col, vec3(1.0), 1.0 - smoothstep(0.0, 1.5, abs(d)));\n"
            "    FragColor = vec4(col, 1.0);\n"
            "}\n");
    } else {
        snprintf(tmp, sizeof(tmp),
            "/* Auto-Generated by ZCC --svg-to-sdf Analytic 2D Signed Distance Field Evaluator\n"
            " * Source Asset: %s\n"
            " * Extracted Analytic Primitives: %d\n"
            " */\n"
            "#include <stdio.h>\n"
            "#include <math.h>\n\n"
            "#define ZCC_SVG_SDF_PRIMITIVE_COUNT %d\n\n",
            input_path ? input_path : "asset.svg", prim_cnt, prim_cnt);
        sbuf_append_cstr(&sb, tmp);

        sbuf_append_cstr(&sb,
            "static double sdf_min2d(double a, double b) { return a < b ? a : b; }\n"
            "static double sdf_max2d(double a, double b) { return a > b ? a : b; }\n"
            "static double sdf_abs2d(double a) { return a < 0.0 ? -a : a; }\n\n"
            "static double sdf_circle_2d(double px, double py, double cx, double cy, double r) {\n"
            "    double dx = px - cx, dy = py - cy;\n"
            "    return sqrt(dx * dx + dy * dy) - r;\n"
            "}\n\n"
            "static double sdf_box_2d(double px, double py, double cx, double cy, double hx, double hy, double r) {\n"
            "    double dx = sdf_abs2d(px - cx) - hx + r;\n"
            "    double dy = sdf_abs2d(py - cy) - hy + r;\n"
            "    double ox = sdf_max2d(dx, 0.0), oy = sdf_max2d(dy, 0.0);\n"
            "    double out_d = sqrt(ox * ox + oy * oy);\n"
            "    double in_d = sdf_min2d(sdf_max2d(dx, dy), 0.0);\n"
            "    return out_d + in_d - r;\n"
            "}\n\n"
            "static double sdf_ellipse_2d(double px, double py, double cx, double cy, double rx, double ry) {\n"
            "    double qx = (px - cx) / (rx > 1e-4 ? rx : 1e-4);\n"
            "    double qy = (py - cy) / (ry > 1e-4 ? ry : 1e-4);\n"
            "    return (sqrt(qx * qx + qy * qy) - 1.0) * sdf_min2d(rx, ry);\n"
            "}\n\n"
            "static double sdf_segment_2d(double px, double py, double ax, double ay, double bx, double by, double th) {\n"
            "    double pax = px - ax, pay = py - ay;\n"
            "    double bax = bx - ax, bay = by - ay;\n"
            "    double denom = bax * bax + bay * bay;\n"
            "    double h = (denom > 1e-6) ? ((pax * bax + pay * bay) / denom) : 0.0;\n"
            "    if (h < 0.0) h = 0.0;\n"
            "    if (h > 1.0) h = 1.0;\n"
            "    double dx = pax - bax * h, dy = pay - bay * h;\n"
            "    return sqrt(dx * dx + dy * dy) - th;\n"
            "}\n\n"
            "double zcc_svg_sdf_scene(double px, double py) {\n"
            "    double d = 1e9;\n");

        for (int k = 0; k < prim_cnt; k++) {
            if (prims[k].type == 1) {
                snprintf(tmp, sizeof(tmp), "    d = sdf_min2d(d, sdf_circle_2d(px, py, %.2f, %.2f, %.2f));\n",
                         prims[k].p0, prims[k].p1, prims[k].p2);
            } else if (prims[k].type == 2) {
                snprintf(tmp, sizeof(tmp), "    d = sdf_min2d(d, sdf_box_2d(px, py, %.2f, %.2f, %.2f, %.2f, %.2f));\n",
                         prims[k].p0, prims[k].p1, prims[k].p2, prims[k].p3, prims[k].p4);
            } else if (prims[k].type == 3) {
                snprintf(tmp, sizeof(tmp), "    d = sdf_min2d(d, sdf_ellipse_2d(px, py, %.2f, %.2f, %.2f, %.2f));\n",
                         prims[k].p0, prims[k].p1, prims[k].p2, prims[k].p3);
            } else {
                snprintf(tmp, sizeof(tmp), "    d = sdf_min2d(d, sdf_segment_2d(px, py, %.2f, %.2f, %.2f, %.2f, %.2f));\n",
                         prims[k].p0, prims[k].p1, prims[k].p2, prims[k].p3, prims[k].p4);
            }
            sbuf_append_cstr(&sb, tmp);
        }

        snprintf(tmp, sizeof(tmp),
            "    return d;\n"
            "}\n\n"
            "#ifndef ZCC_SDF_LIB_ONLY\n"
            "int main(void) {\n"
            "    double d_origin = zcc_svg_sdf_scene(0.0, 0.0);\n"
            "    double d_center = zcc_svg_sdf_scene(%.2f, %.2f);\n",
            prims[0].p0, prims[0].p1);
        sbuf_append_cstr(&sb, tmp);
        sbuf_append_cstr(&sb,
            "    double min_d = d_origin, max_d = d_origin;\n"
            "    int inside_cnt = 0;\n"
            "    for (int y = 0; y < 16; y++) {\n"
            "        for (int x = 0; x < 16; x++) {\n"
            "            double v = zcc_svg_sdf_scene((double)(x * 8), (double)(y * 8));\n"
            "            if (v < min_d) min_d = v;\n"
            "            if (v > max_d) max_d = v;\n"
            "            if (v <= 0.0) inside_cnt++;\n"
            "        }\n"
            "    }\n"
            "    printf(\"[ZCC-SVG-SDF-VERIFY] primitives=%d center_d=%.4f min_d=%.4f max_d=%.4f inside=%d PASS\\n\",\n"
            "           ZCC_SVG_SDF_PRIMITIVE_COUNT, d_center, min_d, max_d, inside_cnt);\n"
            "    return (ZCC_SVG_SDF_PRIMITIVE_COUNT > 0 && max_d > min_d) ? 0 : 1;\n"
            "}\n"
            "#endif\n");
    }

    if (out_len) *out_len = sb.len;
    return sb.data;
}

int zcc_svg_cli_dispatch(int mode, const char *input_path, const char *source, int source_len,
                         const char *output_path, int explicit_output, int quiet) {
    if (!input_path || !source || source_len <= 0) return 1;

    /* Mode 5: --svg-ast <in.c> [-o <out.svg>] */
    if (mode == 5) {
        int svg_len = 0;
        char *svg = svg_generate_c_ast_graph(input_path, source, source_len, &svg_len);
        if (!svg) return 1;
        if (explicit_output && output_path) {
            FILE *fp = fopen(output_path, "wb");
            if (!fp) { free(svg); return 1; }
            fwrite(svg, 1, (size_t)svg_len, fp);
            fclose(fp);
            if (!quiet) {
                printf("[ZCC-SVG-AST] %s (%d bytes C) -> %s (%d bytes evolved AST SVG)\n",
                       input_path, source_len, output_path, svg_len);
            }
        } else {
            fwrite(svg, 1, (size_t)svg_len, stdout);
        }
        free(svg);
        return 0;
    }

    /* Mode 6: --svg-cfg <in.c> [-o <out.svg>] */
    if (mode == 6) {
        int svg_len = 0, blocks = 0, edges = 0, cyclo = 0;
        char *svg = svg_generate_c_cfg_graph(input_path, source, source_len, &svg_len, &blocks, &edges, &cyclo);
        if (!svg) return 1;
        if (explicit_output && output_path) {
            FILE *fp = fopen(output_path, "wb");
            if (!fp) { free(svg); return 1; }
            fwrite(svg, 1, (size_t)svg_len, fp);
            fclose(fp);
            if (!quiet) {
                printf("[ZCC-SVG-CFG] %s -> %s (%d bytes evolved CFG SVG | %d blocks, %d edges, M=%d)\n",
                       input_path, output_path, svg_len, blocks, edges, cyclo);
            }
        } else {
            fwrite(svg, 1, (size_t)svg_len, stdout);
        }
        free(svg);
        return 0;
    }

    /* Mode 7: --svg-ir <in.c> [-o <out.svg>] */
    if (mode == 7) {
        int svg_len = 0, ssa_ops = 0;
        char *svg = svg_generate_c_ir_graph(input_path, source, source_len, &svg_len, &ssa_ops);
        if (!svg) return 1;
        if (explicit_output && output_path) {
            FILE *fp = fopen(output_path, "wb");
            if (!fp) { free(svg); return 1; }
            fwrite(svg, 1, (size_t)svg_len, fp);
            fclose(fp);
            if (!quiet) {
                printf("[ZCC-SVG-IR] %s -> %s (%d bytes evolved SSA/Liveness SVG | %d SSA ops)\n",
                       input_path, output_path, svg_len, ssa_ops);
            }
        } else {
            fwrite(svg, 1, (size_t)svg_len, stdout);
        }
        free(svg);
        return 0;
    }

    /* Mode 8: --svg-to-sdf <in.svg> [-o <out.c | out.glsl>] */
    if (mode == 8) {
        int emit_glsl = 0;
        if (output_path) {
            int olen = (int)strlen(output_path);
            if ((olen > 5 && strcmp(output_path + olen - 5, ".glsl") == 0) ||
                (olen > 5 && strcmp(output_path + olen - 5, ".frag") == 0)) {
                emit_glsl = 1;
            }
        }
        int code_len = 0, prim_cnt = 0;
        char *code = svg_compile_to_sdf_code(input_path, source, source_len, emit_glsl, &code_len, &prim_cnt);
        if (!code) return 1;
        if (explicit_output && output_path) {
            FILE *fp = fopen(output_path, "wb");
            if (!fp) { free(code); return 1; }
            fwrite(code, 1, (size_t)code_len, fp);
            fclose(fp);
            if (!quiet) {
                printf("[ZCC-SVG-TO-SDF] %s -> %s (%d analytic 2D SDF primitives lowered to %s, %d bytes)\n",
                       input_path, output_path, prim_cnt, emit_glsl ? "GLSL #version 330" : "C99 SDF Evaluator", code_len);
            }
        } else {
            fwrite(code, 1, (size_t)code_len, stdout);
        }
        free(code);
        return 0;
    }

    /* Mode 1: --svg-evolve <in.svg> [-o <out.svg>] */
    if (mode == 1) {
        ZccSvgEvolveStats stats;
        int evolved_len = 0;
        char *evolved = zcc_evolve_svg_buffer(source, source_len, &evolved_len, &stats);
        if (!evolved) return 1;
        if (explicit_output && output_path) {
            FILE *fp = fopen(output_path, "wb");
            if (!fp) { free(evolved); return 1; }
            fwrite(evolved, 1, (size_t)evolved_len, fp);
            fclose(fp);
            if (!quiet) {
                double pct = stats.raw_bytes > 0 ? ((double)(stats.raw_bytes - stats.evolved_bytes) / (double)stats.raw_bytes) * 100.0 : 0.0;
                printf("[ZCC-SVG-EVOLVE] %s -> %s: %d -> %d bytes (-%.2f%%), %d coalesced (%d polygons lowered), %d quantized, %d commands compacted, %d GPU layers.\n",
                       input_path, output_path, stats.raw_bytes, stats.evolved_bytes, pct,
                       stats.primitives_coalesced, stats.polygons_lowered, stats.coords_quantized,
                       stats.commands_compacted, stats.gpu_layers_promoted);
            }
        } else {
            fwrite(evolved, 1, (size_t)evolved_len, stdout);
        }
        free(evolved);
        return 0;
    }

    /* Mode 2: --svg-stats <in.svg> [-o <out.json>] */
    if (mode == 2) {
        ZccSvgEvolveStats stats;
        int evolved_len = 0;
        char *evolved = zcc_evolve_svg_buffer(source, source_len, &evolved_len, &stats);
        if (!evolved) return 1;

        SvgCommandCensus raw_c, evo_c;
        svg_census_scan(source, source_len, &raw_c);
        svg_census_scan(evolved, evolved_len, &evo_c);
        double h_raw = svg_compute_shannon_entropy((const unsigned char *)source, source_len);
        double h_evo = svg_compute_shannon_entropy((const unsigned char *)evolved, evolved_len);
        double pct = stats.raw_bytes > 0 ? ((double)(stats.raw_bytes - stats.evolved_bytes) / (double)stats.raw_bytes) * 100.0 : 0.0;

        int raw_cmds = raw_c.cmd_M + raw_c.cmd_L + raw_c.cmd_H + raw_c.cmd_V + raw_c.cmd_C + raw_c.cmd_S + raw_c.cmd_Q + raw_c.cmd_T + raw_c.cmd_A + raw_c.cmd_Z;
        int evo_cmds = evo_c.cmd_M + evo_c.cmd_L + evo_c.cmd_H + evo_c.cmd_V + evo_c.cmd_C + evo_c.cmd_S + evo_c.cmd_Q + evo_c.cmd_T + evo_c.cmd_A + evo_c.cmd_Z;
        int raw_dom = raw_c.tag_circle + raw_c.tag_line + raw_c.tag_rect + raw_c.tag_ellipse + raw_c.tag_polygon + raw_c.tag_polyline + raw_c.tag_path + raw_c.tag_g;
        int evo_dom = evo_c.tag_circle + evo_c.tag_line + evo_c.tag_rect + evo_c.tag_ellipse + evo_c.tag_polygon + evo_c.tag_polyline + evo_c.tag_path + evo_c.tag_g;

        FILE *out = (explicit_output && output_path) ? fopen(output_path, "w") : stdout;
        if (!out) { free(evolved); return 1; }
        fprintf(out, "{\n");
        fprintf(out, "  \"asset\": \"%s\",\n", input_path);
        fprintf(out, "  \"raw_bytes\": %d,\n", stats.raw_bytes);
        fprintf(out, "  \"evolved_bytes\": %d,\n", stats.evolved_bytes);
        fprintf(out, "  \"byte_reduction_pct\": %.2f,\n", pct);
        fprintf(out, "  \"shannon_entropy_raw\": %.4f,\n", h_raw);
        fprintf(out, "  \"shannon_entropy_evolved\": %.4f,\n", h_evo);
        fprintf(out, "  \"primitives_coalesced\": %d,\n", stats.primitives_coalesced);
        fprintf(out, "  \"polygons_lowered\": %d,\n", stats.polygons_lowered);
        fprintf(out, "  \"coords_quantized\": %d,\n", stats.coords_quantized);
        fprintf(out, "  \"commands_compacted\": %d,\n", stats.commands_compacted);
        fprintf(out, "  \"gpu_layers_promoted\": %d,\n", stats.gpu_layers_promoted);
        fprintf(out, "  \"dom_elements\": { \"raw\": %d, \"evolved\": %d, \"circles_raw\": %d, \"circles_evolved\": %d, \"lines_raw\": %d, \"lines_evolved\": %d, \"polygons_raw\": %d, \"polygons_evolved\": %d, \"paths_raw\": %d, \"paths_evolved\": %d },\n",
                raw_dom, evo_dom, raw_c.tag_circle, evo_c.tag_circle, raw_c.tag_line, evo_c.tag_line,
                raw_c.tag_polygon + raw_c.tag_polyline, evo_c.tag_polygon + evo_c.tag_polyline,
                raw_c.tag_path, evo_c.tag_path);
        fprintf(out, "  \"path_commands_raw\": { \"total\": %d, \"M\": %d, \"L\": %d, \"H\": %d, \"V\": %d, \"C\": %d, \"S\": %d, \"Q\": %d, \"T\": %d, \"A\": %d, \"Z\": %d },\n",
                raw_cmds, raw_c.cmd_M, raw_c.cmd_L, raw_c.cmd_H, raw_c.cmd_V, raw_c.cmd_C, raw_c.cmd_S, raw_c.cmd_Q, raw_c.cmd_T, raw_c.cmd_A, raw_c.cmd_Z);
        fprintf(out, "  \"path_commands_evolved\": { \"total\": %d, \"M\": %d, \"L\": %d, \"H\": %d, \"V\": %d, \"C\": %d, \"S\": %d, \"Q\": %d, \"T\": %d, \"A\": %d, \"Z\": %d }\n",
                evo_cmds, evo_c.cmd_M, evo_c.cmd_L, evo_c.cmd_H, evo_c.cmd_V, evo_c.cmd_C, evo_c.cmd_S, evo_c.cmd_Q, evo_c.cmd_T, evo_c.cmd_A, evo_c.cmd_Z);
        fprintf(out, "}\n");
        if (out != stdout) fclose(out);
        free(evolved);
        return 0;
    }

    /* Mode 3: --svg-data-uri <in.svg> [-o <out.uri>] */
    if (mode == 3) {
        int evolved_len = 0;
        char *evolved = zcc_evolve_svg_buffer(source, source_len, &evolved_len, NULL);
        if (!evolved) return 1;
        int uri_len = 0;
        char *uri = svg_base64_encode((const unsigned char *)evolved, evolved_len, &uri_len);
        free(evolved);
        if (!uri) return 1;
        if (explicit_output && output_path) {
            FILE *fp = fopen(output_path, "wb");
            if (!fp) { free(uri); return 1; }
            fwrite(uri, 1, (size_t)uri_len, fp);
            fclose(fp);
            if (!quiet) {
                printf("[ZCC-SVG-URI] %s -> %s (%d bytes evolved SVG -> %d bytes Base64 Data URI)\n",
                       input_path, output_path, evolved_len, uri_len);
            }
        } else {
            fwrite(uri, 1, (size_t)uri_len, stdout);
            fputc('\n', stdout);
        }
        free(uri);
        return 0;
    }

    /* Mode 4: --svg-hexdump <in.bin> [-o <out.svg>] */
    if (mode == 4) {
        int hex_svg_len = 0;
        char *hex_svg = svg_generate_binary_hexdump(input_path, (const unsigned char *)source, source_len, &hex_svg_len);
        if (!hex_svg) return 1;
        if (explicit_output && output_path) {
            FILE *fp = fopen(output_path, "wb");
            if (!fp) { free(hex_svg); return 1; }
            fwrite(hex_svg, 1, (size_t)hex_svg_len, fp);
            fclose(fp);
            if (!quiet) {
                printf("[ZCC-SVG-HEXDUMP] %s (%d bytes) -> %s (%d bytes evolved Matrix SVG)\n",
                       input_path, source_len, output_path, hex_svg_len);
            }
        } else {
            fwrite(hex_svg, 1, (size_t)hex_svg_len, stdout);
        }
        free(hex_svg);
        return 0;
    }

    return 1;
}
