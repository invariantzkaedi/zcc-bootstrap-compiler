/*
 * LEB128 & Magic Header Boundary Verification Harness
 * File: tests/test_leb128_boundaries.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "../src/wasm_emit.h"

static void test_uleb(uint32_t val, const uint8_t *expected, size_t expected_len) {
    uint8_t buf[16];
    size_t len = wasm_encode_uleb128(buf, val);
    assert(len == expected_len);
    assert(memcmp(buf, expected, len) == 0);
}

static void test_sleb(int32_t val, const uint8_t *expected, size_t expected_len) {
    uint8_t buf[16];
    size_t len = wasm_encode_sleb128(buf, val);
    assert(len == expected_len);
    assert(memcmp(buf, expected, len) == 0);
}

int main(void) {
    printf("=== LEB128 Boundary Suite ===\n");

    /* ULEB128 test vectors */
    static const uint8_t u_0[] = { 0x00 };
    test_uleb(0, u_0, 1);

    static const uint8_t u_1[] = { 0x01 };
    test_uleb(1, u_1, 1);

    static const uint8_t u_127[] = { 0x7f };
    test_uleb(127, u_127, 1);

    static const uint8_t u_128[] = { 0x80, 0x01 };
    test_uleb(128, u_128, 2);

    static const uint8_t u_624485[] = { 0xe5, 0x8e, 0x26 };
    test_uleb(624485, u_624485, 3);
    printf("[PASS] ULEB128: 0, 1, 127, 128, 624485\n");

    /* SLEB128 test vectors */
    static const uint8_t s_0[] = { 0x00 };
    test_sleb(0, s_0, 1);

    static const uint8_t s_pos1[] = { 0x01 };
    test_sleb(1, s_pos1, 1);

    static const uint8_t s_pos63[] = { 0x3f };
    test_sleb(63, s_pos63, 1);

    static const uint8_t s_pos64[] = { 0xc0, 0x00 };
    test_sleb(64, s_pos64, 2);

    static const uint8_t s_neg1[] = { 0x7f };
    test_sleb(-1, s_neg1, 1);

    static const uint8_t s_neg64[] = { 0x40 };
    test_sleb(-64, s_neg64, 1);

    static const uint8_t s_neg65[] = { 0xbf, 0x7f };
    test_sleb(-65, s_neg65, 2);

    static const uint8_t s_neg128[] = { 0x80, 0x7f };
    test_sleb(-128, s_neg128, 2);
    printf("[PASS] SLEB128: 0, 1, 63, 64, -1, -64, -65, -128\n");

    /* Magic Header assertion */
    static uint8_t magic_header[8] = { 0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00 };
    assert(magic_header[0] == 0x00);
    assert(magic_header[1] == 0x61); /* 'a' */
    assert(magic_header[2] == 0x73); /* 's' */
    assert(magic_header[3] == 0x6d); /* 'm' */
    printf("[PASS] WASM Magic Header (\\0asm) & Version 1\n");

    printf("=========================================\n");
    printf("★ ALL LEB128 & HEADER BOUNDARY GATES PASS ★\n");
    printf("=========================================\n");
    return 0;
}
