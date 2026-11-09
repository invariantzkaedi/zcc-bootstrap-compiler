#include "zcc_qec_decoder.h"
#include <stdio.h>
#include <stdlib.h>

static int test_d1_laflamme_5_1_3(void) {
    ZccQecCode code;
    uint8_t seen_syndromes[16] = {0};
    int q, t, total_corrected = 0;
    ZccQecDecodeResult res;

    printf("[QEC-D1] Laflamme Perfect [[5, 1, 3]] Cyclic Stabilizer Code (Quantum Hamming Bound)...\n");
    if (!zcc_qec_init_laflamme_5_1_3(&code)) {
        printf("  [FAIL] Laflamme [[5, 1, 3]] stabilizer group verification failed.\n");
        return 0;
    }
    if (!zcc_qec_decode_and_recover(&code, ZCC_QPAULI_I, &res) ||
        res.syndrome != 0u || !res.is_exact_identity || !res.logical_preserved) {
        printf("  [FAIL] Identity channel recovery check failed.\n");
        return 0;
    }
    seen_syndromes[0] = 1;
    total_corrected++;

    for (q = 0; q < 5; q++) {
        uint32_t ops[3];
        ops[0] = ZCC_QPAULI_X(q);
        ops[1] = ZCC_QPAULI_Z(q);
        ops[2] = ZCC_QPAULI_Y(q);
        for (t = 0; t < 3; t++) {
            if (!zcc_qec_decode_and_recover(&code, ops[t], &res)) {
                printf("  [FAIL] Recovery failed for q=%d type=%d\n", q, t);
                return 0;
            }
            if (res.syndrome == 0u || res.syndrome >= 16u || seen_syndromes[res.syndrome]) {
                printf("  [FAIL] Non-bijective syndrome %u for q=%d type=%d\n", res.syndrome, q, t);
                return 0;
            }
            if (!res.is_exact_identity || res.residual_weight != 0 || res.multiplication_phase_mod4 != 0) {
                printf("  [FAIL] Expected exact identity residual for non-degenerate [[5,1,3]]\n");
                return 0;
            }
            seen_syndromes[res.syndrome] = 1;
            total_corrected++;
        }
    }
    printf("  Laflamme [[5, 1, 3]]: %d/16 syndromes bijective, %d/16 errors corrected to exact I.\n",
           total_corrected, total_corrected);
    printf("  [PASS] QEC-D1: Perfect [[5, 1, 3]] code saturates Quantum Hamming Bound (1 + 15 = 2^4).\n\n");
    return 1;
}

static int test_d2_steane_7_1_3(void) {
    ZccQecCode code;
    int q, q1, q2, t, single_ok = 0, css_pair_ok = 0;
    ZccQecDecodeResult res;

    printf("[QEC-D2] Steane [[7, 1, 3]] CSS Fano-Plane Code (Single-Qubit + Simultaneous X_i Z_j)...\n");
    if (!zcc_qec_init_steane_7_1_3(&code)) {
        printf("  [FAIL] Steane [[7, 1, 3]] stabilizer group verification failed.\n");
        return 0;
    }
    if (!zcc_qec_decode_and_recover(&code, ZCC_QPAULI_I, &res) || !res.logical_preserved) {
        return 0;
    }
    single_ok++;
    for (q = 0; q < 7; q++) {
        uint32_t ops[3];
        ops[0] = ZCC_QPAULI_X(q);
        ops[1] = ZCC_QPAULI_Z(q);
        ops[2] = ZCC_QPAULI_Y(q);
        for (t = 0; t < 3; t++) {
            if (!zcc_qec_decode_and_recover(&code, ops[t], &res) || !res.is_exact_identity) {
                printf("  [FAIL] Steane single-qubit recovery failed at q=%d t=%d\n", q, t);
                return 0;
            }
            single_ok++;
        }
    }
    for (q1 = 0; q1 < 7; q1++) {
        for (q2 = 0; q2 < 7; q2++) {
            uint32_t xz_err = zcc_hw_qpauli_mul(ZCC_QPAULI_X(q1), ZCC_QPAULI_Z(q2));
            if (!zcc_qec_decode_and_recover(&code, xz_err, &res) || !res.is_exact_identity) {
                printf("  [FAIL] Steane CSS X_%d Z_%d recovery failed\n", q1, q2);
                return 0;
            }
            css_pair_ok++;
        }
    }
    printf("  Steane [[7, 1, 3]]: %d/22 single-qubit + %d/49 two-qubit CSS X_i Z_j errors corrected.\n",
           single_ok, css_pair_ok);
    printf("  [PASS] QEC-D2: All 71/71 Fano-plane syndrome trajectories decoded with 0 logical faults.\n\n");
    return 1;
}

static int test_d3_shor_9_1_3_degeneracy(void) {
    ZccQecCode code;
    int q, t, deg_pairs = 0, exact_cnt = 0, deg_cnt = 0;
    ZccQecDecodeResult res;

    printf("[QEC-D3] Shor [[9, 1, 3]] Concatenated Code & Quantum Degeneracy Proof...\n");
    if (!zcc_qec_init_shor_9_1_3(&code)) {
        printf("  [FAIL] Shor [[9, 1, 3]] stabilizer group verification failed.\n");
        return 0;
    }
    if (!zcc_qec_verify_shor_degeneracy(&code, &deg_pairs) || deg_pairs != 9) {
        printf("  [FAIL] Shor degeneracy verification failed (deg_pairs=%d)\n", deg_pairs);
        return 0;
    }
    for (q = 0; q < 9; q++) {
        uint32_t ops[3];
        ops[0] = ZCC_QPAULI_X(q);
        ops[1] = ZCC_QPAULI_Z(q);
        ops[2] = ZCC_QPAULI_Y(q);
        for (t = 0; t < 3; t++) {
            if (!zcc_qec_decode_and_recover(&code, ops[t], &res) || !res.logical_preserved) {
                printf("  [FAIL] Shor [[9, 1, 3]] failed to preserve logical state at q=%d t=%d\n", q, t);
                return 0;
            }
            if (res.is_exact_identity) exact_cnt++;
            else if (res.is_degenerate_stabilizer) deg_cnt++;
            else return 0;
        }
    }
    printf("  Shor [[9, 1, 3]]: 27/27 single-qubit Pauli errors corrected (%d exact I, %d modulo weight-2 Z_i Z_j stabilizer).\n",
           exact_cnt, deg_cnt);
    printf("  [PASS] QEC-D3: All 9 degenerate intra-cluster phase pairs proven equivalent in N(S)/S.\n\n");
    return 1;
}

static int test_d4_surface_17_topological(void) {
    ZccQecCode code;
    int q, t, exact_cnt = 0, deg_cnt = 0;
    ZccQecDecodeResult res;

    printf("[QEC-D4] Rotated Planar Surface-17 [[9, 1, 3]] Topological Lattice Code...\n");
    if (!zcc_qec_init_surface_17(&code)) {
        printf("  [FAIL] Surface-17 [[9, 1, 3]] stabilizer group verification failed.\n");
        return 0;
    }
    for (q = 0; q < 9; q++) {
        uint32_t ops[3];
        ops[0] = ZCC_QPAULI_X(q);
        ops[1] = ZCC_QPAULI_Z(q);
        ops[2] = ZCC_QPAULI_Y(q);
        for (t = 0; t < 3; t++) {
            if (!zcc_qec_decode_and_recover(&code, ops[t], &res) || !res.logical_preserved) {
                printf("  [FAIL] Surface-17 recovery failed at q=%d t=%d\n", q, t);
                return 0;
            }
            if (res.is_exact_identity) exact_cnt++;
            else if (res.is_degenerate_stabilizer) deg_cnt++;
            else return 0;
        }
    }
    printf("  Surface-17 [[9, 1, 3]]: 27/27 single-qubit errors corrected (%d exact I, %d boundary-digon stabilizer).\n",
           exact_cnt, deg_cnt);
    printf("  [PASS] QEC-D4: Bulk 4-body + boundary 2-body plaquettes preserve logical X_L/Z_L strings.\n\n");
    return 1;
}

static int test_d5_pauli_2cocycle_algebra(void) {
    uint32_t basis[24];
    int idx = 0, q;

    printf("[QEC-D5] 16-Qubit Pauli Group 2-Cocycle H^2(F_2^32, Z_4) & Levi-Civita Phase Gauntlet...\n");
    /* Verify canonical single-qubit Levi-Civita cyclic phases:
     * X*Y = +i Z (phase=1), Y*X = -i Z (phase=3),
     * Y*Z = +i X (phase=1), Z*Y = -i X (phase=3),
     * Z*X = +i Y (phase=1), X*Z = -i Y (phase=3) */
    for (q = 0; q < 16; q++) {
        uint32_t x = ZCC_QPAULI_X(q);
        uint32_t y = ZCC_QPAULI_Y(q);
        uint32_t z = ZCC_QPAULI_Z(q);
        if (zcc_hw_qpauli_mul(x, y) != z || zcc_hw_qpauli_phase(x, y) != 1 || zcc_hw_qpauli_phase(y, x) != 3) return 0;
        if (zcc_hw_qpauli_mul(y, z) != x || zcc_hw_qpauli_phase(y, z) != 1 || zcc_hw_qpauli_phase(z, y) != 3) return 0;
        if (zcc_hw_qpauli_mul(z, x) != y || zcc_hw_qpauli_phase(z, x) != 1 || zcc_hw_qpauli_phase(x, z) != 3) return 0;
    }

    basis[idx++] = ZCC_QPAULI_I;
    for (q = 0; q < 5; q++) {
        basis[idx++] = ZCC_QPAULI_X(q);
        basis[idx++] = ZCC_QPAULI_Y(q);
        basis[idx++] = ZCC_QPAULI_Z(q);
    }
    basis[idx++] = 0x000F000Fu; /* Y0 Y1 Y2 Y3 */
    basis[idx++] = 0x005500AAu; /* mixed 8-qubit string */
    basis[idx++] = 0x12345678u; /* arbitrary 16-qubit operator A */
    basis[idx++] = 0x9ABCDEF0u; /* arbitrary 16-qubit operator B */
    basis[idx++] = 0xCAFEBABEu; /* arbitrary 16-qubit operator C */
    basis[idx++] = 0xFFFF0000u; /* Z^{\otimes 16} */
    basis[idx++] = 0x0000FFFFu; /* X^{\otimes 16} */
    basis[idx++] = 0xFFFFFFFFu; /* Y^{\otimes 16} */

    if (!zcc_qec_verify_2cocycle_batch(basis, idx)) {
        printf("  [FAIL] 2-cocycle associativity verification failed!\n");
        return 0;
    }
    printf("  Verified %d x %d x %d = %d Pauli triples: 0 cocycle or skew-symmetry violations.\n",
           idx, idx, idx, idx * idx * idx);
    printf("  [PASS] QEC-D5: 16-qubit symplectic product & mod-4 Levi-Civita phase algebra verified.\n\n");
    return 1;
}

int main(void) {
    printf("========================================================================\n");
    printf("  ZCC HARDWARE-INTRINSIC FAULT-TOLERANT QEC SYNDROME DECODER GAUNTLET   \n");
    printf("========================================================================\n\n");

    if (!test_d1_laflamme_5_1_3()) return 1;
    if (!test_d2_steane_7_1_3()) return 2;
    if (!test_d3_shor_9_1_3_degeneracy()) return 3;
    if (!test_d4_surface_17_topological()) return 4;
    if (!test_d5_pauli_2cocycle_algebra()) return 5;

    printf("========================================================================\n");
    printf("  ALL 5 QEC DECODER MILESTONES (QEC-D1..D5) PASSED (0 ERRORS)           \n");
    printf("========================================================================\n");
    return 0;
}

