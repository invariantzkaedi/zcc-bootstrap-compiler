#include "zcc_qec_decoder.h"
#include <string.h>

static void build_min_weight_syndrome_lut(ZccQecCode *code, int include_css_xz_pairs) {
    int i, q1, q2, t;
    for (i = 0; i < ZCC_QEC_MAX_SYNDROMES; i++) {
        code->syndrome_lut[i] = 0u;
        code->syndrome_valid[i] = 0u;
    }
    /* Syndrome 0 -> Identity recovery */
    code->syndrome_lut[0] = ZCC_QPAULI_I;
    code->syndrome_valid[0] = 1u;

    /* Pass 1: All weight-1 single-qubit Pauli errors (X_q, Z_q, Y_q) */
    for (q1 = 0; q1 < code->n_qubits; q1++) {
        uint32_t single_ops[3];
        single_ops[0] = ZCC_QPAULI_X(q1);
        single_ops[1] = ZCC_QPAULI_Z(q1);
        single_ops[2] = ZCC_QPAULI_Y(q1);
        for (t = 0; t < 3; t++) {
            uint32_t op = single_ops[t];
            uint32_t syn = zcc_qec_extract_syndrome(code, op);
            if (syn < ZCC_QEC_MAX_SYNDROMES) {
                if (!code->syndrome_valid[syn] ||
                    zcc_hw_qpauli_weight(op) < zcc_hw_qpauli_weight(code->syndrome_lut[syn])) {
                    code->syndrome_lut[syn] = op;
                    code->syndrome_valid[syn] = 1u;
                }
            }
        }
    }

    /* Pass 2: For CSS codes, all 2-qubit cross-channel X_{q1} Z_{q2} errors */
    if (include_css_xz_pairs) {
        for (q1 = 0; q1 < code->n_qubits; q1++) {
            for (q2 = 0; q2 < code->n_qubits; q2++) {
                uint32_t op = zcc_hw_qpauli_mul(ZCC_QPAULI_X(q1), ZCC_QPAULI_Z(q2));
                uint32_t syn = zcc_qec_extract_syndrome(code, op);
                if (syn < ZCC_QEC_MAX_SYNDROMES) {
                    if (!code->syndrome_valid[syn] ||
                        zcc_hw_qpauli_weight(op) < zcc_hw_qpauli_weight(code->syndrome_lut[syn])) {
                        code->syndrome_lut[syn] = op;
                        code->syndrome_valid[syn] = 1u;
                    }
                }
            }
        }
    }
}

uint32_t zcc_qec_extract_syndrome(const ZccQecCode *code, uint32_t error_op) {
    uint32_t syn = 0u;
    int j;
    if (!code) return 0u;
    for (j = 0; j < code->num_stabilizers; j++) {
        uint32_t bit = (uint32_t)(zcc_hw_qpauli_commute(error_op, code->stabilizers[j]) & 1);
        syn |= (bit << (unsigned int)j);
    }
    return syn;
}

int zcc_qec_verify_stabilizer_group(const ZccQecCode *code) {
    int i, j;
    uint32_t logical_y;
    if (!code || code->num_stabilizers <= 0 || code->num_stabilizers > ZCC_QEC_MAX_STABILIZERS) {
        return 0;
    }
    /* 1. Verify all stabilizer generators are non-trivial, self-inverse, and mutually commuting */
    for (i = 0; i < code->num_stabilizers; i++) {
        uint32_t gi = code->stabilizers[i];
        if (zcc_hw_qpauli_weight(gi) <= 0) return 0;
        if (zcc_hw_qpauli_mul(gi, gi) != 0u) return 0;
        if (zcc_hw_qpauli_phase(gi, gi) != 0) return 0;
        for (j = 0; j < code->num_stabilizers; j++) {
            uint32_t gj = code->stabilizers[j];
            if (zcc_hw_qpauli_commute(gi, gj) != 0) return 0;
            if ((zcc_hw_qpauli_phase(gi, gj) & 1) != 0) return 0;
        }
        /* 2. Logical X_L and Z_L must commute with every stabilizer generator */
        if (zcc_hw_qpauli_commute(code->logical_x, gi) != 0) return 0;
        if (zcc_hw_qpauli_commute(code->logical_z, gi) != 0) return 0;
    }
    /* 3. Logical X_L and Z_L must anticommute with each other */
    if (zcc_hw_qpauli_commute(code->logical_x, code->logical_z) != 1) return 0;
    if ((zcc_hw_qpauli_phase(code->logical_x, code->logical_z) & 1) != 1) return 0;

    /* 4. Logical Y_L = X_L * Z_L must also commute with all stabilizers and satisfy distance bound */
    logical_y = zcc_hw_qpauli_mul(code->logical_x, code->logical_z);
    for (i = 0; i < code->num_stabilizers; i++) {
        if (zcc_hw_qpauli_commute(logical_y, code->stabilizers[i]) != 0) return 0;
    }
    if (zcc_hw_qpauli_weight(code->logical_x) < code->distance) return 0;
    if (zcc_hw_qpauli_weight(code->logical_z) < code->distance) return 0;
    if (zcc_hw_qpauli_weight(logical_y) < code->distance) return 0;
    return 1;
}

int zcc_qec_init_laflamme_5_1_3(ZccQecCode *code) {
    if (!code) return 0;
    memset(code, 0, sizeof(*code));
    code->kind = ZCC_QEC_CODE_LAFLAMME_5_1_3;
    code->name = "Laflamme Perfect [[5, 1, 3]]";
    code->n_qubits = 5;
    code->k_logical = 1;
    code->distance = 3;
    code->num_stabilizers = 4;

    /* g0 = X0 Z1 Z2 X3 I4 */
    code->stabilizers[0] = ZCC_QPAULI_X(0) | ZCC_QPAULI_Z(1) | ZCC_QPAULI_Z(2) | ZCC_QPAULI_X(3);
    /* g1 = I0 X1 Z2 Z3 X4 */
    code->stabilizers[1] = ZCC_QPAULI_X(1) | ZCC_QPAULI_Z(2) | ZCC_QPAULI_Z(3) | ZCC_QPAULI_X(4);
    /* g2 = X0 I1 X2 Z3 Z4 */
    code->stabilizers[2] = ZCC_QPAULI_X(0) | ZCC_QPAULI_X(2) | ZCC_QPAULI_Z(3) | ZCC_QPAULI_Z(4);
    /* g3 = Z0 X1 I2 X3 Z4 */
    code->stabilizers[3] = ZCC_QPAULI_Z(0) | ZCC_QPAULI_X(1) | ZCC_QPAULI_X(3) | ZCC_QPAULI_Z(4);

    /* Logical X_L = X0 X1 X2 X3 X4, Logical Z_L = Z0 Z1 Z2 Z3 Z4 */
    code->logical_x = 0x0000001Fu;
    code->logical_z = 0x001F0000u;

    build_min_weight_syndrome_lut(code, 0);
    return zcc_qec_verify_stabilizer_group(code);
}

int zcc_qec_init_steane_7_1_3(ZccQecCode *code) {
    if (!code) return 0;
    memset(code, 0, sizeof(*code));
    code->kind = ZCC_QEC_CODE_STEANE_7_1_3;
    code->name = "Steane CSS Fano [[7, 1, 3]]";
    code->n_qubits = 7;
    code->k_logical = 1;
    code->distance = 3;
    code->num_stabilizers = 6;

    /* 3 Fano-plane X-generators: {0,2,4,6}=0x55, {1,2,5,6}=0x66, {3,4,5,6}=0x78 */
    code->stabilizers[0] = 0x00000055u;
    code->stabilizers[1] = 0x00000066u;
    code->stabilizers[2] = 0x00000078u;
    /* 3 Fano-plane Z-generators */
    code->stabilizers[3] = 0x00550000u;
    code->stabilizers[4] = 0x00660000u;
    code->stabilizers[5] = 0x00780000u;

    /* Logical X_L = X^{\otimes 7}, Logical Z_L = Z^{\otimes 7} */
    code->logical_x = 0x0000007Fu;
    code->logical_z = 0x007F0000u;

    build_min_weight_syndrome_lut(code, 1);
    return zcc_qec_verify_stabilizer_group(code);
}

int zcc_qec_init_shor_9_1_3(ZccQecCode *code) {
    if (!code) return 0;
    memset(code, 0, sizeof(*code));
    code->kind = ZCC_QEC_CODE_SHOR_9_1_3;
    code->name = "Shor Degenerate [[9, 1, 3]]";
    code->n_qubits = 9;
    code->k_logical = 1;
    code->distance = 3;
    code->num_stabilizers = 8;

    /* 6 weight-2 Z-stabilizers detecting bit-flip X errors within each 3-qubit block */
    code->stabilizers[0] = ZCC_QPAULI_Z(0) | ZCC_QPAULI_Z(1);
    code->stabilizers[1] = ZCC_QPAULI_Z(1) | ZCC_QPAULI_Z(2);
    code->stabilizers[2] = ZCC_QPAULI_Z(3) | ZCC_QPAULI_Z(4);
    code->stabilizers[3] = ZCC_QPAULI_Z(4) | ZCC_QPAULI_Z(5);
    code->stabilizers[4] = ZCC_QPAULI_Z(6) | ZCC_QPAULI_Z(7);
    code->stabilizers[5] = ZCC_QPAULI_Z(7) | ZCC_QPAULI_Z(8);
    /* 2 weight-6 X-stabilizers detecting phase-flip Z errors across blocks */
    code->stabilizers[6] = 0x0000003Fu; /* X0 X1 X2 X3 X4 X5 */
    code->stabilizers[7] = 0x000001F8u; /* X3 X4 X5 X6 X7 X8 */

    /* Logical X_L = Z^{\otimes 9}, Logical Z_L = X^{\otimes 9} */
    code->logical_x = 0x01FF0000u;
    code->logical_z = 0x000001FFu;

    build_min_weight_syndrome_lut(code, 1);
    return zcc_qec_verify_stabilizer_group(code);
}

int zcc_qec_init_surface_17(ZccQecCode *code) {
    if (!code) return 0;
    memset(code, 0, sizeof(*code));
    code->kind = ZCC_QEC_CODE_SURFACE_17;
    code->name = "Rotated Planar Surface-17 [[9, 1, 3]]";
    code->n_qubits = 9;
    code->k_logical = 1;
    code->distance = 3;
    code->num_stabilizers = 8;

    /* 4 X-plaquettes on 3x3 lattice: {0,1,3,4}, {4,5,7,8}, {1,2}, {6,7} */
    code->stabilizers[0] = ZCC_QPAULI_X(0) | ZCC_QPAULI_X(1) | ZCC_QPAULI_X(3) | ZCC_QPAULI_X(4);
    code->stabilizers[1] = ZCC_QPAULI_X(4) | ZCC_QPAULI_X(5) | ZCC_QPAULI_X(7) | ZCC_QPAULI_X(8);
    code->stabilizers[2] = ZCC_QPAULI_X(1) | ZCC_QPAULI_X(2);
    code->stabilizers[3] = ZCC_QPAULI_X(6) | ZCC_QPAULI_X(7);

    /* 4 Z-plaquettes on 3x3 lattice: {1,2,4,5}, {3,4,6,7}, {0,3}, {5,8} */
    code->stabilizers[4] = ZCC_QPAULI_Z(1) | ZCC_QPAULI_Z(2) | ZCC_QPAULI_Z(4) | ZCC_QPAULI_Z(5);
    code->stabilizers[5] = ZCC_QPAULI_Z(3) | ZCC_QPAULI_Z(4) | ZCC_QPAULI_Z(6) | ZCC_QPAULI_Z(7);
    code->stabilizers[6] = ZCC_QPAULI_Z(0) | ZCC_QPAULI_Z(3);
    code->stabilizers[7] = ZCC_QPAULI_Z(5) | ZCC_QPAULI_Z(8);

    /* Anti-diagonal Logical X_L = X2 X4 X6, Main-diagonal Logical Z_L = Z0 Z4 Z8 */
    code->logical_x = ZCC_QPAULI_X(2) | ZCC_QPAULI_X(4) | ZCC_QPAULI_X(6);
    code->logical_z = ZCC_QPAULI_Z(0) | ZCC_QPAULI_Z(4) | ZCC_QPAULI_Z(8);

    build_min_weight_syndrome_lut(code, 1);
    return zcc_qec_verify_stabilizer_group(code);
}

uint32_t zcc_qec_lookup_recovery(const ZccQecCode *code, uint32_t syndrome) {
    if (!code || syndrome >= ZCC_QEC_MAX_SYNDROMES) return ZCC_QPAULI_I;
    if (!code->syndrome_valid[syndrome]) return ZCC_QPAULI_I;
    return code->syndrome_lut[syndrome];
}

int zcc_qec_decode_and_recover(const ZccQecCode *code, uint32_t error_op, ZccQecDecodeResult *out) {
    ZccQecDecodeResult local_res;
    if (!code) return 0;
    memset(&local_res, 0, sizeof(local_res));

    local_res.injected_error = error_op;
    local_res.syndrome = zcc_qec_extract_syndrome(code, error_op);
    local_res.recovery_op = zcc_qec_lookup_recovery(code, local_res.syndrome);
    local_res.residual_op = zcc_hw_qpauli_mul(error_op, local_res.recovery_op);
    local_res.residual_weight = zcc_hw_qpauli_weight(local_res.residual_op);
    local_res.multiplication_phase_mod4 = zcc_hw_qpauli_phase(error_op, local_res.recovery_op);
    local_res.post_syndrome = (int)zcc_qec_extract_syndrome(code, local_res.residual_op);
    local_res.logical_x_flipped = zcc_hw_qpauli_commute(local_res.residual_op, code->logical_x);
    local_res.logical_z_flipped = zcc_hw_qpauli_commute(local_res.residual_op, code->logical_z);
    local_res.is_exact_identity = (local_res.residual_op == 0u) ? 1 : 0;
    local_res.is_degenerate_stabilizer =
        (local_res.residual_op != 0u &&
         local_res.post_syndrome == 0 &&
         local_res.logical_x_flipped == 0 &&
         local_res.logical_z_flipped == 0) ? 1 : 0;
    local_res.logical_preserved =
        (local_res.post_syndrome == 0 &&
         local_res.logical_x_flipped == 0 &&
         local_res.logical_z_flipped == 0) ? 1 : 0;

    if (out) *out = local_res;
    return local_res.logical_preserved;
}

int zcc_qec_verify_2cocycle_batch(const uint32_t *ops, int count) {
    int i, j, k;
    if (!ops || count <= 0) return 0;
    for (i = 0; i < count; i++) {
        uint32_t p1 = ops[i];
        if (zcc_hw_qpauli_mul(p1, p1) != 0u) return 0;
        if (zcc_hw_qpauli_phase(p1, p1) != 0) return 0;
        for (j = 0; j < count; j++) {
            uint32_t p2 = ops[j];
            int ph12 = zcc_hw_qpauli_phase(p1, p2);
            int ph21 = zcc_hw_qpauli_phase(p2, p1);
            int cm12 = zcc_hw_qpauli_commute(p1, p2);
            uint32_t m12 = zcc_hw_qpauli_mul(p1, p2);
            if ((ph12 & 1) != cm12) return 0;
            if (ph21 != ((4 - ph12) & 3)) return 0;
            for (k = 0; k < count; k++) {
                uint32_t p3 = ops[k];
                uint32_t m23 = zcc_hw_qpauli_mul(p2, p3);
                int lhs = (ph12 + zcc_hw_qpauli_phase(m12, p3)) & 3;
                int rhs = (zcc_hw_qpauli_phase(p1, m23) + zcc_hw_qpauli_phase(p2, p3)) & 3;
                if (lhs != rhs) return 0;
                if (zcc_hw_qpauli_mul(m12, p3) != zcc_hw_qpauli_mul(p1, m23)) return 0;
            }
        }
    }
    return 1;
}

int zcc_qec_verify_shor_degeneracy(const ZccQecCode *shor_code, int *out_degenerate_pairs) {
    int blk, a, b, deg_count = 0;
    if (!shor_code || shor_code->kind != ZCC_QEC_CODE_SHOR_9_1_3) return 0;

    for (blk = 0; blk < 3; blk++) {
        int base = blk * 3;
        uint32_t syn_rep = zcc_qec_extract_syndrome(shor_code, ZCC_QPAULI_Z(base));
        if (syn_rep == 0u) return 0;
        for (a = 0; a < 3; a++) {
            for (b = a + 1; b < 3; b++) {
                uint32_t za = ZCC_QPAULI_Z(base + a);
                uint32_t zb = ZCC_QPAULI_Z(base + b);
                uint32_t syn_a = zcc_qec_extract_syndrome(shor_code, za);
                uint32_t syn_b = zcc_qec_extract_syndrome(shor_code, zb);
                uint32_t residual = zcc_hw_qpauli_mul(za, zb);
                ZccQecDecodeResult dec;
                /* Both phase errors in the same 3-qubit cluster share an identical non-zero syndrome */
                if (syn_a != syn_rep || syn_b != syn_rep) return 0;
                /* Their product is a weight-2 Z_a Z_b stabilizer with phase +1 (exponent 0) */
                if (zcc_hw_qpauli_weight(residual) != 2) return 0;
                if (zcc_hw_qpauli_phase(za, zb) != 0) return 0;
                if (zcc_qec_extract_syndrome(shor_code, residual) != 0u) return 0;
                if (zcc_hw_qpauli_commute(residual, shor_code->logical_x) != 0) return 0;
                if (zcc_hw_qpauli_commute(residual, shor_code->logical_z) != 0) return 0;
                if (!zcc_qec_decode_and_recover(shor_code, zb, &dec)) return 0;
                if (!dec.is_degenerate_stabilizer) return 0;
                deg_count++;
            }
        }
    }
    if (out_degenerate_pairs) *out_degenerate_pairs = deg_count;
    return (deg_count == 9) ? 1 : 0;
}

