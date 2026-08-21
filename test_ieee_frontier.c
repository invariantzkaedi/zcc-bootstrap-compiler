#include <stdio.h>
#include <assert.h>
#include "include/zcc_cordic.h"
#include "include/zcc_snan_poison.h"
#include "include/zcc_dd_real.h"

int main(void) {
    printf("==================================================\n");
    printf("[TEST] ZCC IEEE 754 Frontier Suite Verification\n");
    printf("==================================================\n\n");

    /* 1. Test CORDIC Transcendentals */
    printf("1. Verifying CORDIC Transcendentals (sin, cos, exp, log, sqrt)...\n");
    double sin_val = zcc_sin(ZCC_PI / 6.0); // sin(30 deg) = 0.5
    double cos_val = zcc_cos(ZCC_PI / 3.0); // cos(60 deg) = 0.5
    double sqrt_val = zcc_sqrt(144.0);      // sqrt(144) = 12
    double exp_val = zcc_exp(1.0);          // exp(1) ≈ 2.718281828459
    double log_val = zcc_log(2.718281828459045); // log(e) ≈ 1.0

    printf("   sin(pi/6) = %.10f (Expected: 0.5)\n", sin_val);
    printf("   cos(pi/3) = %.10f (Expected: 0.5)\n", cos_val);
    printf("   sqrt(144) = %.10f (Expected: 12.0)\n", sqrt_val);
    printf("   exp(1.0)  = %.10f (Expected: 2.7182818285)\n", exp_val);
    printf("   log(e)    = %.10f (Expected: 1.0)\n", log_val);

    assert(zcc_fabs(sin_val - 0.5) < 1e-6);
    assert(zcc_fabs(cos_val - 0.5) < 1e-6);
    assert(zcc_fabs(sqrt_val - 12.0) < 1e-6);
    assert(zcc_fabs(exp_val - 2.718281828459) < 1e-6);
    assert(zcc_fabs(log_val - 1.0) < 1e-6);
    printf("   -> [PASS] CORDIC Mathematical Parity Verified.\n\n");

    /* 2. Test Forensic sNaN Diagnostic Poison */
    printf("2. Verifying Forensic sNaN Diagnostic Poison (51-bit Payload)...\n");
    uint32_t test_func_id = 0x1A3F;
    uint16_t test_file_hash = 0xC4E2;
    uint16_t test_line_no = 847;

    double poisoned_val = zcc_make_forensic_snan(test_func_id, test_file_hash, test_line_no);
    zcc_snan_metadata_t decoded_meta;
    int is_snan = zcc_decode_forensic_snan(poisoned_val, &decoded_meta);

    printf("   Encoding -> FuncID: 0x%05X | FileHash: 0x%04X | Line: %u\n", test_func_id, test_file_hash, test_line_no);
    printf("   Decoded  -> FuncID: 0x%05X | FileHash: 0x%04X | Line: %u\n", decoded_meta.func_id, decoded_meta.file_hash, decoded_meta.line_no);
    assert(is_snan == 1);
    assert(decoded_meta.func_id == test_func_id);
    assert(decoded_meta.file_hash == test_file_hash);
    assert(decoded_meta.line_no == test_line_no);
    printf("   -> [PASS] 51-bit Forensic sNaN Payload Invariance Verified.\n\n");

    /* 3. Test Double-Double (dd_real) 106-bit Precision */
    printf("3. Verifying Double-Double (dd_real) 106-Bit Precision...\n");
    dd_real_t a = dd_from_double(1.0);
    dd_real_t b = dd_from_double(3.0);
    dd_real_t third = dd_div(a, b); // 1.0 / 3.0 in 106-bit precision
    dd_real_t back = dd_mul(third, b); // (1/3) * 3 ≈ 1.0

    printf("   1.0 / 3.0 -> hi: %.17f | lo: %.17e\n", third.hi, third.lo);
    printf("   (1/3) * 3 -> hi: %.17f | lo: %.17e\n", back.hi, back.lo);
    assert(zcc_fabs(back.hi - 1.0) < 1e-15);
    assert(zcc_fabs(back.lo) < 1e-30);
    printf("   -> [PASS] 106-Bit Error-Free Transformation Parity Verified.\n\n");

    printf("==================================================\n");
    printf("★ ALL IEEE 754 FRONTIER TEST KERNELS PASSED (3/3) ★\n");
    printf("==================================================\n");
    return 0;
}
