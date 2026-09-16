#ifndef RUST_C_ZERO_COPY_LAYOUT_H
#define RUST_C_ZERO_COPY_LAYOUT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 1. Adversarial FFI Test Struct (Forces interior and tail padding) */
typedef struct {
    uint8_t   tag_u8;         /* Offset 0, Size 1 (7 bytes padding follows) */
    uint64_t  signature_u64;  /* Offset 8, Size 8 */
    uint8_t   flags_u8;       /* Offset 16, Size 1 (7 bytes padding follows) */
    double    latency_f64;    /* Offset 24, Size 8 */
    uint32_t  shard_id_u32;   /* Offset 32, Size 4 (4 bytes padding follows) */
    void     *buffer_ptr;     /* Offset 40, Size 8 */
} ZccAdversarialFFIPacket;     /* Total Padded Size: 48 Bytes, Alignment: 8 Bytes */

/* Rust Incompatible Struct for Negative Control (Missing repr(C) / field reorder) */
typedef struct {
    uint64_t  signature_u64;
    uint8_t   tag_u8;
    uint8_t   flags_u8;
    uint32_t  shard_id_u32;
    double    latency_f64;
    void     *buffer_ptr;
} ZccIncompatibleFFIPacket;

/* C Functions exported to Rust */
void c_initialize_packet_with_canary(ZccAdversarialFFIPacket *pkt, uint8_t canary_byte);
int c_verify_rust_mutations(const ZccAdversarialFFIPacket *pkt, const void *expected_addr, uint8_t canary_byte);

/* Rust Functions exported to C */
void rust_mutate_packet(ZccAdversarialFFIPacket *pkt);
int rust_verify_c_packet(const ZccAdversarialFFIPacket *pkt, const void *expected_addr, uint8_t canary_byte);

#ifdef __cplusplus
}
#endif

#endif /* RUST_C_ZERO_COPY_LAYOUT_H */
