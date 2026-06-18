#include "zcc_vir.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define ZCC_VIR_MAX_SEGMENTS 100000
#define ZCC_VIR_MAX_COORD_ABS 100000000.0

static ZccSvgStatus zcc_svg_fail(
    ZccSvgError *err,
    ZccSvgStatus status,
    const char *msg,
    size_t offset
) {
    if (err) {
        err->status = status;
        err->message = msg;
        err->offset = offset;
    }
    return status;
}

static int is_number_start(char c) {
    return (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.';
}

static void skip_spaces_and_commas(const char **p) {
    while (**p == ' ' || **p == '\t' || **p == '\r' || **p == '\n' || **p == ',') {
        (*p)++;
    }
}

static int64_t quantize_coord(float val, float epsilon) {
    double scaled = (double)val / (double)epsilon;
    double rounded = round(scaled);
    if (rounded == -0.0) rounded = 0.0;
    return (int64_t)rounded;
}

static void fnv1a_64_update(uint64_t *hash, const void *data, size_t size) {
    const uint8_t *bytes = (const uint8_t *)data;
    const uint64_t prime = 1099511628211ULL;
    for (size_t i = 0; i < size; i++) {
        *hash ^= bytes[i];
        *hash *= prime;
    }
}

static int zcc_parse_double(const char **p, double *out) {
    skip_spaces_and_commas(p);
    if (!is_number_start(**p)) return 0;

    char *end = NULL;
    errno = 0;
    double v = strtod(*p, &end);
    if (end == *p || errno == ERANGE) return 0;

    *out = v;
    *p = end;
    return 1;
}

static int zcc_is_valid_coord(double v) {
    return isfinite(v) && (fabs(v) <= ZCC_VIR_MAX_COORD_ABS);
}

void vir_path_invalidate_bounds(VirPath *path) {
    if (path) {
        path->bounds_valid = 0;
        path->state_flags &= ~(VIR_STATE_BOUNDS_VALID | VIR_STATE_EXACT_BOUNDS);
        path->min_x = 0.0f;
        path->min_y = 0.0f;
        path->max_x = 0.0f;
        path->max_y = 0.0f;
    }
}

VirPath* vir_path_create(void) {
    VirPath *p = (VirPath*)calloc(1, sizeof(VirPath));
    p->capacity = 16;
    p->segments = (VirSegment*)calloc(p->capacity, sizeof(VirSegment));
    p->count = 0;
    p->bounds_valid = 0;
    return p;
}

void vir_path_free(VirPath *path) {
    if (!path) return;
    if (path->segments) free(path->segments);
    free(path);
}

static int vir_path_ensure_capacity(VirPath *path) {
    if (path->count >= path->capacity) {
        size_t new_cap = path->capacity * 2;
        if (new_cap > ZCC_VIR_MAX_SEGMENTS) {
            return 0;
        }
        VirSegment *new_segs = (VirSegment*)realloc(path->segments, new_cap * sizeof(VirSegment));
        if (!new_segs) return 0;
        path->segments = new_segs;
        path->capacity = new_cap;
    }
    return 1;
}

int vir_path_add_move_to(VirPath *path, float x, float y) {
    if (!vir_path_ensure_capacity(path)) return 0;
    VirSegment *s = &path->segments[path->count++];
    s->op = VIR_MOVE;
    s->coords[0] = x;
    s->coords[1] = y;
    vir_path_invalidate_bounds(path);
    return 1;
}

int vir_path_add_line_to(VirPath *path, float x, float y) {
    if (!vir_path_ensure_capacity(path)) return 0;
    VirSegment *s = &path->segments[path->count++];
    s->op = VIR_LINE;
    s->coords[0] = x;
    s->coords[1] = y;
    vir_path_invalidate_bounds(path);
    return 1;
}

int vir_path_add_cubic_to(VirPath *path, float x1, float y1, float x2, float y2, float x, float y) {
    if (!vir_path_ensure_capacity(path)) return 0;
    VirSegment *s = &path->segments[path->count++];
    s->op = VIR_CUBIC;
    s->coords[0] = x1;
    s->coords[1] = y1;
    s->coords[2] = x2;
    s->coords[3] = y2;
    s->coords[4] = x;
    s->coords[5] = y;
    vir_path_invalidate_bounds(path);
    return 1;
}

int vir_path_add_arc_to(VirPath *path, float rx, float ry, float rotx, float fa, float fs, float x, float y) {
    if (!vir_path_ensure_capacity(path)) return 0;
    VirSegment *s = &path->segments[path->count++];
    s->op = VIR_ARC;
    s->coords[0] = rx;
    s->coords[1] = ry;
    s->coords[2] = rotx;
    s->coords[3] = fa;
    s->coords[4] = fs;
    s->coords[5] = x;
    s->coords[6] = y;
    vir_path_invalidate_bounds(path);
    return 1;
}

int vir_path_add_close(VirPath *path) {
    if (!vir_path_ensure_capacity(path)) return 0;
    VirSegment *s = &path->segments[path->count++];
    s->op = VIR_CLOSE;
    vir_path_invalidate_bounds(path);
    return 1;
}

VirPassResult vir_path_optimize_degenerate(VirPath *path) {
    if (!path) return VIR_PASS_ERROR;
    if (path->count == 0) return VIR_PASS_NO_CHANGE;

    size_t write_idx = 0;
    float cx = 0.0f, cy = 0.0f;
    float start_x = 0.0f, start_y = 0.0f;
    int changed = 0;

    for (size_t i = 0; i < path->count; i++) {
        VirSegment *s = &path->segments[i];
        int keep = 1;

        if (s->op == VIR_MOVE) {
            cx = s->coords[0];
            cy = s->coords[1];
            start_x = cx;
            start_y = cy;
        } else if (s->op == VIR_LINE) {
            float target_x = s->coords[0];
            float target_y = s->coords[1];
            if (fabsf(target_x - cx) < 1e-5f && fabsf(target_y - cy) < 1e-5f) {
                keep = 0;
                changed = 1;
            } else {
                cx = target_x;
                cy = target_y;
            }
        } else if (s->op == VIR_CUBIC) {
            float x1 = s->coords[0], y1 = s->coords[1];
            float x2 = s->coords[2], y2 = s->coords[3];
            float target_x = s->coords[4];
            float target_y = s->coords[5];
            if (fabsf(x1 - cx) < 1e-5f && fabsf(y1 - cy) < 1e-5f &&
                fabsf(x2 - cx) < 1e-5f && fabsf(y2 - cy) < 1e-5f &&
                fabsf(target_x - cx) < 1e-5f && fabsf(target_y - cy) < 1e-5f) {
                keep = 0;
                changed = 1;
            } else {
                cx = target_x;
                cy = target_y;
            }
        } else if (s->op == VIR_CLOSE) {
            cx = start_x;
            cy = start_y;
        }

        if (keep) {
            if (write_idx != i) {
                path->segments[write_idx] = *s;
            }
            write_idx++;
        }
    }
    if (changed) {
        path->count = write_idx;
        vir_path_invalidate_bounds(path);
    }
    return changed ? VIR_PASS_OK : VIR_PASS_NO_CHANGE;
}

void vir_path_compute_bounds(const VirPath *path, float *min_x, float *min_y, float *max_x, float *max_y) {
    if (!path || path->count == 0 || !min_x || !min_y || !max_x || !max_y) {
        if (min_x) *min_x = 0;
        if (min_y) *min_y = 0;
        if (max_x) *max_x = 0;
        if (max_y) *max_y = 0;
        return;
    }

    if (path->bounds_valid) {
        *min_x = path->min_x;
        *min_y = path->min_y;
        *max_x = path->max_x;
        *max_y = path->max_y;
        return;
    }

    float mix = 1e9f, miy = 1e9f;
    float max_val_x = -1e9f, max_val_y = -1e9f;
    int has_points = 0;

    #define UPDATE_BOUNDS(x, y) do { \
        if ((x) < mix) mix = (x); \
        if ((y) < miy) miy = (y); \
        if ((x) > max_val_x) max_val_x = (x); \
        if ((y) > max_val_y) max_val_y = (y); \
        has_points = 1; \
    } while(0)

    for (size_t i = 0; i < path->count; i++) {
        const VirSegment *s = &path->segments[i];
        if (s->op == VIR_MOVE || s->op == VIR_LINE) {
            UPDATE_BOUNDS(s->coords[0], s->coords[1]);
        } else if (s->op == VIR_CUBIC) {
            UPDATE_BOUNDS(s->coords[0], s->coords[1]);
            UPDATE_BOUNDS(s->coords[2], s->coords[3]);
            UPDATE_BOUNDS(s->coords[4], s->coords[5]);
        } else if (s->op == VIR_ARC) {
            UPDATE_BOUNDS(s->coords[5], s->coords[6]);
        }
    }
    #undef UPDATE_BOUNDS

    float final_min_x = 0.0f, final_min_y = 0.0f, final_max_x = 0.0f, final_max_y = 0.0f;
    if (has_points) {
        final_min_x = mix;
        final_min_y = miy;
        final_max_x = max_val_x;
        final_max_y = max_val_y;
    }

    VirPath *mutable_path = (VirPath*)path;
    mutable_path->min_x = final_min_x;
    mutable_path->min_y = final_min_y;
    mutable_path->max_x = final_max_x;
    mutable_path->max_y = final_max_y;
    mutable_path->bounds_valid = 1;
    mutable_path->state_flags |= VIR_STATE_BOUNDS_VALID;

    *min_x = final_min_x;
    *min_y = final_min_y;
    *max_x = final_max_x;
    *max_y = final_max_y;
}

static float nsvg_sqr(float x) { return x * x; }
static float nsvg_vmag(float x, float y) { return sqrtf(x * x + y * y); }
static float nsvg_vecrat(float ux, float uy, float vx, float vy) {
    float mag_u = nsvg_vmag(ux, uy);
    float mag_v = nsvg_vmag(vx, vy);
    if (mag_u < 1e-6f || mag_v < 1e-6f) return 1.0f;
    return (ux * vx + uy * vy) / (mag_u * mag_v);
}
static float nsvg_vecang(float ux, float uy, float vx, float vy) {
    float r = nsvg_vecrat(ux, uy, vx, vy);
    if (r < -1.0f) r = -1.0f;
    if (r > 1.0f) r = 1.0f;
    return ((ux * vy < uy * vx) ? -1.0f : 1.0f) * acosf(r);
}

static void nsvg_xformPoint(float* dx, float* dy, float x, float y, float* t) {
    *dx = x * t[0] + y * t[2] + t[4];
    *dy = x * t[1] + y * t[3] + t[5];
}
static void nsvg_xformVec(float* dx, float* dy, float x, float y, float* t) {
    *dx = x * t[0] + y * t[2];
    *dy = x * t[1] + y * t[3];
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void emit_arc_generic(
    void *target,
    int is_builder,
    float cx, float cy,
    float rx, float ry,
    float rotx,
    float fa, float fs,
    float x2, float y2
) {
    float x1 = cx;
    float y1 = cy;
    float dx = x1 - x2;
    float dy = y1 - y2;
    float d = sqrtf(dx * dx + dy * dy);

    if (d < 1e-6f || rx < 1e-6f || ry < 1e-6f) {
        if (is_builder) {
            svg_path_line_to((SvgPathBuilder*)target, x2, y2);
        } else {
            vir_path_add_line_to((VirPath*)target, x2, y2);
        }
        return;
    }

    float rotx_rad = rotx / 180.0f * (float)M_PI;
    float sinrx = sinf(rotx_rad);
    float cosrx = cosf(rotx_rad);

    float x1p = cosrx * dx / 2.0f + sinrx * dy / 2.0f;
    float y1p = -sinrx * dx / 2.0f + cosrx * dy / 2.0f;
    float lambda = nsvg_sqr(x1p) / nsvg_sqr(rx) + nsvg_sqr(y1p) / nsvg_sqr(ry);

    if (lambda > 1.0f) {
        float s_val = sqrtf(lambda);
        rx *= s_val;
        ry *= s_val;
    }

    float sa = nsvg_sqr(rx) * nsvg_sqr(ry) - nsvg_sqr(rx) * nsvg_sqr(y1p) - nsvg_sqr(ry) * nsvg_sqr(x1p);
    float sb = nsvg_sqr(rx) * nsvg_sqr(y1p) + nsvg_sqr(ry) * nsvg_sqr(x1p);
    float s_val = 0.0f;
    if (sa < 0.0f) sa = 0.0f;
    if (sb > 0.0f) s_val = sqrtf(sa / sb);
    if (fa == fs) s_val = -s_val;
    float cxp = s_val * rx * y1p / ry;
    float cyp = s_val * -ry * x1p / rx;

    float center_x = (x1 + x2) / 2.0f + cosrx * cxp - sinrx * cyp;
    float center_y = (y1 + y2) / 2.0f + sinrx * cxp + cosrx * cyp;

    float ux = (x1p - cxp) / rx;
    float uy = (y1p - cyp) / ry;
    float vx = (-x1p - cxp) / rx;
    float vy = (-y1p - cyp) / ry;
    float a1 = nsvg_vecang(1.0f, 0.0f, ux, uy);
    float da = nsvg_vecang(ux, uy, vx, vy);

    if (fs == 0.0f && da > 0.0f) {
        da -= 2.0f * (float)M_PI;
    } else if (fs == 1.0f && da < 0.0f) {
        da += 2.0f * (float)M_PI;
    }

    float t[6];
    t[0] = cosrx; t[1] = sinrx;
    t[2] = -sinrx; t[3] = cosrx;
    t[4] = center_x; t[5] = center_y;

    int ndivs = (int)(fabsf(da) / ((float)M_PI * 0.5f) + 1.0f);
    float hda = (da / (float)ndivs) / 2.0f;
    if (hda < 1e-3f && hda > -1e-3f) {
        hda *= 0.5f;
    } else {
        hda = (1.0f - cosf(hda)) / sinf(hda);
    }
    float kappa = fabsf(4.0f / 3.0f * hda);
    if (da < 0.0f) kappa = -kappa;

    float px = x1, py = y1;
    float ptanx = 0.0f, ptany = 0.0f;

    for (int i = 0; i <= ndivs; i++) {
        float a = a1 + da * ((float)i / (float)ndivs);
        float c_a = cosf(a);
        float s_a = sinf(a);
        float x, y, tanx, tany;
        nsvg_xformPoint(&x, &y, c_a * rx, s_a * ry, t);
        nsvg_xformVec(&tanx, &tany, -s_a * rx * kappa, c_a * ry * kappa, t);
        if (i > 0) {
            if (is_builder) {
                svg_path_cubic_to((SvgPathBuilder*)target, px + ptanx, py + ptany, x - tanx, y - tany, x, y);
            } else {
                vir_path_add_cubic_to((VirPath*)target, px + ptanx, py + ptany, x - tanx, y - tany, x, y);
            }
        }
        px = x;
        py = y;
        ptanx = tanx;
        ptany = tany;
    }
}

VirPassResult vir_path_expand_arcs(VirPath *path) {
    if (!path) return VIR_PASS_ERROR;
    if (path->count == 0) return VIR_PASS_NO_CHANGE;

    int has_arcs = 0;
    for (size_t i = 0; i < path->count; i++) {
        if (path->segments[i].op == VIR_ARC) {
            has_arcs = 1;
            break;
        }
    }
    if (!has_arcs) return VIR_PASS_NO_CHANGE;

    VirPath *new_path = vir_path_create();
    if (!new_path) return VIR_PASS_ERROR;
    new_path->metadata = path->metadata;

    float cx = 0.0f, cy = 0.0f;
    float start_x = 0.0f, start_y = 0.0f;

    for (size_t i = 0; i < path->count; i++) {
        VirSegment *s = &path->segments[i];
        if (s->op == VIR_ARC) {
            float rx = s->coords[0];
            float ry = s->coords[1];
            float rotx = s->coords[2];
            float fa = s->coords[3];
            float fs = s->coords[4];
            float x2 = s->coords[5];
            float y2 = s->coords[6];

            emit_arc_generic(new_path, 0, cx, cy, rx, ry, rotx, fa, fs, x2, y2);
            cx = x2;
            cy = y2;
        } else {
            if (s->op == VIR_MOVE) {
                cx = s->coords[0];
                cy = s->coords[1];
                start_x = cx;
                start_y = cy;
                vir_path_add_move_to(new_path, cx, cy);
            } else if (s->op == VIR_LINE) {
                cx = s->coords[0];
                cy = s->coords[1];
                vir_path_add_line_to(new_path, cx, cy);
            } else if (s->op == VIR_CUBIC) {
                cx = s->coords[4];
                cy = s->coords[5];
                vir_path_add_cubic_to(new_path, s->coords[0], s->coords[1], s->coords[2], s->coords[3], cx, cy);
            } else if (s->op == VIR_CLOSE) {
                cx = start_x;
                cy = start_y;
                vir_path_add_close(new_path);
            }
        }
    }

    VirSegment *tmp_segs = path->segments;
    path->segments = new_path->segments;
    new_path->segments = tmp_segs;

    size_t tmp_count = path->count;
    path->count = new_path->count;
    new_path->count = tmp_count;

    size_t tmp_cap = path->capacity;
    path->capacity = new_path->capacity;
    new_path->capacity = tmp_cap;

    vir_path_free(new_path);
    vir_path_invalidate_bounds(path);
    return VIR_PASS_OK;
}

void vir_path_to_builder(const VirPath *path, SvgPathBuilder *out) {
    if (!path || !out) return;
    float cx = 0.0f, cy = 0.0f;
    float start_x = 0.0f, start_y = 0.0f;

    for (size_t i = 0; i < path->count; i++) {
        const VirSegment *s = &path->segments[i];
        if (s->op == VIR_MOVE) {
            cx = s->coords[0];
            cy = s->coords[1];
            start_x = cx;
            start_y = cy;
            svg_path_move_to(out, cx, cy);
        } else if (s->op == VIR_LINE) {
            cx = s->coords[0];
            cy = s->coords[1];
            svg_path_line_to(out, cx, cy);
        } else if (s->op == VIR_CUBIC) {
            cx = s->coords[4];
            cy = s->coords[5];
            svg_path_cubic_to(out, s->coords[0], s->coords[1], s->coords[2], s->coords[3], cx, cy);
        } else if (s->op == VIR_ARC) {
            float rx = s->coords[0];
            float ry = s->coords[1];
            float rotx = s->coords[2];
            float fa = s->coords[3];
            float fs = s->coords[4];
            float x2 = s->coords[5];
            float y2 = s->coords[6];

            emit_arc_generic(out, 1, cx, cy, rx, ry, rotx, fa, fs, x2, y2);
            cx = x2;
            cy = y2;
        } else if (s->op == VIR_CLOSE) {
            cx = start_x;
            cy = start_y;
            svg_path_close(out);
        }
    }
}

ZccSvgStatus zcc_svg_parse_to_vir(const char *d, VirPath *out, ZccSvgError *err) {
    if (!d || !out) {
        return zcc_svg_fail(err, ZCC_SVG_ERR_NULL_INPUT, "Null input parameters", 0);
    }

    const char *p = d;
    char cmd = '\0';
    double cx = 0.0, cy = 0.0;
    double start_x = 0.0, start_y = 0.0;
    size_t segment_count = 0;

    while (*p != '\0') {
        skip_spaces_and_commas(&p);
        if (*p == '\0') break;

        const char *cmd_ptr = p;
        char c = *p;
        // Identify new command character
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            cmd = c;
            p++;
            skip_spaces_and_commas(&p);
        }

        if (cmd == '\0') {
            return zcc_svg_fail(err, ZCC_SVG_ERR_BAD_COMMAND, "Expected command letter", ZCC_SVG_OFFSET(d, cmd_ptr));
        }

        if (++segment_count > ZCC_VIR_MAX_SEGMENTS) {
            return zcc_svg_fail(err, ZCC_SVG_ERR_PATH_OVERFLOW, "Maximum segment count exceeded", ZCC_SVG_OFFSET(d, cmd_ptr));
        }

        // Execute Parser Command State Machine
        if (cmd == 'M' || cmd == 'm') {
            double x, y;
            if (!zcc_parse_double(&p, &x) || !zcc_parse_double(&p, &y)) {
                return zcc_svg_fail(err, ZCC_SVG_ERR_BAD_NUMBER, "Expected coordinate pair for moveto", ZCC_SVG_OFFSET(d, p));
            }
            if (!zcc_is_valid_coord(x) || !zcc_is_valid_coord(y)) {
                return zcc_svg_fail(err, ZCC_SVG_ERR_BAD_NUMBER, "Coordinate not finite or out of range", ZCC_SVG_OFFSET(d, p));
            }
            if (cmd == 'm') {
                cx += x;
                cy += y;
            } else {
                cx = x;
                cy = y;
            }
            if (!zcc_is_valid_coord(cx) || !zcc_is_valid_coord(cy)) {
                return zcc_svg_fail(err, ZCC_SVG_ERR_BAD_NUMBER, "Coordinate overflow", ZCC_SVG_OFFSET(d, p));
            }
            start_x = cx;
            start_y = cy;
            if (!vir_path_add_move_to(out, (float)cx, (float)cy)) {
                return zcc_svg_fail(err, ZCC_SVG_ERR_PATH_OVERFLOW, "VirPath capacity exceeded", ZCC_SVG_OFFSET(d, p));
            }
            
            // Standard SVG Rule: Subsequent coordinate pairs are implicit linetos
            cmd = (cmd == 'm') ? 'l' : 'L';

        } else if (cmd == 'L' || cmd == 'l') {
            double x, y;
            if (!zcc_parse_double(&p, &x) || !zcc_parse_double(&p, &y)) {
                return zcc_svg_fail(err, ZCC_SVG_ERR_BAD_NUMBER, "Expected coordinate pair for lineto", ZCC_SVG_OFFSET(d, p));
            }
            if (!zcc_is_valid_coord(x) || !zcc_is_valid_coord(y)) {
                return zcc_svg_fail(err, ZCC_SVG_ERR_BAD_NUMBER, "Coordinate not finite or out of range", ZCC_SVG_OFFSET(d, p));
            }
            if (cmd == 'l') {
                cx += x;
                cy += y;
            } else {
                cx = x;
                cy = y;
            }
            if (!zcc_is_valid_coord(cx) || !zcc_is_valid_coord(cy)) {
                return zcc_svg_fail(err, ZCC_SVG_ERR_BAD_NUMBER, "Coordinate overflow", ZCC_SVG_OFFSET(d, p));
            }
            if (!vir_path_add_line_to(out, (float)cx, (float)cy)) {
                return zcc_svg_fail(err, ZCC_SVG_ERR_PATH_OVERFLOW, "VirPath capacity exceeded", ZCC_SVG_OFFSET(d, p));
            }

        } else if (cmd == 'H' || cmd == 'h') {
            double x;
            if (!zcc_parse_double(&p, &x)) {
                return zcc_svg_fail(err, ZCC_SVG_ERR_BAD_NUMBER, "Expected coordinate for horizontal lineto", ZCC_SVG_OFFSET(d, p));
            }
            if (!zcc_is_valid_coord(x)) {
                return zcc_svg_fail(err, ZCC_SVG_ERR_BAD_NUMBER, "Coordinate not finite or out of range", ZCC_SVG_OFFSET(d, p));
            }
            if (cmd == 'h') {
                cx += x;
            } else {
                cx = x;
            }
            if (!zcc_is_valid_coord(cx)) {
                return zcc_svg_fail(err, ZCC_SVG_ERR_BAD_NUMBER, "Coordinate overflow", ZCC_SVG_OFFSET(d, p));
            }
            if (!vir_path_add_line_to(out, (float)cx, (float)cy)) {
                return zcc_svg_fail(err, ZCC_SVG_ERR_PATH_OVERFLOW, "VirPath capacity exceeded", ZCC_SVG_OFFSET(d, p));
            }

        } else if (cmd == 'V' || cmd == 'v') {
            double y;
            if (!zcc_parse_double(&p, &y)) {
                return zcc_svg_fail(err, ZCC_SVG_ERR_BAD_NUMBER, "Expected coordinate for vertical lineto", ZCC_SVG_OFFSET(d, p));
            }
            if (!zcc_is_valid_coord(y)) {
                return zcc_svg_fail(err, ZCC_SVG_ERR_BAD_NUMBER, "Coordinate not finite or out of range", ZCC_SVG_OFFSET(d, p));
            }
            if (cmd == 'v') {
                cy += y;
            } else {
                cy = y;
            }
            if (!zcc_is_valid_coord(cy)) {
                return zcc_svg_fail(err, ZCC_SVG_ERR_BAD_NUMBER, "Coordinate overflow", ZCC_SVG_OFFSET(d, p));
            }
            if (!vir_path_add_line_to(out, (float)cx, (float)cy)) {
                return zcc_svg_fail(err, ZCC_SVG_ERR_PATH_OVERFLOW, "VirPath capacity exceeded", ZCC_SVG_OFFSET(d, p));
            }

        } else if (cmd == 'C' || cmd == 'c') {
            double x1, y1, x2, y2, x, y;
            if (!zcc_parse_double(&p, &x1) || !zcc_parse_double(&p, &y1) ||
                !zcc_parse_double(&p, &x2) || !zcc_parse_double(&p, &y2) ||
                !zcc_parse_double(&p, &x)  || !zcc_parse_double(&p, &y)) {
                return zcc_svg_fail(err, ZCC_SVG_ERR_BAD_NUMBER, "Expected 6 coordinates for curveto", ZCC_SVG_OFFSET(d, p));
            }
            if (!zcc_is_valid_coord(x1) || !zcc_is_valid_coord(y1) ||
                !zcc_is_valid_coord(x2) || !zcc_is_valid_coord(y2) ||
                !zcc_is_valid_coord(x)  || !zcc_is_valid_coord(y)) {
                return zcc_svg_fail(err, ZCC_SVG_ERR_BAD_NUMBER, "Coordinate not finite or out of range", ZCC_SVG_OFFSET(d, p));
            }
            if (cmd == 'c') {
                x1 += cx; y1 += cy;
                x2 += cx; y2 += cy;
                x  += cx; y  += cy;
            }
            if (!zcc_is_valid_coord(x1) || !zcc_is_valid_coord(y1) ||
                !zcc_is_valid_coord(x2) || !zcc_is_valid_coord(y2) ||
                !zcc_is_valid_coord(x)  || !zcc_is_valid_coord(y)) {
                return zcc_svg_fail(err, ZCC_SVG_ERR_BAD_NUMBER, "Coordinate overflow", ZCC_SVG_OFFSET(d, p));
            }
            if (!vir_path_add_cubic_to(out, (float)x1, (float)y1, (float)x2, (float)y2, (float)x, (float)y)) {
                return zcc_svg_fail(err, ZCC_SVG_ERR_PATH_OVERFLOW, "VirPath capacity exceeded", ZCC_SVG_OFFSET(d, p));
            }
            cx = x;
            cy = y;

        } else if (cmd == 'Q' || cmd == 'q') {
            double x1, y1, x, y;
            if (!zcc_parse_double(&p, &x1) || !zcc_parse_double(&p, &y1) ||
                !zcc_parse_double(&p, &x)  || !zcc_parse_double(&p, &y)) {
                return zcc_svg_fail(err, ZCC_SVG_ERR_BAD_NUMBER, "Expected 4 coordinates for quadratic curveto", ZCC_SVG_OFFSET(d, p));
            }
            if (!zcc_is_valid_coord(x1) || !zcc_is_valid_coord(y1) ||
                !zcc_is_valid_coord(x)  || !zcc_is_valid_coord(y)) {
                return zcc_svg_fail(err, ZCC_SVG_ERR_BAD_NUMBER, "Coordinate not finite or out of range", ZCC_SVG_OFFSET(d, p));
            }
            if (cmd == 'q') {
                x1 += cx; y1 += cy;
                x  += cx; y  += cy;
            }
            if (!zcc_is_valid_coord(x1) || !zcc_is_valid_coord(y1) ||
                !zcc_is_valid_coord(x)  || !zcc_is_valid_coord(y)) {
                return zcc_svg_fail(err, ZCC_SVG_ERR_BAD_NUMBER, "Coordinate overflow", ZCC_SVG_OFFSET(d, p));
            }
            
            // Mathematically elevate Quadratic spline to Cubic Bezier
            double c1x = cx + (2.0 / 3.0) * (x1 - cx);
            double c1y = cy + (2.0 / 3.0) * (y1 - cy);
            double c2x = x  + (2.0 / 3.0) * (x1 - x);
            double c2y = y  + (2.0 / 3.0) * (y1 - y);

            if (!vir_path_add_cubic_to(out, (float)c1x, (float)c1y, (float)c2x, (float)c2y, (float)x, (float)y)) {
                return zcc_svg_fail(err, ZCC_SVG_ERR_PATH_OVERFLOW, "VirPath capacity exceeded", ZCC_SVG_OFFSET(d, p));
            }
            cx = x;
            cy = y;

        } else if (cmd == 'Z' || cmd == 'z') {
            if (!vir_path_add_close(out)) {
                return zcc_svg_fail(err, ZCC_SVG_ERR_PATH_OVERFLOW, "VirPath capacity exceeded", ZCC_SVG_OFFSET(d, p));
            }
            cx = start_x;
            cy = start_y;
            cmd = '\0'; // Require explicit command next

        } else if (cmd == 'A' || cmd == 'a') {
            double rx, ry, rotx, fa, fs, x, y;
            if (!zcc_parse_double(&p, &rx) || !zcc_parse_double(&p, &ry) ||
                !zcc_parse_double(&p, &rotx) || !zcc_parse_double(&p, &fa) ||
                !zcc_parse_double(&p, &fs) || !zcc_parse_double(&p, &x) ||
                !zcc_parse_double(&p, &y)) {
                return zcc_svg_fail(err, ZCC_SVG_ERR_BAD_NUMBER, "Expected 7 parameters for arc command", ZCC_SVG_OFFSET(d, p));
            }
            if (!zcc_is_valid_coord(rx) || !zcc_is_valid_coord(ry) ||
                !zcc_is_valid_coord(rotx) || !zcc_is_valid_coord(fa) ||
                !zcc_is_valid_coord(fs) || !zcc_is_valid_coord(x) ||
                !zcc_is_valid_coord(y)) {
                return zcc_svg_fail(err, ZCC_SVG_ERR_BAD_NUMBER, "Coordinate not finite or out of range", ZCC_SVG_OFFSET(d, p));
            }
            if (cmd == 'a') {
                x += cx;
                y += cy;
            }
            if (!zcc_is_valid_coord(x) || !zcc_is_valid_coord(y)) {
                return zcc_svg_fail(err, ZCC_SVG_ERR_BAD_NUMBER, "Coordinate overflow", ZCC_SVG_OFFSET(d, p));
            }

            if (!vir_path_add_arc_to(out, (float)rx, (float)ry, (float)rotx, (float)fa, (float)fs, (float)x, (float)y)) {
                return zcc_svg_fail(err, ZCC_SVG_ERR_PATH_OVERFLOW, "VirPath capacity exceeded", ZCC_SVG_OFFSET(d, p));
            }
            cx = x;
            cy = y;

        } else if (cmd == 'S' || cmd == 's' || cmd == 'T' || cmd == 't') {
            return zcc_svg_fail(err, ZCC_SVG_ERR_UNSUPPORTED_COMMAND,
                "Unsupported SVG smooth curve command", ZCC_SVG_OFFSET(d, cmd_ptr));

        } else {
            return zcc_svg_fail(err, ZCC_SVG_ERR_BAD_COMMAND, "Unknown SVG command", ZCC_SVG_OFFSET(d, cmd_ptr));
        }
    }

    if (err) {
        err->status = ZCC_SVG_OK;
        err->message = "Success";
        err->offset = ZCC_SVG_OFFSET(d, p);
    }
    return ZCC_SVG_OK;
}

char* vir_to_svg_path_data(const VirPath *path) {
    if (!path) return NULL;
    size_t capacity = path->count * 128 + 32;
    char *buf = (char*)malloc(capacity);
    if (!buf) return NULL;
    buf[0] = '\0';
    size_t len = 0;

    for (size_t i = 0; i < path->count; i++) {
        const VirSegment *s = &path->segments[i];
        char temp[256];
        int n = 0;
        if (s->op == VIR_MOVE) {
            n = sprintf(temp, "M%.2f,%.2f ", s->coords[0], s->coords[1]);
        } else if (s->op == VIR_LINE) {
            n = sprintf(temp, "L%.2f,%.2f ", s->coords[0], s->coords[1]);
        } else if (s->op == VIR_CUBIC) {
            n = sprintf(temp, "C%.2f,%.2f %.2f,%.2f %.2f,%.2f ",
                s->coords[0], s->coords[1],
                s->coords[2], s->coords[3],
                s->coords[4], s->coords[5]);
        } else if (s->op == VIR_ARC) {
            n = sprintf(temp, "A%.2f,%.2f %.2f %.0f %.0f %.2f,%.2f ",
                s->coords[0], s->coords[1],
                s->coords[2],
                s->coords[3], s->coords[4],
                s->coords[5], s->coords[6]);
        } else if (s->op == VIR_CLOSE) {
            n = sprintf(temp, "Z ");
        }

        if (n > 0) {
            if (len + n >= capacity) {
                capacity = (len + n) * 2;
                char *new_buf = (char*)realloc(buf, capacity);
                if (!new_buf) {
                    free(buf);
                    return NULL;
                }
                buf = new_buf;
            }
            strcpy(buf + len, temp);
            len += n;
        }
    }

    if (len > 0 && buf[len - 1] == ' ') {
        buf[len - 1] = '\0';
    }
    return buf;
}

static VirPath* vir_path_clone(const VirPath *src) {
    if (!src) return NULL;
    VirPath *dest = (VirPath*)calloc(1, sizeof(VirPath));
    if (!dest) return NULL;
    dest->capacity = src->count > 0 ? src->count : 16;
    dest->segments = (VirSegment*)calloc(dest->capacity, sizeof(VirSegment));
    if (!dest->segments) {
        free(dest);
        return NULL;
    }
    dest->count = src->count;
    if (src->count > 0) {
        memcpy(dest->segments, src->segments, src->count * sizeof(VirSegment));
    }
    dest->metadata = src->metadata;
    dest->bounds_valid = src->bounds_valid;
    dest->min_x = src->min_x;
    dest->min_y = src->min_y;
    dest->max_x = src->max_x;
    dest->max_y = src->max_y;
    return dest;
}

static int sdf_seed_ensure_capacity(SdfSeed *seed, size_t *capacity) {
    if (seed->count >= *capacity) {
        size_t new_cap = *capacity == 0 ? 16 : *capacity * 2;
        SdfSeedSegment *new_segs = (SdfSeedSegment*)realloc(seed->segments, new_cap * sizeof(SdfSeedSegment));
        if (!new_segs) return 0;
        seed->segments = new_segs;
        *capacity = new_cap;
    }
    return 1;
}

static int sdf_seed_add_line(SdfSeed *seed, size_t *capacity, float x0, float y0, float x1, float y1) {
    if (!sdf_seed_ensure_capacity(seed, capacity)) return 0;
    SdfSeedSegment *seg = &seed->segments[seed->count++];
    seg->op = SDF_LINE;
    seg->points[0] = x0;
    seg->points[1] = y0;
    seg->points[2] = x1;
    seg->points[3] = y1;
    seg->points[4] = 0.0f;
    seg->points[5] = 0.0f;
    seg->points[6] = 0.0f;
    seg->points[7] = 0.0f;
    return 1;
}

static int sdf_seed_add_cubic(SdfSeed *seed, size_t *capacity, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3) {
    if (!sdf_seed_ensure_capacity(seed, capacity)) return 0;
    SdfSeedSegment *seg = &seed->segments[seed->count++];
    seg->op = SDF_CUBIC;
    seg->points[0] = x0;
    seg->points[1] = y0;
    seg->points[2] = x1;
    seg->points[3] = y1;
    seg->points[4] = x2;
    seg->points[5] = y2;
    seg->points[6] = x3;
    seg->points[7] = y3;
    return 1;
}

void sdf_seed_free(SdfSeed *seed);

/* VIR_CACHE_SCHEMA_VERSION is defined in zcc_vir.h */

typedef struct {
    uint64_t key;
    int occupied;
    int has_bounds;
    float min_x;
    float min_y;
    float max_x;
    float max_y;
    SdfSeed *sdf_seed;
    char *glsl_shader;
    
    // Collision hardening metadata
    size_t segment_count;
    uint32_t state_flags;
} VirCacheEntry;

#define VIR_CACHE_SIZE 1024
static VirCacheEntry g_vir_cache[VIR_CACHE_SIZE];
static int g_vir_cache_initialized = 0;
static VirCacheStats g_cache_stats = {0, 0, 0};

void vir_cache_init(void) {
    if (!g_vir_cache_initialized) {
        memset(g_vir_cache, 0, sizeof(g_vir_cache));
        g_vir_cache_initialized = 1;
        memset(&g_cache_stats, 0, sizeof(g_cache_stats));
    }
}

void vir_cache_clear(void) {
    if (!g_vir_cache_initialized) return;
    for (int i = 0; i < VIR_CACHE_SIZE; i++) {
        if (g_vir_cache[i].occupied) {
            if (g_vir_cache[i].sdf_seed) {
                sdf_seed_free(g_vir_cache[i].sdf_seed);
            }
            if (g_vir_cache[i].glsl_shader) {
                free(g_vir_cache[i].glsl_shader);
            }
        }
    }
    memset(g_vir_cache, 0, sizeof(g_vir_cache));
    memset(&g_cache_stats, 0, sizeof(g_cache_stats));
}

void vir_cache_shutdown(void) {
    vir_cache_clear();
    g_vir_cache_initialized = 0;
}

VirCacheStats vir_cache_get_stats(void) {
    return g_cache_stats;
}

void vir_cache_reset_stats(void) {
    memset(&g_cache_stats, 0, sizeof(g_cache_stats));
}

static SdfSeed* sdf_seed_clone(const SdfSeed *src) {
    if (!src) return NULL;
    SdfSeed *dest = (SdfSeed*)malloc(sizeof(SdfSeed));
    if (!dest) return NULL;
    dest->count = src->count;
    dest->segments = (SdfSeedSegment*)calloc(src->count, sizeof(SdfSeedSegment));
    if (!dest->segments) {
        free(dest);
        return NULL;
    }
    memcpy(dest->segments, src->segments, src->count * sizeof(SdfSeedSegment));
    return dest;
}

static VirCacheEntry* cache_find_slot(uint64_t key, size_t segment_count, uint32_t state_flags, int create_if_missing) {
    vir_cache_init();
    size_t idx = (size_t)(key % VIR_CACHE_SIZE);
    size_t start = idx;
    do {
        if (!g_vir_cache[idx].occupied) {
            if (create_if_missing) {
                g_vir_cache[idx].key = key;
                g_vir_cache[idx].occupied = 1;
                g_vir_cache[idx].segment_count = segment_count;
                g_vir_cache[idx].state_flags = state_flags;
                return &g_vir_cache[idx];
            }
            return NULL;
        }
        if (g_vir_cache[idx].key == key &&
            g_vir_cache[idx].segment_count == segment_count &&
            g_vir_cache[idx].state_flags == state_flags) {
            return &g_vir_cache[idx];
        }
        idx = (idx + 1) % VIR_CACHE_SIZE;
    } while (idx != start);

    if (create_if_missing) {
        if (g_vir_cache[start].occupied) {
            g_cache_stats.evictions++;
        }
        if (g_vir_cache[start].sdf_seed) {
            sdf_seed_free(g_vir_cache[start].sdf_seed);
            g_vir_cache[start].sdf_seed = NULL;
        }
        if (g_vir_cache[start].glsl_shader) {
            free(g_vir_cache[start].glsl_shader);
            g_vir_cache[start].glsl_shader = NULL;
        }
        g_vir_cache[start].key = key;
        g_vir_cache[start].segment_count = segment_count;
        g_vir_cache[start].state_flags = state_flags;
        g_vir_cache[start].has_bounds = 0;
        g_vir_cache[start].sdf_seed = NULL;
        g_vir_cache[start].glsl_shader = NULL;
        return &g_vir_cache[start];
    }
    return NULL;
}

SdfSeed* vir_to_sdf_seed(const VirPath *path) {
    if (!path) return NULL;

    uint64_t fp = vir_path_canonical_fingerprint(path, 1e-3f);
    VirCacheEntry *entry = cache_find_slot(fp, path->count, path->state_flags, 0);
    if (entry && entry->sdf_seed) {
        g_cache_stats.hits++;
        return sdf_seed_clone(entry->sdf_seed);
    }
    g_cache_stats.misses++;

    VirPath *expanded = vir_path_clone(path);
    if (!expanded) return NULL;
    vir_path_expand_arcs(expanded);

    SdfSeed *seed = (SdfSeed*)malloc(sizeof(SdfSeed));
    if (!seed) {
        vir_path_free(expanded);
        return NULL;
    }
    seed->count = 0;
    size_t capacity = expanded->count;
    if (capacity < 16) capacity = 16;
    seed->segments = (SdfSeedSegment*)calloc(capacity, sizeof(SdfSeedSegment));
    if (!seed->segments) {
        free(seed);
        vir_path_free(expanded);
        return NULL;
    }

    float cx = 0.0f, cy = 0.0f;
    float start_x = 0.0f, start_y = 0.0f;
    int has_subpath_start = 0;

    for (size_t i = 0; i < expanded->count; i++) {
        const VirSegment *s = &expanded->segments[i];
        if (s->op == VIR_MOVE) {
            cx = s->coords[0];
            cy = s->coords[1];
            start_x = cx;
            start_y = cy;
            has_subpath_start = 1;
        } else if (s->op == VIR_LINE) {
            float tx = s->coords[0];
            float ty = s->coords[1];
            if (has_subpath_start) {
                if (!sdf_seed_add_line(seed, &capacity, cx, cy, tx, ty)) {
                    sdf_seed_free(seed);
                    vir_path_free(expanded);
                    return NULL;
                }
            }
            cx = tx;
            cy = ty;
        } else if (s->op == VIR_CUBIC) {
            float x1 = s->coords[0], y1 = s->coords[1];
            float x2 = s->coords[2], y2 = s->coords[3];
            float tx = s->coords[4], ty = s->coords[5];
            if (has_subpath_start) {
                if (!sdf_seed_add_cubic(seed, &capacity, cx, cy, x1, y1, x2, y2, tx, ty)) {
                    sdf_seed_free(seed);
                    vir_path_free(expanded);
                    return NULL;
                }
            }
            cx = tx;
            cy = ty;
        } else if (s->op == VIR_CLOSE) {
            if (has_subpath_start) {
                if (cx != start_x || cy != start_y) {
                    if (!sdf_seed_add_line(seed, &capacity, cx, cy, start_x, start_y)) {
                        sdf_seed_free(seed);
                        vir_path_free(expanded);
                        return NULL;
                    }
                }
            }
            cx = start_x;
            cy = start_y;
        }
    }

    vir_path_free(expanded);

    entry = cache_find_slot(fp, path->count, path->state_flags, 1);
    if (entry) {
        if (entry->sdf_seed) {
            sdf_seed_free(entry->sdf_seed);
        }
        entry->sdf_seed = sdf_seed_clone(seed);
    }

    return seed;
}

void sdf_seed_free(SdfSeed *seed) {
    if (!seed) return;
    if (seed->segments) free(seed->segments);
    free(seed);
}

SdfBounds sdf_seed_compute_bounds(const SdfSeed *seed) {
    SdfBounds b = {0.0f, 0.0f, 0.0f, 0.0f};
    if (!seed || seed->count == 0) return b;

    float mix = 1e9f, miy = 1e9f;
    float max_val_x = -1e9f, max_val_y = -1e9f;
    int has_points = 0;

    #define UPDATE_BOUNDS(x, y) do { \
        if ((x) < mix) mix = (x); \
        if ((y) < miy) miy = (y); \
        if ((x) > max_val_x) max_val_x = (x); \
        if ((y) > max_val_y) max_val_y = (y); \
        has_points = 1; \
    } while(0)

    for (size_t i = 0; i < seed->count; i++) {
        const SdfSeedSegment *seg = &seed->segments[i];
        if (seg->op == SDF_LINE) {
            UPDATE_BOUNDS(seg->points[0], seg->points[1]);
            UPDATE_BOUNDS(seg->points[2], seg->points[3]);
        } else if (seg->op == SDF_CUBIC) {
            UPDATE_BOUNDS(seg->points[0], seg->points[1]);
            UPDATE_BOUNDS(seg->points[2], seg->points[3]);
            UPDATE_BOUNDS(seg->points[4], seg->points[5]);
            UPDATE_BOUNDS(seg->points[6], seg->points[7]);
        }
    }
    #undef UPDATE_BOUNDS

    if (has_points) {
        b.min_x = mix;
        b.min_y = miy;
        b.max_x = max_val_x;
        b.max_y = max_val_y;
    }
    return b;
}

static uint64_t sdf_seed_fingerprint(const SdfSeed *seed) {
    if (!seed) return 0;
    uint64_t hash = 14695981039346656037ULL;
    uint64_t schema_ver = VIR_CACHE_SCHEMA_VERSION;
    fnv1a_64_update(&hash, &schema_ver, sizeof(schema_ver));
    uint64_t state_val = 0;
    fnv1a_64_update(&hash, &state_val, sizeof(state_val));
    uint64_t count_val = (uint64_t)seed->count;
    fnv1a_64_update(&hash, &count_val, sizeof(count_val));
    for (size_t i = 0; i < seed->count; i++) {
        const SdfSeedSegment *seg = &seed->segments[i];
        uint32_t op_val = (uint32_t)seg->op;
        fnv1a_64_update(&hash, &op_val, sizeof(op_val));

        int num_points = seg->op == SDF_LINE ? 4 : 8;
        for (int c = 0; c < num_points; c++) {
            int64_t q = quantize_coord(seg->points[c], 1e-4f);
            fnv1a_64_update(&hash, &q, sizeof(q));
        }
    }
    return hash;
}

char* sdf_seed_to_glsl(const SdfSeed *seed) {
    if (!seed) return NULL;

    uint64_t fp = sdf_seed_fingerprint(seed);
    VirCacheEntry *entry = cache_find_slot(fp, seed->count, 0, 0);
    if (entry && entry->glsl_shader) {
        g_cache_stats.hits++;
        return strdup(entry->glsl_shader);
    }
    g_cache_stats.misses++;

    size_t capacity = 1024 + seed->count * 256;
    char *buf = (char*)malloc(capacity);
    if (!buf) return NULL;

    buf[0] = '\0';
    size_t len = 0;

    const char *header =
        "// GLSL Signed Distance Field (SDF) representation generated from SdfSeed\n\n"
        "float sdLine(vec2 p, vec2 a, vec2 b) {\n"
        "    vec2 pa = p - a, ba = b - a;\n"
        "    float h = clamp(dot(pa, ba)/dot(ba, ba), 0.0, 1.0);\n"
        "    return length(pa - ba*h);\n"
        "}\n\n"
        "float sdCubicBezier(vec2 p, vec2 p0, vec2 p1, vec2 p2, vec2 p3) {\n"
        "    float d = 1e9;\n"
        "    vec2 prev = p0;\n"
        "    for (int i = 1; i <= 10; i++) {\n"
        "        float t = float(i) / 10.0;\n"
        "        float mt = 1.0 - t;\n"
        "        vec2 curr = mt*mt*mt*p0 + 3.0*mt*mt*t*p1 + 3.0*mt*t*t*p2 + t*t*t*p3;\n"
        "        vec2 pa = p - prev, ba = curr - prev;\n"
        "        float h = clamp(dot(pa, ba)/dot(ba, ba), 0.0, 1.0);\n"
        "        d = min(d, length(pa - ba*h));\n"
        "        prev = curr;\n"
        "    }\n"
        "    return d;\n"
        "}\n\n"
        "float sdf_shape(vec2 p) {\n"
        "    float d = 1e9;\n";

    strcpy(buf, header);
    len = strlen(header);

    for (size_t i = 0; i < seed->count; i++) {
        const SdfSeedSegment *seg = &seed->segments[i];
        char temp[512];
        int n = 0;
        if (seg->op == SDF_LINE) {
            n = sprintf(temp, "    d = min(d, sdLine(p, vec2(%.4f, %.4f), vec2(%.4f, %.4f)));\n",
                seg->points[0], seg->points[1],
                seg->points[2], seg->points[3]);
        } else if (seg->op == SDF_CUBIC) {
            n = sprintf(temp, "    d = min(d, sdCubicBezier(p, vec2(%.4f, %.4f), vec2(%.4f, %.4f), vec2(%.4f, %.4f), vec2(%.4f, %.4f)));\n",
                seg->points[0], seg->points[1],
                seg->points[2], seg->points[3],
                seg->points[4], seg->points[5],
                seg->points[6], seg->points[7]);
        }

        if (n > 0) {
            if (len + n >= capacity) {
                capacity = len + n + 256;
                char *new_buf = (char*)realloc(buf, capacity);
                if (!new_buf) {
                    free(buf);
                    return NULL;
                }
                buf = new_buf;
            }
            strcpy(buf + len, temp);
            len += n;
        }
    }

    const char *footer = "    return d;\n}\n";
    if (len + strlen(footer) >= capacity) {
        capacity = len + strlen(footer) + 16;
        char *new_buf = (char*)realloc(buf, capacity);
        if (!new_buf) {
            free(buf);
            return NULL;
        }
        buf = new_buf;
    }
    strcpy(buf + len, footer);

    entry = cache_find_slot(fp, seed->count, 0, 1);
    if (entry) {
        if (entry->glsl_shader) {
            free(entry->glsl_shader);
        }
        entry->glsl_shader = strdup(buf);
    }

    return buf;
}

VirPassResult vir_path_canonicalize(VirPath *path) {
    if (!path) return VIR_PASS_ERROR;
    if (path->count == 0) return VIR_PASS_NO_CHANGE;

    // First ensure arcs are expanded
    VirPassResult arc_res = vir_path_expand_arcs(path);
    if (arc_res == VIR_PASS_ERROR) return VIR_PASS_ERROR;

    // Check if there are any VIR_LINE segments
    int has_lines = 0;
    for (size_t i = 0; i < path->count; i++) {
        if (path->segments[i].op == VIR_LINE) {
            has_lines = 1;
            break;
        }
    }

    // If no lines and no arc changes, return NO_CHANGE
    if (!has_lines && arc_res == VIR_PASS_NO_CHANGE) {
        return VIR_PASS_NO_CHANGE;
    }

    // Create a new path to collect the canonical cubics
    VirPath *new_path = vir_path_create();
    if (!new_path) return VIR_PASS_ERROR;
    new_path->metadata = path->metadata;

    float cx = 0.0f, cy = 0.0f;
    float start_x = 0.0f, start_y = 0.0f;

    for (size_t i = 0; i < path->count; i++) {
        VirSegment *s = &path->segments[i];
        if (s->op == VIR_MOVE) {
            cx = s->coords[0];
            cy = s->coords[1];
            start_x = cx;
            start_y = cy;
            vir_path_add_move_to(new_path, cx, cy);
        } else if (s->op == VIR_LINE) {
            float tx = s->coords[0];
            float ty = s->coords[1];
            float c1x = cx + (1.0f / 3.0f) * (tx - cx);
            float c1y = cy + (1.0f / 3.0f) * (ty - cy);
            float c2x = cx + (2.0f / 3.0f) * (tx - cx);
            float c2y = cy + (2.0f / 3.0f) * (ty - cy);
            vir_path_add_cubic_to(new_path, c1x, c1y, c2x, c2y, tx, ty);
            cx = tx;
            cy = ty;
        } else if (s->op == VIR_CUBIC) {
            cx = s->coords[4];
            cy = s->coords[5];
            vir_path_add_cubic_to(new_path, s->coords[0], s->coords[1], s->coords[2], s->coords[3], cx, cy);
        } else if (s->op == VIR_CLOSE) {
            cx = start_x;
            cy = start_y;
            vir_path_add_close(new_path);
        }
    }

    VirSegment *tmp_segs = path->segments;
    path->segments = new_path->segments;
    new_path->segments = tmp_segs;

    size_t tmp_count = path->count;
    path->count = new_path->count;
    new_path->count = tmp_count;

    size_t tmp_cap = path->capacity;
    path->capacity = new_path->capacity;
    new_path->capacity = tmp_cap;

    vir_path_free(new_path);
    vir_path_invalidate_bounds(path);
    return VIR_PASS_OK;
}

VirPassResult vir_path_compute_bounds_pass(VirPath *path) {
    if (!path) return VIR_PASS_ERROR;
    if (path->bounds_valid && (path->state_flags & VIR_STATE_BOUNDS_VALID)) return VIR_PASS_NO_CHANGE;
    float min_x, min_y, max_x, max_y;
    vir_path_compute_bounds(path, &min_x, &min_y, &max_x, &max_y);
    return VIR_PASS_OK;
}

static float eval_bezier(float p0, float p1, float p2, float p3, float t) {
    float mt = 1.0f - t;
    return mt * mt * mt * p0 + 3.0f * mt * mt * t * p1 + 3.0f * mt * t * t * p2 + t * t * t * p3;
}

static void solve_bezier_extrema(float p0, float p1, float p2, float p3, float roots[2], int *num_roots) {
    *num_roots = 0;
    float A = 3.0f * (-p0 + 3.0f * p1 - 3.0f * p2 + p3);
    float B = 6.0f * (p0 - 2.0f * p1 + p2);
    float C = 3.0f * (-p0 + p1);

    if (fabsf(A) < 1e-6f) {
        if (fabsf(B) > 1e-6f) {
            float t = -C / B;
            if (t > 0.0f && t < 1.0f) {
                roots[(*num_roots)++] = t;
            }
        }
    } else {
        float disc = B * B - 4.0f * A * C;
        if (disc >= 0.0f) {
            float sqrt_disc = sqrtf(disc);
            float t1 = (-B - sqrt_disc) / (2.0f * A);
            float t2 = (-B + sqrt_disc) / (2.0f * A);
            if (t1 > 0.0f && t1 < 1.0f) {
                roots[(*num_roots)++] = t1;
            }
            if (t2 > 0.0f && t2 < 1.0f) {
                roots[(*num_roots)++] = t2;
            }
        }
    }
}

static uint64_t hash_normalized_path(const VirPath *path) {
    uint64_t hash = 14695981039346656037ULL;
    uint64_t schema_version = VIR_CACHE_SCHEMA_VERSION;
    fnv1a_64_update(&hash, &schema_version, sizeof(schema_version));
    uint64_t state_flags_val = (uint64_t)path->state_flags;
    fnv1a_64_update(&hash, &state_flags_val, sizeof(state_flags_val));
    fnv1a_64_update(&hash, &path->count, sizeof(path->count));
    for (size_t i = 0; i < path->count; i++) {
        const VirSegment *seg = &path->segments[i];
        uint32_t op_val = (uint32_t)seg->op;
        fnv1a_64_update(&hash, &op_val, sizeof(op_val));

        int num_coords = 0;
        if (seg->op == VIR_MOVE || seg->op == VIR_LINE) num_coords = 2;
        else if (seg->op == VIR_CUBIC) num_coords = 6;
        else if (seg->op == VIR_ARC) num_coords = 7;

        for (int c = 0; c < num_coords; c++) {
            fnv1a_64_update(&hash, &seg->coords[c], sizeof(seg->coords[c]));
        }
    }
    return hash;
}

VirPassResult vir_path_compute_exact_bounds_pass(VirPath *path) {
    if (!path) return VIR_PASS_ERROR;
    if (path->bounds_valid && (path->state_flags & VIR_STATE_EXACT_BOUNDS)) {
        return VIR_PASS_NO_CHANGE;
    }

    if (path->count == 0) {
        path->min_x = 0.0f;
        path->min_y = 0.0f;
        path->max_x = 0.0f;
        path->max_y = 0.0f;
        path->bounds_valid = 1;
        path->state_flags |= (VIR_STATE_BOUNDS_VALID | VIR_STATE_EXACT_BOUNDS);
        return VIR_PASS_OK;
    }

    uint32_t input_state_flags = path->state_flags;
    uint64_t raw_hash = hash_normalized_path(path);
    VirCacheEntry *entry = cache_find_slot(raw_hash, path->count, input_state_flags, 0);
    if (entry && entry->has_bounds) {
        path->min_x = entry->min_x;
        path->min_y = entry->min_y;
        path->max_x = entry->max_x;
        path->max_y = entry->max_y;
        path->bounds_valid = 1;
        path->state_flags |= (VIR_STATE_BOUNDS_VALID | VIR_STATE_EXACT_BOUNDS);
        g_cache_stats.hits++;
        return VIR_PASS_OK;
    }
    g_cache_stats.misses++;

    float mix = 1e9f, miy = 1e9f;
    float max_val_x = -1e9f, max_val_y = -1e9f;
    int has_points = 0;

    #define UPDATE_BOUNDS(x, y) do { \
        if ((x) < mix) mix = (x); \
        if ((y) < miy) miy = (y); \
        if ((x) > max_val_x) max_val_x = (x); \
        if ((y) > max_val_y) max_val_y = (y); \
        has_points = 1; \
    } while(0)

    float cx = 0.0f, cy = 0.0f;
    float start_x = 0.0f, start_y = 0.0f;

    for (size_t i = 0; i < path->count; i++) {
        VirSegment *s = &path->segments[i];
        if (s->op == VIR_MOVE) {
            cx = s->coords[0];
            cy = s->coords[1];
            start_x = cx;
            start_y = cy;
            UPDATE_BOUNDS(cx, cy);
        } else if (s->op == VIR_LINE) {
            float tx = s->coords[0];
            float ty = s->coords[1];
            UPDATE_BOUNDS(tx, ty);
            cx = tx;
            cy = ty;
        } else if (s->op == VIR_CUBIC) {
            float x1 = s->coords[0], y1 = s->coords[1];
            float x2 = s->coords[2], y2 = s->coords[3];
            float tx = s->coords[4], ty = s->coords[5];

            UPDATE_BOUNDS(cx, cy);
            UPDATE_BOUNDS(tx, ty);

            // Solve X extrema
            float x_roots[2];
            int num_x_roots = 0;
            solve_bezier_extrema(cx, x1, x2, tx, x_roots, &num_x_roots);
            for (int k = 0; k < num_x_roots; k++) {
                float val_x = eval_bezier(cx, x1, x2, tx, x_roots[k]);
                float val_y = eval_bezier(cy, y1, y2, ty, x_roots[k]);
                UPDATE_BOUNDS(val_x, val_y);
            }

            // Solve Y extrema
            float y_roots[2];
            int num_y_roots = 0;
            solve_bezier_extrema(cy, y1, y2, ty, y_roots, &num_y_roots);
            for (int k = 0; k < num_y_roots; k++) {
                float val_x = eval_bezier(cx, x1, x2, tx, y_roots[k]);
                float val_y = eval_bezier(cy, y1, y2, ty, y_roots[k]);
                UPDATE_BOUNDS(val_x, val_y);
            }

            cx = tx;
            cy = ty;
        } else if (s->op == VIR_CLOSE) {
            UPDATE_BOUNDS(start_x, start_y);
            cx = start_x;
            cy = start_y;
        } else if (s->op == VIR_ARC) {
            UPDATE_BOUNDS(s->coords[5], s->coords[6]);
            cx = s->coords[5];
            cy = s->coords[6];
        }
    }
    #undef UPDATE_BOUNDS

    if (has_points) {
        path->min_x = mix;
        path->min_y = miy;
        path->max_x = max_val_x;
        path->max_y = max_val_y;
    } else {
        path->min_x = 0.0f;
        path->min_y = 0.0f;
        path->max_x = 0.0f;
        path->max_y = 0.0f;
    }
    path->bounds_valid = 1;
    path->state_flags |= (VIR_STATE_BOUNDS_VALID | VIR_STATE_EXACT_BOUNDS);

    entry = cache_find_slot(raw_hash, path->count, input_state_flags, 1);
    if (entry) {
        entry->has_bounds = 1;
        entry->min_x = path->min_x;
        entry->min_y = path->min_y;
        entry->max_x = path->max_x;
        entry->max_y = path->max_y;
    }

    return VIR_PASS_OK;
}

VirPassResult vir_path_normalize(VirPath *path) {
    if (!path) return VIR_PASS_ERROR;
    if (path->count == 0) {
        return VIR_PASS_NO_CHANGE;
    }

    // Checking prerequisites: structural normalization requires canonicalized
    if (!(path->state_flags & VIR_STATE_CANONICALIZED)) {
        return VIR_PASS_ERROR;
    }

    if (path->state_flags & VIR_STATE_NORMALIZED) {
        return VIR_PASS_NO_CHANGE;
    }

    VirSegment *new_segs = (VirSegment*)calloc(path->count, sizeof(VirSegment));
    if (!new_segs) return VIR_PASS_ERROR;

    size_t new_count = 0;
    int subpath_has_geometry = 0;
    int pending_move = 0;
    VirSegment pending_move_seg;

    for (size_t i = 0; i < path->count; i++) {
        VirSegment *s = &path->segments[i];
        if (s->op == VIR_MOVE) {
            pending_move_seg = *s;
            pending_move = 1;
        } else if (s->op == VIR_CLOSE) {
            if (subpath_has_geometry) {
                if (new_count > 0 && new_segs[new_count - 1].op == VIR_CLOSE) {
                    continue;
                }
                new_segs[new_count++] = *s;
                subpath_has_geometry = 0;
            }
        } else {
            if (pending_move) {
                new_segs[new_count++] = pending_move_seg;
                pending_move = 0;
                subpath_has_geometry = 0;
            }
            new_segs[new_count++] = *s;
            subpath_has_geometry = 1;
        }
    }

    int mutated = (new_count != path->count);
    if (!mutated) {
        for (size_t i = 0; i < new_count; i++) {
            if (new_segs[i].op != path->segments[i].op ||
                memcmp(new_segs[i].coords, path->segments[i].coords, sizeof(float) * 8) != 0) {
                mutated = 1;
                break;
            }
        }
    }

    if (!mutated) {
        free(new_segs);
        path->state_flags |= VIR_STATE_NORMALIZED;
        return VIR_PASS_NO_CHANGE;
    }

    free(path->segments);
    path->segments = new_segs;
    path->count = new_count;
    path->state_flags |= VIR_STATE_NORMALIZED;
    vir_path_invalidate_bounds(path);

    return VIR_PASS_OK;
}

VirPassResult vir_path_localize(VirPath *path) {
    if (!path) return VIR_PASS_ERROR;
    if (path->count == 0) {
        return VIR_PASS_NO_CHANGE;
    }

    // Localize requires exact bounds
    if (!path->bounds_valid || !(path->state_flags & VIR_STATE_EXACT_BOUNDS)) {
        return VIR_PASS_ERROR;
    }

    float dx = -path->min_x;
    float dy = -path->min_y;

    if (fabsf(dx) < 1e-5f && fabsf(dy) < 1e-5f) {
        if (path->state_flags & VIR_STATE_LOCALIZED) {
            return VIR_PASS_NO_CHANGE;
        }
        path->min_x = 0.0f;
        path->min_y = 0.0f;
        path->state_flags |= VIR_STATE_LOCALIZED;
        return VIR_PASS_OK;
    }

    for (size_t i = 0; i < path->count; i++) {
        VirSegment *s = &path->segments[i];
        switch (s->op) {
            case VIR_MOVE:
            case VIR_LINE:
                s->coords[0] += dx;
                s->coords[1] += dy;
                break;
            case VIR_CUBIC:
                s->coords[0] += dx;
                s->coords[1] += dy;
                s->coords[2] += dx;
                s->coords[3] += dy;
                s->coords[4] += dx;
                s->coords[5] += dy;
                break;
            case VIR_ARC:
                s->coords[5] += dx;
                s->coords[6] += dy;
                break;
            case VIR_CLOSE:
            default:
                break;
        }
    }

    path->min_x = 0.0f;
    path->min_y = 0.0f;
    path->max_x += dx;
    path->max_y += dy;
    path->state_flags |= VIR_STATE_LOCALIZED;

    return VIR_PASS_OK;
}

int vir_paths_equivalent(const VirPath *a, const VirPath *b, float epsilon) {
    if (!a || !b) return 0;

    VirPath *cp_a = vir_path_create();
    VirPath *cp_b = vir_path_create();
    if (!cp_a || !cp_b) {
        if (cp_a) vir_path_free(cp_a);
        if (cp_b) vir_path_free(cp_b);
        return 0;
    }

    for (size_t i = 0; i < a->count; i++) {
        VirSegment s = a->segments[i];
        if (s.op == VIR_MOVE) {
            vir_path_add_move_to(cp_a, s.coords[0], s.coords[1]);
        } else if (s.op == VIR_LINE) {
            vir_path_add_line_to(cp_a, s.coords[0], s.coords[1]);
        } else if (s.op == VIR_CUBIC) {
            vir_path_add_cubic_to(cp_a, s.coords[0], s.coords[1], s.coords[2], s.coords[3], s.coords[4], s.coords[5]);
        } else if (s.op == VIR_ARC) {
            vir_path_add_arc_to(cp_a, s.coords[0], s.coords[1], s.coords[2], s.coords[3], s.coords[4], s.coords[5], s.coords[6]);
        } else if (s.op == VIR_CLOSE) {
            vir_path_add_close(cp_a);
        }
    }
    for (size_t i = 0; i < b->count; i++) {
        VirSegment s = b->segments[i];
        if (s.op == VIR_MOVE) {
            vir_path_add_move_to(cp_b, s.coords[0], s.coords[1]);
        } else if (s.op == VIR_LINE) {
            vir_path_add_line_to(cp_b, s.coords[0], s.coords[1]);
        } else if (s.op == VIR_CUBIC) {
            vir_path_add_cubic_to(cp_b, s.coords[0], s.coords[1], s.coords[2], s.coords[3], s.coords[4], s.coords[5]);
        } else if (s.op == VIR_ARC) {
            vir_path_add_arc_to(cp_b, s.coords[0], s.coords[1], s.coords[2], s.coords[3], s.coords[4], s.coords[5], s.coords[6]);
        } else if (s.op == VIR_CLOSE) {
            vir_path_add_close(cp_b);
        }
    }

    size_t registry_count = 0;
    VirPassDescriptor *registry = vir_pipeline_get_default_registry(&registry_count);

    VirPipelineStats stats_a = {0};
    VirPipelineStats stats_b = {0};
    VirPass target_passes[] = { VIR_PASS_LOCALIZE };

    int ok_a = vir_run_pipeline_with_deps(cp_a, registry, registry_count, target_passes, 1, &stats_a);
    int ok_b = vir_run_pipeline_with_deps(cp_b, registry, registry_count, target_passes, 1, &stats_b);

    if (!ok_a || !ok_b) {
        vir_path_free(cp_a);
        vir_path_free(cp_b);
        return 0;
    }

    if (cp_a->count != cp_b->count) {
        vir_path_free(cp_a);
        vir_path_free(cp_b);
        return 0;
    }

    int equivalent = 1;
    for (size_t i = 0; i < cp_a->count; i++) {
        VirSegment *sa = &cp_a->segments[i];
        VirSegment *sb = &cp_b->segments[i];
        if (sa->op != sb->op) {
            equivalent = 0;
            break;
        }
        int num_coords = 0;
        if (sa->op == VIR_MOVE || sa->op == VIR_LINE) num_coords = 2;
        else if (sa->op == VIR_CUBIC) num_coords = 6;
        else if (sa->op == VIR_ARC) num_coords = 7;

        for (int c = 0; c < num_coords; c++) {
            if (fabsf(sa->coords[c] - sb->coords[c]) > epsilon) {
                equivalent = 0;
                break;
            }
        }
        if (!equivalent) break;
    }

    vir_path_free(cp_a);
    vir_path_free(cp_b);
    return equivalent;
}

uint64_t vir_path_canonical_fingerprint(const VirPath *path, float epsilon) {
    if (!path) return 0;
    if (epsilon <= 0.0f) epsilon = 1e-3f;

    const VirPath *target = path;
    VirPath *cp = NULL;

    if (!(path->state_flags & VIR_STATE_LOCALIZED)) {
        cp = vir_path_create();
        if (!cp) return 0;

        for (size_t i = 0; i < path->count; i++) {
            VirSegment s = path->segments[i];
            if (s.op == VIR_MOVE) {
                vir_path_add_move_to(cp, s.coords[0], s.coords[1]);
            } else if (s.op == VIR_LINE) {
                vir_path_add_line_to(cp, s.coords[0], s.coords[1]);
            } else if (s.op == VIR_CUBIC) {
                vir_path_add_cubic_to(cp, s.coords[0], s.coords[1], s.coords[2], s.coords[3], s.coords[4], s.coords[5]);
            } else if (s.op == VIR_ARC) {
                vir_path_add_arc_to(cp, s.coords[0], s.coords[1], s.coords[2], s.coords[3], s.coords[4], s.coords[5], s.coords[6]);
            } else if (s.op == VIR_CLOSE) {
                vir_path_add_close(cp);
            }
        }

        size_t registry_count = 0;
        VirPassDescriptor *registry = vir_pipeline_get_default_registry(&registry_count);

        VirPipelineStats stats = {0};
        VirPass target_passes[] = { VIR_PASS_LOCALIZE };

        int ok = vir_run_pipeline_with_deps(cp, registry, registry_count, target_passes, 1, &stats);
        if (!ok) {
            vir_path_free(cp);
            return 0;
        }
        target = cp;
    }

    uint64_t hash = 14695981039346656037ULL;
    uint64_t schema_ver = VIR_CACHE_SCHEMA_VERSION;
    fnv1a_64_update(&hash, &schema_ver, sizeof(schema_ver));
    uint64_t count_val = (uint64_t)target->count;
    fnv1a_64_update(&hash, &count_val, sizeof(count_val));
    uint64_t state_val = (uint64_t)target->state_flags;
    fnv1a_64_update(&hash, &state_val, sizeof(state_val));

    for (size_t i = 0; i < target->count; i++) {
        VirSegment *seg = &target->segments[i];
        uint32_t op_val = (uint32_t)seg->op;
        fnv1a_64_update(&hash, &op_val, sizeof(op_val));

        int num_coords = 0;
        if (seg->op == VIR_MOVE || seg->op == VIR_LINE) num_coords = 2;
        else if (seg->op == VIR_CUBIC) num_coords = 6;
        else if (seg->op == VIR_ARC) num_coords = 7;

        for (int c = 0; c < num_coords; c++) {
            int64_t q = quantize_coord(seg->coords[c], epsilon);
            fnv1a_64_update(&hash, &q, sizeof(q));
        }
    }

    if (cp) {
        vir_path_free(cp);
    }
    return hash;
}

uint64_t vir_path_fingerprint(const VirPath *path, float epsilon) {
    return vir_path_canonical_fingerprint(path, epsilon);
}

/* ── Artifact Manifest ───────────────────────────────────────────────────── */

VirArtifactManifest vir_path_manifest(const VirPath *path, float epsilon) {
    VirArtifactManifest m;
    memset(&m, 0, sizeof(m));
    if (!path) return m;

    m.canonical_fingerprint = vir_path_canonical_fingerprint(path, epsilon);
    m.schema_version        = VIR_CACHE_SCHEMA_VERSION;
    m.state_flags           = path->state_flags;
    m.segment_count         = (uint32_t)path->count;

    /* Copy exact bounds only when already computed — non-mutating. */
    if (path->state_flags & VIR_STATE_EXACT_BOUNDS) {
        m.min_x = path->min_x;
        m.min_y = path->min_y;
        m.max_x = path->max_x;
        m.max_y = path->max_y;
    }
    return m;
}

int vir_manifest_verify(const VirPath *path,
                        const VirArtifactManifest *manifest,
                        float epsilon) {
    if (!path || !manifest) return 0;

    VirArtifactManifest live = vir_path_manifest(path, epsilon);

    if (live.canonical_fingerprint != manifest->canonical_fingerprint) return 0;
    if (live.schema_version        != manifest->schema_version)        return 0;
    if (live.state_flags           != manifest->state_flags)           return 0;
    if (live.segment_count         != manifest->segment_count)         return 0;
    /* Bounds only compared when the manifest captured them. */
    if (manifest->state_flags & VIR_STATE_EXACT_BOUNDS) {
        if (live.min_x != manifest->min_x) return 0;
        if (live.min_y != manifest->min_y) return 0;
        if (live.max_x != manifest->max_x) return 0;
        if (live.max_y != manifest->max_y) return 0;
    }
    return 1;
}

/* ── Execution Plan ─────────────────────────────────────────────────────── */

/* Internal: collect passes required to produce a single state flag into out,
 * respecting dependencies recursively.  Mirrors schedule_and_run_pass but is
 * completely read-only — never calls run() and never touches path->state_flags.
 * sim_flags tracks what state flags would be produced by already-collected passes
 * so we avoid collecting duplicates.                                           */
static int collect_deps_for_state(
    uint32_t flag,
    uint32_t *sim_flags,
    const VirPassDescriptor *registry,
    size_t registry_count,
    int *in_stack,
    VirExecutionPlan *out
) {
    /* Already satisfied by original path state or previously collected passes. */
    if (*sim_flags & flag) return 1;

    /* Find the registry entry that produces this flag. */
    const VirPassDescriptor *provider = NULL;
    size_t provider_index = 0;
    for (size_t j = 0; j < registry_count; j++) {
        if (registry[j].produced_state & flag) {
            provider = &registry[j];
            provider_index = j;
            break;
        }
    }
    if (!provider) return 0; /* Unsatisfiable — no producer in registry. */

    /* Cycle guard. */
    if (in_stack[provider_index]) return 0;
    in_stack[provider_index] = 1;

    /* Recurse for each prerequisite the provider requires. */
    if (provider->required_state) {
        uint32_t prereqs = provider->required_state & ~(*sim_flags);
        for (size_t bit = 0; bit < 32; bit++) {
            uint32_t prereq_flag = 1U << bit;
            if (prereqs & prereq_flag) {
                if (!collect_deps_for_state(prereq_flag, sim_flags,
                                            registry, registry_count,
                                            in_stack, out)) {
                    in_stack[provider_index] = 0;
                    return 0;
                }
            }
        }
    }

    /* Append this pass if it hasn't already been added. */
    int already = 0;
    for (size_t k = 0; k < out->count; k++) {
        if (out->passes[k] == provider->pass_id) { already = 1; break; }
    }
    if (!already) {
        if (out->count >= VIR_EXECUTION_PLAN_MAX) {
            in_stack[provider_index] = 0;
            return 0; /* Plan capacity exceeded. */
        }
        out->passes[out->count++] = provider->pass_id;
        *sim_flags |= provider->produced_state;
    }

    in_stack[provider_index] = 0;
    return 1;
}

int vir_build_execution_plan(const VirPath *path,
                             const VirPassDescriptor *registry,
                             size_t registry_count,
                             uint32_t target_state,
                             VirExecutionPlan *out) {
    if (!path || !registry || !out) return 0;

    memset(out, 0, sizeof(*out));
    out->target_state  = target_state;
    out->current_state = path->state_flags;

    /* Nothing to do — path already satisfies the target state. */
    if ((path->state_flags & target_state) == target_state) return 1;

    int *in_stack = (int*)calloc(registry_count, sizeof(int));
    if (!in_stack) return 0;

    /* sim_flags starts from the path's current state and grows as passes
     * are appended to the plan.                                          */
    uint32_t sim_flags = path->state_flags;
    uint32_t missing   = target_state & ~sim_flags;

    for (size_t bit = 0; bit < 32; bit++) {
        uint32_t flag = 1U << bit;
        if (missing & flag) {
            if (!collect_deps_for_state(flag, &sim_flags,
                                        registry, registry_count,
                                        in_stack, out)) {
                free(in_stack);
                return 0;
            }
        }
    }

    free(in_stack);
    return 1;
}

int vir_execute_plan(VirPath *path,
                     const VirExecutionPlan *plan,
                     const VirPassDescriptor *registry,
                     size_t registry_count,
                     VirPipelineStats *stats) {
    if (!path || !plan || !registry || !stats) return 0;
    memset(stats, 0, sizeof(*stats));
    if (plan->count == 0) return 1; /* Nothing to run. */

    return vir_run_pipeline_with_deps(path,
                                      (VirPassDescriptor *)registry,
                                      registry_count,
                                      plan->passes,
                                      plan->count,
                                      stats);
}

/* ── Provenance Receipt ───────────────────────────────────────────────────── */

/* ── State Flag Diagnostics ────────────────────────────────────────────── */

const char *vir_state_flag_name(uint32_t flag) {
    switch (flag) {
        case VIR_STATE_DEGENERATE_FREE: return "DEGENERATE_FREE";
        case VIR_STATE_ARCS_EXPANDED:   return "ARCS_EXPANDED";
        case VIR_STATE_CANONICALIZED:   return "CANONICALIZED";
        case VIR_STATE_BOUNDS_VALID:    return "BOUNDS_VALID";
        case VIR_STATE_EXACT_BOUNDS:    return "EXACT_BOUNDS";
        case VIR_STATE_NORMALIZED:      return "NORMALIZED";
        case VIR_STATE_LOCALIZED:       return "LOCALIZED";
        default:                        return "UNKNOWN";
    }
}

char *vir_state_flags_to_string(uint32_t flags) {
    if (flags == 0) {
        char *s = (char *)malloc(6); /* "CLEAN" + NUL */
        if (s) strcpy(s, "CLEAN");
        return s;
    }

    /* Two-pass: measure total length, then allocate and fill. */
    size_t total = 0;
    int first = 1;
    for (size_t bit = 0; bit < 32; bit++) {
        uint32_t f = 1U << bit;
        if (flags & f) {
            const char *name = vir_state_flag_name(f);
            if (!first) total += 3; /* " | " */
            total += strlen(name);
            first = 0;
        }
    }

    char *out = (char *)malloc(total + 1);
    if (!out) return NULL;
    out[0] = '\0';

    first = 1;
    for (size_t bit = 0; bit < 32; bit++) {
        uint32_t f = 1U << bit;
        if (flags & f) {
            if (!first) strcat(out, " | ");
            strcat(out, vir_state_flag_name(f));
            first = 0;
        }
    }
    return out;
}

/* ── Geometry Metrics ─────────────────────────────────────────────────────── */

VirGeometryMetrics vir_path_compute_metrics(const VirPath *path) {
    VirGeometryMetrics m;
    memset(&m, 0, sizeof(m));
    if (!path || path->count == 0) return m;

    /* Current pen position for chord-length and shoelace accumulation. */
    float cx = 0.0f, cy = 0.0f;
    /* Shoelace accumulator (raw sum; divide by 2 at end). */
    double area_acc = 0.0;

    m.total_count = (uint32_t)path->count;

    for (size_t i = 0; i < path->count; i++) {
        const VirSegment *s = &path->segments[i];

        switch (s->op) {
            case VIR_MOVE: {
                m.move_count++;
                cx = s->coords[0];
                cy = s->coords[1];
                break;
            }
            case VIR_LINE: {
                m.line_count++;
                float ex = s->coords[0], ey = s->coords[1];
                /* Chord length (exact for lines). */
                float dx = ex - cx, dy = ey - cy;
                m.approx_length += sqrtf(dx*dx + dy*dy);
                /* Shoelace contribution: (x0*y1 - x1*y0). */
                area_acc += (double)cx * ey - (double)ex * cy;
                cx = ex; cy = ey;
                break;
            }
            case VIR_CUBIC: {
                m.cubic_count++;
                /* coords: [x1,y1, x2,y2, x3,y3, ...] — end point at [4..5]. */
                float ex = s->coords[4], ey = s->coords[5];
                float dx = ex - cx, dy = ey - cy;
                m.approx_length += sqrtf(dx*dx + dy*dy);
                /* Shoelace: treat as implicit chord from pen to end point.  */
                area_acc += (double)cx * ey - (double)ex * cy;
                cx = ex; cy = ey;
                break;
            }
            case VIR_ARC: {
                m.arc_count++;
                /* coords: [rx,ry, x_rot, large_arc, sweep, ex, ey, ...].   */
                float ex = s->coords[5], ey = s->coords[6];
                float dx = ex - cx, dy = ey - cy;
                m.approx_length += sqrtf(dx*dx + dy*dy);
                area_acc += (double)cx * ey - (double)ex * cy;
                cx = ex; cy = ey;
                break;
            }
            case VIR_CLOSE: {
                m.close_count++;
                /* CLOSE draws an implicit line back to the subpath origin.
                 * We do not have the subpath start cached in this walk, so
                 * the shoelace closure is handled via the pen already being
                 * at the last point — no additional chord length added.    */
                break;
            }
        }
    }

    m.signed_area = (float)(area_acc * 0.5);
    return m;
}

/* ── Cache Record Header ──────────────────────────────────────────────────── */

/* CRC32 IEEE 802.3 (reflected poly 0xEDB88320).
 * Table-driven, processes one byte per iteration.                          */
static uint32_t vir_crc32_ieee(const void *data, size_t len) {
    static const uint32_t table[256] = {
        0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,
        0xE963A535,0x9E6495A3,0x0EDB8832,0x79DCB8A4,0xE0D5E91B,0x97D2D988,
        0x09B64C2B,0x7EB17CBF,0xE7B82D09,0x90BF1CBF,0x1DB71064,0x6AB020F2,
        0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,
        0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,
        0xFA0F3D63,0x8D080DF5,0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,
        0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,0x35B5A8FA,0x42B2986C,
        0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
        0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F928,0x56B3C9BE,
        0xCFBA9599,0xB8BDA50F,0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,
        0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,0x76DC4190,0x01DB7106,
        0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,
        0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6AD9BB,0x086D3D2D,
        0x91646C97,0xE6635C01,0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,
        0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,0x65B0D9C6,0x12B7E950,
        0x8BBEB8EA,0xFCB9887C,0x62DD1D7F,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
        0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,
        0xA4D1C46D,0xD3D6F4FB,0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,
        0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,0x5005713C,0x270241AA,
        0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,
        0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,
        0xB7BD5C3B,0xC0BA6CAD,0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,
        0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,0xE3630B12,0x94643B84,
        0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
        0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,
        0x196C3671,0x6E6B06E7,0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,
        0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,0xD6D6A3E8,0xA1D1937E,
        0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
        0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA8670955,
        0x316658EF,0x46616879,0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D
    };
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < len; i++)
        crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFU;
}

VirCacheRecordHeader vir_cache_record_header_init(const VirPath *path,
                                                   uint32_t payload_size) {
    VirCacheRecordHeader hdr;
    memset(&hdr, 0, sizeof(hdr));

    /* Pull identity from manifest (non-mutating). */
    VirArtifactManifest m;
    memset(&m, 0, sizeof(m));
    if (path) m = vir_path_manifest(path, 1e-3f);

    hdr.magic                = VIR_CACHE_RECORD_MAGIC;
    hdr.schema_version       = VIR_CACHE_SCHEMA_VERSION;
    hdr.canonical_fingerprint = m.canonical_fingerprint;
    hdr.state_flags          = m.state_flags;
    hdr.segment_count        = m.segment_count;
    if (m.state_flags & VIR_STATE_EXACT_BOUNDS) {
        hdr.min_x = m.min_x; hdr.min_y = m.min_y;
        hdr.max_x = m.max_x; hdr.max_y = m.max_y;
    }
    hdr.payload_size = payload_size;

    /* CRC32 covers all fields before header_crc32. */
    hdr.header_crc32 = vir_crc32_ieee(&hdr, offsetof(VirCacheRecordHeader, header_crc32));
    return hdr;
}

int vir_cache_record_header_validate(const VirCacheRecordHeader *hdr) {
    if (!hdr)                                           return 0;
    if (hdr->magic != VIR_CACHE_RECORD_MAGIC)           return 0;
    if (hdr->schema_version != VIR_CACHE_SCHEMA_VERSION) return 0;
    uint32_t expected = vir_crc32_ieee(hdr, offsetof(VirCacheRecordHeader, header_crc32));
    return hdr->header_crc32 == expected ? 1 : 0;
}

char *vir_pipeline_provenance_json(const VirPath        *path,
                                   const VirPipelineStats *stats,
                                   const VirCacheStats    *cache) {
    /* Capture identity via manifest (non-mutating). */
    VirArtifactManifest m;
    memset(&m, 0, sizeof(m));
    if (path) m = vir_path_manifest(path, 1e-3f);

    /* Zero-fill optional sections when caller passes NULL. */
    VirPipelineStats zstats; memset(&zstats, 0, sizeof(zstats));
    VirCacheStats    zcache; memset(&zcache, 0, sizeof(zcache));
    const VirPipelineStats *p = stats ? stats : &zstats;
    const VirCacheStats    *c = cache ? cache : &zcache;

    /* Bounds section — only meaningful when VIR_STATE_EXACT_BOUNDS is set. */
    float bmin_x = 0.0f, bmin_y = 0.0f, bmax_x = 0.0f, bmax_y = 0.0f;
    if (m.state_flags & VIR_STATE_EXACT_BOUNDS) {
        bmin_x = m.min_x; bmin_y = m.min_y;
        bmax_x = m.max_x; bmax_y = m.max_y;
    }

    /* Build state_names string for inclusion in JSON. */
    char *state_names = vir_state_flags_to_string(m.state_flags);
    if (!state_names) return NULL;

    /* Two-pass: dry-run with NULL to measure, then allocate and format. */
    int needed = snprintf(NULL, 0,
        "{\n"
        "  \"canonical_fingerprint\": \"0x%016llx\",\n"
        "  \"schema_version\": %u,\n"
        "  \"state_flags\": %u,\n"
        "  \"state_names\": \"%s\",\n"
        "  \"segment_count\": %u,\n"
        "  \"bounds\": {\n"
        "    \"min_x\": %.6g,\n"
        "    \"min_y\": %.6g,\n"
        "    \"max_x\": %.6g,\n"
        "    \"max_y\": %.6g\n"
        "  },\n"
        "  \"pipeline\": {\n"
        "    \"total_passes\": %llu,\n"
        "    \"mutations\": %llu,\n"
        "    \"no_change\": %llu,\n"
        "    \"failures\": %llu\n"
        "  },\n"
        "  \"cache\": {\n"
        "    \"hits\": %llu,\n"
        "    \"misses\": %llu,\n"
        "    \"evictions\": %llu\n"
        "  }\n"
        "}",
        (unsigned long long)m.canonical_fingerprint,
        m.schema_version,
        m.state_flags,
        state_names,
        m.segment_count,
        (double)bmin_x, (double)bmin_y,
        (double)bmax_x, (double)bmax_y,
        (unsigned long long)p->total_passes,
        (unsigned long long)p->mutations,
        (unsigned long long)p->no_change,
        (unsigned long long)p->failures,
        (unsigned long long)c->hits,
        (unsigned long long)c->misses,
        (unsigned long long)c->evictions
    );

    if (needed < 0) { free(state_names); return NULL; }
    char *out = (char *)malloc((size_t)needed + 1);
    if (!out) { free(state_names); return NULL; }

    snprintf(out, (size_t)needed + 1,
        "{\n"
        "  \"canonical_fingerprint\": \"0x%016llx\",\n"
        "  \"schema_version\": %u,\n"
        "  \"state_flags\": %u,\n"
        "  \"state_names\": \"%s\",\n"
        "  \"segment_count\": %u,\n"
        "  \"bounds\": {\n"
        "    \"min_x\": %.6g,\n"
        "    \"min_y\": %.6g,\n"
        "    \"max_x\": %.6g,\n"
        "    \"max_y\": %.6g\n"
        "  },\n"
        "  \"pipeline\": {\n"
        "    \"total_passes\": %llu,\n"
        "    \"mutations\": %llu,\n"
        "    \"no_change\": %llu,\n"
        "    \"failures\": %llu\n"
        "  },\n"
        "  \"cache\": {\n"
        "    \"hits\": %llu,\n"
        "    \"misses\": %llu,\n"
        "    \"evictions\": %llu\n"
        "  }\n"
        "}",
        (unsigned long long)m.canonical_fingerprint,
        m.schema_version,
        m.state_flags,
        state_names,
        m.segment_count,
        (double)bmin_x, (double)bmin_y,
        (double)bmax_x, (double)bmax_y,
        (unsigned long long)p->total_passes,
        (unsigned long long)p->mutations,
        (unsigned long long)p->no_change,
        (unsigned long long)p->failures,
        (unsigned long long)c->hits,
        (unsigned long long)c->misses,
        (unsigned long long)c->evictions
    );

    free(state_names);
    return out;
}

int vir_run_passes(VirPath *path, const VirPass *passes, size_t count) {

    if (!path || !passes) return 0;
    for (size_t i = 0; i < count; i++) {

        VirPassResult res = VIR_PASS_NO_CHANGE;
        switch (passes[i]) {
            case VIR_PASS_DEGENERATE:
                res = vir_path_optimize_degenerate(path);
                break;
            case VIR_PASS_EXPAND_ARCS:
                res = vir_path_expand_arcs(path);
                break;
            case VIR_PASS_COMPUTE_BOUNDS:
                res = vir_path_compute_bounds_pass(path);
                break;
            case VIR_PASS_CANONICALIZE:
                res = vir_path_canonicalize(path);
                break;
            case VIR_PASS_EXACT_BOUNDS:
                res = vir_path_compute_exact_bounds_pass(path);
                break;
            case VIR_PASS_NORMALIZE:
                res = vir_path_normalize(path);
                break;
            case VIR_PASS_LOCALIZE:
                res = vir_path_localize(path);
                break;
            default:
                return 0;
        }
        if (res == VIR_PASS_ERROR) {
            return 0;
        }
    }
    return 1;
}

static const char* vir_pass_result_to_str(VirPassResult res) {
    switch (res) {
        case VIR_PASS_OK: return "OK";
        case VIR_PASS_NO_CHANGE: return "NO_CHANGE";
        case VIR_PASS_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

static VirPassDescriptor default_registry[] = {
    { VIR_PASS_DEGENERATE, "degenerate", vir_path_optimize_degenerate, 0, 0, 0,
      VIR_STATE_CLEAN, VIR_STATE_DEGENERATE_FREE, VIR_STATE_BOUNDS_VALID | VIR_STATE_CANONICALIZED | VIR_STATE_EXACT_BOUNDS | VIR_STATE_NORMALIZED | VIR_STATE_LOCALIZED },
    { VIR_PASS_EXPAND_ARCS, "expand_arcs", vir_path_expand_arcs, 0, 0, 0,
      VIR_STATE_CLEAN, VIR_STATE_ARCS_EXPANDED, VIR_STATE_BOUNDS_VALID | VIR_STATE_CANONICALIZED | VIR_STATE_EXACT_BOUNDS | VIR_STATE_NORMALIZED | VIR_STATE_LOCALIZED },
    { VIR_PASS_COMPUTE_BOUNDS, "bounds", vir_path_compute_bounds_pass, 0, 0, 0,
      VIR_STATE_CLEAN, VIR_STATE_BOUNDS_VALID, 0 },
    { VIR_PASS_CANONICALIZE, "canonicalize", vir_path_canonicalize, 0, 0, 0,
      VIR_STATE_ARCS_EXPANDED, VIR_STATE_CANONICALIZED, VIR_STATE_BOUNDS_VALID | VIR_STATE_EXACT_BOUNDS | VIR_STATE_NORMALIZED | VIR_STATE_LOCALIZED },
    { VIR_PASS_NORMALIZE, "normalize", vir_path_normalize, 0, 0, 0,
      VIR_STATE_CANONICALIZED, VIR_STATE_NORMALIZED, VIR_STATE_BOUNDS_VALID | VIR_STATE_EXACT_BOUNDS | VIR_STATE_LOCALIZED },
    { VIR_PASS_EXACT_BOUNDS, "exact_bounds", vir_path_compute_exact_bounds_pass, 0, 0, 0,
      VIR_STATE_NORMALIZED, VIR_STATE_EXACT_BOUNDS, 0 },
    { VIR_PASS_LOCALIZE, "localize", vir_path_localize, 0, 0, 0,
      VIR_STATE_EXACT_BOUNDS, VIR_STATE_LOCALIZED, 0 }
};

VirPassDescriptor* vir_pipeline_get_default_registry(size_t *out_count) {
    if (out_count) {
        *out_count = sizeof(default_registry) / sizeof(default_registry[0]);
    }
    return default_registry;
}

void vir_pipeline_reset_telemetry(VirPassDescriptor *registry, size_t count) {
    if (!registry) return;
    for (size_t i = 0; i < count; i++) {
        registry[i].runs = 0;
        registry[i].mutations = 0;
        registry[i].failures = 0;
    }
}

int vir_run_pipeline(
    VirPath *path,
    VirPassDescriptor *registry,
    size_t registry_count,
    const VirPass *passes,
    size_t pass_count,
    VirPipelineStats *stats
) {
    if (!path || !registry || !passes || !stats) return 0;

    memset(stats, 0, sizeof(VirPipelineStats));

    printf("\nPipeline Execution\n\n");
    printf("%-25sResult\n", "Pass");
    printf("------------------------------------------\n");

    for (size_t i = 0; i < pass_count; i++) {
        VirPass requested = passes[i];
        VirPassDescriptor *desc = NULL;
        for (size_t j = 0; j < registry_count; j++) {
            if (registry[j].pass_id == requested) {
                desc = &registry[j];
                break;
            }
        }

        if (!desc) {
            printf("%-25sERROR (Pass not found in registry)\n", "unknown");
            stats->failures++;
            return 0;
        }

        desc->runs++;
        stats->total_passes++;

        VirPassResult res = desc->run(path);
        printf("%-25s%s\n", desc->name, vir_pass_result_to_str(res));

        if (res == VIR_PASS_OK) {
            desc->mutations++;
            stats->mutations++;
        } else if (res == VIR_PASS_NO_CHANGE) {
            stats->no_change++;
        } else if (res == VIR_PASS_ERROR) {
            desc->failures++;
            stats->failures++;
            printf("\nMutations: %llu\nFailures : %llu\n",
                (unsigned long long)stats->mutations,
                (unsigned long long)stats->failures);
            return 0;
        }
    }

    printf("\nMutations: %llu\nFailures : %llu\n",
        (unsigned long long)stats->mutations,
        (unsigned long long)stats->failures);

    return 1;
}

int vir_run_pipeline_until_stable(
    VirPath *path,
    VirPassDescriptor *registry,
    size_t registry_count,
    const VirPass *passes,
    size_t pass_count,
    VirPipelineStats *stats,
    size_t max_iterations
) {
    if (!path || !registry || !passes || !stats || max_iterations == 0) return 0;

    memset(stats, 0, sizeof(VirPipelineStats));

    size_t iterations = 0;
    while (iterations < max_iterations) {
        iterations++;
        int iteration_changed = 0;

        printf("\nPipeline Iteration %zu\n", iterations);
        printf("%-25sResult\n", "Pass");
        printf("------------------------------------------\n");

        for (size_t i = 0; i < pass_count; i++) {
            VirPass requested = passes[i];
            VirPassDescriptor *desc = NULL;
            for (size_t j = 0; j < registry_count; j++) {
                if (registry[j].pass_id == requested) {
                    desc = &registry[j];
                    break;
                }
            }

            if (!desc) {
                printf("%-25sERROR (Pass not found in registry)\n", "unknown");
                stats->failures++;
                return 0;
            }

            desc->runs++;
            stats->total_passes++;

            VirPassResult res = desc->run(path);
            printf("%-25s%s\n", desc->name, vir_pass_result_to_str(res));

            if (res == VIR_PASS_OK) {
                desc->mutations++;
                stats->mutations++;
                iteration_changed = 1;
            } else if (res == VIR_PASS_NO_CHANGE) {
                stats->no_change++;
            } else if (res == VIR_PASS_ERROR) {
                desc->failures++;
                stats->failures++;
                printf("\nMutations: %llu\nFailures : %llu\n",
                    (unsigned long long)stats->mutations,
                    (unsigned long long)stats->failures);
                return 0;
            }
        }

        if (!iteration_changed) {
            printf("\nConvergence reached in %zu iterations.\n", iterations);
            printf("Total Passes : %llu\n", (unsigned long long)stats->total_passes);
            printf("Mutations    : %llu\n", (unsigned long long)stats->mutations);
            printf("No Change    : %llu\n", (unsigned long long)stats->no_change);
            printf("Failures     : %llu\n", (unsigned long long)stats->failures);
            return (int)iterations;
        }
    }

    printf("\nFailed to converge within limit of %zu iterations.\n", max_iterations);
    return 0;
}

static int schedule_and_run_pass(
    VirPath *path,
    VirPassDescriptor *registry,
    size_t registry_count,
    VirPass requested,
    VirPipelineStats *stats,
    int *visited
) {
    size_t pass_index = (size_t)-1;
    for (size_t i = 0; i < registry_count; i++) {
        if (registry[i].pass_id == requested) {
            pass_index = i;
            break;
        }
    }
    if (pass_index == (size_t)-1) {
        printf("Pass not found in registry: %d\n", requested);
        return 0;
    }

    if (visited[pass_index]) {
        printf("Circular dependency detected for pass %d!\n", requested);
        return 0;
    }
    visited[pass_index] = 1;

    VirPassDescriptor *desc = &registry[pass_index];

    // Check if the produced_state flags are already fully satisfied.
    if (desc->produced_state != 0 && (path->state_flags & desc->produced_state) == desc->produced_state) {
        printf("%-25sSKIPPED (already satisfied)\n", desc->name);
        stats->no_change++;
        visited[pass_index] = 0;
        return 1;
    }

    // Resolve required prerequisites recursively
    if (desc->required_state != 0) {
        uint32_t missing = desc->required_state & ~path->state_flags;
        if (missing != 0) {
            for (size_t flag_bit = 0; flag_bit < 32; flag_bit++) {
                uint32_t flag = 1U << flag_bit;
                if (missing & flag) {
                    VirPassDescriptor *provider = NULL;
                    for (size_t j = 0; j < registry_count; j++) {
                        if (registry[j].produced_state & flag) {
                            provider = &registry[j];
                            break;
                        }
                    }
                    if (provider) {
                        if (!schedule_and_run_pass(path, registry, registry_count, provider->pass_id, stats, visited)) {
                            visited[pass_index] = 0;
                            return 0;
                        }
                    } else {
                        printf("No provider pass found for state flag 0x%X required by %s!\n", flag, desc->name);
                        visited[pass_index] = 0;
                        return 0;
                    }
                }
            }
        }
    }

    // Execute the pass
    desc->runs++;
    stats->total_passes++;

    VirPassResult res = desc->run(path);
    printf("%-25s%s\n", desc->name, vir_pass_result_to_str(res));

    if (res == VIR_PASS_OK) {
        desc->mutations++;
        stats->mutations++;
        path->state_flags &= ~desc->invalidated_state;
        path->state_flags |= desc->produced_state;
    } else if (res == VIR_PASS_NO_CHANGE) {
        stats->no_change++;
        path->state_flags |= desc->produced_state;
    } else if (res == VIR_PASS_ERROR) {
        desc->failures++;
        stats->failures++;
        visited[pass_index] = 0;
        return 0;
    }

    visited[pass_index] = 0;
    return 1;
}

int vir_run_pipeline_with_deps(
    VirPath *path,
    VirPassDescriptor *registry,
    size_t registry_count,
    const VirPass *passes,
    size_t pass_count,
    VirPipelineStats *stats
) {
    if (!path || !registry || !passes || !stats) return 0;

    memset(stats, 0, sizeof(VirPipelineStats));

    int *visited = (int*)calloc(registry_count, sizeof(int));
    if (!visited) return 0;

    printf("\nPipeline Execution (with dependencies)\n\n");
    printf("%-25sResult\n", "Pass");
    printf("------------------------------------------\n");

    for (size_t i = 0; i < pass_count; i++) {
        if (!schedule_and_run_pass(path, registry, registry_count, passes[i], stats, visited)) {
            free(visited);
            printf("\nMutations: %llu\nFailures : %llu\n",
                (unsigned long long)stats->mutations,
                (unsigned long long)stats->failures);
            return 0;
        }
    }

    free(visited);

    printf("\nMutations: %llu\nFailures : %llu\n",
        (unsigned long long)stats->mutations,
        (unsigned long long)stats->failures);

    return 1;
}

int vir_prepare_backend(
    VirPath *path,
    VirPassDescriptor *registry,
    size_t registry_count,
    VirBackend backend,
    VirPipelineStats *stats
) {
    if (!path || !registry || !stats) return 0;

    uint32_t target_state = 0;
    switch (backend) {
        case VIR_BACKEND_SVG:
            target_state = VIR_STATE_CLEAN;
            break;
        case VIR_BACKEND_SDF:
            target_state = VIR_STATE_ARCS_EXPANDED;
            break;
        case VIR_BACKEND_GLSL:
            target_state = VIR_STATE_ARCS_EXPANDED | VIR_STATE_CANONICALIZED | VIR_STATE_EXACT_BOUNDS | VIR_STATE_LOCALIZED;
            break;
        default:
            return 0;
    }

    if ((path->state_flags & target_state) == target_state) {
        memset(stats, 0, sizeof(VirPipelineStats));
        return 1;
    }

    uint32_t missing = target_state & ~path->state_flags;
    VirPass passes_to_run[16];
    size_t pass_count = 0;

    for (size_t flag_bit = 0; flag_bit < 32; flag_bit++) {
        uint32_t flag = 1U << flag_bit;
        if (missing & flag) {
            for (size_t i = 0; i < registry_count; i++) {
                if (registry[i].produced_state & flag) {
                    int duplicate = 0;
                    for (size_t k = 0; k < pass_count; k++) {
                        if (passes_to_run[k] == registry[i].pass_id) {
                            duplicate = 1;
                            break;
                        }
                    }
                    if (!duplicate && pass_count < 16) {
                        passes_to_run[pass_count++] = registry[i].pass_id;
                    }
                }
            }
        }
    }

    if (pass_count > 0) {
        return vir_run_pipeline_with_deps(path, registry, registry_count, passes_to_run, pass_count, stats);
    }

    return 1;
}

char* vir_pipeline_to_dot(
    VirPassDescriptor *registry,
    size_t registry_count
) {
    if (!registry || registry_count == 0) return NULL;

    size_t capacity = 4096;
    char *buf = (char*)malloc(capacity);
    if (!buf) return NULL;

    buf[0] = '\0';
    size_t len = sprintf(buf, "digraph VIR_Pipeline {\n"
                              "  node [shape=box, style=filled, color=lightgray];\n"
                              "  edge [fontsize=10];\n\n");

    for (size_t i = 0; i < registry_count; i++) {
        char node_line[256];
        int n = sprintf(node_line, "  %s [label=\"%s\\nruns: %llu\\nmutations: %llu\\nfailures: %llu\"];\n",
                        registry[i].name, registry[i].name,
                        (unsigned long long)registry[i].runs,
                        (unsigned long long)registry[i].mutations,
                        (unsigned long long)registry[i].failures);
        if (len + n + 256 >= capacity) {
            capacity *= 2;
            char *new_buf = (char*)realloc(buf, capacity);
            if (!new_buf) { free(buf); return NULL; }
            buf = new_buf;
        }
        strcpy(buf + len, node_line);
        len += n;
    }

    for (size_t i = 0; i < registry_count; i++) {
        VirPassDescriptor *p = &registry[i];
        if (p->required_state != 0) {
            for (size_t bit = 0; bit < 32; bit++) {
                uint32_t flag = 1U << bit;
                if (p->required_state & flag) {
                    for (size_t j = 0; j < registry_count; j++) {
                        if (registry[j].produced_state & flag) {
                            char edge_line[256];
                            int n = sprintf(edge_line, "  %s -> %s [label=\"requires %s\"];\n",
                                            registry[j].name, p->name,
                                            vir_state_flag_name(flag));
                            if (len + n + 256 >= capacity) {
                                capacity *= 2;
                                char *new_buf = (char*)realloc(buf, capacity);
                                if (!new_buf) { free(buf); return NULL; }
                                buf = new_buf;
                            }
                            strcpy(buf + len, edge_line);
                            len += n;
                        }
                    }
                }
            }
        }
    }

    for (size_t i = 0; i < registry_count; i++) {
        VirPassDescriptor *p = &registry[i];
        if (p->invalidated_state != 0) {
            for (size_t bit = 0; bit < 32; bit++) {
                uint32_t flag = 1U << bit;
                if (p->invalidated_state & flag) {
                    for (size_t j = 0; j < registry_count; j++) {
                        if (registry[j].produced_state & flag) {
                            char edge_line[256];
                            int n = sprintf(edge_line, "  %s -> %s [color=red, style=dashed, label=\"invalidates %s\"];\n",
                                            p->name, registry[j].name,
                                            vir_state_flag_name(flag));
                            if (len + n + 256 >= capacity) {
                                capacity *= 2;
                                char *new_buf = (char*)realloc(buf, capacity);
                                if (!new_buf) { free(buf); return NULL; }
                                buf = new_buf;
                            }
                            strcpy(buf + len, edge_line);
                            len += n;
                        }
                    }
                }
            }
        }
    }

    char stats_line[256];
    int n = sprintf(stats_line, "\n  subgraph cluster_cache {\n"
                                "    label=\"Cache Telemetry\";\n"
                                "    color=blue;\n"
                                "    cache_stats [shape=Mrecord, label=\"{Hits: %llu|Misses: %llu|Evictions: %llu}\", style=filled, fillcolor=lightblue];\n"
                                "  }\n",
                    (unsigned long long)g_cache_stats.hits,
                    (unsigned long long)g_cache_stats.misses,
                    (unsigned long long)g_cache_stats.evictions);
    if (len + n + 256 >= capacity) {
        capacity *= 2;
        char *new_buf = (char*)realloc(buf, capacity);
        if (!new_buf) { free(buf); return NULL; }
        buf = new_buf;
    }
    strcpy(buf + len, stats_line);
    len += n;

    strcpy(buf + len, "}\n");
    return buf;
}

static int check_cycle_dfs(
    size_t u,
    const VirPassDescriptor *registry,
    size_t registry_count,
    int *colors
) {
    colors[u] = 1; // GRAY
    for (size_t v = 0; v < registry_count; v++) {
        if (v != u) {
            // Edge from u -> v if u produces a flag required by v
            if ((registry[u].produced_state & registry[v].required_state) != 0) {
                if (colors[v] == 1) {
                    return 1; // Cycle detected
                } else if (colors[v] == 0) {
                    if (check_cycle_dfs(v, registry, registry_count, colors)) {
                        return 1;
                    }
                }
            }
        }
    }
    colors[u] = 2; // BLACK
    return 0;
}

static int is_state_satisfiable(
    uint32_t target_state,
    const VirPassDescriptor *registry,
    size_t registry_count,
    int *visited,
    uint32_t *resolving
) {
    if (target_state == 0) return 1;

    for (size_t bit = 0; bit < 32; bit++) {
        uint32_t flag = 1U << bit;
        if (target_state & flag) {
            size_t provider_idx = (size_t)-1;
            for (size_t i = 0; i < registry_count; i++) {
                if (registry[i].produced_state & flag) {
                    provider_idx = i;
                    break;
                }
            }
            if (provider_idx == (size_t)-1) {
                return 0; // Orphan state
            }

            if (resolving[provider_idx]) {
                return 0; // Cycle/self-dependency
            }

            if (!visited[provider_idx]) {
                resolving[provider_idx] = 1;
                if (!is_state_satisfiable(registry[provider_idx].required_state, registry, registry_count, visited, resolving)) {
                    resolving[provider_idx] = 0;
                    return 0;
                }
                resolving[provider_idx] = 0;
                visited[provider_idx] = 1;
            }
        }
    }
    return 1;
}

VirRegistryValidationResult vir_validate_registry(
    const VirPassDescriptor *registry,
    size_t registry_count,
    VirRegistryValidationError *err
) {
    if (err) {
        memset(err, 0, sizeof(VirRegistryValidationError));
    }

    // 0. State Alias Check
    uint32_t known_flags[] = {
        VIR_STATE_DEGENERATE_FREE,
        VIR_STATE_ARCS_EXPANDED,
        VIR_STATE_CANONICALIZED,
        VIR_STATE_BOUNDS_VALID,
        VIR_STATE_EXACT_BOUNDS,
        VIR_STATE_NORMALIZED,
        VIR_STATE_LOCALIZED
    };
    size_t num_flags = sizeof(known_flags) / sizeof(known_flags[0]);
    for (size_t i = 0; i < num_flags; i++) {
        uint32_t f = known_flags[i];
        if (f == 0 || (f & (f - 1)) != 0) {
            if (err) {
                err->status = VIR_REGISTRY_ERR_STATE_ALIAS;
                err->message = "State flag is not a unique single-bit power of two.";
                err->state_mask = f;
            }
            return VIR_REGISTRY_ERR_STATE_ALIAS;
        }
        for (size_t j = i + 1; j < num_flags; j++) {
            if (f == known_flags[j]) {
                if (err) {
                    err->status = VIR_REGISTRY_ERR_STATE_ALIAS;
                    err->message = "Duplicate state flag bit/definition detected (alias).";
                    err->state_mask = f;
                }
                return VIR_REGISTRY_ERR_STATE_ALIAS;
            }
        }
    }

    if (!registry && registry_count > 0) {
        if (err) {
            err->status = VIR_REGISTRY_ERR_NULL;
            err->message = "Null registry pointer with non-zero registry count.";
        }
        return VIR_REGISTRY_ERR_NULL;
    }

    // 1. Duplicate Pass ID Check
    for (size_t i = 0; i < registry_count; i++) {
        for (size_t j = i + 1; j < registry_count; j++) {
            if (registry[i].pass_id == registry[j].pass_id) {
                if (err) {
                    err->status = VIR_REGISTRY_ERR_DUPLICATE_PASS;
                    err->message = "Duplicate pass_id registered.";
                    err->pass_id = registry[i].pass_id;
                }
                return VIR_REGISTRY_ERR_DUPLICATE_PASS;
            }
        }
    }

    // 2. Duplicate State Producer Check
    uint32_t produced_all = 0;
    for (size_t i = 0; i < registry_count; i++) {
        uint32_t p = registry[i].produced_state;
        if (p != 0) {
            uint32_t intersect = produced_all & p;
            if (intersect != 0) {
                if (err) {
                    err->status = VIR_REGISTRY_ERR_DUPLICATE_PRODUCER;
                    err->message = "State flag has multiple producer passes.";
                    err->pass_id = registry[i].pass_id;
                    err->state_mask = intersect;
                }
                return VIR_REGISTRY_ERR_DUPLICATE_PRODUCER;
            }
            produced_all |= p;
        }
    }

    // 3. Orphan Required State Check
    for (size_t i = 0; i < registry_count; i++) {
        uint32_t req = registry[i].required_state;
        if (req != 0) {
            uint32_t missing = req & ~produced_all;
            if (missing != 0) {
                if (err) {
                    err->status = VIR_REGISTRY_ERR_ORPHAN_REQUIRED_STATE;
                    err->message = "Required state has no registered producer pass.";
                    err->pass_id = registry[i].pass_id;
                    err->state_mask = missing;
                }
                return VIR_REGISTRY_ERR_ORPHAN_REQUIRED_STATE;
            }
        }
    }

    // 4. Invalid Invalidation Check
    for (size_t i = 0; i < registry_count; i++) {
        uint32_t inv = registry[i].invalidated_state;
        if (inv != 0) {
            uint32_t invalid_bits = inv & ~produced_all;
            if (invalid_bits != 0) {
                if (err) {
                    err->status = VIR_REGISTRY_ERR_INVALID_INVALIDATION;
                    err->message = "Pass invalidates a state flag that is not produced by any pass.";
                    err->pass_id = registry[i].pass_id;
                    err->state_mask = invalid_bits;
                }
                return VIR_REGISTRY_ERR_INVALID_INVALIDATION;
            }
        }
    }

    // 5. Pass requiring or producing state it also invalidates (self-conflict check)
    for (size_t i = 0; i < registry_count; i++) {
        uint32_t self_conflict_req = registry[i].required_state & registry[i].invalidated_state;
        uint32_t self_conflict_prod = registry[i].produced_state & registry[i].invalidated_state;
        uint32_t conflict = self_conflict_req | self_conflict_prod;
        if (conflict != 0) {
            if (err) {
                err->status = VIR_REGISTRY_ERR_INVALID_INVALIDATION;
                err->message = "Pass has a self-conflict (requires or produces a state it invalidates).";
                err->pass_id = registry[i].pass_id;
                err->state_mask = conflict;
            }
            return VIR_REGISTRY_ERR_INVALID_INVALIDATION;
        }
    }

    // 6. Dependency Graph Cycle Check (DFS)
    int *colors = (int*)calloc(registry_count, sizeof(int));
    if (!colors) {
        return VIR_REGISTRY_ERR_NULL;
    }
    for (size_t i = 0; i < registry_count; i++) {
        if (colors[i] == 0) {
            if (check_cycle_dfs(i, registry, registry_count, colors)) {
                if (err) {
                    err->status = VIR_REGISTRY_ERR_CYCLE;
                    err->message = "Cyclic dependency detected in pass dependency graph.";
                    err->pass_id = registry[i].pass_id;
                }
                free(colors);
                return VIR_REGISTRY_ERR_CYCLE;
            }
        }
    }
    free(colors);

    // 7. Backend-required states satisfiability check
    uint32_t backends_to_check[] = {
        VIR_STATE_CLEAN, // SVG (0)
        VIR_STATE_ARCS_EXPANDED, // SDF (2)
        VIR_STATE_ARCS_EXPANDED | VIR_STATE_CANONICALIZED | VIR_STATE_EXACT_BOUNDS | VIR_STATE_LOCALIZED // GLSL
    };
    for (size_t b = 0; b < 3; b++) {
        uint32_t target = backends_to_check[b];
        int *visited = (int*)calloc(registry_count, sizeof(int));
        uint32_t *resolving = (uint32_t*)calloc(registry_count, sizeof(uint32_t));
        if (!visited || !resolving) {
            if (visited) free(visited);
            if (resolving) free(resolving);
            return VIR_REGISTRY_ERR_NULL;
        }

        if (!is_state_satisfiable(target, registry, registry_count, visited, resolving)) {
            if (err) {
                err->status = VIR_REGISTRY_ERR_UNREACHABLE_PASS;
                err->message = "Backend required states are unsatisfiable / unreachable.";
                err->state_mask = target;
            }
            free(visited);
            free(resolving);
            return VIR_REGISTRY_ERR_UNREACHABLE_PASS;
        }
        free(visited);
        free(resolving);
    }

    if (err) {
        err->status = VIR_REGISTRY_OK;
        err->message = "Registry passed validation successfully.";
    }
    return VIR_REGISTRY_OK;
}

/* ── Artifact Blob Serialization ─────────────────────────────────────────── */

int vir_artifact_serialize(const VirPath *path,
                            void **out_buffer,
                            size_t *out_size) {
    if (!path || !out_buffer || !out_size) {
        return 0;
    }

    size_t payload_size = path->count * sizeof(VirSegment);
    size_t total_size = sizeof(VirCacheRecordHeader) + payload_size;

    uint8_t *buf = (uint8_t *)malloc(total_size);
    if (!buf) {
        return 0;
    }

    VirCacheRecordHeader hdr = vir_cache_record_header_init(path, (uint32_t)payload_size);

    memcpy(buf, &hdr, sizeof(VirCacheRecordHeader));
    if (payload_size > 0 && path->segments) {
        memcpy(buf + sizeof(VirCacheRecordHeader), path->segments, payload_size);
    }

    *out_buffer = buf;
    *out_size = total_size;
    return 1;
}

int vir_artifact_validate(const void *buffer,
                          size_t size) {
    if (!buffer) {
        return 0;
    }
    if (size < sizeof(VirCacheRecordHeader)) {
        return 0;
    }

    const VirCacheRecordHeader *hdr = (const VirCacheRecordHeader *)buffer;

    if (!vir_cache_record_header_validate(hdr)) {
        return 0;
    }

    if (size != sizeof(VirCacheRecordHeader) + hdr->payload_size) {
        return 0;
    }

    if (hdr->segment_count > UINT32_MAX / sizeof(VirSegment)) {
        return 0;
    }

    if (hdr->payload_size != hdr->segment_count * sizeof(VirSegment)) {
        return 0;
    }

    return 1;
}

VirPath *vir_artifact_deserialize(const void *buffer,
                                  size_t size) {
    if (!vir_artifact_validate(buffer, size)) {
        return NULL;
    }

    const VirCacheRecordHeader *hdr = (const VirCacheRecordHeader *)buffer;
    uint32_t seg_count = hdr->segment_count;

    VirPath *path = vir_path_create();
    if (!path) {
        return NULL;
    }

    if (seg_count > 0) {
        if (seg_count > path->capacity) {
            VirSegment *new_segs = (VirSegment*)realloc(path->segments, seg_count * sizeof(VirSegment));
            if (!new_segs) {
                vir_path_free(path);
                return NULL;
            }
            path->segments = new_segs;
            path->capacity = seg_count;
        }

        const VirSegment *src = (const VirSegment *)((const uint8_t *)buffer + sizeof(VirCacheRecordHeader));
        memcpy(path->segments, src, seg_count * sizeof(VirSegment));
    }

    path->count = seg_count;
    path->state_flags = hdr->state_flags;

    path->min_x = hdr->min_x;
    path->min_y = hdr->min_y;
    path->max_x = hdr->max_x;
    path->max_y = hdr->max_y;

    if (hdr->state_flags & (VIR_STATE_BOUNDS_VALID | VIR_STATE_EXACT_BOUNDS)) {
        path->bounds_valid = 1;
    } else {
        path->bounds_valid = 0;
    }

    return path;
}

/* ── Content-Addressable Repository (Tier 1) ───────────────────────────────── */

#ifdef _WIN32
#include <direct.h>
#define mkdir_compat(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define mkdir_compat(path) mkdir(path, 0755)
#endif

static void vir_mkdir_p(const char *path) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len == 0) return;
    if (tmp[len - 1] == '/' || tmp[len - 1] == '\\') {
        tmp[len - 1] = 0;
    }
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char c = *p;
            *p = 0;
            mkdir_compat(tmp);
            *p = c;
        }
    }
    mkdir_compat(tmp);
}

static void vir_repository_resolve_path(const char *repo_path, uint64_t fingerprint, char *out_path, size_t max_len) {
    uint8_t prefix = (uint8_t)((fingerprint >> 56) & 0xFF);
    uint64_t suffix = fingerprint & 0x00FFFFFFFFFFFFFFULL;
    snprintf(out_path, max_len, "%s/v%d/%02x/%014lx.vir", repo_path, VIR_CACHE_SCHEMA_VERSION, prefix, (unsigned long)suffix);
}

static void vir_repository_ensure_dir(const char *repo_path, uint64_t fingerprint) {
    char dir_path[1024];
    uint8_t prefix = (uint8_t)((fingerprint >> 56) & 0xFF);
    snprintf(dir_path, sizeof(dir_path), "%s/v%d/%02x", repo_path, VIR_CACHE_SCHEMA_VERSION, prefix);
    vir_mkdir_p(dir_path);
}

int vir_repository_exists(const char *repo_path, uint64_t fingerprint) {
    if (!repo_path) return 0;
    char file_path[1024];
    vir_repository_resolve_path(repo_path, fingerprint, file_path, sizeof(file_path));
    FILE *f = fopen(file_path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

int vir_repository_store(const char *repo_path, const VirPath *path) {
    if (!repo_path || !path) return 0;

    uint64_t fingerprint = vir_path_canonical_fingerprint(path, 1e-3f);

    /* Check if already present to avoid redundant writes */
    if (vir_repository_exists(repo_path, fingerprint)) {
        return 1;
    }

    void *buf = NULL;
    size_t size = 0;
    if (!vir_artifact_serialize(path, &buf, &size)) {
        return 0;
    }

    vir_repository_ensure_dir(repo_path, fingerprint);

    char file_path[1024];
    vir_repository_resolve_path(repo_path, fingerprint, file_path, sizeof(file_path));

    FILE *f = fopen(file_path, "wb");
    if (!f) {
        free(buf);
        return 0;
    }

    size_t written = fwrite(buf, 1, size, f);
    fclose(f);
    free(buf);

    return written == size ? 1 : 0;
}

VirPath *vir_repository_load(const char *repo_path, uint64_t fingerprint) {
    if (!repo_path) return NULL;

    char file_path[1024];
    vir_repository_resolve_path(repo_path, fingerprint, file_path, sizeof(file_path));

    FILE *f = fopen(file_path, "rb");
    if (!f) return NULL;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }

    void *buf = malloc(size);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t read_bytes = fread(buf, 1, size, f);
    fclose(f);

    if (read_bytes != (size_t)size) {
        free(buf);
        return NULL;
    }

    VirPath *path = vir_artifact_deserialize(buf, read_bytes);
    free(buf);
    return path;
}

int vir_repository_remove(const char *repo_path, uint64_t fingerprint) {
    if (!repo_path) return 0;

    char file_path[1024];
    vir_repository_resolve_path(repo_path, fingerprint, file_path, sizeof(file_path));

    /* If file does not exist, removal is functionally successful (already absent). */
    FILE *f = fopen(file_path, "rb");
    if (!f) return 1;
    fclose(f);

    return remove(file_path) == 0 ? 1 : 0;
}



