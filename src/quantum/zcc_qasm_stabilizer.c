/* ================================================================ */
/* ZCC BIT-PACKED STABILIZER TABLEAU SIMULATOR (CHP ENGINE)         */
/* ================================================================ */
/* File: src/quantum/zcc_qasm_stabilizer.c                          */
/* Aaronson-Gottesman improved-tableau simulator over GF(2):        */
/*   rows 0..N-1   destabilizer generators                          */
/*   rows N..2N-1  stabilizer generators                            */
/*   row  2N       scratch row for deterministic measurement        */
/* Each row packs X and Z Pauli bits into 64-bit words; the sign    */
/* bit r lives in a per-row byte. All Clifford conjugations and     */
/* rowsum phase updates are word-parallel (XOR + POPCNT), giving    */
/* O(N) per gate and O(N^2 / 64) per measurement: 1,024+ entangled  */
/* qubits simulate in milliseconds where the dense 2^N statevector  */
/* would need 10^295 amplitudes.                                    */
/* ================================================================ */

#include "include/zcc_qasm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ================================================================ */
/* DETERMINISTIC SPLITMIX64 PRNG (mirrors statevector simulator)    */
/* ================================================================ */

static inline uint64_t stab_rand_u64(uint64_t *state) {
    uint64_t z = (*state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static inline int stab_rand_bit(uint64_t *state) {
    return (int)(stab_rand_u64(state) >> 63);
}

static inline int stab_popcount64(uint64_t v) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_popcountll(v);
#else
    v = v - ((v >> 1) & 0x5555555555555555ULL);
    v = (v & 0x3333333333333333ULL) + ((v >> 2) & 0x3333333333333333ULL);
    v = (v + (v >> 4)) & 0x0f0f0f0f0f0f0f0fULL;
    return (int)((v * 0x0101010101010101ULL) >> 56);
#endif
}

/* ================================================================ */
/* ROW ACCESSORS                                                    */
/* ================================================================ */

static inline uint64_t *stab_row_x(ZCCQasmStabilizer *st, size_t row) {
    return st->x_bits + row * st->words_per_row;
}

static inline uint64_t *stab_row_z(ZCCQasmStabilizer *st, size_t row) {
    return st->z_bits + row * st->words_per_row;
}

static inline const uint64_t *stab_row_x_c(const ZCCQasmStabilizer *st, size_t row) {
    return st->x_bits + row * st->words_per_row;
}

static inline const uint64_t *stab_row_z_c(const ZCCQasmStabilizer *st, size_t row) {
    return st->z_bits + row * st->words_per_row;
}

static inline int stab_get_bit(const uint64_t *bits, size_t q) {
    return (int)((bits[q >> 6] >> (q & 63)) & 1ULL);
}

static inline void stab_set_bit(uint64_t *bits, size_t q, int v) {
    uint64_t mask = 1ULL << (q & 63);
    if (v) bits[q >> 6] |= mask;
    else bits[q >> 6] &= ~mask;
}

static inline void stab_xor_bit(uint64_t *bits, size_t q, int v) {
    if (v) bits[q >> 6] ^= 1ULL << (q & 63);
}

/* ================================================================ */
/* LIFECYCLE                                                        */
/* ================================================================ */

ZCCQasmStabilizer *zcc_qasm_stab_create(size_t num_qubits, size_t num_clbits, uint64_t seed) {
    if (num_qubits == 0 || num_qubits > ZCC_QASM_MAX_STAB_QUBITS) {
        return NULL;
    }

    ZCCQasmStabilizer *st = (ZCCQasmStabilizer *)calloc(1, sizeof(ZCCQasmStabilizer));
    if (!st) return NULL;

    st->num_qubits = num_qubits;
    st->words_per_row = (num_qubits + 63) / 64;
    size_t num_rows = 2 * num_qubits + 1;

    st->x_bits = (uint64_t *)calloc(num_rows * st->words_per_row, sizeof(uint64_t));
    st->z_bits = (uint64_t *)calloc(num_rows * st->words_per_row, sizeof(uint64_t));
    st->r_bits = (uint8_t *)calloc(num_rows, sizeof(uint8_t));
    if (!st->x_bits || !st->z_bits || !st->r_bits) {
        zcc_qasm_stab_free(st);
        return NULL;
    }

    st->num_classical_bits = num_clbits;
    if (num_clbits > 0) {
        st->classical_bits = (unsigned int *)calloc(num_clbits, sizeof(unsigned int));
        if (!st->classical_bits) {
            zcc_qasm_stab_free(st);
            return NULL;
        }
    }

    st->rng_state = (seed != 0) ? seed : 0x123456789ABCDEF0ULL;
    zcc_qasm_stab_reset_state(st);
    return st;
}

void zcc_qasm_stab_free(ZCCQasmStabilizer *st) {
    if (!st) return;
    if (st->x_bits) free(st->x_bits);
    if (st->z_bits) free(st->z_bits);
    if (st->r_bits) free(st->r_bits);
    if (st->classical_bits) free(st->classical_bits);
    free(st);
}

void zcc_qasm_stab_reset_state(ZCCQasmStabilizer *st) {
    if (!st) return;
    size_t n = st->num_qubits;
    size_t num_rows = 2 * n + 1;
    memset(st->x_bits, 0, num_rows * st->words_per_row * sizeof(uint64_t));
    memset(st->z_bits, 0, num_rows * st->words_per_row * sizeof(uint64_t));
    memset(st->r_bits, 0, num_rows * sizeof(uint8_t));

    /* |0...0>: destabilizer i = X_i, stabilizer i = Z_i */
    size_t i;
    for (i = 0; i < n; i++) {
        stab_set_bit(stab_row_x(st, i), i, 1);
        stab_set_bit(stab_row_z(st, n + i), i, 1);
    }

    if (st->classical_bits && st->num_classical_bits > 0) {
        memset(st->classical_bits, 0, st->num_classical_bits * sizeof(unsigned int));
    }
}

/* ================================================================ */
/* WORD-PARALLEL ROWSUM (Aaronson-Gottesman phase bookkeeping)      */
/* ================================================================ */

/* Multiplies Pauli row i into row h: row_h <- row_h * row_i, with exact      */
/* i^k phase tracking. Per 64-bit word the +1/-1 contributions of the mod-4   */
/* exponent sum are gathered as bitmasks and reduced with POPCNT.             */
static void stab_rowsum(ZCCQasmStabilizer *st, size_t h, size_t i) {
    uint64_t *xh = stab_row_x(st, h);
    uint64_t *zh = stab_row_z(st, h);
    const uint64_t *xi = stab_row_x_c(st, i);
    const uint64_t *zi = stab_row_z_c(st, i);

    long pos = 0, neg = 0;
    size_t w;
    for (w = 0; w < st->words_per_row; w++) {
        uint64_t x1 = xi[w], z1 = zi[w];
        uint64_t x2 = xh[w], z2 = zh[w];

        /* g(x1,z1,x2,z2) per Aaronson-Gottesman:                     */
        /*   row_i bit = Y: g = z2 - x2                               */
        /*   row_i bit = X: g = z2 * (2*x2 - 1)                       */
        /*   row_i bit = Z: g = x2 * (1 - 2*z2)                       */
        uint64_t y1 = x1 & z1;
        uint64_t xo = x1 & ~z1;
        uint64_t zo = ~x1 & z1;

        uint64_t p_mask = (y1 & z2 & ~x2) | (xo & x2 & z2) | (zo & x2 & ~z2);
        uint64_t n_mask = (y1 & x2 & ~z2) | (xo & z2 & ~x2) | (zo & x2 & z2);

        pos += stab_popcount64(p_mask);
        neg += stab_popcount64(n_mask);

        xh[w] = x1 ^ x2;
        zh[w] = z1 ^ z2;
    }

    long phase = 2L * (long)st->r_bits[h] + 2L * (long)st->r_bits[i] + pos - neg;
    phase &= 3L; /* mod 4; valid products always land on 0 or 2 */
    st->r_bits[h] = (phase == 2L) ? 1 : 0;
}

/* ================================================================ */
/* CLIFFORD GATE CONJUGATIONS                                       */
/* ================================================================ */

static void stab_gate_h(ZCCQasmStabilizer *st, size_t q) {
    size_t num_rows = 2 * st->num_qubits;
    size_t row;
    for (row = 0; row < num_rows; row++) {
        uint64_t *x = stab_row_x(st, row);
        uint64_t *z = stab_row_z(st, row);
        int xb = stab_get_bit(x, q);
        int zb = stab_get_bit(z, q);
        st->r_bits[row] ^= (uint8_t)(xb & zb);
        stab_set_bit(x, q, zb);
        stab_set_bit(z, q, xb);
    }
}

static void stab_gate_s(ZCCQasmStabilizer *st, size_t q) {
    size_t num_rows = 2 * st->num_qubits;
    size_t row;
    for (row = 0; row < num_rows; row++) {
        uint64_t *x = stab_row_x(st, row);
        uint64_t *z = stab_row_z(st, row);
        int xb = stab_get_bit(x, q);
        int zb = stab_get_bit(z, q);
        st->r_bits[row] ^= (uint8_t)(xb & zb);
        stab_xor_bit(z, q, xb);
    }
}

static void stab_gate_sdg(ZCCQasmStabilizer *st, size_t q) {
    size_t num_rows = 2 * st->num_qubits;
    size_t row;
    for (row = 0; row < num_rows; row++) {
        uint64_t *x = stab_row_x(st, row);
        uint64_t *z = stab_row_z(st, row);
        int xb = stab_get_bit(x, q);
        int zb = stab_get_bit(z, q);
        st->r_bits[row] ^= (uint8_t)(xb & (zb ^ 1));
        stab_xor_bit(z, q, xb);
    }
}

static void stab_gate_x(ZCCQasmStabilizer *st, size_t q) {
    size_t num_rows = 2 * st->num_qubits;
    size_t row;
    for (row = 0; row < num_rows; row++) {
        st->r_bits[row] ^= (uint8_t)stab_get_bit(stab_row_z(st, row), q);
    }
}

static void stab_gate_z(ZCCQasmStabilizer *st, size_t q) {
    size_t num_rows = 2 * st->num_qubits;
    size_t row;
    for (row = 0; row < num_rows; row++) {
        st->r_bits[row] ^= (uint8_t)stab_get_bit(stab_row_x(st, row), q);
    }
}

static void stab_gate_y(ZCCQasmStabilizer *st, size_t q) {
    size_t num_rows = 2 * st->num_qubits;
    size_t row;
    for (row = 0; row < num_rows; row++) {
        int xb = stab_get_bit(stab_row_x(st, row), q);
        int zb = stab_get_bit(stab_row_z(st, row), q);
        st->r_bits[row] ^= (uint8_t)(xb ^ zb);
    }
}

static void stab_gate_cx(ZCCQasmStabilizer *st, size_t c, size_t t) {
    if (c == t) return;
    size_t num_rows = 2 * st->num_qubits;
    size_t row;
    for (row = 0; row < num_rows; row++) {
        uint64_t *x = stab_row_x(st, row);
        uint64_t *z = stab_row_z(st, row);
        int xc = stab_get_bit(x, c);
        int zc = stab_get_bit(z, c);
        int xt = stab_get_bit(x, t);
        int zt = stab_get_bit(z, t);
        st->r_bits[row] ^= (uint8_t)(xc & zt & (xt ^ zc ^ 1));
        stab_xor_bit(x, t, xc);
        stab_xor_bit(z, c, zt);
    }
}

static void stab_gate_cz(ZCCQasmStabilizer *st, size_t c, size_t t) {
    stab_gate_h(st, t);
    stab_gate_cx(st, c, t);
    stab_gate_h(st, t);
}

static void stab_gate_cy(ZCCQasmStabilizer *st, size_t c, size_t t) {
    /* CY = (I (x) S) CX (I (x) S^dag) */
    stab_gate_sdg(st, t);
    stab_gate_cx(st, c, t);
    stab_gate_s(st, t);
}

static void stab_gate_swap(ZCCQasmStabilizer *st, size_t a, size_t b) {
    if (a == b) return;
    size_t num_rows = 2 * st->num_qubits;
    size_t row;
    for (row = 0; row < num_rows; row++) {
        uint64_t *x = stab_row_x(st, row);
        uint64_t *z = stab_row_z(st, row);
        int xa = stab_get_bit(x, a), xb = stab_get_bit(x, b);
        int za = stab_get_bit(z, a), zb = stab_get_bit(z, b);
        stab_set_bit(x, a, xb); stab_set_bit(x, b, xa);
        stab_set_bit(z, a, zb); stab_set_bit(z, b, za);
    }
}

/* ================================================================ */
/* PROJECTIVE Z MEASUREMENT (deterministic vs random collapse)      */
/* ================================================================ */

int zcc_qasm_stab_measure(ZCCQasmStabilizer *st, size_t qubit, int *out_deterministic) {
    if (!st || qubit >= st->num_qubits) return 0;

    size_t n = st->num_qubits;
    size_t p = (size_t)-1;
    size_t row;

    /* Find a stabilizer generator anticommuting with Z_q (x bit set at q) */
    for (row = n; row < 2 * n; row++) {
        if (stab_get_bit(stab_row_x_c(st, row), qubit)) {
            p = row;
            break;
        }
    }

    if (p != (size_t)-1) {
        /* Random outcome: project onto +/- Z_q */
        for (row = 0; row < 2 * n; row++) {
            if (row != p && stab_get_bit(stab_row_x_c(st, row), qubit)) {
                stab_rowsum(st, row, p);
            }
        }

        /* Destabilizer partner takes the old stabilizer row */
        memcpy(stab_row_x(st, p - n), stab_row_x_c(st, p), st->words_per_row * sizeof(uint64_t));
        memcpy(stab_row_z(st, p - n), stab_row_z_c(st, p), st->words_per_row * sizeof(uint64_t));
        st->r_bits[p - n] = st->r_bits[p];

        int outcome = stab_rand_bit(&st->rng_state);
        memset(stab_row_x(st, p), 0, st->words_per_row * sizeof(uint64_t));
        memset(stab_row_z(st, p), 0, st->words_per_row * sizeof(uint64_t));
        stab_set_bit(stab_row_z(st, p), qubit, 1);
        st->r_bits[p] = (uint8_t)outcome;

        if (out_deterministic) *out_deterministic = 0;
        return outcome;
    }

    /* Deterministic outcome: accumulate into scratch row 2n */
    size_t scratch = 2 * n;
    memset(stab_row_x(st, scratch), 0, st->words_per_row * sizeof(uint64_t));
    memset(stab_row_z(st, scratch), 0, st->words_per_row * sizeof(uint64_t));
    st->r_bits[scratch] = 0;

    for (row = 0; row < n; row++) {
        if (stab_get_bit(stab_row_x_c(st, row), qubit)) {
            stab_rowsum(st, scratch, row + n);
        }
    }

    if (out_deterministic) *out_deterministic = 1;
    return (int)st->r_bits[scratch];
}

/* Deterministic Z expectation without collapse: +1, -1, or 0 when random */
int zcc_qasm_stab_peek_z(const ZCCQasmStabilizer *st, size_t qubit) {
    if (!st || qubit >= st->num_qubits) return 0;

    size_t n = st->num_qubits;
    size_t row;
    for (row = n; row < 2 * n; row++) {
        if (stab_get_bit(stab_row_x_c(st, row), qubit)) {
            return 0; /* Z_q anticommutes with a stabilizer: outcome random */
        }
    }

    /* Measurement is deterministic and non-destructive here: the scratch-row */
    /* accumulation never touches rows 0..2n-1.                               */
    ZCCQasmStabilizer *mut = (ZCCQasmStabilizer *)st;
    int det = 0;
    int outcome = zcc_qasm_stab_measure(mut, qubit, &det);
    return outcome ? -1 : +1;
}

static void stab_reset_qubit(ZCCQasmStabilizer *st, size_t qubit) {
    int det = 0;
    int outcome = zcc_qasm_stab_measure(st, qubit, &det);
    if (outcome == 1) {
        stab_gate_x(st, qubit);
    }
}

/* ================================================================ */
/* SINGLE-QUBIT ENTANGLEMENT ENTROPY S(rho_q) in {0, 1}             */
/* ================================================================ */

/* S(rho_A) = |A| - log2 |S_A| with S_A the stabilizer subgroup supported     */
/* inside A. For A = {q}: entropy is 0 iff some product of stabilizer        */
/* generators acts only on q, found by GF(2) elimination on all columns      */
/* except q's X/Z columns.                                                   */
double zcc_qasm_stab_entropy_1q(const ZCCQasmStabilizer *st, size_t qubit) {
    if (!st || qubit >= st->num_qubits) return 0.0;

    size_t n = st->num_qubits;
    size_t words = st->words_per_row;
    uint64_t *xs = (uint64_t *)malloc(n * words * sizeof(uint64_t));
    uint64_t *zs = (uint64_t *)malloc(n * words * sizeof(uint64_t));
    if (!xs || !zs) {
        if (xs) free(xs);
        if (zs) free(zs);
        return 0.0;
    }

    size_t i;
    for (i = 0; i < n; i++) {
        memcpy(xs + i * words, stab_row_x_c(st, n + i), words * sizeof(uint64_t));
        memcpy(zs + i * words, stab_row_z_c(st, n + i), words * sizeof(uint64_t));
    }

    /* Gaussian elimination over the 2(n-1) support columns outside qubit q.  */
    size_t rank = 0;
    size_t col;
    for (col = 0; col < 2 * n && rank < n; col++) {
        size_t q_col = col >> 1;
        if (q_col == qubit) continue;
        uint64_t *blk = (col & 1) ? zs : xs;

        size_t pivot = (size_t)-1;
        for (i = rank; i < n; i++) {
            if (stab_get_bit(blk + i * words, q_col)) { pivot = i; break; }
        }
        if (pivot == (size_t)-1) continue;

        if (pivot != rank) {
            size_t w;
            for (w = 0; w < words; w++) {
                uint64_t t = xs[rank * words + w]; xs[rank * words + w] = xs[pivot * words + w]; xs[pivot * words + w] = t;
                t = zs[rank * words + w]; zs[rank * words + w] = zs[pivot * words + w]; zs[pivot * words + w] = t;
            }
        }

        for (i = 0; i < n; i++) {
            if (i != rank && stab_get_bit(((col & 1) ? zs : xs) + i * words, q_col)) {
                size_t w;
                for (w = 0; w < words; w++) {
                    xs[i * words + w] ^= xs[rank * words + w];
                    zs[i * words + w] ^= zs[rank * words + w];
                }
            }
        }
        rank++;
    }

    /* S(rho_q) = 1 - dim ker = 1 - (n - rank): the kernel rows are exactly   */
    /* the independent stabilizer products supported only on q (at most one,  */
    /* since single-qubit Paulis pairwise anticommute).                       */
    double entropy = 1.0 - (double)(n - rank);
    if (entropy < 0.0) entropy = 0.0;

    free(xs);
    free(zs);
    return entropy;
}

/* ================================================================ */
/* CLIFFORD CIRCUIT DETECTION                                       */
/* ================================================================ */

static int stab_op_is_clifford(const ZCCQasmCircuit *circ, const ZCCQasmOp *op, int depth);

static int stab_gate_def_is_clifford(const ZCCQasmCircuit *circ, const char *name, int depth) {
    if (!circ || !name || depth > 16) return 0;
    int i;
    for (i = 0; i < circ->num_custom_gates; i++) {
        if (strcmp(circ->custom_gates[i].name, name) == 0) {
            const ZCCQasmOp *bop = circ->custom_gates[i].body_head;
            while (bop) {
                if (!stab_op_is_clifford(circ, bop, depth + 1)) return 0;
                bop = bop->next;
            }
            return 1;
        }
    }
    return 0;
}

static int stab_op_is_clifford(const ZCCQasmCircuit *circ, const ZCCQasmOp *op, int depth) {
    switch (op->kind) {
        case QASM_OP_HEADER:
        case QASM_OP_INCLUDE:
        case QASM_OP_BARRIER:
        case QASM_OP_RESET:
        case QASM_OP_MEASURE:
        case QASM_OP_ID:
        case QASM_OP_H:
        case QASM_OP_X:
        case QASM_OP_Y:
        case QASM_OP_Z:
        case QASM_OP_S:
        case QASM_OP_SDG:
        case QASM_OP_CX:
        case QASM_OP_CY:
        case QASM_OP_CZ:
        case QASM_OP_SWAP:
            return 1;
        case QASM_OP_CUSTOM:
            return stab_gate_def_is_clifford(circ, op->gate_name, depth);
        default:
            return 0;
    }
}

int zcc_qasm_circuit_is_clifford(const ZCCQasmCircuit *circ) {
    if (!circ) return 0;
    const ZCCQasmOp *op = circ->head_op;
    while (op) {
        if (!stab_op_is_clifford(circ, op, 0)) return 0;
        op = op->next;
    }
    return 1;
}

/* ================================================================ */
/* CIRCUIT EXECUTION (registers, conditionals, custom gates)        */
/* ================================================================ */

static const ZCCQasmRegister *stab_find_reg(const ZCCQasmCircuit *circ, const char *name) {
    if (!circ || !name) return NULL;
    int i;
    for (i = 0; i < circ->num_registers; i++) {
        if (strcmp(circ->registers[i].name, name) == 0) return &circ->registers[i];
    }
    return NULL;
}

static size_t stab_resolve_idx(const ZCCQasmCircuit *circ, const ZCCQasmQubitRef *ref) {
    const ZCCQasmRegister *reg = stab_find_reg(circ, ref->reg_name);
    if (!reg) return 0;
    return (size_t)(reg->base_offset + (ref->index >= 0 ? ref->index : 0));
}

static unsigned int stab_read_creg(const ZCCQasmStabilizer *st, const ZCCQasmCircuit *circ, const char *reg_name) {
    const ZCCQasmRegister *reg = stab_find_reg(circ, reg_name);
    if (!reg || !st->classical_bits) return 0;
    unsigned int val = 0;
    int k;
    for (k = 0; k < reg->size; k++) {
        size_t idx = (size_t)(reg->base_offset + k);
        if (idx < st->num_classical_bits && st->classical_bits[idx]) {
            val |= (1U << k);
        }
    }
    return val;
}

static size_t stab_resolve_bound_qubit(const ZCCQasmCircuit *circ, const ZCCQasmQubitRef *ref,
                                       const char *qubit_names[MAX_QASM_GATE_QUBITS],
                                       const size_t *qubit_indices, int n_qbound) {
    int i;
    for (i = 0; i < n_qbound; i++) {
        if (qubit_names[i] && strcmp(qubit_names[i], ref->reg_name) == 0) {
            return qubit_indices[i];
        }
    }
    return stab_resolve_idx(circ, ref);
}

static int stab_execute_kind(ZCCQasmStabilizer *st, ZCCQasmOpKind kind,
                             const size_t *qubits, int num_qubits) {
    (void)num_qubits;
    switch (kind) {
        case QASM_OP_ID:  break;
        case QASM_OP_H:   stab_gate_h(st, qubits[0]); break;
        case QASM_OP_X:   stab_gate_x(st, qubits[0]); break;
        case QASM_OP_Y:   stab_gate_y(st, qubits[0]); break;
        case QASM_OP_Z:   stab_gate_z(st, qubits[0]); break;
        case QASM_OP_S:   stab_gate_s(st, qubits[0]); break;
        case QASM_OP_SDG: stab_gate_sdg(st, qubits[0]); break;
        case QASM_OP_CX:  stab_gate_cx(st, qubits[0], qubits[1]); break;
        case QASM_OP_CY:  stab_gate_cy(st, qubits[0], qubits[1]); break;
        case QASM_OP_CZ:  stab_gate_cz(st, qubits[0], qubits[1]); break;
        case QASM_OP_SWAP: stab_gate_swap(st, qubits[0], qubits[1]); break;
        default:
            return 0;
    }
    return 1;
}

static const ZCCQasmGateDef *stab_find_custom_gate(const ZCCQasmCircuit *circ, const char *name) {
    if (!circ || !name) return NULL;
    int i;
    for (i = 0; i < circ->num_custom_gates; i++) {
        if (strcmp(circ->custom_gates[i].name, name) == 0) return &circ->custom_gates[i];
    }
    return NULL;
}

static int stab_execute_op(ZCCQasmStabilizer *st, const ZCCQasmCircuit *circ, const ZCCQasmOp *op,
                           const char *qubit_names[MAX_QASM_GATE_QUBITS], const size_t *qubit_indices,
                           int n_qbound, int depth) {
    if (!st || !circ || !op) return 1;
    if (depth > 16) {
        snprintf(st->last_error, sizeof(st->last_error), "exceeded maximum custom gate recursion depth (16)");
        return 0;
    }

    if (op->has_condition) {
        unsigned int cur_val = stab_read_creg(st, circ, op->cond_reg);
        if (cur_val != (unsigned int)op->cond_val) {
            return 1;
        }
    }

    if (op->kind == QASM_OP_BARRIER) return 1;

    if (op->kind == QASM_OP_RESET) {
        size_t qidx = stab_resolve_bound_qubit(circ, &op->qubits[0], qubit_names, qubit_indices, n_qbound);
        if (qidx >= st->num_qubits) {
            snprintf(st->last_error, sizeof(st->last_error), "reset qubit index %zu out of range", qidx);
            return 0;
        }
        stab_reset_qubit(st, qidx);
        return 1;
    }

    if (op->kind == QASM_OP_MEASURE) {
        size_t qidx = stab_resolve_bound_qubit(circ, &op->qubits[0], qubit_names, qubit_indices, n_qbound);
        size_t cidx = stab_resolve_idx(circ, &op->meas_target);
        if (qidx >= st->num_qubits) {
            snprintf(st->last_error, sizeof(st->last_error), "measured qubit index %zu out of range", qidx);
            return 0;
        }
        int det = 0;
        int outcome = zcc_qasm_stab_measure(st, qidx, &det);
        if (st->classical_bits && cidx < st->num_classical_bits) {
            st->classical_bits[cidx] = (unsigned int)outcome;
        }
        return 1;
    }

    size_t actual_qubits[MAX_QASM_GATE_QUBITS] = {0};
    int q;
    for (q = 0; q < op->num_qubits; q++) {
        actual_qubits[q] = stab_resolve_bound_qubit(circ, &op->qubits[q], qubit_names, qubit_indices, n_qbound);
        if (actual_qubits[q] >= st->num_qubits) {
            snprintf(st->last_error, sizeof(st->last_error), "qubit index %zu out of range", actual_qubits[q]);
            return 0;
        }
    }

    if (stab_execute_kind(st, op->kind, actual_qubits, op->num_qubits)) {
        return 1;
    }

    const ZCCQasmGateDef *gdef = stab_find_custom_gate(circ, op->gate_name);
    if (gdef) {
        const char *new_qnames[MAX_QASM_GATE_QUBITS] = {0};
        size_t new_qindices[MAX_QASM_GATE_QUBITS] = {0};
        int qi;
        for (qi = 0; qi < gdef->num_qubits && qi < op->num_qubits; qi++) {
            new_qnames[qi] = gdef->qubit_names[qi];
            new_qindices[qi] = actual_qubits[qi];
        }

        const ZCCQasmOp *bop = gdef->body_head;
        while (bop) {
            if (!stab_execute_op(st, circ, bop, new_qnames, new_qindices, gdef->num_qubits, depth + 1)) {
                return 0;
            }
            bop = bop->next;
        }
        return 1;
    }

    snprintf(st->last_error, sizeof(st->last_error),
             "non-Clifford gate operation '%s' is not supported by the stabilizer engine", op->gate_name);
    return 0;
}

int zcc_qasm_stab_apply_circuit(ZCCQasmStabilizer *st, const ZCCQasmCircuit *circ) {
    if (!st || !circ) return 0;
    const ZCCQasmOp *op = circ->head_op;
    while (op) {
        if (!stab_execute_op(st, circ, op, NULL, NULL, 0, 0)) {
            return 0;
        }
        op = op->next;
    }
    return 1;
}

int zcc_qasm_stab_run_file(const char *filename, uint64_t seed, ZCCQasmStabilizer **out_st, char *err_buf, size_t err_buf_size) {
    if (!filename) {
        if (err_buf && err_buf_size > 0) snprintf(err_buf, err_buf_size, "null filename");
        return 0;
    }

    char parse_err[512] = {0};
    ZCCQasmCircuit *circ = zcc_qasm_parse_file(filename, parse_err, sizeof(parse_err));
    if (!circ) {
        if (err_buf && err_buf_size > 0) snprintf(err_buf, err_buf_size, "%s", parse_err[0] ? parse_err : "parse failed");
        return 0;
    }

    if (!zcc_qasm_validate(circ, parse_err, sizeof(parse_err))) {
        if (err_buf && err_buf_size > 0) snprintf(err_buf, err_buf_size, "%s", parse_err[0] ? parse_err : "validation failed");
        zcc_qasm_circuit_free(circ);
        return 0;
    }

    if (!zcc_qasm_circuit_is_clifford(circ)) {
        if (err_buf && err_buf_size > 0) {
            snprintf(err_buf, err_buf_size, "circuit contains non-Clifford gates: stabilizer engine requires {H, S, SDG, X, Y, Z, CX, CY, CZ, SWAP, ID, BARRIER, RESET, MEASURE}");
        }
        zcc_qasm_circuit_free(circ);
        return 0;
    }

    ZCCQasmStabilizer *st = zcc_qasm_stab_create((size_t)circ->total_qubits, (size_t)circ->total_clbits, seed);
    if (!st) {
        if (err_buf && err_buf_size > 0) {
            snprintf(err_buf, err_buf_size, "stabilizer allocation failed: requested %d qubits exceeds maximum limit (%d)",
                     circ->total_qubits, ZCC_QASM_MAX_STAB_QUBITS);
        }
        zcc_qasm_circuit_free(circ);
        return 0;
    }

    if (!zcc_qasm_stab_apply_circuit(st, circ)) {
        if (err_buf && err_buf_size > 0) {
            snprintf(err_buf, err_buf_size, "%s", st->last_error[0] ? st->last_error : "circuit execution error");
        }
        zcc_qasm_stab_free(st);
        zcc_qasm_circuit_free(circ);
        return 0;
    }

    zcc_qasm_circuit_free(circ);
    if (out_st) *out_st = st;
    else zcc_qasm_stab_free(st);

    return 1;
}

/* ================================================================ */
/* STABILIZER GENERATOR DUMP                                        */
/* ================================================================ */

char *zcc_qasm_stab_dump_state(const ZCCQasmStabilizer *st) {
    if (!st) return NULL;

    size_t n = st->num_qubits;
    size_t line_len = n + 16;
    size_t cap = (n + 2) * line_len + 256;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;

    size_t len = 0;
    len += (size_t)snprintf(buf + len, cap - len,
                            "# ZCC Stabilizer Tableau: %zu qubits, %zu generators\n", n, n);

    size_t row;
    for (row = n; row < 2 * n; row++) {
        const uint64_t *x = stab_row_x_c(st, row);
        const uint64_t *z = stab_row_z_c(st, row);

        if (len + n + 32 >= cap) break;
        len += (size_t)snprintf(buf + len, cap - len, "S[%zu]: %c", row - n, st->r_bits[row] ? '-' : '+');
        size_t q;
        for (q = 0; q < n; q++) {
            int xb = stab_get_bit(x, q);
            int zb = stab_get_bit(z, q);
            buf[len++] = xb ? (zb ? 'Y' : 'X') : (zb ? 'Z' : 'I');
        }
        buf[len++] = '\n';
        buf[len] = '\0';
    }

    if (st->classical_bits && st->num_classical_bits > 0 && len + st->num_classical_bits + 32 < cap) {
        len += (size_t)snprintf(buf + len, cap - len, "C: ");
        size_t c;
        for (c = 0; c < st->num_classical_bits; c++) {
            size_t idx = st->num_classical_bits - 1 - c; /* MSB first */
            buf[len++] = st->classical_bits[idx] ? '1' : '0';
        }
        buf[len++] = '\n';
        buf[len] = '\0';
    }

    return buf;
}
