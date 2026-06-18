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

        VirPassResult r1 = vir_path_optimize_degenerate(path);
        assert(r1 == VIR_PASS_OK);
        VirPassResult r2 = vir_path_optimize_degenerate(path);
        assert(r2 == VIR_PASS_NO_CHANGE);

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

        VirPassResult r1 = vir_path_optimize_degenerate(path);
        assert(r1 == VIR_PASS_OK);
        VirPassResult r2 = vir_path_optimize_degenerate(path);
        assert(r2 == VIR_PASS_NO_CHANGE);

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

        VirPassResult r1 = vir_path_optimize_degenerate(path);
        assert(r1 == VIR_PASS_OK);
        VirPassResult r2 = vir_path_optimize_degenerate(path);
        assert(r2 == VIR_PASS_NO_CHANGE);

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
        VirPassResult r1 = vir_path_expand_arcs(path);
        assert(r1 == VIR_PASS_OK);
        VirPassResult r2 = vir_path_expand_arcs(path);
        assert(r2 == VIR_PASS_NO_CHANGE);

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

static void test_vir_backend_diversification() {
    printf("[*] Running test_vir_backend_diversification...\n");

    // Case 1: Simple shape to check SVG and SDF serialization
    {
        VirPath *path = vir_path_create();
        ZccSvgError err = {0};
        // Close does not meet start (from 10,10 to 0,0) -> should insert SDF line from 10,10 to 0,0
        ZccSvgStatus st = zcc_svg_parse_to_vir("M 0 0 L 10 0 L 10 10 Z", path, &err);
        assert(st == ZCC_SVG_OK);

        // Verify SVG path data serialization
        char *svg_data = vir_to_svg_path_data(path);
        assert(svg_data != NULL);
        assert(strcmp(svg_data, "M0.00,0.00 L10.00,0.00 L10.00,10.00 Z") == 0);
        free(svg_data);

        // Verify SDF seed generation
        SdfSeed *seed = vir_to_sdf_seed(path);
        assert(seed != NULL);
        // We have 3 segments: (0,0 -> 10,0), (10,0 -> 10,10), and the closing line (10,10 -> 0,0)
        assert(seed->count == 3);
        assert(seed->segments[0].op == SDF_LINE);
        assert(fabsf(seed->segments[0].points[0] - 0.0f) < EPSILON);
        assert(fabsf(seed->segments[0].points[1] - 0.0f) < EPSILON);
        assert(fabsf(seed->segments[0].points[2] - 10.0f) < EPSILON);
        assert(fabsf(seed->segments[0].points[3] - 0.0f) < EPSILON);

        assert(seed->segments[1].op == SDF_LINE);
        assert(fabsf(seed->segments[1].points[0] - 10.0f) < EPSILON);
        assert(fabsf(seed->segments[1].points[1] - 0.0f) < EPSILON);
        assert(fabsf(seed->segments[1].points[2] - 10.0f) < EPSILON);
        assert(fabsf(seed->segments[1].points[3] - 10.0f) < EPSILON);

        assert(seed->segments[2].op == SDF_LINE);
        assert(fabsf(seed->segments[2].points[0] - 10.0f) < EPSILON);
        assert(fabsf(seed->segments[2].points[1] - 10.0f) < EPSILON);
        assert(fabsf(seed->segments[2].points[2] - 0.0f) < EPSILON);
        assert(fabsf(seed->segments[2].points[3] - 0.0f) < EPSILON);

        sdf_seed_free(seed);
        vir_path_free(path);
    }

    // Case 2: Path containing arcs and curves
    {
        VirPath *path = vir_path_create();
        ZccSvgError err = {0};
        // 90-degree circular arc segment (radius 50) from (0,0) to (50,50)
        ZccSvgStatus st = zcc_svg_parse_to_vir("M 0 0 A 50 50 0 0 1 50 50", path, &err);
        assert(st == ZCC_SVG_OK);

        // Serialize directly: SVG data should contain 'A' command
        char *svg_data = vir_to_svg_path_data(path);
        assert(svg_data != NULL);
        assert(strstr(svg_data, "A50.00,50.00 0.00 0 1 50.00,50.00") != NULL);
        free(svg_data);

        // SDF seed generation should expand the arc into cubics
        SdfSeed *seed = vir_to_sdf_seed(path);
        assert(seed != NULL);
        assert(seed->count > 0);
        for (size_t i = 0; i < seed->count; i++) {
            assert(seed->segments[i].op == SDF_CUBIC);
        }
        // Verify final point matches (50, 50)
        size_t last = seed->count - 1;
        assert(fabsf(seed->segments[last].points[6] - 50.0f) < EPSILON);
        assert(fabsf(seed->segments[last].points[7] - 50.0f) < EPSILON);

        sdf_seed_free(seed);
        vir_path_free(path);
    }

    printf("[+] test_vir_backend_diversification PASSED.\n");
}

static void test_sdf_to_glsl_compilation() {
    printf("[*] Running test_sdf_to_glsl_compilation...\n");

    VirPath *path = vir_path_create();
    ZccSvgError err = {0};
    ZccSvgStatus st = zcc_svg_parse_to_vir("M 0 0 L 10 20 C 30 40 50 60 70 80", path, &err);
    assert(st == ZCC_SVG_OK);

    SdfSeed *seed = vir_to_sdf_seed(path);
    assert(seed != NULL);

    // Verify SdfBounds computation
    SdfBounds b = sdf_seed_compute_bounds(seed);
    assert(fabsf(b.min_x - 0.0f) < EPSILON);
    assert(fabsf(b.min_y - 0.0f) < EPSILON);
    assert(fabsf(b.max_x - 70.0f) < EPSILON);
    assert(fabsf(b.max_y - 80.0f) < EPSILON);

    // Compile to GLSL code
    char *glsl = sdf_seed_to_glsl(seed);
    assert(glsl != NULL);

    // Verify the GLSL contains shader code and the coordinates
    assert(strstr(glsl, "float sdLine(vec2 p, vec2 a, vec2 b)") != NULL);
    assert(strstr(glsl, "float sdCubicBezier(vec2 p, vec2 p0, vec2 p1, vec2 p2, vec2 p3)") != NULL);
    assert(strstr(glsl, "float sdf_shape(vec2 p)") != NULL);
    assert(strstr(glsl, "sdLine(p, vec2(0.0000, 0.0000), vec2(10.0000, 20.0000))") != NULL);
    assert(strstr(glsl, "sdCubicBezier(p, vec2(10.0000, 20.0000), vec2(30.0000, 40.0000), vec2(50.0000, 60.0000), vec2(70.0000, 80.0000))") != NULL);

    free(glsl);
    sdf_seed_free(seed);
    vir_path_free(path);

    printf("[+] test_sdf_to_glsl_compilation PASSED.\n");
}

static void test_vir_pass_canonicalize() {
    printf("[*] Running test_vir_pass_canonicalize...\n");

    VirPath *path = vir_path_create();
    ZccSvgError err = {0};
    ZccSvgStatus st = zcc_svg_parse_to_vir("M 0 0 L 30 60 A 50 50 0 0 1 50 50 Z", path, &err);
    assert(st == ZCC_SVG_OK);

    VirPassResult r1 = vir_path_canonicalize(path);
    assert(r1 == VIR_PASS_OK);
    VirPassResult r2 = vir_path_canonicalize(path);
    assert(r2 == VIR_PASS_NO_CHANGE);

    // Verify all segments are only MOVE, CUBIC, or CLOSE
    for (size_t i = 0; i < path->count; i++) {
        assert(path->segments[i].op != VIR_ARC);
        assert(path->segments[i].op != VIR_LINE);
        assert(path->segments[i].op == VIR_MOVE || path->segments[i].op == VIR_CUBIC || path->segments[i].op == VIR_CLOSE);
    }

    // Verify line elevation math
    // The first line was from (0,0) to (30,60).
    // It should be elevated to a CUBIC segment.
    assert(path->segments[1].op == VIR_CUBIC);
    // Control point 1: 0 + 1/3 * 30 = 10, 0 + 1/3 * 60 = 20
    assert(fabsf(path->segments[1].coords[0] - 10.0f) < EPSILON);
    assert(fabsf(path->segments[1].coords[1] - 20.0f) < EPSILON);
    // Control point 2: 0 + 2/3 * 30 = 20, 0 + 2/3 * 60 = 40
    assert(fabsf(path->segments[1].coords[2] - 20.0f) < EPSILON);
    assert(fabsf(path->segments[1].coords[3] - 40.0f) < EPSILON);
    // End point: 30, 60
    assert(fabsf(path->segments[1].coords[4] - 30.0f) < EPSILON);
    assert(fabsf(path->segments[1].coords[5] - 60.0f) < EPSILON);

    vir_path_free(path);
    printf("[+] test_vir_pass_canonicalize PASSED.\n");
}

static void test_vir_pass_manager() {
    printf("[*] Running test_vir_pass_manager...\n");

    VirPath *path = vir_path_create();
    ZccSvgError err = {0};
    ZccSvgStatus st = zcc_svg_parse_to_vir("M 0 0 L 0 0 A 50 50 0 0 1 50 50 Z", path, &err);
    assert(st == ZCC_SVG_OK);

    assert(path->count == 4);
    assert(path->bounds_valid == 0);

    VirPass passes[] = {
        VIR_PASS_DEGENERATE,
        VIR_PASS_EXPAND_ARCS,
        VIR_PASS_COMPUTE_BOUNDS
    };

    int res = vir_run_passes(path, passes, 3);
    assert(res == 1);

    assert(path->bounds_valid == 1);
    assert(path->count > 2);
    
    for (size_t i = 0; i < path->count; i++) {
        assert(path->segments[i].op != VIR_ARC);
        assert(path->segments[i].op != VIR_LINE);
    }

    VirPass invalid_passes[] = { (VirPass)9999 };
    int invalid_res = vir_run_passes(path, invalid_passes, 1);
    assert(invalid_res == 0);

    vir_path_free(path);
    printf("[+] test_vir_pass_manager PASSED.\n");
}

static void test_vir_pipeline_telemetry() {
    printf("[*] Running test_vir_pipeline_telemetry...\n");

    size_t registry_count = 0;
    VirPassDescriptor *registry = vir_pipeline_get_default_registry(&registry_count);
    assert(registry != NULL);
    assert(registry_count == 7);

    VirPath *path = vir_path_create();
    ZccSvgError err = {0};
    ZccSvgStatus st = zcc_svg_parse_to_vir("M 0 0 L 0 0 A 50 50 0 0 1 50 50 Z", path, &err);
    assert(st == ZCC_SVG_OK);

    vir_pipeline_reset_telemetry(registry, registry_count);
    for (size_t i = 0; i < registry_count; i++) {
        assert(registry[i].runs == 0);
        assert(registry[i].mutations == 0);
        assert(registry[i].failures == 0);
    }

    VirPass passes[] = {
        VIR_PASS_DEGENERATE,
        VIR_PASS_EXPAND_ARCS,
        VIR_PASS_CANONICALIZE,
        VIR_PASS_COMPUTE_BOUNDS
    };

    VirPipelineStats stats = {0};
    int res = vir_run_pipeline(path, registry, registry_count, passes, 4, &stats);
    assert(res == 1);

    assert(stats.total_passes == 4);
    assert(stats.mutations == 3);
    assert(stats.no_change == 1);
    assert(stats.failures == 0);

    // Verify individual descriptor counters
    // degenerate: optimized out the degenerate L 0 0 -> OK (mutation)
    assert(registry[0].runs == 1);
    assert(registry[0].mutations == 1);
    assert(registry[0].failures == 0);

    // expand_arcs: expanded the arc to cubics -> OK (mutation)
    assert(registry[1].runs == 1);
    assert(registry[1].mutations == 1);
    assert(registry[1].failures == 0);

    // bounds: calculated bounds for the first time -> OK (mutation)
    assert(registry[2].runs == 1);
    assert(registry[2].mutations == 1);
    assert(registry[2].failures == 0);

    // canonicalize: since degenerate L 0 0 was removed and arcs were already expanded,
    // there are no lines or arcs left, so canonicalize is a no-change
    assert(registry[3].runs == 1);
    assert(registry[3].mutations == 0);
    assert(registry[3].failures == 0);

    vir_pipeline_reset_telemetry(registry, registry_count);
    for (size_t i = 0; i < registry_count; i++) {
        assert(registry[i].runs == 0);
        assert(registry[i].mutations == 0);
        assert(registry[i].failures == 0);
    }

    vir_path_free(path);
    printf("[+] test_vir_pipeline_telemetry PASSED.\n");
}

static void test_vir_fixed_point_pipeline() {
    printf("[*] Running test_vir_fixed_point_pipeline...\n");

    size_t registry_count = 0;
    VirPassDescriptor *registry = vir_pipeline_get_default_registry(&registry_count);
    assert(registry != NULL);
    assert(registry_count == 7);

    VirPath *path = vir_path_create();
    ZccSvgError err = {0};
    ZccSvgStatus st = zcc_svg_parse_to_vir("M 0 0 L 0 0 A 50 50 0 0 1 50 50 Z", path, &err);
    assert(st == ZCC_SVG_OK);

    vir_pipeline_reset_telemetry(registry, registry_count);

    VirPass passes[] = {
        VIR_PASS_DEGENERATE,
        VIR_PASS_EXPAND_ARCS,
        VIR_PASS_CANONICALIZE,
        VIR_PASS_COMPUTE_BOUNDS
    };

    VirPipelineStats stats = {0};
    int iterations = vir_run_pipeline_until_stable(path, registry, registry_count, passes, 4, &stats, 10);
    assert(iterations == 2);

    assert(stats.total_passes == 8);
    assert(stats.mutations == 3);
    assert(stats.no_change == 5);
    assert(stats.failures == 0);

    // Verify idempotency: running again on the stable path should converge in exactly 1 iteration
    VirPipelineStats stats2 = {0};
    int iterations2 = vir_run_pipeline_until_stable(path, registry, registry_count, passes, 4, &stats2, 10);
    assert(iterations2 == 1);
    assert(stats2.total_passes == 4);
    assert(stats2.mutations == 0);
    assert(stats2.no_change == 4);
    assert(stats2.failures == 0);

    // Verify iteration cap: setting max_iterations = 1 on a path that needs 2 should return 0 (not stabilized)
    vir_path_free(path);

    path = vir_path_create();
    st = zcc_svg_parse_to_vir("M 0 0 L 0 0 A 50 50 0 0 1 50 50 Z", path, &err);
    assert(st == ZCC_SVG_OK);

    VirPipelineStats stats3 = {0};
    int iterations3 = vir_run_pipeline_until_stable(path, registry, registry_count, passes, 4, &stats3, 1);
    assert(iterations3 == 0);

    vir_path_free(path);
    printf("[+] test_vir_fixed_point_pipeline PASSED.\n");
}

static void test_vir_pass_dependency_graph() {
    printf("[*] Running test_vir_pass_dependency_graph...\n");

    size_t registry_count = 0;
    VirPassDescriptor *registry = vir_pipeline_get_default_registry(&registry_count);
    assert(registry != NULL);
    assert(registry_count == 7);

    // 1. Prerequisite Auto-scheduling:
    // canonicalize requires ARCS_EXPANDED.
    // If we run canonicalize on a path with arcs, it should auto-schedule expand_arcs.
    {
        VirPath *path = vir_path_create();
        ZccSvgError err = {0};
        ZccSvgStatus st = zcc_svg_parse_to_vir("M 0 0 L 10 10 A 50 50 0 0 1 50 50", path, &err);
        assert(st == ZCC_SVG_OK);
        assert(path->state_flags == 0);

        vir_pipeline_reset_telemetry(registry, registry_count);

        VirPass passes[] = { VIR_PASS_CANONICALIZE };
        VirPipelineStats stats = {0};
        int res = vir_run_pipeline_with_deps(path, registry, registry_count, passes, 1, &stats);
        assert(res == 1);

        // expand_arcs must run and mutate (1 runs, 1 mutations)
        assert(registry[1].runs == 1);
        assert(registry[1].mutations == 1);

        // canonicalize must run and mutate (1 runs, 1 mutations)
        assert(registry[3].runs == 1);
        assert(registry[3].mutations == 1);

        assert(stats.total_passes == 2);
        assert(stats.mutations == 2);
        assert(path->state_flags & VIR_STATE_ARCS_EXPANDED);
        assert(path->state_flags & VIR_STATE_CANONICALIZED);

        vir_path_free(path);
    }

    // 2. Redundancy Pruning (skipping already satisfied states):
    // If we run bounds twice, the second execution should be skipped.
    {
        VirPath *path = vir_path_create();
        ZccSvgError err = {0};
        ZccSvgStatus st = zcc_svg_parse_to_vir("M 0 0 L 10 10", path, &err);
        assert(st == ZCC_SVG_OK);

        vir_pipeline_reset_telemetry(registry, registry_count);

        VirPass passes[] = { VIR_PASS_COMPUTE_BOUNDS };
        VirPipelineStats stats1 = {0};
        int res1 = vir_run_pipeline_with_deps(path, registry, registry_count, passes, 1, &stats1);
        assert(res1 == 1);
        assert(stats1.total_passes == 1);
        assert(stats1.mutations == 1);
        assert(registry[2].runs == 1);

        // Second run
        VirPipelineStats stats2 = {0};
        int res2 = vir_run_pipeline_with_deps(path, registry, registry_count, passes, 1, &stats2);
        assert(res2 == 1);
        // Should skip the pass entirely
        assert(stats2.total_passes == 0);
        assert(stats2.no_change == 1);
        // registry runs count should remain 1
        assert(registry[2].runs == 1);

        vir_path_free(path);
    }

    // 3. Invalidation Propagation:
    // degenerate invalidates BOUNDS_VALID.
    // If we compute bounds, degenerate (with a change), and then bounds, it should recompute bounds.
    {
        VirPath *path = vir_path_create();
        ZccSvgError err = {0};
        ZccSvgStatus st = zcc_svg_parse_to_vir("M 0 0 L 0 0 L 10 10", path, &err);
        assert(st == ZCC_SVG_OK);

        vir_pipeline_reset_telemetry(registry, registry_count);

        // Run bounds first
        VirPass passes_b[] = { VIR_PASS_COMPUTE_BOUNDS };
        VirPipelineStats stats_b1 = {0};
        vir_run_pipeline_with_deps(path, registry, registry_count, passes_b, 1, &stats_b1);
        assert(path->state_flags & VIR_STATE_BOUNDS_VALID);

        // Run degenerate, which mutates the path (removes L 0 0) and invalidates BOUNDS_VALID
        VirPass passes_d[] = { VIR_PASS_DEGENERATE };
        VirPipelineStats stats_d = {0};
        vir_run_pipeline_with_deps(path, registry, registry_count, passes_d, 1, &stats_d);
        assert(stats_d.mutations == 1);
        assert(!(path->state_flags & VIR_STATE_BOUNDS_VALID));

        // Run bounds again, it should execute again because BOUNDS_VALID was invalidated
        VirPipelineStats stats_b2 = {0};
        vir_run_pipeline_with_deps(path, registry, registry_count, passes_b, 1, &stats_b2);
        assert(stats_b2.total_passes == 1);
        assert(stats_b2.mutations == 1);
        assert(path->state_flags & VIR_STATE_BOUNDS_VALID);

        vir_path_free(path);
    }

    // 4. State Convergence:
    // Run degenerate, expand_arcs, canonicalize, bounds.
    // The final state flags should converge to all four.
    {
        VirPath *path = vir_path_create();
        ZccSvgError err = {0};
        ZccSvgStatus st = zcc_svg_parse_to_vir("M 0 0 L 0 0 A 50 50 0 0 1 50 50 Z", path, &err);
        assert(st == ZCC_SVG_OK);

        vir_pipeline_reset_telemetry(registry, registry_count);

        VirPass passes[] = {
            VIR_PASS_DEGENERATE,
            VIR_PASS_EXPAND_ARCS,
            VIR_PASS_CANONICALIZE,
            VIR_PASS_COMPUTE_BOUNDS
        };

        VirPipelineStats stats = {0};
        int res = vir_run_pipeline_with_deps(path, registry, registry_count, passes, 4, &stats);
        assert(res == 1);

        uint32_t expected = VIR_STATE_DEGENERATE_FREE | VIR_STATE_ARCS_EXPANDED | VIR_STATE_CANONICALIZED | VIR_STATE_BOUNDS_VALID;
        assert(path->state_flags == expected);

        vir_path_free(path);
    }

    printf("[+] test_vir_pass_dependency_graph PASSED.\n");
}

static void test_vir_backend_planner() {
    printf("[*] Running test_vir_backend_planner...\n");

    size_t registry_count = 0;
    VirPassDescriptor *registry = vir_pipeline_get_default_registry(&registry_count);
    assert(registry != NULL);
    assert(registry_count == 7);

    // 1. Prepare SVG backend (target state clean -> 0)
    {
        VirPath *path = vir_path_create();
        ZccSvgError err = {0};
        ZccSvgStatus st = zcc_svg_parse_to_vir("M 0 0 L 10 10 A 50 50 0 0 1 50 50", path, &err);
        assert(st == ZCC_SVG_OK);

        vir_pipeline_reset_telemetry(registry, registry_count);

        VirPipelineStats stats = {0};
        int res = vir_prepare_backend(path, registry, registry_count, VIR_BACKEND_SVG, &stats);
        assert(res == 1);
        // Prerequisite is clean, which path has initially (state_flags = 0). So 0 passes run.
        assert(stats.total_passes == 0);
        assert(stats.mutations == 0);

        vir_path_free(path);
    }

    // 2. Prepare SDF backend (target state: ARCS_EXPANDED)
    {
        VirPath *path = vir_path_create();
        ZccSvgError err = {0};
        ZccSvgStatus st = zcc_svg_parse_to_vir("M 0 0 L 10 10 A 50 50 0 0 1 50 50", path, &err);
        assert(st == ZCC_SVG_OK);

        vir_pipeline_reset_telemetry(registry, registry_count);

        VirPipelineStats stats = {0};
        int res = vir_prepare_backend(path, registry, registry_count, VIR_BACKEND_SDF, &stats);
        assert(res == 1);
        // Should auto-schedule expand_arcs pass
        assert(stats.total_passes == 1);
        assert(stats.mutations == 1);
        assert(path->state_flags & VIR_STATE_ARCS_EXPANDED);

        // Run preparation again on fully prepared path - should skip
        VirPipelineStats stats2 = {0};
        res = vir_prepare_backend(path, registry, registry_count, VIR_BACKEND_SDF, &stats2);
        assert(res == 1);
        assert(stats2.total_passes == 0);

        vir_path_free(path);
    }

    // 3. Prepare GLSL backend (target state: ARCS_EXPANDED | CANONICALIZED | BOUNDS_VALID)
    {
        VirPath *path = vir_path_create();
        ZccSvgError err = {0};
        ZccSvgStatus st = zcc_svg_parse_to_vir("M 0 0 L 10 10 A 50 50 0 0 1 50 50 Z", path, &err);
        assert(st == ZCC_SVG_OK);

        vir_pipeline_reset_telemetry(registry, registry_count);

        VirPipelineStats stats = {0};
        int res = vir_prepare_backend(path, registry, registry_count, VIR_BACKEND_GLSL, &stats);
        assert(res == 1);
        // Target state requires expand_arcs, canonicalize, normalize, exact_bounds, and localize.
        assert(stats.total_passes == 5);
        assert(stats.mutations == 4);
        assert((path->state_flags & (VIR_STATE_ARCS_EXPANDED | VIR_STATE_CANONICALIZED | VIR_STATE_EXACT_BOUNDS | VIR_STATE_LOCALIZED)) ==
               (VIR_STATE_ARCS_EXPANDED | VIR_STATE_CANONICALIZED | VIR_STATE_EXACT_BOUNDS | VIR_STATE_LOCALIZED));

        // Preparing again should skip
        VirPipelineStats stats2 = {0};
        res = vir_prepare_backend(path, registry, registry_count, VIR_BACKEND_GLSL, &stats2);
        assert(res == 1);
        assert(stats2.total_passes == 0);

        vir_path_free(path);
    }

    printf("[+] test_vir_backend_planner PASSED.\n");
}

static void test_vir_pass_graph_exporter() {
    printf("[*] Running test_vir_pass_graph_exporter...\n");

    size_t registry_count = 0;
    VirPassDescriptor *registry = vir_pipeline_get_default_registry(&registry_count);
    assert(registry != NULL);
    assert(registry_count == 7);

    // Populate telemetry by running some passes
    VirPath *path = vir_path_create();
    ZccSvgError err = {0};
    ZccSvgStatus st = zcc_svg_parse_to_vir("M 0 0 L 0 0 A 50 50 0 0 1 50 50 Z", path, &err);
    assert(st == ZCC_SVG_OK);

    vir_pipeline_reset_telemetry(registry, registry_count);

    VirPass passes[] = {
        VIR_PASS_DEGENERATE,
        VIR_PASS_EXPAND_ARCS,
        VIR_PASS_CANONICALIZE,
        VIR_PASS_COMPUTE_BOUNDS
    };

    VirPipelineStats stats = {0};
    int res = vir_run_pipeline_with_deps(path, registry, registry_count, passes, 4, &stats);
    assert(res == 1);

    char *dot = vir_pipeline_to_dot(registry, registry_count);
    assert(dot != NULL);

    // Verify format
    assert(strstr(dot, "digraph VIR_Pipeline {") != NULL);
    assert(strstr(dot, "degenerate") != NULL);
    assert(strstr(dot, "expand_arcs") != NULL);
    assert(strstr(dot, "bounds") != NULL);
    assert(strstr(dot, "canonicalize") != NULL);

    // Check node label telemetry formatting
    assert(strstr(dot, "runs:") != NULL);
    assert(strstr(dot, "mutations:") != NULL);
    assert(strstr(dot, "failures:") != NULL);

    // Check dependency edges
    assert(strstr(dot, "-> canonicalize [label=\"requires flag") != NULL);

    // Check invalidation edges
    assert(strstr(dot, "color=red") != NULL);
    assert(strstr(dot, "style=dashed") != NULL);
    assert(strstr(dot, "invalidates flag") != NULL);

    free(dot);
    vir_path_free(path);

    printf("[+] test_vir_pass_graph_exporter PASSED.\n");
}

static void test_vir_exact_bounds_solving() {
    printf("[*] Running test_vir_exact_bounds_solving...\n");

    size_t registry_count = 0;
    VirPassDescriptor *registry = vir_pipeline_get_default_registry(&registry_count);
    assert(registry != NULL);

    VirPath *path = vir_path_create();
    ZccSvgError err = {0};
    // Parse the curve: M 0 0 C 10 100, 40 -50, 50 50
    ZccSvgStatus st = zcc_svg_parse_to_vir("M 0 0 C 10 100, 40 -50, 50 50", path, &err);
    assert(st == ZCC_SVG_OK);

    // Compute simple bounds first
    float min_x, min_y, max_x, max_y;
    vir_path_compute_bounds(path, &min_x, &min_y, &max_x, &max_y);
    // Control-polygon bounds should include control points y1=100 and y2=-50
    assert(fabsf(min_y - (-50.0f)) < EPSILON);
    assert(fabsf(max_y - 100.0f) < EPSILON);

    // Now run the exact_bounds pass
    vir_pipeline_reset_telemetry(registry, registry_count);
    VirPipelineStats stats = {0};
    VirPass passes[] = { VIR_PASS_EXACT_BOUNDS };
    int res = vir_run_pipeline_with_deps(path, registry, registry_count, passes, 1, &stats);
    assert(res == 1);

    // The exact bounds should be tighter: min_y = 0.0f, max_y = 50.0f
    assert(path->state_flags & VIR_STATE_EXACT_BOUNDS);
    assert(fabsf(path->min_x - 0.0f) < EPSILON);
    assert(fabsf(path->max_x - 50.0f) < EPSILON);
    assert(fabsf(path->min_y - 0.0f) < EPSILON);
    assert(fabsf(path->max_y - 50.0f) < EPSILON);

    vir_path_free(path);
    printf("[+] test_vir_exact_bounds_solving PASSED.\n");
}

static void test_vir_registry_validation() {
    printf("[*] Running test_vir_registry_validation...\n");

    size_t registry_count = 0;
    VirPassDescriptor *registry = vir_pipeline_get_default_registry(&registry_count);
    assert(registry != NULL);
    assert(registry_count == 7);

    // 1. Valid default registry test
    {
        VirRegistryValidationError err = {0};
        VirRegistryValidationResult res = vir_validate_registry(registry, registry_count, &err);
        assert(res == VIR_REGISTRY_OK);
        assert(err.status == VIR_REGISTRY_OK);
    }

    // 2. Duplicate Pass ID test
    {
        VirPassDescriptor temp[10];
        memcpy(temp, registry, registry_count * sizeof(VirPassDescriptor));
        temp[registry_count - 1].pass_id = temp[0].pass_id;

        VirRegistryValidationError err = {0};
        VirRegistryValidationResult res = vir_validate_registry(temp, registry_count, &err);
        assert(res == VIR_REGISTRY_ERR_DUPLICATE_PASS);
        assert(err.status == VIR_REGISTRY_ERR_DUPLICATE_PASS);
        assert(err.pass_id == temp[0].pass_id);
    }

    // 3. Duplicate Producer test
    {
        VirPassDescriptor temp[10];
        memcpy(temp, registry, registry_count * sizeof(VirPassDescriptor));
        temp[0].produced_state = 1U << 15;
        temp[1].produced_state = 1U << 15;

        VirRegistryValidationError err = {0};
        VirRegistryValidationResult res = vir_validate_registry(temp, registry_count, &err);
        assert(res == VIR_REGISTRY_ERR_DUPLICATE_PRODUCER);
        assert(err.status == VIR_REGISTRY_ERR_DUPLICATE_PRODUCER);
        assert(err.state_mask == (1U << 15));
    }

    // 4. Orphan Required State test
    {
        VirPassDescriptor temp[10];
        memcpy(temp, registry, registry_count * sizeof(VirPassDescriptor));
        temp[0].required_state = 1U << 29;

        VirRegistryValidationError err = {0};
        VirRegistryValidationResult res = vir_validate_registry(temp, registry_count, &err);
        assert(res == VIR_REGISTRY_ERR_ORPHAN_REQUIRED_STATE);
        assert(err.status == VIR_REGISTRY_ERR_ORPHAN_REQUIRED_STATE);
        assert(err.state_mask == (1U << 29));
    }

    // 5. Invalid Invalidation test
    {
        VirPassDescriptor temp[10];
        memcpy(temp, registry, registry_count * sizeof(VirPassDescriptor));
        temp[0].invalidated_state = 1U << 28;

        VirRegistryValidationError err = {0};
        VirRegistryValidationResult res = vir_validate_registry(temp, registry_count, &err);
        assert(res == VIR_REGISTRY_ERR_INVALID_INVALIDATION);
        assert(err.status == VIR_REGISTRY_ERR_INVALID_INVALIDATION);
        assert(err.state_mask == (1U << 28));
    }

    // 6. Self-conflict test
    {
        VirPassDescriptor temp[10];
        memcpy(temp, registry, registry_count * sizeof(VirPassDescriptor));
        temp[1].required_state = 1U << 1;
        temp[1].invalidated_state = 1U << 1;

        VirRegistryValidationError err = {0};
        VirRegistryValidationResult res = vir_validate_registry(temp, registry_count, &err);
        assert(res == VIR_REGISTRY_ERR_INVALID_INVALIDATION);
        assert(err.status == VIR_REGISTRY_ERR_INVALID_INVALIDATION);
        assert(err.state_mask == (1U << 1));
    }

    // 7. Cycle test
    {
        VirPassDescriptor temp[2] = {
            { VIR_PASS_DEGENERATE, "degenerate", NULL, 0, 0, 0, 1U << 11, 1U << 10, 0 },
            { VIR_PASS_EXPAND_ARCS, "expand_arcs", NULL, 0, 0, 0, 1U << 10, 1U << 11, 0 }
        };
        VirRegistryValidationError err = {0};
        VirRegistryValidationResult res = vir_validate_registry(temp, 2, &err);
        assert(res == VIR_REGISTRY_ERR_CYCLE);
        assert(err.status == VIR_REGISTRY_ERR_CYCLE);
    }

    printf("[+] test_vir_registry_validation PASSED.\n");
}

static void test_vir_path_normalization() {
    printf("[*] Running test_vir_path_normalization...\n");

    size_t registry_count = 0;
    VirPassDescriptor *registry = vir_pipeline_get_default_registry(&registry_count);
    assert(registry != NULL);
    assert(registry_count == 7);

    VirPath *path = vir_path_create();
    ZccSvgError err = {0};
    ZccSvgStatus st = zcc_svg_parse_to_vir("M 10 20 L 30 40", path, &err);
    assert(st == ZCC_SVG_OK);
    assert(path->state_flags == 0);

    vir_pipeline_reset_telemetry(registry, registry_count);
    VirPipelineStats stats = {0};
    VirPass passes[] = { VIR_PASS_LOCALIZE };
    
    int res = vir_run_pipeline_with_deps(path, registry, registry_count, passes, 1, &stats);
    assert(res == 1);
    assert(stats.total_passes >= 5);

    assert(path->state_flags & VIR_STATE_NORMALIZED);
    assert(path->state_flags & VIR_STATE_LOCALIZED);
    assert(fabsf(path->min_x - 0.0f) < 1e-5f);
    assert(fabsf(path->min_y - 0.0f) < 1e-5f);
    assert(fabsf(path->max_x - 20.0f) < 1e-5f);
    assert(fabsf(path->max_y - 20.0f) < 1e-5f);

    assert(path->count == 2);
    assert(path->segments[0].op == VIR_MOVE);
    assert(fabsf(path->segments[0].coords[0] - 0.0f) < 1e-5f);
    assert(fabsf(path->segments[0].coords[1] - 0.0f) < 1e-5f);
    assert(path->segments[1].op == VIR_CUBIC);
    assert(fabsf(path->segments[1].coords[0] - 6.66667f) < 1e-4f);
    assert(fabsf(path->segments[1].coords[1] - 6.66667f) < 1e-4f);
    assert(fabsf(path->segments[1].coords[2] - 13.33333f) < 1e-4f);
    assert(fabsf(path->segments[1].coords[3] - 13.33333f) < 1e-4f);
    assert(fabsf(path->segments[1].coords[4] - 20.0f) < 1e-4f);
    assert(fabsf(path->segments[1].coords[5] - 20.0f) < 1e-4f);

    VirPassResult r_loc = vir_path_localize(path);
    assert(r_loc == VIR_PASS_NO_CHANGE);

    VirPassResult r_norm = vir_path_normalize(path);
    assert(r_norm == VIR_PASS_NO_CHANGE);

    vir_path_free(path);
    printf("[+] test_vir_path_normalization PASSED.\n");
}

static void test_vir_path_equivalence() {
    printf("[*] Running test_vir_path_equivalence...\n");

    VirPath *path1 = vir_path_create();
    VirPath *path2 = vir_path_create();
    ZccSvgError err = {0};

    ZccSvgStatus st1 = zcc_svg_parse_to_vir("M 10 20 L 30 40 Z", path1, &err);
    ZccSvgStatus st2 = zcc_svg_parse_to_vir("M 10 20 L 30 40 Z M 30 40 Z", path2, &err);
    assert(st1 == ZCC_SVG_OK);
    assert(st2 == ZCC_SVG_OK);

    int eq = vir_paths_equivalent(path1, path2, 1e-3f);
    assert(eq == 1);

    vir_path_free(path1);
    vir_path_free(path2);

    path1 = vir_path_create();
    path2 = vir_path_create();
    st1 = zcc_svg_parse_to_vir("M 10 20 L 30 40", path1, &err);
    st2 = zcc_svg_parse_to_vir("M 100 200 L 120 220", path2, &err);
    assert(st1 == ZCC_SVG_OK);
    assert(st2 == ZCC_SVG_OK);

    eq = vir_paths_equivalent(path1, path2, 1e-3f);
    assert(eq == 1);

    vir_path_free(path1);
    vir_path_free(path2);

    path1 = vir_path_create();
    path2 = vir_path_create();
    st1 = zcc_svg_parse_to_vir("M 10 20 L 30 40", path1, &err);
    st2 = zcc_svg_parse_to_vir("M 10 20 L 50 60", path2, &err);
    assert(st1 == ZCC_SVG_OK);
    assert(st2 == ZCC_SVG_OK);

    eq = vir_paths_equivalent(path1, path2, 1e-3f);
    assert(eq == 0);

    vir_path_free(path1);
    vir_path_free(path2);

    printf("[+] test_vir_path_equivalence PASSED.\n");
}

static void test_vir_path_fingerprint() {
    printf("[*] Running test_vir_path_fingerprint...\n");

    VirPath *path1 = vir_path_create();
    VirPath *path2 = vir_path_create();
    ZccSvgError err = {0};

    // Case 1: Identical geometry, structurally different initial paths
    ZccSvgStatus st1 = zcc_svg_parse_to_vir("M 10 20 L 30 40 Z", path1, &err);
    ZccSvgStatus st2 = zcc_svg_parse_to_vir("M 10 20 L 30 40 Z M 30 40 Z", path2, &err);
    assert(st1 == ZCC_SVG_OK);
    assert(st2 == ZCC_SVG_OK);

    uint64_t fp1 = vir_path_fingerprint(path1, 1e-3f);
    uint64_t fp2 = vir_path_fingerprint(path2, 1e-3f);
    assert(fp1 != 0);
    assert(fp1 == fp2);

    vir_path_free(path1);
    vir_path_free(path2);

    // Case 2: Translated equivalent coordinates
    path1 = vir_path_create();
    path2 = vir_path_create();
    st1 = zcc_svg_parse_to_vir("M 10 20 L 30 40", path1, &err);
    st2 = zcc_svg_parse_to_vir("M 100 200 L 120 220", path2, &err);
    assert(st1 == ZCC_SVG_OK);
    assert(st2 == ZCC_SVG_OK);

    fp1 = vir_path_fingerprint(path1, 1e-3f);
    fp2 = vir_path_fingerprint(path2, 1e-3f);
    assert(fp1 == fp2);

    vir_path_free(path1);
    vir_path_free(path2);

    // Case 3: Distinct paths
    path1 = vir_path_create();
    path2 = vir_path_create();
    st1 = zcc_svg_parse_to_vir("M 10 20 L 30 40", path1, &err);
    st2 = zcc_svg_parse_to_vir("M 10 20 L 50 60", path2, &err);
    assert(st1 == ZCC_SVG_OK);
    assert(st2 == ZCC_SVG_OK);

    fp1 = vir_path_fingerprint(path1, 1e-3f);
    fp2 = vir_path_fingerprint(path2, 1e-3f);
    assert(fp1 != fp2);

    vir_path_free(path1);
    vir_path_free(path2);

    printf("[+] test_vir_path_fingerprint PASSED.\n");
}

static void test_vir_canonical_fingerprint() {
    printf("[*] Running test_vir_canonical_fingerprint...\n");

    VirPath *path_raw = vir_path_create();
    VirPath *path_pre = vir_path_create();
    ZccSvgError err = {0};

    // 1. Ingest identical paths
    ZccSvgStatus st1 = zcc_svg_parse_to_vir("M 10 20 L 30 40 Z", path_raw, &err);
    ZccSvgStatus st2 = zcc_svg_parse_to_vir("M 10 20 L 30 40 Z", path_pre, &err);
    assert(st1 == ZCC_SVG_OK);
    assert(st2 == ZCC_SVG_OK);

    // 2. Pre-compile/localize path_pre
    size_t registry_count = 0;
    VirPassDescriptor *registry = vir_pipeline_get_default_registry(&registry_count);
    assert(registry != NULL);
    VirPipelineStats stats = {0};
    VirPass target_passes[] = { VIR_PASS_LOCALIZE };
    int ok = vir_run_pipeline_with_deps(path_pre, registry, registry_count, target_passes, 1, &stats);
    assert(ok == 1);
    assert(path_pre->state_flags & VIR_STATE_LOCALIZED);

    // Record path_pre state and pointers to verify zero mutation/allocation side-effect
    VirSegment *orig_segments = path_pre->segments;
    size_t orig_count = path_pre->count;
    size_t orig_capacity = path_pre->capacity;
    uint32_t orig_flags = path_pre->state_flags;

    // 3. Compute fingerprints
    uint64_t fp_raw = vir_path_canonical_fingerprint(path_raw, 1e-3f);
    uint64_t fp_pre = vir_path_canonical_fingerprint(path_pre, 1e-3f);

    assert(fp_raw != 0);
    assert(fp_raw == fp_pre);

    // Verify path_pre remains unchanged (no new allocation, no flag changes)
    assert(path_pre->segments == orig_segments);
    assert(path_pre->count == orig_count);
    assert(path_pre->capacity == orig_capacity);
    assert(path_pre->state_flags == orig_flags);

    // 4. Translate path_raw to verify translation invariance
    VirPath *path_trans = vir_path_create();
    ZccSvgStatus st3 = zcc_svg_parse_to_vir("M 110 220 L 130 240 Z", path_trans, &err);
    assert(st3 == ZCC_SVG_OK);

    uint64_t fp_trans = vir_path_canonical_fingerprint(path_trans, 1e-3f);
    assert(fp_raw == fp_trans);

    vir_path_free(path_raw);
    vir_path_free(path_pre);
    vir_path_free(path_trans);

    printf("[+] test_vir_canonical_fingerprint PASSED.\n");
}

static void test_vir_compilation_caching() {
    printf("[*] Running test_vir_compilation_caching...\n");

    vir_cache_init();
    vir_cache_clear();
    vir_cache_reset_stats();

    VirCacheStats st0 = vir_cache_get_stats();
    assert(st0.hits == 0);
    assert(st0.misses == 0);
    assert(st0.evictions == 0);

    VirPath *path1 = vir_path_create();
    VirPath *path2 = vir_path_create();
    ZccSvgError err = {0};

    ZccSvgStatus st1 = zcc_svg_parse_to_vir("M 10 20 C 15 25, 25 35, 30 40", path1, &err);
    ZccSvgStatus st2 = zcc_svg_parse_to_vir("M 10 20 C 15 25, 25 35, 30 40", path2, &err);
    assert(st1 == ZCC_SVG_OK);
    assert(st2 == ZCC_SVG_OK);

    size_t registry_count = 0;
    VirPassDescriptor *registry = vir_pipeline_get_default_registry(&registry_count);
    VirPipelineStats stats1 = {0};
    VirPass passes[] = { VIR_PASS_EXACT_BOUNDS };
    int ok1 = vir_run_pipeline_with_deps(path1, registry, registry_count, passes, 1, &stats1);
    assert(ok1 == 1);
    assert(path1->bounds_valid);

    VirCacheStats st1_stat = vir_cache_get_stats();
    assert(st1_stat.misses == 1);
    assert(st1_stat.hits == 0);

    VirPipelineStats stats2 = {0};
    int ok2 = vir_run_pipeline_with_deps(path2, registry, registry_count, passes, 1, &stats2);
    assert(ok2 == 1);
    assert(path2->bounds_valid);
    assert(fabsf(path1->min_x - path2->min_x) < 1e-5f);
    assert(fabsf(path1->min_y - path2->min_y) < 1e-5f);
    assert(fabsf(path1->max_x - path2->max_x) < 1e-5f);
    assert(fabsf(path1->max_y - path2->max_y) < 1e-5f);

    VirCacheStats st2_stat = vir_cache_get_stats();
    printf("[DEBUG-TEST] st2_stat: hits=%llu, misses=%llu, evictions=%llu\n", (unsigned long long)st2_stat.hits, (unsigned long long)st2_stat.misses, (unsigned long long)st2_stat.evictions);
    assert(st2_stat.misses == 1);
    assert(st2_stat.hits == 1);

    SdfSeed *seed1 = vir_to_sdf_seed(path1);
    assert(seed1 != NULL);

    VirCacheStats st3_stat = vir_cache_get_stats();
    printf("[DEBUG-TEST] st3_stat: hits=%llu, misses=%llu\n", (unsigned long long)st3_stat.hits, (unsigned long long)st3_stat.misses);
    assert(st3_stat.misses == 2);
    assert(st3_stat.hits == 2);

    SdfSeed *seed2 = vir_to_sdf_seed(path2);
    assert(seed2 != NULL);
    assert(seed1->count == seed2->count);
    for (size_t i = 0; i < seed1->count; i++) {
        assert(seed1->segments[i].op == seed2->segments[i].op);
        for (int c = 0; c < 8; c++) {
            assert(fabsf(seed1->segments[i].points[c] - seed2->segments[i].points[c]) < 1e-5f);
        }
    }

    VirCacheStats st4_stat = vir_cache_get_stats();
    printf("[DEBUG-TEST] st4_stat: hits=%llu, misses=%llu\n", (unsigned long long)st4_stat.hits, (unsigned long long)st4_stat.misses);
    assert(st4_stat.misses == 2);
    assert(st4_stat.hits == 4);

    char *glsl1 = sdf_seed_to_glsl(seed1);
    assert(glsl1 != NULL);

    VirCacheStats st5_stat = vir_cache_get_stats();
    printf("[DEBUG-TEST] st5_stat: hits=%llu, misses=%llu\n", (unsigned long long)st5_stat.hits, (unsigned long long)st5_stat.misses);
    assert(st5_stat.misses == 3);
    assert(st5_stat.hits == 4);

    char *glsl2 = sdf_seed_to_glsl(seed2);
    assert(glsl2 != NULL);
    assert(strcmp(glsl1, glsl2) == 0);

    VirCacheStats st6_stat = vir_cache_get_stats();
    printf("[DEBUG-TEST] st6_stat: hits=%llu, misses=%llu\n", (unsigned long long)st6_stat.hits, (unsigned long long)st6_stat.misses);
    assert(st6_stat.misses == 3);
    assert(st6_stat.hits == 5);

    // Test stats reset
    vir_cache_reset_stats();
    VirCacheStats st_reset = vir_cache_get_stats();
    assert(st_reset.hits == 0);
    assert(st_reset.misses == 0);
    assert(st_reset.evictions == 0);

    free(glsl1);
    free(glsl2);
    sdf_seed_free(seed1);
    sdf_seed_free(seed2);
    vir_path_free(path1);
    vir_path_free(path2);

    vir_cache_shutdown();

    printf("[+] test_vir_compilation_caching PASSED.\n");
}

int main() {
    setbuf(stdout, NULL);
    printf("=== ZCC Vector IR (VIR) Test Harness ===\n");
    test_vir_path_equivalence();
    test_vir_path_fingerprint();
    test_vir_canonical_fingerprint();
    test_vir_compilation_caching();
    test_vir_path_normalization();
    test_vir_path_creation_and_growth();
    test_vir_degenerate_removal();
    test_vir_bounds_propagation();
    test_svg_to_vir_adapter();
    test_extreme_and_overflow_vir();
    test_vir_metadata_and_bounds_caching();
    test_vir_arc_ingestion_and_expansion();
    test_vir_backend_diversification();
    test_sdf_to_glsl_compilation();
    test_vir_pass_canonicalize();
    test_vir_pass_manager();
    test_vir_pipeline_telemetry();
    test_vir_fixed_point_pipeline();
    test_vir_pass_dependency_graph();
    test_vir_backend_planner();
    test_vir_pass_graph_exporter();
    test_vir_exact_bounds_solving();
    test_vir_registry_validation();
    vir_cache_shutdown();
    printf("777JACKPOT777 — ALL VIR CORE TESTS GREEN.\n");
    return 0;
}

