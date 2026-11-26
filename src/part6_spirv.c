/* SPDX-License-Identifier: MIT */
/* ZCC Isolated SPIR-V 1.0 Binary Compiler / Emitter (PASSES-integrated, Track 21B) */
#define _POSIX_C_SOURCE 200809L
#include "zcc_compute.h"
#include "zcc_spirv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    SPV_OP_NAME                 = 5,
    SPV_OP_EXT_INST_IMPORT      = 11,
    SPV_OP_EXT_INST             = 12,
    SPV_OP_MEMORY_MODEL         = 14,
    SPV_OP_ENTRY_POINT          = 15,
    SPV_OP_EXECUTION_MODE       = 16,
    SPV_OP_CAPABILITY           = 17,
    SPV_OP_TYPE_VOID            = 19,
    SPV_OP_TYPE_BOOL            = 20,
    SPV_OP_TYPE_INT             = 21,
    SPV_OP_TYPE_FLOAT           = 22,
    SPV_OP_TYPE_VECTOR          = 23,
    SPV_OP_TYPE_RUNTIME_ARRAY   = 29,
    SPV_OP_TYPE_STRUCT          = 30,
    SPV_OP_TYPE_POINTER         = 32,
    SPV_OP_TYPE_FUNCTION        = 33,
    SPV_OP_CONSTANT             = 43,
    SPV_OP_FUNCTION             = 54,
    SPV_OP_FUNCTION_END         = 56,
    SPV_OP_VARIABLE             = 59,
    SPV_OP_LOAD                 = 61,
    SPV_OP_STORE                = 62,
    SPV_OP_ACCESS_CHAIN         = 65,
    SPV_OP_DECORATE             = 71,
    SPV_OP_MEMBER_DECORATE      = 72,
    SPV_OP_COMPOSITE_EXTRACT    = 81,
    SPV_OP_CONVERT_STOF         = 111,
    SPV_OP_CONVERT_UTOF         = 112,
    SPV_OP_BITCAST              = 124,
    SPV_OP_IADD                 = 128,
    SPV_OP_FADD                 = 129,
    SPV_OP_ISUB                 = 130,
    SPV_OP_FSUB                 = 131,
    SPV_OP_IMUL                 = 132,
    SPV_OP_FMUL                 = 133,
    SPV_OP_FDIV                 = 136,
    SPV_OP_IS_NAN               = 156,
    SPV_OP_LOGICAL_OR           = 166,
    SPV_OP_LOGICAL_AND          = 167,
    SPV_OP_LOGICAL_NOT          = 168,
    SPV_OP_SELECT               = 169,
    SPV_OP_IEQUAL               = 170,
    SPV_OP_ULESS_THAN           = 176,
    SPV_OP_FORD_GREATER_THAN    = 186,
    SPV_OP_SHR_LOGICAL          = 194,
    SPV_OP_SHR_ARITHMETIC       = 195,
    SPV_OP_SHL                  = 196,
    SPV_OP_BITWISE_OR           = 197,
    SPV_OP_BITWISE_AND          = 199,
    SPV_OP_LOOP_MERGE           = 246,
    SPV_OP_SELECTION_MERGE      = 247,
    SPV_OP_LABEL                = 248,
    SPV_OP_BRANCH               = 249,
    SPV_OP_BRANCH_CONDITIONAL   = 250,
    SPV_OP_RETURN               = 253
};

enum {
    SPV_CAP_SHADER              = 1,
    SPV_ADDR_LOGICAL            = 0,
    SPV_MEM_GLSL450             = 1,
    SPV_EXEC_GLCOMPUTE          = 5,
    SPV_MODE_LOCAL_SIZE         = 17,
    SPV_SC_INPUT                = 1,
    SPV_SC_UNIFORM              = 2,
    SPV_SC_FUNCTION             = 7,
    SPV_SC_PUSH_CONSTANT        = 9,
    SPV_DEC_BLOCK               = 2,
    SPV_DEC_BUFFER_BLOCK        = 3,
    SPV_DEC_ARRAY_STRIDE        = 6,
    SPV_DEC_BUILTIN             = 11,
    SPV_DEC_NON_WRITABLE        = 24,
    SPV_DEC_BINDING             = 33,
    SPV_DEC_DESCRIPTOR_SET      = 34,
    SPV_DEC_OFFSET              = 35,
    SPV_BI_GLOBAL_INVOCATION_ID = 28
};

enum {
    GLSL_EXP             = 27,
    GLSL_SQRT            = 31,
    GLSL_FMA             = 50,
    GLSL_UNPACK_HALF2X16 = 62
};

typedef struct spv_buf_s {
    uint32_t *words;
    size_t    len;
    size_t    cap;
} spv_buf_t;

typedef struct spv_const_u32_entry_s {
    uint32_t val;
    uint32_t id;
} spv_const_u32_entry_t;

typedef struct spv_const_f32_entry_s {
    uint32_t bits;
    uint32_t id;
} spv_const_f32_entry_t;

typedef struct spv_mod_s {
    spv_buf_t pre;
    spv_buf_t entry;
    spv_buf_t debug;
    spv_buf_t annot;
    spv_buf_t types;
    spv_buf_t fn_head;
    spv_buf_t fn_body;
    uint32_t  next_id;
    int       oom;

    uint32_t id_glsl;
    uint32_t t_void, t_bool, t_u32, t_i32, t_f32, t_v3u32, t_v2f32;
    uint32_t t_rt_u32, t_ssbo, t_push, t_fn_void;
    uint32_t t_ptr_unif_ssbo, t_ptr_unif_u32, t_ptr_push_struct, t_ptr_push_u32;
    uint32_t t_ptr_input_v3u32, t_ptr_input_u32;
    uint32_t t_ptr_fn_u32, t_ptr_fn_f32;

    uint32_t var_buf[4];
    uint32_t var_push;
    uint32_t var_gid;
    uint32_t fn_main;

    spv_const_u32_entry_t u32_consts[96];
    size_t                u32_const_count;
    spv_const_f32_entry_t f32_consts[32];
    size_t                f32_const_count;
    uint32_t              c_i32_8;
    uint32_t              c_i32_24;
} spv_mod_t;

static void buf_push(spv_mod_t *m, spv_buf_t *b, uint32_t w) {
    if (m->oom) return;
    if (b->len >= b->cap) {
        size_t ncap = b->cap ? (b->cap * 2U) : 128U;
        uint32_t *nw = (uint32_t *)realloc(b->words, ncap * sizeof(uint32_t));
        if (!nw) { m->oom = 1; return; }
        b->words = nw;
        b->cap = ncap;
    }
    b->words[b->len++] = w;
}

static uint32_t spv_id(spv_mod_t *m) {
    return m->next_id++;
}

static void emit_op(spv_mod_t *m, spv_buf_t *b, uint16_t op, size_t nops, const uint32_t *ops) {
    uint32_t wc = (uint32_t)(1U + nops);
    buf_push(m, b, (wc << 16) | (uint32_t)op);
    for (size_t i = 0; i < nops; i++) buf_push(m, b, ops[i]);
}

static void emit_op_str(spv_mod_t *m, spv_buf_t *b, uint16_t op,
                        size_t pre_cnt, const uint32_t *pre_ops,
                        const char *str, size_t post_cnt, const uint32_t *post_ops) {
    size_t slen = strlen(str) + 1U;
    size_t swords = (slen + 3U) / 4U;
    uint32_t wc = (uint32_t)(1U + pre_cnt + swords + post_cnt);
    buf_push(m, b, (wc << 16) | (uint32_t)op);
    for (size_t i = 0; i < pre_cnt; i++) buf_push(m, b, pre_ops[i]);
    for (size_t wi = 0; wi < swords; wi++) {
        uint32_t w = 0U;
        for (size_t bi = 0; bi < 4U; bi++) {
            size_t pos = wi * 4U + bi;
            if (pos < slen) w |= ((uint32_t)(uint8_t)str[pos]) << (bi * 8U);
        }
        buf_push(m, b, w);
    }
    for (size_t i = 0; i < post_cnt; i++) buf_push(m, b, post_ops[i]);
}

static void op1(spv_mod_t *m, spv_buf_t *b, uint16_t op, uint32_t a) {
    emit_op(m, b, op, 1, &a);
}

static void op2(spv_mod_t *m, spv_buf_t *b, uint16_t op, uint32_t a, uint32_t x) {
    uint32_t ops[2] = {a, x};
    emit_op(m, b, op, 2, ops);
}

static void op3(spv_mod_t *m, spv_buf_t *b, uint16_t op, uint32_t a, uint32_t x, uint32_t y) {
    uint32_t ops[3] = {a, x, y};
    emit_op(m, b, op, 3, ops);
}

static void op4(spv_mod_t *m, spv_buf_t *b, uint16_t op, uint32_t a, uint32_t x, uint32_t y, uint32_t z) {
    uint32_t ops[4] = {a, x, y, z};
    emit_op(m, b, op, 4, ops);
}

static uint32_t emit_res3(spv_mod_t *m, spv_buf_t *b, uint16_t op, uint32_t ty, uint32_t a) {
    uint32_t id = spv_id(m);
    op3(m, b, op, ty, id, a);
    return id;
}

static uint32_t emit_res4(spv_mod_t *m, spv_buf_t *b, uint16_t op, uint32_t ty, uint32_t a, uint32_t x) {
    uint32_t id = spv_id(m);
    op4(m, b, op, ty, id, a, x);
    return id;
}

static uint32_t emit_res5(spv_mod_t *m, spv_buf_t *b, uint16_t op, uint32_t ty, uint32_t a, uint32_t x, uint32_t y) {
    uint32_t id = spv_id(m);
    uint32_t ops[5] = {ty, id, a, x, y};
    emit_op(m, b, op, 5, ops);
    return id;
}

static uint32_t const_u32(spv_mod_t *m, uint32_t val) {
    for (size_t i = 0; i < m->u32_const_count; i++) {
        if (m->u32_consts[i].val == val) return m->u32_consts[i].id;
    }
    uint32_t id = spv_id(m);
    op3(m, &m->types, SPV_OP_CONSTANT, m->t_u32, id, val);
    if (m->u32_const_count < 96U) {
        m->u32_consts[m->u32_const_count].val = val;
        m->u32_consts[m->u32_const_count].id = id;
        m->u32_const_count++;
    }
    return id;
}

static uint32_t const_f32_bits(spv_mod_t *m, uint32_t bits) {
    for (size_t i = 0; i < m->f32_const_count; i++) {
        if (m->f32_consts[i].bits == bits) return m->f32_consts[i].id;
    }
    uint32_t id = spv_id(m);
    op3(m, &m->types, SPV_OP_CONSTANT, m->t_f32, id, bits);
    if (m->f32_const_count < 32U) {
        m->f32_consts[m->f32_const_count].bits = bits;
        m->f32_consts[m->f32_const_count].id = id;
        m->f32_const_count++;
    }
    return id;
}

static uint32_t local_var(spv_mod_t *m, uint32_t ptr_type) {
    uint32_t id = spv_id(m);
    op3(m, &m->fn_head, SPV_OP_VARIABLE, ptr_type, id, SPV_SC_FUNCTION);
    return id;
}

static uint32_t load_u32(spv_mod_t *m, uint32_t ptr) {
    return emit_res3(m, &m->fn_body, SPV_OP_LOAD, m->t_u32, ptr);
}

static uint32_t load_f32(spv_mod_t *m, uint32_t ptr) {
    return emit_res3(m, &m->fn_body, SPV_OP_LOAD, m->t_f32, ptr);
}

static void store_var(spv_mod_t *m, uint32_t ptr, uint32_t val) {
    op2(m, &m->fn_body, SPV_OP_STORE, ptr, val);
}

static uint32_t glsl1(spv_mod_t *m, uint32_t ret_ty, uint32_t ext_op, uint32_t x) {
    uint32_t id = spv_id(m);
    uint32_t ops[5] = {ret_ty, id, m->id_glsl, ext_op, x};
    emit_op(m, &m->fn_body, SPV_OP_EXT_INST, 5, ops);
    return id;
}

static uint32_t glsl3(spv_mod_t *m, uint32_t ret_ty, uint32_t ext_op, uint32_t a, uint32_t b, uint32_t c) {
    uint32_t id = spv_id(m);
    uint32_t ops[7] = {ret_ty, id, m->id_glsl, ext_op, a, b, c};
    emit_op(m, &m->fn_body, SPV_OP_EXT_INST, 7, ops);
    return id;
}

static uint32_t push_u32(spv_mod_t *m, uint32_t field_idx) {
    uint32_t cidx = const_u32(m, field_idx);
    uint32_t ptr = emit_res4(m, &m->fn_body, SPV_OP_ACCESS_CHAIN, m->t_ptr_push_u32, m->var_push, cidx);
    return load_u32(m, ptr);
}

static uint32_t gid_comp(spv_mod_t *m, uint32_t comp) {
    uint32_t cidx = const_u32(m, comp);
    uint32_t ptr = emit_res4(m, &m->fn_body, SPV_OP_ACCESS_CHAIN, m->t_ptr_input_u32, m->var_gid, cidx);
    return load_u32(m, ptr);
}

static uint32_t buf_word_load(spv_mod_t *m, uint32_t buf_idx, uint32_t word_idx) {
    uint32_t c0 = const_u32(m, 0U);
    uint32_t ptr = emit_res5(m, &m->fn_body, SPV_OP_ACCESS_CHAIN, m->t_ptr_unif_u32, m->var_buf[buf_idx], c0, word_idx);
    return load_u32(m, ptr);
}

static void buf_word_store(spv_mod_t *m, uint32_t buf_idx, uint32_t word_idx, uint32_t u32_val) {
    uint32_t c0 = const_u32(m, 0U);
    uint32_t ptr = emit_res5(m, &m->fn_body, SPV_OP_ACCESS_CHAIN, m->t_ptr_unif_u32, m->var_buf[buf_idx], c0, word_idx);
    store_var(m, ptr, u32_val);
}

static void store_f32_at_byte(spv_mod_t *m, uint32_t byte_addr, uint32_t f32_val) {
    uint32_t widx = emit_res4(m, &m->fn_body, SPV_OP_SHR_LOGICAL, m->t_u32, byte_addr, const_u32(m, 2U));
    uint32_t bits = emit_res3(m, &m->fn_body, SPV_OP_BITCAST, m->t_u32, f32_val);
    buf_word_store(m, 3U, widx, bits);
}

static uint32_t load_f32_at_byte_out(spv_mod_t *m, uint32_t byte_addr) {
    uint32_t widx = emit_res4(m, &m->fn_body, SPV_OP_SHR_LOGICAL, m->t_u32, byte_addr, const_u32(m, 2U));
    uint32_t bits = buf_word_load(m, 3U, widx);
    return emit_res3(m, &m->fn_body, SPV_OP_BITCAST, m->t_f32, bits);
}

static uint32_t load_u8(spv_mod_t *m, uint32_t buf_idx, uint32_t byte_addr) {
    uint32_t widx = emit_res4(m, &m->fn_body, SPV_OP_SHR_LOGICAL, m->t_u32, byte_addr, const_u32(m, 2U));
    uint32_t word = buf_word_load(m, buf_idx, widx);
    uint32_t rem  = emit_res4(m, &m->fn_body, SPV_OP_BITWISE_AND, m->t_u32, byte_addr, const_u32(m, 3U));
    uint32_t sh   = emit_res4(m, &m->fn_body, SPV_OP_SHL, m->t_u32, rem, const_u32(m, 3U));
    uint32_t shv  = emit_res4(m, &m->fn_body, SPV_OP_SHR_LOGICAL, m->t_u32, word, sh);
    return emit_res4(m, &m->fn_body, SPV_OP_BITWISE_AND, m->t_u32, shv, const_u32(m, 0xFFU));
}

static uint32_t load_u16_le(spv_mod_t *m, uint32_t buf_idx, uint32_t byte_addr) {
    uint32_t b0 = load_u8(m, buf_idx, byte_addr);
    uint32_t b1 = load_u8(m, buf_idx, emit_res4(m, &m->fn_body, SPV_OP_IADD, m->t_u32, byte_addr, const_u32(m, 1U)));
    uint32_t hi = emit_res4(m, &m->fn_body, SPV_OP_SHL, m->t_u32, b1, const_u32(m, 8U));
    return emit_res4(m, &m->fn_body, SPV_OP_BITWISE_OR, m->t_u32, b0, hi);
}

static uint32_t load_f32_byte(spv_mod_t *m, uint32_t buf_idx, uint32_t byte_addr) {
    uint32_t var_res = local_var(m, m->t_ptr_fn_u32);
    uint32_t widx = emit_res4(m, &m->fn_body, SPV_OP_SHR_LOGICAL, m->t_u32, byte_addr, const_u32(m, 2U));
    uint32_t w0 = buf_word_load(m, buf_idx, widx);
    uint32_t rem = emit_res4(m, &m->fn_body, SPV_OP_BITWISE_AND, m->t_u32, byte_addr, const_u32(m, 3U));
    uint32_t is_al = emit_res4(m, &m->fn_body, SPV_OP_IEQUAL, m->t_bool, rem, const_u32(m, 0U));

    uint32_t l_then = spv_id(m), l_else = spv_id(m), l_merge = spv_id(m);
    op2(m, &m->fn_body, SPV_OP_SELECTION_MERGE, l_merge, 0U);
    op3(m, &m->fn_body, SPV_OP_BRANCH_CONDITIONAL, is_al, l_then, l_else);

    op1(m, &m->fn_body, SPV_OP_LABEL, l_then);
    store_var(m, var_res, w0);
    op1(m, &m->fn_body, SPV_OP_BRANCH, l_merge);

    op1(m, &m->fn_body, SPV_OP_LABEL, l_else);
    uint32_t widx1 = emit_res4(m, &m->fn_body, SPV_OP_IADD, m->t_u32, widx, const_u32(m, 1U));
    uint32_t w1 = buf_word_load(m, buf_idx, widx1);
    uint32_t sh_lo = emit_res4(m, &m->fn_body, SPV_OP_SHL, m->t_u32, rem, const_u32(m, 3U));
    uint32_t four_minus = emit_res4(m, &m->fn_body, SPV_OP_ISUB, m->t_u32, const_u32(m, 4U), rem);
    uint32_t sh_hi = emit_res4(m, &m->fn_body, SPV_OP_SHL, m->t_u32, four_minus, const_u32(m, 3U));
    uint32_t lo_part = emit_res4(m, &m->fn_body, SPV_OP_SHR_LOGICAL, m->t_u32, w0, sh_lo);
    uint32_t hi_part = emit_res4(m, &m->fn_body, SPV_OP_SHL, m->t_u32, w1, sh_hi);
    uint32_t comb = emit_res4(m, &m->fn_body, SPV_OP_BITWISE_OR, m->t_u32, lo_part, hi_part);
    store_var(m, var_res, comb);
    op1(m, &m->fn_body, SPV_OP_BRANCH, l_merge);

    op1(m, &m->fn_body, SPV_OP_LABEL, l_merge);
    uint32_t loaded_bits = load_u32(m, var_res);
    return emit_res3(m, &m->fn_body, SPV_OP_BITCAST, m->t_f32, loaded_bits);
}

static uint32_t f16_u32_to_f32(spv_mod_t *m, uint32_t h_u32) {
    uint32_t vec2 = glsl1(m, m->t_v2f32, GLSL_UNPACK_HALF2X16, h_u32);
    uint32_t res = spv_id(m);
    uint32_t ops[4] = {m->t_f32, res, vec2, 0U};
    emit_op(m, &m->fn_body, SPV_OP_COMPOSITE_EXTRACT, 4, ops);
    return res;
}

static uint32_t u8_signed_to_f32(spv_mod_t *m, uint32_t u8_val) {
    uint32_t as_i32 = emit_res3(m, &m->fn_body, SPV_OP_BITCAST, m->t_i32, u8_val);
    uint32_t s_shl = emit_res4(m, &m->fn_body, SPV_OP_SHL, m->t_i32, as_i32, m->c_i32_24);
    uint32_t s_sar = emit_res4(m, &m->fn_body, SPV_OP_SHR_ARITHMETIC, m->t_i32, s_shl, m->c_i32_24);
    return emit_res3(m, &m->fn_body, SPV_OP_CONVERT_STOF, m->t_f32, s_sar);
}

static uint32_t u32_to_f32(spv_mod_t *m, uint32_t u) {
    return emit_res3(m, &m->fn_body, SPV_OP_CONVERT_UTOF, m->t_f32, u);
}

static uint32_t addr_1d(spv_mod_t *m, uint32_t base_off, uint32_t idx, uint32_t stride) {
    uint32_t prod = emit_res4(m, &m->fn_body, SPV_OP_IMUL, m->t_u32, idx, stride);
    return emit_res4(m, &m->fn_body, SPV_OP_IADD, m->t_u32, base_off, prod);
}

static uint32_t addr_2d(spv_mod_t *m, uint32_t base_off, uint32_t r, uint32_t c, uint32_t s0, uint32_t s1) {
    uint32_t pr = emit_res4(m, &m->fn_body, SPV_OP_IMUL, m->t_u32, r, s0);
    uint32_t pc = emit_res4(m, &m->fn_body, SPV_OP_IMUL, m->t_u32, c, s1);
    uint32_t sum = emit_res4(m, &m->fn_body, SPV_OP_IADD, m->t_u32, base_off, pr);
    return emit_res4(m, &m->fn_body, SPV_OP_IADD, m->t_u32, sum, pc);
}

typedef void (*spv_block_fn)(spv_mod_t *m, uint32_t loop_var_ptr, void *ctx);

static void emit_if(spv_mod_t *m, uint32_t cond_bool, void (*then_fn)(spv_mod_t *, void *), void *ctx) {
    uint32_t l_then = spv_id(m), l_merge = spv_id(m);
    op2(m, &m->fn_body, SPV_OP_SELECTION_MERGE, l_merge, 0U);
    op3(m, &m->fn_body, SPV_OP_BRANCH_CONDITIONAL, cond_bool, l_then, l_merge);
    op1(m, &m->fn_body, SPV_OP_LABEL, l_then);
    then_fn(m, ctx);
    op1(m, &m->fn_body, SPV_OP_BRANCH, l_merge);
    op1(m, &m->fn_body, SPV_OP_LABEL, l_merge);
}

static void emit_for_u32(spv_mod_t *m, uint32_t start_u32, uint32_t end_u32, spv_block_fn body_fn, void *ctx) {
    uint32_t ivar = local_var(m, m->t_ptr_fn_u32);
    store_var(m, ivar, start_u32);

    uint32_t l_hdr = spv_id(m), l_cond = spv_id(m), l_body = spv_id(m), l_cont = spv_id(m), l_after = spv_id(m);
    op1(m, &m->fn_body, SPV_OP_BRANCH, l_hdr);

    op1(m, &m->fn_body, SPV_OP_LABEL, l_hdr);
    op3(m, &m->fn_body, SPV_OP_LOOP_MERGE, l_after, l_cont, 0U);
    op1(m, &m->fn_body, SPV_OP_BRANCH, l_cond);

    op1(m, &m->fn_body, SPV_OP_LABEL, l_cond);
    uint32_t cur = load_u32(m, ivar);
    uint32_t cmp = emit_res4(m, &m->fn_body, SPV_OP_ULESS_THAN, m->t_bool, cur, end_u32);
    op3(m, &m->fn_body, SPV_OP_BRANCH_CONDITIONAL, cmp, l_body, l_after);

    op1(m, &m->fn_body, SPV_OP_LABEL, l_body);
    body_fn(m, ivar, ctx);
    op1(m, &m->fn_body, SPV_OP_BRANCH, l_cont);

    op1(m, &m->fn_body, SPV_OP_LABEL, l_cont);
    uint32_t cur2 = load_u32(m, ivar);
    uint32_t nxt = emit_res4(m, &m->fn_body, SPV_OP_IADD, m->t_u32, cur2, const_u32(m, 1U));
    store_var(m, ivar, nxt);
    op1(m, &m->fn_body, SPV_OP_BRANCH, l_hdr);

    op1(m, &m->fn_body, SPV_OP_LABEL, l_after);
}

static uint32_t emit_dequant_elem_f32(spv_mod_t *m, uint32_t op_id, uint32_t row, uint32_t col, uint32_t dim_k) {
    uint32_t off_a = push_u32(m, 12U);
    if (op_id == ZCC_OP_DEQUANT_Q8_0 || op_id == ZCC_OP_MATVEC_Q8_0) {
        uint32_t nb = emit_res4(m, &m->fn_body, SPV_OP_SHR_LOGICAL, m->t_u32, dim_k, const_u32(m, 5U));
        uint32_t b = emit_res4(m, &m->fn_body, SPV_OP_SHR_LOGICAL, m->t_u32, col, const_u32(m, 5U));
        uint32_t l = emit_res4(m, &m->fn_body, SPV_OP_BITWISE_AND, m->t_u32, col, const_u32(m, 31U));
        uint32_t rb = emit_res4(m, &m->fn_body, SPV_OP_IADD, m->t_u32,
                                emit_res4(m, &m->fn_body, SPV_OP_IMUL, m->t_u32, row, nb), b);
        uint32_t blk = emit_res4(m, &m->fn_body, SPV_OP_IADD, m->t_u32, off_a,
                                 emit_res4(m, &m->fn_body, SPV_OP_IMUL, m->t_u32, rb, const_u32(m, 34U)));
        uint32_t d = f16_u32_to_f32(m, load_u16_le(m, 0U, blk));
        uint32_t qaddr = emit_res4(m, &m->fn_body, SPV_OP_IADD, m->t_u32, blk,
                                   emit_res4(m, &m->fn_body, SPV_OP_IADD, m->t_u32, const_u32(m, 2U), l));
        uint32_t qf = u8_signed_to_f32(m, load_u8(m, 0U, qaddr));
        return emit_res4(m, &m->fn_body, SPV_OP_FMUL, m->t_f32, d, qf);
    }

    if (op_id == ZCC_OP_DEQUANT_Q4_0 || op_id == ZCC_OP_MATVEC_Q4_0) {
        uint32_t nb = emit_res4(m, &m->fn_body, SPV_OP_SHR_LOGICAL, m->t_u32, dim_k, const_u32(m, 5U));
        uint32_t b = emit_res4(m, &m->fn_body, SPV_OP_SHR_LOGICAL, m->t_u32, col, const_u32(m, 5U));
        uint32_t l = emit_res4(m, &m->fn_body, SPV_OP_BITWISE_AND, m->t_u32, col, const_u32(m, 31U));
        uint32_t rb = emit_res4(m, &m->fn_body, SPV_OP_IADD, m->t_u32,
                                emit_res4(m, &m->fn_body, SPV_OP_IMUL, m->t_u32, row, nb), b);
        uint32_t blk = emit_res4(m, &m->fn_body, SPV_OP_IADD, m->t_u32, off_a,
                                 emit_res4(m, &m->fn_body, SPV_OP_IMUL, m->t_u32, rb, const_u32(m, 18U)));
        uint32_t d = f16_u32_to_f32(m, load_u16_le(m, 0U, blk));
        uint32_t l15 = emit_res4(m, &m->fn_body, SPV_OP_BITWISE_AND, m->t_u32, l, const_u32(m, 15U));
        uint32_t qaddr = emit_res4(m, &m->fn_body, SPV_OP_IADD, m->t_u32, blk,
                                   emit_res4(m, &m->fn_body, SPV_OP_IADD, m->t_u32, const_u32(m, 2U), l15));
        uint32_t qb = load_u8(m, 0U, qaddr);
        uint32_t lo_nib = emit_res4(m, &m->fn_body, SPV_OP_BITWISE_AND, m->t_u32, qb, const_u32(m, 15U));
        uint32_t hi_nib = emit_res4(m, &m->fn_body, SPV_OP_SHR_LOGICAL, m->t_u32, qb, const_u32(m, 4U));
        uint32_t is_lo = emit_res4(m, &m->fn_body, SPV_OP_ULESS_THAN, m->t_bool, l, const_u32(m, 16U));
        uint32_t nib = emit_res5(m, &m->fn_body, SPV_OP_SELECT, m->t_u32, is_lo, lo_nib, hi_nib);
        uint32_t nib_s = emit_res3(m, &m->fn_body, SPV_OP_BITCAST, m->t_i32, nib);
        uint32_t qs = emit_res4(m, &m->fn_body, SPV_OP_ISUB, m->t_i32, nib_s, m->c_i32_8);
        uint32_t qf = emit_res3(m, &m->fn_body, SPV_OP_CONVERT_STOF, m->t_f32, qs);
        return emit_res4(m, &m->fn_body, SPV_OP_FMUL, m->t_f32, d, qf);
    }

    uint32_t nb = emit_res4(m, &m->fn_body, SPV_OP_SHR_LOGICAL, m->t_u32, dim_k, const_u32(m, 8U));
    uint32_t b = emit_res4(m, &m->fn_body, SPV_OP_SHR_LOGICAL, m->t_u32, col, const_u32(m, 8U));
    uint32_t l = emit_res4(m, &m->fn_body, SPV_OP_BITWISE_AND, m->t_u32, col, const_u32(m, 255U));
    uint32_t rb = emit_res4(m, &m->fn_body, SPV_OP_IADD, m->t_u32,
                            emit_res4(m, &m->fn_body, SPV_OP_IMUL, m->t_u32, row, nb), b);
    uint32_t blk = emit_res4(m, &m->fn_body, SPV_OP_IADD, m->t_u32, off_a,
                             emit_res4(m, &m->fn_body, SPV_OP_IMUL, m->t_u32, rb, const_u32(m, 144U)));
    uint32_t d = f16_u32_to_f32(m, load_u16_le(m, 0U, blk));
    uint32_t dmin = f16_u32_to_f32(m, load_u16_le(m, 0U, emit_res4(m, &m->fn_body, SPV_OP_IADD, m->t_u32, blk, const_u32(m, 2U))));

    uint32_t chunk = emit_res4(m, &m->fn_body, SPV_OP_SHR_LOGICAL, m->t_u32, l, const_u32(m, 6U));
    uint32_t in_c = emit_res4(m, &m->fn_body, SPV_OP_BITWISE_AND, m->t_u32, l, const_u32(m, 63U));
    uint32_t sub = emit_res4(m, &m->fn_body, SPV_OP_SHR_LOGICAL, m->t_u32, in_c, const_u32(m, 5U));
    uint32_t idx32 = emit_res4(m, &m->fn_body, SPV_OP_BITWISE_AND, m->t_u32, in_c, const_u32(m, 31U));
    uint32_t j = emit_res4(m, &m->fn_body, SPV_OP_IADD, m->t_u32,
                           emit_res4(m, &m->fn_body, SPV_OP_SHL, m->t_u32, chunk, const_u32(m, 1U)), sub);

    uint32_t sc_var = local_var(m, m->t_ptr_fn_u32);
    uint32_t mn_var = local_var(m, m->t_ptr_fn_u32);
    uint32_t scales_base = emit_res4(m, &m->fn_body, SPV_OP_IADD, m->t_u32, blk, const_u32(m, 4U));
    uint32_t is_j_lt4 = emit_res4(m, &m->fn_body, SPV_OP_ULESS_THAN, m->t_bool, j, const_u32(m, 4U));

    uint32_t l_jlt = spv_id(m), l_jge = spv_id(m), l_jm = spv_id(m);
    op2(m, &m->fn_body, SPV_OP_SELECTION_MERGE, l_jm, 0U);
    op3(m, &m->fn_body, SPV_OP_BRANCH_CONDITIONAL, is_j_lt4, l_jlt, l_jge);

    op1(m, &m->fn_body, SPV_OP_LABEL, l_jlt);
    uint32_t s_j = load_u8(m, 0U, emit_res4(m, &m->fn_body, SPV_OP_IADD, m->t_u32, scales_base, j));
    uint32_t s_j4 = load_u8(m, 0U, emit_res4(m, &m->fn_body, SPV_OP_IADD, m->t_u32, scales_base,
                                             emit_res4(m, &m->fn_body, SPV_OP_IADD, m->t_u32, j, const_u32(m, 4U))));
    store_var(m, sc_var, emit_res4(m, &m->fn_body, SPV_OP_BITWISE_AND, m->t_u32, s_j, const_u32(m, 63U)));
    store_var(m, mn_var, emit_res4(m, &m->fn_body, SPV_OP_BITWISE_AND, m->t_u32, s_j4, const_u32(m, 63U)));
    op1(m, &m->fn_body, SPV_OP_BRANCH, l_jm);

    op1(m, &m->fn_body, SPV_OP_LABEL, l_jge);
    uint32_t sj = load_u8(m, 0U, emit_res4(m, &m->fn_body, SPV_OP_IADD, m->t_u32, scales_base, j));
    uint32_t sj4 = load_u8(m, 0U, emit_res4(m, &m->fn_body, SPV_OP_IADD, m->t_u32, scales_base,
                                            emit_res4(m, &m->fn_body, SPV_OP_IADD, m->t_u32, j, const_u32(m, 4U))));
    uint32_t sjm4 = load_u8(m, 0U, emit_res4(m, &m->fn_body, SPV_OP_IADD, m->t_u32, scales_base,
                                             emit_res4(m, &m->fn_body, SPV_OP_ISUB, m->t_u32, j, const_u32(m, 4U))));
    uint32_t sc_lo = emit_res4(m, &m->fn_body, SPV_OP_BITWISE_AND, m->t_u32, sj4, const_u32(m, 15U));
    uint32_t sc_hi = emit_res4(m, &m->fn_body, SPV_OP_SHL, m->t_u32,
                               emit_res4(m, &m->fn_body, SPV_OP_SHR_LOGICAL, m->t_u32, sjm4, const_u32(m, 6U)),
                               const_u32(m, 4U));
    uint32_t mn_lo = emit_res4(m, &m->fn_body, SPV_OP_SHR_LOGICAL, m->t_u32, sj4, const_u32(m, 4U));
    uint32_t mn_hi = emit_res4(m, &m->fn_body, SPV_OP_SHL, m->t_u32,
                               emit_res4(m, &m->fn_body, SPV_OP_SHR_LOGICAL, m->t_u32, sj, const_u32(m, 6U)),
                               const_u32(m, 4U));
    store_var(m, sc_var, emit_res4(m, &m->fn_body, SPV_OP_BITWISE_OR, m->t_u32, sc_lo, sc_hi));
    store_var(m, mn_var, emit_res4(m, &m->fn_body, SPV_OP_BITWISE_OR, m->t_u32, mn_lo, mn_hi));
    op1(m, &m->fn_body, SPV_OP_BRANCH, l_jm);

    op1(m, &m->fn_body, SPV_OP_LABEL, l_jm);
    uint32_t sc = load_u32(m, sc_var);
    uint32_t mn = load_u32(m, mn_var);
    uint32_t qs_off = emit_res4(m, &m->fn_body, SPV_OP_IADD, m->t_u32,
                                emit_res4(m, &m->fn_body, SPV_OP_IADD, m->t_u32, blk, const_u32(m, 16U)),
                                emit_res4(m, &m->fn_body, SPV_OP_IADD, m->t_u32,
                                          emit_res4(m, &m->fn_body, SPV_OP_SHL, m->t_u32, chunk, const_u32(m, 5U)), idx32));
    uint32_t qbyte = load_u8(m, 0U, qs_off);
    uint32_t qlo = emit_res4(m, &m->fn_body, SPV_OP_BITWISE_AND, m->t_u32, qbyte, const_u32(m, 15U));
    uint32_t qhi = emit_res4(m, &m->fn_body, SPV_OP_SHR_LOGICAL, m->t_u32, qbyte, const_u32(m, 4U));
    uint32_t sub0 = emit_res4(m, &m->fn_body, SPV_OP_IEQUAL, m->t_bool, sub, const_u32(m, 0U));
    uint32_t q = emit_res5(m, &m->fn_body, SPV_OP_SELECT, m->t_u32, sub0, qlo, qhi);

    uint32_t d_sc = emit_res4(m, &m->fn_body, SPV_OP_FMUL, m->t_f32, d, u32_to_f32(m, sc));
    uint32_t m_mn = emit_res4(m, &m->fn_body, SPV_OP_FMUL, m->t_f32, dmin, u32_to_f32(m, mn));
    uint32_t term1 = emit_res4(m, &m->fn_body, SPV_OP_FMUL, m->t_f32, d_sc, u32_to_f32(m, q));
    return emit_res4(m, &m->fn_body, SPV_OP_FSUB, m->t_f32, term1, m_mn);
}

typedef struct k_ctx_s {
    uint32_t op_id;
    uint32_t idx;
    uint32_t row;
    uint32_t col;
    uint32_t dim_m;
    uint32_t dim_n;
    uint32_t dim_k;
    uint32_t acc_var;
    uint32_t max_var;
    uint32_t inv_var;
    uint32_t sum_var;
} k_ctx_t;

static void body_vec_op(spv_mod_t *m, void *vctx) {
    k_ctx_t *ctx = (k_ctx_t *)vctx;
    uint32_t a_byte = addr_1d(m, push_u32(m, 12U), ctx->idx, push_u32(m, 4U));
    uint32_t b_byte = addr_1d(m, push_u32(m, 13U), ctx->idx, push_u32(m, 6U));
    uint32_t o_byte = addr_1d(m, push_u32(m, 15U), ctx->idx, push_u32(m, 10U));
    uint32_t va = load_f32_byte(m, 0U, a_byte);
    uint32_t vb = load_f32_byte(m, 1U, b_byte);
    uint32_t res = 0U;
    if (ctx->op_id == ZCC_OP_VEC_ADD_F32) {
        res = emit_res4(m, &m->fn_body, SPV_OP_FADD, m->t_f32, va, vb);
    } else if (ctx->op_id == ZCC_OP_VEC_MUL_F32) {
        res = emit_res4(m, &m->fn_body, SPV_OP_FMUL, m->t_f32, va, vb);
    } else {
        uint32_t c_byte = addr_1d(m, push_u32(m, 14U), ctx->idx, push_u32(m, 8U));
        uint32_t vc = load_f32_byte(m, 2U, c_byte);
        res = glsl3(m, m->t_f32, GLSL_FMA, va, vb, vc);
    }
    store_f32_at_byte(m, o_byte, res);
}

static void loop_dot_step(spv_mod_t *m, uint32_t ivar, void *vctx) {
    k_ctx_t *ctx = (k_ctx_t *)vctx;
    uint32_t k = load_u32(m, ivar);
    uint32_t va = load_f32_byte(m, 0U, addr_1d(m, push_u32(m, 12U), k, push_u32(m, 4U)));
    uint32_t vb = load_f32_byte(m, 1U, addr_1d(m, push_u32(m, 13U), k, push_u32(m, 6U)));
    uint32_t cur = load_f32(m, ctx->acc_var);
    store_var(m, ctx->acc_var, glsl3(m, m->t_f32, GLSL_FMA, va, vb, cur));
}

static void body_dot_op(spv_mod_t *m, void *vctx) {
    k_ctx_t *ctx = (k_ctx_t *)vctx;
    store_var(m, ctx->acc_var, const_f32_bits(m, 0U));
    emit_for_u32(m, const_u32(m, 0U), ctx->dim_k, loop_dot_step, ctx);
    store_f32_at_byte(m, push_u32(m, 15U), load_f32(m, ctx->acc_var));
}

static void loop_gemm_step(spv_mod_t *m, uint32_t ivar, void *vctx) {
    k_ctx_t *ctx = (k_ctx_t *)vctx;
    uint32_t k = load_u32(m, ivar);
    uint32_t a_addr = addr_2d(m, push_u32(m, 12U), ctx->row, k, push_u32(m, 4U), push_u32(m, 5U));
    uint32_t b_addr = addr_2d(m, push_u32(m, 13U), k, ctx->col, push_u32(m, 6U), push_u32(m, 7U));
    uint32_t va = load_f32_byte(m, 0U, a_addr);
    uint32_t vb = load_f32_byte(m, 1U, b_addr);
    uint32_t cur = load_f32(m, ctx->acc_var);
    store_var(m, ctx->acc_var, glsl3(m, m->t_f32, GLSL_FMA, va, vb, cur));
}

static void body_gemm_op(spv_mod_t *m, void *vctx) {
    k_ctx_t *ctx = (k_ctx_t *)vctx;
    store_var(m, ctx->acc_var, const_f32_bits(m, 0U));
    emit_for_u32(m, const_u32(m, 0U), ctx->dim_k, loop_gemm_step, ctx);
    uint32_t o_addr = addr_2d(m, push_u32(m, 15U), ctx->row, ctx->col, push_u32(m, 10U), push_u32(m, 11U));
    store_f32_at_byte(m, o_addr, load_f32(m, ctx->acc_var));
}

static void loop_rms_pass1(spv_mod_t *m, uint32_t ivar, void *vctx) {
    k_ctx_t *ctx = (k_ctx_t *)vctx;
    uint32_t j = load_u32(m, ivar);
    uint32_t vx = load_f32_byte(m, 0U, addr_2d(m, push_u32(m, 12U), ctx->row, j, push_u32(m, 4U), push_u32(m, 5U)));
    uint32_t cur = load_f32(m, ctx->acc_var);
    store_var(m, ctx->acc_var, glsl3(m, m->t_f32, GLSL_FMA, vx, vx, cur));
}

static void loop_rms_pass2(spv_mod_t *m, uint32_t ivar, void *vctx) {
    k_ctx_t *ctx = (k_ctx_t *)vctx;
    uint32_t j = load_u32(m, ivar);
    uint32_t vx = load_f32_byte(m, 0U, addr_2d(m, push_u32(m, 12U), ctx->row, j, push_u32(m, 4U), push_u32(m, 5U)));
    uint32_t vw = load_f32_byte(m, 1U, addr_1d(m, push_u32(m, 13U), j, push_u32(m, 6U)));
    uint32_t scaled = emit_res4(m, &m->fn_body, SPV_OP_FMUL, m->t_f32, vx, load_f32(m, ctx->inv_var));
    uint32_t res = emit_res4(m, &m->fn_body, SPV_OP_FMUL, m->t_f32, scaled, vw);
    uint32_t o_addr = addr_2d(m, push_u32(m, 15U), ctx->row, j, push_u32(m, 10U), push_u32(m, 11U));
    store_f32_at_byte(m, o_addr, res);
}

static void body_rmsnorm_op(spv_mod_t *m, void *vctx) {
    k_ctx_t *ctx = (k_ctx_t *)vctx;
    store_var(m, ctx->acc_var, const_f32_bits(m, 0U));
    emit_for_u32(m, const_u32(m, 0U), ctx->dim_n, loop_rms_pass1, ctx);
    uint32_t ss = load_f32(m, ctx->acc_var);
    uint32_t mean = emit_res4(m, &m->fn_body, SPV_OP_FDIV, m->t_f32, ss, u32_to_f32(m, ctx->dim_n));
    uint32_t eps = emit_res3(m, &m->fn_body, SPV_OP_BITCAST, m->t_f32, push_u32(m, 3U));
    uint32_t var_eps = emit_res4(m, &m->fn_body, SPV_OP_FADD, m->t_f32, mean, eps);
    uint32_t denom = glsl1(m, m->t_f32, GLSL_SQRT, var_eps);
    uint32_t inv = emit_res4(m, &m->fn_body, SPV_OP_FDIV, m->t_f32, const_f32_bits(m, 0x3F800000U), denom);
    store_var(m, ctx->inv_var, inv);
    emit_for_u32(m, const_u32(m, 0U), ctx->dim_n, loop_rms_pass2, ctx);
}

static void loop_soft_max(spv_mod_t *m, uint32_t ivar, void *vctx) {
    k_ctx_t *ctx = (k_ctx_t *)vctx;
    uint32_t j = load_u32(m, ivar);
    uint32_t v = load_f32_byte(m, 0U, addr_2d(m, push_u32(m, 12U), ctx->row, j, push_u32(m, 4U), push_u32(m, 5U)));
    uint32_t cur = load_f32(m, ctx->max_var);
    uint32_t v_nan = emit_res3(m, &m->fn_body, SPV_OP_IS_NAN, m->t_bool, v);
    uint32_t m_nan = emit_res3(m, &m->fn_body, SPV_OP_IS_NAN, m->t_bool, cur);
    uint32_t m_ok  = emit_res3(m, &m->fn_body, SPV_OP_LOGICAL_NOT, m->t_bool, m_nan);
    uint32_t v_gt  = emit_res4(m, &m->fn_body, SPV_OP_FORD_GREATER_THAN, m->t_bool, v, cur);
    uint32_t take_gt = emit_res4(m, &m->fn_body, SPV_OP_LOGICAL_AND, m->t_bool, m_ok, v_gt);
    uint32_t take_v  = emit_res4(m, &m->fn_body, SPV_OP_LOGICAL_OR, m->t_bool, v_nan, take_gt);
    store_var(m, ctx->max_var, emit_res5(m, &m->fn_body, SPV_OP_SELECT, m->t_f32, take_v, v, cur));
}

static void loop_soft_exp(spv_mod_t *m, uint32_t ivar, void *vctx) {
    k_ctx_t *ctx = (k_ctx_t *)vctx;
    uint32_t j = load_u32(m, ivar);
    uint32_t v = load_f32_byte(m, 0U, addr_2d(m, push_u32(m, 12U), ctx->row, j, push_u32(m, 4U), push_u32(m, 5U)));
    uint32_t diff = emit_res4(m, &m->fn_body, SPV_OP_FSUB, m->t_f32, v, load_f32(m, ctx->max_var));
    uint32_t ev = glsl1(m, m->t_f32, GLSL_EXP, diff);
    uint32_t o_addr = addr_2d(m, push_u32(m, 15U), ctx->row, j, push_u32(m, 10U), push_u32(m, 11U));
    store_f32_at_byte(m, o_addr, ev);
    store_var(m, ctx->sum_var, emit_res4(m, &m->fn_body, SPV_OP_FADD, m->t_f32, load_f32(m, ctx->sum_var), ev));
}

static void loop_soft_div(spv_mod_t *m, uint32_t ivar, void *vctx) {
    k_ctx_t *ctx = (k_ctx_t *)vctx;
    uint32_t j = load_u32(m, ivar);
    uint32_t o_addr = addr_2d(m, push_u32(m, 15U), ctx->row, j, push_u32(m, 10U), push_u32(m, 11U));
    uint32_t ev = load_f32_at_byte_out(m, o_addr);
    store_f32_at_byte(m, o_addr, emit_res4(m, &m->fn_body, SPV_OP_FDIV, m->t_f32, ev, load_f32(m, ctx->sum_var)));
}

static void body_softmax_op(spv_mod_t *m, void *vctx) {
    k_ctx_t *ctx = (k_ctx_t *)vctx;
    uint32_t x0 = load_f32_byte(m, 0U, addr_2d(m, push_u32(m, 12U), ctx->row, const_u32(m, 0U),
                                                push_u32(m, 4U), push_u32(m, 5U)));
    store_var(m, ctx->max_var, x0);
    emit_for_u32(m, const_u32(m, 1U), ctx->dim_n, loop_soft_max, ctx);
    store_var(m, ctx->sum_var, const_f32_bits(m, 0U));
    emit_for_u32(m, const_u32(m, 0U), ctx->dim_n, loop_soft_exp, ctx);
    emit_for_u32(m, const_u32(m, 0U), ctx->dim_n, loop_soft_div, ctx);
}

static void body_dequant_op(spv_mod_t *m, void *vctx) {
    k_ctx_t *ctx = (k_ctx_t *)vctx;
    uint32_t val = emit_dequant_elem_f32(m, ctx->op_id, ctx->row, ctx->col, ctx->dim_k);
    uint32_t o_addr = addr_2d(m, push_u32(m, 15U), ctx->row, ctx->col, push_u32(m, 10U), push_u32(m, 11U));
    store_f32_at_byte(m, o_addr, val);
}

static void loop_matvec_step(spv_mod_t *m, uint32_t ivar, void *vctx) {
    k_ctx_t *ctx = (k_ctx_t *)vctx;
    uint32_t col = load_u32(m, ivar);
    uint32_t wval = emit_dequant_elem_f32(m, ctx->op_id, ctx->row, col, ctx->dim_k);
    uint32_t xval = load_f32_byte(m, 1U, addr_1d(m, push_u32(m, 13U), col, push_u32(m, 6U)));
    store_var(m, ctx->acc_var, glsl3(m, m->t_f32, GLSL_FMA, wval, xval, load_f32(m, ctx->acc_var)));
}

static void body_matvec_op(spv_mod_t *m, void *vctx) {
    k_ctx_t *ctx = (k_ctx_t *)vctx;
    store_var(m, ctx->acc_var, const_f32_bits(m, 0U));
    emit_for_u32(m, const_u32(m, 0U), ctx->dim_k, loop_matvec_step, ctx);
    uint32_t o_addr = addr_1d(m, push_u32(m, 15U), ctx->row, push_u32(m, 10U));
    store_f32_at_byte(m, o_addr, load_f32(m, ctx->acc_var));
}

static void spv_mod_init(spv_mod_t *m, uint32_t lx, uint32_t ly, uint32_t lz) {
    memset(m, 0, sizeof(*m));
    m->next_id = 1U;

    uint32_t cap = SPV_CAP_SHADER;
    emit_op(m, &m->pre, SPV_OP_CAPABILITY, 1, &cap);
    m->id_glsl = spv_id(m);
    emit_op_str(m, &m->pre, SPV_OP_EXT_INST_IMPORT, 1, &m->id_glsl, "GLSL.std.450", 0, NULL);
    op2(m, &m->pre, SPV_OP_MEMORY_MODEL, SPV_ADDR_LOGICAL, SPV_MEM_GLSL450);

    m->t_void  = spv_id(m); emit_op(m, &m->types, SPV_OP_TYPE_VOID, 1, &m->t_void);
    m->t_bool  = spv_id(m); emit_op(m, &m->types, SPV_OP_TYPE_BOOL, 1, &m->t_bool);
    m->t_u32   = spv_id(m); op3(m, &m->types, SPV_OP_TYPE_INT, m->t_u32, 32U, 0U);
    m->t_i32   = spv_id(m); op3(m, &m->types, SPV_OP_TYPE_INT, m->t_i32, 32U, 1U);
    m->t_f32   = spv_id(m); op2(m, &m->types, SPV_OP_TYPE_FLOAT, m->t_f32, 32U);
    m->t_v3u32 = spv_id(m); op3(m, &m->types, SPV_OP_TYPE_VECTOR, m->t_v3u32, m->t_u32, 3U);
    m->t_v2f32 = spv_id(m); op3(m, &m->types, SPV_OP_TYPE_VECTOR, m->t_v2f32, m->t_f32, 2U);

    m->t_rt_u32 = spv_id(m); op2(m, &m->types, SPV_OP_TYPE_RUNTIME_ARRAY, m->t_rt_u32, m->t_u32);
    op3(m, &m->annot, SPV_OP_DECORATE, m->t_rt_u32, SPV_DEC_ARRAY_STRIDE, 4U);

    m->t_ssbo = spv_id(m); op2(m, &m->types, SPV_OP_TYPE_STRUCT, m->t_ssbo, m->t_rt_u32);
    op2(m, &m->annot, SPV_OP_DECORATE, m->t_ssbo, SPV_DEC_BUFFER_BLOCK);
    op4(m, &m->annot, SPV_OP_MEMBER_DECORATE, m->t_ssbo, 0U, SPV_DEC_OFFSET, 0U);

    m->t_push = spv_id(m);
    uint32_t pfields[17];
    pfields[0] = m->t_push;
    for (uint32_t i = 0; i < 16U; i++) {
        pfields[1U + i] = m->t_u32;
        op4(m, &m->annot, SPV_OP_MEMBER_DECORATE, m->t_push, i, SPV_DEC_OFFSET, i * 4U);
    }
    emit_op(m, &m->types, SPV_OP_TYPE_STRUCT, 17U, pfields);
    op2(m, &m->annot, SPV_OP_DECORATE, m->t_push, SPV_DEC_BLOCK);

    m->t_ptr_unif_ssbo   = spv_id(m); op3(m, &m->types, SPV_OP_TYPE_POINTER, m->t_ptr_unif_ssbo,   SPV_SC_UNIFORM,       m->t_ssbo);
    m->t_ptr_unif_u32    = spv_id(m); op3(m, &m->types, SPV_OP_TYPE_POINTER, m->t_ptr_unif_u32,    SPV_SC_UNIFORM,       m->t_u32);
    m->t_ptr_push_struct = spv_id(m); op3(m, &m->types, SPV_OP_TYPE_POINTER, m->t_ptr_push_struct, SPV_SC_PUSH_CONSTANT, m->t_push);
    m->t_ptr_push_u32    = spv_id(m); op3(m, &m->types, SPV_OP_TYPE_POINTER, m->t_ptr_push_u32,    SPV_SC_PUSH_CONSTANT, m->t_u32);
    m->t_ptr_input_v3u32 = spv_id(m); op3(m, &m->types, SPV_OP_TYPE_POINTER, m->t_ptr_input_v3u32, SPV_SC_INPUT,         m->t_v3u32);
    m->t_ptr_input_u32   = spv_id(m); op3(m, &m->types, SPV_OP_TYPE_POINTER, m->t_ptr_input_u32,   SPV_SC_INPUT,         m->t_u32);
    m->t_ptr_fn_u32      = spv_id(m); op3(m, &m->types, SPV_OP_TYPE_POINTER, m->t_ptr_fn_u32,      SPV_SC_FUNCTION,      m->t_u32);
    m->t_ptr_fn_f32      = spv_id(m); op3(m, &m->types, SPV_OP_TYPE_POINTER, m->t_ptr_fn_f32,      SPV_SC_FUNCTION,      m->t_f32);
    m->t_fn_void         = spv_id(m); op2(m, &m->types, SPV_OP_TYPE_FUNCTION, m->t_fn_void,        m->t_void);

    m->c_i32_8  = spv_id(m); op3(m, &m->types, SPV_OP_CONSTANT, m->t_i32, m->c_i32_8, 8U);
    m->c_i32_24 = spv_id(m); op3(m, &m->types, SPV_OP_CONSTANT, m->t_i32, m->c_i32_24, 24U);

    for (uint32_t b = 0U; b < 4U; b++) {
        m->var_buf[b] = spv_id(m);
        op3(m, &m->types, SPV_OP_VARIABLE, m->t_ptr_unif_ssbo, m->var_buf[b], SPV_SC_UNIFORM);
        op3(m, &m->annot, SPV_OP_DECORATE, m->var_buf[b], SPV_DEC_DESCRIPTOR_SET, 0U);
        op3(m, &m->annot, SPV_OP_DECORATE, m->var_buf[b], SPV_DEC_BINDING, b);
        if (b < 3U) {
            op2(m, &m->annot, SPV_OP_DECORATE, m->var_buf[b], SPV_DEC_NON_WRITABLE);
        }
    }
    m->var_push = spv_id(m); op3(m, &m->types, SPV_OP_VARIABLE, m->t_ptr_push_struct, m->var_push, SPV_SC_PUSH_CONSTANT);
    m->var_gid  = spv_id(m); op3(m, &m->types, SPV_OP_VARIABLE, m->t_ptr_input_v3u32, m->var_gid,  SPV_SC_INPUT);
    op3(m, &m->annot, SPV_OP_DECORATE, m->var_gid, SPV_DEC_BUILTIN, SPV_BI_GLOBAL_INVOCATION_ID);

    m->fn_main = spv_id(m);
    uint32_t ep_pre[2] = {SPV_EXEC_GLCOMPUTE, m->fn_main};
    emit_op_str(m, &m->entry, SPV_OP_ENTRY_POINT, 2, ep_pre, "main", 1, &m->var_gid);
    uint32_t em[5] = {m->fn_main, SPV_MODE_LOCAL_SIZE, lx, ly, lz};
    emit_op(m, &m->entry, SPV_OP_EXECUTION_MODE, 5, em);
    emit_op_str(m, &m->debug, SPV_OP_NAME, 1, &m->fn_main, "main", 0, NULL);

    op4(m, &m->fn_head, SPV_OP_FUNCTION, m->t_void, m->fn_main, 0U, m->t_fn_void);
    op1(m, &m->fn_head, SPV_OP_LABEL, spv_id(m));
}

static void spv_mod_free_bufs(spv_mod_t *m) {
    free(m->pre.words); free(m->entry.words); free(m->debug.words);
    free(m->annot.words); free(m->types.words); free(m->fn_head.words);
    free(m->fn_body.words);
}

static int spv_mod_finalize(spv_mod_t *m, uint32_t lx, uint32_t ly, uint32_t lz, zcc_spv_module_t *out) {
    emit_op(m, &m->fn_body, SPV_OP_RETURN, 0, NULL);
    emit_op(m, &m->fn_body, SPV_OP_FUNCTION_END, 0, NULL);
    if (m->oom) { spv_mod_free_bufs(m); return -1; }

    size_t total_words = 5U + m->pre.len + m->entry.len + m->debug.len +
                         m->annot.len + m->types.len + m->fn_head.len + m->fn_body.len;
    uint32_t *words = (uint32_t *)malloc(total_words * sizeof(uint32_t));
    if (!words) { spv_mod_free_bufs(m); return -1; }

    size_t pos = 0U;
    words[pos++] = ZCC_SPIRV_MAGIC_NUMBER;
    words[pos++] = ZCC_SPIRV_VERSION_1_0;
    words[pos++] = ZCC_SPIRV_GENERATOR_ID;
    words[pos++] = m->next_id;
    words[pos++] = 0U;

    const spv_buf_t *secs[7] = {&m->pre, &m->entry, &m->debug, &m->annot, &m->types, &m->fn_head, &m->fn_body};
    for (size_t s = 0U; s < 7U; s++) {
        if (secs[s]->len > 0U) {
            memcpy(words + pos, secs[s]->words, secs[s]->len * sizeof(uint32_t));
            pos += secs[s]->len;
        }
    }
    spv_mod_free_bufs(m);

    out->words = words;
    out->word_count = total_words;
    out->byte_size = total_words * sizeof(uint32_t);
    out->local_x = lx;
    out->local_y = ly;
    out->local_z = lz;
    snprintf(out->entry_name, sizeof(out->entry_name), "main");
    return 0;
}

static void local_sizes_for_op(uint32_t op_id, uint32_t *lx, uint32_t *ly, uint32_t *lz) {
    *lz = 1U;
    if (op_id == ZCC_OP_DOT_F32) {
        *lx = 1U; *ly = 1U;
    } else if (op_id == ZCC_OP_GEMM_F32 ||
               op_id == ZCC_OP_DEQUANT_Q8_0 ||
               op_id == ZCC_OP_DEQUANT_Q4_0 ||
               op_id == ZCC_OP_DEQUANT_Q4_K) {
        *lx = 8U; *ly = 8U;
    } else {
        *lx = 64U; *ly = 1U;
    }
}

void zcc_spirv_dispatch_grid(uint32_t op_id, uint32_t m, uint32_t n, uint32_t k,
                             uint32_t *out_gx, uint32_t *out_gy, uint32_t *out_gz) {
    uint32_t lx = 64U, ly = 1U, lz = 1U;
    local_sizes_for_op(op_id, &lx, &ly, &lz);
    *out_gz = 1U;
    if (op_id == ZCC_OP_DOT_F32) {
        *out_gx = 1U; *out_gy = 1U;
    } else if (op_id == ZCC_OP_GEMM_F32) {
        *out_gx = (n + lx - 1U) / lx;
        *out_gy = (m + ly - 1U) / ly;
    } else if (op_id == ZCC_OP_DEQUANT_Q8_0 || op_id == ZCC_OP_DEQUANT_Q4_0 || op_id == ZCC_OP_DEQUANT_Q4_K) {
        *out_gx = (k + lx - 1U) / lx;
        *out_gy = (m + ly - 1U) / ly;
    } else {
        *out_gx = (m + lx - 1U) / lx;
        *out_gy = 1U;
    }
    if (*out_gx == 0U) *out_gx = 1U;
    if (*out_gy == 0U) *out_gy = 1U;
}

int zcc_spirv_emit_op_kernel(uint32_t op_id, zcc_spv_module_t *out_mod) {
    if (!out_mod || op_id < ZCC_OP_VEC_ADD_F32 || op_id > ZCC_OP_MATVEC_Q4_K) {
        return -1;
    }
    memset(out_mod, 0, sizeof(*out_mod));

    uint32_t lx = 64U, ly = 1U, lz = 1U;
    local_sizes_for_op(op_id, &lx, &ly, &lz);

    spv_mod_t mod;
    spv_mod_init(&mod, lx, ly, lz);

    k_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.op_id = op_id;
    ctx.dim_m = push_u32(&mod, 0U);
    ctx.dim_n = push_u32(&mod, 1U);
    ctx.dim_k = push_u32(&mod, 2U);
    ctx.acc_var = local_var(&mod, mod.t_ptr_fn_f32);
    ctx.max_var = local_var(&mod, mod.t_ptr_fn_f32);
    ctx.inv_var = local_var(&mod, mod.t_ptr_fn_f32);
    ctx.sum_var = local_var(&mod, mod.t_ptr_fn_f32);

    if (op_id == ZCC_OP_VEC_ADD_F32 || op_id == ZCC_OP_VEC_MUL_F32 || op_id == ZCC_OP_VEC_FMA_F32) {
        ctx.idx = gid_comp(&mod, 0U);
        uint32_t cond = emit_res4(&mod, &mod.fn_body, SPV_OP_ULESS_THAN, mod.t_bool, ctx.idx, ctx.dim_m);
        emit_if(&mod, cond, body_vec_op, &ctx);
    } else if (op_id == ZCC_OP_DOT_F32) {
        ctx.idx = gid_comp(&mod, 0U);
        uint32_t cond = emit_res4(&mod, &mod.fn_body, SPV_OP_IEQUAL, mod.t_bool, ctx.idx, const_u32(&mod, 0U));
        emit_if(&mod, cond, body_dot_op, &ctx);
    } else if (op_id == ZCC_OP_GEMM_F32) {
        ctx.col = gid_comp(&mod, 0U);
        ctx.row = gid_comp(&mod, 1U);
        uint32_t cr = emit_res4(&mod, &mod.fn_body, SPV_OP_ULESS_THAN, mod.t_bool, ctx.row, ctx.dim_m);
        uint32_t cc = emit_res4(&mod, &mod.fn_body, SPV_OP_ULESS_THAN, mod.t_bool, ctx.col, ctx.dim_n);
        uint32_t cond = emit_res4(&mod, &mod.fn_body, SPV_OP_LOGICAL_AND, mod.t_bool, cr, cc);
        emit_if(&mod, cond, body_gemm_op, &ctx);
    } else if (op_id == ZCC_OP_RMSNORM_F32) {
        ctx.row = gid_comp(&mod, 0U);
        uint32_t cond = emit_res4(&mod, &mod.fn_body, SPV_OP_ULESS_THAN, mod.t_bool, ctx.row, ctx.dim_m);
        emit_if(&mod, cond, body_rmsnorm_op, &ctx);
    } else if (op_id == ZCC_OP_SOFTMAX_F32) {
        ctx.row = gid_comp(&mod, 0U);
        uint32_t cond = emit_res4(&mod, &mod.fn_body, SPV_OP_ULESS_THAN, mod.t_bool, ctx.row, ctx.dim_m);
        emit_if(&mod, cond, body_softmax_op, &ctx);
    } else if (op_id == ZCC_OP_DEQUANT_Q8_0 || op_id == ZCC_OP_DEQUANT_Q4_0 || op_id == ZCC_OP_DEQUANT_Q4_K) {
        ctx.col = gid_comp(&mod, 0U);
        ctx.row = gid_comp(&mod, 1U);
        uint32_t cr = emit_res4(&mod, &mod.fn_body, SPV_OP_ULESS_THAN, mod.t_bool, ctx.row, ctx.dim_m);
        uint32_t cc = emit_res4(&mod, &mod.fn_body, SPV_OP_ULESS_THAN, mod.t_bool, ctx.col, ctx.dim_k);
        uint32_t cond = emit_res4(&mod, &mod.fn_body, SPV_OP_LOGICAL_AND, mod.t_bool, cr, cc);
        emit_if(&mod, cond, body_dequant_op, &ctx);
    } else {
        ctx.row = gid_comp(&mod, 0U);
        uint32_t cond = emit_res4(&mod, &mod.fn_body, SPV_OP_ULESS_THAN, mod.t_bool, ctx.row, ctx.dim_m);
        emit_if(&mod, cond, body_matvec_op, &ctx);
    }

    return spv_mod_finalize(&mod, lx, ly, lz, out_mod);
}

void zcc_spirv_module_free(zcc_spv_module_t *mod) {
    if (!mod) return;
    free(mod->words);
    memset(mod, 0, sizeof(*mod));
}

int zcc_spirv_write_file(const char *path, const zcc_spv_module_t *mod) {
    if (!path || !mod || !mod->words || mod->byte_size == 0U) return -1;
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    size_t nw = fwrite(mod->words, 1U, mod->byte_size, fp);
    int rc = fclose(fp);
    return (nw == mod->byte_size && rc == 0) ? 0 : -1;
}
