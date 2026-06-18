#include "zcc_vir.h"
#include <stdlib.h>
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

VirPath* vir_path_create(void) {
    VirPath *p = (VirPath*)calloc(1, sizeof(VirPath));
    p->capacity = 16;
    p->segments = (VirSegment*)calloc(p->capacity, sizeof(VirSegment));
    p->count = 0;
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
    return 1;
}

int vir_path_add_line_to(VirPath *path, float x, float y) {
    if (!vir_path_ensure_capacity(path)) return 0;
    VirSegment *s = &path->segments[path->count++];
    s->op = VIR_LINE;
    s->coords[0] = x;
    s->coords[1] = y;
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
    return 1;
}

int vir_path_add_close(VirPath *path) {
    if (!vir_path_ensure_capacity(path)) return 0;
    VirSegment *s = &path->segments[path->count++];
    s->op = VIR_CLOSE;
    return 1;
}

void vir_path_optimize_degenerate(VirPath *path) {
    if (!path || path->count == 0) return;

    size_t write_idx = 0;
    float cx = 0.0f, cy = 0.0f;
    float start_x = 0.0f, start_y = 0.0f;

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
    path->count = write_idx;
}

void vir_path_compute_bounds(const VirPath *path, float *min_x, float *min_y, float *max_x, float *max_y) {
    if (!path || path->count == 0 || !min_x || !min_y || !max_x || !max_y) {
        if (min_x) *min_x = 0;
        if (min_y) *min_y = 0;
        if (max_x) *max_x = 0;
        if (max_y) *max_y = 0;
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
        }
    }
    #undef UPDATE_BOUNDS

    if (has_points) {
        *min_x = mix;
        *min_y = miy;
        *max_x = max_val_x;
        *max_y = max_val_y;
    } else {
        *min_x = 0; *min_y = 0; *max_x = 0; *max_y = 0;
    }
}

void vir_path_to_builder(const VirPath *path, SvgPathBuilder *out) {
    if (!path || !out) return;
    for (size_t i = 0; i < path->count; i++) {
        const VirSegment *s = &path->segments[i];
        if (s->op == VIR_MOVE) {
            svg_path_move_to(out, s->coords[0], s->coords[1]);
        } else if (s->op == VIR_LINE) {
            svg_path_line_to(out, s->coords[0], s->coords[1]);
        } else if (s->op == VIR_CUBIC) {
            svg_path_cubic_to(out, s->coords[0], s->coords[1], s->coords[2], s->coords[3], s->coords[4], s->coords[5]);
        } else if (s->op == VIR_CLOSE) {
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
            return zcc_svg_fail(err, ZCC_SVG_ERR_UNSUPPORTED_ARC,
                "Unsupported SVG elliptical arc command", ZCC_SVG_OFFSET(d, cmd_ptr));

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
