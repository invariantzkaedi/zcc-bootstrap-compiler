#include "include/zcc_qasm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <stdarg.h>

#if defined(__AVX2__) && defined(__FMA__)
#include <immintrin.h>
#define ZCC_SIM_AVX2 1
#else
#define ZCC_SIM_AVX2 0
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_SQRT1_2
#define M_SQRT1_2 0.70710678118654752440
#endif

/* ================================================================ */
/* DETERMINISTIC SPLITMIX64 PRNG                                    */
/* ================================================================ */

static inline uint64_t sim_rand_u64(uint64_t *state) {
    uint64_t z = (*state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static inline double sim_rand_double(uint64_t *state) {
    return (sim_rand_u64(state) >> 11) * (1.0 / 9007199254740992.0); /* [0.0, 1.0) */
}

/* ================================================================ */
/* CODY-WAITE RANGE REDUCTION & COMPLEX ARITHMETIC                  */
/* ================================================================ */

/* Sub-ULP accurate Cody-Waite range reduction and Chebyshev minimax evaluation */
static inline void zcc_cody_waite_sincos(double theta, double *s, double *c) {
    const double inv_half_pi = 0.636619772367581343075535;
    const double C1 = 1.570796326794896557998982;
    const double C2 = 6.123233995736766035868820e-17;

    double y = theta * inv_half_pi;
    int64_t k = (int64_t)(y >= 0.0 ? (y + 0.5) : (y - 0.5));
    double r = (theta - (double)k * C1) - (double)k * C2;

    double r2 = r * r;
    /* Degree-11 / Degree-10 Chebyshev minimax polynomials */
    double s_poly = r * (1.0 + r2 * (-1.666666666666666666666667e-01 +
                               r2 * ( 8.333333333333333333333333e-03 +
                               r2 * (-1.984126984126984126984127e-04 +
                               r2 * ( 2.755731922398589065255732e-06 +
                               r2 * (-2.505210838544171877505211e-08))))));

    double c_poly = 1.0 + r2 * (-5.000000000000000000000000e-01 +
                          r2 * ( 4.166666666666666666666667e-02 +
                          r2 * (-1.388888888888888888888889e-03 +
                          r2 * ( 2.480158730158730158730159e-05 +
                          r2 * (-2.755731922398589065255732e-07 +
                          r2 * ( 2.087675698786809897921009e-09))))));

    int quad = (int)(k & 3);
    if (quad < 0) quad += 4;

    switch (quad) {
        case 0: *s = s_poly;  *c = c_poly;  break;
        case 1: *s = c_poly;  *c = -s_poly; break;
        case 2: *s = -s_poly; *c = -c_poly; break;
        case 3: *s = -c_poly; *c = s_poly;  break;
    }
}

static inline ZCCComplex c_make(double r, double i) {
    ZCCComplex z = { r, i };
    return z;
}

static inline ZCCComplex c_add(ZCCComplex a, ZCCComplex b) {
    ZCCComplex z = { a.real + b.real, a.imag + b.imag };
    return z;
}

static inline ZCCComplex c_sub(ZCCComplex a, ZCCComplex b) {
    ZCCComplex z = { a.real - b.real, a.imag - b.imag };
    return z;
}

static inline ZCCComplex c_mul(ZCCComplex a, ZCCComplex b) {
    ZCCComplex z = {
        a.real * b.real - a.imag * b.imag,
        a.real * b.imag + a.imag * b.real
    };
    return z;
}

static inline ZCCComplex c_scale(ZCCComplex a, double s) {
    ZCCComplex z = { a.real * s, a.imag * s };
    return z;
}

static inline double c_norm_sq(ZCCComplex a) {
    return a.real * a.real + a.imag * a.imag;
}

static inline ZCCComplex c_exp_i(double theta) {
    double s, c;
    zcc_cody_waite_sincos(theta, &s, &c);
    ZCCComplex z = { c, s };
    return z;
}

/* ================================================================ */
/* SIMULATOR LIFECYCLE & STATE CREATION                             */
/* ================================================================ */

ZCCQasmSimulator *zcc_qasm_sim_create(size_t num_qubits, size_t num_clbits, uint64_t seed) {
    if (num_qubits > ZCC_QASM_MAX_SIM_QUBITS) {
        return NULL;
    }

    size_t num_amplitudes = (size_t)1 << num_qubits;
    if (num_qubits > 0 && (num_amplitudes >> num_qubits) != 1) {
        return NULL; /* Overflow protection */
    }

    ZCCQasmSimulator *sim = (ZCCQasmSimulator *)calloc(1, sizeof(ZCCQasmSimulator));
    if (!sim) return NULL;

    sim->num_qubits = num_qubits;
    sim->num_amplitudes = num_amplitudes;
    sim->amplitudes = (ZCCComplex *)calloc(num_amplitudes, sizeof(ZCCComplex));
    if (!sim->amplitudes) {
        free(sim);
        return NULL;
    }

    sim->num_classical_bits = num_clbits;
    if (num_clbits > 0) {
        sim->classical_bits = (unsigned int *)calloc(num_clbits, sizeof(unsigned int));
        if (!sim->classical_bits) {
            free(sim->amplitudes);
            free(sim);
            return NULL;
        }
    }

    sim->rng_state = (seed != 0) ? seed : 0x123456789ABCDEF0ULL;

    /* Initialize ground state |0...0> = 1 + 0i */
    sim->amplitudes[0].real = 1.0;
    sim->amplitudes[0].imag = 0.0;

    return sim;
}

void zcc_qasm_sim_free(ZCCQasmSimulator *sim) {
    if (!sim) return;
    if (sim->amplitudes) free(sim->amplitudes);
    if (sim->classical_bits) free(sim->classical_bits);
    free(sim);
}

void zcc_qasm_sim_reset_state(ZCCQasmSimulator *sim) {
    if (!sim || !sim->amplitudes) return;
    memset(sim->amplitudes, 0, sim->num_amplitudes * sizeof(ZCCComplex));
    sim->amplitudes[0].real = 1.0;
    sim->amplitudes[0].imag = 0.0;
    if (sim->classical_bits && sim->num_classical_bits > 0) {
        memset(sim->classical_bits, 0, sim->num_classical_bits * sizeof(unsigned int));
    }
}

double zcc_qasm_sim_norm(const ZCCQasmSimulator *sim) {
    if (!sim || !sim->amplitudes) return 0.0;
    double sum = 0.0;
    size_t i;
    for (i = 0; i < sim->num_amplitudes; i++) {
        sum += c_norm_sq(sim->amplitudes[i]);
    }
    return sum;
}

/* ================================================================ */
/* GATE MATRIX KERNELS (AVX2 + BLOCK-STRIDED SIMD)                  */
/* ================================================================ */

/* In-place generic 2x2 unitary application on target_qubit */
static void apply_matrix_2x2(ZCCQasmSimulator *sim, size_t target_qubit,
                             ZCCComplex u00, ZCCComplex u01,
                             ZCCComplex u10, ZCCComplex u11) {
    if (!sim || target_qubit >= sim->num_qubits) return;

    size_t stride = (size_t)1 << target_qubit;
    size_t two_stride = stride << 1;
    size_t num_amps = sim->num_amplitudes;

    if (stride == 1) {
        /* Target qubit 0: consecutive pairs (2k, 2k+1) */
        for (size_t i = 0; i < num_amps; i += 2) {
            ZCCComplex a = sim->amplitudes[i];
            ZCCComplex b = sim->amplitudes[i + 1];
            sim->amplitudes[i]     = c_add(c_mul(u00, a), c_mul(u01, b));
            sim->amplitudes[i + 1] = c_add(c_mul(u10, a), c_mul(u11, b));
        }
        return;
    }

#if ZCC_SIM_AVX2
    __m256d u00_r = _mm256_set1_pd(u00.real);
    __m256d u00_i = _mm256_set1_pd(u00.imag);
    __m256d u01_r = _mm256_set1_pd(u01.real);
    __m256d u01_i = _mm256_set1_pd(u01.imag);
    __m256d u10_r = _mm256_set1_pd(u10.real);
    __m256d u10_i = _mm256_set1_pd(u10.imag);
    __m256d u11_r = _mm256_set1_pd(u11.real);
    __m256d u11_i = _mm256_set1_pd(u11.imag);
    __m256d sign_mask = _mm256_set_pd(1.0, -1.0, 1.0, -1.0);

    for (size_t block = 0; block < num_amps; block += two_stride) {
        size_t k = 0;
        for (; k + 1 < stride; k += 2) {
            size_t idx0 = block + k;
            size_t idx1 = idx0 + stride;

            __m256d va = _mm256_loadu_pd((const double *)&sim->amplitudes[idx0]);
            __m256d vb = _mm256_loadu_pd((const double *)&sim->amplitudes[idx1]);

            __m256d va_swapped = _mm256_shuffle_pd(va, va, 0x5);
            __m256d vb_swapped = _mm256_shuffle_pd(vb, vb, 0x5);

            __m256d va_rot = _mm256_mul_pd(va_swapped, sign_mask);
            __m256d vb_rot = _mm256_mul_pd(vb_swapped, sign_mask);

            __m256d u00_a = _mm256_fmadd_pd(u00_i, va_rot, _mm256_mul_pd(u00_r, va));
            __m256d u01_b = _mm256_fmadd_pd(u01_i, vb_rot, _mm256_mul_pd(u01_r, vb));
            __m256d out_a = _mm256_add_pd(u00_a, u01_b);

            __m256d u10_a = _mm256_fmadd_pd(u10_i, va_rot, _mm256_mul_pd(u10_r, va));
            __m256d u11_b = _mm256_fmadd_pd(u11_i, vb_rot, _mm256_mul_pd(u11_r, vb));
            __m256d out_b = _mm256_add_pd(u10_a, u11_b);

            _mm256_storeu_pd((double *)&sim->amplitudes[idx0], out_a);
            _mm256_storeu_pd((double *)&sim->amplitudes[idx1], out_b);
        }

        for (; k < stride; k++) {
            size_t i = block + k;
            size_t j = i + stride;
            ZCCComplex a = sim->amplitudes[i];
            ZCCComplex b = sim->amplitudes[j];
            sim->amplitudes[i] = c_add(c_mul(u00, a), c_mul(u01, b));
            sim->amplitudes[j] = c_add(c_mul(u10, a), c_mul(u11, b));
        }
    }
#else
    for (size_t block = 0; block < num_amps; block += two_stride) {
        for (size_t k = 0; k < stride; k++) {
            size_t i = block + k;
            size_t j = i + stride;
            ZCCComplex a = sim->amplitudes[i];
            ZCCComplex b = sim->amplitudes[j];
            sim->amplitudes[i] = c_add(c_mul(u00, a), c_mul(u01, b));
            sim->amplitudes[j] = c_add(c_mul(u10, a), c_mul(u11, b));
        }
    }
#endif
}

/* In-place generic Controlled-2x2 unitary application */
static void apply_controlled_matrix_2x2(ZCCQasmSimulator *sim, size_t control_qubit, size_t target_qubit,
                                        ZCCComplex u00, ZCCComplex u01,
                                        ZCCComplex u10, ZCCComplex u11) {
    if (!sim || control_qubit >= sim->num_qubits || target_qubit >= sim->num_qubits || control_qubit == target_qubit) return;

    size_t cbit = (size_t)1 << control_qubit;
    size_t tbit = (size_t)1 << target_qubit;
    size_t num_amps = sim->num_amplitudes;
    size_t i;

    for (i = 0; i < num_amps; i++) {
        if ((i & cbit) != 0 && (i & tbit) == 0) {
            size_t j = i | tbit;
            ZCCComplex a = sim->amplitudes[i];
            ZCCComplex b = sim->amplitudes[j];

            sim->amplitudes[i] = c_add(c_mul(u00, a), c_mul(u01, b));
            sim->amplitudes[j] = c_add(c_mul(u10, a), c_mul(u11, b));
        }
    }
}

/* SWAP Gate */
static void apply_swap(ZCCQasmSimulator *sim, size_t q0, size_t q1) {
    if (!sim || q0 >= sim->num_qubits || q1 >= sim->num_qubits || q0 == q1) return;
    size_t b0 = (size_t)1 << q0;
    size_t b1 = (size_t)1 << q1;
    size_t i;

    for (i = 0; i < sim->num_amplitudes; i++) {
        int v0 = ((i & b0) != 0);
        int v1 = ((i & b1) != 0);
        if (v0 && !v1) {
            size_t j = (i ^ b0) | b1;
            ZCCComplex tmp = sim->amplitudes[i];
            sim->amplitudes[i] = sim->amplitudes[j];
            sim->amplitudes[j] = tmp;
        }
    }
}

/* iSWAP Gate */
static void apply_iswap(ZCCQasmSimulator *sim, size_t q0, size_t q1) {
    if (!sim || q0 >= sim->num_qubits || q1 >= sim->num_qubits || q0 == q1) return;
    size_t b0 = (size_t)1 << q0;
    size_t b1 = (size_t)1 << q1;
    size_t i;

    for (i = 0; i < sim->num_amplitudes; i++) {
        int v0 = ((i & b0) != 0);
        int v1 = ((i & b1) != 0);
        if (v0 && !v1) {
            size_t j = (i ^ b0) | b1;
            ZCCComplex a01 = sim->amplitudes[j];
            ZCCComplex a10 = sim->amplitudes[i];
            /* iSWAP: |01> -> i|10>, |10> -> i|01> */
            sim->amplitudes[j] = c_make(-a10.imag, a10.real);
            sim->amplitudes[i] = c_make(-a01.imag, a01.real);
        }
    }
}

/* RZZ Gate: diag(e^{-i theta/2}, e^{i theta/2}, e^{i theta/2}, e^{-i theta/2}) */
static void apply_rzz(ZCCQasmSimulator *sim, size_t q0, size_t q1, double theta) {
    if (!sim || q0 >= sim->num_qubits || q1 >= sim->num_qubits || q0 == q1) return;
    size_t b0 = (size_t)1 << q0;
    size_t b1 = (size_t)1 << q1;
    ZCCComplex p_neg = c_exp_i(-0.5 * theta);
    ZCCComplex p_pos = c_exp_i(0.5 * theta);
    size_t i;

    for (i = 0; i < sim->num_amplitudes; i++) {
        int v0 = ((i & b0) != 0);
        int v1 = ((i & b1) != 0);
        if (v0 == v1) {
            sim->amplitudes[i] = c_mul(sim->amplitudes[i], p_neg);
        } else {
            sim->amplitudes[i] = c_mul(sim->amplitudes[i], p_pos);
        }
    }
}

/* Toffoli Gate (CCX) */
static void apply_ccx(ZCCQasmSimulator *sim, size_t c0, size_t c1, size_t t) {
    if (!sim || c0 >= sim->num_qubits || c1 >= sim->num_qubits || t >= sim->num_qubits) return;
    if (c0 == c1 || c0 == t || c1 == t) return;

    size_t bit_c0 = (size_t)1 << c0;
    size_t bit_c1 = (size_t)1 << c1;
    size_t bit_t  = (size_t)1 << t;
    size_t i;

    for (i = 0; i < sim->num_amplitudes; i++) {
        if ((i & bit_c0) != 0 && (i & bit_c1) != 0 && (i & bit_t) == 0) {
            size_t j = i | bit_t;
            ZCCComplex tmp = sim->amplitudes[i];
            sim->amplitudes[i] = sim->amplitudes[j];
            sim->amplitudes[j] = tmp;
        }
    }
}

/* Fredkin Gate (CSWAP) */
static void apply_cswap(ZCCQasmSimulator *sim, size_t c, size_t t0, size_t t1) {
    if (!sim || c >= sim->num_qubits || t0 >= sim->num_qubits || t1 >= sim->num_qubits) return;
    if (c == t0 || c == t1 || t0 == t1) return;

    size_t bit_c  = (size_t)1 << c;
    size_t bit_t0 = (size_t)1 << t0;
    size_t bit_t1 = (size_t)1 << t1;
    size_t i;

    for (i = 0; i < sim->num_amplitudes; i++) {
        if ((i & bit_c) != 0) {
            int v0 = ((i & bit_t0) != 0);
            int v1 = ((i & bit_t1) != 0);
            if (v0 && !v1) {
                size_t j = (i ^ bit_t0) | bit_t1;
                ZCCComplex tmp = sim->amplitudes[i];
                sim->amplitudes[i] = sim->amplitudes[j];
                sim->amplitudes[j] = tmp;
            }
        }
    }
}

/* ================================================================ */
/* MEASUREMENT & RESET SEMANTICS                                    */
/* ================================================================ */

static int sim_measure_qubit(ZCCQasmSimulator *sim, size_t target_qubit, size_t clbit_idx) {
    if (!sim || target_qubit >= sim->num_qubits) return 0;

    size_t bit = (size_t)1 << target_qubit;
    double p1 = 0.0;
    size_t i;

    for (i = 0; i < sim->num_amplitudes; i++) {
        if ((i & bit) != 0) {
            p1 += c_norm_sq(sim->amplitudes[i]);
        }
    }

    double r = sim_rand_double(&sim->rng_state);
    int outcome = (r >= (1.0 - p1)) ? 1 : 0;
    double norm_factor = outcome ? sqrt(p1) : sqrt(1.0 - p1);

    if (norm_factor < 1e-15) norm_factor = 1e-15; /* Numerical safety */
    double inv_norm = 1.0 / norm_factor;

    for (i = 0; i < sim->num_amplitudes; i++) {
        int is_one = ((i & bit) != 0);
        if (is_one == outcome) {
            sim->amplitudes[i] = c_scale(sim->amplitudes[i], inv_norm);
        } else {
            sim->amplitudes[i].real = 0.0;
            sim->amplitudes[i].imag = 0.0;
        }
    }

    if (sim->classical_bits && clbit_idx < sim->num_classical_bits) {
        sim->classical_bits[clbit_idx] = (unsigned int)outcome;
    }

    return outcome;
}

static void sim_reset_qubit(ZCCQasmSimulator *sim, size_t target_qubit) {
    if (!sim || target_qubit >= sim->num_qubits) return;
    int res = sim_measure_qubit(sim, target_qubit, (size_t)-1);
    if (res == 1) {
        /* Apply X to return to |0> */
        ZCCComplex x00 = c_make(0, 0), x01 = c_make(1, 0), x10 = c_make(1, 0), x11 = c_make(0, 0);
        apply_matrix_2x2(sim, target_qubit, x00, x01, x10, x11);
    }
}

/* ================================================================ */
/* ENTANGLEMENT ENTROPY (SINGLE-QUBIT BIPARTITION)                  */
/* ================================================================ */

double zcc_qasm_sim_entropy_1q(const ZCCQasmSimulator *sim, size_t target_qubit) {
    if (!sim || target_qubit >= sim->num_qubits) return 0.0;

    size_t bit = (size_t)1 << target_qubit;
    double rho00 = 0.0;
    double rho11 = 0.0;
    ZCCComplex rho01 = c_make(0, 0);
    size_t i;

    for (i = 0; i < sim->num_amplitudes; i++) {
        if ((i & bit) == 0) {
            size_t j = i | bit;
            rho00 += c_norm_sq(sim->amplitudes[i]);
            rho11 += c_norm_sq(sim->amplitudes[j]);
            /* rho01 += amp[i] * conj(amp[j]) */
            ZCCComplex a = sim->amplitudes[i];
            ZCCComplex b_conj = c_make(sim->amplitudes[j].real, -sim->amplitudes[j].imag);
            rho01 = c_add(rho01, c_mul(a, b_conj));
        }
    }

    /* Trace normalization */
    double trace = rho00 + rho11;
    if (trace > 1e-15) {
        rho00 /= trace;
        rho11 /= trace;
        rho01 = c_scale(rho01, 1.0 / trace);
    }

    /* Eigenvalues of 2x2 Hermitian matrix */
    double diff = rho00 - rho11;
    double disc = sqrt(diff * diff + 4.0 * c_norm_sq(rho01));
    double lambda1 = 0.5 * (1.0 + disc);
    double lambda2 = 0.5 * (1.0 - disc);

    double s = 0.0;
    if (lambda1 > 1e-14) s -= lambda1 * (log(lambda1) / log(2.0));
    if (lambda2 > 1e-14) s -= lambda2 * (log(lambda2) / log(2.0));

    if (s < 0.0) s = 0.0;
    return s;
}

/* ================================================================ */
/* CIRCUIT OPERATION DISPATCH & CUSTOM GATE INTERPRETER             */
/* ================================================================ */

static const ZCCQasmRegister *find_sim_reg(const ZCCQasmCircuit *circ, const char *name) {
    if (!circ || !name) return NULL;
    int i;
    for (i = 0; i < circ->num_registers; i++) {
        if (strcmp(circ->registers[i].name, name) == 0) return &circ->registers[i];
    }
    return NULL;
}

static size_t resolve_qubit_idx(const ZCCQasmCircuit *circ, const ZCCQasmQubitRef *ref) {
    const ZCCQasmRegister *reg = find_sim_reg(circ, ref->reg_name);
    if (!reg) return 0;
    return (size_t)(reg->base_offset + (ref->index >= 0 ? ref->index : 0));
}

static size_t resolve_clbit_idx(const ZCCQasmCircuit *circ, const ZCCQasmQubitRef *ref) {
    const ZCCQasmRegister *reg = find_sim_reg(circ, ref->reg_name);
    if (!reg) return 0;
    return (size_t)(reg->base_offset + (ref->index >= 0 ? ref->index : 0));
}

static unsigned int read_creg_val(const ZCCQasmSimulator *sim, const ZCCQasmCircuit *circ, const char *reg_name) {
    const ZCCQasmRegister *reg = find_sim_reg(circ, reg_name);
    if (!reg || !sim->classical_bits) return 0;
    unsigned int val = 0;
    int k;
    for (k = 0; k < reg->size; k++) {
        size_t idx = (size_t)(reg->base_offset + k);
        if (idx < sim->num_classical_bits && sim->classical_bits[idx]) {
            val |= (1U << k);
        }
    }
    return val;
}

static int execute_op(ZCCQasmSimulator *sim, const ZCCQasmCircuit *circ, const ZCCQasmOp *op,
                     const char *param_names[MAX_QASM_GATE_PARAMS], const double *param_vals, int n_pbound,
                     const char *qubit_names[MAX_QASM_GATE_QUBITS], const size_t *qubit_indices, int n_qbound,
                     int depth);

static int execute_gate_by_kind(ZCCQasmSimulator *sim, ZCCQasmOpKind kind, const char *gate_name,
                                const double *params, int num_params,
                                const size_t *qubits, int num_qubits) {
    switch (kind) {
        case QASM_OP_ID:
            break;
        case QASM_OP_H: {
            ZCCComplex u00 = c_make(M_SQRT1_2, 0), u01 = c_make(M_SQRT1_2, 0);
            ZCCComplex u10 = c_make(M_SQRT1_2, 0), u11 = c_make(-M_SQRT1_2, 0);
            apply_matrix_2x2(sim, qubits[0], u00, u01, u10, u11);
            break;
        }
        case QASM_OP_X: {
            ZCCComplex u00 = c_make(0, 0), u01 = c_make(1, 0);
            ZCCComplex u10 = c_make(1, 0), u11 = c_make(0, 0);
            apply_matrix_2x2(sim, qubits[0], u00, u01, u10, u11);
            break;
        }
        case QASM_OP_Y: {
            ZCCComplex u00 = c_make(0, 0), u01 = c_make(0, -1);
            ZCCComplex u10 = c_make(0, 1), u11 = c_make(0, 0);
            apply_matrix_2x2(sim, qubits[0], u00, u01, u10, u11);
            break;
        }
        case QASM_OP_Z: {
            ZCCComplex u00 = c_make(1, 0), u01 = c_make(0, 0);
            ZCCComplex u10 = c_make(0, 0), u11 = c_make(-1, 0);
            apply_matrix_2x2(sim, qubits[0], u00, u01, u10, u11);
            break;
        }
        case QASM_OP_S: {
            ZCCComplex u00 = c_make(1, 0), u01 = c_make(0, 0);
            ZCCComplex u10 = c_make(0, 0), u11 = c_make(0, 1);
            apply_matrix_2x2(sim, qubits[0], u00, u01, u10, u11);
            break;
        }
        case QASM_OP_SDG: {
            ZCCComplex u00 = c_make(1, 0), u01 = c_make(0, 0);
            ZCCComplex u10 = c_make(0, 0), u11 = c_make(0, -1);
            apply_matrix_2x2(sim, qubits[0], u00, u01, u10, u11);
            break;
        }
        case QASM_OP_T: {
            ZCCComplex u00 = c_make(1, 0), u01 = c_make(0, 0);
            ZCCComplex u10 = c_make(0, 0), u11 = c_exp_i(M_PI / 4.0);
            apply_matrix_2x2(sim, qubits[0], u00, u01, u10, u11);
            break;
        }
        case QASM_OP_TDG: {
            ZCCComplex u00 = c_make(1, 0), u01 = c_make(0, 0);
            ZCCComplex u10 = c_make(0, 0), u11 = c_exp_i(-M_PI / 4.0);
            apply_matrix_2x2(sim, qubits[0], u00, u01, u10, u11);
            break;
        }
        case QASM_OP_RX: {
            double theta = (num_params > 0) ? params[0] : 0.0;
            double st, ct;
            zcc_cody_waite_sincos(0.5 * theta, &st, &ct);
            ZCCComplex u00 = c_make(ct, 0), u01 = c_make(0, -st);
            ZCCComplex u10 = c_make(0, -st), u11 = c_make(ct, 0);
            apply_matrix_2x2(sim, qubits[0], u00, u01, u10, u11);
            break;
        }
        case QASM_OP_RY: {
            double theta = (num_params > 0) ? params[0] : 0.0;
            double st, ct;
            zcc_cody_waite_sincos(0.5 * theta, &st, &ct);
            ZCCComplex u00 = c_make(ct, 0), u01 = c_make(-st, 0);
            ZCCComplex u10 = c_make(st, 0), u11 = c_make(ct, 0);
            apply_matrix_2x2(sim, qubits[0], u00, u01, u10, u11);
            break;
        }
        case QASM_OP_RZ: {
            double theta = (num_params > 0) ? params[0] : 0.0;
            ZCCComplex u00 = c_exp_i(-0.5 * theta), u01 = c_make(0, 0);
            ZCCComplex u10 = c_make(0, 0), u11 = c_exp_i(0.5 * theta);
            apply_matrix_2x2(sim, qubits[0], u00, u01, u10, u11);
            break;
        }
        case QASM_OP_P:
        case QASM_OP_U1: {
            double lam = (num_params > 0) ? params[0] : 0.0;
            ZCCComplex u00 = c_make(1, 0), u01 = c_make(0, 0);
            ZCCComplex u10 = c_make(0, 0), u11 = c_exp_i(lam);
            apply_matrix_2x2(sim, qubits[0], u00, u01, u10, u11);
            break;
        }
        case QASM_OP_U2: {
            double phi = (num_params > 0) ? params[0] : 0.0;
            double lam = (num_params > 1) ? params[1] : 0.0;
            ZCCComplex u00 = c_make(M_SQRT1_2, 0);
            ZCCComplex u01 = c_scale(c_exp_i(lam), -M_SQRT1_2);
            ZCCComplex u10 = c_scale(c_exp_i(phi), M_SQRT1_2);
            ZCCComplex u11 = c_scale(c_exp_i(phi + lam), M_SQRT1_2);
            apply_matrix_2x2(sim, qubits[0], u00, u01, u10, u11);
            break;
        }
        case QASM_OP_U:
        case QASM_OP_U3: {
            double theta = (num_params > 0) ? params[0] : 0.0;
            double phi   = (num_params > 1) ? params[1] : 0.0;
            double lam   = (num_params > 2) ? params[2] : 0.0;
            double st, ct;
            zcc_cody_waite_sincos(0.5 * theta, &st, &ct);
            ZCCComplex u00 = c_make(ct, 0);
            ZCCComplex u01 = c_scale(c_exp_i(lam), -st);
            ZCCComplex u10 = c_scale(c_exp_i(phi), st);
            ZCCComplex u11 = c_scale(c_exp_i(phi + lam), ct);
            apply_matrix_2x2(sim, qubits[0], u00, u01, u10, u11);
            break;
        }
        case QASM_OP_CX: {
            ZCCComplex u00 = c_make(0, 0), u01 = c_make(1, 0);
            ZCCComplex u10 = c_make(1, 0), u11 = c_make(0, 0);
            apply_controlled_matrix_2x2(sim, qubits[0], qubits[1], u00, u01, u10, u11);
            break;
        }
        case QASM_OP_CY: {
            ZCCComplex u00 = c_make(0, 0), u01 = c_make(0, -1);
            ZCCComplex u10 = c_make(0, 1), u11 = c_make(0, 0);
            apply_controlled_matrix_2x2(sim, qubits[0], qubits[1], u00, u01, u10, u11);
            break;
        }
        case QASM_OP_CZ: {
            ZCCComplex u00 = c_make(1, 0), u01 = c_make(0, 0);
            ZCCComplex u10 = c_make(0, 0), u11 = c_make(-1, 0);
            apply_controlled_matrix_2x2(sim, qubits[0], qubits[1], u00, u01, u10, u11);
            break;
        }
        case QASM_OP_CH: {
            ZCCComplex u00 = c_make(M_SQRT1_2, 0), u01 = c_make(M_SQRT1_2, 0);
            ZCCComplex u10 = c_make(M_SQRT1_2, 0), u11 = c_make(-M_SQRT1_2, 0);
            apply_controlled_matrix_2x2(sim, qubits[0], qubits[1], u00, u01, u10, u11);
            break;
        }
        case QASM_OP_SWAP:
            apply_swap(sim, qubits[0], qubits[1]);
            break;
        case QASM_OP_ISWAP:
            apply_iswap(sim, qubits[0], qubits[1]);
            break;
        case QASM_OP_CRX: {
            double theta = (num_params > 0) ? params[0] : 0.0;
            double st, ct;
            zcc_cody_waite_sincos(0.5 * theta, &st, &ct);
            ZCCComplex u00 = c_make(ct, 0), u01 = c_make(0, -st);
            ZCCComplex u10 = c_make(0, -st), u11 = c_make(ct, 0);
            apply_controlled_matrix_2x2(sim, qubits[0], qubits[1], u00, u01, u10, u11);
            break;
        }
        case QASM_OP_CRY: {
            double theta = (num_params > 0) ? params[0] : 0.0;
            double st, ct;
            zcc_cody_waite_sincos(0.5 * theta, &st, &ct);
            ZCCComplex u00 = c_make(ct, 0), u01 = c_make(-st, 0);
            ZCCComplex u10 = c_make(st, 0), u11 = c_make(ct, 0);
            apply_controlled_matrix_2x2(sim, qubits[0], qubits[1], u00, u01, u10, u11);
            break;
        }
        case QASM_OP_CRZ: {
            double theta = (num_params > 0) ? params[0] : 0.0;
            ZCCComplex u00 = c_exp_i(-0.5 * theta), u01 = c_make(0, 0);
            ZCCComplex u10 = c_make(0, 0), u11 = c_exp_i(0.5 * theta);
            apply_controlled_matrix_2x2(sim, qubits[0], qubits[1], u00, u01, u10, u11);
            break;
        }
        case QASM_OP_CU1: {
            double lam = (num_params > 0) ? params[0] : 0.0;
            ZCCComplex u00 = c_make(1, 0), u01 = c_make(0, 0);
            ZCCComplex u10 = c_make(0, 0), u11 = c_exp_i(lam);
            apply_controlled_matrix_2x2(sim, qubits[0], qubits[1], u00, u01, u10, u11);
            break;
        }
        case QASM_OP_CU3: {
            double theta = (num_params > 0) ? params[0] : 0.0;
            double phi   = (num_params > 1) ? params[1] : 0.0;
            double lam   = (num_params > 2) ? params[2] : 0.0;
            double st, ct;
            zcc_cody_waite_sincos(0.5 * theta, &st, &ct);
            ZCCComplex u00 = c_make(ct, 0);
            ZCCComplex u01 = c_scale(c_exp_i(lam), -st);
            ZCCComplex u10 = c_scale(c_exp_i(phi), st);
            ZCCComplex u11 = c_scale(c_exp_i(phi + lam), ct);
            apply_controlled_matrix_2x2(sim, qubits[0], qubits[1], u00, u01, u10, u11);
            break;
        }
        case QASM_OP_RZZ: {
            double theta = (num_params > 0) ? params[0] : 0.0;
            apply_rzz(sim, qubits[0], qubits[1], theta);
            break;
        }
        case QASM_OP_CCX:
            apply_ccx(sim, qubits[0], qubits[1], qubits[2]);
            break;
        case QASM_OP_CSWAP:
            apply_cswap(sim, qubits[0], qubits[1], qubits[2]);
            break;
        default:
            return 0;
    }
    return 1;
}

static double eval_param_expr(const ZCCQasmExpr *expr,
                              const char *param_names[MAX_QASM_GATE_PARAMS],
                              const double *param_vals, int n_pbound) {
    if (!expr) return 0.0;
    if (expr->kind == EXPR_PARAM_REF) {
        int i;
        for (i = 0; i < n_pbound; i++) {
            if (param_names[i] && strcmp(param_names[i], expr->param_name) == 0) {
                return param_vals[i];
            }
        }
        return 0.0;
    }
    if (expr->kind == EXPR_NUM) return expr->num_val;
    if (expr->kind == EXPR_PI) return M_PI;
    if (expr->kind == EXPR_ADD) return eval_param_expr(expr->lhs, param_names, param_vals, n_pbound) + eval_param_expr(expr->rhs, param_names, param_vals, n_pbound);
    if (expr->kind == EXPR_SUB) return eval_param_expr(expr->lhs, param_names, param_vals, n_pbound) - eval_param_expr(expr->rhs, param_names, param_vals, n_pbound);
    if (expr->kind == EXPR_MUL) return eval_param_expr(expr->lhs, param_names, param_vals, n_pbound) * eval_param_expr(expr->rhs, param_names, param_vals, n_pbound);
    if (expr->kind == EXPR_DIV) {
        double r = eval_param_expr(expr->rhs, param_names, param_vals, n_pbound);
        return (r != 0.0) ? (eval_param_expr(expr->lhs, param_names, param_vals, n_pbound) / r) : 0.0;
    }
    if (expr->kind == EXPR_NEG) return -eval_param_expr(expr->lhs, param_names, param_vals, n_pbound);
    if (expr->kind == EXPR_SIN) return sin(eval_param_expr(expr->lhs, param_names, param_vals, n_pbound));
    if (expr->kind == EXPR_COS) return cos(eval_param_expr(expr->lhs, param_names, param_vals, n_pbound));
    if (expr->kind == EXPR_TAN) return tan(eval_param_expr(expr->lhs, param_names, param_vals, n_pbound));
    if (expr->kind == EXPR_LN) {
        double v = eval_param_expr(expr->lhs, param_names, param_vals, n_pbound);
        return (v > 0.0) ? log(v) : 0.0;
    }
    if (expr->kind == EXPR_EXP) return exp(eval_param_expr(expr->lhs, param_names, param_vals, n_pbound));
    if (expr->kind == EXPR_SQRT) {
        double v = eval_param_expr(expr->lhs, param_names, param_vals, n_pbound);
        return (v >= 0.0) ? sqrt(v) : 0.0;
    }
    return 0.0;
}

static size_t resolve_bound_qubit(const ZCCQasmCircuit *circ, const ZCCQasmQubitRef *ref,
                                  const char *qubit_names[MAX_QASM_GATE_QUBITS],
                                  const size_t *qubit_indices, int n_qbound) {
    int i;
    for (i = 0; i < n_qbound; i++) {
        if (qubit_names[i] && strcmp(qubit_names[i], ref->reg_name) == 0) {
            return qubit_indices[i];
        }
    }
    return resolve_qubit_idx(circ, ref);
}

static const ZCCQasmGateDef *find_custom_gate(const ZCCQasmCircuit *circ, const char *name) {
    if (!circ || !name) return NULL;
    int i;
    for (i = 0; i < circ->num_custom_gates; i++) {
        if (strcmp(circ->custom_gates[i].name, name) == 0) return &circ->custom_gates[i];
    }
    return NULL;
}

static int execute_op(ZCCQasmSimulator *sim, const ZCCQasmCircuit *circ, const ZCCQasmOp *op,
                     const char *param_names[MAX_QASM_GATE_PARAMS], const double *param_vals, int n_pbound,
                     const char *qubit_names[MAX_QASM_GATE_QUBITS], const size_t *qubit_indices, int n_qbound,
                     int depth) {
    if (!sim || !circ || !op) return 1;
    if (depth > 16) {
        snprintf(sim->last_error, sizeof(sim->last_error), "exceeded maximum custom gate recursion depth (16)");
        return 0;
    }

    /* Check Condition if present */
    if (op->has_condition) {
        unsigned int cur_val = read_creg_val(sim, circ, op->cond_reg);
        if (cur_val != (unsigned int)op->cond_val) {
            return 1; /* Skip */
        }
    }

    if (op->kind == QASM_OP_BARRIER) {
        return 1; /* No-op in simulator */
    }

    if (op->kind == QASM_OP_RESET) {
        size_t qidx = resolve_bound_qubit(circ, &op->qubits[0], qubit_names, qubit_indices, n_qbound);
        sim_reset_qubit(sim, qidx);
        return 1;
    }

    if (op->kind == QASM_OP_MEASURE) {
        size_t qidx = resolve_bound_qubit(circ, &op->qubits[0], qubit_names, qubit_indices, n_qbound);
        size_t cidx = resolve_clbit_idx(circ, &op->meas_target);
        sim_measure_qubit(sim, qidx, cidx);
        return 1;
    }

    /* Evaluate actual parameters */
    double actual_params[MAX_QASM_GATE_PARAMS] = {0};
    int p;
    for (p = 0; p < op->num_params; p++) {
        actual_params[p] = eval_param_expr(op->params[p], param_names, param_vals, n_pbound);
    }

    /* Resolve actual qubit indices */
    size_t actual_qubits[MAX_QASM_GATE_QUBITS] = {0};
    int q;
    for (q = 0; q < op->num_qubits; q++) {
        actual_qubits[q] = resolve_bound_qubit(circ, &op->qubits[q], qubit_names, qubit_indices, n_qbound);
    }

    /* Try executing as standard built-in gate */
    if (execute_gate_by_kind(sim, op->kind, op->gate_name, actual_params, op->num_params, actual_qubits, op->num_qubits)) {
        return 1;
    }

    /* Custom gate lookup & body expansion */
    const ZCCQasmGateDef *gdef = find_custom_gate(circ, op->gate_name);
    if (gdef) {
        const char *new_pnames[MAX_QASM_GATE_PARAMS] = {0};
        double new_pvals[MAX_QASM_GATE_PARAMS] = {0};
        int pi;
        for (pi = 0; pi < gdef->num_params && pi < op->num_params; pi++) {
            new_pnames[pi] = gdef->param_names[pi];
            new_pvals[pi] = actual_params[pi];
        }

        const char *new_qnames[MAX_QASM_GATE_QUBITS] = {0};
        size_t new_qindices[MAX_QASM_GATE_QUBITS] = {0};
        int qi;
        for (qi = 0; qi < gdef->num_qubits && qi < op->num_qubits; qi++) {
            new_qnames[qi] = gdef->qubit_names[qi];
            new_qindices[qi] = actual_qubits[qi];
        }

        const ZCCQasmOp *bop = gdef->body_head;
        while (bop) {
            if (!execute_op(sim, circ, bop, new_pnames, new_pvals, gdef->num_params,
                            new_qnames, new_qindices, gdef->num_qubits, depth + 1)) {
                return 0;
            }
            bop = bop->next;
        }
        return 1;
    }

    snprintf(sim->last_error, sizeof(sim->last_error), "unrecognized gate operation '%s'", op->gate_name);
    return 0;
}

int zcc_qasm_sim_apply_circuit(ZCCQasmSimulator *sim, const ZCCQasmCircuit *circ) {
    if (!sim || !circ) return 0;
    const ZCCQasmOp *op = circ->head_op;
    while (op) {
        if (!execute_op(sim, circ, op, NULL, NULL, 0, NULL, NULL, 0, 0)) {
            return 0;
        }
        op = op->next;
    }
    return 1;
}

int zcc_qasm_sim_run_file(const char *filename, uint64_t seed, ZCCQasmSimulator **out_sim, char *err_buf, size_t err_buf_size) {
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

    size_t total_qubits = (size_t)circ->total_qubits;
    size_t total_clbits = (size_t)circ->total_clbits;

    ZCCQasmSimulator *sim = zcc_qasm_sim_create(total_qubits, total_clbits, seed);
    if (!sim) {
        if (err_buf && err_buf_size > 0) {
            snprintf(err_buf, err_buf_size, "simulator allocation failed: requested %zu qubits exceeds maximum limit (%d)",
                     total_qubits, ZCC_QASM_MAX_SIM_QUBITS);
        }
        zcc_qasm_circuit_free(circ);
        return 0;
    }

    if (!zcc_qasm_sim_apply_circuit(sim, circ)) {
        if (err_buf && err_buf_size > 0) {
            snprintf(err_buf, err_buf_size, "%s", sim->last_error[0] ? sim->last_error : "circuit execution error");
        }
        zcc_qasm_sim_free(sim);
        zcc_qasm_circuit_free(circ);
        return 0;
    }

    zcc_qasm_circuit_free(circ);
    if (out_sim) *out_sim = sim;
    else zcc_qasm_sim_free(sim);

    return 1;
}

/* ================================================================ */
/* STATEVECTOR FORMATTED DUMP                                       */
/* ================================================================ */

char *zcc_qasm_sim_dump_state(const ZCCQasmSimulator *sim, double threshold) {
    if (!sim || !sim->amplitudes) return NULL;

    size_t cap = 2048;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    buf[0] = '\0';
    size_t len = 0;

    size_t n = sim->num_qubits;
    size_t i;

    for (i = 0; i < sim->num_amplitudes; i++) {
        double prob = c_norm_sq(sim->amplitudes[i]);
        if (prob >= threshold) {
            char bitstr[64];
            size_t b;
            for (b = 0; b < n; b++) {
                size_t q = n - 1 - b; /* MSB to LSB display */
                bitstr[b] = ((i & ((size_t)1 << q)) != 0) ? '1' : '0';
            }
            bitstr[n] = '\0';

            char line[128];
            snprintf(line, sizeof(line), "|%s>: %+.8f %+.8fi (prob: %.6f)\n",
                     bitstr, sim->amplitudes[i].real, sim->amplitudes[i].imag, prob);

            size_t llen = strlen(line);
            while (len + llen + 1 >= cap) {
                cap *= 2;
                buf = (char *)realloc(buf, cap);
            }
            memcpy(buf + len, line, llen);
            len += llen;
            buf[len] = '\0';
        }
    }

    return buf;
}
