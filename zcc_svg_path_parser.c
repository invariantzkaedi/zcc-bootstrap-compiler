#include "zcc_svg_path_parser.h"
#include <stdlib.h>
#include <math.h>

#define ZCC_SVG_MAX_PATH_BUILDER_CAP (4 * 1024 * 1024) // 4MB memory limit per path
#define ZCC_SVG_MAX_SEGMENTS 100000
#define ZCC_SVG_MAX_COORD_ABS 100000000.0

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

static int svg_path_builder_can_append(const SvgPathBuilder *b, size_t n) {
    if (!b) return 0;
    if ((size_t)b->len + n + 64 > ZCC_SVG_MAX_PATH_BUILDER_CAP) {
        return 0;
    }
    return 1;
}

static int zcc_is_valid_coord(double v) {
    return isfinite(v) && (fabs(v) <= ZCC_SVG_MAX_COORD_ABS);
}

static ZccSvgStatus append_move(SvgPathBuilder *out, float x, float y, ZccSvgError *err, size_t offset) {
    if (!svg_path_builder_can_append(out, 64)) {
        return zcc_svg_fail(err, ZCC_SVG_ERR_PATH_OVERFLOW, "SvgPathBuilder capacity exceeded", offset);
    }
    svg_path_move_to(out, x, y);
    return ZCC_SVG_OK;
}

static ZccSvgStatus append_line(SvgPathBuilder *out, float x, float y, ZccSvgError *err, size_t offset) {
    if (!svg_path_builder_can_append(out, 64)) {
        return zcc_svg_fail(err, ZCC_SVG_ERR_PATH_OVERFLOW, "SvgPathBuilder capacity exceeded", offset);
    }
    svg_path_line_to(out, x, y);
    return ZCC_SVG_OK;
}

static ZccSvgStatus append_cubic(SvgPathBuilder *out, float x1, float y1, float x2, float y2, float x, float y, ZccSvgError *err, size_t offset) {
    if (!svg_path_builder_can_append(out, 128)) {
        return zcc_svg_fail(err, ZCC_SVG_ERR_PATH_OVERFLOW, "SvgPathBuilder capacity exceeded", offset);
    }
    svg_path_cubic_to(out, x1, y1, x2, y2, x, y);
    return ZCC_SVG_OK;
}

static ZccSvgStatus append_close(SvgPathBuilder *out, ZccSvgError *err, size_t offset) {
    if (!svg_path_builder_can_append(out, 16)) {
        return zcc_svg_fail(err, ZCC_SVG_ERR_PATH_OVERFLOW, "SvgPathBuilder capacity exceeded", offset);
    }
    svg_path_close(out);
    return ZCC_SVG_OK;
}

static void skip_spaces_and_commas(const char **p) {
    while (**p == ' ' || **p == '\t' || **p == '\r' || **p == '\n' || **p == ',') {
        (*p)++;
    }
}

static int is_number_start(char c) {
    return (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.';
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

ZccSvgStatus zcc_svg_parse_path(const char *d, SvgPathBuilder *out, ZccSvgError *err) {
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

        if (++segment_count > ZCC_SVG_MAX_SEGMENTS) {
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
            ZccSvgStatus st = append_move(out, (float)cx, (float)cy, err, ZCC_SVG_OFFSET(d, p));
            if (st != ZCC_SVG_OK) return st;
            
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
            ZccSvgStatus st = append_line(out, (float)cx, (float)cy, err, ZCC_SVG_OFFSET(d, p));
            if (st != ZCC_SVG_OK) return st;

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
            ZccSvgStatus st = append_line(out, (float)cx, (float)cy, err, ZCC_SVG_OFFSET(d, p));
            if (st != ZCC_SVG_OK) return st;

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
            ZccSvgStatus st = append_line(out, (float)cx, (float)cy, err, ZCC_SVG_OFFSET(d, p));
            if (st != ZCC_SVG_OK) return st;

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
            ZccSvgStatus st = append_cubic(out, (float)x1, (float)y1, (float)x2, (float)y2, (float)x, (float)y, err, ZCC_SVG_OFFSET(d, p));
            if (st != ZCC_SVG_OK) return st;
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

            ZccSvgStatus st = append_cubic(out, (float)c1x, (float)c1y, (float)c2x, (float)c2y, (float)x, (float)y, err, ZCC_SVG_OFFSET(d, p));
            if (st != ZCC_SVG_OK) return st;
            cx = x;
            cy = y;

        } else if (cmd == 'Z' || cmd == 'z') {
            ZccSvgStatus st = append_close(out, err, ZCC_SVG_OFFSET(d, p));
            if (st != ZCC_SVG_OK) return st;
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
