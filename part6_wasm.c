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
    if (type->kind == TY_FLOAT) return WASM_TYPE_F32;
    if (type->kind == TY_DOUBLE || type->kind == TY_LONGDOUBLE) return WASM_TYPE_F64;
    return WASM_TYPE_I32;
}

static uint8_t wasm_node_valtype(Node *n) {
    if (!n) return WASM_TYPE_I32;
    if (n->kind == ND_FLIT) {
        if (n->type && n->type->kind == TY_FLOAT) return WASM_TYPE_F32;
        return WASM_TYPE_F64;
    }
    if (n->type) {
        return wasm_valtype_from_ctype(n->type);
    }
    return WASM_TYPE_I32;
}

static uint8_t wasm_promoted_valtype(uint8_t vt1, uint8_t vt2) {
    if (vt1 == WASM_TYPE_F64 || vt2 == WASM_TYPE_F64) return WASM_TYPE_F64;
    if (vt1 == WASM_TYPE_F32 || vt2 == WASM_TYPE_F32) return WASM_TYPE_F32;
    return WASM_TYPE_I32;
}

static void wasm_emit_numeric_conversion(WASMBuffer *buf, uint8_t src_vt, uint8_t dst_vt, int is_unsigned) {
    if (src_vt == dst_vt) return;

    if (src_vt == WASM_TYPE_I32 && dst_vt == WASM_TYPE_F32) {
        wasm_buf_append_u8(buf, is_unsigned ? WASM_OP_F32_CONVERT_I32_U : WASM_OP_F32_CONVERT_I32_S);
    } else if (src_vt == WASM_TYPE_I32 && dst_vt == WASM_TYPE_F64) {
        wasm_buf_append_u8(buf, is_unsigned ? WASM_OP_F64_CONVERT_I32_U : WASM_OP_F64_CONVERT_I32_S);
    } else if (src_vt == WASM_TYPE_F32 && dst_vt == WASM_TYPE_F64) {
        wasm_buf_append_u8(buf, WASM_OP_F64_PROMOTE_F32);
    } else if (src_vt == WASM_TYPE_F64 && dst_vt == WASM_TYPE_F32) {
        wasm_buf_append_u8(buf, WASM_OP_F32_DEMOTE_F64);
    } else if (src_vt == WASM_TYPE_F32 && dst_vt == WASM_TYPE_I32) {
        wasm_buf_append_u8(buf, is_unsigned ? WASM_OP_I32_TRUNC_F32_U : WASM_OP_I32_TRUNC_F32_S);
    } else if (src_vt == WASM_TYPE_F64 && dst_vt == WASM_TYPE_I32) {
        wasm_buf_append_u8(buf, is_unsigned ? WASM_OP_I32_TRUNC_F64_U : WASM_OP_I32_TRUNC_F64_S);
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
    if (n->kind == ND_ASSIGN || n->kind == ND_COMPOUND_ASSIGN || n->kind == ND_CALL) return 1;
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
            wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
            wasm_buf_append_sleb128(&fn->body, (int32_t)node->int_val);
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

            if (node->lhs && node->lhs->kind == ND_VAR) {
                const char *vname = node->lhs->sym ? node->lhs->sym->name : node->lhs->name;
                WasmLocalEntry *entry = wasm_local_find_entry(lmap, vname);
                if (entry && !entry->is_memory) {
                    /* Direct local register assignment with numeric conversion */
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
                    wasm_emit_numeric_conversion(&fn->body, rhs_vt, entry->val_type, is_unsigned_type(node->lhs->type));
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_TEE);
                    wasm_buf_append_uleb128(&fn->body, entry->local_idx);
                    break;
                }
            }

            /* Memory Lvalue Store: [addr, val] */
            wasm_lower_addr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_numeric_conversion(&fn->body, rhs_vt, lhs_vt, is_unsigned_type(node->lhs->type));

            if (lhs_vt == WASM_TYPE_F32) {
                if (lmap->tmp_f32_idx >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_TEE);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_f32_idx);
                }
                wasm_emit_typed_store(&fn->body, node->lhs ? node->lhs->type : NULL, 0);
                if (lmap->tmp_f32_idx >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_f32_idx);
                }
            } else if (lhs_vt == WASM_TYPE_F64) {
                if (lmap->tmp_f64_idx >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_TEE);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_f64_idx);
                }
                wasm_emit_typed_store(&fn->body, node->lhs ? node->lhs->type : NULL, 0);
                if (lmap->tmp_f64_idx >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_f64_idx);
                }
            } else {
                if (lmap->tmp_val_idx >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_TEE);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                }
                wasm_emit_typed_store(&fn->body, node->lhs ? node->lhs->type : NULL, 0);
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
            uint8_t common_vt = wasm_promoted_valtype(lhs_vt, rhs_vt);

            if (node->lhs && node->lhs->kind == ND_VAR) {
                const char *vname = node->lhs->sym ? node->lhs->sym->name : node->lhs->name;
                WasmLocalEntry *entry = wasm_local_find_entry(lmap, vname);
                if (entry && !entry->is_memory) {
                    /* Register compound assignment */
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, entry->local_idx);
                    wasm_emit_numeric_conversion(&fn->body, entry->val_type, common_vt, is_unsigned_type(node->lhs->type));
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
                    wasm_emit_numeric_conversion(&fn->body, rhs_vt, common_vt, is_unsigned_type(node->rhs->type));

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
                    } else {
                        switch (node->compound_op) {
                            case ND_ADD: wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD); break;
                            case ND_SUB: wasm_buf_append_u8(&fn->body, WASM_OP_I32_SUB); break;
                            case ND_MUL: wasm_buf_append_u8(&fn->body, WASM_OP_I32_MUL); break;
                            case ND_DIV: wasm_buf_append_u8(&fn->body, WASM_OP_I32_DIV_S); break;
                            case ND_MOD: wasm_buf_append_u8(&fn->body, WASM_OP_I32_REM_S); break;
                            case ND_BAND: wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND); break;
                            case ND_BOR: wasm_buf_append_u8(&fn->body, WASM_OP_I32_OR); break;
                            case ND_BXOR: wasm_buf_append_u8(&fn->body, WASM_OP_I32_XOR); break;
                            case ND_SHL: wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHL); break;
                            case ND_SHR: wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHR_S); break;
                            default: wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD); break;
                        }
                    }
                    wasm_emit_numeric_conversion(&fn->body, common_vt, entry->val_type, is_unsigned_type(node->lhs->type));
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
            wasm_emit_typed_load(&fn->body, node->lhs ? node->lhs->type : NULL, 0);
            wasm_emit_numeric_conversion(&fn->body, lhs_vt, common_vt, is_unsigned_type(node->lhs->type));
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_numeric_conversion(&fn->body, rhs_vt, common_vt, is_unsigned_type(node->rhs->type));

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
            } else {
                switch (node->compound_op) {
                    case ND_ADD: wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD); break;
                    case ND_SUB: wasm_buf_append_u8(&fn->body, WASM_OP_I32_SUB); break;
                    case ND_MUL: wasm_buf_append_u8(&fn->body, WASM_OP_I32_MUL); break;
                    case ND_DIV: wasm_buf_append_u8(&fn->body, WASM_OP_I32_DIV_S); break;
                    case ND_MOD: wasm_buf_append_u8(&fn->body, WASM_OP_I32_REM_S); break;
                    case ND_BAND: wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND); break;
                    case ND_BOR: wasm_buf_append_u8(&fn->body, WASM_OP_I32_OR); break;
                    case ND_BXOR: wasm_buf_append_u8(&fn->body, WASM_OP_I32_XOR); break;
                    case ND_SHL: wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHL); break;
                    case ND_SHR: wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHR_S); break;
                    default: wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD); break;
                }
            }
            wasm_emit_numeric_conversion(&fn->body, common_vt, lhs_vt, is_unsigned_type(node->lhs->type));

            if (lhs_vt == WASM_TYPE_F32) {
                if (lmap->tmp_f32_idx >= 0 && lmap->tmp_addr_idx >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_f32_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_addr_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_f32_idx);
                }
                wasm_emit_typed_store(&fn->body, node->lhs ? node->lhs->type : NULL, 0);
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
                wasm_emit_typed_store(&fn->body, node->lhs ? node->lhs->type : NULL, 0);
                if (lmap->tmp_f64_idx >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_f64_idx);
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
                wasm_emit_typed_store(&fn->body, node->lhs ? node->lhs->type : NULL, 0);
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
            uint8_t common_vt = wasm_promoted_valtype(lhs_vt, rhs_vt);

            if (common_vt == WASM_TYPE_I32) {
                int esz_l = (node->lhs && node->lhs->type) ? wasm_ptr_elem_size(node->lhs->type) : 1;
                int esz_r = (node->rhs && node->rhs->type) ? wasm_ptr_elem_size(node->rhs->type) : 1;
                if (node->lhs && node->lhs->type && (is_pointer(node->lhs->type) || node->lhs->type->kind == TY_ARRAY) && esz_l > 1) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, esz_l);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_MUL);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD);
                    break;
                } else if (node->rhs && node->rhs->type && (is_pointer(node->rhs->type) || node->rhs->type->kind == TY_ARRAY) && esz_r > 1) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, esz_r);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_MUL);
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD);
                    break;
                }
            }

            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_emit_numeric_conversion(&fn->body, lhs_vt, common_vt, is_unsigned_type(node->lhs->type));
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_numeric_conversion(&fn->body, rhs_vt, common_vt, is_unsigned_type(node->rhs->type));

            if (common_vt == WASM_TYPE_F32) wasm_buf_append_u8(&fn->body, WASM_OP_F32_ADD);
            else if (common_vt == WASM_TYPE_F64) wasm_buf_append_u8(&fn->body, WASM_OP_F64_ADD);
            else wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD);
            break;
        }

        case ND_SUB:
        case ND_FSUB: {
            uint8_t lhs_vt = wasm_node_valtype(node->lhs);
            uint8_t rhs_vt = wasm_node_valtype(node->rhs);
            uint8_t common_vt = wasm_promoted_valtype(lhs_vt, rhs_vt);

            if (common_vt == WASM_TYPE_I32) {
                int esz_l = (node->lhs && node->lhs->type) ? wasm_ptr_elem_size(node->lhs->type) : 1;
                if (node->lhs && node->lhs->type && (is_pointer(node->lhs->type) || node->lhs->type->kind == TY_ARRAY) &&
                    node->rhs && node->rhs->type && (is_pointer(node->rhs->type) || node->rhs->type->kind == TY_ARRAY)) {
                    /* Pointer difference: (p1 - p2) / sizeof(*p) */
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_SUB);
                    if (esz_l > 1) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                        wasm_buf_append_sleb128(&fn->body, esz_l);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_DIV_S);
                    }
                    break;
                } else if (node->lhs && node->lhs->type && (is_pointer(node->lhs->type) || node->lhs->type->kind == TY_ARRAY) && esz_l > 1) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, esz_l);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_MUL);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_SUB);
                    break;
                }
            }

            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_emit_numeric_conversion(&fn->body, lhs_vt, common_vt, is_unsigned_type(node->lhs->type));
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_numeric_conversion(&fn->body, rhs_vt, common_vt, is_unsigned_type(node->rhs->type));

            if (common_vt == WASM_TYPE_F32) wasm_buf_append_u8(&fn->body, WASM_OP_F32_SUB);
            else if (common_vt == WASM_TYPE_F64) wasm_buf_append_u8(&fn->body, WASM_OP_F64_SUB);
            else wasm_buf_append_u8(&fn->body, WASM_OP_I32_SUB);
            break;
        }

        case ND_MUL:
        case ND_FMUL: {
            uint8_t lhs_vt = wasm_node_valtype(node->lhs);
            uint8_t rhs_vt = wasm_node_valtype(node->rhs);
            uint8_t common_vt = wasm_promoted_valtype(lhs_vt, rhs_vt);

            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_emit_numeric_conversion(&fn->body, lhs_vt, common_vt, is_unsigned_type(node->lhs->type));
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_numeric_conversion(&fn->body, rhs_vt, common_vt, is_unsigned_type(node->rhs->type));

            if (common_vt == WASM_TYPE_F32) wasm_buf_append_u8(&fn->body, WASM_OP_F32_MUL);
            else if (common_vt == WASM_TYPE_F64) wasm_buf_append_u8(&fn->body, WASM_OP_F64_MUL);
            else wasm_buf_append_u8(&fn->body, WASM_OP_I32_MUL);
            break;
        }

        case ND_DIV:
        case ND_FDIV: {
            uint8_t lhs_vt = wasm_node_valtype(node->lhs);
            uint8_t rhs_vt = wasm_node_valtype(node->rhs);
            uint8_t common_vt = wasm_promoted_valtype(lhs_vt, rhs_vt);

            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_emit_numeric_conversion(&fn->body, lhs_vt, common_vt, is_unsigned_type(node->lhs->type));
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_numeric_conversion(&fn->body, rhs_vt, common_vt, is_unsigned_type(node->rhs->type));

            if (common_vt == WASM_TYPE_F32) wasm_buf_append_u8(&fn->body, WASM_OP_F32_DIV);
            else if (common_vt == WASM_TYPE_F64) wasm_buf_append_u8(&fn->body, WASM_OP_F64_DIV);
            else {
                if (node->lhs && node->lhs->type && is_unsigned_type(node->lhs->type)) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_DIV_S);
                } else {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_DIV_S);
                }
            }
            break;
        }

        case ND_MOD:
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_buf_append_u8(&fn->body, WASM_OP_I32_REM_S);
            break;

        case ND_BAND:
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_buf_append_u8(&fn->body, WASM_OP_I32_AND);
            break;

        case ND_BOR:
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_buf_append_u8(&fn->body, WASM_OP_I32_OR);
            break;

        case ND_BXOR:
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_buf_append_u8(&fn->body, WASM_OP_I32_XOR);
            break;

        case ND_SHL:
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHL);
            break;

        case ND_SHR:
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            if (node->lhs && node->lhs->type && is_unsigned_type(node->lhs->type)) {
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHR_U);
            } else {
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_SHR_S);
            }
            break;

        case ND_EQ: {
            uint8_t lhs_vt = wasm_node_valtype(node->lhs);
            uint8_t rhs_vt = wasm_node_valtype(node->rhs);
            uint8_t common_vt = wasm_promoted_valtype(lhs_vt, rhs_vt);

            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_emit_numeric_conversion(&fn->body, lhs_vt, common_vt, is_unsigned_type(node->lhs->type));
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_numeric_conversion(&fn->body, rhs_vt, common_vt, is_unsigned_type(node->rhs->type));

            if (common_vt == WASM_TYPE_F32) wasm_buf_append_u8(&fn->body, WASM_OP_F32_EQ);
            else if (common_vt == WASM_TYPE_F64) wasm_buf_append_u8(&fn->body, WASM_OP_F64_EQ);
            else wasm_buf_append_u8(&fn->body, WASM_OP_I32_EQ);
            break;
        }

        case ND_NE: {
            uint8_t lhs_vt = wasm_node_valtype(node->lhs);
            uint8_t rhs_vt = wasm_node_valtype(node->rhs);
            uint8_t common_vt = wasm_promoted_valtype(lhs_vt, rhs_vt);

            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_emit_numeric_conversion(&fn->body, lhs_vt, common_vt, is_unsigned_type(node->lhs->type));
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_numeric_conversion(&fn->body, rhs_vt, common_vt, is_unsigned_type(node->rhs->type));

            if (common_vt == WASM_TYPE_F32) wasm_buf_append_u8(&fn->body, WASM_OP_F32_NE);
            else if (common_vt == WASM_TYPE_F64) wasm_buf_append_u8(&fn->body, WASM_OP_F64_NE);
            else wasm_buf_append_u8(&fn->body, WASM_OP_I32_NE);
            break;
        }

        case ND_LT: {
            uint8_t lhs_vt = wasm_node_valtype(node->lhs);
            uint8_t rhs_vt = wasm_node_valtype(node->rhs);
            uint8_t common_vt = wasm_promoted_valtype(lhs_vt, rhs_vt);

            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_emit_numeric_conversion(&fn->body, lhs_vt, common_vt, is_unsigned_type(node->lhs->type));
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_numeric_conversion(&fn->body, rhs_vt, common_vt, is_unsigned_type(node->rhs->type));

            if (common_vt == WASM_TYPE_F32) wasm_buf_append_u8(&fn->body, WASM_OP_F32_LT);
            else if (common_vt == WASM_TYPE_F64) wasm_buf_append_u8(&fn->body, WASM_OP_F64_LT);
            else wasm_buf_append_u8(&fn->body, WASM_OP_I32_LT_S);
            break;
        }

        case ND_LE: {
            uint8_t lhs_vt = wasm_node_valtype(node->lhs);
            uint8_t rhs_vt = wasm_node_valtype(node->rhs);
            uint8_t common_vt = wasm_promoted_valtype(lhs_vt, rhs_vt);

            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_emit_numeric_conversion(&fn->body, lhs_vt, common_vt, is_unsigned_type(node->lhs->type));
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_numeric_conversion(&fn->body, rhs_vt, common_vt, is_unsigned_type(node->rhs->type));

            if (common_vt == WASM_TYPE_F32) wasm_buf_append_u8(&fn->body, WASM_OP_F32_LE);
            else if (common_vt == WASM_TYPE_F64) wasm_buf_append_u8(&fn->body, WASM_OP_F64_LE);
            else wasm_buf_append_u8(&fn->body, WASM_OP_I32_LE_S);
            break;
        }

        case ND_GT: {
            uint8_t lhs_vt = wasm_node_valtype(node->lhs);
            uint8_t rhs_vt = wasm_node_valtype(node->rhs);
            uint8_t common_vt = wasm_promoted_valtype(lhs_vt, rhs_vt);

            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_emit_numeric_conversion(&fn->body, lhs_vt, common_vt, is_unsigned_type(node->lhs->type));
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_numeric_conversion(&fn->body, rhs_vt, common_vt, is_unsigned_type(node->rhs->type));

            if (common_vt == WASM_TYPE_F32) wasm_buf_append_u8(&fn->body, WASM_OP_F32_GT);
            else if (common_vt == WASM_TYPE_F64) wasm_buf_append_u8(&fn->body, WASM_OP_F64_GT);
            else wasm_buf_append_u8(&fn->body, WASM_OP_I32_GT_S);
            break;
        }

        case ND_GE: {
            uint8_t lhs_vt = wasm_node_valtype(node->lhs);
            uint8_t rhs_vt = wasm_node_valtype(node->rhs);
            uint8_t common_vt = wasm_promoted_valtype(lhs_vt, rhs_vt);

            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_emit_numeric_conversion(&fn->body, lhs_vt, common_vt, is_unsigned_type(node->lhs->type));
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_numeric_conversion(&fn->body, rhs_vt, common_vt, is_unsigned_type(node->rhs->type));

            if (common_vt == WASM_TYPE_F32) wasm_buf_append_u8(&fn->body, WASM_OP_F32_GE);
            else if (common_vt == WASM_TYPE_F64) wasm_buf_append_u8(&fn->body, WASM_OP_F64_GE);
            else wasm_buf_append_u8(&fn->body, WASM_OP_I32_GE_S);
            break;
        }

        case ND_LNOT:
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_buf_append_u8(&fn->body, WASM_OP_I32_EQZ);
            break;

        case ND_NEG: {
            uint8_t vt = wasm_node_valtype(node->lhs);
            if (vt == WASM_TYPE_F32) {
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
                wasm_buf_append_u8(&fn->body, WASM_OP_F32_NEG);
            } else if (vt == WASM_TYPE_F64) {
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
                wasm_buf_append_u8(&fn->body, WASM_OP_F64_NEG);
            } else {
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                wasm_buf_append_sleb128(&fn->body, 0);
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_SUB);
            }
            break;
        }

        case ND_CAST: {
            uint8_t src_vt = wasm_node_valtype(node->lhs);
            uint8_t dst_vt = wasm_valtype_from_ctype(node->type ? node->type : node->cast_type);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_emit_numeric_conversion(&fn->body, src_vt, dst_vt, is_unsigned_type(node->type ? node->type : node->cast_type));
            break;
        }

        case ND_CALL: {
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
                        wasm_emit_numeric_conversion(&fn->body, arg_vt, param_vt, is_unsigned_type(node->args[a]->type));
                    }
                }
            }

            if (callee_idx >= 0) {
                /* Direct static function call using absolute function index */
                wasm_buf_append_u8(&fn->body, WASM_OP_CALL);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)callee_idx);
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
            if (cond_vt == WASM_TYPE_F32) {
                wasm_buf_append_u8(&fn->body, WASM_OP_F32_CONST);
                wasm_buf_append_f32(&fn->body, 0.0f);
                wasm_buf_append_u8(&fn->body, WASM_OP_F32_NE);
            } else if (cond_vt == WASM_TYPE_F64) {
                wasm_buf_append_u8(&fn->body, WASM_OP_F64_CONST);
                wasm_buf_append_f64(&fn->body, 0.0);
                wasm_buf_append_u8(&fn->body, WASM_OP_F64_NE);
            }

            /* WASM if block producing target_vt value */
            wasm_buf_append_u8(&fn->body, WASM_OP_IF);
            wasm_buf_append_u8(&fn->body, target_vt);

            if (node->then_body) {
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->then_body);
                wasm_emit_numeric_conversion(&fn->body, then_vt, target_vt, is_unsigned_type(node->then_body->type));
            } else {
                if (target_vt == WASM_TYPE_F32) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_F32_CONST);
                    wasm_buf_append_f32(&fn->body, 0.0f);
                } else if (target_vt == WASM_TYPE_F64) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_F64_CONST);
                    wasm_buf_append_f64(&fn->body, 0.0);
                } else {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 0);
                }
            }

            wasm_buf_append_u8(&fn->body, WASM_OP_ELSE);

            if (node->else_body) {
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->else_body);
                wasm_emit_numeric_conversion(&fn->body, else_vt, target_vt, is_unsigned_type(node->else_body->type));
            } else {
                if (target_vt == WASM_TYPE_F32) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_F32_CONST);
                    wasm_buf_append_f32(&fn->body, 0.0f);
                } else if (target_vt == WASM_TYPE_F64) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_F64_CONST);
                    wasm_buf_append_f64(&fn->body, 0.0);
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

/* Statement Lowering */
static void wasm_lower_stmt(Compiler *cc, const WasmModule *mod, WasmFunction *fn, WasmLocalMap *lmap, WasmGlobalMap *gmap, Node *node, Type *ret_type) {
    if (!node || !fn) return;

    switch (node->kind) {
        case ND_VAR:
        case ND_NOP:
            /* Declarations without initializers produce no runtime bytecode */
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
                wasm_emit_numeric_conversion(&fn->body, expr_vt, func_ret_vt, is_unsigned_type(ret_type));

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
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->cond);
            wasm_buf_append_u8(&fn->body, WASM_OP_IF);
            wasm_buf_append_u8(&fn->body, WASM_TYPE_VOID);
            if (then_node) wasm_lower_stmt(cc, mod, fn, lmap, gmap, then_node, ret_type);
            if (else_node) {
                wasm_buf_append_u8(&fn->body, WASM_OP_ELSE);
                wasm_lower_stmt(cc, mod, fn, lmap, gmap, else_node, ret_type);
            }
            wasm_buf_append_u8(&fn->body, WASM_OP_END);
            break;
        }

        case ND_WHILE: {
            Node *loop_body = node->body ? node->body : node->then_body;
            wasm_buf_append_u8(&fn->body, WASM_OP_BLOCK);
            wasm_buf_append_u8(&fn->body, WASM_TYPE_VOID);
            wasm_buf_append_u8(&fn->body, WASM_OP_LOOP);
            wasm_buf_append_u8(&fn->body, WASM_TYPE_VOID);

            if (node->cond) {
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->cond);
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_EQZ);
                wasm_buf_append_u8(&fn->body, WASM_OP_BR_IF);
                wasm_buf_append_uleb128(&fn->body, 1); /* Break to outer block */
            }

            if (loop_body) {
                wasm_lower_stmt(cc, mod, fn, lmap, gmap, loop_body, ret_type);
            }

            wasm_buf_append_u8(&fn->body, WASM_OP_BR);
            wasm_buf_append_uleb128(&fn->body, 0); /* Loop back */
            wasm_buf_append_u8(&fn->body, WASM_OP_END);
            wasm_buf_append_u8(&fn->body, WASM_OP_END);
            break;
        }

        case ND_FOR: {
            Node *loop_body = node->body ? node->body : node->then_body;
            if (node->init) {
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->init);
                wasm_buf_append_u8(&fn->body, WASM_OP_DROP);
            }
            wasm_buf_append_u8(&fn->body, WASM_OP_BLOCK);
            wasm_buf_append_u8(&fn->body, WASM_TYPE_VOID);
            wasm_buf_append_u8(&fn->body, WASM_OP_LOOP);
            wasm_buf_append_u8(&fn->body, WASM_TYPE_VOID);

            if (node->cond) {
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->cond);
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_EQZ);
                wasm_buf_append_u8(&fn->body, WASM_OP_BR_IF);
                wasm_buf_append_uleb128(&fn->body, 1);
            }

            if (loop_body) {
                wasm_lower_stmt(cc, mod, fn, lmap, gmap, loop_body, ret_type);
            }

            if (node->inc) {
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->inc);
                wasm_buf_append_u8(&fn->body, WASM_OP_DROP);
            }

            wasm_buf_append_u8(&fn->body, WASM_OP_BR);
            wasm_buf_append_uleb128(&fn->body, 0);
            wasm_buf_append_u8(&fn->body, WASM_OP_END);
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
        global_static_offset = (global_static_offset + 3) & -4;

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
                int32_t val = (int32_t)eval_const_expr_p4(gvar->initializer, &const_ok);
                if (!const_ok) val = (int32_t)gvar->initializer->int_val;
                wasm_module_add_data_segment(&mod, global_static_offset, (const uint8_t *)&val, sz <= 4 ? (size_t)sz : 4);
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
                global_static_offset = (global_static_offset + 3) & -4;

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
                        int32_t val = (int32_t)eval_const_expr_p4(curr->initializer, &const_ok);
                        if (!const_ok) val = (int32_t)curr->initializer->int_val;
                        wasm_module_add_data_segment(&mod, global_static_offset, (const uint8_t *)&val, sz <= 4 ? (size_t)sz : 4);
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
            lmap.tmp_f32_idx = -1;
            lmap.tmp_f64_idx = -1;

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
                    frame_size = (frame_size + 3) & -4;
                    lmap.entries[i].stack_offset = frame_size;
                    frame_size += lmap.entries[i].size > 0 ? lmap.entries[i].size : 4;
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
