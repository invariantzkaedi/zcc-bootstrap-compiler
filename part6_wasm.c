/* ================================================================ */
/* PART 6 (WASM): NATIVE WEBASSEMBLY STACK MACHINE CODE GENERATOR    */
/* ================================================================ */

#include "src/wasm_emit.h"

typedef struct {
    char name[64];
    uint32_t offset;
    int size;
    Type *type;
} WasmGlobalVar;

typedef struct {
    WasmGlobalVar vars[64];
    int count;
} WasmGlobalMap;

static uint8_t wasm_valtype_from_ctype(Type *type) {
    if (!type) return WASM_TYPE_I32;
    switch (type->kind) {
        case TY_FLOAT:
            return WASM_TYPE_F32;
        case TY_DOUBLE:
        case TY_LONGDOUBLE:
            return WASM_TYPE_F64;
        case TY_LONG:
        case TY_ULONG:
            return (type->size == 8) ? WASM_TYPE_I64 : WASM_TYPE_I32;
        case TY_LONGLONG:
        case TY_ULONGLONG:
            return WASM_TYPE_I64;
        case TY_PTR:
        case TY_ARRAY:
            return WASM_TYPE_I32; /* wasm32 pointers */
        default:
            return WASM_TYPE_I32;
    }
}

static uint8_t wasm_node_valtype(Node *n) {
    if (!n) return WASM_TYPE_I32;
    if (n->kind == ND_FLIT) {
        if (n->type && n->type->kind == TY_FLOAT) return WASM_TYPE_F32;
        return WASM_TYPE_F64;
    }
    switch (n->kind) {
        case ND_EQ:
        case ND_NE:
        case ND_LT:
        case ND_LE:
        case ND_GT:
        case ND_GE:
        case ND_LNOT:
        case ND_LAND:
        case ND_LOR:
            return WASM_TYPE_I32;
        default:
            break;
    }
    if (n->type) {
        return wasm_valtype_from_ctype(n->type);
    }
    return WASM_TYPE_I32;
}

static uint8_t wasm_promoted_valtype(uint8_t vt1, uint8_t vt2) {
    if (vt1 == WASM_TYPE_F64 || vt2 == WASM_TYPE_F64) return WASM_TYPE_F64;
    if (vt1 == WASM_TYPE_F32 || vt2 == WASM_TYPE_F32) return WASM_TYPE_F32;
    if (vt1 == WASM_TYPE_I64 || vt2 == WASM_TYPE_I64) return WASM_TYPE_I64;
    return WASM_TYPE_I32;
}

static int wasm_type_is_unsigned_or_ptr(Type *type) {
    if (!type) return 0;
    if (is_pointer(type) || type->kind == TY_ARRAY) return 1;
    return is_unsigned_type(type);
}

static void wasm_emit_numeric_conversion(WASMBuffer *buf, uint8_t src_vt, uint8_t dst_vt, int is_unsigned) {
    if (src_vt == dst_vt) return;

    if (src_vt == WASM_TYPE_I32 && dst_vt == WASM_TYPE_I64) {
        wasm_buf_append_u8(buf, is_unsigned ? WASM_OP_I64_EXTEND_I32_U : WASM_OP_I64_EXTEND_I32_S);
    } else if (src_vt == WASM_TYPE_I64 && dst_vt == WASM_TYPE_I32) {
        wasm_buf_append_u8(buf, WASM_OP_I32_WRAP_I64);
    } else if (src_vt == WASM_TYPE_I32 && dst_vt == WASM_TYPE_F32) {
        wasm_buf_append_u8(buf, is_unsigned ? WASM_OP_F32_CONVERT_I32_U : WASM_OP_F32_CONVERT_I32_S);
    } else if (src_vt == WASM_TYPE_I32 && dst_vt == WASM_TYPE_F64) {
        wasm_buf_append_u8(buf, is_unsigned ? WASM_OP_F64_CONVERT_I32_U : WASM_OP_F64_CONVERT_I32_S);
    } else if (src_vt == WASM_TYPE_I64 && dst_vt == WASM_TYPE_F32) {
        wasm_buf_append_u8(buf, is_unsigned ? WASM_OP_F32_CONVERT_I64_U : WASM_OP_F32_CONVERT_I64_S);
    } else if (src_vt == WASM_TYPE_I64 && dst_vt == WASM_TYPE_F64) {
        wasm_buf_append_u8(buf, is_unsigned ? WASM_OP_F64_CONVERT_I64_U : WASM_OP_F64_CONVERT_I64_S);
    } else if (src_vt == WASM_TYPE_F32 && dst_vt == WASM_TYPE_F64) {
        wasm_buf_append_u8(buf, WASM_OP_F64_PROMOTE_F32);
    } else if (src_vt == WASM_TYPE_F64 && dst_vt == WASM_TYPE_F32) {
        wasm_buf_append_u8(buf, WASM_OP_F32_DEMOTE_F64);
    } else if (src_vt == WASM_TYPE_F32 && dst_vt == WASM_TYPE_I32) {
        wasm_buf_append_u8(buf, is_unsigned ? WASM_OP_I32_TRUNC_F32_U : WASM_OP_I32_TRUNC_F32_S);
    } else if (src_vt == WASM_TYPE_F64 && dst_vt == WASM_TYPE_I32) {
        wasm_buf_append_u8(buf, is_unsigned ? WASM_OP_I32_TRUNC_F64_U : WASM_OP_I32_TRUNC_F64_S);
    } else if (src_vt == WASM_TYPE_F32 && dst_vt == WASM_TYPE_I64) {
        wasm_buf_append_u8(buf, is_unsigned ? WASM_OP_I64_TRUNC_F32_U : WASM_OP_I64_TRUNC_F32_S);
    } else if (src_vt == WASM_TYPE_F64 && dst_vt == WASM_TYPE_I64) {
        wasm_buf_append_u8(buf, is_unsigned ? WASM_OP_I64_TRUNC_F64_U : WASM_OP_I64_TRUNC_F64_S);
    }
}

static void wasm_emit_type_conversion(WASMBuffer *buf, uint8_t src_vt, Type *src_type, uint8_t dst_vt, Type *dst_type) {
    if (src_vt == dst_vt) return;
    int is_uns = (src_vt == WASM_TYPE_F32 || src_vt == WASM_TYPE_F64)
        ? wasm_type_is_unsigned_or_ptr(dst_type)
        : wasm_type_is_unsigned_or_ptr(src_type);
    wasm_emit_numeric_conversion(buf, src_vt, dst_vt, is_uns);
}

static void wasm_emit_to_bool_i32(WASMBuffer *buf, uint8_t vt) {
    if (vt == WASM_TYPE_I64) {
        wasm_buf_append_u8(buf, WASM_OP_I64_CONST);
        wasm_buf_append_sleb128_i64(buf, 0);
        wasm_buf_append_u8(buf, WASM_OP_I64_NE);
    } else if (vt == WASM_TYPE_F32) {
        wasm_buf_append_u8(buf, WASM_OP_F32_CONST);
        wasm_buf_append_f32(buf, 0.0f);
        wasm_buf_append_u8(buf, WASM_OP_F32_NE);
    } else if (vt == WASM_TYPE_F64) {
        wasm_buf_append_u8(buf, WASM_OP_F64_CONST);
        wasm_buf_append_f64(buf, 0.0);
        wasm_buf_append_u8(buf, WASM_OP_F64_NE);
    } else {
        wasm_buf_append_u8(buf, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(buf, 0);
        wasm_buf_append_u8(buf, WASM_OP_I32_NE);
    }
}

static int wasm_ptr_elem_size(Type *type) {
    if (!type) return 1;
    if (is_pointer(type) && type->base) return type_size(type->base);
    if (type->kind == TY_ARRAY && type->base) return type_size(type->base);
    return 1;
}

static WasmLocalEntry *wasm_local_find_entry(WasmLocalMap *map, const char *name) {
    if (!map || !name) return NULL;
    int i;
    for (i = 0; i < map->count; i++) {
        if (strcmp(map->entries[i].name, name) == 0) {
            return &map->entries[i];
        }
    }
    return NULL;
}

static int wasm_local_lookup(const WasmLocalMap *map, const char *name) {
    if (!map || !name) return -1;
    int i;
    for (i = 0; i < map->count; i++) {
        if (strcmp(map->entries[i].name, name) == 0) {
            return (int)map->entries[i].local_idx;
        }
    }
    return -1;
}

static WasmGlobalVar *wasm_global_find(WasmGlobalMap *gmap, const char *name) {
    if (!gmap || !name) return NULL;
    int i;
    for (i = 0; i < gmap->count; i++) {
        if (strcmp(gmap->vars[i].name, name) == 0) {
            return &gmap->vars[i];
        }
    }
    return NULL;
}

static int wasm_find_func_idx(const WasmModule *mod, const char *name) {
    return wasm_module_find_func_idx(mod, name);
}

static void wasm_emit_memarg_safe(WASMBuffer *buf, uint32_t align, uint32_t offset) {
    wasm_buf_append_uleb128(buf, align);
    wasm_buf_append_uleb128(buf, offset);
}

static void wasm_emit_typed_load(WASMBuffer *buf, Type *type, uint32_t offset) {
    if (type && type->kind == TY_FLOAT) {
        wasm_buf_append_u8(buf, WASM_OP_F32_LOAD);
        wasm_emit_memarg_safe(buf, 2, offset);
        return;
    }
    if (type && (type->kind == TY_DOUBLE || type->kind == TY_LONGDOUBLE)) {
        wasm_buf_append_u8(buf, WASM_OP_F64_LOAD);
        wasm_emit_memarg_safe(buf, 3, offset);
        return;
    }
    if (wasm_valtype_from_ctype(type) == WASM_TYPE_I64) {
        wasm_buf_append_u8(buf, WASM_OP_I64_LOAD);
        wasm_emit_memarg_safe(buf, 3, offset);
        return;
    }
    if (type && is_pointer(type)) {
        wasm_buf_append_u8(buf, WASM_OP_I32_LOAD);
        wasm_emit_memarg_safe(buf, 2, offset);
        return;
    }
    int sz = type ? type_size(type) : 4;
    int is_uns = type ? is_unsigned_type(type) : 0;
    if (sz == 1) {
        wasm_buf_append_u8(buf, is_uns ? WASM_OP_I32_LOAD8_U : WASM_OP_I32_LOAD8_S);
        wasm_emit_memarg_safe(buf, 0, offset);
    } else if (sz == 2) {
        wasm_buf_append_u8(buf, is_uns ? WASM_OP_I32_LOAD16_U : WASM_OP_I32_LOAD16_S);
        wasm_emit_memarg_safe(buf, 1, offset);
    } else {
        wasm_buf_append_u8(buf, WASM_OP_I32_LOAD);
        wasm_emit_memarg_safe(buf, 2, offset);
    }
}

static void wasm_emit_typed_store(WASMBuffer *buf, Type *type, uint32_t offset) {
    if (type && type->kind == TY_FLOAT) {
        wasm_buf_append_u8(buf, WASM_OP_F32_STORE);
        wasm_emit_memarg_safe(buf, 2, offset);
        return;
    }
    if (type && (type->kind == TY_DOUBLE || type->kind == TY_LONGDOUBLE)) {
        wasm_buf_append_u8(buf, WASM_OP_F64_STORE);
        wasm_emit_memarg_safe(buf, 3, offset);
        return;
    }
    if (wasm_valtype_from_ctype(type) == WASM_TYPE_I64) {
        wasm_buf_append_u8(buf, WASM_OP_I64_STORE);
        wasm_emit_memarg_safe(buf, 3, offset);
        return;
    }
    if (type && is_pointer(type)) {
        wasm_buf_append_u8(buf, WASM_OP_I32_STORE);
        wasm_emit_memarg_safe(buf, 2, offset);
        return;
    }
    int sz = type ? type_size(type) : 4;
    if (sz == 1) {
        wasm_buf_append_u8(buf, WASM_OP_I32_STORE8);
        wasm_emit_memarg_safe(buf, 0, offset);
    } else if (sz == 2) {
        wasm_buf_append_u8(buf, WASM_OP_I32_STORE16);
        wasm_emit_memarg_safe(buf, 1, offset);
    } else {
        wasm_buf_append_u8(buf, WASM_OP_I32_STORE);
        wasm_emit_memarg_safe(buf, 2, offset);
    }
}

static int wasm_node_has_side_effects(Node *n) {
    if (!n) return 0;
    if (n->kind == ND_ASSIGN || n->kind == ND_COMPOUND_ASSIGN || n->kind == ND_CALL ||
        n->kind == ND_PRE_INC || n->kind == ND_PRE_DEC ||
        n->kind == ND_POST_INC || n->kind == ND_POST_DEC) return 1;
    if (n->lhs && wasm_node_has_side_effects(n->lhs)) return 1;
    if (n->rhs && wasm_node_has_side_effects(n->rhs)) return 1;
    return 0;
}

/* Global string offset mapping */
static uint32_t wasm_string_offsets[MAX_STRINGS];

/* Forward declarations */
static void wasm_lower_addr(Compiler *cc, const WasmModule *mod, WasmFunction *fn, WasmLocalMap *lmap, WasmGlobalMap *gmap, Node *node);
static void wasm_lower_expr(Compiler *cc, const WasmModule *mod, WasmFunction *fn, WasmLocalMap *lmap, WasmGlobalMap *gmap, Node *node);
static void wasm_lower_stmt(Compiler *cc, const WasmModule *mod, WasmFunction *fn, WasmLocalMap *lmap, WasmGlobalMap *gmap, Node *node, Type *ret_type);

/* Address / Lvalue Lowering */
static void wasm_lower_addr(Compiler *cc, const WasmModule *mod, WasmFunction *fn, WasmLocalMap *lmap, WasmGlobalMap *gmap, Node *node) {
    if (!node || !fn) return;

    switch (node->kind) {
        case ND_VAR: {
            const char *vname = node->sym ? node->sym->name : node->name;
            int f_idx = wasm_find_func_idx(mod, vname);
            if (f_idx >= 0 && !wasm_local_find_entry(lmap, vname) && !wasm_global_find(gmap, vname)) {
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                wasm_buf_append_sleb128(&fn->body, f_idx);
                break;
            }

            WasmLocalEntry *entry = wasm_local_find_entry(lmap, vname);
            if (entry && entry->is_memory) {
                /* Local stack variable: $fp + stack_offset */
                if (lmap->fp_local_idx >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->fp_local_idx);
                } else {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 0);
                }
                if (entry->stack_offset > 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, entry->stack_offset);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD);
                }
            } else {
                /* Check if it is a global variable */
                WasmGlobalVar *gv = wasm_global_find(gmap, vname);
                if (gv) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, (int32_t)gv->offset);
                } else if (node->sym && node->sym->stack_offset != 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, (int32_t)node->sym->stack_offset);
                } else {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 1024);
                }
            }
            break;
        }

        case ND_DEREF:
            /* Address of *p is the evaluated value of pointer p */
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            break;

        case ND_MEMBER: {
            /* Address of s.m or ptr->m: base_addr + member_offset */
            if (node->lhs) {
                if (node->lhs->type && is_pointer(node->lhs->type)) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
                } else {
                    wasm_lower_addr(cc, mod, fn, lmap, gmap, node->lhs);
                }
                if (node->member_offset > 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, node->member_offset);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD);
                }
            }
            break;
        }

        case ND_ADDR: {
            if (node->lhs && node->lhs->kind == ND_VAR) {
                const char *fn_name = node->lhs->sym ? node->lhs->sym->name : node->lhs->name;
                int f_idx = wasm_find_func_idx(mod, fn_name);
                if (f_idx >= 0 && !wasm_local_find_entry(lmap, fn_name) && !wasm_global_find(gmap, fn_name)) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, f_idx);
                    break;
                }
            }
            wasm_lower_addr(cc, mod, fn, lmap, gmap, node->lhs);
            break;
        }

        default:
            /* Array indexing address arithmetic *(base + idx) */
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node);
            break;
    }
}

/* Expression Lowering */
static void wasm_lower_expr(Compiler *cc, const WasmModule *mod, WasmFunction *fn, WasmLocalMap *lmap, WasmGlobalMap *gmap, Node *node) {
    if (!node || !fn) return;

    switch (node->kind) {
        case ND_NUM:
            if (wasm_node_valtype(node) == WASM_TYPE_I64) {
                wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                wasm_buf_append_sleb128_i64(&fn->body, (int64_t)node->int_val);
            } else {
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                wasm_buf_append_sleb128(&fn->body, (int32_t)node->int_val);
            }
            break;

        case ND_STR: {
            int s_idx = node->str_id;
            uint32_t s_off = 1024;
            if (s_idx >= 0 && s_idx < MAX_STRINGS && wasm_string_offsets[s_idx] > 0) {
                s_off = wasm_string_offsets[s_idx];
            }
            wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
            wasm_buf_append_sleb128(&fn->body, (int32_t)s_off);
            break;
        }

        case ND_FLIT:
            if (node->type && node->type->kind == TY_FLOAT) {
                wasm_buf_append_u8(&fn->body, WASM_OP_F32_CONST);
                wasm_buf_append_f32(&fn->body, (float)node->f_val);
            } else {
                wasm_buf_append_u8(&fn->body, WASM_OP_F64_CONST);
                wasm_buf_append_f64(&fn->body, (double)node->f_val);
            }
            break;

        case ND_VAR: {
            const char *vname = node->sym ? node->sym->name : node->name;

            /* Check if it is a function name decaying to function pointer index */
            int f_idx = wasm_find_func_idx(mod, vname);
            if (f_idx >= 0 && !wasm_local_find_entry(lmap, vname) && !wasm_global_find(gmap, vname)) {
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                wasm_buf_append_sleb128(&fn->body, f_idx);
                break;
            }

            WasmLocalEntry *entry = wasm_local_find_entry(lmap, vname);

            /* Arrays and Structs decay to their base address in expression contexts */
            if (node->type && (node->type->kind == TY_ARRAY || node->type->kind == TY_STRUCT || node->type->kind == TY_UNION)) {
                wasm_lower_addr(cc, mod, fn, lmap, gmap, node);
                break;
            }

            if (entry && entry->is_memory) {
                wasm_lower_addr(cc, mod, fn, lmap, gmap, node);
                wasm_emit_typed_load(&fn->body, node->type ? node->type : entry->type, 0);
            } else if (entry && !entry->is_memory) {
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                wasm_buf_append_uleb128(&fn->body, entry->local_idx);
            } else {
                /* Global variable load */
                WasmGlobalVar *gv = wasm_global_find(gmap, vname);
                if (gv) {
                    wasm_lower_addr(cc, mod, fn, lmap, gmap, node);
                    wasm_emit_typed_load(&fn->body, node->type ? node->type : gv->type, 0);
                } else {
                    int idx = wasm_local_lookup(lmap, vname);
                    if (idx >= 0) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                        wasm_buf_append_uleb128(&fn->body, (uint32_t)idx);
                    } else if (wasm_node_valtype(node) == WASM_TYPE_I64) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                        wasm_buf_append_sleb128_i64(&fn->body, 0);
                    } else {
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                        wasm_buf_append_sleb128(&fn->body, 0);
                    }
                }
            }
            break;
        }

        case ND_ADDR:
            /* &lvalue evaluates to its memory address or function index */
            wasm_lower_addr(cc, mod, fn, lmap, gmap, node);
            break;

        case ND_DEREF:
            /* *p: if array/struct, yield pointer; else load typed value */
            if (node->type && (node->type->kind == TY_ARRAY || node->type->kind == TY_STRUCT || node->type->kind == TY_UNION)) {
                wasm_lower_addr(cc, mod, fn, lmap, gmap, node);
            } else {
                wasm_lower_addr(cc, mod, fn, lmap, gmap, node);
                wasm_emit_typed_load(&fn->body, node->type, 0);
            }
            break;

        case ND_MEMBER:
            /* s.member or ptr->member */
            if (node->type && (node->type->kind == TY_ARRAY || node->type->kind == TY_STRUCT || node->type->kind == TY_UNION)) {
                wasm_lower_addr(cc, mod, fn, lmap, gmap, node);
            } else {
                wasm_lower_addr(cc, mod, fn, lmap, gmap, node);
                wasm_emit_typed_load(&fn->body, node->type, 0);
            }
            break;

        case ND_ASSIGN: {
            uint8_t lhs_vt = wasm_node_valtype(node->lhs);
            uint8_t rhs_vt = wasm_node_valtype(node->rhs);
            Type *lhs_type = node->lhs ? node->lhs->type : NULL;
            Type *rhs_type = node->rhs ? node->rhs->type : NULL;

            if (node->lhs && node->lhs->kind == ND_VAR) {
                const char *vname = node->lhs->sym ? node->lhs->sym->name : node->lhs->name;
                WasmLocalEntry *entry = wasm_local_find_entry(lmap, vname);
                if (entry && !entry->is_memory) {
                    /* Direct local register assignment with numeric conversion */
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
                    wasm_emit_type_conversion(&fn->body, rhs_vt, rhs_type, entry->val_type, lhs_type);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_TEE);
                    wasm_buf_append_uleb128(&fn->body, entry->local_idx);
                    break;
                }
            }

            /* Memory Lvalue Store: [addr, val] */
            wasm_lower_addr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_type_conversion(&fn->body, rhs_vt, rhs_type, lhs_vt, lhs_type);

            if (lhs_vt == WASM_TYPE_F32) {
                if (lmap->tmp_f32_idx >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_TEE);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_f32_idx);
                }
                wasm_emit_typed_store(&fn->body, lhs_type, 0);
                if (lmap->tmp_f32_idx >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_f32_idx);
                }
            } else if (lhs_vt == WASM_TYPE_F64) {
                if (lmap->tmp_f64_idx >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_TEE);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_f64_idx);
                }
                wasm_emit_typed_store(&fn->body, lhs_type, 0);
                if (lmap->tmp_f64_idx >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_f64_idx);
                }
            } else if (lhs_vt == WASM_TYPE_I64) {
                if (lmap->tmp_i64_idx >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_TEE);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                }
                wasm_emit_typed_store(&fn->body, lhs_type, 0);
                if (lmap->tmp_i64_idx >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                }
            } else {
                if (lmap->tmp_val_idx >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_TEE);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                }
                wasm_emit_typed_store(&fn->body, lhs_type, 0);
                if (lmap->tmp_val_idx >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                }
            }
            break;
        }

        case ND_COMPOUND_ASSIGN: {
            uint8_t lhs_vt = wasm_node_valtype(node->lhs);
            uint8_t rhs_vt = wasm_node_valtype(node->rhs);
            Type *lhs_type = node->lhs ? node->lhs->type : NULL;
            Type *rhs_type = node->rhs ? node->rhs->type : NULL;
            int is_shift_op = (node->compound_op == ND_SHL || node->compound_op == ND_SHR);
            uint8_t common_vt = is_shift_op ? lhs_vt : wasm_promoted_valtype(lhs_vt, rhs_vt);
            int is_uns = is_shift_op
                ? wasm_type_is_unsigned_or_ptr(lhs_type)
                : (is_pointer(lhs_type) || wasm_type_is_unsigned_or_ptr(rhs_type));

            if (node->lhs && node->lhs->kind == ND_VAR) {
                const char *vname = node->lhs->sym ? node->lhs->sym->name : node->lhs->name;
                WasmLocalEntry *entry = wasm_local_find_entry(lmap, vname);
                if (entry && !entry->is_memory) {
                    /* Register compound assignment */
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, entry->local_idx);
                    wasm_emit_type_conversion(&fn->body, entry->val_type, lhs_type, common_vt, lhs_type);
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
                    wasm_emit_type_conversion(&fn->body, rhs_vt, rhs_type, common_vt, lhs_type);

                    if (common_vt == WASM_TYPE_F32) {
                        switch (node->compound_op) {
                            case ND_ADD: case ND_FADD: wasm_buf_append_u8(&fn->body, WASM_OP_F32_ADD); break;
                            case ND_SUB: case ND_FSUB: wasm_buf_append_u8(&fn->body, WASM_OP_F32_SUB); break;
                            case ND_MUL: case ND_FMUL: wasm_buf_append_u8(&fn->body, WASM_OP_F32_MUL); break;
                            case ND_DIV: case ND_FDIV: wasm_buf_append_u8(&fn->body, WASM_OP_F32_DIV); break;
                            default: wasm_buf_append_u8(&fn->body, WASM_OP_F32_ADD); break;
                        }
                    } else if (common_vt == WASM_TYPE_F64) {
                        switch (node->compound_op) {
                            case ND_ADD: case ND_FADD: wasm_buf_append_u8(&fn->body, WASM_OP_F64_ADD); break;
                            case ND_SUB: case ND_FSUB: wasm_buf_append_u8(&fn->body, WASM_OP_F64_SUB); break;
                            case ND_MUL: case ND_FMUL: wasm_buf_append_u8(&fn->body, WASM_OP_F64_MUL); break;
                            case ND_DIV: case ND_FDIV: wasm_buf_append_u8(&fn->body, WASM_OP_F64_DIV); break;
                            default: wasm_buf_append_u8(&fn->body, WASM_OP_F64_ADD); break;
                        }
                    } else if (common_vt == WASM_TYPE_I64) {
                        switch (node->compound_op) {
                            case ND_ADD: wasm_buf_append_u8(&fn->body, WASM_OP_I64_ADD); break;
                            case ND_SUB: wasm_buf_append_u8(&fn->body, WASM_OP_I64_SUB); break;
                            case ND_MUL: wasm_buf_append_u8(&fn->body, WASM_OP_I64_MUL); break;
                            case ND_DIV: wasm_buf_append_u8(&fn->body, is_uns ? WASM_OP_I64_DIV_U : WASM_OP_I64_DIV_S); break;
                            case ND_MOD: wasm_buf_append_u8(&fn->body, is_uns ? WASM_OP_I64_REM_U : WASM_OP_I64_REM_S); break;
                            case ND_BAND: wasm_buf_append_u8(&fn->body, WASM_OP_I64_AND); break;
                            case ND_BOR: wasm_buf_append_u8(&fn->body, WASM_OP_I64_OR); break;
                            case ND_BXOR: wasm_buf_append_u8(&fn->body, WASM_OP_I64_XOR); break;
                            case ND_SHL: wasm_buf_append_u8(&fn->body, WASM_OP_I64_SHL); break;
                            case ND_SHR: wasm_buf_append_u8(&fn->body, is_uns ? WASM_OP_I64_SHR_U : WASM_OP_I64_SHR_S); break;
                            default: wasm_buf_append_u8(&fn->body, WASM_OP_I64_ADD); break;
                        }
                    } else {
                        switch (node->compound_op) {
                            case ND_ADD: wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD); break;
                            case ND_SUB: wasm_buf_append_u8(&fn->body, WASM_OP_I32_SUB); break;
                            case ND_MUL: wasm_buf_append_u8(&fn->body, WASM_OP_I32_MUL); break;
                            case ND_DIV: wasm_buf_append_u8(&fn->body, is_uns ? WASM_OP_I32_DIV_U : WASM_OP_I32_DIV_S); break;
                            case ND_MOD: wasm_buf_append_u8(&fn->body, is_uns ? WASM_OP_I32_REM_U : WASM_OP_I32_REM_S); break;
                            case ND_BAND: wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND); break;
                            case ND_BOR: wasm_buf_append_u8(&fn->body, WASM_OP_I32_OR); break;
                            case ND_BXOR: wasm_buf_append_u8(&fn->body, WASM_OP_I32_XOR); break;
                            case ND_SHL: wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHL); break;
                            case ND_SHR: wasm_buf_append_u8(&fn->body, is_uns ? WASM_OP_I32_SHR_U : WASM_OP_I32_SHR_S); break;
                            default: wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD); break;
                        }
                    }
                    wasm_emit_type_conversion(&fn->body, common_vt, lhs_type, entry->val_type, lhs_type);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_TEE);
                    wasm_buf_append_uleb128(&fn->body, entry->local_idx);
                    break;
                }
            }

            /* Memory Lvalue Compound Assignment (side-effect-free, address evaluated once) */
            wasm_lower_addr(cc, mod, fn, lmap, gmap, node->lhs);
            if (lmap->tmp_addr_idx >= 0) {
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_addr_idx);
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_addr_idx);
            }
            wasm_emit_typed_load(&fn->body, lhs_type, 0);
            wasm_emit_type_conversion(&fn->body, lhs_vt, lhs_type, common_vt, lhs_type);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_type_conversion(&fn->body, rhs_vt, rhs_type, common_vt, lhs_type);

            if (common_vt == WASM_TYPE_F32) {
                switch (node->compound_op) {
                    case ND_ADD: case ND_FADD: wasm_buf_append_u8(&fn->body, WASM_OP_F32_ADD); break;
                    case ND_SUB: case ND_FSUB: wasm_buf_append_u8(&fn->body, WASM_OP_F32_SUB); break;
                    case ND_MUL: case ND_FMUL: wasm_buf_append_u8(&fn->body, WASM_OP_F32_MUL); break;
                    case ND_DIV: case ND_FDIV: wasm_buf_append_u8(&fn->body, WASM_OP_F32_DIV); break;
                    default: wasm_buf_append_u8(&fn->body, WASM_OP_F32_ADD); break;
                }
            } else if (common_vt == WASM_TYPE_F64) {
                switch (node->compound_op) {
                    case ND_ADD: case ND_FADD: wasm_buf_append_u8(&fn->body, WASM_OP_F64_ADD); break;
                    case ND_SUB: case ND_FSUB: wasm_buf_append_u8(&fn->body, WASM_OP_F64_SUB); break;
                    case ND_MUL: case ND_FMUL: wasm_buf_append_u8(&fn->body, WASM_OP_F64_MUL); break;
                    case ND_DIV: case ND_FDIV: wasm_buf_append_u8(&fn->body, WASM_OP_F64_DIV); break;
                    default: wasm_buf_append_u8(&fn->body, WASM_OP_F64_ADD); break;
                }
            } else if (common_vt == WASM_TYPE_I64) {
                switch (node->compound_op) {
                    case ND_ADD: wasm_buf_append_u8(&fn->body, WASM_OP_I64_ADD); break;
                    case ND_SUB: wasm_buf_append_u8(&fn->body, WASM_OP_I64_SUB); break;
                    case ND_MUL: wasm_buf_append_u8(&fn->body, WASM_OP_I64_MUL); break;
                    case ND_DIV: wasm_buf_append_u8(&fn->body, is_uns ? WASM_OP_I64_DIV_U : WASM_OP_I64_DIV_S); break;
                    case ND_MOD: wasm_buf_append_u8(&fn->body, is_uns ? WASM_OP_I64_REM_U : WASM_OP_I64_REM_S); break;
                    case ND_BAND: wasm_buf_append_u8(&fn->body, WASM_OP_I64_AND); break;
                    case ND_BOR: wasm_buf_append_u8(&fn->body, WASM_OP_I64_OR); break;
                    case ND_BXOR: wasm_buf_append_u8(&fn->body, WASM_OP_I64_XOR); break;
                    case ND_SHL: wasm_buf_append_u8(&fn->body, WASM_OP_I64_SHL); break;
                    case ND_SHR: wasm_buf_append_u8(&fn->body, is_uns ? WASM_OP_I64_SHR_U : WASM_OP_I64_SHR_S); break;
                    default: wasm_buf_append_u8(&fn->body, WASM_OP_I64_ADD); break;
                }
            } else {
                switch (node->compound_op) {
                    case ND_ADD: wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD); break;
                    case ND_SUB: wasm_buf_append_u8(&fn->body, WASM_OP_I32_SUB); break;
                    case ND_MUL: wasm_buf_append_u8(&fn->body, WASM_OP_I32_MUL); break;
                    case ND_DIV: wasm_buf_append_u8(&fn->body, is_uns ? WASM_OP_I32_DIV_U : WASM_OP_I32_DIV_S); break;
                    case ND_MOD: wasm_buf_append_u8(&fn->body, is_uns ? WASM_OP_I32_REM_U : WASM_OP_I32_REM_S); break;
                    case ND_BAND: wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND); break;
                    case ND_BOR: wasm_buf_append_u8(&fn->body, WASM_OP_I32_OR); break;
                    case ND_BXOR: wasm_buf_append_u8(&fn->body, WASM_OP_I32_XOR); break;
                    case ND_SHL: wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHL); break;
                    case ND_SHR: wasm_buf_append_u8(&fn->body, is_uns ? WASM_OP_I32_SHR_U : WASM_OP_I32_SHR_S); break;
                    default: wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD); break;
                }
            }
            wasm_emit_type_conversion(&fn->body, common_vt, lhs_type, lhs_vt, lhs_type);

            if (lhs_vt == WASM_TYPE_F32) {
                if (lmap->tmp_f32_idx >= 0 && lmap->tmp_addr_idx >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_f32_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_addr_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_f32_idx);
                }
                wasm_emit_typed_store(&fn->body, lhs_type, 0);
                if (lmap->tmp_f32_idx >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_f32_idx);
                }
            } else if (lhs_vt == WASM_TYPE_F64) {
                if (lmap->tmp_f64_idx >= 0 && lmap->tmp_addr_idx >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_f64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_addr_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_f64_idx);
                }
                wasm_emit_typed_store(&fn->body, lhs_type, 0);
                if (lmap->tmp_f64_idx >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_f64_idx);
                }
            } else if (lhs_vt == WASM_TYPE_I64) {
                if (lmap->tmp_i64_idx >= 0 && lmap->tmp_addr_idx >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_addr_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                }
                wasm_emit_typed_store(&fn->body, lhs_type, 0);
                if (lmap->tmp_i64_idx >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                }
            } else {
                if (lmap->tmp_val_idx >= 0 && lmap->tmp_addr_idx >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_addr_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                }
                wasm_emit_typed_store(&fn->body, lhs_type, 0);
                if (lmap->tmp_val_idx >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                }
            }
            break;
        }

        case ND_ADD:
        case ND_FADD: {
            uint8_t lhs_vt = wasm_node_valtype(node->lhs);
            uint8_t rhs_vt = wasm_node_valtype(node->rhs);
            Type *lhs_type = node->lhs ? node->lhs->type : NULL;
            Type *rhs_type = node->rhs ? node->rhs->type : NULL;

            if (lhs_type && (is_pointer(lhs_type) || lhs_type->kind == TY_ARRAY)) {
                int esz_l = wasm_ptr_elem_size(lhs_type);
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
                wasm_emit_type_conversion(&fn->body, rhs_vt, rhs_type, WASM_TYPE_I32, cc->ty_int);
                if (esz_l > 1) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, esz_l);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_MUL);
                }
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD);
                break;
            } else if (rhs_type && (is_pointer(rhs_type) || rhs_type->kind == TY_ARRAY)) {
                int esz_r = wasm_ptr_elem_size(rhs_type);
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
                wasm_emit_type_conversion(&fn->body, lhs_vt, lhs_type, WASM_TYPE_I32, cc->ty_int);
                if (esz_r > 1) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, esz_r);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_MUL);
                }
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD);
                break;
            }

            uint8_t common_vt = wasm_promoted_valtype(lhs_vt, rhs_vt);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_emit_type_conversion(&fn->body, lhs_vt, lhs_type, common_vt, node->type);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_type_conversion(&fn->body, rhs_vt, rhs_type, common_vt, node->type);

            if (common_vt == WASM_TYPE_F32) wasm_buf_append_u8(&fn->body, WASM_OP_F32_ADD);
            else if (common_vt == WASM_TYPE_F64) wasm_buf_append_u8(&fn->body, WASM_OP_F64_ADD);
            else if (common_vt == WASM_TYPE_I64) wasm_buf_append_u8(&fn->body, WASM_OP_I64_ADD);
            else wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD);
            break;
        }

        case ND_SUB:
        case ND_FSUB: {
            uint8_t lhs_vt = wasm_node_valtype(node->lhs);
            uint8_t rhs_vt = wasm_node_valtype(node->rhs);
            Type *lhs_type = node->lhs ? node->lhs->type : NULL;
            Type *rhs_type = node->rhs ? node->rhs->type : NULL;

            if (lhs_type && (is_pointer(lhs_type) || lhs_type->kind == TY_ARRAY) &&
                rhs_type && (is_pointer(rhs_type) || rhs_type->kind == TY_ARRAY)) {
                /* Pointer difference: (p1 - p2) / sizeof(*p) */
                int esz_l = wasm_ptr_elem_size(lhs_type);
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_SUB);
                if (esz_l > 1) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, esz_l);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_DIV_S);
                }
                if (wasm_node_valtype(node) == WASM_TYPE_I64) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_EXTEND_I32_S);
                }
                break;
            } else if (lhs_type && (is_pointer(lhs_type) || lhs_type->kind == TY_ARRAY)) {
                int esz_l = wasm_ptr_elem_size(lhs_type);
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
                wasm_emit_type_conversion(&fn->body, rhs_vt, rhs_type, WASM_TYPE_I32, cc->ty_int);
                if (esz_l > 1) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, esz_l);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_MUL);
                }
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_SUB);
                break;
            }

            uint8_t common_vt = wasm_promoted_valtype(lhs_vt, rhs_vt);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_emit_type_conversion(&fn->body, lhs_vt, lhs_type, common_vt, node->type);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_type_conversion(&fn->body, rhs_vt, rhs_type, common_vt, node->type);

            if (common_vt == WASM_TYPE_F32) wasm_buf_append_u8(&fn->body, WASM_OP_F32_SUB);
            else if (common_vt == WASM_TYPE_F64) wasm_buf_append_u8(&fn->body, WASM_OP_F64_SUB);
            else if (common_vt == WASM_TYPE_I64) wasm_buf_append_u8(&fn->body, WASM_OP_I64_SUB);
            else wasm_buf_append_u8(&fn->body, WASM_OP_I32_SUB);
            break;
        }

        case ND_MUL:
        case ND_FMUL: {
            uint8_t lhs_vt = wasm_node_valtype(node->lhs);
            uint8_t rhs_vt = wasm_node_valtype(node->rhs);
            Type *lhs_type = node->lhs ? node->lhs->type : NULL;
            Type *rhs_type = node->rhs ? node->rhs->type : NULL;
            uint8_t common_vt = wasm_promoted_valtype(lhs_vt, rhs_vt);

            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_emit_type_conversion(&fn->body, lhs_vt, lhs_type, common_vt, node->type);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_type_conversion(&fn->body, rhs_vt, rhs_type, common_vt, node->type);

            if (common_vt == WASM_TYPE_F32) wasm_buf_append_u8(&fn->body, WASM_OP_F32_MUL);
            else if (common_vt == WASM_TYPE_F64) wasm_buf_append_u8(&fn->body, WASM_OP_F64_MUL);
            else if (common_vt == WASM_TYPE_I64) wasm_buf_append_u8(&fn->body, WASM_OP_I64_MUL);
            else wasm_buf_append_u8(&fn->body, WASM_OP_I32_MUL);
            break;
        }

        case ND_DIV:
        case ND_FDIV: {
            uint8_t lhs_vt = wasm_node_valtype(node->lhs);
            uint8_t rhs_vt = wasm_node_valtype(node->rhs);
            Type *lhs_type = node->lhs ? node->lhs->type : NULL;
            Type *rhs_type = node->rhs ? node->rhs->type : NULL;
            uint8_t common_vt = wasm_promoted_valtype(lhs_vt, rhs_vt);
            int is_uns = wasm_type_is_unsigned_or_ptr(node->type) ||
                         wasm_type_is_unsigned_or_ptr(lhs_type) ||
                         wasm_type_is_unsigned_or_ptr(rhs_type);

            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_emit_type_conversion(&fn->body, lhs_vt, lhs_type, common_vt, node->type);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_type_conversion(&fn->body, rhs_vt, rhs_type, common_vt, node->type);

            if (common_vt == WASM_TYPE_F32) wasm_buf_append_u8(&fn->body, WASM_OP_F32_DIV);
            else if (common_vt == WASM_TYPE_F64) wasm_buf_append_u8(&fn->body, WASM_OP_F64_DIV);
            else if (common_vt == WASM_TYPE_I64) {
                wasm_buf_append_u8(&fn->body, is_uns ? WASM_OP_I64_DIV_U : WASM_OP_I64_DIV_S);
            } else {
                wasm_buf_append_u8(&fn->body, is_uns ? WASM_OP_I32_DIV_U : WASM_OP_I32_DIV_S);
            }
            break;
        }

        case ND_MOD: {
            uint8_t lhs_vt = wasm_node_valtype(node->lhs);
            uint8_t rhs_vt = wasm_node_valtype(node->rhs);
            Type *lhs_type = node->lhs ? node->lhs->type : NULL;
            Type *rhs_type = node->rhs ? node->rhs->type : NULL;
            uint8_t common_vt = wasm_promoted_valtype(lhs_vt, rhs_vt);
            int is_uns = wasm_type_is_unsigned_or_ptr(node->type) ||
                         wasm_type_is_unsigned_or_ptr(lhs_type) ||
                         wasm_type_is_unsigned_or_ptr(rhs_type);

            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_emit_type_conversion(&fn->body, lhs_vt, lhs_type, common_vt, node->type);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_type_conversion(&fn->body, rhs_vt, rhs_type, common_vt, node->type);

            if (common_vt == WASM_TYPE_I64) {
                wasm_buf_append_u8(&fn->body, is_uns ? WASM_OP_I64_REM_U : WASM_OP_I64_REM_S);
            } else {
                wasm_buf_append_u8(&fn->body, is_uns ? WASM_OP_I32_REM_U : WASM_OP_I32_REM_S);
            }
            break;
        }

        case ND_BAND: {
            uint8_t lhs_vt = wasm_node_valtype(node->lhs);
            uint8_t rhs_vt = wasm_node_valtype(node->rhs);
            Type *lhs_type = node->lhs ? node->lhs->type : NULL;
            Type *rhs_type = node->rhs ? node->rhs->type : NULL;
            uint8_t common_vt = wasm_promoted_valtype(lhs_vt, rhs_vt);

            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_emit_type_conversion(&fn->body, lhs_vt, lhs_type, common_vt, node->type);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_type_conversion(&fn->body, rhs_vt, rhs_type, common_vt, node->type);
            wasm_buf_append_u8(&fn->body, common_vt == WASM_TYPE_I64 ? WASM_OP_I64_AND : WASM_OP_I32_AND);
            break;
        }

        case ND_BOR: {
            uint8_t lhs_vt = wasm_node_valtype(node->lhs);
            uint8_t rhs_vt = wasm_node_valtype(node->rhs);
            Type *lhs_type = node->lhs ? node->lhs->type : NULL;
            Type *rhs_type = node->rhs ? node->rhs->type : NULL;
            uint8_t common_vt = wasm_promoted_valtype(lhs_vt, rhs_vt);

            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_emit_type_conversion(&fn->body, lhs_vt, lhs_type, common_vt, node->type);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_type_conversion(&fn->body, rhs_vt, rhs_type, common_vt, node->type);
            wasm_buf_append_u8(&fn->body, common_vt == WASM_TYPE_I64 ? WASM_OP_I64_OR : WASM_OP_I32_OR);
            break;
        }

        case ND_BXOR: {
            uint8_t lhs_vt = wasm_node_valtype(node->lhs);
            uint8_t rhs_vt = wasm_node_valtype(node->rhs);
            Type *lhs_type = node->lhs ? node->lhs->type : NULL;
            Type *rhs_type = node->rhs ? node->rhs->type : NULL;
            uint8_t common_vt = wasm_promoted_valtype(lhs_vt, rhs_vt);

            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_emit_type_conversion(&fn->body, lhs_vt, lhs_type, common_vt, node->type);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_type_conversion(&fn->body, rhs_vt, rhs_type, common_vt, node->type);
            wasm_buf_append_u8(&fn->body, common_vt == WASM_TYPE_I64 ? WASM_OP_I64_XOR : WASM_OP_I32_XOR);
            break;
        }

        case ND_SHL: {
            uint8_t lhs_vt = wasm_node_valtype(node->lhs);
            uint8_t rhs_vt = wasm_node_valtype(node->rhs);
            Type *lhs_type = node->lhs ? node->lhs->type : NULL;
            Type *rhs_type = node->rhs ? node->rhs->type : NULL;

            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_type_conversion(&fn->body, rhs_vt, rhs_type, lhs_vt, lhs_type);
            wasm_buf_append_u8(&fn->body, lhs_vt == WASM_TYPE_I64 ? WASM_OP_I64_SHL : WASM_OP_I32_SHL);
            break;
        }

        case ND_SHR: {
            uint8_t lhs_vt = wasm_node_valtype(node->lhs);
            uint8_t rhs_vt = wasm_node_valtype(node->rhs);
            Type *lhs_type = node->lhs ? node->lhs->type : NULL;
            Type *rhs_type = node->rhs ? node->rhs->type : NULL;
            int is_uns = wasm_type_is_unsigned_or_ptr(lhs_type) || wasm_type_is_unsigned_or_ptr(node->type);

            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_type_conversion(&fn->body, rhs_vt, rhs_type, lhs_vt, lhs_type);
            if (lhs_vt == WASM_TYPE_I64) {
                wasm_buf_append_u8(&fn->body, is_uns ? WASM_OP_I64_SHR_U : WASM_OP_I64_SHR_S);
            } else {
                wasm_buf_append_u8(&fn->body, is_uns ? WASM_OP_I32_SHR_U : WASM_OP_I32_SHR_S);
            }
            break;
        }

        case ND_EQ: {
            uint8_t lhs_vt = wasm_node_valtype(node->lhs);
            uint8_t rhs_vt = wasm_node_valtype(node->rhs);
            Type *lhs_type = node->lhs ? node->lhs->type : NULL;
            Type *rhs_type = node->rhs ? node->rhs->type : NULL;
            uint8_t common_vt = wasm_promoted_valtype(lhs_vt, rhs_vt);
            int cmp_uns = wasm_type_is_unsigned_or_ptr(lhs_type) || wasm_type_is_unsigned_or_ptr(rhs_type);

            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_emit_numeric_conversion(&fn->body, lhs_vt, common_vt, cmp_uns || wasm_type_is_unsigned_or_ptr(lhs_type));
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_numeric_conversion(&fn->body, rhs_vt, common_vt, cmp_uns || wasm_type_is_unsigned_or_ptr(rhs_type));

            if (common_vt == WASM_TYPE_F32) wasm_buf_append_u8(&fn->body, WASM_OP_F32_EQ);
            else if (common_vt == WASM_TYPE_F64) wasm_buf_append_u8(&fn->body, WASM_OP_F64_EQ);
            else if (common_vt == WASM_TYPE_I64) wasm_buf_append_u8(&fn->body, WASM_OP_I64_EQ);
            else wasm_buf_append_u8(&fn->body, WASM_OP_I32_EQ);
            break;
        }

        case ND_NE: {
            uint8_t lhs_vt = wasm_node_valtype(node->lhs);
            uint8_t rhs_vt = wasm_node_valtype(node->rhs);
            Type *lhs_type = node->lhs ? node->lhs->type : NULL;
            Type *rhs_type = node->rhs ? node->rhs->type : NULL;
            uint8_t common_vt = wasm_promoted_valtype(lhs_vt, rhs_vt);
            int cmp_uns = wasm_type_is_unsigned_or_ptr(lhs_type) || wasm_type_is_unsigned_or_ptr(rhs_type);

            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_emit_numeric_conversion(&fn->body, lhs_vt, common_vt, cmp_uns || wasm_type_is_unsigned_or_ptr(lhs_type));
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_numeric_conversion(&fn->body, rhs_vt, common_vt, cmp_uns || wasm_type_is_unsigned_or_ptr(rhs_type));

            if (common_vt == WASM_TYPE_F32) wasm_buf_append_u8(&fn->body, WASM_OP_F32_NE);
            else if (common_vt == WASM_TYPE_F64) wasm_buf_append_u8(&fn->body, WASM_OP_F64_NE);
            else if (common_vt == WASM_TYPE_I64) wasm_buf_append_u8(&fn->body, WASM_OP_I64_NE);
            else wasm_buf_append_u8(&fn->body, WASM_OP_I32_NE);
            break;
        }

        case ND_LT: {
            uint8_t lhs_vt = wasm_node_valtype(node->lhs);
            uint8_t rhs_vt = wasm_node_valtype(node->rhs);
            Type *lhs_type = node->lhs ? node->lhs->type : NULL;
            Type *rhs_type = node->rhs ? node->rhs->type : NULL;
            uint8_t common_vt = wasm_promoted_valtype(lhs_vt, rhs_vt);
            int is_uns = wasm_type_is_unsigned_or_ptr(lhs_type) || wasm_type_is_unsigned_or_ptr(rhs_type);

            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_emit_numeric_conversion(&fn->body, lhs_vt, common_vt, is_uns || wasm_type_is_unsigned_or_ptr(lhs_type));
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_numeric_conversion(&fn->body, rhs_vt, common_vt, is_uns || wasm_type_is_unsigned_or_ptr(rhs_type));

            if (common_vt == WASM_TYPE_F32) wasm_buf_append_u8(&fn->body, WASM_OP_F32_LT);
            else if (common_vt == WASM_TYPE_F64) wasm_buf_append_u8(&fn->body, WASM_OP_F64_LT);
            else if (common_vt == WASM_TYPE_I64) wasm_buf_append_u8(&fn->body, is_uns ? WASM_OP_I64_LT_U : WASM_OP_I64_LT_S);
            else wasm_buf_append_u8(&fn->body, is_uns ? WASM_OP_I32_LT_U : WASM_OP_I32_LT_S);
            break;
        }

        case ND_LE: {
            uint8_t lhs_vt = wasm_node_valtype(node->lhs);
            uint8_t rhs_vt = wasm_node_valtype(node->rhs);
            Type *lhs_type = node->lhs ? node->lhs->type : NULL;
            Type *rhs_type = node->rhs ? node->rhs->type : NULL;
            uint8_t common_vt = wasm_promoted_valtype(lhs_vt, rhs_vt);
            int is_uns = wasm_type_is_unsigned_or_ptr(lhs_type) || wasm_type_is_unsigned_or_ptr(rhs_type);

            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_emit_numeric_conversion(&fn->body, lhs_vt, common_vt, is_uns || wasm_type_is_unsigned_or_ptr(lhs_type));
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_numeric_conversion(&fn->body, rhs_vt, common_vt, is_uns || wasm_type_is_unsigned_or_ptr(rhs_type));

            if (common_vt == WASM_TYPE_F32) wasm_buf_append_u8(&fn->body, WASM_OP_F32_LE);
            else if (common_vt == WASM_TYPE_F64) wasm_buf_append_u8(&fn->body, WASM_OP_F64_LE);
            else if (common_vt == WASM_TYPE_I64) wasm_buf_append_u8(&fn->body, is_uns ? WASM_OP_I64_LE_U : WASM_OP_I64_LE_S);
            else wasm_buf_append_u8(&fn->body, is_uns ? WASM_OP_I32_LE_U : WASM_OP_I32_LE_S);
            break;
        }

        case ND_GT: {
            uint8_t lhs_vt = wasm_node_valtype(node->lhs);
            uint8_t rhs_vt = wasm_node_valtype(node->rhs);
            Type *lhs_type = node->lhs ? node->lhs->type : NULL;
            Type *rhs_type = node->rhs ? node->rhs->type : NULL;
            uint8_t common_vt = wasm_promoted_valtype(lhs_vt, rhs_vt);
            int is_uns = wasm_type_is_unsigned_or_ptr(lhs_type) || wasm_type_is_unsigned_or_ptr(rhs_type);

            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_emit_numeric_conversion(&fn->body, lhs_vt, common_vt, is_uns || wasm_type_is_unsigned_or_ptr(lhs_type));
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_numeric_conversion(&fn->body, rhs_vt, common_vt, is_uns || wasm_type_is_unsigned_or_ptr(rhs_type));

            if (common_vt == WASM_TYPE_F32) wasm_buf_append_u8(&fn->body, WASM_OP_F32_GT);
            else if (common_vt == WASM_TYPE_F64) wasm_buf_append_u8(&fn->body, WASM_OP_F64_GT);
            else if (common_vt == WASM_TYPE_I64) wasm_buf_append_u8(&fn->body, is_uns ? WASM_OP_I64_GT_U : WASM_OP_I64_GT_S);
            else wasm_buf_append_u8(&fn->body, is_uns ? WASM_OP_I32_GT_U : WASM_OP_I32_GT_S);
            break;
        }

        case ND_GE: {
            uint8_t lhs_vt = wasm_node_valtype(node->lhs);
            uint8_t rhs_vt = wasm_node_valtype(node->rhs);
            Type *lhs_type = node->lhs ? node->lhs->type : NULL;
            Type *rhs_type = node->rhs ? node->rhs->type : NULL;
            uint8_t common_vt = wasm_promoted_valtype(lhs_vt, rhs_vt);
            int is_uns = wasm_type_is_unsigned_or_ptr(lhs_type) || wasm_type_is_unsigned_or_ptr(rhs_type);

            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_emit_numeric_conversion(&fn->body, lhs_vt, common_vt, is_uns || wasm_type_is_unsigned_or_ptr(lhs_type));
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_numeric_conversion(&fn->body, rhs_vt, common_vt, is_uns || wasm_type_is_unsigned_or_ptr(rhs_type));

            if (common_vt == WASM_TYPE_F32) wasm_buf_append_u8(&fn->body, WASM_OP_F32_GE);
            else if (common_vt == WASM_TYPE_F64) wasm_buf_append_u8(&fn->body, WASM_OP_F64_GE);
            else if (common_vt == WASM_TYPE_I64) wasm_buf_append_u8(&fn->body, is_uns ? WASM_OP_I64_GE_U : WASM_OP_I64_GE_S);
            else wasm_buf_append_u8(&fn->body, is_uns ? WASM_OP_I32_GE_U : WASM_OP_I32_GE_S);
            break;
        }

        case ND_LNOT: {
            uint8_t vt = wasm_node_valtype(node->lhs);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            if (vt == WASM_TYPE_I64) {
                wasm_buf_append_u8(&fn->body, WASM_OP_I64_EQZ);
            } else if (vt == WASM_TYPE_F32) {
                wasm_buf_append_u8(&fn->body, WASM_OP_F32_CONST);
                wasm_buf_append_f32(&fn->body, 0.0f);
                wasm_buf_append_u8(&fn->body, WASM_OP_F32_EQ);
            } else if (vt == WASM_TYPE_F64) {
                wasm_buf_append_u8(&fn->body, WASM_OP_F64_CONST);
                wasm_buf_append_f64(&fn->body, 0.0);
                wasm_buf_append_u8(&fn->body, WASM_OP_F64_EQ);
            } else {
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_EQZ);
            }
            break;
        }

        case ND_BNOT: {
            uint8_t vt = wasm_node_valtype(node->lhs);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            if (vt == WASM_TYPE_I64) {
                wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                wasm_buf_append_sleb128_i64(&fn->body, -1);
                wasm_buf_append_u8(&fn->body, WASM_OP_I64_XOR);
            } else {
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                wasm_buf_append_sleb128(&fn->body, -1);
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_XOR);
            }
            break;
        }

        case ND_NEG: {
            uint8_t vt = wasm_node_valtype(node->lhs);
            if (vt == WASM_TYPE_F32) {
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
                wasm_buf_append_u8(&fn->body, WASM_OP_F32_NEG);
            } else if (vt == WASM_TYPE_F64) {
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
                wasm_buf_append_u8(&fn->body, WASM_OP_F64_NEG);
            } else if (vt == WASM_TYPE_I64) {
                wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                wasm_buf_append_sleb128_i64(&fn->body, 0);
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
                wasm_buf_append_u8(&fn->body, WASM_OP_I64_SUB);
            } else {
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                wasm_buf_append_sleb128(&fn->body, 0);
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_SUB);
            }
            break;
        }

        case ND_PRE_INC:
        case ND_PRE_DEC:
        case ND_POST_INC:
        case ND_POST_DEC: {
            int is_inc = (node->kind == ND_PRE_INC || node->kind == ND_POST_INC);
            int is_post = (node->kind == ND_POST_INC || node->kind == ND_POST_DEC);
            uint8_t vt = wasm_node_valtype(node->lhs);
            Type *ltype = node->lhs ? node->lhs->type : NULL;
            int step = (ltype && (is_pointer(ltype) || ltype->kind == TY_ARRAY)) ? wasm_ptr_elem_size(ltype) : 1;

            if (node->lhs && node->lhs->kind == ND_VAR) {
                const char *vname = node->lhs->sym ? node->lhs->sym->name : node->lhs->name;
                WasmLocalEntry *entry = wasm_local_find_entry(lmap, vname);
                if (entry && !entry->is_memory) {
                    if (is_post) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                        wasm_buf_append_uleb128(&fn->body, entry->local_idx);
                    }
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, entry->local_idx);
                    if (entry->val_type == WASM_TYPE_I64) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                        wasm_buf_append_sleb128_i64(&fn->body, (int64_t)step);
                        wasm_buf_append_u8(&fn->body, is_inc ? WASM_OP_I64_ADD : WASM_OP_I64_SUB);
                    } else if (entry->val_type == WASM_TYPE_F32) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_F32_CONST);
                        wasm_buf_append_f32(&fn->body, 1.0f);
                        wasm_buf_append_u8(&fn->body, is_inc ? WASM_OP_F32_ADD : WASM_OP_F32_SUB);
                    } else if (entry->val_type == WASM_TYPE_F64) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_F64_CONST);
                        wasm_buf_append_f64(&fn->body, 1.0);
                        wasm_buf_append_u8(&fn->body, is_inc ? WASM_OP_F64_ADD : WASM_OP_F64_SUB);
                    } else {
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                        wasm_buf_append_sleb128(&fn->body, step);
                        wasm_buf_append_u8(&fn->body, is_inc ? WASM_OP_I32_ADD : WASM_OP_I32_SUB);
                    }
                    if (is_post) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                        wasm_buf_append_uleb128(&fn->body, entry->local_idx);
                    } else {
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_TEE);
                        wasm_buf_append_uleb128(&fn->body, entry->local_idx);
                    }
                    break;
                }
            }

            /* Memory lvalue increment/decrement */
            int tmp_idx = (vt == WASM_TYPE_I64) ? lmap->tmp_i64_idx
                        : (vt == WASM_TYPE_F32) ? lmap->tmp_f32_idx
                        : (vt == WASM_TYPE_F64) ? lmap->tmp_f64_idx
                        : lmap->tmp_val_idx;
            wasm_lower_addr(cc, mod, fn, lmap, gmap, node->lhs);
            if (lmap->tmp_addr_idx >= 0) {
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_addr_idx);
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_addr_idx);
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_addr_idx);
            }
            wasm_emit_typed_load(&fn->body, ltype, 0);
            if (is_post && tmp_idx >= 0) {
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_TEE);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)tmp_idx);
            }
            if (vt == WASM_TYPE_I64) {
                wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                wasm_buf_append_sleb128_i64(&fn->body, (int64_t)step);
                wasm_buf_append_u8(&fn->body, is_inc ? WASM_OP_I64_ADD : WASM_OP_I64_SUB);
            } else if (vt == WASM_TYPE_F32) {
                wasm_buf_append_u8(&fn->body, WASM_OP_F32_CONST);
                wasm_buf_append_f32(&fn->body, 1.0f);
                wasm_buf_append_u8(&fn->body, is_inc ? WASM_OP_F32_ADD : WASM_OP_F32_SUB);
            } else if (vt == WASM_TYPE_F64) {
                wasm_buf_append_u8(&fn->body, WASM_OP_F64_CONST);
                wasm_buf_append_f64(&fn->body, 1.0);
                wasm_buf_append_u8(&fn->body, is_inc ? WASM_OP_F64_ADD : WASM_OP_F64_SUB);
            } else {
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                wasm_buf_append_sleb128(&fn->body, step);
                wasm_buf_append_u8(&fn->body, is_inc ? WASM_OP_I32_ADD : WASM_OP_I32_SUB);
            }
            if (!is_post && tmp_idx >= 0) {
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_TEE);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)tmp_idx);
                wasm_emit_typed_store(&fn->body, ltype, 0);
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)tmp_idx);
            } else if (is_post && tmp_idx >= 0) {
                wasm_emit_typed_store(&fn->body, ltype, 0);
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)tmp_idx);
            }
            break;
        }

        case ND_LAND: {
            uint8_t l_vt = wasm_node_valtype(node->lhs);
            uint8_t r_vt = wasm_node_valtype(node->rhs);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_emit_to_bool_i32(&fn->body, l_vt);
            wasm_buf_append_u8(&fn->body, WASM_OP_IF);
            wasm_buf_append_u8(&fn->body, WASM_TYPE_I32);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_to_bool_i32(&fn->body, r_vt);
            wasm_buf_append_u8(&fn->body, WASM_OP_ELSE);
            wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
            wasm_buf_append_sleb128(&fn->body, 0);
            wasm_buf_append_u8(&fn->body, WASM_OP_END);
            break;
        }

        case ND_LOR: {
            uint8_t l_vt = wasm_node_valtype(node->lhs);
            uint8_t r_vt = wasm_node_valtype(node->rhs);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_emit_to_bool_i32(&fn->body, l_vt);
            wasm_buf_append_u8(&fn->body, WASM_OP_IF);
            wasm_buf_append_u8(&fn->body, WASM_TYPE_I32);
            wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
            wasm_buf_append_sleb128(&fn->body, 1);
            wasm_buf_append_u8(&fn->body, WASM_OP_ELSE);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_to_bool_i32(&fn->body, r_vt);
            wasm_buf_append_u8(&fn->body, WASM_OP_END);
            break;
        }

        case ND_COMMA_EXPR:
            if (node->lhs) {
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
                wasm_buf_append_u8(&fn->body, WASM_OP_DROP);
            }
            if (node->rhs) {
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            }
            break;

        case ND_CAST: {
            Type *src_type = node->lhs ? node->lhs->type : NULL;
            Type *dst_type = node->type ? node->type : node->cast_type;
            uint8_t src_vt = wasm_node_valtype(node->lhs);
            uint8_t dst_vt = wasm_valtype_from_ctype(dst_type);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_emit_type_conversion(&fn->body, src_vt, src_type, dst_vt, dst_type);
            break;
        }

        case ND_CALL: {
            if (strncmp(node->func_name, "__zcc_wasm_", 11) == 0) {
                const char *sname = node->func_name + 11;
                uint32_t simd_op = 0;
                int is_bin_mem = 0, is_splat_mem = 0;
                if (strcmp(sname, "i32x4_add_mem") == 0) { simd_op = WASM_SIMD_I32X4_ADD; is_bin_mem = 1; }
                else if (strcmp(sname, "i32x4_sub_mem") == 0) { simd_op = WASM_SIMD_I32X4_SUB; is_bin_mem = 1; }
                else if (strcmp(sname, "i32x4_mul_mem") == 0) { simd_op = WASM_SIMD_I32X4_MUL; is_bin_mem = 1; }
                else if (strcmp(sname, "i32x4_dot_i16x8_s_mem") == 0) { simd_op = WASM_SIMD_I32X4_DOT_I16X8_S; is_bin_mem = 1; }
                else if (strcmp(sname, "f32x4_add_mem") == 0) { simd_op = WASM_SIMD_F32X4_ADD; is_bin_mem = 1; }
                else if (strcmp(sname, "f32x4_sub_mem") == 0) { simd_op = WASM_SIMD_F32X4_SUB; is_bin_mem = 1; }
                else if (strcmp(sname, "f32x4_mul_mem") == 0) { simd_op = WASM_SIMD_F32X4_MUL; is_bin_mem = 1; }
                else if (strcmp(sname, "v128_and_mem") == 0) { simd_op = WASM_SIMD_V128_AND; is_bin_mem = 1; }
                else if (strcmp(sname, "v128_or_mem") == 0) { simd_op = WASM_SIMD_V128_OR; is_bin_mem = 1; }
                else if (strcmp(sname, "v128_xor_mem") == 0) { simd_op = WASM_SIMD_V128_XOR; is_bin_mem = 1; }
                else if (strcmp(sname, "i32x4_splat_mem") == 0) { simd_op = WASM_SIMD_I32X4_SPLAT; is_splat_mem = 1; }
                else if (strcmp(sname, "f32x4_splat_mem") == 0) { simd_op = WASM_SIMD_F32X4_SPLAT; is_splat_mem = 2; }

                if (is_bin_mem && node->num_args == 3) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I32, cc->ty_int);
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[1]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[1]), node->args[1]->type, WASM_TYPE_I32, cc->ty_int);
                    wasm_buf_append_u8(&fn->body, WASM_OP_SIMD_PREFIX);
                    wasm_buf_append_uleb128(&fn->body, WASM_SIMD_V128_LOAD);
                    wasm_emit_memarg_safe(&fn->body, 4, 0);
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[2]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[2]), node->args[2]->type, WASM_TYPE_I32, cc->ty_int);
                    wasm_buf_append_u8(&fn->body, WASM_OP_SIMD_PREFIX);
                    wasm_buf_append_uleb128(&fn->body, WASM_SIMD_V128_LOAD);
                    wasm_emit_memarg_safe(&fn->body, 4, 0);
                    wasm_buf_append_u8(&fn->body, WASM_OP_SIMD_PREFIX);
                    wasm_buf_append_uleb128(&fn->body, simd_op);
                    wasm_buf_append_u8(&fn->body, WASM_OP_SIMD_PREFIX);
                    wasm_buf_append_uleb128(&fn->body, WASM_SIMD_V128_STORE);
                    wasm_emit_memarg_safe(&fn->body, 4, 0);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 0);
                    break;
                } else if (is_splat_mem && node->num_args == 2) {
                    uint8_t lane_vt = (is_splat_mem == 2) ? WASM_TYPE_F32 : WASM_TYPE_I32;
                    Type *lane_ty = (is_splat_mem == 2) ? cc->ty_float : cc->ty_int;
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I32, cc->ty_int);
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[1]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[1]), node->args[1]->type, lane_vt, lane_ty);
                    wasm_buf_append_u8(&fn->body, WASM_OP_SIMD_PREFIX);
                    wasm_buf_append_uleb128(&fn->body, simd_op);
                    wasm_buf_append_u8(&fn->body, WASM_OP_SIMD_PREFIX);
                    wasm_buf_append_uleb128(&fn->body, WASM_SIMD_V128_STORE);
                    wasm_emit_memarg_safe(&fn->body, 4, 0);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 0);
                    break;
                } else if (strcmp(sname, "i32x4_extract_lane_mem") == 0 && node->num_args == 2) {
                    int lane_idx = (node->args[1] && node->args[1]->kind == ND_NUM) ? ((int)node->args[1]->int_val & 3) : 0;
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I32, cc->ty_int);
                    wasm_buf_append_u8(&fn->body, WASM_OP_SIMD_PREFIX);
                    wasm_buf_append_uleb128(&fn->body, WASM_SIMD_V128_LOAD);
                    wasm_emit_memarg_safe(&fn->body, 4, 0);
                    wasm_buf_append_u8(&fn->body, WASM_OP_SIMD_PREFIX);
                    wasm_buf_append_uleb128(&fn->body, WASM_SIMD_I32X4_EXTRACT_LANE);
                    wasm_buf_append_u8(&fn->body, (uint8_t)lane_idx);
                    break;
                }
            }
            if (strncmp(node->func_name, "__builtin_", 10) == 0 && node->num_args == 1 && node->args && node->args[0]) {
                const char *bfn = node->func_name + 10;
                if (strcmp(bfn, "clz") == 0 || strcmp(bfn, "ctz") == 0 || strcmp(bfn, "popcount") == 0) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I32, cc->ty_uint);
                    wasm_buf_append_u8(&fn->body, (strcmp(bfn, "clz") == 0) ? WASM_OP_I32_CLZ : ((strcmp(bfn, "ctz") == 0) ? WASM_OP_I32_CTZ : WASM_OP_I32_POPCNT));
                    break;
                }
                if (strcmp(bfn, "clzl") == 0 || strcmp(bfn, "clzll") == 0 ||
                    strcmp(bfn, "ctzl") == 0 || strcmp(bfn, "ctzll") == 0 ||
                    strcmp(bfn, "popcountl") == 0 || strcmp(bfn, "popcountll") == 0) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I64, cc->ty_ulonglong);
                    wasm_buf_append_u8(&fn->body, (strncmp(bfn, "clz", 3) == 0) ? WASM_OP_I64_CLZ : ((strncmp(bfn, "ctz", 3) == 0) ? WASM_OP_I64_CTZ : WASM_OP_I64_POPCNT));
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_WRAP_I64);
                    break;
                }
                if (strcmp(bfn, "parity") == 0) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I32, cc->ty_uint);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_POPCNT);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 1);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND);
                    break;
                }
                if (strcmp(bfn, "parityl") == 0 || strcmp(bfn, "parityll") == 0) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I64, cc->ty_ulonglong);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_POPCNT);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_WRAP_I64);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 1);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND);
                    break;
                }
                if (strcmp(bfn, "ffs") == 0 && lmap->tmp_val_idx >= 0) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I32, cc->ty_uint);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_TEE);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CTZ);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 1);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 0);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_SELECT);
                    break;
                }
                if ((strcmp(bfn, "ffsl") == 0 || strcmp(bfn, "ffsll") == 0) && lmap->tmp_i64_idx >= 0) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I64, cc->ty_ulonglong);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_TEE);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_CTZ);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_WRAP_I64);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 1);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 0);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                    wasm_buf_append_sleb128_i64(&fn->body, 0);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_NE);
                    wasm_buf_append_u8(&fn->body, WASM_OP_SELECT);
                    break;
                }
                if (strcmp(bfn, "clrsb") == 0 && lmap->tmp_val_idx >= 0) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I32, cc->ty_int);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_TEE);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 31);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHR_S);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_XOR);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 1);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHL);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 1);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_OR);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CLZ);
                    break;
                }
                if ((strcmp(bfn, "clrsbl") == 0 || strcmp(bfn, "clrsbll") == 0) && lmap->tmp_i64_idx >= 0) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I64, cc->ty_longlong);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_TEE);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                    wasm_buf_append_sleb128_i64(&fn->body, 63);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_SHR_S);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_XOR);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                    wasm_buf_append_sleb128_i64(&fn->body, 1);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_SHL);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                    wasm_buf_append_sleb128_i64(&fn->body, 1);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_OR);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_CLZ);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_WRAP_I64);
                    break;
                }
                if (strcmp(bfn, "bswap16") == 0 && lmap->tmp_val_idx >= 0) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I32, cc->ty_ushort);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 255);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 8);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHL);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 8);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHR_U);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 255);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_OR);
                    break;
                }
                if (strcmp(bfn, "bswap32") == 0 && lmap->tmp_val_idx >= 0) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I32, cc->ty_uint);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                    /* ((v & 0xff) << 24) | (((v >> 8) & 0xff) << 16) | (((v >> 16) & 0xff) << 8) | ((v >> 24) & 0xff) */
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 255);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 24);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHL);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 8);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHR_U);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 255);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 16);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHL);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_OR);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 16);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHR_U);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 255);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 8);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHL);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_OR);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 24);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHR_U);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 255);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_OR);
                    break;
                }
                if (strcmp(bfn, "bswap64") == 0 && lmap->tmp_i64_idx >= 0) {
                    int b;
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I64, cc->ty_ulonglong);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                    wasm_buf_append_sleb128_i64(&fn->body, 0);
                    for (b = 0; b < 8; b++) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                        wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                        if (b > 0) {
                            wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                            wasm_buf_append_sleb128_i64(&fn->body, (int64_t)(b * 8));
                            wasm_buf_append_u8(&fn->body, WASM_OP_I64_SHR_U);
                        }
                        wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                        wasm_buf_append_sleb128_i64(&fn->body, 255);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I64_AND);
                        if ((7 - b) > 0) {
                            wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                            wasm_buf_append_sleb128_i64(&fn->body, (int64_t)((7 - b) * 8));
                            wasm_buf_append_u8(&fn->body, WASM_OP_I64_SHL);
                        }
                        wasm_buf_append_u8(&fn->body, WASM_OP_I64_OR);
                    }
                    break;
                }
                if (strcmp(bfn, "bitreverse32") == 0 && lmap->tmp_val_idx >= 0) {
                    static const int32_t m32[5] = { 0x55555555, 0x33333333, 0x0F0F0F0F, 0x00FF00FF, -1 };
                    static const int32_t s32[5] = { 1, 2, 4, 8, 16 };
                    int st;
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I32, cc->ty_uint);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                    for (st = 0; st < 5; st++) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                        wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                        wasm_buf_append_sleb128(&fn->body, s32[st]);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHR_U);
                        if (st < 4) {
                            wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                            wasm_buf_append_sleb128(&fn->body, m32[st]);
                            wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND);
                        }
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                        wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                        if (st < 4) {
                            wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                            wasm_buf_append_sleb128(&fn->body, m32[st]);
                            wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND);
                        }
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                        wasm_buf_append_sleb128(&fn->body, s32[st]);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHL);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_OR);
                        if (st < 4) {
                            wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                            wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                        }
                    }
                    break;
                }
                if (strcmp(bfn, "bitreverse64") == 0 && lmap->tmp_i64_idx >= 0) {
                    static const int64_t m64[6] = {
                        0x5555555555555555LL, 0x3333333333333333LL, 0x0F0F0F0F0F0F0F0FLL,
                        0x00FF00FF00FF00FFLL, 0x0000FFFF0000FFFFLL, -1LL
                    };
                    static const int64_t s64[6] = { 1, 2, 4, 8, 16, 32 };
                    int st;
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I64, cc->ty_ulonglong);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    for (st = 0; st < 6; st++) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                        wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                        wasm_buf_append_sleb128_i64(&fn->body, s64[st]);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I64_SHR_U);
                        if (st < 5) {
                            wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                            wasm_buf_append_sleb128_i64(&fn->body, m64[st]);
                            wasm_buf_append_u8(&fn->body, WASM_OP_I64_AND);
                        }
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                        wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                        if (st < 5) {
                            wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                            wasm_buf_append_sleb128_i64(&fn->body, m64[st]);
                            wasm_buf_append_u8(&fn->body, WASM_OP_I64_AND);
                        }
                        wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                        wasm_buf_append_sleb128_i64(&fn->body, s64[st]);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I64_SHL);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I64_OR);
                        if (st < 5) {
                            wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                            wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                        }
                    }
                    break;
                }
                if (strcmp(bfn, "qpauli_weight") == 0 && lmap->tmp_val_idx >= 0) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I32, cc->ty_uint);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_TEE);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 16);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHR_U);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_OR);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 65535);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_POPCNT);
                    break;
                }
            }
            if (strncmp(node->func_name, "__builtin_", 10) == 0 && node->num_args == 2 &&
                node->args && node->args[0] && node->args[1]) {
                const char *bfn = node->func_name + 10;
                if (strcmp(bfn, "sadd_overflow_p") == 0 || strcmp(bfn, "ssub_overflow_p") == 0) {
                    int is_sub = (bfn[1] == 's');
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I32, cc->ty_int);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_EXTEND_I32_S);
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[1]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[1]), node->args[1]->type, WASM_TYPE_I32, cc->ty_int);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_EXTEND_I32_S);
                    wasm_buf_append_u8(&fn->body, is_sub ? WASM_OP_I64_SUB : WASM_OP_I64_ADD);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_TEE);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_WRAP_I64);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_EXTEND_I32_S);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_NE);
                    break;
                }
                if (strcmp(bfn, "uadd_overflow_p") == 0) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I32, cc->ty_uint);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_EXTEND_I32_U);
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[1]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[1]), node->args[1]->type, WASM_TYPE_I32, cc->ty_uint);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_EXTEND_I32_U);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_ADD);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                    wasm_buf_append_sleb128_i64(&fn->body, 32);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_SHR_U);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_WRAP_I64);
                    break;
                }
                if (strcmp(bfn, "usub_overflow_p") == 0) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I32, cc->ty_uint);
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[1]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[1]), node->args[1]->type, WASM_TYPE_I32, cc->ty_uint);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_LT_U);
                    break;
                }
                if (strcmp(bfn, "smul_overflow_p") == 0 && lmap->tmp_i64_idx >= 0) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I32, cc->ty_int);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_EXTEND_I32_S);
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[1]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[1]), node->args[1]->type, WASM_TYPE_I32, cc->ty_int);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_EXTEND_I32_S);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_MUL);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_TEE);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_WRAP_I64);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_EXTEND_I32_S);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_NE);
                    break;
                }
                if (strcmp(bfn, "umul_overflow_p") == 0) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I32, cc->ty_uint);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_EXTEND_I32_U);
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[1]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[1]), node->args[1]->type, WASM_TYPE_I32, cc->ty_uint);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_EXTEND_I32_U);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_MUL);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                    wasm_buf_append_sleb128_i64(&fn->body, 32);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_SHR_U);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                    wasm_buf_append_sleb128_i64(&fn->body, 0);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_NE);
                    break;
                }
                if ((strcmp(bfn, "sadd_sat") == 0 || strcmp(bfn, "ssub_sat") == 0) && lmap->tmp_i64_idx >= 0) {
                    int is_sub = (bfn[1] == 's');
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I32, cc->ty_int);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_EXTEND_I32_S);
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[1]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[1]), node->args[1]->type, WASM_TYPE_I32, cc->ty_int);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_EXTEND_I32_S);
                    wasm_buf_append_u8(&fn->body, is_sub ? WASM_OP_I64_SUB : WASM_OP_I64_ADD);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    /* val1 (saturated): ((int32)(tmp_i64 >>s 63)) ^ 0x7FFFFFFF */
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                    wasm_buf_append_sleb128_i64(&fn->body, 63);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_SHR_S);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_WRAP_I64);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 2147483647);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_XOR);
                    /* val2 (unclipped): (int32)tmp_i64 */
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_WRAP_I64);
                    /* cond (overflow): (int64)(int32)tmp_i64 != tmp_i64 */
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_WRAP_I64);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_EXTEND_I32_S);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_NE);
                    wasm_buf_append_u8(&fn->body, WASM_OP_SELECT);
                    break;
                }
                if (strcmp(bfn, "uadd_sat") == 0 && lmap->tmp_i64_idx >= 0) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I32, cc->ty_uint);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_EXTEND_I32_U);
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[1]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[1]), node->args[1]->type, WASM_TYPE_I32, cc->ty_uint);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_EXTEND_I32_U);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_ADD);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, -1);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_WRAP_I64);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                    wasm_buf_append_sleb128_i64(&fn->body, 32);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_SHR_U);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_WRAP_I64);
                    wasm_buf_append_u8(&fn->body, WASM_OP_SELECT);
                    break;
                }
                if (strcmp(bfn, "usub_sat") == 0 && lmap->tmp_i64_idx >= 0) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I32, cc->ty_uint);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_EXTEND_I32_U);
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[1]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[1]), node->args[1]->type, WASM_TYPE_I32, cc->ty_uint);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_EXTEND_I32_U);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_SUB);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 0);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_WRAP_I64);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                    wasm_buf_append_sleb128_i64(&fn->body, 0);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_LT_S);
                    wasm_buf_append_u8(&fn->body, WASM_OP_SELECT);
                    break;
                }
                if (strcmp(bfn, "savg_floor") == 0 || strcmp(bfn, "savg_ceil") == 0 ||
                    strcmp(bfn, "uavg_floor") == 0 || strcmp(bfn, "uavg_ceil") == 0 ||
                    strcmp(bfn, "shadd") == 0 || strcmp(bfn, "srhadd") == 0 ||
                    strcmp(bfn, "uhadd") == 0 || strcmp(bfn, "urhadd") == 0) {
                    int is_u = (bfn[0] == 'u'), is_ceil = (bfn[1] == 'r' || (bfn[1] == 'a' && bfn[5] == 'c'));
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I32, is_u ? cc->ty_uint : cc->ty_int);
                    wasm_buf_append_u8(&fn->body, is_u ? WASM_OP_I64_EXTEND_I32_U : WASM_OP_I64_EXTEND_I32_S);
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[1]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[1]), node->args[1]->type, WASM_TYPE_I32, is_u ? cc->ty_uint : cc->ty_int);
                    wasm_buf_append_u8(&fn->body, is_u ? WASM_OP_I64_EXTEND_I32_U : WASM_OP_I64_EXTEND_I32_S);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_ADD);
                    if (is_ceil) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                        wasm_buf_append_sleb128_i64(&fn->body, 1);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I64_ADD);
                    }
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                    wasm_buf_append_sleb128_i64(&fn->body, 1);
                    wasm_buf_append_u8(&fn->body, is_u ? WASM_OP_I64_SHR_U : WASM_OP_I64_SHR_S);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_WRAP_I64);
                    break;
                }
                if ((strcmp(bfn, "usad32") == 0 || strcmp(bfn, "ssad32") == 0) && lmap->tmp_i64_idx >= 0) {
                    int is_u = (bfn[0] == 'u');
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I32, is_u ? cc->ty_uint : cc->ty_int);
                    wasm_buf_append_u8(&fn->body, is_u ? WASM_OP_I64_EXTEND_I32_U : WASM_OP_I64_EXTEND_I32_S);
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[1]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[1]), node->args[1]->type, WASM_TYPE_I32, is_u ? cc->ty_uint : cc->ty_int);
                    wasm_buf_append_u8(&fn->body, is_u ? WASM_OP_I64_EXTEND_I32_U : WASM_OP_I64_EXTEND_I32_S);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_SUB);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                    wasm_buf_append_sleb128_i64(&fn->body, 63);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_SHR_S);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_XOR);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                    wasm_buf_append_sleb128_i64(&fn->body, 63);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_SHR_S);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_SUB);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_WRAP_I64);
                    break;
                }
                if ((strcmp(bfn, "smin32") == 0 || strcmp(bfn, "smax32") == 0 ||
                     strcmp(bfn, "umin32") == 0 || strcmp(bfn, "umax32") == 0) &&
                    lmap->tmp_addr_idx >= 0 && lmap->tmp_val_idx >= 0) {
                    int is_u = (bfn[0] == 'u'), is_max = (bfn[2] == 'a');
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I32, is_u ? cc->ty_uint : cc->ty_int);
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[1]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[1]), node->args[1]->type, WASM_TYPE_I32, is_u ? cc->ty_uint : cc->ty_int);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_TEE);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_addr_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_addr_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                    wasm_buf_append_u8(&fn->body, is_u ? (is_max ? WASM_OP_I32_GT_U : WASM_OP_I32_LT_U)
                                                       : (is_max ? WASM_OP_I32_GT_S : WASM_OP_I32_LT_S));
                    wasm_buf_append_u8(&fn->body, WASM_OP_SELECT);
                    break;
                }
                if ((strcmp(bfn, "smul_sat") == 0 || strcmp(bfn, "umul_sat") == 0 ||
                     strcmp(bfn, "sshl_sat") == 0 || strcmp(bfn, "ushl_sat") == 0) && lmap->tmp_i64_idx >= 0) {
                    int is_u = (bfn[0] == 'u'), is_shl = (bfn[1] == 's');
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I32, is_u ? cc->ty_uint : cc->ty_int);
                    wasm_buf_append_u8(&fn->body, is_u ? WASM_OP_I64_EXTEND_I32_U : WASM_OP_I64_EXTEND_I32_S);
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[1]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[1]), node->args[1]->type, WASM_TYPE_I32, is_u ? cc->ty_uint : cc->ty_int);
                    if (is_shl) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                        wasm_buf_append_sleb128(&fn->body, 31);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND);
                    }
                    wasm_buf_append_u8(&fn->body, is_u ? WASM_OP_I64_EXTEND_I32_U : WASM_OP_I64_EXTEND_I32_S);
                    wasm_buf_append_u8(&fn->body, is_shl ? WASM_OP_I64_SHL : WASM_OP_I64_MUL);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    if (!is_u) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                        wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                        wasm_buf_append_sleb128_i64(&fn->body, 63);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I64_SHR_S);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_WRAP_I64);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                        wasm_buf_append_sleb128(&fn->body, 2147483647);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_XOR);
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                        wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_WRAP_I64);
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                        wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_WRAP_I64);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I64_EXTEND_I32_S);
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                        wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I64_NE);
                        wasm_buf_append_u8(&fn->body, WASM_OP_SELECT);
                    } else {
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                        wasm_buf_append_sleb128(&fn->body, -1);
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                        wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_WRAP_I64);
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                        wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                        wasm_buf_append_sleb128_i64(&fn->body, 32);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I64_SHR_U);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_WRAP_I64);
                        wasm_buf_append_u8(&fn->body, WASM_OP_SELECT);
                    }
                    break;
                }
                if (strcmp(bfn, "srshr") == 0 || strcmp(bfn, "urshr") == 0) {
                    int is_u = (bfn[0] == 'u');
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I32, is_u ? cc->ty_uint : cc->ty_int);
                    wasm_buf_append_u8(&fn->body, is_u ? WASM_OP_I64_EXTEND_I32_U : WASM_OP_I64_EXTEND_I32_S);
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[1]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[1]), node->args[1]->type, WASM_TYPE_I32, cc->ty_uint);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 31);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 1);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHL);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 1);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHR_U);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_EXTEND_I32_U);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_ADD);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_EXTEND_I32_U);
                    wasm_buf_append_u8(&fn->body, is_u ? WASM_OP_I64_SHR_U : WASM_OP_I64_SHR_S);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_WRAP_I64);
                    break;
                }
                if (strcmp(bfn, "qdmulh") == 0 || strcmp(bfn, "qrdmulh") == 0) {
                    int is_rnd = (bfn[1] == 'r');
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I32, cc->ty_int);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_EXTEND_I32_S);
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[1]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[1]), node->args[1]->type, WASM_TYPE_I32, cc->ty_int);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_EXTEND_I32_S);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_MUL);
                    if (is_rnd) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                        wasm_buf_append_sleb128_i64(&fn->body, 1073741824LL);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I64_ADD);
                    }
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                    wasm_buf_append_sleb128_i64(&fn->body, 31);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_SHR_S);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                    wasm_buf_append_sleb128_i64(&fn->body, 63);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_SHR_S);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_WRAP_I64);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 2147483647);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_XOR);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_WRAP_I64);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_WRAP_I64);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_EXTEND_I32_S);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_NE);
                    wasm_buf_append_u8(&fn->body, WASM_OP_SELECT);
                    break;
                }
                if (strcmp(bfn, "rotateleft32") == 0 || strcmp(bfn, "rotateright32") == 0) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I32, cc->ty_uint);
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[1]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[1]), node->args[1]->type, WASM_TYPE_I32, cc->ty_uint);
                    wasm_buf_append_u8(&fn->body, (strcmp(bfn, "rotateleft32") == 0) ? WASM_OP_I32_ROTL : WASM_OP_I32_ROTR);
                    break;
                }
                if (strcmp(bfn, "rotateleft64") == 0 || strcmp(bfn, "rotateright64") == 0) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I64, cc->ty_ulonglong);
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[1]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[1]), node->args[1]->type, WASM_TYPE_I64, cc->ty_ulonglong);
                    wasm_buf_append_u8(&fn->body, (strcmp(bfn, "rotateleft64") == 0) ? WASM_OP_I64_ROTL : WASM_OP_I64_ROTR);
                    break;
                }
                if (strcmp(bfn, "qpauli_commute") == 0 && lmap->tmp_addr_idx >= 0 && lmap->tmp_val_idx >= 0) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I32, cc->ty_uint);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_addr_idx);
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[1]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[1]), node->args[1]->type, WASM_TYPE_I32, cc->ty_uint);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_addr_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 16);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHR_U);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_addr_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 16);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHR_U);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_XOR);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 65535);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_POPCNT);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 1);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND);
                    break;
                }
                if (strcmp(bfn, "qpauli_mul") == 0) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I32, cc->ty_uint);
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[1]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[1]), node->args[1]->type, WASM_TYPE_I32, cc->ty_uint);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_XOR);
                    break;
                }
                if (strcmp(bfn, "qpauli_phase") == 0 && lmap->tmp_addr_idx >= 0 && lmap->tmp_val_idx >= 0) {
                    uint32_t la = (uint32_t)lmap->tmp_addr_idx, lb = (uint32_t)lmap->tmp_val_idx;
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_I32, cc->ty_uint);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET); wasm_buf_append_uleb128(&fn->body, la);
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[1]);
                    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[1]), node->args[1]->type, WASM_TYPE_I32, cc->ty_uint);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET); wasm_buf_append_uleb128(&fn->body, lb);
                    /* pos = ((a & (b >> 16) & (b ^ (a >> 16))) | ((a >> 16) & b & ~(a | (b >> 16)))) & 65535 */
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(&fn->body, la);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(&fn->body, lb);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST); wasm_buf_append_sleb128(&fn->body, 16); wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHR_U); wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(&fn->body, lb);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(&fn->body, la);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST); wasm_buf_append_sleb128(&fn->body, 16); wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHR_U); wasm_buf_append_u8(&fn->body, WASM_OP_I32_XOR); wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(&fn->body, la);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST); wasm_buf_append_sleb128(&fn->body, 16); wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHR_U);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(&fn->body, lb); wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(&fn->body, la);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(&fn->body, lb);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST); wasm_buf_append_sleb128(&fn->body, 16); wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHR_U); wasm_buf_append_u8(&fn->body, WASM_OP_I32_OR);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST); wasm_buf_append_sleb128(&fn->body, -1); wasm_buf_append_u8(&fn->body, WASM_OP_I32_XOR); wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_OR);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST); wasm_buf_append_sleb128(&fn->body, 65535); wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_POPCNT);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST); wasm_buf_append_sleb128(&fn->body, 1); wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHL);
                    /* anti = ((a & (b >> 16)) ^ ((a >> 16) & b)) & 65535 */
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(&fn->body, la);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(&fn->body, lb);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST); wasm_buf_append_sleb128(&fn->body, 16); wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHR_U); wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(&fn->body, la);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST); wasm_buf_append_sleb128(&fn->body, 16); wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHR_U);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(&fn->body, lb); wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_XOR);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST); wasm_buf_append_sleb128(&fn->body, 65535); wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_POPCNT);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_SUB);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST); wasm_buf_append_sleb128(&fn->body, 3); wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND);
                    break;
                }
            }
            if (node->num_args == 1 && node->args && node->args[0] &&
                wasm_find_func_idx(mod, node->func_name) < 0 &&
                (strcmp(node->func_name, "fabsf") == 0 || strcmp(node->func_name, "__builtin_fabsf") == 0 ||
                 strcmp(node->func_name, "sqrtf") == 0 || strcmp(node->func_name, "__builtin_sqrtf") == 0)) {
                int is_sqrt = (strcmp(node->func_name, "sqrtf") == 0 || strcmp(node->func_name, "__builtin_sqrtf") == 0);
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                wasm_emit_type_conversion(&fn->body, wasm_node_valtype(node->args[0]), node->args[0]->type, WASM_TYPE_F32, cc->ty_float);
                wasm_buf_append_u8(&fn->body, is_sqrt ? WASM_OP_F32_SQRT : WASM_OP_F32_ABS);
                break;
            }
            int callee_idx = node->func_name[0] ? wasm_find_func_idx(mod, node->func_name) : -1;
            int a;
            for (a = 0; a < node->num_args; a++) {
                uint8_t arg_vt = wasm_node_valtype(node->args[a]);
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[a]);
                if (callee_idx >= 0) {
                    /* If direct call, coerce argument to expected type if known */
                    int tidx = -1;
                    if (callee_idx < mod->num_imports) {
                        tidx = mod->imports[callee_idx].type_idx;
                    } else if (callee_idx - mod->num_imports < mod->num_funcs) {
                        tidx = mod->funcs[callee_idx - mod->num_imports].type_idx;
                    }
                    if (tidx >= 0 && a < mod->types[tidx].num_params) {
                        uint8_t param_vt = mod->types[tidx].params[a];
                        wasm_emit_type_conversion(&fn->body, arg_vt, node->args[a]->type, param_vt, node->args[a]->type);
                    }
                }
            }

            if (callee_idx >= 0) {
                /* Direct static function call using absolute function index */
                wasm_buf_append_u8(&fn->body, WASM_OP_CALL);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)callee_idx);
                {
                    int tidx = -1;
                    if (callee_idx < mod->num_imports) {
                        tidx = mod->imports[callee_idx].type_idx;
                    } else if (callee_idx - mod->num_imports < mod->num_funcs) {
                        tidx = mod->funcs[callee_idx - mod->num_imports].type_idx;
                    }
                    if (tidx >= 0 && mod->types[tidx].num_results > 0) {
                        uint8_t ret_vt = mod->types[tidx].results[0];
                        uint8_t expr_vt = wasm_node_valtype(node);
                        wasm_emit_type_conversion(&fn->body, ret_vt, NULL, expr_vt, node->type);
                    }
                }
            } else {
                /* Indirect function pointer call */
                if (node->lhs) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
                } else if (node->func_name[0]) {
                    WasmLocalEntry *ventry = wasm_local_find_entry(lmap, node->func_name);
                    if (ventry && !ventry->is_memory) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                        wasm_buf_append_uleb128(&fn->body, ventry->local_idx);
                    } else {
                        WasmGlobalVar *gv = wasm_global_find(gmap, node->func_name);
                        if (gv) {
                            wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                            wasm_buf_append_sleb128(&fn->body, (int32_t)gv->offset);
                            wasm_emit_typed_load(&fn->body, gv->type, 0);
                        } else {
                            wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                            wasm_buf_append_sleb128(&fn->body, 0);
                        }
                    }
                } else {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 0);
                }
                /* Find or synthesize type signature for call_indirect */
                uint8_t ptypes[16];
                int p;
                for (p = 0; p < node->num_args && p < 16; p++) {
                    ptypes[p] = wasm_node_valtype(node->args[p]);
                }
                uint8_t rtypes[1];
                int num_r = 0;
                if (node->type && node->type->kind != TY_VOID) {
                    rtypes[0] = wasm_valtype_from_ctype(node->type);
                    num_r = 1;
                } else {
                    rtypes[0] = WASM_TYPE_I32;
                    num_r = 1;
                }
                int call_type_idx = wasm_module_add_type((WasmModule *)mod, ptypes, node->num_args, rtypes, num_r);
                wasm_buf_append_u8(&fn->body, WASM_OP_CALL_INDIRECT);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)call_type_idx);
                wasm_buf_append_uleb128(&fn->body, 0); /* Table 0 */
            }
            break;
        }

        case ND_TERNARY: {
            uint8_t target_vt = wasm_node_valtype(node);
            uint8_t then_vt = wasm_node_valtype(node->then_body);
            uint8_t else_vt = wasm_node_valtype(node->else_body);

            /* Evaluate condition (must be i32 boolean) */
            uint8_t cond_vt = wasm_node_valtype(node->cond);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->cond);
            if (cond_vt != WASM_TYPE_I32) {
                wasm_emit_to_bool_i32(&fn->body, cond_vt);
            }

            /* WASM if block producing target_vt value */
            wasm_buf_append_u8(&fn->body, WASM_OP_IF);
            wasm_buf_append_u8(&fn->body, target_vt);

            if (node->then_body) {
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->then_body);
                wasm_emit_type_conversion(&fn->body, then_vt, node->then_body->type, target_vt, node->type);
            } else {
                if (target_vt == WASM_TYPE_F32) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_F32_CONST);
                    wasm_buf_append_f32(&fn->body, 0.0f);
                } else if (target_vt == WASM_TYPE_F64) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_F64_CONST);
                    wasm_buf_append_f64(&fn->body, 0.0);
                } else if (target_vt == WASM_TYPE_I64) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                    wasm_buf_append_sleb128_i64(&fn->body, 0);
                } else {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 0);
                }
            }

            wasm_buf_append_u8(&fn->body, WASM_OP_ELSE);

            if (node->else_body) {
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->else_body);
                wasm_emit_type_conversion(&fn->body, else_vt, node->else_body->type, target_vt, node->type);
            } else {
                if (target_vt == WASM_TYPE_F32) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_F32_CONST);
                    wasm_buf_append_f32(&fn->body, 0.0f);
                } else if (target_vt == WASM_TYPE_F64) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_F64_CONST);
                    wasm_buf_append_f64(&fn->body, 0.0);
                } else if (target_vt == WASM_TYPE_I64) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                    wasm_buf_append_sleb128_i64(&fn->body, 0);
                } else {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 0);
                }
            }

            wasm_buf_append_u8(&fn->body, WASM_OP_END);
            break;
        }

        default:
            if (node->lhs) wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            if (node->rhs) wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            break;
    }
}

static int wasm_stmt_block_depth = 0;
static int wasm_cur_break_depth = -1;
static int wasm_cur_cont_depth = -1;

typedef struct {
    char name[128];
    int flat_idx;
} WasmSwitchLabel;

static void wasm_flatten_switch_body(Node *stmt, Node **flat_stmts, int *num_flat, int max_flat,
                                     WasmSwitchLabel *labels, int *num_labels, int max_labels) {
    if (!stmt) return;
    if (stmt->kind == ND_BLOCK) {
        int i;
        for (i = 0; i < stmt->num_stmts; i++) {
            wasm_flatten_switch_body(stmt->stmts[i], flat_stmts, num_flat, max_flat, labels, num_labels, max_labels);
        }
        return;
    }
    while (stmt && stmt->kind == ND_LABEL) {
        if (*num_labels < max_labels) {
            strncpy(labels[*num_labels].name, stmt->label_name, sizeof(labels[*num_labels].name) - 1);
            labels[*num_labels].name[sizeof(labels[*num_labels].name) - 1] = '\0';
            labels[*num_labels].flat_idx = *num_flat;
            (*num_labels)++;
        }
        stmt = stmt->lhs;
    }
    if (stmt) {
        if (stmt->kind == ND_BLOCK) {
            wasm_flatten_switch_body(stmt, flat_stmts, num_flat, max_flat, labels, num_labels, max_labels);
        } else if (*num_flat < max_flat) {
            flat_stmts[(*num_flat)++] = stmt;
        }
    }
}

static Node *wasm_unwrap_cast(Node *n) {
    while (n && n->kind == ND_CAST) n = n->lhs;
    return n;
}

static Node *wasm_match_indexed_ptr(Node *deref_node, const char *ivar_name, int *out_elem_size, int *out_is_f32) {
    deref_node = wasm_unwrap_cast(deref_node);
    if (!deref_node || deref_node->kind != ND_DEREF || !deref_node->lhs) return NULL;
    Node *add = deref_node->lhs;
    if (add->kind != ND_ADD || !add->lhs || !add->rhs) return NULL;
    Type *elem_ty = deref_node->type;
    if (!elem_ty) return NULL;
    int esz = type_size(elem_ty);
    int is_f32 = (elem_ty->kind == TY_FLOAT);
    int is_i32 = (elem_ty->kind == TY_INT || elem_ty->kind == TY_UINT || elem_ty->kind == TY_ENUM);
    int is_i16 = (elem_ty->kind == TY_SHORT || elem_ty->kind == TY_USHORT);
    int is_u8  = (elem_ty->kind == TY_UCHAR);
    if (!((esz == 4 && (is_f32 || is_i32)) || (esz == 2 && is_i16) || (esz == 1 && is_u8))) return NULL;

    Node *base = NULL;
    Node *idx = NULL;
    if (add->rhs->kind == ND_VAR) {
        const char *rname = add->rhs->sym ? add->rhs->sym->name : add->rhs->name;
        if (rname && strcmp(rname, ivar_name) == 0) {
            base = add->lhs;
            idx = add->rhs;
        }
    }
    if (!base && add->lhs->kind == ND_VAR) {
        const char *lname = add->lhs->sym ? add->lhs->sym->name : add->lhs->name;
        if (lname && strcmp(lname, ivar_name) == 0) {
            base = add->rhs;
            idx = add->lhs;
        }
    }
    if (!base || !idx) return NULL;
    if (wasm_node_has_side_effects(base)) return NULL;
    *out_elem_size = esz;
    *out_is_f32 = is_f32;
    return base;
}

static void wasm_emit_indexed_addr_scaled(Compiler *cc, const WasmModule *mod, WasmFunction *fn,
                                          WasmLocalMap *lmap, WasmGlobalMap *gmap, Node *base, Node *ivar, int shift_amt) {
    if (base->kind == ND_VAR && base->type && base->type->kind == TY_ARRAY) {
        wasm_lower_addr(cc, mod, fn, lmap, gmap, base);
    } else {
        wasm_lower_expr(cc, mod, fn, lmap, gmap, base);
    }
    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(base), base->type, WASM_TYPE_I32, cc->ty_int);
    wasm_lower_expr(cc, mod, fn, lmap, gmap, ivar);
    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(ivar), ivar->type, WASM_TYPE_I32, cc->ty_int);
    if (shift_amt > 0) {
        wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&fn->body, shift_amt);
        wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHL);
    }
    wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD);
}

static void wasm_emit_indexed_addr4(Compiler *cc, const WasmModule *mod, WasmFunction *fn,
                                    WasmLocalMap *lmap, WasmGlobalMap *gmap, Node *base, Node *ivar) {
    wasm_emit_indexed_addr_scaled(cc, mod, fn, lmap, gmap, base, ivar, 2);
}

static void wasm_try_autovec_for_loop(Compiler *cc, const WasmModule *mod, WasmFunction *fn,
                                      WasmLocalMap *lmap, WasmGlobalMap *gmap, Node *node) {
    if (!node || !node->init || !node->cond || !node->inc) return;
    Node *loop_body = node->body ? node->body : node->then_body;
    if (!loop_body) return;
    if (loop_body->kind == ND_BLOCK) {
        if (loop_body->num_stmts != 1 || !loop_body->stmts[0]) return;
        loop_body = loop_body->stmts[0];
    }
    if ((loop_body->kind != ND_ASSIGN && loop_body->kind != ND_COMPOUND_ASSIGN) ||
        !loop_body->lhs || !loop_body->rhs) return;

    /* Match init: i = 0 */
    if (node->init->kind != ND_ASSIGN || !node->init->lhs || node->init->lhs->kind != ND_VAR) return;
    if (!node->init->rhs || node->init->rhs->kind != ND_NUM || node->init->rhs->int_val != 0) return;
    Node *ivar = node->init->lhs;
    const char *ivar_name = ivar->sym ? ivar->sym->name : ivar->name;
    if (!ivar_name || !ivar_name[0]) return;

    /* Match cond: i < N */
    if (node->cond->kind != ND_LT || !node->cond->lhs || node->cond->lhs->kind != ND_VAR || !node->cond->rhs) return;
    const char *cvar_name = node->cond->lhs->sym ? node->cond->lhs->sym->name : node->cond->lhs->name;
    if (!cvar_name || strcmp(cvar_name, ivar_name) != 0) return;
    Node *bound_expr = node->cond->rhs;
    if (wasm_node_has_side_effects(bound_expr)) return;

    /* Match inc: i++ or ++i */
    Node *inc_var = NULL;
    if ((node->inc->kind == ND_POST_INC || node->inc->kind == ND_PRE_INC) && node->inc->lhs && node->inc->lhs->kind == ND_VAR) {
        inc_var = node->inc->lhs;
    } else if (node->inc->kind == ND_COMPOUND_ASSIGN && node->inc->compound_op == ND_ADD &&
               node->inc->lhs && node->inc->lhs->kind == ND_VAR &&
               node->inc->rhs && node->inc->rhs->kind == ND_NUM && node->inc->rhs->int_val == 1) {
        inc_var = node->inc->lhs;
    }
    if (!inc_var) return;
    const char *inc_name = inc_var->sym ? inc_var->sym->name : inc_var->name;
    if (!inc_name || strcmp(inc_name, ivar_name) != 0) return;

    /* Match body: dst[i] = a[i] OP b[i]  OR  dst[i] OP= b[i]  OR  acc += a[i] / acc += a[i] * b[i] */
    int op_kind = 0;
    int is_reduction = 0;
    int step = 4;
    int addr_shift = 2;
    int hadd_mode = 0; /* 1=shadd, 2=uhadd, 3=srhadd, 4=urhadd */
    int esz_d = 0, f32_d = 0, esz_a = 0, f32_a = 0, esz_b = 0, f32_b = 0;
    Node *dst_base = (loop_body->lhs && loop_body->lhs->kind == ND_DEREF)
                         ? wasm_match_indexed_ptr(loop_body->lhs, ivar_name, &esz_d, &f32_d)
                         : NULL;
    Node *acc_var = NULL;
    Node *a_base = NULL;
    Node *b_base = NULL;
    Node *b_scalar = NULL;
    if (!dst_base && loop_body->kind == ND_COMPOUND_ASSIGN && loop_body->compound_op == ND_ADD &&
        loop_body->lhs && loop_body->lhs->kind == ND_VAR) {
        const char *acc_name = loop_body->lhs->sym ? loop_body->lhs->sym->name : loop_body->lhs->name;
        if (acc_name && acc_name[0] && strcmp(acc_name, ivar_name) != 0 &&
            (!loop_body->lhs->type || (loop_body->lhs->type->size == 4 && !is_float_type(loop_body->lhs->type)))) {
            Node *urhs = wasm_unwrap_cast(loop_body->rhs);
            a_base = wasm_match_indexed_ptr(urhs, ivar_name, &esz_a, &f32_a);
            if (a_base && !f32_a && esz_a == 4) {
                const char *an = a_base->sym ? a_base->sym->name : a_base->name;
                if (!an || (strcmp(an, acc_name) != 0 && strcmp(an, ivar_name) != 0)) {
                    acc_var = loop_body->lhs;
                    is_reduction = 1;
                }
            } else if (urhs && urhs->kind == ND_MUL) {
                Node *ul = wasm_unwrap_cast(urhs->lhs);
                Node *ur = wasm_unwrap_cast(urhs->rhs);
                a_base = wasm_match_indexed_ptr(ul, ivar_name, &esz_a, &f32_a);
                b_base = wasm_match_indexed_ptr(ur, ivar_name, &esz_b, &f32_b);
                if (a_base && b_base && !f32_a && !f32_b && esz_a == esz_b && (esz_a == 4 || esz_a == 2)) {
                    const char *an = a_base->sym ? a_base->sym->name : a_base->name;
                    const char *bn = b_base->sym ? b_base->sym->name : b_base->name;
                    if ((!an || (strcmp(an, acc_name) != 0 && strcmp(an, ivar_name) != 0)) &&
                        (!bn || (strcmp(bn, acc_name) != 0 && strcmp(bn, ivar_name) != 0))) {
                        if (esz_a == 4) {
                            acc_var = loop_body->lhs;
                            is_reduction = 2;
                        } else if (ul && ur && ul->type && ur->type &&
                                   ul->type->kind == TY_SHORT && ur->type->kind == TY_SHORT) {
                            acc_var = loop_body->lhs;
                            is_reduction = 3; /* i32x4.dot_i16x8_s */
                            step = 8;
                            addr_shift = 1;
                        }
                    }
                }
            }
        }
    }
    uint32_t simd_op = 0;
    int is_shift_op = 0;
    int is_unary_simd = 0;
    if (!is_reduction && loop_body->kind == ND_ASSIGN && loop_body->rhs &&
        loop_body->rhs->kind == ND_TERNARY && loop_body->rhs->cond &&
        loop_body->rhs->then_body && loop_body->rhs->else_body && dst_base && esz_d == 4) {
        Node *cond = loop_body->rhs->cond;
        Node *tb = loop_body->rhs->then_body;
        Node *eb = loop_body->rhs->else_body;
        int ck = cond->kind;
        if ((ck == ND_LT || ck == ND_LE || ck == ND_GT || ck == ND_GE) && cond->lhs && cond->rhs) {
            int esz_c0 = 0, f32_c0 = 0, esz_c1 = 0, f32_c1 = 0;
            Node *c0 = wasm_match_indexed_ptr(cond->lhs, ivar_name, &esz_c0, &f32_c0);
            Node *c1 = wasm_match_indexed_ptr(cond->rhs, ivar_name, &esz_c1, &f32_c1);
            Node *cr = wasm_unwrap_cast(cond->rhs);
            int is_zero_rhs = (cr && ((cr->kind == ND_NUM && cr->int_val == 0) ||
                                      (cr->kind == ND_FLIT && cr->f_val == 0.0)));
            if (c0 && esz_c0 == 4 && f32_c0 == f32_d && (ck == ND_LT || ck == ND_LE) &&
                is_zero_rhs && tb->kind == ND_NEG && tb->lhs) {
                int esz_t = 0, f32_t = 0, esz_e = 0, f32_e = 0;
                Node *t0 = wasm_match_indexed_ptr(tb->lhs, ivar_name, &esz_t, &f32_t);
                Node *e0 = wasm_match_indexed_ptr(eb, ivar_name, &esz_e, &f32_e);
                const char *cn = c0->sym ? c0->sym->name : c0->name;
                const char *tn = t0 ? (t0->sym ? t0->sym->name : t0->name) : NULL;
                const char *en = e0 ? (e0->sym ? e0->sym->name : e0->name) : NULL;
                if (t0 && e0 && esz_t == 4 && esz_e == 4 && f32_t == f32_d && f32_e == f32_d &&
                    cn && tn && en && strcmp(cn, tn) == 0 && strcmp(cn, en) == 0) {
                    a_base = c0; esz_a = esz_c0; f32_a = f32_d;
                    simd_op = WASM_SIMD_I32X4_ABS;
                    is_unary_simd = 1;
                    if (f32_d) simd_op = WASM_SIMD_F32X4_ABS;
                }
            } else if (c0 && c1 && esz_c0 == 4 && esz_c1 == 4 && f32_c0 == f32_d && f32_c1 == f32_d) {
                int esz_t = 0, f32_t = 0, esz_e = 0, f32_e = 0;
                Node *t0 = wasm_match_indexed_ptr(tb, ivar_name, &esz_t, &f32_t);
                Node *e0 = wasm_match_indexed_ptr(eb, ivar_name, &esz_e, &f32_e);
                const char *c0n = c0->sym ? c0->sym->name : c0->name;
                const char *c1n = c1->sym ? c1->sym->name : c1->name;
                const char *tn = t0 ? (t0->sym ? t0->sym->name : t0->name) : NULL;
                const char *en = e0 ? (e0->sym ? e0->sym->name : e0->name) : NULL;
                if (t0 && e0 && esz_t == 4 && esz_e == 4 && f32_t == f32_d && f32_e == f32_d &&
                    c0n && c1n && tn && en &&
                    strcmp(c0n, tn) == 0 && strcmp(c1n, en) == 0) {
                    int is_u = (cond->lhs->type && is_unsigned_type(cond->lhs->type));
                    a_base = c0; b_base = c1; esz_a = esz_c0; esz_b = esz_c1; f32_a = f32_d; f32_b = f32_d;
                    if (ck == ND_LT || ck == ND_LE) simd_op = is_u ? WASM_SIMD_I32X4_MIN_U : WASM_SIMD_I32X4_MIN_S;
                    else simd_op = is_u ? WASM_SIMD_I32X4_MAX_U : WASM_SIMD_I32X4_MAX_S;
                    if (f32_d) simd_op = (ck == ND_LT || ck == ND_LE) ? WASM_SIMD_F32X4_MIN : WASM_SIMD_F32X4_MAX;
                }
            }
        }
    }
    if (!is_reduction && !simd_op && loop_body->kind == ND_ASSIGN && dst_base && loop_body->rhs) {
        Node *rhs = wasm_unwrap_cast(loop_body->rhs);
        if (esz_d == 4 && rhs->kind == ND_NEG && rhs->lhs) {
            a_base = wasm_match_indexed_ptr(rhs->lhs, ivar_name, &esz_a, &f32_a);
            if (a_base && esz_a == 4 && f32_a == f32_d) {
                simd_op = f32_d ? WASM_SIMD_F32X4_NEG : WASM_SIMD_I32X4_NEG;
                is_unary_simd = 1;
            }
        } else if (esz_d == 4 && rhs->kind == ND_BNOT && rhs->lhs) {
            a_base = wasm_match_indexed_ptr(rhs->lhs, ivar_name, &esz_a, &f32_a);
            if (a_base && esz_a == 4 && !f32_a && !f32_d) {
                simd_op = WASM_SIMD_V128_NOT;
                is_unary_simd = 1;
            }
        } else if (esz_d == 4 && rhs->kind == ND_CALL && rhs->num_args == 1 && rhs->args && rhs->args[0]) {
            const char *fnm = rhs->func_name;
            if (strcmp(fnm, "fabsf") == 0 || strcmp(fnm, "__builtin_fabsf") == 0 ||
                strcmp(fnm, "sqrtf") == 0 || strcmp(fnm, "__builtin_sqrtf") == 0) {
                a_base = wasm_match_indexed_ptr(rhs->args[0], ivar_name, &esz_a, &f32_a);
                if (a_base && esz_a == 4 && f32_a && f32_d) {
                    int is_sqrt = (strcmp(fnm, "sqrtf") == 0 || strcmp(fnm, "__builtin_sqrtf") == 0);
                    simd_op = is_sqrt ? WASM_SIMD_F32X4_SQRT : WASM_SIMD_F32X4_ABS;
                    is_unary_simd = 1;
                }
            }
        } else if (rhs->kind == ND_CALL && rhs->num_args == 2 && rhs->args && rhs->args[0] && rhs->args[1]) {
            const char *fnm = rhs->func_name;
            a_base = wasm_match_indexed_ptr(rhs->args[0], ivar_name, &esz_a, &f32_a);
            b_base = wasm_match_indexed_ptr(rhs->args[1], ivar_name, &esz_b, &f32_b);
            if (a_base && b_base && !f32_d && !f32_a && !f32_b && esz_a == esz_d && esz_b == esz_d) {
                if (esz_d == 2 && loop_body->lhs->type && loop_body->lhs->type->kind == TY_USHORT &&
                    (strcmp(fnm, "__builtin_urhadd") == 0 || strcmp(fnm, "__builtin_uavg_ceil") == 0)) {
                    simd_op = WASM_SIMD_I16X8_AVGR_U;
                    step = 8;
                    addr_shift = 1;
                } else if (esz_d == 1 && loop_body->lhs->type && loop_body->lhs->type->kind == TY_UCHAR &&
                           (strcmp(fnm, "__builtin_urhadd") == 0 || strcmp(fnm, "__builtin_uavg_ceil") == 0)) {
                    simd_op = WASM_SIMD_I8X16_AVGR_U;
                    step = 16;
                    addr_shift = 0;
                } else if (esz_d == 4) {
                    if (strcmp(fnm, "__builtin_shadd") == 0 || strcmp(fnm, "__builtin_savg_floor") == 0) {
                        hadd_mode = 1; simd_op = WASM_SIMD_I32X4_ADD;
                    } else if (strcmp(fnm, "__builtin_uhadd") == 0 || strcmp(fnm, "__builtin_uavg_floor") == 0) {
                        hadd_mode = 2; simd_op = WASM_SIMD_I32X4_ADD;
                    } else if (strcmp(fnm, "__builtin_srhadd") == 0 || strcmp(fnm, "__builtin_savg_ceil") == 0) {
                        hadd_mode = 3; simd_op = WASM_SIMD_I32X4_SUB;
                    } else if (strcmp(fnm, "__builtin_urhadd") == 0 || strcmp(fnm, "__builtin_uavg_ceil") == 0) {
                        hadd_mode = 4; simd_op = WASM_SIMD_I32X4_SUB;
                    }
                }
            }
        } else if (rhs->kind == ND_SHR && (esz_d == 2 || esz_d == 1) &&
                   loop_body->lhs->type && is_unsigned_type(loop_body->lhs->type)) {
            Node *shr_cnt = wasm_unwrap_cast(rhs->rhs);
            Node *add1 = wasm_unwrap_cast(rhs->lhs);
            if (shr_cnt && shr_cnt->kind == ND_NUM && shr_cnt->int_val == 1 &&
                add1 && add1->kind == ND_ADD) {
                Node *c1 = wasm_unwrap_cast(add1->rhs);
                Node *ab = wasm_unwrap_cast(add1->lhs);
                if (c1 && c1->kind == ND_NUM && c1->int_val == 1 && ab && ab->kind == ND_ADD) {
                    a_base = wasm_match_indexed_ptr(ab->lhs, ivar_name, &esz_a, &f32_a);
                    b_base = wasm_match_indexed_ptr(ab->rhs, ivar_name, &esz_b, &f32_b);
                    if (a_base && b_base && !f32_d && !f32_a && !f32_b && esz_a == esz_d && esz_b == esz_d) {
                        simd_op = (esz_d == 2) ? WASM_SIMD_I16X8_AVGR_U : WASM_SIMD_I8X16_AVGR_U;
                        step = (esz_d == 2) ? 8 : 16;
                        addr_shift = (esz_d == 2) ? 1 : 0;
                    }
                }
            }
        }
    }
    if (!is_reduction && !simd_op && esz_d == 4) {
        Node *rhs_op = NULL;
        if (loop_body->kind == ND_COMPOUND_ASSIGN) {
            op_kind = loop_body->compound_op;
            a_base = dst_base;
            esz_a = esz_d;
            f32_a = f32_d;
            rhs_op = loop_body->rhs;
            b_base = wasm_match_indexed_ptr(rhs_op, ivar_name, &esz_b, &f32_b);
        } else {
            Node *rhs = loop_body->rhs;
            if (!rhs) return;
            op_kind = rhs->kind;
            a_base = wasm_match_indexed_ptr(rhs->lhs, ivar_name, &esz_a, &f32_a);
            rhs_op = rhs->rhs;
            b_base = wasm_match_indexed_ptr(rhs_op, ivar_name, &esz_b, &f32_b);
        }
        if (!b_base && rhs_op) {
            Node *u = wasm_unwrap_cast(rhs_op);
            if (u && (u->kind == ND_NUM || u->kind == ND_FLIT)) {
                b_scalar = rhs_op;
                f32_b = f32_d;
            } else if (u && u->kind == ND_VAR) {
                const char *sn = u->sym ? u->sym->name : u->name;
                const char *dn = dst_base ? (dst_base->sym ? dst_base->sym->name : dst_base->name) : NULL;
                if (sn && sn[0] && strcmp(sn, ivar_name) != 0 && (!dn || strcmp(sn, dn) != 0)) {
                    b_scalar = rhs_op;
                    f32_b = f32_d;
                }
            }
        }
        if (!dst_base || !a_base || (!b_base && !b_scalar)) return;
        if (esz_a != 4 || (b_base && esz_b != 4)) return;
        if (f32_d != f32_a || f32_d != f32_b) return;
    }

    if (is_reduction) {
        simd_op = (is_reduction == 3) ? WASM_SIMD_I32X4_DOT_I16X8_S : ((is_reduction == 2) ? WASM_SIMD_I32X4_MUL : WASM_SIMD_I32X4_ADD);
    } else if (!simd_op && f32_d) {
        if (op_kind == ND_ADD) simd_op = WASM_SIMD_F32X4_ADD;
        else if (op_kind == ND_SUB) simd_op = WASM_SIMD_F32X4_SUB;
        else if (op_kind == ND_MUL) simd_op = WASM_SIMD_F32X4_MUL;
        else if (op_kind == ND_DIV) simd_op = WASM_SIMD_F32X4_DIV;
    } else if (!simd_op) {
        if (op_kind == ND_ADD) simd_op = WASM_SIMD_I32X4_ADD;
        else if (op_kind == ND_SUB) simd_op = WASM_SIMD_I32X4_SUB;
        else if (op_kind == ND_MUL) simd_op = WASM_SIMD_I32X4_MUL;
        else if (op_kind == ND_BAND) simd_op = WASM_SIMD_V128_AND;
        else if (op_kind == ND_BOR) simd_op = WASM_SIMD_V128_OR;
        else if (op_kind == ND_BXOR) simd_op = WASM_SIMD_V128_XOR;
        else if (op_kind == ND_SHL && b_scalar && !b_base) {
            simd_op = WASM_SIMD_I32X4_SHL;
            is_shift_op = 1;
        } else if (op_kind == ND_SHR && b_scalar && !b_base) {
            Type *aty = (loop_body->kind == ND_COMPOUND_ASSIGN)
                            ? (loop_body->lhs ? loop_body->lhs->type : NULL)
                            : (loop_body->rhs && loop_body->rhs->lhs ? loop_body->rhs->lhs->type : NULL);
            simd_op = (aty && is_unsigned_type(aty)) ? WASM_SIMD_I32X4_SHR_U : WASM_SIMD_I32X4_SHR_S;
            is_shift_op = 1;
        }
    }
    if (!simd_op) return;

    /* Emit SIMD128 pre-loop: while (i + step <= N) { ...; i += step; } */
    wasm_buf_append_u8(&fn->body, WASM_OP_BLOCK);
    wasm_buf_append_u8(&fn->body, WASM_TYPE_VOID);
    wasm_stmt_block_depth++;
    wasm_buf_append_u8(&fn->body, WASM_OP_LOOP);
    wasm_buf_append_u8(&fn->body, WASM_TYPE_VOID);
    wasm_stmt_block_depth++;

    /* Condition: (i + step) > N => br_if 1 */
    wasm_lower_expr(cc, mod, fn, lmap, gmap, ivar);
    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(ivar), ivar->type, WASM_TYPE_I32, cc->ty_int);
    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
    wasm_buf_append_sleb128(&fn->body, step);
    wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD);
    wasm_lower_expr(cc, mod, fn, lmap, gmap, bound_expr);
    wasm_emit_type_conversion(&fn->body, wasm_node_valtype(bound_expr), bound_expr->type, WASM_TYPE_I32, cc->ty_int);
    wasm_buf_append_u8(&fn->body, WASM_OP_I32_GT_S);
    wasm_buf_append_u8(&fn->body, WASM_OP_BR_IF);
    wasm_buf_append_uleb128(&fn->body, 1);

    if (is_reduction && acc_var) {
        const char *acc_name = acc_var->sym ? acc_var->sym->name : acc_var->name;
        WasmLocalEntry *aentry = wasm_local_find_entry(lmap, acc_name);
        int lane;
        if (lmap->tmp_v128_idx < 0) {
            lmap->tmp_v128_idx = (int)(fn->num_params + fn->num_locals++);
            fn->local_types[fn->num_locals - 1] = WASM_TYPE_V128;
        }
        /* Load vector (and multiply/dot-product) ONCE into $tmp_v128 */
        wasm_emit_indexed_addr_scaled(cc, mod, fn, lmap, gmap, a_base, ivar, addr_shift);
        wasm_buf_append_u8(&fn->body, WASM_OP_SIMD_PREFIX);
        wasm_buf_append_uleb128(&fn->body, WASM_SIMD_V128_LOAD);
        wasm_emit_memarg_safe(&fn->body, (uint32_t)addr_shift, 0);
        if ((is_reduction == 2 || is_reduction == 3) && b_base) {
            wasm_emit_indexed_addr_scaled(cc, mod, fn, lmap, gmap, b_base, ivar, addr_shift);
            wasm_buf_append_u8(&fn->body, WASM_OP_SIMD_PREFIX);
            wasm_buf_append_uleb128(&fn->body, WASM_SIMD_V128_LOAD);
            wasm_emit_memarg_safe(&fn->body, (uint32_t)addr_shift, 0);
            wasm_buf_append_u8(&fn->body, WASM_OP_SIMD_PREFIX);
            wasm_buf_append_uleb128(&fn->body, (is_reduction == 3) ? WASM_SIMD_I32X4_DOT_I16X8_S : WASM_SIMD_I32X4_MUL);
        }
        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
        wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_v128_idx);
        if (!aentry || aentry->is_memory) {
            wasm_lower_addr(cc, mod, fn, lmap, gmap, acc_var);
        }
        wasm_lower_expr(cc, mod, fn, lmap, gmap, acc_var);
        wasm_emit_type_conversion(&fn->body, wasm_node_valtype(acc_var), acc_var->type, WASM_TYPE_I32, cc->ty_int);
        for (lane = 0; lane < 4; lane++) {
            wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
            wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_v128_idx);
            wasm_buf_append_u8(&fn->body, WASM_OP_SIMD_PREFIX);
            wasm_buf_append_uleb128(&fn->body, WASM_SIMD_I32X4_EXTRACT_LANE);
            wasm_buf_append_u8(&fn->body, (uint8_t)lane);
            wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD);
        }
        if (aentry && !aentry->is_memory) {
            wasm_emit_type_conversion(&fn->body, WASM_TYPE_I32, cc->ty_int, aentry->val_type, acc_var->type);
            wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
            wasm_buf_append_uleb128(&fn->body, aentry->local_idx);
        } else {
            wasm_emit_typed_store(&fn->body, acc_var->type ? acc_var->type : cc->ty_int, 0);
        }
    } else if (hadd_mode > 0) {
        /* 4-lane i32x4 carry-free halving-add: (va & vb) + ((va ^ vb) >> 1) or (va | vb) - ((va ^ vb) >> 1) */
        int is_u = (hadd_mode == 2 || hadd_mode == 4);
        int is_rnd = (hadd_mode == 3 || hadd_mode == 4);
        wasm_emit_indexed_addr_scaled(cc, mod, fn, lmap, gmap, dst_base, ivar, 2);
        wasm_emit_indexed_addr_scaled(cc, mod, fn, lmap, gmap, a_base, ivar, 2);
        wasm_buf_append_u8(&fn->body, WASM_OP_SIMD_PREFIX);
        wasm_buf_append_uleb128(&fn->body, WASM_SIMD_V128_LOAD);
        wasm_emit_memarg_safe(&fn->body, 2, 0);
        wasm_emit_indexed_addr_scaled(cc, mod, fn, lmap, gmap, b_base, ivar, 2);
        wasm_buf_append_u8(&fn->body, WASM_OP_SIMD_PREFIX);
        wasm_buf_append_uleb128(&fn->body, WASM_SIMD_V128_LOAD);
        wasm_emit_memarg_safe(&fn->body, 2, 0);
        wasm_buf_append_u8(&fn->body, WASM_OP_SIMD_PREFIX);
        wasm_buf_append_uleb128(&fn->body, is_rnd ? WASM_SIMD_V128_OR : WASM_SIMD_V128_AND);
        wasm_emit_indexed_addr_scaled(cc, mod, fn, lmap, gmap, a_base, ivar, 2);
        wasm_buf_append_u8(&fn->body, WASM_OP_SIMD_PREFIX);
        wasm_buf_append_uleb128(&fn->body, WASM_SIMD_V128_LOAD);
        wasm_emit_memarg_safe(&fn->body, 2, 0);
        wasm_emit_indexed_addr_scaled(cc, mod, fn, lmap, gmap, b_base, ivar, 2);
        wasm_buf_append_u8(&fn->body, WASM_OP_SIMD_PREFIX);
        wasm_buf_append_uleb128(&fn->body, WASM_SIMD_V128_LOAD);
        wasm_emit_memarg_safe(&fn->body, 2, 0);
        wasm_buf_append_u8(&fn->body, WASM_OP_SIMD_PREFIX);
        wasm_buf_append_uleb128(&fn->body, WASM_SIMD_V128_XOR);
        wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&fn->body, 1);
        wasm_buf_append_u8(&fn->body, WASM_OP_SIMD_PREFIX);
        wasm_buf_append_uleb128(&fn->body, is_u ? WASM_SIMD_I32X4_SHR_U : WASM_SIMD_I32X4_SHR_S);
        wasm_buf_append_u8(&fn->body, WASM_OP_SIMD_PREFIX);
        wasm_buf_append_uleb128(&fn->body, is_rnd ? WASM_SIMD_I32X4_SUB : WASM_SIMD_I32X4_ADD);
        wasm_buf_append_u8(&fn->body, WASM_OP_SIMD_PREFIX);
        wasm_buf_append_uleb128(&fn->body, WASM_SIMD_V128_STORE);
        wasm_emit_memarg_safe(&fn->body, 2, 0);
    } else {
        /* Stack: dst_addr, v128.load(a_addr), v128.load(b_addr) OR splat(b_scalar), simd_op, v128.store */
        wasm_emit_indexed_addr_scaled(cc, mod, fn, lmap, gmap, dst_base, ivar, addr_shift);
        wasm_emit_indexed_addr_scaled(cc, mod, fn, lmap, gmap, a_base, ivar, addr_shift);
        wasm_buf_append_u8(&fn->body, WASM_OP_SIMD_PREFIX);
        wasm_buf_append_uleb128(&fn->body, WASM_SIMD_V128_LOAD);
        wasm_emit_memarg_safe(&fn->body, (uint32_t)addr_shift, 0);
        if (b_base) {
            wasm_emit_indexed_addr_scaled(cc, mod, fn, lmap, gmap, b_base, ivar, addr_shift);
            wasm_buf_append_u8(&fn->body, WASM_OP_SIMD_PREFIX);
            wasm_buf_append_uleb128(&fn->body, WASM_SIMD_V128_LOAD);
            wasm_emit_memarg_safe(&fn->body, (uint32_t)addr_shift, 0);
        } else if (!is_unary_simd) {
            wasm_lower_expr(cc, mod, fn, lmap, gmap, b_scalar);
            wasm_emit_type_conversion(&fn->body, wasm_node_valtype(b_scalar), b_scalar->type,
                                      f32_d ? WASM_TYPE_F32 : WASM_TYPE_I32,
                                      f32_d ? cc->ty_float : cc->ty_int);
            if (!is_shift_op) {
                wasm_buf_append_u8(&fn->body, WASM_OP_SIMD_PREFIX);
                wasm_buf_append_uleb128(&fn->body, f32_d ? WASM_SIMD_F32X4_SPLAT : WASM_SIMD_I32X4_SPLAT);
            }
        }
        wasm_buf_append_u8(&fn->body, WASM_OP_SIMD_PREFIX);
        wasm_buf_append_uleb128(&fn->body, simd_op);
        wasm_buf_append_u8(&fn->body, WASM_OP_SIMD_PREFIX);
        wasm_buf_append_uleb128(&fn->body, WASM_SIMD_V128_STORE);
        wasm_emit_memarg_safe(&fn->body, (uint32_t)addr_shift, 0);
    }

    /* i = i + step */
    {
        WasmLocalEntry *ientry = wasm_local_find_entry(lmap, ivar_name);
        if (ientry && !ientry->is_memory) {
            wasm_lower_expr(cc, mod, fn, lmap, gmap, ivar);
            wasm_emit_type_conversion(&fn->body, wasm_node_valtype(ivar), ivar->type, WASM_TYPE_I32, cc->ty_int);
            wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
            wasm_buf_append_sleb128(&fn->body, step);
            wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD);
            wasm_emit_type_conversion(&fn->body, WASM_TYPE_I32, cc->ty_int, ientry->val_type, ivar->type);
            wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
            wasm_buf_append_uleb128(&fn->body, ientry->local_idx);
        } else {
            wasm_lower_addr(cc, mod, fn, lmap, gmap, ivar);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, ivar);
            wasm_emit_type_conversion(&fn->body, wasm_node_valtype(ivar), ivar->type, WASM_TYPE_I32, cc->ty_int);
            wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
            wasm_buf_append_sleb128(&fn->body, step);
            wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD);
            wasm_emit_typed_store(&fn->body, ivar->type ? ivar->type : cc->ty_int, 0);
        }
    }

    wasm_buf_append_u8(&fn->body, WASM_OP_BR);
    wasm_buf_append_uleb128(&fn->body, 0);
    wasm_stmt_block_depth -= 2;
    wasm_buf_append_u8(&fn->body, WASM_OP_END);
    wasm_buf_append_u8(&fn->body, WASM_OP_END);
}

/* Statement Lowering */
static void wasm_lower_stmt(Compiler *cc, const WasmModule *mod, WasmFunction *fn, WasmLocalMap *lmap, WasmGlobalMap *gmap, Node *node, Type *ret_type) {
    if (!node || !fn) return;

    switch (node->kind) {
        case ND_VAR:
        case ND_NOP:
            /* Declarations without initializers produce no runtime bytecode */
            break;

        case ND_LABEL:
            if (node->lhs) {
                wasm_lower_stmt(cc, mod, fn, lmap, gmap, node->lhs, ret_type);
            }
            break;

        case ND_BREAK:
            if (wasm_cur_break_depth >= 0 && wasm_stmt_block_depth > wasm_cur_break_depth) {
                wasm_buf_append_u8(&fn->body, WASM_OP_BR);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)(wasm_stmt_block_depth - 1 - wasm_cur_break_depth));
            }
            break;

        case ND_CONTINUE:
            if (wasm_cur_cont_depth >= 0 && wasm_stmt_block_depth > wasm_cur_cont_depth) {
                wasm_buf_append_u8(&fn->body, WASM_OP_BR);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)(wasm_stmt_block_depth - 1 - wasm_cur_cont_depth));
            }
            break;

        case ND_ASSIGN:
        case ND_COMPOUND_ASSIGN:
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node);
            wasm_buf_append_u8(&fn->body, WASM_OP_DROP);
            break;

        case ND_CALL: {
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node);
            int callee_idx = node->func_name[0] ? wasm_find_func_idx(mod, node->func_name) : -1;
            if (callee_idx >= 0) {
                int tidx = -1;
                if (callee_idx < mod->num_imports) {
                    tidx = mod->imports[callee_idx].type_idx;
                } else if (callee_idx - mod->num_imports < mod->num_funcs) {
                    tidx = mod->funcs[callee_idx - mod->num_imports].type_idx;
                }
                if (tidx >= 0 && mod->types[tidx].num_results > 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_DROP);
                }
            } else {
                /* Indirect calls return 1 result */
                wasm_buf_append_u8(&fn->body, WASM_OP_DROP);
            }
            break;
        }

        case ND_RETURN:
            if (node->lhs) {
                uint8_t expr_vt = wasm_node_valtype(node->lhs);
                uint8_t func_ret_vt = wasm_valtype_from_ctype(ret_type);
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
                wasm_emit_type_conversion(&fn->body, expr_vt, node->lhs->type, func_ret_vt, ret_type);

                /* Epilogue: restore stack pointer if frame allocated */
                if (lmap->frame_size > 0 && lmap->fp_local_idx >= 0) {
                    if (func_ret_vt == WASM_TYPE_F32 && lmap->tmp_f32_idx >= 0) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                        wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_f32_idx);
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                        wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->fp_local_idx);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                        wasm_buf_append_sleb128(&fn->body, lmap->frame_size);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD);
                        wasm_buf_append_u8(&fn->body, WASM_OP_GLOBAL_SET);
                        wasm_buf_append_uleb128(&fn->body, 0); /* __stack_pointer */
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                        wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_f32_idx);
                    } else if (func_ret_vt == WASM_TYPE_F64 && lmap->tmp_f64_idx >= 0) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                        wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_f64_idx);
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                        wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->fp_local_idx);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                        wasm_buf_append_sleb128(&fn->body, lmap->frame_size);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD);
                        wasm_buf_append_u8(&fn->body, WASM_OP_GLOBAL_SET);
                        wasm_buf_append_uleb128(&fn->body, 0);
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                        wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_f64_idx);
                    } else if (func_ret_vt == WASM_TYPE_I64 && lmap->tmp_i64_idx >= 0) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                        wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                        wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->fp_local_idx);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                        wasm_buf_append_sleb128(&fn->body, lmap->frame_size);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD);
                        wasm_buf_append_u8(&fn->body, WASM_OP_GLOBAL_SET);
                        wasm_buf_append_uleb128(&fn->body, 0);
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                        wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    } else if (lmap->tmp_val_idx >= 0) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                        wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                        wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->fp_local_idx);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                        wasm_buf_append_sleb128(&fn->body, lmap->frame_size);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD);
                        wasm_buf_append_u8(&fn->body, WASM_OP_GLOBAL_SET);
                        wasm_buf_append_uleb128(&fn->body, 0);
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                        wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                    }
                }
            } else {
                if (lmap->frame_size > 0 && lmap->fp_local_idx >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->fp_local_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, lmap->frame_size);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD);
                    wasm_buf_append_u8(&fn->body, WASM_OP_GLOBAL_SET);
                    wasm_buf_append_uleb128(&fn->body, 0);
                }
            }
            wasm_buf_append_u8(&fn->body, WASM_OP_RETURN);
            break;

        case ND_IF: {
            Node *then_node = node->then_body ? node->then_body : node->body;
            Node *else_node = node->else_body;
            uint8_t cond_vt = wasm_node_valtype(node->cond);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->cond);
            if (cond_vt != WASM_TYPE_I32) {
                wasm_emit_to_bool_i32(&fn->body, cond_vt);
            }
            wasm_buf_append_u8(&fn->body, WASM_OP_IF);
            wasm_buf_append_u8(&fn->body, WASM_TYPE_VOID);
            wasm_stmt_block_depth++;
            if (then_node) wasm_lower_stmt(cc, mod, fn, lmap, gmap, then_node, ret_type);
            if (else_node) {
                wasm_buf_append_u8(&fn->body, WASM_OP_ELSE);
                wasm_lower_stmt(cc, mod, fn, lmap, gmap, else_node, ret_type);
            }
            wasm_stmt_block_depth--;
            wasm_buf_append_u8(&fn->body, WASM_OP_END);
            break;
        }

        case ND_WHILE: {
            Node *loop_body = node->body ? node->body : node->then_body;
            int prev_break = wasm_cur_break_depth;
            int prev_cont = wasm_cur_cont_depth;
            wasm_buf_append_u8(&fn->body, WASM_OP_BLOCK);
            wasm_buf_append_u8(&fn->body, WASM_TYPE_VOID);
            wasm_cur_break_depth = wasm_stmt_block_depth++;
            wasm_buf_append_u8(&fn->body, WASM_OP_LOOP);
            wasm_buf_append_u8(&fn->body, WASM_TYPE_VOID);
            wasm_cur_cont_depth = wasm_stmt_block_depth++;

            if (node->cond) {
                uint8_t cond_vt = wasm_node_valtype(node->cond);
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->cond);
                if (cond_vt == WASM_TYPE_I64) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_EQZ);
                } else if (cond_vt == WASM_TYPE_F32) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_F32_CONST);
                    wasm_buf_append_f32(&fn->body, 0.0f);
                    wasm_buf_append_u8(&fn->body, WASM_OP_F32_EQ);
                } else if (cond_vt == WASM_TYPE_F64) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_F64_CONST);
                    wasm_buf_append_f64(&fn->body, 0.0);
                    wasm_buf_append_u8(&fn->body, WASM_OP_F64_EQ);
                } else {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_EQZ);
                }
                wasm_buf_append_u8(&fn->body, WASM_OP_BR_IF);
                wasm_buf_append_uleb128(&fn->body, 1); /* Break to outer block */
            }

            if (loop_body) {
                wasm_lower_stmt(cc, mod, fn, lmap, gmap, loop_body, ret_type);
            }

            wasm_buf_append_u8(&fn->body, WASM_OP_BR);
            wasm_buf_append_uleb128(&fn->body, 0); /* Loop back */
            wasm_stmt_block_depth -= 2;
            wasm_cur_break_depth = prev_break;
            wasm_cur_cont_depth = prev_cont;
            wasm_buf_append_u8(&fn->body, WASM_OP_END);
            wasm_buf_append_u8(&fn->body, WASM_OP_END);
            break;
        }

        case ND_DO_WHILE: {
            Node *loop_body = node->body ? node->body : node->then_body;
            int prev_break = wasm_cur_break_depth;
            int prev_cont = wasm_cur_cont_depth;
            wasm_buf_append_u8(&fn->body, WASM_OP_BLOCK);
            wasm_buf_append_u8(&fn->body, WASM_TYPE_VOID);
            wasm_cur_break_depth = wasm_stmt_block_depth++;
            wasm_buf_append_u8(&fn->body, WASM_OP_LOOP);
            wasm_buf_append_u8(&fn->body, WASM_TYPE_VOID);
            wasm_stmt_block_depth++;
            wasm_buf_append_u8(&fn->body, WASM_OP_BLOCK);
            wasm_buf_append_u8(&fn->body, WASM_TYPE_VOID);
            wasm_cur_cont_depth = wasm_stmt_block_depth++;

            if (loop_body) {
                wasm_lower_stmt(cc, mod, fn, lmap, gmap, loop_body, ret_type);
            }

            wasm_stmt_block_depth--;
            wasm_buf_append_u8(&fn->body, WASM_OP_END);

            if (node->cond) {
                uint8_t cond_vt = wasm_node_valtype(node->cond);
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->cond);
                if (cond_vt != WASM_TYPE_I32) {
                    wasm_emit_to_bool_i32(&fn->body, cond_vt);
                }
                wasm_buf_append_u8(&fn->body, WASM_OP_BR_IF);
                wasm_buf_append_uleb128(&fn->body, 0); /* Loop back if cond true */
            }

            wasm_stmt_block_depth -= 2;
            wasm_cur_break_depth = prev_break;
            wasm_cur_cont_depth = prev_cont;
            wasm_buf_append_u8(&fn->body, WASM_OP_END);
            wasm_buf_append_u8(&fn->body, WASM_OP_END);
            break;
        }

        case ND_FOR: {
            Node *loop_body = node->body ? node->body : node->then_body;
            int prev_break = wasm_cur_break_depth;
            int prev_cont = wasm_cur_cont_depth;
            if (node->init) {
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->init);
                wasm_buf_append_u8(&fn->body, WASM_OP_DROP);
            }
            wasm_try_autovec_for_loop(cc, mod, fn, lmap, gmap, node);
            wasm_buf_append_u8(&fn->body, WASM_OP_BLOCK);
            wasm_buf_append_u8(&fn->body, WASM_TYPE_VOID);
            wasm_cur_break_depth = wasm_stmt_block_depth++;
            wasm_buf_append_u8(&fn->body, WASM_OP_LOOP);
            wasm_buf_append_u8(&fn->body, WASM_TYPE_VOID);
            wasm_stmt_block_depth++;

            if (node->cond) {
                uint8_t cond_vt = wasm_node_valtype(node->cond);
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->cond);
                if (cond_vt == WASM_TYPE_I64) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_EQZ);
                } else if (cond_vt == WASM_TYPE_F32) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_F32_CONST);
                    wasm_buf_append_f32(&fn->body, 0.0f);
                    wasm_buf_append_u8(&fn->body, WASM_OP_F32_EQ);
                } else if (cond_vt == WASM_TYPE_F64) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_F64_CONST);
                    wasm_buf_append_f64(&fn->body, 0.0);
                    wasm_buf_append_u8(&fn->body, WASM_OP_F64_EQ);
                } else {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_EQZ);
                }
                wasm_buf_append_u8(&fn->body, WASM_OP_BR_IF);
                wasm_buf_append_uleb128(&fn->body, 1);
            }

            wasm_buf_append_u8(&fn->body, WASM_OP_BLOCK);
            wasm_buf_append_u8(&fn->body, WASM_TYPE_VOID);
            wasm_cur_cont_depth = wasm_stmt_block_depth++;

            if (loop_body) {
                wasm_lower_stmt(cc, mod, fn, lmap, gmap, loop_body, ret_type);
            }

            wasm_stmt_block_depth--;
            wasm_buf_append_u8(&fn->body, WASM_OP_END);

            if (node->inc) {
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->inc);
                wasm_buf_append_u8(&fn->body, WASM_OP_DROP);
            }

            wasm_buf_append_u8(&fn->body, WASM_OP_BR);
            wasm_buf_append_uleb128(&fn->body, 0);
            wasm_stmt_block_depth -= 2;
            wasm_cur_break_depth = prev_break;
            wasm_cur_cont_depth = prev_cont;
            wasm_buf_append_u8(&fn->body, WASM_OP_END);
            wasm_buf_append_u8(&fn->body, WASM_OP_END);
            break;
        }

        case ND_SWITCH: {
            Node *flat_stmts[512];
            WasmSwitchLabel labels[256];
            int num_flat = 0;
            int num_labels = 0;
            wasm_flatten_switch_body(node->body, flat_stmts, &num_flat, 512, labels, &num_labels, 256);

            int grp_starts[256];
            int num_grps = 0;
            int li;
            for (li = 0; li < num_labels; li++) {
                int fidx = labels[li].flat_idx;
                if (num_grps == 0 || grp_starts[num_grps - 1] != fidx) {
                    grp_starts[num_grps++] = fidx;
                }
            }

            int prev_break = wasm_cur_break_depth;
            wasm_buf_append_u8(&fn->body, WASM_OP_BLOCK);
            wasm_buf_append_u8(&fn->body, WASM_TYPE_VOID);
            wasm_cur_break_depth = wasm_stmt_block_depth++;

            int gi;
            for (gi = 0; gi < num_grps; gi++) {
                wasm_buf_append_u8(&fn->body, WASM_OP_BLOCK);
                wasm_buf_append_u8(&fn->body, WASM_TYPE_VOID);
                wasm_stmt_block_depth++;
            }

            /* Evaluate controlling expression and store in tmp_i64_idx */
            uint8_t cond_vt = wasm_node_valtype(node->cond);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->cond);
            wasm_emit_type_conversion(&fn->body, cond_vt, node->cond ? node->cond->type : NULL, WASM_TYPE_I64, node->cond ? node->cond->type : NULL);
            wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
            wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);

            int ci;
            for (ci = 0; ci < node->num_cases; ci++) {
                Node *cnode = node->cases[ci];
                if (!cnode || !cnode->case_body) continue;
                const char *cname = cnode->case_body->label_name;
                int target_grp = -1;
                for (li = 0; li < num_labels; li++) {
                    if (strcmp(labels[li].name, cname) == 0) {
                        int g;
                        for (g = 0; g < num_grps; g++) {
                            if (grp_starts[g] == labels[li].flat_idx) {
                                target_grp = g;
                                break;
                            }
                        }
                        break;
                    }
                }
                if (target_grp >= 0) {
                    int64_t cval = (int64_t)cnode->case_val;
                    if (cond_vt == WASM_TYPE_I32) {
                        if (node->cond && wasm_type_is_unsigned_or_ptr(node->cond->type)) {
                            cval = (int64_t)(uint64_t)(uint32_t)cval;
                        } else {
                            cval = (int64_t)(int32_t)cval;
                        }
                    }
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                    wasm_buf_append_sleb128_i64(&fn->body, cval);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_EQ);
                    wasm_buf_append_u8(&fn->body, WASM_OP_BR_IF);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)target_grp);
                }
            }

            /* Branch to default case or break out of switch */
            int def_grp = num_grps; /* default to break block if no default: label */
            if (node->default_case && node->default_case->case_body) {
                const char *dname = node->default_case->case_body->label_name;
                for (li = 0; li < num_labels; li++) {
                    if (strcmp(labels[li].name, dname) == 0) {
                        int g;
                        for (g = 0; g < num_grps; g++) {
                            if (grp_starts[g] == labels[li].flat_idx) {
                                def_grp = g;
                                break;
                            }
                        }
                        break;
                    }
                }
            }
            wasm_buf_append_u8(&fn->body, WASM_OP_BR);
            wasm_buf_append_uleb128(&fn->body, (uint32_t)def_grp);

            /* Emit each group of statements in order */
            for (gi = 0; gi < num_grps; gi++) {
                wasm_stmt_block_depth--;
                wasm_buf_append_u8(&fn->body, WASM_OP_END);
                int start_idx = grp_starts[gi];
                int end_idx = (gi + 1 < num_grps) ? grp_starts[gi + 1] : num_flat;
                int si;
                for (si = start_idx; si < end_idx; si++) {
                    wasm_lower_stmt(cc, mod, fn, lmap, gmap, flat_stmts[si], ret_type);
                }
            }

            wasm_stmt_block_depth--;
            wasm_cur_break_depth = prev_break;
            wasm_buf_append_u8(&fn->body, WASM_OP_END);
            break;
        }

        case ND_BLOCK: {
            int s;
            for (s = 0; s < node->num_stmts; s++) {
                wasm_lower_stmt(cc, mod, fn, lmap, gmap, node->stmts[s], ret_type);
            }
            break;
        }

        default:
            if (wasm_node_has_side_effects(node)) {
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node);
                wasm_buf_append_u8(&fn->body, WASM_OP_DROP);
            }
            break;
    }
}

/* Pre-scan AST to discover local variables, arrays, structs, and address-taken entities */
static void wasm_scan_ast_locals(const WasmModule *mod, Node *node, WasmLocalMap *lmap, WasmGlobalMap *gmap) {
    if (!node || !lmap) return;

    if (node->kind == ND_VAR) {
        const char *vname = node->sym ? node->sym->name : node->name;
        if (vname && vname[0] && !wasm_global_find(gmap, vname) && (wasm_find_func_idx(mod, vname) < 0) && !wasm_local_find_entry(lmap, vname)) {
            int e = lmap->count++;
            strncpy(lmap->entries[e].name, vname, sizeof(lmap->entries[e].name) - 1);
            lmap->entries[e].type = node->type;
            lmap->entries[e].val_type = wasm_valtype_from_ctype(node->type);
            if (node->type && (node->type->kind == TY_ARRAY || node->type->kind == TY_STRUCT || node->type->kind == TY_UNION)) {
                lmap->entries[e].is_memory = 1;
                lmap->entries[e].size = type_size(node->type);
                if (lmap->entries[e].size <= 0) lmap->entries[e].size = 4;
            }
        }
    } else if (node->kind == ND_ADDR) {
        if (node->lhs && node->lhs->kind == ND_VAR) {
            const char *vname = node->lhs->sym ? node->lhs->sym->name : node->lhs->name;
            if (vname && vname[0] && !wasm_global_find(gmap, vname) && (wasm_find_func_idx(mod, vname) < 0)) {
                WasmLocalEntry *entry = wasm_local_find_entry(lmap, vname);
                if (!entry) {
                    int e = lmap->count++;
                    entry = &lmap->entries[e];
                    strncpy(entry->name, vname, sizeof(entry->name) - 1);
                    entry->type = node->lhs->type;
                    entry->val_type = wasm_valtype_from_ctype(node->lhs->type);
                }
                if (entry) {
                    entry->is_memory = 1;
                    entry->size = entry->type ? type_size((Type *)entry->type) : 4;
                    if (entry->size <= 0) entry->size = 4;
                }
            }
        }
    }

    if (node->lhs) wasm_scan_ast_locals(mod, node->lhs, lmap, gmap);
    if (node->rhs) wasm_scan_ast_locals(mod, node->rhs, lmap, gmap);
    if (node->cond) wasm_scan_ast_locals(mod, node->cond, lmap, gmap);
    if (node->then_body) wasm_scan_ast_locals(mod, node->then_body, lmap, gmap);
    if (node->else_body) wasm_scan_ast_locals(mod, node->else_body, lmap, gmap);
    if (node->body) wasm_scan_ast_locals(mod, node->body, lmap, gmap);
    if (node->init) wasm_scan_ast_locals(mod, node->init, lmap, gmap);
    if (node->inc) wasm_scan_ast_locals(mod, node->inc, lmap, gmap);
    if (node->stmts) {
        int s;
        for (s = 0; s < node->num_stmts; s++) {
            wasm_scan_ast_locals(mod, node->stmts[s], lmap, gmap);
        }
    }
    if (node->args) {
        int a;
        for (a = 0; a < node->num_args; a++) {
            wasm_scan_ast_locals(mod, node->args[a], lmap, gmap);
        }
    }
}

static int wasm_ast_has_call_name(Node *node, const char *name) {
    if (!node || !name) return 0;
    if (node->kind == ND_CALL && node->func_name[0] && strcmp(node->func_name, name) == 0) return 1;
    if (node->lhs && wasm_ast_has_call_name(node->lhs, name)) return 1;
    if (node->rhs && wasm_ast_has_call_name(node->rhs, name)) return 1;
    if (node->cond && wasm_ast_has_call_name(node->cond, name)) return 1;
    if (node->then_body && wasm_ast_has_call_name(node->then_body, name)) return 1;
    if (node->else_body && wasm_ast_has_call_name(node->else_body, name)) return 1;
    if (node->body && wasm_ast_has_call_name(node->body, name)) return 1;
    if (node->init && wasm_ast_has_call_name(node->init, name)) return 1;
    if (node->inc && wasm_ast_has_call_name(node->inc, name)) return 1;
    if (node->stmts) {
        int s;
        for (s = 0; s < node->num_stmts; s++) {
            if (wasm_ast_has_call_name(node->stmts[s], name)) return 1;
        }
    }
    if (node->args) {
        int a;
        for (a = 0; a < node->num_args; a++) {
            if (wasm_ast_has_call_name(node->args[a], name)) return 1;
        }
    }
    return 0;
}

int wasm_lower_program(Compiler *cc, Node *prog, const char *output_file) {
    if (!prog || !output_file) return -1;

    WasmModule mod;
    wasm_module_init(&mod);

    WasmGlobalMap gmap;
    memset(&gmap, 0, sizeof(WasmGlobalMap));
    memset(wasm_string_offsets, 0, sizeof(wasm_string_offsets));

    /* 0. Place String Literals and Global Variables into Data Segments */
    uint32_t global_static_offset = 1024;

    /* Emit String Literals */
    int s;
    for (s = 0; s < cc->num_strings && s < MAX_STRINGS; s++) {
        if (cc->strings[s].data) {
            int slen = cc->strings[s].len;
            if (slen < 0) slen = (int)strlen(cc->strings[s].data);
            global_static_offset = (global_static_offset + 3) & -4;
            wasm_string_offsets[s] = global_static_offset;
            wasm_module_add_data_segment(&mod, global_static_offset, (const uint8_t *)cc->strings[s].data, (size_t)(slen + 1));
            global_static_offset += (uint32_t)(slen + 1);
        }
    }

    /* Scan cc->globals array */
    int g;
    for (g = 0; g < cc->num_globals; g++) {
        Node *gvar = cc->globals[g];
        if (!gvar || gvar->kind != ND_GLOBAL_VAR) continue;
        const char *gname = gvar->name[0] ? gvar->name : (gvar->sym ? gvar->sym->name : NULL);
        if (!gname || !gname[0] || wasm_global_find(&gmap, gname)) continue;

        int sz = gvar->type ? type_size(gvar->type) : 4;
        if (sz <= 0) sz = 4;
        uint32_t galign = (sz >= 8) ? 8 : 4;
        global_static_offset = (global_static_offset + (galign - 1)) & ~(galign - 1);

        int g_idx = gmap.count++;
        strncpy(gmap.vars[g_idx].name, gname, sizeof(gmap.vars[g_idx].name) - 1);
        gmap.vars[g_idx].offset = global_static_offset;
        gmap.vars[g_idx].size = sz;
        gmap.vars[g_idx].type = gvar->type;

        if (gvar->sym) gvar->sym->stack_offset = (int)global_static_offset;

        if (gvar->initializer) {
            if (gvar->initializer->kind == ND_FLIT || (gvar->type && (gvar->type->kind == TY_FLOAT || gvar->type->kind == TY_DOUBLE))) {
                if (gvar->type && gvar->type->kind == TY_FLOAT) {
                    float fval = (float)gvar->initializer->f_val;
                    wasm_module_add_data_segment(&mod, global_static_offset, (const uint8_t *)&fval, sizeof(float));
                } else {
                    double dval = (double)gvar->initializer->f_val;
                    wasm_module_add_data_segment(&mod, global_static_offset, (const uint8_t *)&dval, sizeof(double));
                }
            } else {
                int const_ok = 1;
                int64_t val = (int64_t)eval_const_expr_p4(gvar->initializer, &const_ok);
                if (!const_ok) val = (int64_t)gvar->initializer->int_val;
                wasm_module_add_data_segment(&mod, global_static_offset, (const uint8_t *)&val, sz <= 8 ? (size_t)sz : 8);
            }
        }
        global_static_offset += (uint32_t)sz;
    }

    /* Also scan linked prog nodes for any globals */
    Node *curr = prog;
    while (curr) {
        if (curr->kind == ND_GLOBAL_VAR || (curr->kind == ND_VAR && curr->sym && curr->sym->is_global)) {
            const char *gname = curr->name[0] ? curr->name : (curr->sym ? curr->sym->name : NULL);
            if (gname && gname[0] && !wasm_global_find(&gmap, gname)) {
                int sz = curr->type ? type_size(curr->type) : 4;
                if (sz <= 0) sz = 4;
                uint32_t galign = (sz >= 8) ? 8 : 4;
                global_static_offset = (global_static_offset + (galign - 1)) & ~(galign - 1);

                int g_idx = gmap.count++;
                strncpy(gmap.vars[g_idx].name, gname, sizeof(gmap.vars[g_idx].name) - 1);
                gmap.vars[g_idx].offset = global_static_offset;
                gmap.vars[g_idx].size = sz;
                gmap.vars[g_idx].type = curr->type;

                if (curr->sym) curr->sym->stack_offset = (int)global_static_offset;

                if (curr->initializer) {
                    if (curr->initializer->kind == ND_FLIT || (curr->type && (curr->type->kind == TY_FLOAT || curr->type->kind == TY_DOUBLE))) {
                        if (curr->type && curr->type->kind == TY_FLOAT) {
                            float fval = (float)curr->initializer->f_val;
                            wasm_module_add_data_segment(&mod, global_static_offset, (const uint8_t *)&fval, sizeof(float));
                        } else {
                            double dval = (double)curr->initializer->f_val;
                            wasm_module_add_data_segment(&mod, global_static_offset, (const uint8_t *)&dval, sizeof(double));
                        }
                    } else {
                        int const_ok = 1;
                        int64_t val = (int64_t)eval_const_expr_p4(curr->initializer, &const_ok);
                        if (!const_ok) val = (int64_t)curr->initializer->int_val;
                        wasm_module_add_data_segment(&mod, global_static_offset, (const uint8_t *)&val, sz <= 8 ? (size_t)sz : 8);
                    }
                }
                global_static_offset += (uint32_t)sz;
            }
        }
        curr = curr->next;
    }

    /* Initialize Global 1: __heap_base */
    uint32_t heap_base = (global_static_offset + 15) & -16;
    if (heap_base < 4096) heap_base = 4096;
    wasm_module_add_global(&mod, WASM_TYPE_I32, 1, (int32_t)heap_base);

    /* 1. Detect built-in and WASI dependencies */
    int has_user_puts = 0, has_user_putchar = 0, has_user_write = 0, has_user_read = 0, has_user_exit = 0, has_user_start = 0;
    int needs_puts = 0, needs_putchar = 0, needs_write = 0, needs_read = 0, needs_exit = 0;
    int has_malloc = 0, has_free = 0, needs_malloc = 0, needs_free = 0;
    int has_main = 0;

    curr = prog;
    while (curr) {
        if (curr->kind == ND_FUNC_DEF) {
            if (strcmp(curr->func_def_name, "puts") == 0) has_user_puts = 1;
            if (strcmp(curr->func_def_name, "putchar") == 0) has_user_putchar = 1;
            if (strcmp(curr->func_def_name, "write") == 0) has_user_write = 1;
            if (strcmp(curr->func_def_name, "read") == 0) has_user_read = 1;
            if (strcmp(curr->func_def_name, "exit") == 0) has_user_exit = 1;
            if (strcmp(curr->func_def_name, "_start") == 0) has_user_start = 1;
            if (strcmp(curr->func_def_name, "malloc") == 0) has_malloc = 1;
            if (strcmp(curr->func_def_name, "free") == 0) has_free = 1;
            if (strcmp(curr->func_def_name, "main") == 0) has_main = 1;
        }
        curr = curr->next;
    }

    curr = prog;
    while (curr) {
        if (curr->kind == ND_FUNC_DEF && curr->body) {
            if (!has_user_puts && wasm_ast_has_call_name(curr->body, "puts")) needs_puts = 1;
            if (!has_user_putchar && wasm_ast_has_call_name(curr->body, "putchar")) needs_putchar = 1;
            if (!has_user_write && wasm_ast_has_call_name(curr->body, "write")) needs_write = 1;
            if (!has_user_read && wasm_ast_has_call_name(curr->body, "read")) needs_read = 1;
            if (!has_user_exit && wasm_ast_has_call_name(curr->body, "exit")) needs_exit = 1;
            if (!has_malloc && wasm_ast_has_call_name(curr->body, "malloc")) needs_malloc = 1;
            if (!has_free && wasm_ast_has_call_name(curr->body, "free")) needs_free = 1;
        }
        curr = curr->next;
    }

    int has_wasi_calls = (needs_puts || needs_putchar || needs_write || needs_read || needs_exit);
    int synthesize_start = (!has_user_start && has_main && has_wasi_calls);

    /* Declare WASI function imports if I/O or termination are needed */
    int fd_write_imp_idx = -1;
    int fd_read_imp_idx = -1;
    int proc_exit_imp_idx = -1;

    if (needs_puts || needs_putchar || needs_write) {
        uint8_t wasi_io_params[4] = { WASM_TYPE_I32, WASM_TYPE_I32, WASM_TYPE_I32, WASM_TYPE_I32 };
        uint8_t wasi_io_results[1] = { WASM_TYPE_I32 };
        int wasi_io_tidx = wasm_module_add_type(&mod, wasi_io_params, 4, wasi_io_results, 1);
        fd_write_imp_idx = wasm_module_add_import_func(&mod, "wasi_snapshot_preview1", "fd_write", wasi_io_tidx);
    }

    if (needs_read) {
        uint8_t wasi_io_params[4] = { WASM_TYPE_I32, WASM_TYPE_I32, WASM_TYPE_I32, WASM_TYPE_I32 };
        uint8_t wasi_io_results[1] = { WASM_TYPE_I32 };
        int wasi_io_tidx = wasm_module_add_type(&mod, wasi_io_params, 4, wasi_io_results, 1);
        fd_read_imp_idx = wasm_module_add_import_func(&mod, "wasi_snapshot_preview1", "fd_read", wasi_io_tidx);
    }

    if (needs_exit || synthesize_start) {
        uint8_t exit_params[1] = { WASM_TYPE_I32 };
        int exit_tidx = wasm_module_add_type(&mod, exit_params, 1, NULL, 0);
        proc_exit_imp_idx = wasm_module_add_import_func(&mod, "wasi_snapshot_preview1", "proc_exit", exit_tidx);
    }

    /* 2. Pre-declare all user function signatures and add to Table 0 and Exports */
    curr = prog;
    while (curr) {
        if (curr->kind == ND_FUNC_DEF) {
            uint8_t param_types[16];
            int num_params = curr->num_params;
            int p;
            for (p = 0; p < num_params && p < 16; p++) {
                Type *ptype = (curr->func_params && curr->func_params->types) ? curr->func_params->types[p] : NULL;
                param_types[p] = wasm_valtype_from_ctype(ptype);
            }
            uint8_t res_types[1];
            int num_res = 0;
            Type *ret_type = curr->func_type ? curr->func_type->ret : NULL;
            if (!ret_type || ret_type->kind != TY_VOID) {
                res_types[0] = wasm_valtype_from_ctype(ret_type);
                num_res = 1;
            }
            int t_idx = wasm_module_add_type(&mod, param_types, num_params, res_types, num_res);
            int f_idx = wasm_module_add_function(&mod, curr->func_def_name, t_idx);
            uint32_t abs_f_idx = (uint32_t)(mod.num_imports + f_idx);
            wasm_module_add_table_element(&mod, abs_f_idx);
            if (!curr->is_static) {
                wasm_module_add_export(&mod, curr->func_def_name, WASM_EXPORT_FUNC, abs_f_idx);
            }
        }
        curr = curr->next;
    }

    /* 3. Synthesize freestanding runtime functions */
    if (!has_malloc && needs_malloc) {
        uint8_t m_params[1] = { WASM_TYPE_I32 };
        uint8_t m_res[1] = { WASM_TYPE_I32 };
        int m_tidx = wasm_module_add_type(&mod, m_params, 1, m_res, 1);
        int m_fidx = wasm_module_add_function(&mod, "malloc", m_tidx);
        uint32_t abs_m_idx = (uint32_t)(mod.num_imports + m_fidx);
        wasm_module_add_table_element(&mod, abs_m_idx);
        wasm_module_add_export(&mod, "malloc", WASM_EXPORT_FUNC, abs_m_idx);

        WasmFunction *mfn = &mod.funcs[m_fidx];
        mfn->num_params = 1;
        mfn->local_types[mfn->num_locals++] = WASM_TYPE_I32;
        /* aligned = (size + 7) & -8 */
        wasm_buf_append_u8(&mfn->body, WASM_OP_LOCAL_GET);
        wasm_buf_append_uleb128(&mfn->body, 0);
        wasm_buf_append_u8(&mfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&mfn->body, 7);
        wasm_buf_append_u8(&mfn->body, WASM_OP_I32_ADD);
        wasm_buf_append_u8(&mfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&mfn->body, -8);
        wasm_buf_append_u8(&mfn->body, WASM_OP_I32_AND);
        /* cur = __heap_base */
        wasm_buf_append_u8(&mfn->body, WASM_OP_GLOBAL_GET);
        wasm_buf_append_uleb128(&mfn->body, 1);
        wasm_buf_append_u8(&mfn->body, WASM_OP_LOCAL_TEE);
        wasm_buf_append_uleb128(&mfn->body, 1);
        wasm_buf_append_u8(&mfn->body, WASM_OP_I32_ADD);
        /* __heap_base = cur + aligned */
        wasm_buf_append_u8(&mfn->body, WASM_OP_GLOBAL_SET);
        wasm_buf_append_uleb128(&mfn->body, 1);
        /* return cur */
        wasm_buf_append_u8(&mfn->body, WASM_OP_LOCAL_GET);
        wasm_buf_append_uleb128(&mfn->body, 1);
        wasm_buf_append_u8(&mfn->body, WASM_OP_RETURN);
        wasm_buf_append_u8(&mfn->body, WASM_OP_END);
    }

    if (!has_free && needs_free) {
        uint8_t f_params[1] = { WASM_TYPE_I32 };
        int f_tidx = wasm_module_add_type(&mod, f_params, 1, NULL, 0);
        int f_fidx = wasm_module_add_function(&mod, "free", f_tidx);
        uint32_t abs_f_idx = (uint32_t)(mod.num_imports + f_fidx);
        wasm_module_add_table_element(&mod, abs_f_idx);
        wasm_module_add_export(&mod, "free", WASM_EXPORT_FUNC, abs_f_idx);

        WasmFunction *ffn = &mod.funcs[f_fidx];
        ffn->num_params = 1;
        wasm_buf_append_u8(&ffn->body, WASM_OP_RETURN);
        wasm_buf_append_u8(&ffn->body, WASM_OP_END);
    }

    if (!has_user_puts && needs_puts && fd_write_imp_idx >= 0) {
        uint8_t p_params[1] = { WASM_TYPE_I32 };
        uint8_t p_res[1] = { WASM_TYPE_I32 };
        int p_tidx = wasm_module_add_type(&mod, p_params, 1, p_res, 1);
        int p_fidx = wasm_module_add_function(&mod, "puts", p_tidx);
        uint32_t abs_p_idx = (uint32_t)(mod.num_imports + p_fidx);
        wasm_module_add_table_element(&mod, abs_p_idx);
        wasm_module_add_export(&mod, "puts", WASM_EXPORT_FUNC, abs_p_idx);

        WasmFunction *pfn = &mod.funcs[p_fidx];
        pfn->num_params = 1;
        pfn->local_types[pfn->num_locals++] = WASM_TYPE_I32; /* local 1: len */

        /* len = 0 */
        wasm_buf_append_u8(&pfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&pfn->body, 0);
        wasm_buf_append_u8(&pfn->body, WASM_OP_LOCAL_SET);
        wasm_buf_append_uleb128(&pfn->body, 1);

        /* strlen loop */
        wasm_buf_append_u8(&pfn->body, WASM_OP_BLOCK);
        wasm_buf_append_u8(&pfn->body, WASM_TYPE_VOID);
        wasm_buf_append_u8(&pfn->body, WASM_OP_LOOP);
        wasm_buf_append_u8(&pfn->body, WASM_TYPE_VOID);

        wasm_buf_append_u8(&pfn->body, WASM_OP_LOCAL_GET);
        wasm_buf_append_uleb128(&pfn->body, 0);
        wasm_buf_append_u8(&pfn->body, WASM_OP_LOCAL_GET);
        wasm_buf_append_uleb128(&pfn->body, 1);
        wasm_buf_append_u8(&pfn->body, WASM_OP_I32_ADD);
        wasm_buf_append_u8(&pfn->body, WASM_OP_I32_LOAD8_U);
        wasm_emit_memarg_safe(&pfn->body, 0, 0);
        wasm_buf_append_u8(&pfn->body, WASM_OP_I32_EQZ);
        wasm_buf_append_u8(&pfn->body, WASM_OP_BR_IF);
        wasm_buf_append_uleb128(&pfn->body, 1); /* Break */

        wasm_buf_append_u8(&pfn->body, WASM_OP_LOCAL_GET);
        wasm_buf_append_uleb128(&pfn->body, 1);
        wasm_buf_append_u8(&pfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&pfn->body, 1);
        wasm_buf_append_u8(&pfn->body, WASM_OP_I32_ADD);
        wasm_buf_append_u8(&pfn->body, WASM_OP_LOCAL_SET);
        wasm_buf_append_uleb128(&pfn->body, 1);
        wasm_buf_append_u8(&pfn->body, WASM_OP_BR);
        wasm_buf_append_uleb128(&pfn->body, 0);

        wasm_buf_append_u8(&pfn->body, WASM_OP_END);
        wasm_buf_append_u8(&pfn->body, WASM_OP_END);

        /* Write string via iov 1 at 512 */
        wasm_buf_append_u8(&pfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&pfn->body, 512);
        wasm_buf_append_u8(&pfn->body, WASM_OP_LOCAL_GET);
        wasm_buf_append_uleb128(&pfn->body, 0);
        wasm_buf_append_u8(&pfn->body, WASM_OP_I32_STORE);
        wasm_emit_memarg_safe(&pfn->body, 2, 0);

        wasm_buf_append_u8(&pfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&pfn->body, 512);
        wasm_buf_append_u8(&pfn->body, WASM_OP_LOCAL_GET);
        wasm_buf_append_uleb128(&pfn->body, 1);
        wasm_buf_append_u8(&pfn->body, WASM_OP_I32_STORE);
        wasm_emit_memarg_safe(&pfn->body, 2, 4);

        /* Write newline via iov 2 at 520 */
        wasm_buf_append_u8(&pfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&pfn->body, 536);
        wasm_buf_append_u8(&pfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&pfn->body, 10); /* '\n' */
        wasm_buf_append_u8(&pfn->body, WASM_OP_I32_STORE8);
        wasm_emit_memarg_safe(&pfn->body, 0, 0);

        wasm_buf_append_u8(&pfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&pfn->body, 520);
        wasm_buf_append_u8(&pfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&pfn->body, 536);
        wasm_buf_append_u8(&pfn->body, WASM_OP_I32_STORE);
        wasm_emit_memarg_safe(&pfn->body, 2, 0);

        wasm_buf_append_u8(&pfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&pfn->body, 520);
        wasm_buf_append_u8(&pfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&pfn->body, 1);
        wasm_buf_append_u8(&pfn->body, WASM_OP_I32_STORE);
        wasm_emit_memarg_safe(&pfn->body, 2, 4);

        /* fd_write(1, 512, 2, 540) */
        wasm_buf_append_u8(&pfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&pfn->body, 1);
        wasm_buf_append_u8(&pfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&pfn->body, 512);
        wasm_buf_append_u8(&pfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&pfn->body, 2);
        wasm_buf_append_u8(&pfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&pfn->body, 540);
        wasm_buf_append_u8(&pfn->body, WASM_OP_CALL);
        wasm_buf_append_uleb128(&pfn->body, (uint32_t)fd_write_imp_idx);
        wasm_buf_append_u8(&pfn->body, WASM_OP_DROP);

        wasm_buf_append_u8(&pfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&pfn->body, 0);
        wasm_buf_append_u8(&pfn->body, WASM_OP_RETURN);
        wasm_buf_append_u8(&pfn->body, WASM_OP_END);
    }

    if (!has_user_putchar && needs_putchar && fd_write_imp_idx >= 0) {
        uint8_t pc_params[1] = { WASM_TYPE_I32 };
        uint8_t pc_res[1] = { WASM_TYPE_I32 };
        int pc_tidx = wasm_module_add_type(&mod, pc_params, 1, pc_res, 1);
        int pc_fidx = wasm_module_add_function(&mod, "putchar", pc_tidx);
        uint32_t abs_pc_idx = (uint32_t)(mod.num_imports + pc_fidx);
        wasm_module_add_table_element(&mod, abs_pc_idx);
        wasm_module_add_export(&mod, "putchar", WASM_EXPORT_FUNC, abs_pc_idx);

        WasmFunction *pcfn = &mod.funcs[pc_fidx];
        pcfn->num_params = 1;

        /* Store char at 536 */
        wasm_buf_append_u8(&pcfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&pcfn->body, 536);
        wasm_buf_append_u8(&pcfn->body, WASM_OP_LOCAL_GET);
        wasm_buf_append_uleb128(&pcfn->body, 0);
        wasm_buf_append_u8(&pcfn->body, WASM_OP_I32_STORE8);
        wasm_emit_memarg_safe(&pcfn->body, 0, 0);

        /* iov at 512 */
        wasm_buf_append_u8(&pcfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&pcfn->body, 512);
        wasm_buf_append_u8(&pcfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&pcfn->body, 536);
        wasm_buf_append_u8(&pcfn->body, WASM_OP_I32_STORE);
        wasm_emit_memarg_safe(&pcfn->body, 2, 0);

        wasm_buf_append_u8(&pcfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&pcfn->body, 512);
        wasm_buf_append_u8(&pcfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&pcfn->body, 1);
        wasm_buf_append_u8(&pcfn->body, WASM_OP_I32_STORE);
        wasm_emit_memarg_safe(&pcfn->body, 2, 4);

        /* fd_write(1, 512, 1, 540) */
        wasm_buf_append_u8(&pcfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&pcfn->body, 1);
        wasm_buf_append_u8(&pcfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&pcfn->body, 512);
        wasm_buf_append_u8(&pcfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&pcfn->body, 1);
        wasm_buf_append_u8(&pcfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&pcfn->body, 540);
        wasm_buf_append_u8(&pcfn->body, WASM_OP_CALL);
        wasm_buf_append_uleb128(&pcfn->body, (uint32_t)fd_write_imp_idx);
        wasm_buf_append_u8(&pcfn->body, WASM_OP_DROP);

        wasm_buf_append_u8(&pcfn->body, WASM_OP_LOCAL_GET);
        wasm_buf_append_uleb128(&pcfn->body, 0);
        wasm_buf_append_u8(&pcfn->body, WASM_OP_RETURN);
        wasm_buf_append_u8(&pcfn->body, WASM_OP_END);
    }

    if (!has_user_write && needs_write && fd_write_imp_idx >= 0) {
        uint8_t w_params[3] = { WASM_TYPE_I32, WASM_TYPE_I32, WASM_TYPE_I32 };
        uint8_t w_res[1] = { WASM_TYPE_I32 };
        int w_tidx = wasm_module_add_type(&mod, w_params, 3, w_res, 1);
        int w_fidx = wasm_module_add_function(&mod, "write", w_tidx);
        uint32_t abs_w_idx = (uint32_t)(mod.num_imports + w_fidx);
        wasm_module_add_table_element(&mod, abs_w_idx);
        wasm_module_add_export(&mod, "write", WASM_EXPORT_FUNC, abs_w_idx);

        WasmFunction *wfn = &mod.funcs[w_fidx];
        wfn->num_params = 3;

        wasm_buf_append_u8(&wfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&wfn->body, 512);
        wasm_buf_append_u8(&wfn->body, WASM_OP_LOCAL_GET);
        wasm_buf_append_uleb128(&wfn->body, 1); /* buf */
        wasm_buf_append_u8(&wfn->body, WASM_OP_I32_STORE);
        wasm_emit_memarg_safe(&wfn->body, 2, 0);

        wasm_buf_append_u8(&wfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&wfn->body, 512);
        wasm_buf_append_u8(&wfn->body, WASM_OP_LOCAL_GET);
        wasm_buf_append_uleb128(&wfn->body, 2); /* count */
        wasm_buf_append_u8(&wfn->body, WASM_OP_I32_STORE);
        wasm_emit_memarg_safe(&wfn->body, 2, 4);

        /* fd_write(fd, 512, 1, 540) */
        wasm_buf_append_u8(&wfn->body, WASM_OP_LOCAL_GET);
        wasm_buf_append_uleb128(&wfn->body, 0); /* fd */
        wasm_buf_append_u8(&wfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&wfn->body, 512);
        wasm_buf_append_u8(&wfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&wfn->body, 1);
        wasm_buf_append_u8(&wfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&wfn->body, 540);
        wasm_buf_append_u8(&wfn->body, WASM_OP_CALL);
        wasm_buf_append_uleb128(&wfn->body, (uint32_t)fd_write_imp_idx);
        wasm_buf_append_u8(&wfn->body, WASM_OP_DROP);

        /* Return nwritten */
        wasm_buf_append_u8(&wfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&wfn->body, 540);
        wasm_buf_append_u8(&wfn->body, WASM_OP_I32_LOAD);
        wasm_emit_memarg_safe(&wfn->body, 2, 0);
        wasm_buf_append_u8(&wfn->body, WASM_OP_RETURN);
        wasm_buf_append_u8(&wfn->body, WASM_OP_END);
    }

    if (!has_user_read && needs_read && fd_read_imp_idx >= 0) {
        uint8_t r_params[3] = { WASM_TYPE_I32, WASM_TYPE_I32, WASM_TYPE_I32 };
        uint8_t r_res[1] = { WASM_TYPE_I32 };
        int r_tidx = wasm_module_add_type(&mod, r_params, 3, r_res, 1);
        int r_fidx = wasm_module_add_function(&mod, "read", r_tidx);
        uint32_t abs_r_idx = (uint32_t)(mod.num_imports + r_fidx);
        wasm_module_add_table_element(&mod, abs_r_idx);
        wasm_module_add_export(&mod, "read", WASM_EXPORT_FUNC, abs_r_idx);

        WasmFunction *rfn = &mod.funcs[r_fidx];
        rfn->num_params = 3;

        wasm_buf_append_u8(&rfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&rfn->body, 512);
        wasm_buf_append_u8(&rfn->body, WASM_OP_LOCAL_GET);
        wasm_buf_append_uleb128(&rfn->body, 1); /* buf */
        wasm_buf_append_u8(&rfn->body, WASM_OP_I32_STORE);
        wasm_emit_memarg_safe(&rfn->body, 2, 0);

        wasm_buf_append_u8(&rfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&rfn->body, 512);
        wasm_buf_append_u8(&rfn->body, WASM_OP_LOCAL_GET);
        wasm_buf_append_uleb128(&rfn->body, 2); /* count */
        wasm_buf_append_u8(&rfn->body, WASM_OP_I32_STORE);
        wasm_emit_memarg_safe(&rfn->body, 2, 4);

        /* fd_read(fd, 512, 1, 540) */
        wasm_buf_append_u8(&rfn->body, WASM_OP_LOCAL_GET);
        wasm_buf_append_uleb128(&rfn->body, 0); /* fd */
        wasm_buf_append_u8(&rfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&rfn->body, 512);
        wasm_buf_append_u8(&rfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&rfn->body, 1);
        wasm_buf_append_u8(&rfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&rfn->body, 540);
        wasm_buf_append_u8(&rfn->body, WASM_OP_CALL);
        wasm_buf_append_uleb128(&rfn->body, (uint32_t)fd_read_imp_idx);
        wasm_buf_append_u8(&rfn->body, WASM_OP_DROP);

        /* Return nread */
        wasm_buf_append_u8(&rfn->body, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&rfn->body, 540);
        wasm_buf_append_u8(&rfn->body, WASM_OP_I32_LOAD);
        wasm_emit_memarg_safe(&rfn->body, 2, 0);
        wasm_buf_append_u8(&rfn->body, WASM_OP_RETURN);
        wasm_buf_append_u8(&rfn->body, WASM_OP_END);
    }

    if (!has_user_exit && needs_exit && proc_exit_imp_idx >= 0) {
        uint8_t ex_params[1] = { WASM_TYPE_I32 };
        int ex_tidx = wasm_module_add_type(&mod, ex_params, 1, NULL, 0);
        int ex_fidx = wasm_module_add_function(&mod, "exit", ex_tidx);
        uint32_t abs_ex_idx = (uint32_t)(mod.num_imports + ex_fidx);
        wasm_module_add_table_element(&mod, abs_ex_idx);
        wasm_module_add_export(&mod, "exit", WASM_EXPORT_FUNC, abs_ex_idx);

        WasmFunction *exfn = &mod.funcs[ex_fidx];
        exfn->num_params = 1;
        wasm_buf_append_u8(&exfn->body, WASM_OP_LOCAL_GET);
        wasm_buf_append_uleb128(&exfn->body, 0);
        wasm_buf_append_u8(&exfn->body, WASM_OP_CALL);
        wasm_buf_append_uleb128(&exfn->body, (uint32_t)proc_exit_imp_idx);
        wasm_buf_append_u8(&exfn->body, WASM_OP_UNREACHABLE);
        wasm_buf_append_u8(&exfn->body, WASM_OP_END);
    }

    if (synthesize_start) {
        int st_tidx = wasm_module_add_type(&mod, NULL, 0, NULL, 0);
        int st_fidx = wasm_module_add_function(&mod, "_start", st_tidx);
        uint32_t abs_st_idx = (uint32_t)(mod.num_imports + st_fidx);
        wasm_module_add_table_element(&mod, abs_st_idx);
        wasm_module_add_export(&mod, "_start", WASM_EXPORT_FUNC, abs_st_idx);

        WasmFunction *stfn = &mod.funcs[st_fidx];
        stfn->num_params = 0;

        int main_idx = wasm_find_func_idx(&mod, "main");
        if (main_idx >= 0) {
            int main_tidx = -1;
            if (main_idx < mod.num_imports) main_tidx = mod.imports[main_idx].type_idx;
            else if (main_idx - mod.num_imports < mod.num_funcs) main_tidx = mod.funcs[main_idx - mod.num_imports].type_idx;

            if (main_tidx >= 0) {
                int p;
                for (p = 0; p < mod.types[main_tidx].num_params; p++) {
                    wasm_buf_append_u8(&stfn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&stfn->body, 0);
                }
                wasm_buf_append_u8(&stfn->body, WASM_OP_CALL);
                wasm_buf_append_uleb128(&stfn->body, (uint32_t)main_idx);

                if (mod.types[main_tidx].num_results > 0) {
                    if (proc_exit_imp_idx >= 0) {
                        wasm_buf_append_u8(&stfn->body, WASM_OP_CALL);
                        wasm_buf_append_uleb128(&stfn->body, (uint32_t)proc_exit_imp_idx);
                        wasm_buf_append_u8(&stfn->body, WASM_OP_UNREACHABLE);
                    } else {
                        wasm_buf_append_u8(&stfn->body, WASM_OP_DROP);
                        wasm_buf_append_u8(&stfn->body, WASM_OP_RETURN);
                    }
                } else {
                    if (proc_exit_imp_idx >= 0) {
                        wasm_buf_append_u8(&stfn->body, WASM_OP_I32_CONST);
                        wasm_buf_append_sleb128(&stfn->body, 0);
                        wasm_buf_append_u8(&stfn->body, WASM_OP_CALL);
                        wasm_buf_append_uleb128(&stfn->body, (uint32_t)proc_exit_imp_idx);
                        wasm_buf_append_u8(&stfn->body, WASM_OP_UNREACHABLE);
                    } else {
                        wasm_buf_append_u8(&stfn->body, WASM_OP_RETURN);
                    }
                }
            }
        }
        wasm_buf_append_u8(&stfn->body, WASM_OP_END);
    }

    /* 4. Second pass: Lower user function bodies with linear stack frame support */
    curr = prog;
    int f_cursor = 0;
    while (curr) {
        if (curr->kind == ND_FUNC_DEF && f_cursor < mod.num_funcs) {
            WasmFunction *fn = &mod.funcs[f_cursor++];
            WasmLocalMap lmap;
            memset(&lmap, 0, sizeof(WasmLocalMap));
            lmap.fp_local_idx = -1;
            lmap.tmp_addr_idx = -1;
            lmap.tmp_val_idx = -1;
            lmap.tmp_i64_idx = -1;
            lmap.tmp_f32_idx = -1;
            lmap.tmp_f64_idx = -1;
            lmap.tmp_v128_idx = -1;

            Type *ret_type = curr->func_type ? curr->func_type->ret : NULL;

            /* Map parameters to initial local indices */
            if (curr->func_params) {
                int p;
                for (p = 0; p < curr->num_params && p < MAX_PARAMS; p++) {
                    const char *pname = curr->func_params->names[p];
                    Type *ptype = curr->func_params->types ? curr->func_params->types[p] : NULL;
                    if (pname && pname[0]) {
                        int e = lmap.count++;
                        strncpy(lmap.entries[e].name, pname, sizeof(lmap.entries[e].name) - 1);
                        lmap.entries[e].local_idx = (uint32_t)p;
                        lmap.entries[e].type = ptype;
                        lmap.entries[e].val_type = wasm_valtype_from_ctype(ptype);
                        lmap.entries[e].is_memory = 0;
                    }
                }
            }
            fn->num_params = curr->num_params;

            /* Scan AST for local variables & memory allocations */
            if (curr->body) {
                wasm_scan_ast_locals(&mod, curr->body, &lmap, &gmap);
            }

            /* Assign stack frame offsets and WASM local indices */
            int frame_size = 0;
            int i;
            for (i = 0; i < lmap.count; i++) {
                if (lmap.entries[i].is_memory) {
                    int esz = lmap.entries[i].size > 0 ? lmap.entries[i].size : 4;
                    int ealign = (esz >= 8) ? 8 : 4;
                    frame_size = (frame_size + (ealign - 1)) & ~(ealign - 1);
                    lmap.entries[i].stack_offset = frame_size;
                    frame_size += esz;
                } else if (i >= (int)curr->num_params) {
                    lmap.entries[i].local_idx = fn->num_params + fn->num_locals++;
                    fn->local_types[fn->num_locals - 1] = lmap.entries[i].val_type ? lmap.entries[i].val_type : WASM_TYPE_I32;
                }
            }

            /* Allocate frame pointer and temporary locals */
            if (frame_size > 0) {
                frame_size = (frame_size + 15) & -16; /* 16-byte frame alignment */
                lmap.frame_size = frame_size;
                lmap.fp_local_idx = (int)(fn->num_params + fn->num_locals++);
                fn->local_types[fn->num_locals - 1] = WASM_TYPE_I32;
            }
            lmap.tmp_addr_idx = (int)(fn->num_params + fn->num_locals++);
            fn->local_types[fn->num_locals - 1] = WASM_TYPE_I32;
            lmap.tmp_val_idx = (int)(fn->num_params + fn->num_locals++);
            fn->local_types[fn->num_locals - 1] = WASM_TYPE_I32;
            lmap.tmp_i64_idx = (int)(fn->num_params + fn->num_locals++);
            fn->local_types[fn->num_locals - 1] = WASM_TYPE_I64;
            lmap.tmp_f32_idx = (int)(fn->num_params + fn->num_locals++);
            fn->local_types[fn->num_locals - 1] = WASM_TYPE_F32;
            lmap.tmp_f64_idx = (int)(fn->num_params + fn->num_locals++);
            fn->local_types[fn->num_locals - 1] = WASM_TYPE_F64;

            /* Prologue: allocate linear stack frame */
            if (frame_size > 0 && lmap.fp_local_idx >= 0) {
                wasm_buf_append_u8(&fn->body, WASM_OP_GLOBAL_GET);
                wasm_buf_append_uleb128(&fn->body, 0); /* __stack_pointer */
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                wasm_buf_append_sleb128(&fn->body, frame_size);
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_SUB);
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_TEE);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap.fp_local_idx);
                wasm_buf_append_u8(&fn->body, WASM_OP_GLOBAL_SET);
                wasm_buf_append_uleb128(&fn->body, 0);
            }

            /* Lower the function body statement(s) */
            wasm_stmt_block_depth = 0;
            wasm_cur_break_depth = -1;
            wasm_cur_cont_depth = -1;
            if (curr->body) {
                wasm_lower_stmt(cc, &mod, fn, &lmap, &gmap, curr->body, ret_type);
            }

            /* Epilogue: frame cleanup, default return, and function-closing END */
            if (frame_size > 0 && lmap.fp_local_idx >= 0) {
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap.fp_local_idx);
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                wasm_buf_append_sleb128(&fn->body, frame_size);
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD);
                wasm_buf_append_u8(&fn->body, WASM_OP_GLOBAL_SET);
                wasm_buf_append_uleb128(&fn->body, 0);
            }
            uint8_t func_ret_vt = wasm_valtype_from_ctype(ret_type);
            if (ret_type && ret_type->kind == TY_VOID) {
                wasm_buf_append_u8(&fn->body, WASM_OP_RETURN);
            } else if (func_ret_vt == WASM_TYPE_F32) {
                wasm_buf_append_u8(&fn->body, WASM_OP_F32_CONST);
                wasm_buf_append_f32(&fn->body, 0.0f);
                wasm_buf_append_u8(&fn->body, WASM_OP_RETURN);
            } else if (func_ret_vt == WASM_TYPE_F64) {
                wasm_buf_append_u8(&fn->body, WASM_OP_F64_CONST);
                wasm_buf_append_f64(&fn->body, 0.0);
                wasm_buf_append_u8(&fn->body, WASM_OP_RETURN);
            } else if (func_ret_vt == WASM_TYPE_I64) {
                wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                wasm_buf_append_sleb128_i64(&fn->body, 0);
                wasm_buf_append_u8(&fn->body, WASM_OP_RETURN);
            } else {
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                wasm_buf_append_sleb128(&fn->body, 0);
                wasm_buf_append_u8(&fn->body, WASM_OP_RETURN);
            }
            wasm_buf_append_u8(&fn->body, WASM_OP_END);
        }
        curr = curr->next;
    }

    /* 5. Synchronize Table 0 elements to cover all imported and defined functions */
    mod.num_table_elements = 0;
    int total_funcs = mod.num_imports + mod.num_funcs;
    int tf;
    for (tf = 0; tf < total_funcs; tf++) {
        wasm_module_add_table_element(&mod, (uint32_t)tf);
    }

    /* 6. Write final binary .wasm module to file */
    int ret = wasm_module_write_file(&mod, output_file);
    wasm_module_free(&mod);
    return ret;
}
