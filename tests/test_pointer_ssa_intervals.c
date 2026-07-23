/*
 * test_pointer_ssa_intervals.c — Sound Disjoint Interval Points-To Analysis Gate (Stage 3)
 * =========================================================================================
 * Directly links src/opt/pointer_ssa.c to test interval aliasing bounds:
 * [off1, off1 + sz1) ∩ [off2, off2 + sz2) ≠ ∅ -> AMBIGUOUS (Unsafe rewrite blocked).
 * [off1, off1 + sz1) ∩ [off2, off2 + sz2) = ∅ -> Distinct location ID (Safe rewrite permitted).
 *
 * EXPLICIT FAIL-CLOSED INVALIDATION LIST:
 * 1. Negative offset (off < 0) -> AMBIGUOUS
 * 2. Unknown object size (access_size <= 0: heap, args, externs) -> AMBIGUOUS
 * 3. Int64 overflow on offset accumulation (off + sz < off) -> AMBIGUOUS
 *
 * VERDICT PRECEDENCE CONTRACT:
 * 1. Unsound aliasing rewrite permitted on overlapping or invalid interval -> EXIT 1 (RED) [FIRST]
 * 2. locations_evaluated == 0                                              -> EXIT 2 (ORACLE-SUSPECT) [SECOND]
 * 3. All interval disjointness & invalidation rules verified               -> EXIT 0 (GREEN) [THIRD]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#define AMBIGUOUS 65537

typedef uint32_t RegID;

/* Include C source functions directly for granular testing */
#include "../src/opt/pointer_ssa.c"

int main(void) {
    printf("=== STAGE 3 POINTER SSA: INITIALIZING SOUND INTERVAL DISJOINTNESS HARNESS ===\n");

    int locations_evaluated = 0;

#if !defined(TEST_BASELINE_BUILD)
    BaseOffsetKey mem_locations[1024];
    int n_mem_locations = 0;
    RegID mem_points_to_base[1024];
    memset(mem_locations, 0, sizeof(mem_locations));
    memset(mem_points_to_base, 0, sizeof(mem_points_to_base));

    RegID base_ptr = 10;

    /* 1. Track 8-byte store at (p, 0, 8) */
    int loc0 = get_or_create_mem_location(base_ptr, 0, 8, mem_locations, &n_mem_locations, mem_points_to_base);
    mem_points_to_base[loc0] = 100;
    locations_evaluated++;

    /* 2. Track 4-byte load at (p, 4, 4) -> OVERLAPS interval [0, 8) in byte range [4, 8) */
    int loc1 = get_or_create_mem_location(base_ptr, 4, 4, mem_locations, &n_mem_locations, mem_points_to_base);
    locations_evaluated++;

    /* Overlapping interval MUST degrade loc0 to AMBIGUOUS */
    if (mem_points_to_base[loc0] != AMBIGUOUS) {
        fprintf(stderr, "Pointer SSA FAULT DETECTED: Overlapping 4-byte load at offset 4 did NOT degrade 8-byte store at offset 0 to AMBIGUOUS!\n");
        printf("VERDICT: POINTER SSA FAULT DETECTED (EXIT RED AS EXPECTED)\n");
        return 1;
    }

    /* 3. Track 4-byte store at (p, 8, 4) -> DISJOINT from interval [0, 8) */
    int loc2 = get_or_create_mem_location(base_ptr, 8, 4, mem_locations, &n_mem_locations, mem_points_to_base);
    mem_points_to_base[loc2] = 200;
    locations_evaluated++;

    if (loc2 == loc0 || mem_points_to_base[loc2] == AMBIGUOUS) {
        fprintf(stderr, "Pointer SSA FAULT DETECTED: Disjoint 4-byte store at offset 8 was incorrectly degraded to AMBIGUOUS!\n");
        printf("VERDICT: POINTER SSA FAULT DETECTED (EXIT RED AS EXPECTED)\n");
        return 1;
    }

    /* 4. Invalidation List: Negative offset (-4) */
    int loc3 = get_or_create_mem_location(base_ptr, -4, 4, mem_locations, &n_mem_locations, mem_points_to_base);
    locations_evaluated++;
    if (mem_points_to_base[loc3] != AMBIGUOUS) {
        fprintf(stderr, "Pointer SSA FAULT DETECTED: Negative offset (-4) did NOT fail-closed to AMBIGUOUS!\n");
        printf("VERDICT: POINTER SSA FAULT DETECTED (EXIT RED AS EXPECTED)\n");
        return 1;
    }

    /* 5. Invalidation List: Unknown size (heap/args/externs: access_size <= 0) */
    int loc4 = get_or_create_mem_location(base_ptr, 16, 0, mem_locations, &n_mem_locations, mem_points_to_base);
    locations_evaluated++;
    if (mem_points_to_base[loc4] != AMBIGUOUS) {
        fprintf(stderr, "Pointer SSA FAULT DETECTED: Unknown object size (0) did NOT fail-closed to AMBIGUOUS!\n");
        printf("VERDICT: POINTER SSA FAULT DETECTED (EXIT RED AS EXPECTED)\n");
        return 1;
    }

    /* 6. Invalidation List: Int64 overflow on offset accumulation (INT64_MAX) */
    int loc5 = get_or_create_mem_location(base_ptr, 0x7FFFFFFFFFFFFFFFLL, 8, mem_locations, &n_mem_locations, mem_points_to_base);
    locations_evaluated++;
    if (mem_points_to_base[loc5] != AMBIGUOUS) {
        fprintf(stderr, "Pointer SSA FAULT DETECTED: Int64 overflow on offset accumulation did NOT fail-closed to AMBIGUOUS!\n");
        printf("VERDICT: POINTER SSA FAULT DETECTED (EXIT RED AS EXPECTED)\n");
        return 1;
    }
#endif

    if (locations_evaluated == 0) {
        fprintf(stderr, "Pointer SSA VACUOUS: Zero memory locations evaluated (locations_evaluated == 0).\n");
        printf("VERDICT: ORACLE-SUSPECT (EXIT 2 AS EXPECTED)\n");
        return 2;
    }

    printf("Pointer SSA PASS: Sound disjoint interval points-to analysis & invalidation list verified (%d tests).\n", locations_evaluated);
    printf("VERDICT: POINTER SSA TRUTH ORACLE PASS\n");
    return 0;
}
