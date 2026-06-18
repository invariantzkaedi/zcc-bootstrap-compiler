#ifndef ZCC_SVG_PATH_PARSER_H
#define ZCC_SVG_PATH_PARSER_H

#include "zcc_svg.h"
#include <errno.h>
#include <stddef.h>

typedef enum {
    ZCC_SVG_OK = 0,
    ZCC_SVG_ERR_NULL_INPUT,
    ZCC_SVG_ERR_BAD_COMMAND,
    ZCC_SVG_ERR_BAD_NUMBER,
    ZCC_SVG_ERR_UNSUPPORTED_COMMAND,
    ZCC_SVG_ERR_UNSUPPORTED_ARC,
    ZCC_SVG_ERR_PATH_OVERFLOW
} ZccSvgStatus;

typedef struct {
    ZccSvgStatus status;
    const char *message;
    size_t offset;
} ZccSvgError;

#define ZCC_SVG_OFFSET(d, p) ((size_t)((p) - (d)))

/**
 * Parses an SVG path data string ('d' attribute) and appends absolute commands
 * to the provided SvgPathBuilder. Resolves relative movements and implicitly
 * elevates Quadratic Beziers (Q/q) to Cubic Beziers (C/c).
 */
ZccSvgStatus zcc_svg_parse_path(
    const char *d,
    SvgPathBuilder *out,
    ZccSvgError *err
);

#endif
