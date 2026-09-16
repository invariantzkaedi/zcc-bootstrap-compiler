#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include "rust_c_zero_copy_layout.h"

static uint8_t g_dummy_buffer[256];

void c_initialize_packet_with_canary(ZccAdversarialFFIPacket *pkt, uint8_t canary_byte) {
    if (!pkt) return;
    /* Fill entire 48 bytes with padding canary */
    memset(pkt, canary_byte, sizeof(ZccAdversarialFFIPacket));

    /* Populate exact field values */
    pkt->tag_u8 = 0x7E;
    pkt->signature_u64 = 0x0123456789ABCDEFULL;
    pkt->flags_u8 = 0x3C;
    pkt->latency_f64 = 3.141592653589793;
    pkt->shard_id_u32 = 0xA5A55A5AU;
    pkt->buffer_ptr = (void*)g_dummy_buffer;
}

int c_verify_rust_mutations(const ZccAdversarialFFIPacket *pkt, const void *expected_addr, uint8_t canary_byte) {
    if (!pkt) return 1;

    /* 1. Assert Pointer Identity across FFI boundary (Zero-Copy Verification) */
    if ((const void*)pkt != expected_addr) {
        printf("[-] FFI Error: Pointer identity failed! Expected %p, got %p\n", expected_addr, (const void*)pkt);
        return 10;
    }

    /* 2. Verify Mutated Field Values */
    if (pkt->tag_u8 != 0xA1) return 11;
    if (pkt->signature_u64 != 0xFEDCBA9876543210ULL) return 12;
    if (pkt->flags_u8 != 0x9B) return 13;
    if (pkt->latency_f64 != 2.718281828459045) return 14;
    if (pkt->shard_id_u32 != 0x12345678U) return 15;
    if (pkt->buffer_ptr != (void*)(g_dummy_buffer + 64)) return 16;

    /* 3. Padding Canaries Verification (Verify interior & tail padding bytes remain 0xA5) */
    const uint8_t *raw = (const uint8_t*)pkt;

    /* Padding after tag_u8: bytes [1..7] */
    for (int i = 1; i <= 7; i++) {
        if (raw[i] != canary_byte) {
            printf("[-] Padding Canary Corrupted at Offset %d! Expected 0x%02X, got 0x%02X\n", i, canary_byte, raw[i]);
            return 20;
        }
    }

    /* Padding after flags_u8: bytes [17..23] */
    for (int i = 17; i <= 23; i++) {
        if (raw[i] != canary_byte) {
            printf("[-] Padding Canary Corrupted at Offset %d! Expected 0x%02X, got 0x%02X\n", i, canary_byte, raw[i]);
            return 21;
        }
    }

    /* Padding after shard_id_u32: bytes [36..39] */
    for (int i = 36; i <= 39; i++) {
        if (raw[i] != canary_byte) {
            printf("[-] Padding Canary Corrupted at Offset %d! Expected 0x%02X, got 0x%02X\n", i, canary_byte, raw[i]);
            return 22;
        }
    }

    return 0; /* PASS */
}

int main(int argc, char **argv) {
    int run_negative = (argc > 1 && strcmp(argv[1], "--negative-control") == 0);

    printf("========================================================================\n");
    printf("[RUST-FFI-LAYOUT-001] BIDIRECTIONAL ZERO-COPY C & RUST GAUNTLET\n");
    printf("========================================================================\n");

    /* 1. Layout Vector Invariant Check */
    size_t sz = sizeof(ZccAdversarialFFIPacket);
    size_t al = _Alignof(ZccAdversarialFFIPacket);
    size_t off_a = offsetof(ZccAdversarialFFIPacket, tag_u8);
    size_t off_b = offsetof(ZccAdversarialFFIPacket, signature_u64);
    size_t off_c = offsetof(ZccAdversarialFFIPacket, flags_u8);
    size_t off_d = offsetof(ZccAdversarialFFIPacket, latency_f64);
    size_t off_e = offsetof(ZccAdversarialFFIPacket, shard_id_u32);
    size_t off_f = offsetof(ZccAdversarialFFIPacket, buffer_ptr);

    printf("[*] C Layout Vector: Size=%zu, Align=%zu, Offsets=[%zu,%zu,%zu,%zu,%zu,%zu]\n",
           sz, al, off_a, off_b, off_c, off_d, off_e, off_f);

    if (sz != 48 || al != 8 || off_a != 0 || off_b != 8 || off_c != 16 || off_d != 24 || off_e != 32 || off_f != 40) {
        printf("[-] C Layout Vector Failed Canonical System V ABI Specification!\n");
        return 1;
    }

    if (run_negative) {
        printf("[!] Executing Negative Control (Expecting Mismatch Rejection)...\n");
        ZccIncompatibleFFIPacket bad_pkt;
        memset(&bad_pkt, 0xA5, sizeof(bad_pkt));
        bad_pkt.signature_u64 = 0x0123456789ABCDEFULL;
        bad_pkt.tag_u8 = 0x7E;
        bad_pkt.flags_u8 = 0x3C;
        bad_pkt.shard_id_u32 = 0xA5A55A5AU;
        bad_pkt.latency_f64 = 3.141592653589793;
        bad_pkt.buffer_ptr = (void*)g_dummy_buffer;

        /* Passing incompatible layout to Rust verifier MUST fail (non-zero return) */
        int bad_res = rust_verify_c_packet((const ZccAdversarialFFIPacket*)&bad_pkt, (const void*)&bad_pkt, 0xA5);
        if (bad_res != 0) {
            printf("[+] Negative Control Passed: Incompatible Layout Correctly Rejected by Rust Verifier with Error Code %d\n", bad_res);
            return 0;
        }
        printf("[-] Negative Control Failed: Incompatible layout was silently accepted!\n");
        return 99;
    }

    /* 2. Allocate C packet with padding canaries */
    ZccAdversarialFFIPacket pkt;
    uint8_t canary = 0xA5;
    c_initialize_packet_with_canary(&pkt, canary);

    /* 3. Cross FFI Boundary into Rust */
    rust_mutate_packet(&pkt);

    /* 4. Verify C-Side after Rust Mutation */
    int res = c_verify_rust_mutations(&pkt, (const void*)&pkt, canary);
    if (res != 0) {
        printf("[-] C-Side Verification of Rust Mutations Failed with Code %d\n", res);
        return res;
    }
    printf("[+] Direction 1 (C Alloc -> Rust Mutate -> C Verify): 100%% VERIFIED\n");

    /* 5. Direction 2: Rust Verification of C packet */
    c_initialize_packet_with_canary(&pkt, canary);
    int rust_res = rust_verify_c_packet(&pkt, (const void*)&pkt, canary);
    if (rust_res != 0) {
        printf("[-] Rust-Side Verification of C Packet Failed with Code %d\n", rust_res);
        return rust_res;
    }
    printf("[+] Direction 2 (C Alloc -> C Populate -> Rust Verify): 100%% VERIFIED\n");

    printf("========================================================================\n");
    printf("[+] RUST-FFI-LAYOUT-001 GAUNTLET PASSED: COMPLETE ZERO-COPY BIT-EXACT SEAL!\n");
    printf("========================================================================\n");
    return 0;
}
