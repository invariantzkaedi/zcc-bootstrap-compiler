#include "zcc_vir.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

#define EPSILON 1e-2f

static void free_svg_node_tree(ZccSvgNode* node) {
    if (!node) return;
    free_svg_node_tree(node->children);
    free_svg_node_tree(node->next);
    if (node->attributes) free(node->attributes);
    if (node->content) free(node->content);
    free(node);
}

static void test_vir_path_creation_and_growth() {
    printf("[*] Running test_vir_path_creation_and_growth...\n");
    VirPath *path = vir_path_create();
    assert(path != NULL);
    assert(path->count == 0);
    assert(path->capacity == 16);

    // Add 20 moves to force realloc (capacity 16 -> 32)
    for (int i = 0; i < 20; i++) {
        int res = vir_path_add_move_to(path, (float)i, (float)(i * 2));
        assert(res == 1);
    }
    assert(path->count == 20);
    assert(path->capacity == 32);

    for (int i = 0; i < 20; i++) {
        assert(path->segments[i].op == VIR_MOVE);
        assert(fabsf(path->segments[i].coords[0] - (float)i) < EPSILON);
        assert(fabsf(path->segments[i].coords[1] - (float)(i * 2)) < EPSILON);
    }

    vir_path_free(path);
    printf("[+] test_vir_path_creation_and_growth PASSED.\n");
}

static void test_vir_degenerate_removal() {
    printf("[*] Running test_vir_degenerate_removal...\n");

    // Case 1: Simple duplicate line
    {
        VirPath *path = vir_path_create();
        vir_path_add_move_to(path, 10.0f, 10.0f);
        vir_path_add_line_to(path, 10.0f, 10.0f); // Degenerate line to same point
        vir_path_add_line_to(path, 20.0f, 20.0f);
        vir_path_add_line_to(path, 20.0f, 20.0f); // Degenerate line to same point

        vir_path_optimize_degenerate(path);

        assert(path->count == 2);
        assert(path->segments[0].op == VIR_MOVE);
        assert(fabsf(path->segments[0].coords[0] - 10.0f) < EPSILON);
        assert(path->segments[1].op == VIR_LINE);
        assert(fabsf(path->segments[1].coords[0] - 20.0f) < EPSILON);

        vir_path_free(path);
    }

    // Case 2: Degenerate Cubic
    {
        VirPath *path = vir_path_create();
        vir_path_add_move_to(path, 5.0f, 5.0f);
        vir_path_add_cubic_to(path, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f); // Degenerate cubic
        vir_path_add_cubic_to(path, 6.0f, 6.0f, 7.0f, 7.0f, 8.0f, 8.0f); // Non-degenerate cubic

        vir_path_optimize_degenerate(path);

        assert(path->count == 2);
        assert(path->segments[0].op == VIR_MOVE);
        assert(path->segments[1].op == VIR_CUBIC);
        assert(fabsf(path->segments[1].coords[4] - 8.0f) < EPSILON);

        vir_path_free(path);
    }

    // Case 3: Close command resetting current point
    {
        VirPath *path = vir_path_create();
        vir_path_add_move_to(path, 0.0f, 0.0f);
        vir_path_add_line_to(path, 10.0f, 0.0f);
        vir_path_add_close(path); // returns to 0,0
        vir_path_add_line_to(path, 0.0f, 0.0f); // Degenerate, already at 0,0
        vir_path_add_line_to(path, 0.0f, 5.0f); // Non-degenerate

        vir_path_optimize_degenerate(path);

        assert(path->count == 4);
        assert(path->segments[0].op == VIR_MOVE);
        assert(path->segments[1].op == VIR_LINE);
        assert(path->segments[2].op == VIR_CLOSE);
        assert(path->segments[3].op == VIR_LINE);
        assert(fabsf(path->segments[3].coords[1] - 5.0f) < EPSILON);

        vir_path_free(path);
    }

    printf("[+] test_vir_degenerate_removal PASSED.\n");
}

static void test_vir_bounds_propagation() {
    printf("[*] Running test_vir_bounds_propagation...\n");

    // Case 1: Empty path
    {
        VirPath *path = vir_path_create();
        float min_x, min_y, max_x, max_y;
        vir_path_compute_bounds(path, &min_x, &min_y, &max_x, &max_y);
        assert(fabsf(min_x) < EPSILON);
        assert(fabsf(min_y) < EPSILON);
        assert(fabsf(max_x) < EPSILON);
        assert(fabsf(max_y) < EPSILON);
        vir_path_free(path);
    }

    // Case 2: Simple rectangle path
    {
        VirPath *path = vir_path_create();
        vir_path_add_move_to(path, -10.0f, 5.0f);
        vir_path_add_line_to(path, 20.0f, -15.0f);
        vir_path_add_line_to(path, 30.0f, 40.0f);

        float min_x, min_y, max_x, max_y;
        vir_path_compute_bounds(path, &min_x, &min_y, &max_x, &max_y);
        assert(fabsf(min_x - (-10.0f)) < EPSILON);
        assert(fabsf(min_y - (-15.0f)) < EPSILON);
        assert(fabsf(max_x - 30.0f) < EPSILON);
        assert(fabsf(max_y - 40.0f) < EPSILON);

        vir_path_free(path);
    }

    // Case 3: Path containing curves (Cubic control points should influence bounds in our bounding box pass)
    {
        VirPath *path = vir_path_create();
        vir_path_add_move_to(path, 0.0f, 0.0f);
        vir_path_add_cubic_to(path, -50.0f, 100.0f, 150.0f, -200.0f, 10.0f, 10.0f);

        float min_x, min_y, max_x, max_y;
        vir_path_compute_bounds(path, &min_x, &min_y, &max_x, &max_y);
        assert(fabsf(min_x - (-50.0f)) < EPSILON);
        assert(fabsf(min_y - (-200.0f)) < EPSILON);
        assert(fabsf(max_x - 150.0f) < EPSILON);
        assert(fabsf(max_y - 100.0f) < EPSILON);

        vir_path_free(path);
    }

    printf("[+] test_vir_bounds_propagation PASSED.\n");
}

static void test_svg_to_vir_adapter() {
    printf("[*] Running test_svg_to_vir_adapter...\n");

    // Case 1: Complex path parsing into VIR, then adapting to SVG builder
    {
        VirPath *path = vir_path_create();
        ZccSvgError err = {0};
        ZccSvgStatus st = zcc_svg_parse_to_vir("M 10,20 L 30,40 H 50 V 60 C 70 80, 90 100, 110 120 Q 130 140 150 160 Z", path, &err);
        assert(st == ZCC_SVG_OK);

        // Verify the segment operations count
        // M (1), L (2), H (3), V (4), C (5), Q -> elevated to C (6), Z (7)
        assert(path->count == 7);

        assert(path->segments[0].op == VIR_MOVE);
        assert(path->segments[1].op == VIR_LINE);
        assert(path->segments[2].op == VIR_LINE); // adapted from H
        assert(path->segments[3].op == VIR_LINE); // adapted from V
        assert(path->segments[4].op == VIR_CUBIC);
        assert(path->segments[5].op == VIR_CUBIC); // adapted from Q via degree elevation
        assert(path->segments[6].op == VIR_CLOSE);

        // Compute bounds
        float min_x, min_y, max_x, max_y;
        vir_path_compute_bounds(path, &min_x, &min_y, &max_x, &max_y);
        assert(fabsf(min_x - 10.0f) < EPSILON);
        assert(fabsf(min_y - 20.0f) < EPSILON);
        assert(fabsf(max_x - 150.0f) < EPSILON);
        assert(fabsf(max_y - 160.0f) < EPSILON);

        // Serialize to SvgPathBuilder
        SvgPathBuilder *pb = svg_path_begin();
        vir_path_to_builder(path, pb);
        assert(strcmp(pb->d, "M10.00,20.00 L30.00,40.00 L50.00,40.00 L50.00,60.00 C70.00,80.00 90.00,100.00 110.00,120.00 C123.33,133.33 136.67,146.67 150.00,160.00 Z ") == 0);

        ZccSvgNode* node = svg_create_node("path");
        svg_apply_path(node, pb);
        free_svg_node_tree(node);
        vir_path_free(path);
    }

    printf("[+] test_svg_to_vir_adapter PASSED.\n");
}

static void test_extreme_and_overflow_vir() {
    printf("[*] Running test_extreme_and_overflow_vir...\n");

    // Capacity overflow in parse
    {
        VirPath *path = vir_path_create();
        ZccSvgError err = {0};
        size_t count = 100005;
        char *huge_path = (char*)malloc(count * 8 + 32);
        strcpy(huge_path, "M0 0");
        size_t len = 4;
        for (size_t i = 0; i < count; i++) {
            len += sprintf(huge_path + len, " L1 1");
        }
        ZccSvgStatus st = zcc_svg_parse_to_vir(huge_path, path, &err);
        assert(st == ZCC_SVG_ERR_PATH_OVERFLOW);
        free(huge_path);
        vir_path_free(path);
    }

    printf("[+] test_extreme_and_overflow_vir PASSED.\n");
}

static void test_vir_metadata_and_bounds_caching() {
    printf("[*] Running test_vir_metadata_and_bounds_caching...\n");

    VirPath *path = vir_path_create();
    path->metadata.source_id = 42;
    path->metadata.path_id = 999;
    path->metadata.flags = 0xabcdef;

    assert(path->metadata.source_id == 42);
    assert(path->metadata.path_id == 999);
    assert(path->metadata.flags == 0xabcdef);

    // Initial bounds validity
    assert(path->bounds_valid == 0);

    vir_path_add_move_to(path, 10.0f, 10.0f);
    vir_path_add_line_to(path, 20.0f, 30.0f);

    assert(path->bounds_valid == 0);

    float min_x, min_y, max_x, max_y;
    vir_path_compute_bounds(path, &min_x, &min_y, &max_x, &max_y);

    assert(path->bounds_valid == 1);
    assert(fabsf(path->min_x - 10.0f) < EPSILON);
    assert(fabsf(path->min_y - 10.0f) < EPSILON);
    assert(fabsf(path->max_x - 20.0f) < EPSILON);
    assert(fabsf(path->max_y - 30.0f) < EPSILON);

    // Make bounds query again, should return cached
    float cached_min_x, cached_min_y, cached_max_x, cached_max_y;
    vir_path_compute_bounds(path, &cached_min_x, &cached_min_y, &cached_max_x, &cached_max_y);
    assert(fabsf(cached_min_x - 10.0f) < EPSILON);
    assert(fabsf(cached_max_y - 30.0f) < EPSILON);

    // Modify geometry, bounds should invalidate
    vir_path_add_line_to(path, -50.0f, 100.0f);
    assert(path->bounds_valid == 0);

    vir_path_compute_bounds(path, &min_x, &min_y, &max_x, &max_y);
    assert(path->bounds_valid == 1);
    assert(fabsf(path->min_x - (-50.0f)) < EPSILON);
    assert(fabsf(path->max_y - 100.0f) < EPSILON);

    vir_path_free(path);
    printf("[+] test_vir_metadata_and_bounds_caching PASSED.\n");
}

static void test_vir_arc_ingestion_and_expansion() {
    printf("[*] Running test_vir_arc_ingestion_and_expansion...\n");

    // Case 1: Simple circle arc parsing and expansion
    {
        VirPath *path = vir_path_create();
        ZccSvgError err = {0};
        // Parse a 90-degree circular arc segment (radius 50) from (0,0) to (50,50)
        ZccSvgStatus st = zcc_svg_parse_to_vir("M 0 0 A 50 50 0 0 1 50 50", path, &err);
        assert(st == ZCC_SVG_OK);

        assert(path->count == 2);
        assert(path->segments[0].op == VIR_MOVE);
        assert(path->segments[1].op == VIR_ARC);

        // Verify bounds of arc end point is mapped
        float min_x, min_y, max_x, max_y;
        vir_path_compute_bounds(path, &min_x, &min_y, &max_x, &max_y);
        assert(fabsf(min_x - 0.0f) < EPSILON);
        assert(fabsf(max_y - 50.0f) < EPSILON);

        // Expand arc pass
        vir_path_expand_arcs(path);

        // Verify the VIR_ARC segment was expanded to VIR_CUBIC segments
        assert(path->count > 2);
        assert(path->segments[0].op == VIR_MOVE);
        for (size_t i = 1; i < path->count; i++) {
            assert(path->segments[i].op == VIR_CUBIC);
        }

        // Verify serialization works correctly
        SvgPathBuilder *pb = svg_path_begin();
        vir_path_to_builder(path, pb);
        // Verify output string contains C curves and ends at 50,50
        assert(strstr(pb->d, "C") != NULL);
        assert(strstr(pb->d, "50.00,50.00") != NULL);

        ZccSvgNode* node = svg_create_node("path");
        svg_apply_path(node, pb);
        free_svg_node_tree(node);
        vir_path_free(path);
    }

    printf("[+] test_vir_arc_ingestion_and_expansion PASSED.\n");
}

int main() {
    printf("=== ZCC Vector IR (VIR) Test Harness ===\n");
    test_vir_path_creation_and_growth();
    test_vir_degenerate_removal();
    test_vir_bounds_propagation();
    test_svg_to_vir_adapter();
    test_extreme_and_overflow_vir();
    test_vir_metadata_and_bounds_caching();
    test_vir_arc_ingestion_and_expansion();
    printf("777JACKPOT777 — ALL VIR CORE TESTS GREEN.\n");
    return 0;
}
