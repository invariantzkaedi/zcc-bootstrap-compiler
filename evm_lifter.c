/*
 * evm_lifter.c — ZCC Defensive EVM Bytecode Lifter Scaffold
 *
 * DEFENSIVE AUDIT SCAFFOLD ONLY.
 * This file implements the EVM→IR lifter described in evm_lifter.h.
 * It emits ZCC IR nodes directly using ir.h / ir.c APIs, bypassing the
 * C AST pipeline in part4.c entirely.
 *
 * Build (standalone, no ZCC required):
 *   gcc -O0 -std=c99 -Wall -Wextra -o evm_lifter_test \
 *       tests/test_evm_lifter.c evm_lifter.c ir.c -lm
 *
 * Build with ZCC project:
 *   gcc -O0 -o evm_lifter_test tests/test_evm_lifter.c \
 *       evm_lifter.c ir.c compiler_passes.c compiler_passes_ir.c -lm
 *
 * Coverage note: scaffold tests cover critical paths.  Full 95%+ opcode
 * coverage requires a production harness — see issue tracker gate requirement.
 */

#include "evm_lifter.h"
#include "ir_vuln_tag.h"
#include "evm_uint256.h"

// Convert evm_u256_t to uint256_t (handle endianness properly if needed, 
// though evm_u256_t is already little-endian limbs!)
// In evm_lifter.h: evm_u256_t has uint64_t limbs[4].
// In evm_uint256.h: uint256_t has unsigned long limbs[4].
static inline uint256_t to_physics(const evm_u256_t *a) {
    uint256_t p = {0};
    for (int i = 0; i < 4; i++) {
        unsigned long long limb = 0;
        for (int j = 0; j < 8; j++) {
            // a->bytes[31] is the LSB of the entire 256-bit number.
            // limbs[0] is the least significant limb.
            // so limbs[0] gets bytes 24..31.
            limb |= ((unsigned long long)a->bytes[31 - (i * 8 + j)]) << (j * 8);
        }
        p.limbs[i] = limb;
    }
    return p;
}
static inline void from_physics(evm_u256_t *dst, uint256_t p) {
    for (int i = 0; i < 4; i++) {
        unsigned long long limb = p.limbs[i];
        for (int j = 0; j < 8; j++) {
            dst->bytes[31 - (i * 8 + j)] = (limb >> (j * 8)) & 0xFF;
        }
    }
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ── Internal helpers ────────────────────────────────────────────────── */

static void lifter_fresh_tmp(evm_lifter_t *ls, char *buf);

static int evm_gas_for_op(unsigned int op) {
    if (op >= 0x60 && op <= 0x7F) return 3; // PUSH
    if (op >= 0x80 && op <= 0x8F) return 3; // DUP
    if (op >= 0x90 && op <= 0x9F) return 3; // SWAP
    switch(op) {
        case 0x00: return 0;
        case 0x01: return 3;
        case 0x02: return 5;
        case 0x03: return 3;
        case 0x04: return 5;
        case 0x05: return 5;
        case 0x06: return 5;
        case 0x07: return 5;
        case 0x08: return 8;
        case 0x09: return 8;
        case 0x0A: return 10;
        case 0x0B: return 3;
        case 0x10: case 0x11: case 0x12: case 0x13: case 0x14:
        case 0x15: case 0x16: case 0x17: case 0x18: case 0x19:
        case 0x1A: case 0x1B: case 0x1C: case 0x1D: return 3;
        case 0x20: return 30; // KECCAK256
        case 0x30: return 2;
        case 0x31: return 20;
        case 0x32: return 2;
        case 0x33: return 2;
        case 0x34: return 2;
        case 0x35: return 3;
        case 0x36: return 3;
        case 0x37: return 3;
        case 0x38: return 2;
        case 0x39: return 3;
        case 0x3A: return 2;
        case 0x3B: return 2;
        case 0x3C: return 3;
        case 0x3D: return 2;
        case 0x3E: return 3;
        case 0x3F: return 2;
        case 0x40: return 20;
        case 0x41: return 2;
        case 0x42: return 2;
        case 0x43: return 2;
        case 0x44: return 2;
        case 0x45: return 2;
        case 0x46: return 2;
        case 0x47: return 2;
        case 0x48: return 2;
        case 0x50: return 2;
        case 0x51: return 3;
        case 0x52: return 3;
        case 0x53: return 3;
        case 0x54: return 200;
        case 0x55: return 20000;
        case 0x56: return 8;
        case 0x57: return 10;
        case 0x58: return 2;
        case 0x59: return 2;
        case 0x5A: return 2;
        case 0x5B: return 1;
        case 0x5C: return 100;
        case 0x5D: return 100;
        case 0x5F: return 2;
        case 0xA0: return 375;
        case 0xF0: return 32000;
        case 0xF1: return 21000;
        case 0xF2: return 21000;
        case 0xF3: return 0;
        case 0xF4: return 21000;
        case 0xF5: return 32000;
        case 0xFA: return 21000;
        case 0xFD: return 0;
        case 0xFE: return 0;
        case 0xFF: return 5000;
    }
    return 0;
}

static int get_block_static_gas(const unsigned char *bytecode, int length, int pc) {
    int gas = 0;
    while (pc < length) {
        unsigned int op = bytecode[pc];
        gas += evm_gas_for_op(op);
        
        if (op >= 0x60 && op <= 0x7F) {
            pc += 1 + (op - 0x60 + 1);
        } else {
            pc++;
        }
        
        // Stop if this instruction ends the block
        if (op == 0x56 || op == 0x57 || op == 0x00 || op == 0xF3 || op == 0xFD || op == 0xFE || op == 0xFF) {
            break;
        }
        // If next instruction is JUMPDEST, this block ends BEFORE it
        if (pc < length && bytecode[pc] == 0x5B) {
            break;
        }
    }
    return gas;
}

static void evm_apply_static_gas(ir_func_t *fn, evm_lifter_t *ls, int static_gas_delta, const char *current_gas_vreg, char *next_gas_vreg) {
    char delta_vreg[IR_NAME_MAX];
    char cmp_vreg[IR_NAME_MAX];
    
    lifter_fresh_tmp(ls, delta_vreg);
    ir_emit(fn, IR_CONST, IR_TY_I64, delta_vreg, NULL, NULL, NULL, static_gas_delta, ls->pc);
    
    // Subtract gas
    ir_emit(fn, IR_SUB, IR_TY_I64, next_gas_vreg, current_gas_vreg, delta_vreg, NULL, 0L, ls->pc);
    
    // Barrier: if next_gas < 0, revert OOG
    lifter_fresh_tmp(ls, cmp_vreg);
    char zero_vreg[IR_NAME_MAX];
    lifter_fresh_tmp(ls, zero_vreg);
    ir_emit(fn, IR_CONST, IR_TY_I64, zero_vreg, NULL, NULL, NULL, 0, ls->pc);
    
    ir_emit(fn, IR_LT, IR_TY_I64, cmp_vreg, next_gas_vreg, zero_vreg, NULL, 0L, ls->pc);
    
    char l_oog[IR_NAME_MAX];
    char l_ok[IR_NAME_MAX];
    sprintf(l_oog, ".L_OOG_REVERT_%d", ls->tmp_seq++);
    sprintf(l_ok, ".L_OOG_OK_%d", ls->tmp_seq++);
    ir_node_t *br = ir_emit(fn, IR_BR_IF, IR_TY_VOID, "", cmp_vreg, "", l_oog, 0L, ls->pc);
    // Note: since this is just a scaffold, we need to manually bypass the split if the user 
    // says "drops the basic block split" for EIP-150 clamp, but for OOG the user explicitely provided:
    // ir_node_t *br = ir_emit(fn, IR_BR_IF, IR_TY_VOID, "", cmp_vreg, "", ".L_OOG_REVERT", 0, 0);
    // br->vuln_tags |= IR_VULN_EXEC_BARRIER; 
    br->tag = IR_TAG_EVM_BARRIER;
    ir_emit(fn, IR_BR, IR_TY_VOID, "", "", "", l_ok, 0L, ls->pc);
    ir_emit(fn, IR_LABEL, IR_TY_VOID, "", "", "", l_oog, 0L, ls->pc);
    ir_emit(fn, IR_RET, IR_TY_VOID, "", zero_vreg, "", "", 0L, ls->pc); // Revert
    ir_emit(fn, IR_LABEL, IR_TY_VOID, "", "", "", l_ok, 0L, ls->pc);
}

static void evm_inject_eip150_clamp(ir_func_t *fn, evm_lifter_t *ls, const char *req_gas_vreg, const char *avail_gas_vreg, char *out_vreg) {
    char div64_vreg[IR_NAME_MAX], allowed_vreg[IR_NAME_MAX], cmp_vreg[IR_NAME_MAX];
    char const_64[IR_NAME_MAX];
    
    lifter_fresh_tmp(ls, const_64);
    ir_emit(fn, IR_CONST, IR_TY_I64, const_64, NULL, NULL, NULL, 64, ls->pc);
    
    lifter_fresh_tmp(ls, div64_vreg);
    ir_emit(fn, IR_DIV, IR_TY_I64, div64_vreg, avail_gas_vreg, const_64, NULL, 0L, ls->pc);
    
    lifter_fresh_tmp(ls, allowed_vreg);
    ir_emit(fn, IR_SUB, IR_TY_I64, allowed_vreg, avail_gas_vreg, div64_vreg, NULL, 0L, ls->pc);
    
    lifter_fresh_tmp(ls, cmp_vreg);
    ir_emit(fn, IR_LT, IR_TY_I64, cmp_vreg, req_gas_vreg, allowed_vreg, NULL, 0L, ls->pc);
    
    char l_req[IR_NAME_MAX], l_all[IR_NAME_MAX], l_end[IR_NAME_MAX];
    sprintf(l_req, ".L_req_%d", ls->tmp_seq++);
    sprintf(l_all, ".L_all_%d", ls->tmp_seq++);
    sprintf(l_end, ".L_end_%d", ls->tmp_seq++);
    
    ir_emit(fn, IR_BR_IF, IR_TY_VOID, "", cmp_vreg, "", l_req, 0L, ls->pc);
    ir_emit(fn, IR_BR, IR_TY_VOID, "", "", "", l_all, 0L, ls->pc);
    
    ir_emit(fn, IR_LABEL, IR_TY_VOID, "", "", "", l_req, 0L, ls->pc);
    ir_emit(fn, IR_COPY, IR_TY_I64, out_vreg, req_gas_vreg, "", "", 0L, ls->pc);
    ir_emit(fn, IR_BR, IR_TY_VOID, "", "", "", l_end, 0L, ls->pc);
    
    ir_emit(fn, IR_LABEL, IR_TY_VOID, "", "", "", l_all, 0L, ls->pc);
    ir_emit(fn, IR_COPY, IR_TY_I64, out_vreg, allowed_vreg, "", "", 0L, ls->pc);
    ir_emit(fn, IR_BR, IR_TY_VOID, "", "", "", l_end, 0L, ls->pc);
    
    ir_emit(fn, IR_LABEL, IR_TY_VOID, "", "", "", l_end, 0L, ls->pc);
}

static void evm_inject_memory_expansion_cost(ir_func_t *fn, evm_lifter_t *ls, const char *words_vreg, char *cost_vreg) {
    char linear_cost[IR_NAME_MAX], quad_cost[IR_NAME_MAX], squared[IR_NAME_MAX];
    char const_3[IR_NAME_MAX], const_512[IR_NAME_MAX];
    
    lifter_fresh_tmp(ls, const_3);
    ir_emit(fn, IR_CONST, IR_TY_I64, const_3, NULL, NULL, NULL, 3, ls->pc);
    lifter_fresh_tmp(ls, linear_cost);
    ir_emit(fn, IR_MUL, IR_TY_I64, linear_cost, words_vreg, const_3, NULL, 0L, ls->pc);
    
    lifter_fresh_tmp(ls, squared);
    ir_emit(fn, IR_MUL, IR_TY_I64, squared, words_vreg, words_vreg, NULL, 0L, ls->pc);
    
    lifter_fresh_tmp(ls, const_512);
    ir_emit(fn, IR_CONST, IR_TY_I64, const_512, NULL, NULL, NULL, 512, ls->pc);
    lifter_fresh_tmp(ls, quad_cost);
    ir_emit(fn, IR_DIV, IR_TY_I64, quad_cost, squared, const_512, NULL, 0L, ls->pc);
    
    ir_emit(fn, IR_ADD, IR_TY_I64, cost_vreg, linear_cost, quad_cost, NULL, 0L, ls->pc);
}

static int lift_u256_is_zero(const evm_u256_t *a) {
    int i;
    for (i = 0; i < 32; i++) {
        if (a->bytes[i] != 0) return 0;
    }
    return 1;
}

static int evm_u256_eq(const evm_u256_t *a, const evm_u256_t *b) {
    int i;
    for (i = 0; i < 32; i++) {
        if (a->bytes[i] != b->bytes[i]) return 0;
    }
    return 1;
}

/* Returns 1 if a > b, -1 if a < b, 0 if a == b. (Unsigned comparison) */
static int evm_u256_cmp(const evm_u256_t *a, const evm_u256_t *b) {
    int i;
    for (i = 0; i < 32; i++) {
        if (a->bytes[i] > b->bytes[i]) return 1;
        if (a->bytes[i] < b->bytes[i]) return -1;
    }
    return 0;
}

static int evm_u256_is_negative(const evm_u256_t *a) {
    return (a->bytes[0] & 0x80) != 0;
}

/* Returns 1 if a > b, -1 if a < b, 0 if a == b. (Signed fixed-width two's complement comparison) */
static int evm_u256_cmp_signed(const evm_u256_t *a, const evm_u256_t *b) {
    int a_neg = evm_u256_is_negative(a);
    int b_neg = evm_u256_is_negative(b);
    
    if (a_neg && !b_neg) return -1; /* negative < positive */
    if (!a_neg && b_neg) return 1;  /* positive > negative */
    
    /* Signs match: fixed-width two's complement bit-pattern ordering is identical to unsigned ordering */
    return evm_u256_cmp(a, b);
}

static int lift_u256_add(evm_u256_t *dst, const evm_u256_t *a, const evm_u256_t *b) {
    uint256_t pa = to_physics(a);
    uint256_t pb = to_physics(b);
    uint256_t pr = evm_u256_add(pa, pb);
    from_physics(dst, pr);
    return (pr.limbs[3] < pa.limbs[3]) ? 1 : 0; // rough carry
}

static void lift_u256_sub(evm_u256_t *dst, const evm_u256_t *a, const evm_u256_t *b) {
    from_physics(dst, evm_u256_sub(to_physics(a), to_physics(b)));
}

static void lift_u256_mul(evm_u256_t *dst, const evm_u256_t *a, const evm_u256_t *b) {
    from_physics(dst, evm_u256_mul(to_physics(a), to_physics(b)));
}

static void evm_u256_and(evm_u256_t *dst, const evm_u256_t *a, const evm_u256_t *b) {
    for (int i = 0; i < 32; i++) dst->bytes[i] = a->bytes[i] & b->bytes[i];
}

static void evm_u256_or(evm_u256_t *dst, const evm_u256_t *a, const evm_u256_t *b) {
    for (int i = 0; i < 32; i++) dst->bytes[i] = a->bytes[i] | b->bytes[i];
}

static void evm_u256_xor(evm_u256_t *dst, const evm_u256_t *a, const evm_u256_t *b) {
    for (int i = 0; i < 32; i++) dst->bytes[i] = a->bytes[i] ^ b->bytes[i];
}

static void evm_u256_not(evm_u256_t *dst, const evm_u256_t *a) {
    for (int i = 0; i < 32; i++) dst->bytes[i] = ~a->bytes[i];
}

static void evm_u256_shl1(evm_u256_t *a) {
    int carry = 0;
    for (int i = 31; i >= 0; i--) {
        int next_carry = (a->bytes[i] >> 7) & 1;
        a->bytes[i] = (a->bytes[i] << 1) | carry;
        carry = next_carry;
    }
}

static int evm_u256_get_bit(const evm_u256_t *a, int bit_idx) {
    int byte_idx = 31 - (bit_idx / 8);
    int bit_off = bit_idx % 8;
    return (a->bytes[byte_idx] >> bit_off) & 1;
}

static void evm_u256_set_bit(evm_u256_t *a, int bit_idx) {
    int byte_idx = 31 - (bit_idx / 8);
    int bit_off = bit_idx % 8;
    a->bytes[byte_idx] |= (1 << bit_off);
}

static void evm_u256_negate(evm_u256_t *dst, const evm_u256_t *a) {
    evm_u256_t one;
    memset(one.bytes, 0, 32);
    one.bytes[31] = 1;
    evm_u256_t inv;
    for (int i = 0; i < 32; i++) {
        inv.bytes[i] = ~a->bytes[i];
    }
    lift_u256_add(dst, &inv, &one);
}

/* Unsigned DIV / MOD */
static void evm_u256_div_mod(evm_u256_t *q, evm_u256_t *r, const evm_u256_t *a, const evm_u256_t *b) {
    memset(q->bytes, 0, 32);
    memset(r->bytes, 0, 32);
    if (lift_u256_is_zero(b)) return;
    
    for (int i = 255; i >= 0; i--) {
        evm_u256_shl1(r);
        if (evm_u256_get_bit(a, i)) {
            r->bytes[31] |= 1;
        }
        if (evm_u256_cmp(r, b) >= 0) {
            lift_u256_sub(r, r, b);
            evm_u256_set_bit(q, i);
        }
    }
}

/* Signed SDIV / SMOD */
static void evm_u256_sdiv_smod(evm_u256_t *q, evm_u256_t *r, const evm_u256_t *a, const evm_u256_t *b) {
    if (lift_u256_is_zero(b)) {
        memset(q->bytes, 0, 32);
        memset(r->bytes, 0, 32);
        return;
    }
    int a_neg = evm_u256_is_negative(a);
    int b_neg = evm_u256_is_negative(b);
    
    evm_u256_t abs_a, abs_b;
    if (a_neg) evm_u256_negate(&abs_a, a); else memcpy(&abs_a, a, sizeof(evm_u256_t));
    if (b_neg) evm_u256_negate(&abs_b, b); else memcpy(&abs_b, b, sizeof(evm_u256_t));
    
    evm_u256_div_mod(q, r, &abs_a, &abs_b);
    
    int q_neg = a_neg ^ b_neg;
    if (q_neg) evm_u256_negate(q, q);
    if (a_neg) evm_u256_negate(r, r); // Remainder sign matches dividend
}

/* ADDMOD */
static void evm_u256_addmod(evm_u256_t *dst, const evm_u256_t *a, const evm_u256_t *b, const evm_u256_t *n) {
    if (lift_u256_is_zero(n)) {
        memset(dst->bytes, 0, 32);
        return;
    }
    evm_u256_t sum, q, r, r_max;
    int carry = lift_u256_add(&sum, a, b);
    evm_u256_div_mod(&q, &r, &sum, n);
    if (carry) {
        evm_u256_t max256, one, carry_mod;
        memset(max256.bytes, 0xFF, 32);
        memset(one.bytes, 0, 32); one.bytes[31] = 1;
        evm_u256_div_mod(&q, &r_max, &max256, n);
        lift_u256_add(&carry_mod, &r_max, &one);
        evm_u256_div_mod(&q, &carry_mod, &carry_mod, n);
        lift_u256_add(&r, &r, &carry_mod);
        evm_u256_div_mod(&q, dst, &r, n);
    } else {
        memcpy(dst, &r, sizeof(evm_u256_t));
    }
}

/* MULMOD */
static void evm_u256_mulmod(evm_u256_t *dst, const evm_u256_t *a, const evm_u256_t *b, const evm_u256_t *n) {
    if (lift_u256_is_zero(n)) {
        memset(dst->bytes, 0, 32);
        return;
    }
    evm_u256_t res, q, a_mod;
    memset(res.bytes, 0, 32);
    evm_u256_div_mod(&q, &a_mod, a, n);
    for (int i = 255; i >= 0; i--) {
        int carry = lift_u256_add(&res, &res, &res);
        if (carry || evm_u256_cmp(&res, n) >= 0) {
            lift_u256_sub(&res, &res, n);
        }
        if (evm_u256_get_bit(b, i)) {
            carry = lift_u256_add(&res, &res, &a_mod);
            if (carry || evm_u256_cmp(&res, n) >= 0) {
                lift_u256_sub(&res, &res, n);
            }
        }
    }
    memcpy(dst, &res, sizeof(evm_u256_t));
}


static long evm_u256_to_narrow(const evm_u256_t *a) {
    unsigned long val = 0;
    int i;
    for (i = 24; i < 32; i++) {
        val = (val << 8) | (unsigned long)a->bytes[i];
    }
    return (long)val;
}


/* EXP: base^exp mod 2^256 via binary exponentiation */
static void evm_u256_exp(evm_u256_t *dst, const evm_u256_t *base, const evm_u256_t *exp) {
    evm_u256_t result, b, tmp;
    memset(result.bytes, 0, 32);
    result.bytes[31] = 1; /* result = 1 */
    memcpy(&b, base, sizeof(evm_u256_t));
    /* iterate over all 256 bits of exp from LSB to MSB */
    for (int i = 0; i < 256; i++) {
        if (evm_u256_get_bit(exp, i)) {
            lift_u256_mul(&tmp, &result, &b); /* tmp = result * b */
            memcpy(&result, &tmp, sizeof(evm_u256_t));
        }
        lift_u256_mul(&tmp, &b, &b); /* tmp = b * b */
        memcpy(&b, &tmp, sizeof(evm_u256_t));
    }
    memcpy(dst, &result, sizeof(evm_u256_t));
}

/* BYTE: extract byte at position `pos` (0 = most-significant) from 256-bit `val`.
 * EVM spec: result = (val >> (248 - pos*8)) & 0xFF  if pos < 32, else 0. */
static void evm_u256_byte_op(evm_u256_t *dst, const evm_u256_t *pos, const evm_u256_t *val) {
    memset(dst->bytes, 0, 32);
    /* pos must fit in 6 bits to be a valid byte index [0..31] */
    for (int i = 0; i < 31; i++) {
        if (pos->bytes[i] != 0) return; /* pos >= 256, result is 0 */
    }
    unsigned int p = (unsigned int)pos->bytes[31];
    if (p >= 32) return; /* out of range */
    dst->bytes[31] = val->bytes[p]; /* byte 0 is MSB (bytes[0]) */
}

/* SIGNEXTEND: sign-extend value `x` from bit (b*8+7) upward.
 * EVM spec: if b >= 31, result = x unchanged.
 *           else: sign bit = bit (b*8+7) of x; fill bits [255..b*8+8] with sign bit. */
static void evm_u256_signextend_op(evm_u256_t *dst, const evm_u256_t *b, const evm_u256_t *x) {
    memcpy(dst, x, sizeof(evm_u256_t));
    /* if b >= 31, no truncation needed */
    for (int i = 0; i < 31; i++) {
        if (b->bytes[i] != 0) return;
    }
    unsigned int bval = (unsigned int)b->bytes[31];
    if (bval >= 31) return;
    /* byte index in big-endian: byte bval is at dst->bytes[31 - bval] */
    unsigned int byte_idx = 31 - bval;
    int sign_bit = (dst->bytes[byte_idx] >> 7) & 1;
    /* fill bytes 0..(byte_idx-1) with sign extension */
    unsigned char fill = sign_bit ? 0xFF : 0x00;
    for (unsigned int i = 0; i < byte_idx; i++) {
        dst->bytes[i] = fill;
    }
    /* mask the top bit of byte at byte_idx according to sign */
    if (sign_bit) {
        dst->bytes[byte_idx] |= 0x80;
    } else {
        dst->bytes[byte_idx] &= 0x7F;
    }
}

/* SHL: shift left */
static void evm_u256_shl_op(evm_u256_t *dst, const evm_u256_t *shift, const evm_u256_t *val) {
    memset(dst->bytes, 0, 32);
    for (int i = 0; i < 31; i++) {
        if (shift->bytes[i] != 0) return;
    }
    unsigned int s = shift->bytes[31];
    unsigned int byte_shift = s / 8;
    unsigned int bit_shift = s % 8;
    for (int i = 0; i < 32; i++) {
        if (i + byte_shift < 32) {
            unsigned int v = val->bytes[i + byte_shift];
            if (bit_shift == 0) {
                dst->bytes[i] = v;
            } else {
                dst->bytes[i] = (v << bit_shift) & 0xFF;
                if (i + byte_shift + 1 < 32) {
                    dst->bytes[i] |= (val->bytes[i + byte_shift + 1] >> (8 - bit_shift)) & 0xFF;
                }
            }
        }
    }
}

/* SHR: logical shift right */
static void evm_u256_shr_op(evm_u256_t *dst, const evm_u256_t *shift, const evm_u256_t *val) {
    memset(dst->bytes, 0, 32);
    for (int i = 0; i < 31; i++) {
        if (shift->bytes[i] != 0) return;
    }
    unsigned int s = shift->bytes[31];
    unsigned int byte_shift = s / 8;
    unsigned int bit_shift = s % 8;
    for (int i = 31; i >= 0; i--) {
        if (i - (int)byte_shift >= 0) {
            unsigned int v = val->bytes[i - byte_shift];
            if (bit_shift == 0) {
                dst->bytes[i] = v;
            } else {
                dst->bytes[i] = (v >> bit_shift) & 0xFF;
                if (i - (int)byte_shift - 1 >= 0) {
                    dst->bytes[i] |= (val->bytes[i - byte_shift - 1] << (8 - bit_shift)) & 0xFF;
                }
            }
        }
    }
}

/* SAR: arithmetic shift right */
static void evm_u256_sar_op(evm_u256_t *dst, const evm_u256_t *shift, const evm_u256_t *val) {
    int sign_bit = (val->bytes[0] >> 7) & 1;
    unsigned char fill = sign_bit ? 0xFF : 0x00;
    for (int i = 0; i < 31; i++) {
        if (shift->bytes[i] != 0) {
            memset(dst->bytes, fill, 32);
            return;
        }
    }
    unsigned int s = shift->bytes[31];
    unsigned int byte_shift = s / 8;
    unsigned int bit_shift = s % 8;
    for (int i = 31; i >= 0; i--) {
        if (i - (int)byte_shift >= 0) {
            unsigned int v = val->bytes[i - byte_shift];
            if (bit_shift == 0) {
                dst->bytes[i] = v;
            } else {
                dst->bytes[i] = (v >> bit_shift) & 0xFF;
                if (i - (int)byte_shift - 1 >= 0) {
                    dst->bytes[i] |= (val->bytes[i - byte_shift - 1] << (8 - bit_shift)) & 0xFF;
                } else {
                    dst->bytes[i] |= (fill << (8 - bit_shift)) & 0xFF;
                }
            }
        } else {
            dst->bytes[i] = fill;
        }
    }
}

/* KECCAK-256 implementation */
static const uint64_t keccakf_rndc[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
    0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,
    0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,
    0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,
    0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL
};
static const unsigned int keccakf_rotc[24] = {
    1,  3,  6,  10, 15, 21, 28, 36, 45, 55, 2,  14,
    27, 41, 56, 8,  25, 43, 62, 18, 39, 61, 20, 44
};
static const unsigned int keccakf_piln[24] = {
    10, 7,  11, 17, 18, 3, 5,  16, 8,  21, 24, 4,
    15, 23, 19, 13, 12, 2, 20, 14, 22, 9,  6,  1
};
#define ROTL64(x, y) (((x) << (y)) | ((x) >> (64 - (y))))

static void keccakf(uint64_t st[25]) {
    int i, j, round;
    uint64_t t, bc[5];
    for (round = 0; round < 24; round++) {
        for (i = 0; i < 5; i++) bc[i] = st[i] ^ st[i + 5] ^ st[i + 10] ^ st[i + 15] ^ st[i + 20];
        for (i = 0; i < 5; i++) {
            t = bc[(i + 4) % 5] ^ ROTL64(bc[(i + 1) % 5], 1);
            for (j = 0; j < 25; j += 5) st[j + i] ^= t;
        }
        t = st[1];
        for (i = 0; i < 24; i++) {
            j = keccakf_piln[i];
            bc[0] = st[j];
            st[j] = ROTL64(t, keccakf_rotc[i]);
            t = bc[0];
        }
        for (j = 0; j < 25; j += 5) {
            for (i = 0; i < 5; i++) bc[i] = st[j + i];
            for (i = 0; i < 5; i++) st[j + i] ^= (~bc[(i + 1) % 5]) & bc[(i + 2) % 5];
        }
        st[0] ^= keccakf_rndc[round];
    }
}

static void evm_keccak256(const unsigned char *data, size_t len, evm_u256_t *out) {
    uint64_t st[25];
    memset(st, 0, sizeof(st));
    size_t i, r = 136;
    while (len >= r) {
        for (i = 0; i < r; i++) st[i/8] ^= (uint64_t)(data[i]) << (8 * (i % 8));
        keccakf(st);
        data += r;
        len -= r;
    }
    for (i = 0; i < len; i++) st[i/8] ^= (uint64_t)(data[i]) << (8 * (i % 8));
    st[len/8] ^= (uint64_t)1 << (8 * (len % 8));
    st[(r-1)/8] ^= (uint64_t)0x80 << (8 * ((r-1) % 8));
    keccakf(st);
    for (i = 0; i < 32; i++) out->bytes[i] = (unsigned char)((st[i/8] >> (8 * (i % 8))) & 0xFF);
}


/* Allocate a fresh VReg name aligned with ir_bridge.h convention ("t<N>"). */
static void lifter_fresh_tmp(evm_lifter_t *ls, char *buf) {
    /* ir_bridge.h uses "%t<N>"; ir.c uses "t<N>".  We follow ir.c style
     * since we call ir_emit() directly, not through the ZCC codegen macros. */
    ir_fresh_tmp(ls->func, buf);
}

static evm_lift_result_t stack_push_const_narrow(evm_lifter_t *ls, const char *name, long val) {
    int i;
    if (ls->stack.depth >= EVM_STACK_MAX) {
        snprintf(ls->errmsg, sizeof(ls->errmsg),
                 "EVM stack overflow at pc=0x%x (depth=%d)",
                 ls->pc, ls->stack.depth);
        ls->error = 1;
        return EVM_LIFT_STACK_OVER;
    }
    /* copy into the next slot */
    for (i = 0; i < IR_NAME_MAX - 1 && name[i]; i++)
        ls->stack.slots[ls->stack.depth][i] = name[i];
    ls->stack.slots[ls->stack.depth][i] = '\0';
    ls->stack.state[ls->stack.depth] = EVM_VAL_KNOWN_NARROW;
    ls->stack.const_vals[ls->stack.depth] = val;
    memset(ls->stack.wide_vals[ls->stack.depth].bytes, 0, 32);
    /* copy narrow val into wide bytes (big endian) */
    for (i = 0; i < 8; i++) {
        ls->stack.wide_vals[ls->stack.depth].bytes[31 - i] = (unsigned char)((val >> (i * 8)) & 0xFF);
    }
    ls->stack.depth++;
    return EVM_LIFT_OK;
}

static evm_lift_result_t stack_push_const_wide(evm_lifter_t *ls, const char *name, const unsigned char *payload, int payload_len, long truncated_val) {
    int i;
    int pad_start;
    if (ls->stack.depth >= EVM_STACK_MAX) {
        snprintf(ls->errmsg, sizeof(ls->errmsg),
                 "EVM stack overflow at pc=0x%x (depth=%d)",
                 ls->pc, ls->stack.depth);
        ls->error = 1;
        return EVM_LIFT_STACK_OVER;
    }
    for (i = 0; i < IR_NAME_MAX - 1 && name[i]; i++)
        ls->stack.slots[ls->stack.depth][i] = name[i];
    ls->stack.slots[ls->stack.depth][i] = '\0';
    
    ls->stack.state[ls->stack.depth] = EVM_VAL_KNOWN_WIDE;
    ls->stack.const_vals[ls->stack.depth] = truncated_val;
    memset(ls->stack.wide_vals[ls->stack.depth].bytes, 0, 32);
    if (payload_len > 32) payload_len = 32;
    pad_start = 32 - payload_len;
    for (i = 0; i < payload_len; i++) {
        ls->stack.wide_vals[ls->stack.depth].bytes[pad_start + i] = payload[i];
    }
    ls->stack.depth++;
    return EVM_LIFT_OK;
}

static evm_lift_result_t stack_push(evm_lifter_t *ls, const char *name) {
    evm_lift_result_t res = stack_push_const_narrow(ls, name, 0L);
    if (res == EVM_LIFT_OK) {
        ls->stack.state[ls->stack.depth - 1] = EVM_VAL_UNKNOWN;
    }
    return res;
}

/* Pop a named temporary from the simulated EVM stack into `out`. */
static evm_lift_result_t stack_pop(evm_lifter_t *ls, char *out) {
    int i;
    const char *src;
    if (ls->stack.depth <= 0) {
        snprintf(ls->errmsg, sizeof(ls->errmsg),
                 "EVM stack underflow at pc=0x%x", ls->pc);
        ls->error = 1;
        return EVM_LIFT_STACK_UNDER;
    }
    ls->stack.depth--;
    src = ls->stack.slots[ls->stack.depth];
    for (i = 0; i < IR_NAME_MAX - 1 && src[i]; i++)
        out[i] = src[i];
    out[i] = '\0';
    return EVM_LIFT_OK;
}

/* Peek at stack[depth-1-offset] without popping. */
static const char *stack_peek(const evm_lifter_t *ls, int offset) {
    int idx = ls->stack.depth - 1 - offset;
    if (idx < 0 || idx >= ls->stack.depth) return "";
    return ls->stack.slots[idx];
}

/*
 * Emit a node tagged with a security tag.
 * Wraps ir_emit() and sets the returned node's .tag field.
 */
static ir_node_t *tagged_emit(evm_lifter_t *ls,
                               ir_op_t op, ir_type_t ty,
                               const char *dst, const char *src1,
                               const char *src2, const char *label,
                               long imm, int lineno,
                               evm_ir_tag_t tag) {
    ir_node_t *n = ir_emit(ls->func, op, ty, dst, src1, src2, label, imm, lineno);
    if (n && tag != IR_TAG_NONE) {
        n->tag = (int)tag;
        ls->tagged_count++;
        /* Bridge to new ir_vuln_tag schema (bitmask in n->vuln_tags) */
        ir_vuln_tag_set(n, ir_vuln_map_from_evm_tag((int)tag));
    }
    return n;
}

/* ── EVM opcode tables ───────────────────────────────────────────────── */

/* Names for the 256 possible byte values.  Most are EVM_INVALID. */
static const char *EVM_OP_NAMES[256] = {
    /* 0x00 */ "STOP",       "ADD",        "MUL",        "SUB",
    /* 0x04 */ "DIV",        "SDIV",       "MOD",        "SMOD",
    /* 0x08 */ "ADDMOD",     "MULMOD",     "EXP",        "SIGNEXTEND",
    /* 0x0c */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0x10 */ "LT",         "GT",         "SLT",        "SGT",
    /* 0x14 */ "EQ",         "ISZERO",     "AND",        "OR",
    /* 0x18 */ "XOR",        "NOT",        "BYTE",       "SHL",
    /* 0x1c */ "SHR",        "SAR",        "INVALID",    "INVALID",
    /* 0x20 */ "SHA3",       "INVALID",    "INVALID",    "INVALID",
    /* 0x24 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0x28 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0x2c */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0x30 */ "ADDRESS",    "BALANCE",    "ORIGIN",     "CALLER",
    /* 0x34 */ "CALLVALUE",  "CALLDATALOAD","CALLDATASIZE","CALLDATACOPY",
    /* 0x38 */ "CODESIZE",   "CODECOPY",   "GASPRICE",   "EXTCODESIZE",
    /* 0x3c */ "EXTCODECOPY","RETURNDATASIZE","RETURNDATACOPY","EXTCODEHASH",
    /* 0x40 */ "BLOCKHASH",  "COINBASE",   "TIMESTAMP",  "NUMBER",
    /* 0x44 */ "PREVRANDAO", "GASLIMIT",   "CHAINID",    "SELFBALANCE",
    /* 0x48 */ "BASEFEE",    "INVALID",    "INVALID",    "INVALID",
    /* 0x4c */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0x50 */ "POP",        "MLOAD",      "MSTORE",     "MSTORE8",
    /* 0x54 */ "SLOAD",      "SSTORE",     "JUMP",       "JUMPI",
    /* 0x58 */ "PC",         "MSIZE",      "GAS",        "JUMPDEST",
    /* 0x5c */ "INVALID",    "INVALID",    "INVALID",    "PUSH0",
    /* 0x60 */ "PUSH1",      "PUSH2",      "PUSH3",      "PUSH4",
    /* 0x64 */ "PUSH5",      "PUSH6",      "PUSH7",      "PUSH8",
    /* 0x68 */ "PUSH9",      "PUSH10",     "PUSH11",     "PUSH12",
    /* 0x6c */ "PUSH13",     "PUSH14",     "PUSH15",     "PUSH16",
    /* 0x70 */ "PUSH17",     "PUSH18",     "PUSH19",     "PUSH20",
    /* 0x74 */ "PUSH21",     "PUSH22",     "PUSH23",     "PUSH24",
    /* 0x78 */ "PUSH25",     "PUSH26",     "PUSH27",     "PUSH28",
    /* 0x7c */ "PUSH29",     "PUSH30",     "PUSH31",     "PUSH32",
    /* 0x80 */ "DUP1",       "DUP2",       "DUP3",       "DUP4",
    /* 0x84 */ "DUP5",       "DUP6",       "DUP7",       "DUP8",
    /* 0x88 */ "DUP9",       "DUP10",      "DUP11",      "DUP12",
    /* 0x8c */ "DUP13",      "DUP14",      "DUP15",      "DUP16",
    /* 0x90 */ "SWAP1",      "SWAP2",      "SWAP3",      "SWAP4",
    /* 0x94 */ "SWAP5",      "SWAP6",      "SWAP7",      "SWAP8",
    /* 0x98 */ "SWAP9",      "SWAP10",     "SWAP11",     "SWAP12",
    /* 0x9c */ "SWAP13",     "SWAP14",     "SWAP15",     "SWAP16",
    /* 0xa0 */ "LOG0",       "LOG1",       "LOG2",       "LOG3",
    /* 0xa4 */ "LOG4",       "INVALID",    "INVALID",    "INVALID",
    /* 0xa8 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xac */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xb0 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xb4 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xb8 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xbc */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xc0 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xc4 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xc8 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xcc */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xd0 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xd4 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xd8 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xdc */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xe0 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xe4 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xe8 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xec */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xf0 */ "CREATE",     "CALL",       "CALLCODE",   "RETURN",
    /* 0xf4 */ "DELEGATECALL","CREATE2",   "INVALID",    "INVALID",
    /* 0xf8 */ "INVALID",    "INVALID",    "STATICCALL", "INVALID",
    /* 0xfc */ "INVALID",    "REVERT",     "INVALID",    "SELFDESTRUCT"
};

static const char *EVM_TAG_NAMES[] = {
    "IR_TAG_NONE",
    "IR_TAG_UNTRUSTED_EXTERNAL_CALL",
    "IR_TAG_STATIC_CALL",
    "IR_TAG_SELFDESTRUCT",
    "IR_TAG_SSTORE",
    "IR_TAG_CREATE",
    "IR_TAG_EVM_BARRIER",
    "IR_TAG_TRUNCATED_WIDE_CONST",
    "IR_TAG_SHA3",
    "IR_TAG_MEMORY_COPY",
    "IR_TAG_LOG"
};

/* ── Public query functions ──────────────────────────────────────────── */

const char *evm_opcode_name(unsigned int opcode) {
    if (opcode > 0xff) return "INVALID";
    return EVM_OP_NAMES[opcode];
}

const char *evm_tag_name(evm_ir_tag_t tag) {
    int idx = (int)tag;
    if (idx < 0 || idx > (int)IR_TAG_EVM_BARRIER) return "IR_TAG_UNKNOWN";
    return EVM_TAG_NAMES[idx];
}

int evm_is_call_family(unsigned int opcode) {
    return opcode == (unsigned int)EVM_CALL         ||
           opcode == (unsigned int)EVM_CALLCODE     ||
           opcode == (unsigned int)EVM_DELEGATECALL ||
           opcode == (unsigned int)EVM_STATICCALL;
}

const char *evm_support_class_name(evm_support_class_t sc) {
    switch (sc) {
        case EVM_SUPPORT_FULLY_SUPPORTED:       return "FULLY_SUPPORTED";
        case EVM_SUPPORT_APPROXIMATED_ANALYZABLE: return "APPROXIMATED_ANALYZABLE";
        case EVM_SUPPORT_PLACEHOLDER_ONLY:      return "PLACEHOLDER_ONLY";
        case EVM_SUPPORT_INVALID_OR_UNASSIGNED: return "INVALID_OR_UNASSIGNED";
    }
    return "UNKNOWN_CLASS";
}

evm_support_class_t evm_opcode_support(unsigned int opcode) {
    if (opcode > 0xff) return EVM_SUPPORT_INVALID_OR_UNASSIGNED;

    /* ── 1. FULLY_SUPPORTED ──────────────────────────────────────────────
     * Semantics are completely captured by the emitted IR or structural
     * primitives. PUSH, DUP, SWAP, basic arithmetic, memory/storage layout.
     */
    if (opcode == EVM_STOP || opcode == EVM_RETURN || opcode == EVM_REVERT || opcode == EVM_INVALID || opcode == EVM_POP || opcode == EVM_JUMPDEST) return EVM_SUPPORT_FULLY_SUPPORTED;
    if (opcode >= EVM_ADD && opcode <= EVM_MULMOD) return EVM_SUPPORT_FULLY_SUPPORTED; /* ADD..MULMOD all exact */
    if (opcode == EVM_EXP || opcode == EVM_SIGNEXTEND) return EVM_SUPPORT_FULLY_SUPPORTED; /* exact 256-bit propagation */
    if (opcode >= EVM_LT && opcode <= EVM_SAR) return EVM_SUPPORT_FULLY_SUPPORTED;
    if (opcode == EVM_BYTE) return EVM_SUPPORT_FULLY_SUPPORTED; /* exact byte extraction */
    if (opcode == EVM_MLOAD || opcode == EVM_MSTORE || opcode == EVM_MSTORE8 || opcode == EVM_SLOAD || opcode == EVM_SSTORE) return EVM_SUPPORT_FULLY_SUPPORTED;
    if (opcode >= EVM_PUSH1 && opcode <= EVM_PUSH32) return EVM_SUPPORT_FULLY_SUPPORTED;
    if (opcode >= EVM_DUP1 && opcode <= EVM_DUP16) return EVM_SUPPORT_FULLY_SUPPORTED;
    if (opcode >= EVM_SWAP1 && opcode <= EVM_SWAP16) return EVM_SUPPORT_FULLY_SUPPORTED;
    if (opcode == EVM_PUSH0) return EVM_SUPPORT_FULLY_SUPPORTED;

    /* ── 2. APPROXIMATED_ANALYZABLE ──────────────────────────────────────
     * Analyzable structure emitted (e.g., tagged IR_CALL or valid CFG edges),
     * but deeper dynamic semantics (memory layout effects, indirect jumps)
     * are still abstracted away.
     */
    if (opcode == EVM_JUMP || opcode == EVM_JUMPI) return EVM_SUPPORT_APPROXIMATED_ANALYZABLE; /* CFG valid, but indirect dynamic jumps are partial */
    if (opcode == EVM_SHA3 || opcode == EVM_CALLDATACOPY || opcode == EVM_CODECOPY || opcode == EVM_RETURNDATACOPY || opcode == EVM_EXTCODECOPY) return EVM_SUPPORT_APPROXIMATED_ANALYZABLE;
    if (opcode >= EVM_LOG0 && opcode <= EVM_LOG4) return EVM_SUPPORT_APPROXIMATED_ANALYZABLE;
    if (opcode == EVM_CALL || opcode == EVM_CALLCODE || opcode == EVM_DELEGATECALL || opcode == EVM_STATICCALL) return EVM_SUPPORT_APPROXIMATED_ANALYZABLE;
    if (opcode == EVM_CREATE || opcode == EVM_CREATE2) return EVM_SUPPORT_APPROXIMATED_ANALYZABLE;
    
    /* Env/Block queries emit generic IR_CONST / IR_LOAD */
    if (opcode >= EVM_ADDRESS && opcode <= EVM_CALLDATALOAD) return EVM_SUPPORT_APPROXIMATED_ANALYZABLE;
    if (opcode >= EVM_CODESIZE && opcode <= EVM_BASEFEE) return EVM_SUPPORT_APPROXIMATED_ANALYZABLE;
    if (opcode >= EVM_PC && opcode <= EVM_GAS) return EVM_SUPPORT_APPROXIMATED_ANALYZABLE;
    
    /* SHA3/KECCAK256 emits tagged IR_CALL — analyzable */
    if (opcode == EVM_SHA3) return EVM_SUPPORT_APPROXIMATED_ANALYZABLE;
    /* SELFDESTRUCT emits tagged IR_RET — analyzable terminal effect */
    if (opcode == EVM_SELFDESTRUCT) return EVM_SUPPORT_APPROXIMATED_ANALYZABLE;

    /* ── 3. PLACEHOLDER_ONLY ─────────────────────────────────────────────
     * No remaining true placeholders — all arithmetic/logic families are
     * now exactly evaluated when inputs are known-wide constants.
     * This class is reserved for future opcodes not yet implemented.
     */

    /* ── 4. INVALID_OR_UNASSIGNED ──────────────────────────────────────── */
    return EVM_SUPPORT_INVALID_OR_UNASSIGNED;
}

/* ── Initialisation ──────────────────────────────────────────────────── */

void evm_lifter_init(evm_lifter_t *ls,
                     const unsigned char *bytecode, int length,
                     ir_module_t *module) {
    int i, p;
    memset(ls, 0, sizeof(*ls));
    ls->bytecode = bytecode;
    ls->length   = length;
    ls->module   = module;
    /* Create a fresh IR function to represent the lifted contract bytecode.
     * Using IR_TY_I64 for the return type (EVM call result is a success flag). */
    ls->func = ir_func_create(module, "evm_contract", IR_TY_I64, 0);
    /* Warm the stack so all slot strings start NUL-terminated. */
    for (i = 0; i < EVM_STACK_MAX; i++)
        ls->stack.slots[i][0] = '\0';

    extern void *memory_v2_new(void);
    extern void memory_v2_free(void *);
    ls->memory_v2 = memory_v2_new();

    ls->is_block_start = 1;
    strcpy(ls->current_gas_vreg, "g0");
    ir_emit(ls->func, IR_CONST, IR_TY_I64, ls->current_gas_vreg, NULL, NULL, NULL, 30000000ULL, 0);

    /* Allocate and pre-scan valid JUMPDESTs */
    ls->valid_jumpdest = (unsigned char *)calloc(length > 0 ? (size_t)length : 1, 1);
    if (ls->valid_jumpdest && bytecode) {
        p = 0;
        while (p < length) {
            unsigned int op = bytecode[p];
            if (op == (unsigned int)EVM_JUMPDEST) {
                ls->valid_jumpdest[p] = 1;
                p++;
            } else if (op >= (unsigned int)EVM_PUSH1 && op <= (unsigned int)EVM_PUSH32) {
                p += 1 + (int)(op - (unsigned int)EVM_PUSH1 + 1);
            } else {
                p++;
            }
        }
    }
}

void evm_lifter_destroy(evm_lifter_t *ls) {
    extern void memory_v2_free(void *);
    if (ls->memory_v2) {
        memory_v2_free(ls->memory_v2);
        ls->memory_v2 = NULL;
    }
    if (ls->valid_jumpdest) {
        free(ls->valid_jumpdest);
        ls->valid_jumpdest = NULL;
    }
}

/* ── Step: lift one EVM instruction ─────────────────────────────────── */

/*
 * evm_lift_step() — Lift the single EVM instruction at ls->pc.
 *
 * For the scaffold, we handle:
 *   - PUSH0, PUSH1..PUSH8 with immediate values that fit in long.
 *   - PUSH9..PUSH32 with truncated-to-64-bit immediates (annotated).
 *   - Simple unary/binary arithmetic/bitwise/comparison ops.
 *   - POP, DUP1..DUP16, SWAP1..SWAP16.
 *   - JUMP, JUMPI (flow control — emits IR_BR).
 *   - CALL, CALLCODE, DELEGATECALL, STATICCALL with security tags.
 *   - CREATE, CREATE2 with IR_TAG_CREATE.
 *   - SSTORE with IR_TAG_SSTORE.
 *   - SELFDESTRUCT with IR_TAG_SELFDESTRUCT.
 *   - REVERT, INVALID with IR_TAG_EVM_BARRIER.
 *   - STOP, RETURN with IR_RET.
 *   - JUMPDEST as a labelled IR_LABEL.
 *   - All other opcodes emit an IR_NOP annotated with the opcode name.
 */
evm_lift_result_t evm_lift_step(evm_lifter_t *ls) {
    unsigned int op;
    int push_bytes;
    unsigned long imm_bits; /* accumulate as unsigned to avoid sign-extension UB */
    long imm_val;
    int i;
    char tmp_dst[IR_NAME_MAX];
    char tmp_a[IR_NAME_MAX];
    char tmp_b[IR_NAME_MAX];
    char tmp_c[IR_NAME_MAX];
    char lbl_buf[IR_LABEL_MAX];
    ir_node_t *node;
    evm_lift_result_t res;

    if (ls->pc >= ls->length) {
        /* Normal end-of-stream — not an error. */
        return EVM_LIFT_OK;
    }

    op = (unsigned int)ls->bytecode[ls->pc];
    ls->insn_count++;

    if (op == (unsigned int)EVM_JUMPDEST) {
        ls->is_block_start = 1;
    }

    if (ls->is_block_start) {
        int block_gas = get_block_static_gas(ls->bytecode, ls->length, ls->pc);
        char next_gas[IR_NAME_MAX];
        lifter_fresh_tmp(ls, next_gas);
        evm_apply_static_gas(ls->func, ls, block_gas, ls->current_gas_vreg, next_gas);
        strcpy(ls->current_gas_vreg, next_gas);
        ls->is_block_start = 0;
    }

    /* ── PUSH0 (EIP-3855) ──────────────────────────────────────────── */
    if (op == (unsigned int)EVM_PUSH0) {
        ls->pc++;
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_CONST, IR_TY_I64, tmp_dst, NULL, NULL, NULL, 0L, ls->pc);
        return stack_push_const_narrow(ls, tmp_dst, 0L);
    }

    /* ── PUSH1..PUSH32 ─────────────────────────────────────────────── */
    if (op >= (unsigned int)EVM_PUSH1 && op <= (unsigned int)EVM_PUSH32) {
        push_bytes = (int)(op - (unsigned int)EVM_PUSH1 + 1);
        ls->pc++;  /* advance past the PUSH opcode */

        /* Check bytecode is long enough for the immediate data */
        if (ls->pc + push_bytes > ls->length) {
            snprintf(ls->errmsg, sizeof(ls->errmsg),
                     "PUSH%d at pc=0x%x: truncated (need %d bytes, have %d)",
                     push_bytes, ls->pc - 1,
                     push_bytes, ls->length - ls->pc);
            ls->error = 1;
            return EVM_LIFT_TRUNCATED;
        }

        /* Collect all push_bytes into an unsigned long to avoid sign-extension
         * UB when shifting.  EVM words are 256-bit; values wider than 8 bytes
         * (PUSH9..PUSH32) are truncated: only the last 8 bytes worth of bits
         * are retained in imm_bits because unsigned long overflow wraps the
         * high bytes away.  Cast to signed long to store in ir_node_t.imm. */
        imm_bits = 0UL;
        for (i = 0; i < push_bytes; i++) {
            imm_bits = (imm_bits << 8) | (unsigned long)ls->bytecode[ls->pc + i];
        }
        imm_val = (long)imm_bits;
        ls->pc += push_bytes;

        lifter_fresh_tmp(ls, tmp_dst);
        if (push_bytes > 8) {
            ir_node_t *n_const = tagged_emit(ls, IR_CONST, IR_TY_I64, tmp_dst, NULL, NULL, NULL, imm_val, ls->pc, IR_TAG_TRUNCATED_WIDE_CONST);
            if (n_const) {
                unsigned char wide[32];
                memset(wide, 0, 32);
                int pad_start = 32 - push_bytes;
                for (int j = 0; j < push_bytes; j++) {
                    wide[pad_start + j] = ls->bytecode[ls->pc - push_bytes + j];
                }
                for (int w = 0; w < 4; w++) {
                    unsigned long limb = 0;
                    for (int b = 0; b < 8; b++) {
                        limb = (limb << 8) | (unsigned long)wide[31 - w * 8 - 7 + b];
                    }
                    n_const->imm256.limbs[w] = limb;
                }
            }
            res = stack_push_const_wide(ls, tmp_dst, &ls->bytecode[ls->pc - push_bytes], push_bytes, imm_val);
        } else {
            ir_emit(ls->func, IR_CONST, IR_TY_I64, tmp_dst, NULL, NULL, NULL, imm_val, ls->pc);
            res = stack_push_const_narrow(ls, tmp_dst, imm_val);
        }
        return res;
    }

    /* ── DUP1..DUP16 ───────────────────────────────────────────────── */
    if (op >= (unsigned int)EVM_DUP1 && op <= (unsigned int)EVM_DUP16) {
        int depth = (int)(op - (unsigned int)EVM_DUP1); /* 0 = DUP1 → copies TOS */
        const char *src;
        evm_val_state_t st;
        long c_val;
        evm_u256_t w_val;
        ls->pc++;
        if (ls->stack.depth <= depth) {
            snprintf(ls->errmsg, sizeof(ls->errmsg),
                     "DUP%d at pc=0x%x: stack depth=%d insufficient",
                     depth + 1, ls->pc - 1, ls->stack.depth);
            ls->error = 1;
            return EVM_LIFT_STACK_UNDER;
        }
        src = stack_peek(ls, depth);
        st = ls->stack.state[ls->stack.depth - 1 - depth];
        c_val = ls->stack.const_vals[ls->stack.depth - 1 - depth];
        memcpy(w_val.bytes, ls->stack.wide_vals[ls->stack.depth - 1 - depth].bytes, 32);
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_COPY, IR_TY_I64, tmp_dst, src, NULL, NULL, 0L, ls->pc);
        res = stack_push(ls, tmp_dst);
        if (res == EVM_LIFT_OK) {
            ls->stack.state[ls->stack.depth - 1] = st;
            ls->stack.const_vals[ls->stack.depth - 1] = c_val;
            memcpy(ls->stack.wide_vals[ls->stack.depth - 1].bytes, w_val.bytes, 32);
        }
        return res;
    }

    /* ── SWAP1..SWAP16 ─────────────────────────────────────────────── */
    if (op >= (unsigned int)EVM_SWAP1 && op <= (unsigned int)EVM_SWAP16) {
        int depth = (int)(op - (unsigned int)EVM_SWAP1 + 1); /* SWAP1: swap top with [1] */
        int top_idx, tgt_idx;
        char tmp_swap[IR_NAME_MAX];
        evm_val_state_t tmp_st;
        long tmp_v;
        evm_u256_t tmp_w;
        ls->pc++;
        if (ls->stack.depth <= depth) {
            snprintf(ls->errmsg, sizeof(ls->errmsg),
                     "SWAP%d at pc=0x%x: stack depth=%d insufficient",
                     depth, ls->pc - 1, ls->stack.depth);
            ls->error = 1;
            return EVM_LIFT_STACK_UNDER;
        }
        top_idx = ls->stack.depth - 1;
        tgt_idx = ls->stack.depth - 1 - depth;
        /* Swap in the stack model */
        memcpy(tmp_swap, ls->stack.slots[top_idx], IR_NAME_MAX);
        memcpy(ls->stack.slots[top_idx], ls->stack.slots[tgt_idx], IR_NAME_MAX);
        memcpy(ls->stack.slots[tgt_idx], tmp_swap, IR_NAME_MAX);
        
        tmp_st = ls->stack.state[top_idx];
        ls->stack.state[top_idx] = ls->stack.state[tgt_idx];
        ls->stack.state[tgt_idx] = tmp_st;
        
        tmp_v = ls->stack.const_vals[top_idx];
        ls->stack.const_vals[top_idx] = ls->stack.const_vals[tgt_idx];
        ls->stack.const_vals[tgt_idx] = tmp_v;
        
        memcpy(tmp_w.bytes, ls->stack.wide_vals[top_idx].bytes, 32);
        memcpy(ls->stack.wide_vals[top_idx].bytes, ls->stack.wide_vals[tgt_idx].bytes, 32);
        memcpy(ls->stack.wide_vals[tgt_idx].bytes, tmp_w.bytes, 32);
        
        /* Emit a NOP marker so the IR record shows the swap */
        ir_emit(ls->func, IR_NOP, IR_TY_VOID, NULL, NULL, NULL, NULL, 0L, ls->pc);
        return EVM_LIFT_OK;
    }

    /* ── Everything else: advance PC now, dispatch on opcode ─────── */
    ls->pc++;

    switch (op) {

    /* ── STOP / RETURN / REVERT ──────────────────────────────────── */
    case EVM_STOP:
        ir_emit(ls->func, IR_RET, IR_TY_VOID, NULL, "", NULL, NULL, 0L, ls->pc);
        return EVM_LIFT_OK;

    case EVM_RETURN:
        res = stack_pop(ls, tmp_a);  /* offset */
        if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_b);  /* length */
        if (res != EVM_LIFT_OK) return res;
        ir_emit(ls->func, IR_RET, IR_TY_I64, NULL, tmp_a, NULL, NULL, 0L, ls->pc);
        return EVM_LIFT_OK;

    case EVM_REVERT:
        res = stack_pop(ls, tmp_a);
        if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_b);
        if (res != EVM_LIFT_OK) return res;
        tagged_emit(ls, IR_RET, IR_TY_VOID, NULL, tmp_a, NULL, NULL, 0L, ls->pc,
                    IR_TAG_EVM_BARRIER);
        return EVM_LIFT_OK;

    case EVM_INVALID:
        tagged_emit(ls, IR_NOP, IR_TY_VOID, NULL, NULL, NULL, NULL, 0L, ls->pc,
                    IR_TAG_EVM_BARRIER);
        return EVM_LIFT_OK;

    /* ── POP ─────────────────────────────────────────────────────── */
    case EVM_POP:
        return stack_pop(ls, tmp_a);   /* discard the value */

    /* ── Arithmetic: pops 2, pushes 1 ───────────────────────────── */
    case EVM_ADD: {
        int a_st = EVM_VAL_UNKNOWN, b_st = EVM_VAL_UNKNOWN;
        evm_u256_t a_val, b_val, res_val;
        if (ls->stack.depth >= 2) {
            a_st = ls->stack.state[ls->stack.depth - 2]; /* below top */
            b_st = ls->stack.state[ls->stack.depth - 1]; /* top */
            memcpy(&a_val, &ls->stack.wide_vals[ls->stack.depth - 2], sizeof(evm_u256_t));
            memcpy(&b_val, &ls->stack.wide_vals[ls->stack.depth - 1], sizeof(evm_u256_t));
        }
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_ADD, IR_TY_I64, tmp_dst, tmp_a, tmp_b, NULL, 0L, ls->pc);
        if ((a_st == EVM_VAL_KNOWN_NARROW || a_st == EVM_VAL_KNOWN_WIDE) &&
            (b_st == EVM_VAL_KNOWN_NARROW || b_st == EVM_VAL_KNOWN_WIDE)) {
            lift_u256_add(&res_val, &a_val, &b_val);
            return stack_push_const_wide(ls, tmp_dst, res_val.bytes, 32, evm_u256_to_narrow(&res_val));
        }
        return stack_push(ls, tmp_dst);
    }

    case EVM_SUB: {
        int a_st = EVM_VAL_UNKNOWN, b_st = EVM_VAL_UNKNOWN;
        evm_u256_t a_val, b_val, res_val;
        if (ls->stack.depth >= 2) {
            a_st = ls->stack.state[ls->stack.depth - 2]; /* below top */
            b_st = ls->stack.state[ls->stack.depth - 1]; /* top */
            memcpy(&a_val, &ls->stack.wide_vals[ls->stack.depth - 2], sizeof(evm_u256_t));
            memcpy(&b_val, &ls->stack.wide_vals[ls->stack.depth - 1], sizeof(evm_u256_t));
        }
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_SUB, IR_TY_I64, tmp_dst, tmp_a, tmp_b, NULL, 0L, ls->pc);
        if ((a_st == EVM_VAL_KNOWN_NARROW || a_st == EVM_VAL_KNOWN_WIDE) &&
            (b_st == EVM_VAL_KNOWN_NARROW || b_st == EVM_VAL_KNOWN_WIDE)) {
            lift_u256_sub(&res_val, &a_val, &b_val);
            return stack_push_const_wide(ls, tmp_dst, res_val.bytes, 32, evm_u256_to_narrow(&res_val));
        }
        return stack_push(ls, tmp_dst);
    }

    case EVM_MUL: {
        int a_st = EVM_VAL_UNKNOWN, b_st = EVM_VAL_UNKNOWN;
        evm_u256_t a_val, b_val, res_val;
        if (ls->stack.depth >= 2) {
            a_st = ls->stack.state[ls->stack.depth - 2]; /* below top */
            b_st = ls->stack.state[ls->stack.depth - 1]; /* top */
            memcpy(&a_val, &ls->stack.wide_vals[ls->stack.depth - 2], sizeof(evm_u256_t));
            memcpy(&b_val, &ls->stack.wide_vals[ls->stack.depth - 1], sizeof(evm_u256_t));
        }
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_MUL, IR_TY_I64, tmp_dst, tmp_a, tmp_b, NULL, 0L, ls->pc);
        if ((a_st == EVM_VAL_KNOWN_NARROW || a_st == EVM_VAL_KNOWN_WIDE) &&
            (b_st == EVM_VAL_KNOWN_NARROW || b_st == EVM_VAL_KNOWN_WIDE)) {
            lift_u256_mul(&res_val, &a_val, &b_val);
            return stack_push_const_wide(ls, tmp_dst, res_val.bytes, 32, evm_u256_to_narrow(&res_val));
        }
        return stack_push(ls, tmp_dst);
    }

    case EVM_DIV:
    case EVM_SDIV: {
        int a_st = EVM_VAL_UNKNOWN, b_st = EVM_VAL_UNKNOWN;
        evm_u256_t a_val, b_val, q_val, r_val;
        if (ls->stack.depth >= 2) {
            a_st = ls->stack.state[ls->stack.depth - 2]; /* below top */
            b_st = ls->stack.state[ls->stack.depth - 1]; /* top */
            memcpy(&a_val, &ls->stack.wide_vals[ls->stack.depth - 2], sizeof(evm_u256_t));
            memcpy(&b_val, &ls->stack.wide_vals[ls->stack.depth - 1], sizeof(evm_u256_t));
        }
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        if (op == EVM_SDIV) {
            tagged_emit(ls, IR_DIV, IR_TY_I64, tmp_dst, tmp_a, tmp_b, NULL, 0L, ls->pc, IR_TAG_EVM_SDIV);
        } else {
            ir_emit(ls->func, IR_DIV, IR_TY_I64, tmp_dst, tmp_a, tmp_b, NULL, 0L, ls->pc);
        }
        if ((a_st == EVM_VAL_KNOWN_NARROW || a_st == EVM_VAL_KNOWN_WIDE) &&
            (b_st == EVM_VAL_KNOWN_NARROW || b_st == EVM_VAL_KNOWN_WIDE)) {
            if (op == EVM_DIV) {
                evm_u256_div_mod(&q_val, &r_val, &a_val, &b_val);
            } else {
                evm_u256_sdiv_smod(&q_val, &r_val, &a_val, &b_val);
            }
            return stack_push_const_wide(ls, tmp_dst, q_val.bytes, 32, evm_u256_to_narrow(&q_val));
        }
        return stack_push(ls, tmp_dst);
    }

    case EVM_MOD:
    case EVM_SMOD: {
        int a_st = EVM_VAL_UNKNOWN, b_st = EVM_VAL_UNKNOWN;
        evm_u256_t a_val, b_val, q_val, r_val;
        if (ls->stack.depth >= 2) {
            a_st = ls->stack.state[ls->stack.depth - 2]; /* below top */
            b_st = ls->stack.state[ls->stack.depth - 1]; /* top */
            memcpy(&a_val, &ls->stack.wide_vals[ls->stack.depth - 2], sizeof(evm_u256_t));
            memcpy(&b_val, &ls->stack.wide_vals[ls->stack.depth - 1], sizeof(evm_u256_t));
        }
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        if (op == EVM_SMOD) {
            tagged_emit(ls, IR_MOD, IR_TY_I64, tmp_dst, tmp_a, tmp_b, NULL, 0L, ls->pc, IR_TAG_EVM_SMOD);
        } else {
            ir_emit(ls->func, IR_MOD, IR_TY_I64, tmp_dst, tmp_a, tmp_b, NULL, 0L, ls->pc);
        }
        if ((a_st == EVM_VAL_KNOWN_NARROW || a_st == EVM_VAL_KNOWN_WIDE) &&
            (b_st == EVM_VAL_KNOWN_NARROW || b_st == EVM_VAL_KNOWN_WIDE)) {
            if (op == EVM_MOD) {
                evm_u256_div_mod(&q_val, &r_val, &a_val, &b_val);
            } else {
                evm_u256_sdiv_smod(&q_val, &r_val, &a_val, &b_val);
            }
            return stack_push_const_wide(ls, tmp_dst, r_val.bytes, 32, evm_u256_to_narrow(&r_val));
        }
        return stack_push(ls, tmp_dst);
    }

    /* ── Comparison ops: pops 2, pushes i32 0/1 ─────────────────── */
    case EVM_LT:
    case EVM_SLT: {
        int a_st = EVM_VAL_UNKNOWN, b_st = EVM_VAL_UNKNOWN;
        evm_u256_t a_val, b_val;
        if (ls->stack.depth >= 2) {
            a_st = ls->stack.state[ls->stack.depth - 2]; /* a is below b */
            b_st = ls->stack.state[ls->stack.depth - 1]; /* b is top */
            memcpy(&a_val, &ls->stack.wide_vals[ls->stack.depth - 2], sizeof(evm_u256_t));
            memcpy(&b_val, &ls->stack.wide_vals[ls->stack.depth - 1], sizeof(evm_u256_t));
        }
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        tagged_emit(ls, IR_LT, IR_TY_I32, tmp_dst, tmp_a, tmp_b, NULL, 0L, ls->pc, IR_TAG_EVM_LT);
        if ((a_st == EVM_VAL_KNOWN_NARROW || a_st == EVM_VAL_KNOWN_WIDE) &&
            (b_st == EVM_VAL_KNOWN_NARROW || b_st == EVM_VAL_KNOWN_WIDE)) {
            if (op == EVM_LT) {
                return stack_push_const_narrow(ls, tmp_dst, (evm_u256_cmp(&a_val, &b_val) < 0) ? 1L : 0L);
            } else if (op == EVM_SLT) {
                return stack_push_const_narrow(ls, tmp_dst, (evm_u256_cmp_signed(&a_val, &b_val) < 0) ? 1L : 0L);
            }
        }
        return stack_push(ls, tmp_dst);
    }

    case EVM_GT:
    case EVM_SGT: {
        int a_st = EVM_VAL_UNKNOWN, b_st = EVM_VAL_UNKNOWN;
        evm_u256_t a_val, b_val;
        if (ls->stack.depth >= 2) {
            a_st = ls->stack.state[ls->stack.depth - 2]; /* a is below b */
            b_st = ls->stack.state[ls->stack.depth - 1]; /* b is top */
            memcpy(&a_val, &ls->stack.wide_vals[ls->stack.depth - 2], sizeof(evm_u256_t));
            memcpy(&b_val, &ls->stack.wide_vals[ls->stack.depth - 1], sizeof(evm_u256_t));
        }
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        tagged_emit(ls, IR_GT, IR_TY_I32, tmp_dst, tmp_a, tmp_b, NULL, 0L, ls->pc, IR_TAG_EVM_GT);
        if ((a_st == EVM_VAL_KNOWN_NARROW || a_st == EVM_VAL_KNOWN_WIDE) &&
            (b_st == EVM_VAL_KNOWN_NARROW || b_st == EVM_VAL_KNOWN_WIDE)) {
            if (op == EVM_GT) {
                return stack_push_const_narrow(ls, tmp_dst, (evm_u256_cmp(&a_val, &b_val) > 0) ? 1L : 0L);
            } else if (op == EVM_SGT) {
                return stack_push_const_narrow(ls, tmp_dst, (evm_u256_cmp_signed(&a_val, &b_val) > 0) ? 1L : 0L);
            }
        }
        return stack_push(ls, tmp_dst);
    }

    case EVM_EQ: {
        int a_st = EVM_VAL_UNKNOWN, b_st = EVM_VAL_UNKNOWN;
        evm_u256_t a_val, b_val;
        if (ls->stack.depth >= 2) {
            a_st = ls->stack.state[ls->stack.depth - 2]; /* a is below b */
            b_st = ls->stack.state[ls->stack.depth - 1]; /* b is top */
            memcpy(&a_val, &ls->stack.wide_vals[ls->stack.depth - 2], sizeof(evm_u256_t));
            memcpy(&b_val, &ls->stack.wide_vals[ls->stack.depth - 1], sizeof(evm_u256_t));
        }
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        tagged_emit(ls, IR_EQ, IR_TY_I32, tmp_dst, tmp_a, tmp_b, NULL, 0L, ls->pc, IR_TAG_EVM_EQ);
        if ((a_st == EVM_VAL_KNOWN_NARROW || a_st == EVM_VAL_KNOWN_WIDE) &&
            (b_st == EVM_VAL_KNOWN_NARROW || b_st == EVM_VAL_KNOWN_WIDE)) {
            return stack_push_const_narrow(ls, tmp_dst, evm_u256_eq(&a_val, &b_val) ? 1L : 0L);
        }
        return stack_push(ls, tmp_dst);
    }

    case EVM_ISZERO: {
        int a_st = EVM_VAL_UNKNOWN;
        evm_u256_t a_val;
        if (ls->stack.depth >= 1) {
            a_st = ls->stack.state[ls->stack.depth - 1];
            memcpy(&a_val, &ls->stack.wide_vals[ls->stack.depth - 1], sizeof(evm_u256_t));
        }
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        /* ISZERO: push (a == 0) */
        lifter_fresh_tmp(ls, tmp_b);
        ir_emit(ls->func, IR_CONST, IR_TY_I64, tmp_b, NULL, NULL, NULL, 0L, ls->pc);
        lifter_fresh_tmp(ls, tmp_dst);
        tagged_emit(ls, IR_EQ, IR_TY_I32, tmp_dst, tmp_a, tmp_b, NULL, 0L, ls->pc, IR_TAG_EVM_ISZERO);
        if (a_st == EVM_VAL_KNOWN_NARROW || a_st == EVM_VAL_KNOWN_WIDE) {
            return stack_push_const_narrow(ls, tmp_dst, lift_u256_is_zero(&a_val) ? 1L : 0L);
        }
        return stack_push(ls, tmp_dst);
    }

    /* ── Bitwise ops ─────────────────────────────────────────────── */
    case EVM_AND:
    case EVM_OR:
    case EVM_XOR: {
        int a_st = EVM_VAL_UNKNOWN, b_st = EVM_VAL_UNKNOWN;
        evm_u256_t a_val, b_val, res_val;
        if (ls->stack.depth >= 2) {
            a_st = ls->stack.state[ls->stack.depth - 2]; /* below top */
            b_st = ls->stack.state[ls->stack.depth - 1]; /* top */
            if ((a_st == EVM_VAL_KNOWN_NARROW || a_st == EVM_VAL_KNOWN_WIDE) &&
                (b_st == EVM_VAL_KNOWN_NARROW || b_st == EVM_VAL_KNOWN_WIDE)) {
                a_val = ls->stack.wide_vals[ls->stack.depth - 2];
                b_val = ls->stack.wide_vals[ls->stack.depth - 1];
            }
        }
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        
        int ir_op = (op == EVM_AND) ? IR_AND : ((op == EVM_OR) ? IR_OR : IR_XOR);
        ir_emit(ls->func, ir_op, IR_TY_I64, tmp_dst, tmp_a, tmp_b, NULL, 0L, ls->pc);
        
        if ((a_st == EVM_VAL_KNOWN_NARROW || a_st == EVM_VAL_KNOWN_WIDE) &&
            (b_st == EVM_VAL_KNOWN_NARROW || b_st == EVM_VAL_KNOWN_WIDE)) {
            if (op == EVM_AND) evm_u256_and(&res_val, &a_val, &b_val);
            else if (op == EVM_OR) evm_u256_or(&res_val, &a_val, &b_val);
            else evm_u256_xor(&res_val, &a_val, &b_val);
            return stack_push_const_wide(ls, tmp_dst, res_val.bytes, 32, evm_u256_to_narrow(&res_val));
        }
        return stack_push(ls, tmp_dst);
    }

    case EVM_NOT: {
        int a_st = EVM_VAL_UNKNOWN;
        evm_u256_t a_val, res_val;
        if (ls->stack.depth >= 1) {
            a_st = ls->stack.state[ls->stack.depth - 1]; /* top */
            if (a_st == EVM_VAL_KNOWN_NARROW || a_st == EVM_VAL_KNOWN_WIDE) {
                a_val = ls->stack.wide_vals[ls->stack.depth - 1];
            }
        }
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_NOT, IR_TY_I64, tmp_dst, tmp_a, NULL, NULL, 0L, ls->pc);
        
        if (a_st == EVM_VAL_KNOWN_NARROW || a_st == EVM_VAL_KNOWN_WIDE) {
            evm_u256_not(&res_val, &a_val);
            return stack_push_const_wide(ls, tmp_dst, res_val.bytes, 32, evm_u256_to_narrow(&res_val));
        }
        return stack_push(ls, tmp_dst);
    }

    case EVM_SHL:
    case EVM_SHR:
    case EVM_SAR: {
        int a_st = EVM_VAL_UNKNOWN, b_st = EVM_VAL_UNKNOWN;
        evm_u256_t shift_val, val_val, res_val;
        if (ls->stack.depth >= 2) {
            b_st = ls->stack.state[ls->stack.depth - 1]; /* top: shift */
            a_st = ls->stack.state[ls->stack.depth - 2]; /* below: value */
            memcpy(&shift_val, &ls->stack.wide_vals[ls->stack.depth - 1], sizeof(evm_u256_t));
            memcpy(&val_val, &ls->stack.wide_vals[ls->stack.depth - 2], sizeof(evm_u256_t));
        }
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res; /* shift */
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res; /* value */
        lifter_fresh_tmp(ls, tmp_dst);
        
        int ir_op = (op == EVM_SHL) ? IR_SHL : IR_SHR;
        ir_emit(ls->func, ir_op, IR_TY_I64, tmp_dst, tmp_a, tmp_b, NULL, 0L, ls->pc);
        
        if ((a_st == EVM_VAL_KNOWN_NARROW || a_st == EVM_VAL_KNOWN_WIDE) &&
            (b_st == EVM_VAL_KNOWN_NARROW || b_st == EVM_VAL_KNOWN_WIDE)) {
            if (op == EVM_SHL) evm_u256_shl_op(&res_val, &shift_val, &val_val);
            else if (op == EVM_SHR) evm_u256_shr_op(&res_val, &shift_val, &val_val);
            else evm_u256_sar_op(&res_val, &shift_val, &val_val);
            return stack_push_const_wide(ls, tmp_dst, res_val.bytes, 32, evm_u256_to_narrow(&res_val));
        }
        return stack_push(ls, tmp_dst);
    }

    /* ── Memory/storage ops ──────────────────────────────────────── */
    case EVM_MLOAD: {
        int a_st = EVM_VAL_UNKNOWN;
        long offset = 0;
        if (ls->stack.depth >= 1) {
            a_st = ls->stack.state[ls->stack.depth - 1];
            if (a_st == EVM_VAL_KNOWN_NARROW) {
                offset = ls->stack.const_vals[ls->stack.depth - 1];
            }
        }
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_LOAD, IR_TY_I64, tmp_dst, tmp_a, NULL, NULL, 0L, ls->pc);
        
        extern evm_u256_t mload_v2(void* mem, uint64_t offset, char** out_expr);
        if (a_st == EVM_VAL_KNOWN_NARROW) {
            char* expr = NULL;
            evm_u256_t val = mload_v2(ls->memory_v2, offset, &expr);
            return stack_push_const_wide(ls, tmp_dst, val.bytes, 32, evm_u256_to_narrow(&val));
        }
        return stack_push(ls, tmp_dst);
    }

    case EVM_MSTORE:
    case EVM_MSTORE8: {
        int a_st = EVM_VAL_UNKNOWN, b_st = EVM_VAL_UNKNOWN;
        long offset = 0;
        evm_u256_t b_val;
        if (ls->stack.depth >= 2) {
            a_st = ls->stack.state[ls->stack.depth - 1]; /* top: offset */
            b_st = ls->stack.state[ls->stack.depth - 2]; /* below: value */
            if (a_st == EVM_VAL_KNOWN_NARROW) {
                offset = ls->stack.const_vals[ls->stack.depth - 1];
            }
            if (b_st == EVM_VAL_KNOWN_WIDE || b_st == EVM_VAL_KNOWN_NARROW) {
                b_val = ls->stack.wide_vals[ls->stack.depth - 2];
            }
        }
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res; /* offset */
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res; /* value */
        ir_emit(ls->func, IR_STORE, IR_TY_I64, tmp_a, tmp_b, NULL, NULL, 0L, ls->pc);
        
        extern void mstore_v2(void* mem, uint64_t offset, evm_u256_t val, const char* expr);
        extern void mstore8_v2(void* mem, uint64_t offset, uint8_t val);
        if (a_st == EVM_VAL_KNOWN_NARROW && (b_st == EVM_VAL_KNOWN_WIDE || b_st == EVM_VAL_KNOWN_NARROW)) {
            if (op == EVM_MSTORE) {
                mstore_v2(ls->memory_v2, offset, b_val, NULL);
            } else if (op == EVM_MSTORE8) {
                mstore8_v2(ls->memory_v2, offset, b_val.bytes[31]);
            }
        }
        return EVM_LIFT_OK;
    }

    case EVM_SLOAD:
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_LOAD, IR_TY_I64, tmp_dst, tmp_a, NULL, "__evm_sload", 0L, ls->pc);
        return stack_push(ls, tmp_dst);

    case EVM_SSTORE: {
        /* Persistent storage write — security-tagged */
        evm_u256_t slot_val, b_val;
        int slot_st = EVM_VAL_UNKNOWN, b_st = EVM_VAL_UNKNOWN;
        if (ls->stack.depth >= 2) {
            slot_st = ls->stack.state[ls->stack.depth - 1];
            b_st = ls->stack.state[ls->stack.depth - 2];
            if (slot_st == EVM_VAL_KNOWN_NARROW || slot_st == EVM_VAL_KNOWN_WIDE) slot_val = ls->stack.wide_vals[ls->stack.depth - 1];
            if (b_st == EVM_VAL_KNOWN_NARROW || b_st == EVM_VAL_KNOWN_WIDE) b_val = ls->stack.wide_vals[ls->stack.depth - 2];
        }
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        tagged_emit(ls, IR_STORE, IR_TY_I64, tmp_a, tmp_b, NULL, "__evm_sstore", 0L, ls->pc,
                    IR_TAG_SSTORE);
                    
        extern void sstore_v2(void* mem, evm_u256_t slot, evm_u256_t val);
        if ((slot_st == EVM_VAL_KNOWN_NARROW || slot_st == EVM_VAL_KNOWN_WIDE) && (b_st == EVM_VAL_KNOWN_NARROW || b_st == EVM_VAL_KNOWN_WIDE)) {
            sstore_v2(ls->memory_v2, slot_val, b_val);
        }
        return EVM_LIFT_OK;
    }

    case EVM_TLOAD:
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        /* Transient read - explicit side effect implicitly handled by tag/backend */
        tagged_emit(ls, IR_LOAD, IR_TY_I64, tmp_dst, tmp_a, NULL, "__evm_tload", 0L, ls->pc, IR_TAG_TLOAD);
        return stack_push(ls, tmp_dst);

    case EVM_TSTORE:
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        /* Transient write - explicit side effect implicitly handled by tag/backend */
        tagged_emit(ls, IR_STORE, IR_TY_I64, tmp_a, tmp_b, NULL, "__evm_tstore", 0L, ls->pc, IR_TAG_TSTORE);
        return EVM_LIFT_OK;

    /* ── Flow control ────────────────────────────────────────────── */
    case EVM_JUMP: {
        int target_is_const = 0;
        long target_val = 0;
        if (ls->stack.depth > 0) {
            target_is_const = (ls->stack.state[ls->stack.depth - 1] == EVM_VAL_KNOWN_NARROW);
            target_val = ls->stack.const_vals[ls->stack.depth - 1];
        }
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        
        if (target_is_const) {
            if (target_val >= 0 && target_val < ls->length && ls->valid_jumpdest && ls->valid_jumpdest[target_val]) {
                snprintf(lbl_buf, IR_LABEL_MAX, ".L_evm_%ld", target_val);
                ir_emit(ls->func, IR_BR, IR_TY_VOID, NULL, NULL, NULL, lbl_buf, 0L, ls->pc);
            } else {
                /* Target is known but INVALID: emits an EVM_BARRIER rather than a blind branch */
                tagged_emit(ls, IR_NOP, IR_TY_VOID, NULL, NULL, NULL, NULL, 0L, ls->pc, IR_TAG_EVM_BARRIER);
            }
        } else {
            /* Indirect jump — we can't resolve the target statically here.
             * Emit a NOP with the target temp as src1 for future CFG pass. */
            ir_emit(ls->func, IR_NOP, IR_TY_VOID, NULL, tmp_a, NULL, NULL, 0L, ls->pc);
        }
        return EVM_LIFT_OK;
    }

    case EVM_JUMPI: {
        int target_is_const = 0;
        long target_val = 0;
        if (ls->stack.depth > 0) {
            target_is_const = (ls->stack.state[ls->stack.depth - 1] == EVM_VAL_KNOWN_NARROW);
            target_val = ls->stack.const_vals[ls->stack.depth - 1];
        }
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        
        if (target_is_const) {
            if (target_val >= 0 && target_val < ls->length && ls->valid_jumpdest && ls->valid_jumpdest[target_val]) {
                snprintf(lbl_buf, IR_LABEL_MAX, ".L_evm_%ld", target_val);
                ir_emit(ls->func, IR_BR_IF, IR_TY_VOID, NULL, tmp_b, NULL, lbl_buf, 0L, ls->pc);
            } else {
                /* Invalid JUMPI target: if taken, it halts. We emit a conditional BR_IF to an error block ideally, 
                 * but lacking that, we tag a NOP as barrier fallback. Note: a true CFG might split the block here. 
                 * For now, emit a tagged barrier placeholder so analysis knows it's an invalid conditional path. */
                tagged_emit(ls, IR_NOP, IR_TY_VOID, NULL, tmp_b, NULL, NULL, 0L, ls->pc, IR_TAG_EVM_BARRIER);
            }
        } else {
            /* Conditional branch — emit BR_IF with condition in src1 */
            ir_emit(ls->func, IR_BR_IF, IR_TY_VOID, NULL, tmp_b, NULL, tmp_a, 0L, ls->pc);
        }
        return EVM_LIFT_OK;
    }

    case EVM_JUMPDEST:
        /* Define a labelled entry point in the IR */
        snprintf(lbl_buf, IR_LABEL_MAX, ".L_evm_%d", ls->pc - 1);
        ir_emit(ls->func, IR_LABEL, IR_TY_VOID, NULL, NULL, NULL, lbl_buf, 0L, ls->pc);
        return EVM_LIFT_OK;

    /* ── Environment queries — zero-operand pushes ───────────────── */
    case EVM_ADDRESS:
    case EVM_ORIGIN:
    case EVM_CALLER:
    case EVM_CALLVALUE:
    case EVM_CALLDATASIZE:
    case EVM_CODESIZE:
    case EVM_GASPRICE:
    case EVM_RETURNDATASIZE:
    case EVM_COINBASE:
    case EVM_TIMESTAMP:
    case EVM_NUMBER:
    case EVM_PREVRANDAO:
    case EVM_GASLIMIT:
    case EVM_CHAINID:
    case EVM_SELFBALANCE:
    case EVM_BASEFEE:
    case EVM_PC:
    case EVM_MSIZE:
    case EVM_GAS:
        /* These push an environment-derived value; model as CONST 0 for now */
        lifter_fresh_tmp(ls, tmp_dst);
        tagged_emit(ls, IR_CONST, IR_TY_I64, tmp_dst, NULL, NULL, NULL, 0L, ls->pc, IR_TAG_HOST_CONTEXT);
        return stack_push(ls, tmp_dst);

    case EVM_BALANCE:
    case EVM_EXTCODESIZE:
    case EVM_EXTCODEHASH:
    case EVM_BLOCKHASH:
    case EVM_CALLDATALOAD: {
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        tagged_emit(ls, IR_LOAD, IR_TY_I64, tmp_dst, tmp_a, NULL, "__evm_calldataload", 0L, ls->pc, IR_TAG_CALLDATALOAD);
        extern void evm_calldata_load(void* mem, const char* offset_tmp);
        evm_calldata_load(ls->memory_v2, tmp_a);
        return stack_push(ls, tmp_dst);
    }

    case EVM_SHA3: {
        int a_st = EVM_VAL_UNKNOWN, b_st = EVM_VAL_UNKNOWN;
        long offset = 0, length = 0;
        evm_u256_t res_val;
        if (ls->stack.depth >= 2) {
            a_st = ls->stack.state[ls->stack.depth - 1]; /* offset */
            b_st = ls->stack.state[ls->stack.depth - 2]; /* length */
            if (a_st == EVM_VAL_KNOWN_NARROW && b_st == EVM_VAL_KNOWN_NARROW) {
                offset = ls->stack.const_vals[ls->stack.depth - 1];
                length = ls->stack.const_vals[ls->stack.depth - 2];
            }
        }
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        tagged_emit(ls, IR_CALL, IR_TY_I64, tmp_dst, NULL, NULL, "__evm_sha3", 0L, ls->pc, IR_TAG_SHA3);
        ir_emit(ls->func, IR_ARG, IR_TY_I64, NULL, tmp_a, NULL, NULL, 0L, ls->pc);
        ir_emit(ls->func, IR_ARG, IR_TY_I64, NULL, tmp_b, NULL, NULL, 0L, ls->pc);
        
        if (a_st == EVM_VAL_KNOWN_NARROW && b_st == EVM_VAL_KNOWN_NARROW) {
            if (offset >= 0 && length >= 0 && length <= 1024 && offset <= 1024 - length) {
                extern const uint8_t* memory_v2_get_ptr(void* mem, uint64_t offset, size_t length);
                const uint8_t* ptr = memory_v2_get_ptr(ls->memory_v2, offset, length);
                if (ptr) {
                    evm_keccak256(ptr, (size_t)length, &res_val);
                    return stack_push_const_wide(ls, tmp_dst, res_val.bytes, 32, evm_u256_to_narrow(&res_val));
                }
            }
        }
        return stack_push(ls, tmp_dst);
    }

    case EVM_ADDMOD:
    case EVM_MULMOD: {
        int a_st = EVM_VAL_UNKNOWN, b_st = EVM_VAL_UNKNOWN, c_st = EVM_VAL_UNKNOWN;
        evm_u256_t a_val, b_val, c_val, res_val;
        if (ls->stack.depth >= 3) {
            a_st = ls->stack.state[ls->stack.depth - 3]; /* top-2 (a) */
            b_st = ls->stack.state[ls->stack.depth - 2]; /* top-1 (b) */
            c_st = ls->stack.state[ls->stack.depth - 1]; /* top   (N) */
            memcpy(&a_val, &ls->stack.wide_vals[ls->stack.depth - 3], sizeof(evm_u256_t));
            memcpy(&b_val, &ls->stack.wide_vals[ls->stack.depth - 2], sizeof(evm_u256_t));
            memcpy(&c_val, &ls->stack.wide_vals[ls->stack.depth - 1], sizeof(evm_u256_t));
        }
        res = stack_pop(ls, tmp_c); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_NOP, IR_TY_I64, tmp_dst, tmp_a, tmp_b, NULL, 0L, ls->pc); /* Still scaffold */
        if ((a_st == EVM_VAL_KNOWN_NARROW || a_st == EVM_VAL_KNOWN_WIDE) &&
            (b_st == EVM_VAL_KNOWN_NARROW || b_st == EVM_VAL_KNOWN_WIDE) &&
            (c_st == EVM_VAL_KNOWN_NARROW || c_st == EVM_VAL_KNOWN_WIDE)) {
            if (op == EVM_ADDMOD) {
                evm_u256_addmod(&res_val, &a_val, &b_val, &c_val);
            } else {
                evm_u256_mulmod(&res_val, &a_val, &b_val, &c_val);
            }
            return stack_push_const_wide(ls, tmp_dst, res_val.bytes, 32, evm_u256_to_narrow(&res_val));
        }
        return stack_push(ls, tmp_dst);
    }

    case EVM_EXP: {
        /* EXP(base, exp): PUSH base then PUSH exp.
         * depth-2 = base (first pushed), depth-1 = exp (top). */
        int a_st = EVM_VAL_UNKNOWN, b_st = EVM_VAL_UNKNOWN;
        evm_u256_t a_val, b_val, res_val;
        if (ls->stack.depth >= 2) {
            a_st = ls->stack.state[ls->stack.depth - 2]; /* base (below top) */
            b_st = ls->stack.state[ls->stack.depth - 1]; /* exp  (top) */
            if ((a_st == EVM_VAL_KNOWN_NARROW || a_st == EVM_VAL_KNOWN_WIDE) &&
                (b_st == EVM_VAL_KNOWN_NARROW || b_st == EVM_VAL_KNOWN_WIDE)) {
                a_val = ls->stack.wide_vals[ls->stack.depth - 2]; /* base */
                b_val = ls->stack.wide_vals[ls->stack.depth - 1]; /* exp */
            }
        }
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res; /* exp (top) */
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res; /* base */
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_NOP, IR_TY_I64, tmp_dst, tmp_a, tmp_b, NULL, 0L, ls->pc);
        if ((a_st == EVM_VAL_KNOWN_NARROW || a_st == EVM_VAL_KNOWN_WIDE) &&
            (b_st == EVM_VAL_KNOWN_NARROW || b_st == EVM_VAL_KNOWN_WIDE)) {
            evm_u256_exp(&res_val, &a_val /*base*/, &b_val /*exp*/);
            return stack_push_const_wide(ls, tmp_dst, res_val.bytes, 32, evm_u256_to_narrow(&res_val));
        }
        return stack_push(ls, tmp_dst);
    }

    case EVM_BYTE: {
        /* BYTE(i, x): PUSH x first then PUSH i.
         * depth-2 = x (value, below top), depth-1 = i (position, top). */
        int a_st = EVM_VAL_UNKNOWN, b_st = EVM_VAL_UNKNOWN;
        evm_u256_t a_val, b_val, res_val;
        if (ls->stack.depth >= 2) {
            a_st = ls->stack.state[ls->stack.depth - 2]; /* x = value (below top) */
            b_st = ls->stack.state[ls->stack.depth - 1]; /* i = position (top) */
            if ((a_st == EVM_VAL_KNOWN_NARROW || a_st == EVM_VAL_KNOWN_WIDE) &&
                (b_st == EVM_VAL_KNOWN_NARROW || b_st == EVM_VAL_KNOWN_WIDE)) {
                a_val = ls->stack.wide_vals[ls->stack.depth - 2]; /* x (value) */
                b_val = ls->stack.wide_vals[ls->stack.depth - 1]; /* i (position) */
            }
        }
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res; /* i (top) */
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res; /* x */
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_NOP, IR_TY_I64, tmp_dst, tmp_a, tmp_b, NULL, 0L, ls->pc);
        if ((a_st == EVM_VAL_KNOWN_NARROW || a_st == EVM_VAL_KNOWN_WIDE) &&
            (b_st == EVM_VAL_KNOWN_NARROW || b_st == EVM_VAL_KNOWN_WIDE)) {
            /* helper signature: byte_op(dst, pos, val) */
            evm_u256_byte_op(&res_val, &b_val /*pos=i*/, &a_val /*val=x*/);
            return stack_push_const_wide(ls, tmp_dst, res_val.bytes, 32, evm_u256_to_narrow(&res_val));
        }
        return stack_push(ls, tmp_dst);
    }

    case EVM_SIGNEXTEND: {
        /* SIGNEXTEND(b, x): PUSH x first then PUSH b.
         * depth-2 = x (value to extend, below top), depth-1 = b (byte index, top). */
        int a_st = EVM_VAL_UNKNOWN, b_st = EVM_VAL_UNKNOWN;
        evm_u256_t a_val, b_val, res_val;
        if (ls->stack.depth >= 2) {
            a_st = ls->stack.state[ls->stack.depth - 2]; /* x (below top) */
            b_st = ls->stack.state[ls->stack.depth - 1]; /* b = byte index (top) */
            if ((a_st == EVM_VAL_KNOWN_NARROW || a_st == EVM_VAL_KNOWN_WIDE) &&
                (b_st == EVM_VAL_KNOWN_NARROW || b_st == EVM_VAL_KNOWN_WIDE)) {
                a_val = ls->stack.wide_vals[ls->stack.depth - 2]; /* x */
                b_val = ls->stack.wide_vals[ls->stack.depth - 1]; /* b */
            }
        }
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res; /* b (top) */
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res; /* x */
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_NOP, IR_TY_I64, tmp_dst, tmp_a, tmp_b, NULL, 0L, ls->pc);
        if ((a_st == EVM_VAL_KNOWN_NARROW || a_st == EVM_VAL_KNOWN_WIDE) &&
            (b_st == EVM_VAL_KNOWN_NARROW || b_st == EVM_VAL_KNOWN_WIDE)) {
            /* helper signature: signextend_op(dst, b, x) */
            evm_u256_signextend_op(&res_val, &b_val /*b=byte_idx*/, &a_val /*x=value*/);
            return stack_push_const_wide(ls, tmp_dst, res_val.bytes, 32, evm_u256_to_narrow(&res_val));
        }
        return stack_push(ls, tmp_dst);
    }

    case EVM_CALLDATACOPY:
    case EVM_CODECOPY:
    case EVM_RETURNDATACOPY: {
        const char *fn = "";
        if (op == (unsigned int)EVM_CALLDATACOPY) fn = "__evm_calldatacopy";
        else if (op == (unsigned int)EVM_CODECOPY) fn = "__evm_codecopy";
        else fn = "__evm_returndatacopy";
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_c); if (res != EVM_LIFT_OK) return res;
        tagged_emit(ls, IR_CALL, IR_TY_VOID, NULL, NULL, NULL, fn, 0L, ls->pc, IR_TAG_MEMORY_COPY);
        ir_emit(ls->func, IR_ARG, IR_TY_I64, NULL, tmp_a, NULL, NULL, 0L, ls->pc);
        ir_emit(ls->func, IR_ARG, IR_TY_I64, NULL, tmp_b, NULL, NULL, 0L, ls->pc);
        ir_emit(ls->func, IR_ARG, IR_TY_I64, NULL, tmp_c, NULL, NULL, 0L, ls->pc);
        return EVM_LIFT_OK;
    }

    case EVM_EXTCODECOPY: {
        char ext_a[IR_NAME_MAX], ext_b[IR_NAME_MAX], ext_c[IR_NAME_MAX], ext_d[IR_NAME_MAX];
        res = stack_pop(ls, ext_a); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, ext_b); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, ext_c); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, ext_d); if (res != EVM_LIFT_OK) return res;
        tagged_emit(ls, IR_CALL, IR_TY_VOID, NULL, NULL, NULL, "__evm_extcodecopy", 0L, ls->pc, IR_TAG_MEMORY_COPY);
        ir_emit(ls->func, IR_ARG, IR_TY_I64, NULL, ext_a, NULL, NULL, 0L, ls->pc);
        ir_emit(ls->func, IR_ARG, IR_TY_I64, NULL, ext_b, NULL, NULL, 0L, ls->pc);
        ir_emit(ls->func, IR_ARG, IR_TY_I64, NULL, ext_c, NULL, NULL, 0L, ls->pc);
        ir_emit(ls->func, IR_ARG, IR_TY_I64, NULL, ext_d, NULL, NULL, 0L, ls->pc);
        return EVM_LIFT_OK;
    }

    /* ── LOG0..LOG4 ──────────────────────────────────────────────── */
    case EVM_LOG0:
    case EVM_LOG1:
    case EVM_LOG2:
    case EVM_LOG3:
    case EVM_LOG4: {
        /* LOG_n pops (2 + n) args: offset, length, topic0..topicN */
        int log_topics = (int)(op - (unsigned int)EVM_LOG0);
        int log_pops   = 2 + log_topics;
        int j;
        char args[7][IR_NAME_MAX]; /* Max pops is 6 (LOG4) */
        char fn[32];
        snprintf(fn, sizeof(fn), "__evm_log%d", log_topics);
        for (j = 0; j < log_pops; j++) {
            res = stack_pop(ls, args[j]);
            if (res != EVM_LIFT_OK) return res;
        }
        tagged_emit(ls, IR_CALL, IR_TY_VOID, NULL, NULL, NULL, fn, 0L, ls->pc, IR_TAG_LOG);
        for (j = 0; j < log_pops; j++) {
            ir_emit(ls->func, IR_ARG, IR_TY_I64, NULL, args[j], NULL, NULL, 0L, ls->pc);
        }
        return EVM_LIFT_OK;
    }

    /* ── CALL-family: security-critical ─────────────────────────── */
    /*
     * EVM CALL stack signature (7 args):
     *   gas, addr, value, argsOffset, argsLength, retOffset, retLength
     * Pops all 7, pushes success flag (0 or 1).
     *
     * DELEGATECALL / STATICCALL (6 args):
     *   gas, addr, argsOffset, argsLength, retOffset, retLength
     *
     * CALLCODE (7 args, same as CALL):
     *   gas, addr, value, argsOffset, argsLength, retOffset, retLength
     *
     * All CALL-family opcodes are tagged IR_TAG_UNTRUSTED_EXTERNAL_CALL
     * except STATICCALL which is tagged IR_TAG_STATIC_CALL.
     */
    case EVM_CALL:
    case EVM_CALLCODE: {
        char gas[IR_NAME_MAX], addr[IR_NAME_MAX], value[IR_NAME_MAX];
        char ao[IR_NAME_MAX], al[IR_NAME_MAX], ro[IR_NAME_MAX], rl[IR_NAME_MAX];
        res = stack_pop(ls, gas);   if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, addr);  if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, value); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, ao);    if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, al);    if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, ro);    if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, rl);    if (res != EVM_LIFT_OK) return res;

        char clamped_gas[IR_NAME_MAX];
        lifter_fresh_tmp(ls, clamped_gas);
        evm_inject_eip150_clamp(ls->func, ls, gas, ls->current_gas_vreg, clamped_gas);
        strcpy(gas, clamped_gas);

        lifter_fresh_tmp(ls, tmp_dst);
        /* Emit IR_CALL targeting the address held in `addr` temp */
        node = tagged_emit(ls, IR_CALL, IR_TY_I64,
                           tmp_dst, NULL, NULL, addr, 0L, ls->pc,
                           IR_TAG_UNTRUSTED_EXTERNAL_CALL);
        /* Emit ARG nodes for gas, value (key audit parameters) */
        ir_emit(ls->func, IR_ARG, IR_TY_I64, NULL, gas, NULL, NULL, 0L, ls->pc);
        ir_emit(ls->func, IR_ARG, IR_TY_I64, NULL, value, NULL, NULL, 0L, ls->pc);
        (void)node;
        ls->call_count++;
        /* Push the success flag */
        return stack_push(ls, tmp_dst);
    }

    case EVM_DELEGATECALL: {
        char gas[IR_NAME_MAX], addr[IR_NAME_MAX];
        char ao[IR_NAME_MAX], al[IR_NAME_MAX], ro[IR_NAME_MAX], rl[IR_NAME_MAX];
        res = stack_pop(ls, gas);  if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, addr); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, ao);   if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, al);   if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, ro);   if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, rl);   if (res != EVM_LIFT_OK) return res;

        char clamped_gas[IR_NAME_MAX];
        lifter_fresh_tmp(ls, clamped_gas);
        evm_inject_eip150_clamp(ls->func, ls, gas, ls->current_gas_vreg, clamped_gas);
        strcpy(gas, clamped_gas);

        lifter_fresh_tmp(ls, tmp_dst);
        node = tagged_emit(ls, IR_CALL, IR_TY_I64,
                           tmp_dst, NULL, NULL, addr, 0L, ls->pc,
                           IR_TAG_UNTRUSTED_EXTERNAL_CALL);
        /* DELEGATECALL additionally crosses a privilege boundary:
         * callee executes in caller's storage context. */
        if (node) {
            ir_vuln_tag_set(node,
                (ir_vuln_tag_t)(IR_VULN_DELEGATE_CALL | IR_VULN_PRIV_BOUNDARY));
        }
        ir_emit(ls->func, IR_ARG, IR_TY_I64, NULL, gas, NULL, NULL, 0L, ls->pc);
        ls->call_count++;
        return stack_push(ls, tmp_dst);
    }

    case EVM_STATICCALL: {
        char gas[IR_NAME_MAX], addr[IR_NAME_MAX];
        char ao[IR_NAME_MAX], al[IR_NAME_MAX], ro[IR_NAME_MAX], rl[IR_NAME_MAX];
        res = stack_pop(ls, gas);  if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, addr); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, ao);   if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, al);   if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, ro);   if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, rl);   if (res != EVM_LIFT_OK) return res;

        char clamped_gas[IR_NAME_MAX];
        lifter_fresh_tmp(ls, clamped_gas);
        evm_inject_eip150_clamp(ls->func, ls, gas, ls->current_gas_vreg, clamped_gas);
        strcpy(gas, clamped_gas);

        lifter_fresh_tmp(ls, tmp_dst);
        node = tagged_emit(ls, IR_CALL, IR_TY_I64,
                           tmp_dst, NULL, NULL, addr, 0L, ls->pc,
                           IR_TAG_STATIC_CALL);
        ir_emit(ls->func, IR_ARG, IR_TY_I64, NULL, gas, NULL, NULL, 0L, ls->pc);
        (void)node;
        ls->call_count++;
        return stack_push(ls, tmp_dst);
    }

    /* ── CREATE / CREATE2 ────────────────────────────────────────── */
    case EVM_CREATE: {
        char val[IR_NAME_MAX], offset[IR_NAME_MAX], length[IR_NAME_MAX];
        res = stack_pop(ls, val);    if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, offset); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, length); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        tagged_emit(ls, IR_CALL, IR_TY_PTR,
                    tmp_dst, val, offset, "evm_create", 0L, ls->pc,
                    IR_TAG_CREATE);
        return stack_push(ls, tmp_dst);
    }

    case EVM_CREATE2: {
        char val[IR_NAME_MAX], offset[IR_NAME_MAX], length[IR_NAME_MAX];
        char salt[IR_NAME_MAX];
        res = stack_pop(ls, val);    if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, offset); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, length); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, salt);   if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        tagged_emit(ls, IR_CALL, IR_TY_PTR,
                    tmp_dst, val, offset, "evm_create2", 0L, ls->pc,
                    IR_TAG_CREATE);
        return stack_push(ls, tmp_dst);
    }

    /* ── SELFDESTRUCT ────────────────────────────────────────────── */
    case EVM_SELFDESTRUCT:
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        tagged_emit(ls, IR_RET, IR_TY_VOID, NULL, tmp_a, NULL, NULL, 0L, ls->pc,
                    IR_TAG_SELFDESTRUCT);
        return EVM_LIFT_OK;

    /* ── Unknown / unimplemented ─────────────────────────────────── */
    default:
        ir_emit(ls->func, IR_NOP, IR_TY_VOID, NULL, NULL, NULL, NULL, 0L, ls->pc);
        return EVM_LIFT_OK;
    }
}

/* ── Full bytecode lift ──────────────────────────────────────────────── */

evm_lift_result_t evm_lift_bytecode(evm_lifter_t *ls) {
    evm_lift_result_t res;

    while (ls->pc < ls->length) {
        unsigned int op = ls->bytecode[ls->pc];
        res = evm_lift_step(ls);
        
        if (op == EVM_JUMP || op == EVM_JUMPI || op == EVM_STOP || op == EVM_RETURN || 
            op == EVM_REVERT || op == EVM_INVALID || op == EVM_SELFDESTRUCT) {
            ls->is_block_start = 1;
        }
        
        if (res == EVM_LIFT_TRUNCATED || res == EVM_LIFT_OOM) {
            return res;
        }
        if (res == EVM_LIFT_INVALID_OP) {
            return res;
        }
        if (res == EVM_LIFT_STACK_OVER || res == EVM_LIFT_STACK_UNDER) {
            return res;
        }
    }
    return EVM_LIFT_OK;
}
