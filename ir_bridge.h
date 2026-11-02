/* ================================================================ */
/* ir_bridge.h — ZCC IR Bridge Infrastructure                        */
/*                                                                   */
/* Translation layer between register-machine x86-64 codegen and     */
/* 3-address SSA-style IR emission.                                  */
/*                                                                   */
/* Include this ONCE at the top of part4.c, after ir_emit_dispatch.h */
/* is already visible (it comes through part5.c concatenation).      */
/*                                                                   */
/* CRITICAL: All types here must be ZCC-parseable.                   */
/*   - No stdint.h, no _Static_assert, no __attribute__              */
/*   - No designated initializers, no compound literals              */
/* ================================================================ */

#ifndef ZCC_IR_BRIDGE_ALLOWED
#error "ir_bridge.h is a header-hosted implementation file with mutable file-scope state. It may only be included inside part4.c, wrapped in #define/#undef ZCC_IR_BRIDGE_ALLOWED."
#endif

#ifdef ZCC_IR_BRIDGE_H
#error "ir_bridge.h included twice in one translation unit — its static state would be duplicated."
#else
#define ZCC_IR_BRIDGE_H
#include <stdio.h>

/* ── IR Temporary Name Generator ─────────────────────────────────── */
/*                                                                     */
/* The x86-64 backend leaves results in %rax. The IR needs named       */
/* temporaries (%t0, %t1, ...). This counter generates unique names.   */
/* Reset at each function boundary via ir_bridge_func_begin().         */

static int ir_tmp_counter;
static char ir_tmp_buf[32];         /* scratch for sprintf */
static char ir_last_result[32];     /* name of the last IR temp produced */
static int ir_bridge_active;        /* 1 = emit IR alongside asm */

static void ir_bridge_reset(void) {
    ir_tmp_counter = 0;
    ir_last_result[0] = 0;
    ir_bridge_active = 1;
}

static char *ir_bridge_fresh_tmp(void) {
    /* Returns a new temporary name like "%t0", "%t1", etc.
     * Also copies into ir_last_result so callers can reference it. */
    sprintf(ir_tmp_buf, "%%t%d", ir_tmp_counter);
    ir_tmp_counter++;
    /* copy to ir_last_result */
    {
        int i;
        for (i = 0; ir_tmp_buf[i]; i++)
            ir_last_result[i] = ir_tmp_buf[i];
        ir_last_result[i] = 0;
    }
    return ir_tmp_buf;
}

/* Snapshot the current ir_last_result into a caller-owned buffer.
 * Use this to save lhs result before codegen_expr(rhs) overwrites it. */
static void ir_save_result(char *dst) {
    int i;
    for (i = 0; ir_last_result[i]; i++)
        dst[i] = ir_last_result[i];
    dst[i] = 0;
}


/* ── AST Type → IR Type Mapper ───────────────────────────────────── */
/*                                                                     */
/* ── IR PERSISTENCE SCHEMA VERSION ─────────────────────────────────── */
/* See IR_SCHEMA.md for bump rules. Bump on any persisted-IR format     */
/* change (text grammar, JSON shape, opcode rename, type encoding).     */
#define ZCC_IR_VERSION_MAJOR 1
#define ZCC_IR_VERSION_MINOR 0
#define ZCC_IR_VERSION_PATCH 0
#define ZCC_IR_VERSION       "1.0.0"

/* Maps the compiler's internal Type* to the ir_type_t enum.           */
/* Handles all TY_* kinds including pointers, arrays, and structs.     */

static ir_type_t ir_map_type(Type *ty) {
    if (!ty) return IR_TY_I64;   /* default to 64-bit signed */

    switch (ty->kind) {
    case TY_VOID:       return IR_TY_VOID;
    case TY_CHAR:       return IR_TY_I8;
    case TY_UCHAR:      return IR_TY_U8;
    case TY_SHORT:      return IR_TY_I16;
    case TY_USHORT:     return IR_TY_U16;
    case TY_INT:        return IR_TY_I32;
    case TY_UINT:       return IR_TY_U32;
    case TY_LONG:       return IR_TY_I64;
    case TY_ULONG:      return IR_TY_U64;
    case TY_LONGLONG:   return IR_TY_I64;
    case TY_ULONGLONG:  return IR_TY_U64;
    case TY_FLOAT:      return IR_TY_F32;
    case TY_DOUBLE:     return IR_TY_F64;
    case TY_LONGDOUBLE: return IR_TY_F64; /* TODO: add IR_TY_F80 once IR schema supports x87 precision */
    default:            break;
    }

    /* Pointers, arrays, function pointers, structs → IR_TY_PTR (64-bit) */
    if (ty->kind == TY_PTR || ty->kind == TY_ARRAY || ty->kind == TY_FUNC)
        return IR_TY_PTR;

    /* Structs passed by pointer in System V ABI */
    if (ty->kind == TY_STRUCT)
        return IR_TY_PTR;

    /* Enum → i32 */
    if (ty->kind == TY_ENUM)
        return IR_TY_I32;

    return IR_TY_I64;  /* safety fallback */
}


/* ── Variable Name Extraction ────────────────────────────────────── */
/*                                                                     */
/* For ND_VAR nodes, extract the symbol name for IR readability.       */
/* Falls back to stack offset notation if name is unavailable.         */

static char ir_var_buf[64];

static char *ir_var_name(Node *node) {
    if (node->sym && node->sym->stack_offset != 0) {
        sprintf(ir_var_buf, "%%stack_%d", node->sym->stack_offset);
    } else if (node->sym && node->sym->name[0]) {
        sprintf(ir_var_buf, "%%%s", node->sym->name);
    } else {
        sprintf(ir_var_buf, "%%anon");
    }
    return ir_var_buf;
}


/* ── Function Boundary Helpers ───────────────────────────────────── */

static void ir_bridge_func_begin(Node *func) {
    ir_tmp_counter = 0;
    ir_last_result[0] = 0;
    if (func && func->func_type && func->func_type->ret) {
        ZCC_IR_FUNC_BEGIN(func->func_def_name, ir_map_type(func->func_type->ret), func->num_params, func->stack_size);
    }
}

static void ir_bridge_func_end(void) {
    ZCC_IR_FUNC_END();
}


/* ── Binary Op Mapper ────────────────────────────────────────────── */
/*                                                                     */
/* Maps AST node kinds (ND_ADD, ND_SUB, ...) to IR opcodes (IR_ADD,   */
/* IR_SUB, ...). Returns -1 for non-binary nodes.                      */

static ir_op_t ir_map_binop(int nd_kind) {
    switch (nd_kind) {
    case ND_ADD:  return IR_ADD;
    case ND_SUB:  return IR_SUB;
    case ND_MUL:  return IR_MUL;
    case ND_DIV:  return IR_DIV;
    case ND_MOD:  return IR_MOD;
    case ND_BAND: return IR_AND;
    case ND_BOR:  return IR_OR;
    case ND_BXOR: return IR_XOR;
    case ND_SHL:  return IR_SHL;
    case ND_SHR:  return IR_SHR;
    case ND_EQ:   return IR_EQ;
    case ND_NE:   return IR_NE;
    case ND_LT:   return IR_LT;
    case ND_LE:   return IR_LE;
    case ND_GT:   return IR_GT;
    case ND_GE:   return IR_GE;
    default:      return IR_NOP;
    }
}


/* ── Convenience: Emit Binary with Auto-Temp ─────────────────────── */
/*                                                                     */
/* Pattern: save lhs result, codegen rhs, emit IR_BINARY with fresh   */
/* destination temp. Used by all arithmetic/bitwise/comparison nodes.  */

static void ir_emit_binary_op(int nd_kind, Type *ty, char *lhs_tmp,
                               char *rhs_tmp, int line) {
    ir_op_t op;
    char *dst;

    op = ir_map_binop(nd_kind);
    if (op == IR_NOP) return;  /* not a binary op */

    if (is_float_type(ty)) {
        if (op == IR_ADD)      op = IR_FADD;
        else if (op == IR_SUB) op = IR_FSUB;
        else if (op == IR_MUL) op = IR_FMUL;
        else if (op == IR_DIV) op = IR_FDIV;
        else if (op == IR_EQ)  op = IR_FCMP_OEQ;
        else if (op == IR_NE)  op = IR_FCMP_UNE;
        else if (op == IR_LT)  op = IR_FCMP_OLT;
        else if (op == IR_LE)  op = IR_FCMP_OLE;
        else if (op == IR_GT)  op = IR_FCMP_OGT;
        else if (op == IR_GE)  op = IR_FCMP_OGE;
    }

    dst = ir_bridge_fresh_tmp();
    ZCC_EMIT_BINARY(op, ir_map_type(ty), dst, lhs_tmp, rhs_tmp, line);
}

static void ir_emit_var_load(Node *node) {
    char *vname = ir_var_name(node);
    char *dst = ir_bridge_fresh_tmp();
    Type *t = (node->sym && node->sym->type) ? node->sym->type : node->type;
    if (t && (t->kind == TY_ARRAY || t->kind == TY_STRUCT || t->kind == TY_UNION)) {
        ZCC_EMIT_UNARY(IR_ADDR, ir_map_type(node->type), dst, vname, node->line);
    } else {
        ZCC_EMIT_LOAD(ir_map_type(node->type), dst, vname, node->line);
    }
}

static void ir_emit_bitfield_load(Node *node, const char *addr_ir, char *dst) {
    int bf_q;
    ir_type_t st_ty;
    char addr_loc[32];
    char final_dst[32];
    char raw_tmp[32];
    char cur_tmp[32];
    int uns;
    int left_shift;
    int right_shift;
    if (!g_emit_ir || !g_ir_cur_func || !node) return;
    strcpy(addr_loc, addr_ir);
    strcpy(final_dst, dst);
    bf_q = (node->member_size > 4 || (node->bit_offset + node->bit_size > node->member_size * 8));
    st_ty = bf_q ? IR_TY_U64 : (node->member_size == 1 ? IR_TY_U8 : (node->member_size == 2 ? IR_TY_U16 : (node->member_size == 4 ? IR_TY_U32 : IR_TY_U64)));
    strcpy(raw_tmp, ir_bridge_fresh_tmp());
    ZCC_EMIT_LOAD(st_ty, raw_tmp, addr_loc, node->line);
    strcpy(cur_tmp, raw_tmp);
    uns = (node->type && (is_unsigned_type(node->type) || node->type->kind == TY_ENUM)) ? 1 : 0;
    left_shift = 64 - node->bit_offset - node->bit_size;
    right_shift = 64 - node->bit_size;
    if (left_shift > 0 && left_shift < 64) {
        char shl_c[32];
        char shl_t[32];
        strcpy(shl_c, ir_bridge_fresh_tmp());
        ZCC_EMIT_CONST(IR_TY_I64, shl_c, left_shift, node->line);
        strcpy(shl_t, ir_bridge_fresh_tmp());
        ZCC_EMIT_BINARY(IR_SHL, IR_TY_U64, shl_t, cur_tmp, shl_c, node->line);
        strcpy(cur_tmp, shl_t);
    }
    if (right_shift > 0 && right_shift < 64) {
        char shr_c[32];
        strcpy(shr_c, ir_bridge_fresh_tmp());
        ZCC_EMIT_CONST(IR_TY_I64, shr_c, right_shift, node->line);
        ZCC_EMIT_BINARY(IR_SHR, uns ? IR_TY_U64 : IR_TY_I64, final_dst, cur_tmp, shr_c, node->line);
    } else {
        ZCC_EMIT_UNARY(IR_COPY, uns ? IR_TY_U64 : IR_TY_I64, final_dst, cur_tmp, node->line);
    }
    strcpy(ir_last_result, final_dst);
}

static void ir_emit_bitfield_store(Node *lhs, const char *addr_ir, const char *val_ir, int line) {
    int bf_q;
    ir_type_t st_ty;
    char addr_loc[32];
    char val_loc[32];
    char old_t[32];
    char sm_c[32];
    char old_m[32];
    char mv_c[32];
    char val_m[32];
    char bo_c[32];
    char val_sh[32];
    char merged[32];
    char res_t[32];
    unsigned long long mask_val;
    unsigned long long shift_mask;
    int uns;
    int shift64;
    if (!g_emit_ir || !g_ir_cur_func || !lhs) return;
    strcpy(addr_loc, addr_ir);
    strcpy(val_loc, val_ir);
    bf_q = (lhs->member_size > 4 || (lhs->bit_offset + lhs->bit_size > lhs->member_size * 8));
    st_ty = bf_q ? IR_TY_U64 : (lhs->member_size == 1 ? IR_TY_U8 : (lhs->member_size == 2 ? IR_TY_U16 : (lhs->member_size == 4 ? IR_TY_U32 : IR_TY_U64)));
    strcpy(old_t, ir_bridge_fresh_tmp());
    ZCC_EMIT_LOAD(st_ty, old_t, addr_loc, line);
    mask_val = (lhs->bit_size == 64) ? 0xFFFFFFFFFFFFFFFFULL : ((1ULL << lhs->bit_size) - 1ULL);
    shift_mask = ~(mask_val << lhs->bit_offset);
    strcpy(sm_c, ir_bridge_fresh_tmp());
    ZCC_EMIT_CONST(IR_TY_I64, sm_c, (long)shift_mask, line);
    strcpy(old_m, ir_bridge_fresh_tmp());
    ZCC_EMIT_BINARY(IR_AND, IR_TY_U64, old_m, old_t, sm_c, line);
    strcpy(mv_c, ir_bridge_fresh_tmp());
    ZCC_EMIT_CONST(IR_TY_I64, mv_c, (long)mask_val, line);
    strcpy(val_m, ir_bridge_fresh_tmp());
    ZCC_EMIT_BINARY(IR_AND, IR_TY_U64, val_m, val_loc, mv_c, line);
    strcpy(bo_c, ir_bridge_fresh_tmp());
    ZCC_EMIT_CONST(IR_TY_I64, bo_c, lhs->bit_offset, line);
    strcpy(val_sh, ir_bridge_fresh_tmp());
    ZCC_EMIT_BINARY(IR_SHL, IR_TY_U64, val_sh, val_m, bo_c, line);
    strcpy(merged, ir_bridge_fresh_tmp());
    ZCC_EMIT_BINARY(IR_OR, IR_TY_U64, merged, old_m, val_sh, line);
    ZCC_EMIT_STORE(st_ty, addr_loc, merged, line);
    uns = (lhs->type && (is_unsigned_type(lhs->type) || lhs->type->kind == TY_ENUM)) ? 1 : 0;
    shift64 = 64 - lhs->bit_size;
    if (!uns && shift64 > 0 && shift64 < 64) {
        char sc_t[32];
        char shl_t[32];
        strcpy(sc_t, ir_bridge_fresh_tmp());
        ZCC_EMIT_CONST(IR_TY_I64, sc_t, shift64, line);
        strcpy(shl_t, ir_bridge_fresh_tmp());
        ZCC_EMIT_BINARY(IR_SHL, IR_TY_U64, shl_t, val_m, sc_t, line);
        strcpy(res_t, ir_bridge_fresh_tmp());
        ZCC_EMIT_BINARY(IR_SHR, IR_TY_I64, res_t, shl_t, sc_t, line);
        strcpy(ir_last_result, res_t);
    } else {
        strcpy(ir_last_result, val_m);
    }
}

#undef ZCC_IR_BRIDGE_ALLOWED
#endif /* ZCC_IR_BRIDGE_H */
