#include "zcc_svg_path_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

#define EPSILON 0.001

static void free_svg_node_tree(ZccSvgNode* node) {
    if (!node) return;
    free_svg_node_tree(node->children);
    free_svg_node_tree(node->next);
    if (node->attributes) free(node->attributes);
    if (node->content) free(node->content);
    free(node);
}

static void test_successful_parsing() {
    printf("[*] Running test_successful_parsing...\n");
    
    // 1. Simple move and line
    {
        SvgPathBuilder *pb = svg_path_begin();
        ZccSvgError err = {0};
        ZccSvgStatus st = zcc_svg_parse_path("M 10,20 L 30,40", pb, &err);
        assert(st == ZCC_SVG_OK);
        assert(strcmp(pb->d, "M10.00,20.00 L30.00,40.00 ") == 0);
        ZccSvgNode* node = svg_create_node("path");
        svg_apply_path(node, pb); // cleans up pb
        free_svg_node_tree(node);
    }

    // 2. Relative offsets and commas/spaces
    {
        SvgPathBuilder *pb = svg_path_begin();
        ZccSvgError err = {0};
        ZccSvgStatus st = zcc_svg_parse_path("m10 10 l10 10", pb, &err);
        assert(st == ZCC_SVG_OK);
        assert(strcmp(pb->d, "M10.00,10.00 L20.00,20.00 ") == 0);
        ZccSvgNode* node = svg_create_node("path");
        svg_apply_path(node, pb);
        free_svg_node_tree(node);
    }

    // 3. Horizontal and vertical movements
    {
        SvgPathBuilder *pb = svg_path_begin();
        ZccSvgError err = {0};
        ZccSvgStatus st = zcc_svg_parse_path("M100 100 h-50 v50", pb, &err);
        assert(st == ZCC_SVG_OK);
        assert(strcmp(pb->d, "M100.00,100.00 L50.00,100.00 L50.00,150.00 ") == 0);
        ZccSvgNode* node = svg_create_node("path");
        svg_apply_path(node, pb);
        free_svg_node_tree(node);
    }

    // 4. Cubic Bezier curve
    {
        SvgPathBuilder *pb = svg_path_begin();
        ZccSvgError err = {0};
        ZccSvgStatus st = zcc_svg_parse_path("M 10 10 C 20 20, 30 30, 40 40", pb, &err);
        assert(st == ZCC_SVG_OK);
        assert(strcmp(pb->d, "M10.00,10.00 C20.00,20.00 30.00,30.00 40.00,40.00 ") == 0);
        ZccSvgNode* node = svg_create_node("path");
        svg_apply_path(node, pb);
        free_svg_node_tree(node);
    }

    // 5. Quadratic Bezier and implicit line sequence
    {
        SvgPathBuilder *pb = svg_path_begin();
        ZccSvgError err = {0};
        ZccSvgStatus st = zcc_svg_parse_path("M0 0 Q50 100 100 0", pb, &err);
        assert(st == ZCC_SVG_OK);
        assert(strcmp(pb->d, "M0.00,0.00 C33.33,66.67 66.67,66.67 100.00,0.00 ") == 0);
        ZccSvgNode* node = svg_create_node("path");
        svg_apply_path(node, pb);
        free_svg_node_tree(node);
    }

    // 6. Path closing
    {
        SvgPathBuilder *pb = svg_path_begin();
        ZccSvgError err = {0};
        ZccSvgStatus st = zcc_svg_parse_path("M 5 5 L 10 10 Z", pb, &err);
        assert(st == ZCC_SVG_OK);
        assert(strcmp(pb->d, "M5.00,5.00 L10.00,10.00 Z ") == 0);
        ZccSvgNode* node = svg_create_node("path");
        svg_apply_path(node, pb);
        free_svg_node_tree(node);
    }

    printf("[+] test_successful_parsing PASSED.\n");
}

static void test_malformed_paths() {
    printf("[*] Running test_malformed_paths...\n");

    // 1. Missing coordinates
    {
        SvgPathBuilder *pb = svg_path_begin();
        ZccSvgError err = {0};
        ZccSvgStatus st = zcc_svg_parse_path("M 10", pb, &err);
        assert(st == ZCC_SVG_ERR_BAD_NUMBER);
        assert(err.offset == 4);
        ZccSvgNode* node = svg_create_node("path");
        svg_apply_path(node, pb);
        free_svg_node_tree(node);
    }

    // 2. Invalid command letter
    {
        SvgPathBuilder *pb = svg_path_begin();
        ZccSvgError err = {0};
        ZccSvgStatus st = zcc_svg_parse_path("X 10 20", pb, &err);
        assert(st == ZCC_SVG_ERR_BAD_COMMAND);
        assert(err.offset == 0);
        ZccSvgNode* node = svg_create_node("path");
        svg_apply_path(node, pb);
        free_svg_node_tree(node);
    }

    // 3. Unsupported elliptical arc
    {
        SvgPathBuilder *pb = svg_path_begin();
        ZccSvgError err = {0};
        ZccSvgStatus st = zcc_svg_parse_path("M10 20 A 5 5 0 0 0 15 15", pb, &err);
        assert(st == ZCC_SVG_ERR_UNSUPPORTED_ARC);
        assert(err.offset == 7);
        ZccSvgNode* node = svg_create_node("path");
        svg_apply_path(node, pb);
        free_svg_node_tree(node);
    }

    // 4. Unsupported smooth curves (S/T)
    {
        SvgPathBuilder *pb = svg_path_begin();
        ZccSvgError err = {0};
        ZccSvgStatus st = zcc_svg_parse_path("M10 20 S 15 15 20 20", pb, &err);
        assert(st == ZCC_SVG_ERR_UNSUPPORTED_COMMAND);
        assert(err.offset == 7);
        ZccSvgNode* node = svg_create_node("path");
        svg_apply_path(node, pb);
        free_svg_node_tree(node);
    }

    printf("[+] test_malformed_paths PASSED.\n");
}

static void test_extreme_inputs() {
    printf("[*] Running test_extreme_inputs...\n");

    // 1. Out of range coordinates
    {
        SvgPathBuilder *pb = svg_path_begin();
        ZccSvgError err = {0};
        ZccSvgStatus st = zcc_svg_parse_path("M 99999999999.0 0.0", pb, &err);
        assert(st == ZCC_SVG_ERR_BAD_NUMBER);
        ZccSvgNode* node = svg_create_node("path");
        svg_apply_path(node, pb);
        free_svg_node_tree(node);
    }

    // 2. Infinite coordinates (division by zero expression)
    {
        SvgPathBuilder *pb = svg_path_begin();
        ZccSvgError err = {0};
        ZccSvgStatus st = zcc_svg_parse_path("M 1e9999 0", pb, &err);
        assert(st == ZCC_SVG_ERR_BAD_NUMBER);
        ZccSvgNode* node = svg_create_node("path");
        svg_apply_path(node, pb);
        free_svg_node_tree(node);
    }

    // 3. Segment limits exceeded
    {
        SvgPathBuilder *pb = svg_path_begin();
        ZccSvgError err = {0};
        // Build path string with > 100,000 commands
        size_t count = 100005;
        char *huge_path = (char*)malloc(count * 8 + 32);
        strcpy(huge_path, "M0 0");
        size_t len = 4;
        for (size_t i = 0; i < count; i++) {
            len += sprintf(huge_path + len, " L1 1");
        }
        ZccSvgStatus st = zcc_svg_parse_path(huge_path, pb, &err);
        assert(st == ZCC_SVG_ERR_PATH_OVERFLOW);
        free(huge_path);
        ZccSvgNode* node = svg_create_node("path");
        svg_apply_path(node, pb);
        free_svg_node_tree(node);
    }

    printf("[+] test_extreme_inputs PASSED.\n");
}

int main() {
    printf("=== ZCC SVG Path Ingestion Test Harness ===\n");
    test_successful_parsing();
    test_malformed_paths();
    test_extreme_inputs();
    printf("777JACKPOT777 — ALL INGESTION TESTS GREEN.\n");
    return 0;
}
