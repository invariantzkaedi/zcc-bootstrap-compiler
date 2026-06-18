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
        path->state_flags &= ~VIR_STATE_BOUNDS_VALID;
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

SdfSeed* vir_to_sdf_seed(const VirPath *path) {
    if (!path) return NULL;

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
            float c1x = s->coords[0];
            float c1y = s->coords[1];
            float c2x = s->coords[2];
            float c2y = s->coords[3];
            float tx  = s->coords[4];
            float ty  = s->coords[5];
            if (has_subpath_start) {
                if (!sdf_seed_add_cubic(seed, &capacity, cx, cy, c1x, c1y, c2x, c2y, tx, ty)) {
                    sdf_seed_free(seed);
                    vir_path_free(expanded);
                    return NULL;
                }
            }
            cx = tx;
            cy = ty;
        } else if (s->op == VIR_CLOSE) {
            if (has_subpath_start) {
                if (fabsf(cx - start_x) > 1e-5f || fabsf(cy - start_y) > 1e-5f) {
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

char* sdf_seed_to_glsl(const SdfSeed *seed) {
    if (!seed) return NULL;

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
            if (len + n + 64 >= capacity) {
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
      VIR_STATE_CLEAN, VIR_STATE_DEGENERATE_FREE, VIR_STATE_BOUNDS_VALID | VIR_STATE_CANONICALIZED },
    { VIR_PASS_EXPAND_ARCS, "expand_arcs", vir_path_expand_arcs, 0, 0, 0,
      VIR_STATE_CLEAN, VIR_STATE_ARCS_EXPANDED, VIR_STATE_BOUNDS_VALID | VIR_STATE_CANONICALIZED },
    { VIR_PASS_COMPUTE_BOUNDS, "bounds", vir_path_compute_bounds_pass, 0, 0, 0,
      VIR_STATE_CLEAN, VIR_STATE_BOUNDS_VALID, 0 },
    { VIR_PASS_CANONICALIZE, "canonicalize", vir_path_canonicalize, 0, 0, 0,
      VIR_STATE_ARCS_EXPANDED, VIR_STATE_CANONICALIZED, VIR_STATE_BOUNDS_VALID }
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
