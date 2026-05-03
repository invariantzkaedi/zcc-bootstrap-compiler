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

#ifndef ZCC_IR_BRIDGE_H
#define ZCC_IR_BRIDGE_H

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
    ZCC_IR_FUNC_BEGIN(func->func_def_name, ir_map_type(func->func_type->ret), func->num_params);
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

    dst = ir_bridge_fresh_tmp();
    ZCC_EMIT_BINARY(op, ir_map_type(ty), dst, lhs_tmp, rhs_tmp, line);
}

static void ir_emit_var_load(Node *node) {
    char *vname = ir_var_name(node);
    char *dst = ir_bridge_fresh_tmp();
    if (node->type && (node->type->kind == TY_ARRAY || node->type->kind == TY_STRUCT || node->type->kind == TY_UNION)) {
        ZCC_EMIT_UNARY(IR_ADDR, ir_map_type(node->type), dst, vname, node->line);
    } else {
        ZCC_EMIT_LOAD(ir_map_type(node->type), dst, vname, node->line);
    }
}

#endif /* ZCC_IR_BRIDGE_H */
