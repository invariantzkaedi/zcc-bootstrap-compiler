#ifndef ZCC_QEC_DECODER_H
#define ZCC_QEC_DECODER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * ZCC Hardware-Intrinsic Packed 16-Qubit Pauli Algebra & Multi-Code
 * Fault-Tolerant Quantum Error Correction (QEC) Syndrome Decoder
 * =========================================================================
 * Packed 16-qubit symplectic representation in uint32_t:
 *   bits [15:0]  = X_mask (bit q = 1 iff X or Y acts on qubit q)
 *   bits [31:16] = Z_mask (bit q+16 = 1 iff Z or Y acts on qubit q)
 * Single-qubit encoding:
 *   I_q = (0, 0), X_q = (1, 0), Z_q = (0, 1), Y_q = (1, 1) = i X_q Z_q
 * ========================================================================= */

#define ZCC_QPAULI_I          0x00000000u
#define ZCC_QPAULI_X(q)       (1u << ((unsigned int)(q) & 15u))
#define ZCC_QPAULI_Z(q)       (1u << (((unsigned int)(q) & 15u) + 16u))
#define ZCC_QPAULI_Y(q)       (ZCC_QPAULI_X(q) | ZCC_QPAULI_Z(q))

/* Host-compiler fallback shims when not compiled by ZCC's Quad-ISA backend */
#if (defined(__GNUC__) || defined(__clang__)) && !defined(__ZCC__)
static inline int zcc_hw_qpauli_weight(unsigned int p) {
    unsigned int m = (p | (p >> 16)) & 0xFFFFu;
    return __builtin_popcount(m);
}
static inline int zcc_hw_qpauli_commute(unsigned int a, unsigned int b) {
    unsigned int anti = ((a & (b >> 16)) ^ ((a >> 16) & b)) & 0xFFFFu;
    return __builtin_popcount(anti) & 1;
}
static inline unsigned int zcc_hw_qpauli_mul(unsigned int a, unsigned int b) {
    return a ^ b;
}
static inline int zcc_hw_qpauli_phase(unsigned int a, unsigned int b) {
    unsigned int z1 = a >> 16, z2 = b >> 16;
    unsigned int u = a & z2, v = z1 & b;
    unsigned int anti = (u ^ v) & 0xFFFFu;
    unsigned int pos = ((u & (b ^ z1)) | (v & ~(a | z2))) & 0xFFFFu;
    unsigned int neg = anti ^ pos;
    return (__builtin_popcount(pos) - __builtin_popcount(neg)) & 3;
}
#else
#define zcc_hw_qpauli_weight(p)      __builtin_qpauli_weight(p)
#define zcc_hw_qpauli_commute(a, b)  __builtin_qpauli_commute((a), (b))
#define zcc_hw_qpauli_mul(a, b)      __builtin_qpauli_mul((a), (b))
#define zcc_hw_qpauli_phase(a, b)    __builtin_qpauli_phase((a), (b))
#endif

#define ZCC_QEC_MAX_STABILIZERS 12
#define ZCC_QEC_MAX_SYNDROMES   256

typedef enum {
    ZCC_QEC_CODE_LAFLAMME_5_1_3 = 0,
    ZCC_QEC_CODE_STEANE_7_1_3   = 1,
    ZCC_QEC_CODE_SHOR_9_1_3     = 2,
    ZCC_QEC_CODE_SURFACE_17     = 3
} ZccQecCodeKind;

typedef struct {
    ZccQecCodeKind kind;
    const char    *name;
    int            n_qubits;
    int            k_logical;
    int            distance;
    int            num_stabilizers;
    uint32_t       stabilizers[ZCC_QEC_MAX_STABILIZERS];
    uint32_t       logical_x;
    uint32_t       logical_z;
    uint32_t       syndrome_lut[ZCC_QEC_MAX_SYNDROMES];
    uint8_t        syndrome_valid[ZCC_QEC_MAX_SYNDROMES];
} ZccQecCode;

typedef struct {
    uint32_t injected_error;
    uint32_t syndrome;
    uint32_t recovery_op;
    uint32_t residual_op;
    int      residual_weight;
    int      multiplication_phase_mod4;
    int      post_syndrome;
    int      logical_x_flipped;
    int      logical_z_flipped;
    int      is_exact_identity;
    int      is_degenerate_stabilizer;
    int      logical_preserved;
} ZccQecDecodeResult;

/* Initialize built-in stabilizer codes and synthesize minimum-weight LUTs */
int zcc_qec_init_laflamme_5_1_3(ZccQecCode *code);
int zcc_qec_init_steane_7_1_3(ZccQecCode *code);
int zcc_qec_init_shor_9_1_3(ZccQecCode *code);
int zcc_qec_init_surface_17(ZccQecCode *code);

/* Core hardware-intrinsic syndrome extraction & stabilizer group verification */
uint32_t zcc_qec_extract_syndrome(const ZccQecCode *code, uint32_t error_op);
int zcc_qec_verify_stabilizer_group(const ZccQecCode *code);

/* Minimum-weight LUT + CSS Fano-plane decoding & fault-tolerant recovery check */
uint32_t zcc_qec_lookup_recovery(const ZccQecCode *code, uint32_t syndrome);
int zcc_qec_decode_and_recover(const ZccQecCode *code, uint32_t error_op, ZccQecDecodeResult *out);

/* Pauli group 2-cocycle associativity & degeneracy audit */
int zcc_qec_verify_2cocycle_batch(const uint32_t *ops, int count);
int zcc_qec_verify_shor_degeneracy(const ZccQecCode *shor_code, int *out_degenerate_pairs);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_QEC_DECODER_H */

