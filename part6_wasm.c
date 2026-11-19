/* ================================================================ */
/* PART 6 (WASM): NATIVE WEBASSEMBLY STACK MACHINE CODE GENERATOR    */
/* ================================================================ */

#include "src/wasm_emit.h"

typedef struct {
    char name[256];
    uint32_t offset;
    int size;
    Type *type; Node *init;
} WasmGlobalVar;

typedef struct {
    WasmGlobalVar *vars;
    int count;
    int cap;
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
static void wasm_emit_subword_trunc(WASMBuffer *buf, Type *t) {
    if (!t || is_pointer(t) || t->kind == TY_ARRAY || (t->size != 1 && t->size != 2)) return;
    if (is_unsigned_type(t)) { wasm_buf_append_u8(buf, WASM_OP_I32_CONST); wasm_buf_append_sleb128(buf, t->size == 1 ? 255 : 65535); wasm_buf_append_u8(buf, WASM_OP_I32_AND); return; }
    int sh = (t->size == 1) ? 24 : 16;
    wasm_buf_append_u8(buf, WASM_OP_I32_CONST); wasm_buf_append_sleb128(buf, sh); wasm_buf_append_u8(buf, WASM_OP_I32_SHL);
    wasm_buf_append_u8(buf, WASM_OP_I32_CONST); wasm_buf_append_sleb128(buf, sh); wasm_buf_append_u8(buf, WASM_OP_I32_SHR_S);
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

static Node *wasm_cur_prog = NULL;

static const char *wasm_node_var_name(Node *node) {
    if (!node) return "";
    if (node->sym && node->sym->is_global && node->sym->asm_name[0]) {
        return node->sym->asm_name;
    }
    if (node->sym && node->sym->name[0]) {
        return node->sym->name;
    }
    return node->name;
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

static WasmLocalEntry *wasm_local_find_node(WasmLocalMap *map, Node *node) {
    if (!map || !node) return NULL;
    if (node->sym) {
        int i;
        for (i = 0; i < map->count; i++) {
            if (map->entries[i].sym == (void *)node->sym) {
                return &map->entries[i];
            }
        }
        const char *vname = wasm_node_var_name(node);
        if (vname && vname[0]) {
            for (i = 0; i < map->count; i++) {
                if (!map->entries[i].sym && strcmp(map->entries[i].name, vname) == 0) {
                    return &map->entries[i];
                }
            }
        }
        return NULL;
    }
    return wasm_local_find_entry(map, wasm_node_var_name(node));
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

static int wasm_alloc_temp_local(WasmFunction *fn, uint8_t val_type) {
    if (!fn) return -1;
    if (wasm_func_reserve_locals(fn, fn->num_locals + 1U) < 0) return -1;
    uint32_t idx = fn->num_params + fn->num_locals++;
    fn->local_types[fn->num_locals - 1] = val_type ? val_type : WASM_TYPE_I32;
    return (int)idx;
}

static Symbol *wasm_find_scope_symbol(Compiler *cc, const char *name);

static Type *wasm_lookup_func_type(Compiler *cc, Node *prog, const char *fname, int *out_num_params) {
    if (out_num_params) *out_num_params = -1;
    if (!fname || !fname[0]) return NULL;
    Node *c = prog;
    while (c) {
        if (c->kind == ND_FUNC_DEF && strcmp(c->func_def_name, fname) == 0) {
            if (out_num_params) *out_num_params = c->num_params;
            return c->func_type;
        }
        c = c->next;
    }
    Symbol *sym = wasm_find_scope_symbol(cc, fname);
    if (sym && sym->type && sym->type->kind == TY_FUNC) {
        if (out_num_params) *out_num_params = sym->type->num_params;
        return sym->type;
    }
    return NULL;
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

static void wasm_global_map_free(WasmGlobalMap *gmap) {
    if (!gmap) return;
    free(gmap->vars);
    gmap->vars = NULL;
    gmap->count = 0;
    gmap->cap = 0;
}

static int wasm_register_global(WasmGlobalMap *gmap, uint32_t *static_off, Node *gvar) {
    if (!gmap || !static_off || !gvar) return 0;
    const char *gname = (gvar->sym && gvar->sym->asm_name[0]) ? gvar->sym->asm_name
                      : (gvar->name[0] ? gvar->name : (gvar->sym ? gvar->sym->name : NULL));
    if (!gname || !gname[0]) return 0;
    int sz = gvar->type ? type_size(gvar->type) : 0;
    if (gvar->type && gvar->type->kind == TY_ARRAY && gvar->type->base &&
        gvar->initializer && gvar->initializer->kind == ND_INIT_LIST) {
        int esz = type_size(gvar->type->base);
        int list_sz = (esz > 0) ? (esz * gvar->initializer->num_args) : 0;
        if (list_sz > sz) sz = list_sz;
    }
    if (sz <= 0) sz = 4;
    WasmGlobalVar *existing = wasm_global_find(gmap, gname);
    if (existing) {
        if (sz > existing->size) {
            uint32_t galign = (sz >= 8) ? 8U : 4U;
            *static_off = (*static_off + (galign - 1U)) & ~(galign - 1U);
            existing->offset = *static_off;
            existing->size = sz;
            existing->type = gvar->type;
            *static_off += (uint32_t)sz;
        }
        if (!existing->init && gvar->initializer) {
            existing->init = gvar->initializer;
            if (gvar->type && (!existing->type || type_size(gvar->type) >= existing->size)) {
                existing->type = gvar->type;
            }
        }
        if (gvar->sym) gvar->sym->stack_offset = (int)existing->offset;
        return 0;
    }
    if (gmap->count >= gmap->cap) {
        int new_cap = gmap->cap ? gmap->cap * 2 : 64;
        WasmGlobalVar *nv = (WasmGlobalVar *)realloc(gmap->vars, (size_t)new_cap * sizeof(WasmGlobalVar));
        if (!nv) return -1;
        memset(nv + gmap->cap, 0, (size_t)(new_cap - gmap->cap) * sizeof(WasmGlobalVar));
        gmap->vars = nv;
        gmap->cap = new_cap;
    }
    uint32_t galign = (sz >= 8) ? 8U : 4U;
    *static_off = (*static_off + (galign - 1U)) & ~(galign - 1U);
    int g_idx = gmap->count++;
    WasmGlobalVar *gv = &gmap->vars[g_idx];
    memset(gv, 0, sizeof(WasmGlobalVar));
    strncpy(gv->name, gname, sizeof(gv->name) - 1);
    gv->offset = *static_off;
    gv->size = sz;
    gv->type = gvar->type;
    gv->init = gvar->initializer;
    if (gvar->sym) gvar->sym->stack_offset = (int)*static_off;
    *static_off += (uint32_t)sz;
    return 0;
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

static int wasm_bitfield_is_64(const Node *n) {
    if (!n) return 0;
    return (n->member_size > 4 || (n->bit_offset + n->bit_size > 32) ||
            (n->member_size > 0 && n->bit_offset + n->bit_size > n->member_size * 8)) ? 1 : 0;
}

static void wasm_emit_bf_container_op(WASMBuffer *buf, const Node *n, int is_64, int is_store) {
    if (is_64) {
        wasm_buf_append_u8(buf, is_store ? WASM_OP_I64_STORE : WASM_OP_I64_LOAD);
        wasm_emit_memarg_safe(buf, 0, 0);
    } else if (n->member_size == 1) {
        wasm_buf_append_u8(buf, is_store ? WASM_OP_I32_STORE8 : WASM_OP_I32_LOAD8_U);
        wasm_emit_memarg_safe(buf, 0, 0);
    } else if (n->member_size == 2) {
        wasm_buf_append_u8(buf, is_store ? WASM_OP_I32_STORE16 : WASM_OP_I32_LOAD16_U);
        wasm_emit_memarg_safe(buf, 0, 0);
    } else {
        wasm_buf_append_u8(buf, is_store ? WASM_OP_I32_STORE : WASM_OP_I32_LOAD);
        wasm_emit_memarg_safe(buf, 0, 0);
    }
}

static void wasm_emit_bitfield_extract(WASMBuffer *buf, Node *node, int is_64, uint8_t dst_vt) {
    int uns = 1;
    if (node->type) {
        uns = is_unsigned_type(node->type) || node->type->kind == TY_ENUM;
    }
    if (is_64) {
        int ls = 64 - node->bit_offset - node->bit_size;
        int rs = 64 - node->bit_size;
        if (ls > 0 && ls < 64) { wasm_buf_append_u8(buf, WASM_OP_I64_CONST); wasm_buf_append_sleb128_i64(buf, ls); wasm_buf_append_u8(buf, WASM_OP_I64_SHL); }
        if (rs > 0 && rs < 64) { wasm_buf_append_u8(buf, WASM_OP_I64_CONST); wasm_buf_append_sleb128_i64(buf, rs); wasm_buf_append_u8(buf, uns ? WASM_OP_I64_SHR_U : WASM_OP_I64_SHR_S); }
        if (dst_vt == WASM_TYPE_I32) wasm_buf_append_u8(buf, WASM_OP_I32_WRAP_I64);
    } else {
        int ls = 32 - node->bit_offset - node->bit_size;
        int rs = 32 - node->bit_size;
        if (ls > 0 && ls < 32) { wasm_buf_append_u8(buf, WASM_OP_I32_CONST); wasm_buf_append_sleb128(buf, ls); wasm_buf_append_u8(buf, WASM_OP_I32_SHL); }
        if (rs > 0 && rs < 32) { wasm_buf_append_u8(buf, WASM_OP_I32_CONST); wasm_buf_append_sleb128(buf, rs); wasm_buf_append_u8(buf, uns ? WASM_OP_I32_SHR_U : WASM_OP_I32_SHR_S); }
        if (dst_vt == WASM_TYPE_I64) wasm_buf_append_u8(buf, uns ? WASM_OP_I64_EXTEND_I32_U : WASM_OP_I64_EXTEND_I32_S);
    }
}

static void wasm_emit_bitfield_load(WASMBuffer *buf, Node *node) {
    int is_64 = wasm_bitfield_is_64(node);
    wasm_emit_bf_container_op(buf, node, is_64, 0);
    wasm_emit_bitfield_extract(buf, node, is_64, wasm_node_valtype(node));
}

static void wasm_emit_bitfield_rmw_store(WASMBuffer *buf, WasmFunction *fn, Node *lhs, uint8_t val_vt) {
    int is_64 = wasm_bitfield_is_64(lhs);
    uint8_t cvt = is_64 ? WASM_TYPE_I64 : WASM_TYPE_I32;
    int a_loc = wasm_alloc_temp_local(fn, WASM_TYPE_I32);
    int v_loc = wasm_alloc_temp_local(fn, cvt);
    if (val_vt != cvt) {
        wasm_emit_numeric_conversion(buf, val_vt, cvt, 1);
    }
    wasm_buf_append_u8(buf, WASM_OP_LOCAL_SET); wasm_buf_append_uleb128(buf, (uint32_t)v_loc);
    wasm_buf_append_u8(buf, WASM_OP_LOCAL_SET); wasm_buf_append_uleb128(buf, (uint32_t)a_loc);

    wasm_buf_append_u8(buf, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(buf, (uint32_t)a_loc);
    wasm_buf_append_u8(buf, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(buf, (uint32_t)a_loc);
    wasm_emit_bf_container_op(buf, lhs, is_64, 0);
    if (is_64) {
        uint64_t mask = (lhs->bit_size >= 64) ? ~0ULL : ((1ULL << lhs->bit_size) - 1ULL);
        uint64_t clr = ~(mask << lhs->bit_offset);
        wasm_buf_append_u8(buf, WASM_OP_I64_CONST); wasm_buf_append_sleb128_i64(buf, (int64_t)clr);
        wasm_buf_append_u8(buf, WASM_OP_I64_AND);
        wasm_buf_append_u8(buf, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(buf, (uint32_t)v_loc);
        wasm_buf_append_u8(buf, WASM_OP_I64_CONST); wasm_buf_append_sleb128_i64(buf, (int64_t)mask);
        wasm_buf_append_u8(buf, WASM_OP_I64_AND);
        if (lhs->bit_offset > 0) {
            wasm_buf_append_u8(buf, WASM_OP_I64_CONST); wasm_buf_append_sleb128_i64(buf, lhs->bit_offset);
            wasm_buf_append_u8(buf, WASM_OP_I64_SHL);
        }
        wasm_buf_append_u8(buf, WASM_OP_I64_OR);
    } else {
        uint32_t mask = (lhs->bit_size >= 32) ? 0xFFFFFFFFU : ((1U << lhs->bit_size) - 1U);
        uint32_t clr = ~(mask << lhs->bit_offset);
        wasm_buf_append_u8(buf, WASM_OP_I32_CONST); wasm_buf_append_sleb128(buf, (int32_t)clr);
        wasm_buf_append_u8(buf, WASM_OP_I32_AND);
        wasm_buf_append_u8(buf, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(buf, (uint32_t)v_loc);
        wasm_buf_append_u8(buf, WASM_OP_I32_CONST); wasm_buf_append_sleb128(buf, (int32_t)mask);
        wasm_buf_append_u8(buf, WASM_OP_I32_AND);
        if (lhs->bit_offset > 0) {
            wasm_buf_append_u8(buf, WASM_OP_I32_CONST); wasm_buf_append_sleb128(buf, lhs->bit_offset);
            wasm_buf_append_u8(buf, WASM_OP_I32_SHL);
        }
        wasm_buf_append_u8(buf, WASM_OP_I32_OR);
    }
    wasm_buf_append_u8(buf, WASM_OP_LOCAL_TEE); wasm_buf_append_uleb128(buf, (uint32_t)v_loc);
    wasm_emit_bf_container_op(buf, lhs, is_64, 1);
    wasm_buf_append_u8(buf, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(buf, (uint32_t)v_loc);
    wasm_emit_bitfield_extract(buf, lhs, is_64, val_vt);
}

static int wasm_is_aggregate_type(const Type *t) {
    return t && (t->kind == TY_STRUCT || t->kind == TY_UNION);
}

static Type *wasm_call_ret_type(Compiler *cc, const Node *call_node) {
    if (!call_node) return NULL;
    if (call_node->type) return call_node->type;
    if (call_node->func_name[0] && wasm_cur_prog) {
        Node *fd = wasm_cur_prog;
        while (fd) {
            if (fd->kind == ND_FUNC_DEF && strcmp(fd->func_def_name, call_node->func_name) == 0) {
                if (fd->func_type) return fd->func_type->ret;
                break;
            }
            fd = fd->next;
        }
    }
    if (call_node->func_name[0] && cc) {
        Type *ft = wasm_lookup_func_type(cc, wasm_cur_prog, call_node->func_name, NULL);
        if (ft) return ft->ret;
    }
    return NULL;
}

static Type *wasm_indirect_func_type(Compiler *cc, WasmLocalMap *lmap, WasmGlobalMap *gmap, const Node *node) {
    Type *fpt = NULL;
    if (!node) return NULL;
    if (node->lhs && node->lhs->type) {
        fpt = node->lhs->type;
    } else if (node->lhs && node->lhs->lhs && node->lhs->lhs->type) {
        fpt = node->lhs->lhs->type;
    } else if (node->func_name[0]) {
        WasmLocalEntry *ve = wasm_local_find_entry(lmap, node->func_name);
        if (ve && ve->type) fpt = (Type *)ve->type;
        else {
            WasmGlobalVar *gv = wasm_global_find(gmap, node->func_name);
            if (gv && gv->type) fpt = gv->type;
            else if (cc) {
                Symbol *sym = wasm_find_scope_symbol(cc, node->func_name);
                if (sym && sym->type) fpt = sym->type;
            }
        }
    }
    while (fpt && fpt->kind == TY_PTR) fpt = fpt->base;
    return (fpt && fpt->kind == TY_FUNC) ? fpt : NULL;
}

static void wasm_emit_memcpy_locals(WASMBuffer *buf, WasmFunction *fn, uint32_t dst_loc, uint32_t src_loc, int sz) {
    if (!buf || !fn || sz <= 0) return;
    if (sz <= 64) {
        int off = 0;
        while (off + 8 <= sz) {
            wasm_buf_append_u8(buf, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(buf, dst_loc);
            wasm_buf_append_u8(buf, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(buf, src_loc);
            wasm_buf_append_u8(buf, WASM_OP_I64_LOAD); wasm_emit_memarg_safe(buf, 0, (uint32_t)off);
            wasm_buf_append_u8(buf, WASM_OP_I64_STORE); wasm_emit_memarg_safe(buf, 0, (uint32_t)off);
            off += 8;
        }
        if (off + 4 <= sz) {
            wasm_buf_append_u8(buf, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(buf, dst_loc);
            wasm_buf_append_u8(buf, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(buf, src_loc);
            wasm_buf_append_u8(buf, WASM_OP_I32_LOAD); wasm_emit_memarg_safe(buf, 0, (uint32_t)off);
            wasm_buf_append_u8(buf, WASM_OP_I32_STORE); wasm_emit_memarg_safe(buf, 0, (uint32_t)off);
            off += 4;
        }
        if (off + 2 <= sz) {
            wasm_buf_append_u8(buf, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(buf, dst_loc);
            wasm_buf_append_u8(buf, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(buf, src_loc);
            wasm_buf_append_u8(buf, WASM_OP_I32_LOAD16_U); wasm_emit_memarg_safe(buf, 0, (uint32_t)off);
            wasm_buf_append_u8(buf, WASM_OP_I32_STORE16); wasm_emit_memarg_safe(buf, 0, (uint32_t)off);
            off += 2;
        }
        if (off + 1 <= sz) {
            wasm_buf_append_u8(buf, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(buf, dst_loc);
            wasm_buf_append_u8(buf, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(buf, src_loc);
            wasm_buf_append_u8(buf, WASM_OP_I32_LOAD8_U); wasm_emit_memarg_safe(buf, 0, (uint32_t)off);
            wasm_buf_append_u8(buf, WASM_OP_I32_STORE8); wasm_emit_memarg_safe(buf, 0, (uint32_t)off);
        }
        return;
    }
    int idx_loc = wasm_alloc_temp_local(fn, WASM_TYPE_I32);
    int chunks8 = sz & ~7;
    if (idx_loc >= 0 && chunks8 > 0) {
        wasm_buf_append_u8(buf, WASM_OP_I32_CONST); wasm_buf_append_sleb128(buf, 0);
        wasm_buf_append_u8(buf, WASM_OP_LOCAL_SET); wasm_buf_append_uleb128(buf, (uint32_t)idx_loc);
        wasm_buf_append_u8(buf, WASM_OP_BLOCK); wasm_buf_append_u8(buf, WASM_TYPE_VOID);
        wasm_buf_append_u8(buf, WASM_OP_LOOP); wasm_buf_append_u8(buf, WASM_TYPE_VOID);
            wasm_buf_append_u8(buf, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(buf, (uint32_t)idx_loc);
            wasm_buf_append_u8(buf, WASM_OP_I32_CONST); wasm_buf_append_sleb128(buf, chunks8);
            wasm_buf_append_u8(buf, WASM_OP_I32_GE_U);
            wasm_buf_append_u8(buf, WASM_OP_BR_IF); wasm_buf_append_uleb128(buf, 1);

            wasm_buf_append_u8(buf, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(buf, dst_loc);
            wasm_buf_append_u8(buf, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(buf, (uint32_t)idx_loc);
            wasm_buf_append_u8(buf, WASM_OP_I32_ADD);
            wasm_buf_append_u8(buf, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(buf, src_loc);
            wasm_buf_append_u8(buf, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(buf, (uint32_t)idx_loc);
            wasm_buf_append_u8(buf, WASM_OP_I32_ADD);
            wasm_buf_append_u8(buf, WASM_OP_I64_LOAD); wasm_emit_memarg_safe(buf, 0, 0);
            wasm_buf_append_u8(buf, WASM_OP_I64_STORE); wasm_emit_memarg_safe(buf, 0, 0);

            wasm_buf_append_u8(buf, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(buf, (uint32_t)idx_loc);
            wasm_buf_append_u8(buf, WASM_OP_I32_CONST); wasm_buf_append_sleb128(buf, 8);
            wasm_buf_append_u8(buf, WASM_OP_I32_ADD);
            wasm_buf_append_u8(buf, WASM_OP_LOCAL_SET); wasm_buf_append_uleb128(buf, (uint32_t)idx_loc);
            wasm_buf_append_u8(buf, WASM_OP_BR); wasm_buf_append_uleb128(buf, 0);
        wasm_buf_append_u8(buf, WASM_OP_END);
        wasm_buf_append_u8(buf, WASM_OP_END);
    }
    for (int rem = (idx_loc >= 0) ? chunks8 : 0; rem < sz; rem++) {
        wasm_buf_append_u8(buf, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(buf, dst_loc);
        wasm_buf_append_u8(buf, WASM_OP_LOCAL_GET); wasm_buf_append_uleb128(buf, src_loc);
        wasm_buf_append_u8(buf, WASM_OP_I32_LOAD8_U); wasm_emit_memarg_safe(buf, 0, (uint32_t)rem);
        wasm_buf_append_u8(buf, WASM_OP_I32_STORE8); wasm_emit_memarg_safe(buf, 0, (uint32_t)rem);
    }
}

static void wasm_emit_sret_push(WasmFunction *fn, WasmLocalMap *lmap, Node *call_node) {
    if (lmap && call_node) {
        int si;
        for (si = 0; si < lmap->count; si++) {
            if (lmap->entries[si].sym == (void *)call_node && lmap->entries[si].is_memory) {
                if (lmap->fp_local_idx >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->fp_local_idx);
                } else {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 0);
                }
                if (lmap->entries[si].stack_offset > 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, lmap->entries[si].stack_offset);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD);
                }
                return;
            }
        }
    }
    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
    wasm_buf_append_sleb128(&fn->body, 0);
}

static int wasm_node_has_side_effects(Node *n) {
    if (!n) return 0;
    if (n->kind == ND_ASSIGN || n->kind == ND_COMPOUND_ASSIGN || n->kind == ND_CALL ||
        n->kind == ND_PRE_INC || n->kind == ND_PRE_DEC ||
        n->kind == ND_POST_INC || n->kind == ND_POST_DEC) return 1;
    if (n->lhs && wasm_node_has_side_effects(n->lhs)) return 1;
    if (n->rhs && wasm_node_has_side_effects(n->rhs)) return 1;
    if (n->cond && wasm_node_has_side_effects(n->cond)) return 1;
    if (n->then_body && wasm_node_has_side_effects(n->then_body)) return 1;
    if (n->else_body && wasm_node_has_side_effects(n->else_body)) return 1;
    if (n->args) {
        int a;
        for (a = 0; a < n->num_args; a++) {
            if (wasm_node_has_side_effects(n->args[a])) return 1;
        }
    }
    return 0;
}

static int wasm_expr_has_value(const WasmModule *mod, Node *n) {
    if (!n) return 0;
    if (n->kind == ND_BLOCK || n->kind == ND_NOP || n->kind == ND_IF ||
        n->kind == ND_WHILE || n->kind == ND_DO_WHILE || n->kind == ND_FOR ||
        n->kind == ND_RETURN || n->kind == ND_BREAK || n->kind == ND_CONTINUE ||
        n->kind == ND_SWITCH || n->kind == ND_GOTO || n->kind == ND_LABEL ||
        n->kind == ND_FUNC_DEF) {
        return 0;
    }
    if (n->kind == ND_CAST) {
        Type *dt = n->type ? n->type : n->cast_type;
        if (dt && dt->kind == TY_VOID) return 0;
        return 1;
    }
    if (n->kind == ND_COMMA_EXPR) {
        return wasm_expr_has_value(mod, n->rhs);
    }
    if (n->kind == ND_TERNARY) {
        if (n->type && n->type->kind == TY_VOID) return 0;
        if (!wasm_expr_has_value(mod, n->then_body) && !wasm_expr_has_value(mod, n->else_body)) return 0;
        return 1;
    }
    if (n->kind == ND_CALL) {
        if (strncmp(n->func_name, "__zcc_wasm_", 11) == 0 ||
            strncmp(n->func_name, "__builtin_", 10) == 0) {
            return 1;
        }
        int callee_idx = n->func_name[0] ? wasm_find_func_idx(mod, n->func_name) : -1;
        if (callee_idx >= 0) {
            int tidx = -1;
            if (callee_idx < mod->num_imports) tidx = (int)mod->imports[callee_idx].type_idx;
            else if (callee_idx - mod->num_imports < mod->num_funcs) tidx = mod->funcs[callee_idx - mod->num_imports].type_idx;
            if (tidx >= 0 && tidx < mod->num_types) {
                return mod->types[tidx].num_results > 0 ? 1 : 0;
            }
        }
        return (n->type && n->type->kind == TY_VOID) ? 0 : 1;
    }
    return 1;
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
            const char *vname = wasm_node_var_name(node);
            WasmLocalEntry *entry = wasm_local_find_node(lmap, node);
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
                WasmGlobalVar *gv = wasm_global_find(gmap, vname);
                if (gv) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, (int32_t)gv->offset);
                } else {
                    int f_idx = !(node->sym && node->sym->is_local) ? wasm_find_func_idx(mod, vname) : -1;
                    if (f_idx >= 0) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                        wasm_buf_append_sleb128(&fn->body, f_idx);
                    } else if (node->sym && node->sym->stack_offset != 0) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                        wasm_buf_append_sleb128(&fn->body, (int32_t)node->sym->stack_offset);
                    } else {
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                        wasm_buf_append_sleb128(&fn->body, 1024);
                    }
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
            if (node->lhs && node->lhs->kind == ND_VAR &&
                !wasm_local_find_node(lmap, node->lhs) &&
                !(node->lhs->sym && node->lhs->sym->is_local)) {
                const char *fn_name = wasm_node_var_name(node->lhs);
                if (!wasm_global_find(gmap, fn_name)) {
                    int f_idx = wasm_find_func_idx(mod, fn_name);
                    if (f_idx >= 0) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                        wasm_buf_append_sleb128(&fn->body, f_idx);
                        break;
                    }
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
            const char *vname = wasm_node_var_name(node);
            WasmLocalEntry *entry = wasm_local_find_node(lmap, node);

            /* Arrays and Structs decay to their base address in expression contexts */
            if (node->type && (node->type->kind == TY_ARRAY || node->type->kind == TY_STRUCT || node->type->kind == TY_UNION)) {
                wasm_lower_addr(cc, mod, fn, lmap, gmap, node);
                break;
            }

            if (entry && entry->is_memory) {
                wasm_lower_addr(cc, mod, fn, lmap, gmap, node);
                wasm_emit_typed_load(&fn->body, node->type ? node->type : (Type *)entry->type, 0);
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
                    int f_idx = !(node->sym && node->sym->is_local) ? wasm_find_func_idx(mod, vname) : -1;
                    if (f_idx >= 0) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                        wasm_buf_append_sleb128(&fn->body, f_idx);
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
            /* *p: if array/struct/func, yield pointer/table-idx; else load typed value */
            if (node->type && (node->type->kind == TY_ARRAY || node->type->kind == TY_STRUCT || node->type->kind == TY_UNION || node->type->kind == TY_FUNC)) {
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
            } else if (node->is_bitfield && node->bit_size > 0) {
                wasm_lower_addr(cc, mod, fn, lmap, gmap, node);
                wasm_emit_bitfield_load(&fn->body, node);
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

            if (lhs_type && (lhs_type->kind == TY_STRUCT || lhs_type->kind == TY_UNION)) {
                int sz = type_size(lhs_type);
                int dst_loc = wasm_alloc_temp_local(fn, WASM_TYPE_I32);
                int src_loc = wasm_alloc_temp_local(fn, WASM_TYPE_I32);
                wasm_lower_addr(cc, mod, fn, lmap, gmap, node->lhs);
                if (dst_loc >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)dst_loc);
                }
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
                if (src_loc >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)src_loc);
                }
                if (dst_loc >= 0 && src_loc >= 0 && sz > 0) {
                    wasm_emit_memcpy_locals(&fn->body, fn, (uint32_t)dst_loc, (uint32_t)src_loc, sz);
                }
                if (dst_loc >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)dst_loc);
                } else {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 0);
                }
                break;
            }

            if (node->lhs && node->lhs->kind == ND_VAR) {
                WasmLocalEntry *entry = wasm_local_find_node(lmap, node->lhs);
                if (entry && !entry->is_memory) {
                    /* Direct local register assignment with numeric conversion */
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
                    wasm_emit_type_conversion(&fn->body, rhs_vt, rhs_type, entry->val_type, lhs_type); wasm_emit_subword_trunc(&fn->body, lhs_type);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_TEE);
                    wasm_buf_append_uleb128(&fn->body, entry->local_idx);
                    break;
                }
            }

            /* Memory Lvalue Store: [addr, val] */
            wasm_lower_addr(cc, mod, fn, lmap, gmap, node->lhs);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_type_conversion(&fn->body, rhs_vt, rhs_type, lhs_vt, lhs_type);
            if (node->lhs && node->lhs->is_bitfield && node->lhs->bit_size > 0) {
                wasm_emit_bitfield_rmw_store(&fn->body, fn, node->lhs, lhs_vt);
                break;
            }
            wasm_emit_subword_trunc(&fn->body, lhs_type);

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
            int ptr_scale = (lhs_type && (is_pointer(lhs_type) || lhs_type->kind == TY_ARRAY) &&
                             (node->compound_op == ND_ADD || node->compound_op == ND_SUB))
                            ? wasm_ptr_elem_size(lhs_type) : 1;

            if (node->lhs && node->lhs->kind == ND_VAR) {
                WasmLocalEntry *entry = wasm_local_find_node(lmap, node->lhs);
                if (entry && !entry->is_memory) {
                    /* Register compound assignment */
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, entry->local_idx);
                    wasm_emit_type_conversion(&fn->body, entry->val_type, lhs_type, common_vt, lhs_type);
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
                    wasm_emit_type_conversion(&fn->body, rhs_vt, rhs_type, common_vt, lhs_type);
                    if (ptr_scale > 1) {
                        if (common_vt == WASM_TYPE_I64) {
                            wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                            wasm_buf_append_sleb128_i64(&fn->body, (int64_t)ptr_scale);
                            wasm_buf_append_u8(&fn->body, WASM_OP_I64_MUL);
                        } else {
                            wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                            wasm_buf_append_sleb128(&fn->body, ptr_scale);
                            wasm_buf_append_u8(&fn->body, WASM_OP_I32_MUL);
                        }
                    }

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
                    wasm_emit_type_conversion(&fn->body, common_vt, lhs_type, entry->val_type, lhs_type); wasm_emit_subword_trunc(&fn->body, lhs_type);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_TEE);
                    wasm_buf_append_uleb128(&fn->body, entry->local_idx);
                    break;
                }
            }

            /* Memory Lvalue Compound Assignment (side-effect-free, address evaluated once) */
            int is_bf_cmp = (node->lhs && node->lhs->is_bitfield && node->lhs->bit_size > 0);
            int cmp_addr_loc = is_bf_cmp ? wasm_alloc_temp_local(fn, WASM_TYPE_I32) : lmap->tmp_addr_idx;
            wasm_lower_addr(cc, mod, fn, lmap, gmap, node->lhs);
            if (cmp_addr_loc >= 0) {
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)cmp_addr_loc);
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)cmp_addr_loc);
            }
            if (is_bf_cmp) {
                wasm_emit_bitfield_load(&fn->body, node->lhs);
            } else {
                wasm_emit_typed_load(&fn->body, lhs_type, 0);
            }
            wasm_emit_type_conversion(&fn->body, lhs_vt, lhs_type, common_vt, lhs_type);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            wasm_emit_type_conversion(&fn->body, rhs_vt, rhs_type, common_vt, lhs_type);
            if (ptr_scale > 1) {
                if (common_vt == WASM_TYPE_I64) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                    wasm_buf_append_sleb128_i64(&fn->body, (int64_t)ptr_scale);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_MUL);
                } else {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, ptr_scale);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_MUL);
                }
            }

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
            if (is_bf_cmp) {
                int cmp_v_loc = wasm_alloc_temp_local(fn, lhs_vt);
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)cmp_v_loc);
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)cmp_addr_loc);
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)cmp_v_loc);
                wasm_emit_bitfield_rmw_store(&fn->body, fn, node->lhs, lhs_vt);
                break;
            }
            wasm_emit_subword_trunc(&fn->body, lhs_type);

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
                WasmLocalEntry *entry = wasm_local_find_node(lmap, node->lhs);
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
                        wasm_buf_append_u8(&fn->body, is_inc ? WASM_OP_I32_ADD : WASM_OP_I32_SUB); wasm_emit_subword_trunc(&fn->body, ltype);
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
            int is_bf_inc = (node->lhs && node->lhs->is_bitfield && node->lhs->bit_size > 0);
            int bf_inc_addr = is_bf_inc ? wasm_alloc_temp_local(fn, WASM_TYPE_I32) : lmap->tmp_addr_idx;
            int tmp_idx = (vt == WASM_TYPE_I64) ? lmap->tmp_i64_idx
                        : (vt == WASM_TYPE_F32) ? lmap->tmp_f32_idx
                        : (vt == WASM_TYPE_F64) ? lmap->tmp_f64_idx
                        : lmap->tmp_val_idx;
            wasm_lower_addr(cc, mod, fn, lmap, gmap, node->lhs);
            if (bf_inc_addr >= 0) {
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)bf_inc_addr);
                if (!is_bf_inc) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)bf_inc_addr);
                }
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)bf_inc_addr);
            }
            if (is_bf_inc) {
                wasm_emit_bitfield_load(&fn->body, node->lhs);
            } else {
                wasm_emit_typed_load(&fn->body, ltype, 0);
            }
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
                wasm_buf_append_u8(&fn->body, is_inc ? WASM_OP_I32_ADD : WASM_OP_I32_SUB); wasm_emit_subword_trunc(&fn->body, ltype);
            }
            if (is_bf_inc) {
                int bf_nv = wasm_alloc_temp_local(fn, vt);
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)bf_nv);
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)bf_inc_addr);
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)bf_nv);
                wasm_emit_bitfield_rmw_store(&fn->body, fn, node->lhs, vt);
                if (is_post && tmp_idx >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_DROP);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)tmp_idx);
                }
            } else if (!is_post && tmp_idx >= 0) {
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
                if (wasm_expr_has_value(mod, node->lhs)) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_DROP);
                }
            }
            if (node->rhs) {
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->rhs);
            }
            break;

        case ND_CAST: {
            Type *src_type = node->lhs ? node->lhs->type : NULL;
            Type *dst_type = node->type ? node->type : node->cast_type;
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            if (dst_type && dst_type->kind == TY_VOID) {
                if (wasm_expr_has_value(mod, node->lhs)) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_DROP);
                }
                break;
            }
            uint8_t src_vt = wasm_node_valtype(node->lhs);
            uint8_t dst_vt = wasm_valtype_from_ctype(dst_type);
            wasm_emit_type_conversion(&fn->body, src_vt, src_type, dst_vt, dst_type); wasm_emit_subword_trunc(&fn->body, dst_type);
            break;
        }

        case ND_VA_ARG: {
            /* ap is __va_list_struct* (overflow_arg_area stored at byte offset 8) */
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
            if (lmap->tmp_addr_idx >= 0 && lmap->tmp_val_idx >= 0) {
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_TEE);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_addr_idx);
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_LOAD);
                wasm_emit_memarg_safe(&fn->body, 2, 8);
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);

                /* Advance overflow_arg_area by 8 bytes */
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_addr_idx);
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                wasm_buf_append_sleb128(&fn->body, 8);
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD);
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_STORE);
                wasm_emit_memarg_safe(&fn->body, 2, 8);

                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                if (node->type && node->type->kind == TY_FLOAT) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_F64_LOAD);
                    wasm_emit_memarg_safe(&fn->body, 3, 0);
                    wasm_buf_append_u8(&fn->body, WASM_OP_F32_DEMOTE_F64);
                } else if (node->type && (node->type->kind == TY_STRUCT || node->type->kind == TY_UNION)) {
                    /* Struct pointer left on stack */
                } else {
                    wasm_emit_typed_load(&fn->body, node->type, 0);
                    wasm_emit_subword_trunc(&fn->body, node->type);
                }
            } else {
                wasm_buf_append_u8(&fn->body, WASM_OP_DROP);
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                wasm_buf_append_sleb128(&fn->body, 0);
            }
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
            if (strcmp(node->func_name, "__builtin_va_start") == 0) {
                if (node->num_args >= 1 && node->args && node->args[0]) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    if (lmap->va_ptr_local_idx >= 0) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                        wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->va_ptr_local_idx);
                    } else {
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                        wasm_buf_append_sleb128(&fn->body, 0);
                    }
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_STORE);
                    wasm_emit_memarg_safe(&fn->body, 2, 8);
                }
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                wasm_buf_append_sleb128(&fn->body, 0);
                break;
            }
            if (strcmp(node->func_name, "__builtin_va_copy") == 0) {
                if (node->num_args >= 2 && node->args && node->args[0] && node->args[1]) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[0]);
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[1]);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_LOAD);
                    wasm_emit_memarg_safe(&fn->body, 2, 8);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_STORE);
                    wasm_emit_memarg_safe(&fn->body, 2, 8);
                }
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                wasm_buf_append_sleb128(&fn->body, 0);
                break;
            }
            if (strcmp(node->func_name, "__builtin_va_end") == 0) {
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                wasm_buf_append_sleb128(&fn->body, 0);
                break;
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
            int cfixed = -1;
            Type *callee_ft = (callee_idx >= 0) ? wasm_lookup_func_type(cc, wasm_cur_prog, node->func_name, &cfixed) : NULL;
            if (callee_idx >= 0 && callee_ft && callee_ft->is_variadic) {
                int tidx = -1;
                if (callee_idx < mod->num_imports) {
                    tidx = (int)mod->imports[callee_idx].type_idx;
                } else if (callee_idx - mod->num_imports < mod->num_funcs) {
                    tidx = mod->funcs[callee_idx - mod->num_imports].type_idx;
                }
                int fixed = (cfixed >= 0) ? cfixed : callee_ft->num_params;
                int extra = node->num_args - fixed;
                int alloc_sz = 0;
                int va_sp_loc = -1;
                if (extra > 0) {
                    alloc_sz = ((extra * 8) + 15) & ~15;
                    va_sp_loc = wasm_alloc_temp_local(fn, WASM_TYPE_I32);
                    if (va_sp_loc >= 0) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_GLOBAL_GET);
                        wasm_buf_append_uleb128(&fn->body, 0);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                        wasm_buf_append_sleb128(&fn->body, alloc_sz);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_SUB);
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_TEE);
                        wasm_buf_append_uleb128(&fn->body, (uint32_t)va_sp_loc);
                        wasm_buf_append_u8(&fn->body, WASM_OP_GLOBAL_SET);
                        wasm_buf_append_uleb128(&fn->body, 0);
                        for (int vk = 0; vk < extra; vk++) {
                            Node *va_arg_node = node->args[fixed + vk];
                            uint8_t avt = wasm_node_valtype(va_arg_node);
                            uint32_t slot_off = (uint32_t)(vk * 8);
                            wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                            wasm_buf_append_uleb128(&fn->body, (uint32_t)va_sp_loc);
                            wasm_lower_expr(cc, mod, fn, lmap, gmap, va_arg_node);
                            if (avt == WASM_TYPE_F32) {
                                wasm_buf_append_u8(&fn->body, WASM_OP_F64_PROMOTE_F32);
                                wasm_buf_append_u8(&fn->body, WASM_OP_F64_STORE);
                                wasm_emit_memarg_safe(&fn->body, 3, slot_off);
                            } else if (avt == WASM_TYPE_F64) {
                                wasm_buf_append_u8(&fn->body, WASM_OP_F64_STORE);
                                wasm_emit_memarg_safe(&fn->body, 3, slot_off);
                            } else if (avt == WASM_TYPE_I64) {
                                wasm_buf_append_u8(&fn->body, WASM_OP_I64_STORE);
                                wasm_emit_memarg_safe(&fn->body, 3, slot_off);
                            } else {
                                int is_uns = wasm_type_is_unsigned_or_ptr(va_arg_node ? va_arg_node->type : NULL);
                                wasm_buf_append_u8(&fn->body, is_uns ? WASM_OP_I64_EXTEND_I32_U : WASM_OP_I64_EXTEND_I32_S);
                                wasm_buf_append_u8(&fn->body, WASM_OP_I64_STORE);
                                wasm_emit_memarg_safe(&fn->body, 3, slot_off);
                            }
                        }
                    }
                }
                int a;
                for (a = 0; a < fixed; a++) {
                    uint8_t param_vt = (tidx >= 0 && a < mod->types[tidx].num_params) ? mod->types[tidx].params[a] : WASM_TYPE_I32;
                    if (a < node->num_args && node->args[a]) {
                        uint8_t arg_vt = wasm_node_valtype(node->args[a]);
                        wasm_lower_expr(cc, mod, fn, lmap, gmap, node->args[a]);
                        wasm_emit_type_conversion(&fn->body, arg_vt, node->args[a]->type, param_vt, node->args[a]->type);
                    } else {
                        if (param_vt == WASM_TYPE_I64) { wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST); wasm_buf_append_sleb128_i64(&fn->body, 0); }
                        else if (param_vt == WASM_TYPE_F32) { wasm_buf_append_u8(&fn->body, WASM_OP_F32_CONST); wasm_buf_append_f32(&fn->body, 0.0f); }
                        else if (param_vt == WASM_TYPE_F64) { wasm_buf_append_u8(&fn->body, WASM_OP_F64_CONST); wasm_buf_append_f64(&fn->body, 0.0); }
                        else { wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST); wasm_buf_append_sleb128(&fn->body, 0); }
                    }
                }
                if (extra > 0 && va_sp_loc >= 0) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)va_sp_loc);
                } else {
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, 0);
                }
                if (wasm_is_aggregate_type(wasm_call_ret_type(cc, node))) {
                    wasm_emit_sret_push(fn, lmap, node);
                }
                wasm_buf_append_u8(&fn->body, WASM_OP_CALL);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)callee_idx);

                uint8_t ret_vt = (tidx >= 0 && mod->types[tidx].num_results > 0) ? mod->types[tidx].results[0] : WASM_TYPE_VOID;
                if (extra > 0 && va_sp_loc >= 0) {
                    if (ret_vt != WASM_TYPE_VOID) {
                        int save_loc = (ret_vt == WASM_TYPE_I64) ? lmap->tmp_i64_idx
                                     : (ret_vt == WASM_TYPE_F32) ? lmap->tmp_f32_idx
                                     : (ret_vt == WASM_TYPE_F64) ? lmap->tmp_f64_idx
                                     : lmap->tmp_val_idx;
                        if (save_loc < 0) save_loc = wasm_alloc_temp_local(fn, ret_vt);
                        if (save_loc >= 0) {
                            wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                            wasm_buf_append_uleb128(&fn->body, (uint32_t)save_loc);
                            wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                            wasm_buf_append_uleb128(&fn->body, (uint32_t)va_sp_loc);
                            wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                            wasm_buf_append_sleb128(&fn->body, alloc_sz);
                            wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD);
                            wasm_buf_append_u8(&fn->body, WASM_OP_GLOBAL_SET);
                            wasm_buf_append_uleb128(&fn->body, 0);
                            wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                            wasm_buf_append_uleb128(&fn->body, (uint32_t)save_loc);
                        }
                    } else {
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                        wasm_buf_append_uleb128(&fn->body, (uint32_t)va_sp_loc);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                        wasm_buf_append_sleb128(&fn->body, alloc_sz);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD);
                        wasm_buf_append_u8(&fn->body, WASM_OP_GLOBAL_SET);
                        wasm_buf_append_uleb128(&fn->body, 0);
                    }
                }
                if (ret_vt != WASM_TYPE_VOID) {
                    uint8_t expr_vt = wasm_node_valtype(node);
                    wasm_emit_type_conversion(&fn->body, ret_vt, NULL, expr_vt, node->type);
                }
                break;
            }
            Type *ind_ft = (callee_idx < 0) ? wasm_indirect_func_type(cc, lmap, gmap, node) : NULL;
            Type *call_ret = wasm_call_ret_type(cc, node);
            if (!call_ret && ind_ft && ind_ft->ret) call_ret = ind_ft->ret;
            int call_is_sret = wasm_is_aggregate_type(call_ret) ? 1 : 0;
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
                } else if (ind_ft && ind_ft->params && a < ind_ft->num_params && ind_ft->params[a]) {
                    uint8_t param_vt = wasm_valtype_from_ctype(ind_ft->params[a]);
                    wasm_emit_type_conversion(&fn->body, arg_vt, node->args[a]->type, param_vt, ind_ft->params[a]);
                }
            }
            if (call_is_sret) {
                wasm_emit_sret_push(fn, lmap, node);
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
                    } else if (ventry && ventry->is_memory) {
                        if (lmap->fp_local_idx >= 0) {
                            wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                            wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->fp_local_idx);
                        } else {
                            wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                            wasm_buf_append_sleb128(&fn->body, 0);
                        }
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_LOAD);
                        wasm_emit_memarg_safe(&fn->body, 2, (uint32_t)ventry->stack_offset);
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
                int total_call_params = node->num_args + call_is_sret;
                uint8_t stack_ptypes[MAX_PARAMS];
                uint8_t *ptypes = stack_ptypes;
                if (total_call_params > MAX_PARAMS) {
                    ptypes = (uint8_t *)malloc((size_t)total_call_params);
                    if (!ptypes) break;
                }
                int p;
                for (p = 0; p < node->num_args; p++) {
                    if (ind_ft && ind_ft->params && p < ind_ft->num_params && ind_ft->params[p]) {
                        ptypes[p] = wasm_valtype_from_ctype(ind_ft->params[p]);
                    } else {
                        ptypes[p] = wasm_node_valtype(node->args[p]);
                    }
                }
                if (call_is_sret) {
                    ptypes[node->num_args] = WASM_TYPE_I32;
                }
                uint8_t rtypes[1];
                int num_r = 0;
                if (call_ret && call_ret->kind == TY_VOID) {
                    num_r = 0;
                } else if (call_ret) {
                    rtypes[0] = wasm_valtype_from_ctype(call_ret);
                    num_r = 1;
                } else {
                    rtypes[0] = WASM_TYPE_I32;
                    num_r = 1;
                }
                int call_type_idx = wasm_module_add_type((WasmModule *)mod, ptypes, total_call_params, rtypes, num_r);
                if (ptypes != stack_ptypes) free(ptypes);
                if (call_type_idx < 0) break;
                wasm_buf_append_u8(&fn->body, WASM_OP_CALL_INDIRECT);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)call_type_idx);
                wasm_buf_append_uleb128(&fn->body, 0); /* Table 0 */
                if (num_r > 0) {
                    uint8_t expr_vt = wasm_node_valtype(node);
                    wasm_emit_type_conversion(&fn->body, rtypes[0], call_ret, expr_vt, node->type);
                }
            }
            break;
        }

        case ND_TERNARY: {
            int is_void_ternary = !wasm_expr_has_value(mod, node);
            uint8_t target_vt = wasm_node_valtype(node);
            uint8_t then_vt = wasm_node_valtype(node->then_body);
            uint8_t else_vt = wasm_node_valtype(node->else_body);

            /* Evaluate condition (must be i32 boolean) */
            uint8_t cond_vt = wasm_node_valtype(node->cond);
            wasm_lower_expr(cc, mod, fn, lmap, gmap, node->cond);
            if (cond_vt != WASM_TYPE_I32) {
                wasm_emit_to_bool_i32(&fn->body, cond_vt);
            }

            if (is_void_ternary) {
                wasm_buf_append_u8(&fn->body, WASM_OP_IF);
                wasm_buf_append_u8(&fn->body, WASM_TYPE_VOID);
                if (node->then_body) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->then_body);
                    if (wasm_expr_has_value(mod, node->then_body)) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_DROP);
                    }
                }
                wasm_buf_append_u8(&fn->body, WASM_OP_ELSE);
                if (node->else_body) {
                    wasm_lower_expr(cc, mod, fn, lmap, gmap, node->else_body);
                    if (wasm_expr_has_value(mod, node->else_body)) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_DROP);
                    }
                }
                wasm_buf_append_u8(&fn->body, WASM_OP_END);
                break;
            }

            /* WASM if block producing target_vt value */
            wasm_buf_append_u8(&fn->body, WASM_OP_IF);
            wasm_buf_append_u8(&fn->body, target_vt);

            if (node->then_body && wasm_expr_has_value(mod, node->then_body)) {
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->then_body);
                wasm_emit_type_conversion(&fn->body, then_vt, node->then_body->type, target_vt, node->type);
            } else {
                if (node->then_body) wasm_lower_expr(cc, mod, fn, lmap, gmap, node->then_body);
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

            if (node->else_body && wasm_expr_has_value(mod, node->else_body)) {
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->else_body);
                wasm_emit_type_conversion(&fn->body, else_vt, node->else_body->type, target_vt, node->type);
            } else {
                if (node->else_body) wasm_lower_expr(cc, mod, fn, lmap, gmap, node->else_body);
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
            if (wasm_func_reserve_locals(fn, fn->num_locals + 1U) < 0) return;
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
            if (wasm_expr_has_value(mod, node)) {
                wasm_buf_append_u8(&fn->body, WASM_OP_DROP);
            }
            break;
        }

        case ND_RETURN:
            if (node->lhs) {
                uint8_t expr_vt = wasm_node_valtype(node->lhs);
                uint8_t func_ret_vt = wasm_valtype_from_ctype(ret_type);
                wasm_lower_expr(cc, mod, fn, lmap, gmap, node->lhs);
                if (lmap->sret_local_idx >= 0 && wasm_is_aggregate_type(ret_type)) {
                    int rsz = type_size(ret_type);
                    if (rsz <= 0) rsz = 8;
                    if (lmap->tmp_val_idx >= 0) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                        wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_val_idx);
                        wasm_emit_memcpy_locals(&fn->body, fn, (uint32_t)lmap->sret_local_idx,
                                                (uint32_t)lmap->tmp_val_idx, rsz);
                    } else {
                        wasm_buf_append_u8(&fn->body, WASM_OP_DROP);
                    }
                    if (lmap->frame_size > 0 && lmap->fp_local_idx >= 0) {
                        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                        wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->fp_local_idx);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                        wasm_buf_append_sleb128(&fn->body, lmap->frame_size);
                        wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD);
                        wasm_buf_append_u8(&fn->body, WASM_OP_GLOBAL_SET);
                        wasm_buf_append_uleb128(&fn->body, 0);
                    }
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->sret_local_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_RETURN);
                    break;
                }
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
                wasm_lower_stmt(cc, mod, fn, lmap, gmap, node->init, ret_type);
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
                if (wasm_expr_has_value(mod, node->inc)) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_DROP);
                }
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
            enum { WASM_SW_MAX_FLAT = 4096, WASM_SW_MAX_LBL = 1024 };
            Node **flat_stmts = (Node **)calloc(WASM_SW_MAX_FLAT, sizeof(Node *));
            WasmSwitchLabel *labels = (WasmSwitchLabel *)calloc(WASM_SW_MAX_LBL, sizeof(WasmSwitchLabel));
            int *grp_starts = (int *)calloc(WASM_SW_MAX_LBL, sizeof(int));
            if (!flat_stmts || !labels || !grp_starts) {
                free(flat_stmts);
                free(labels);
                free(grp_starts);
                break;
            }
            int num_flat = 0;
            int num_labels = 0;
            wasm_flatten_switch_body(node->body, flat_stmts, &num_flat, WASM_SW_MAX_FLAT, labels, &num_labels, WASM_SW_MAX_LBL);

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
            free(flat_stmts);
            free(labels);
            free(grp_starts);
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
                if (wasm_expr_has_value(mod, node)) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_DROP);
                }
            }
            break;
    }
}

/* CFG Flattener & br_table Dispatcher for Functions Containing goto */
typedef enum {
    WASM_CFG_NONE = 0,
    WASM_CFG_JMP,
    WASM_CFG_BRANCH,
    WASM_CFG_SWITCH,
    WASM_CFG_RETURN,
    WASM_CFG_EXIT
} WasmCfgTermKind;

typedef struct {
    int64_t val;
    int target;
} WasmCfgSwitchCase;

typedef struct {
    Node **stmts;
    int num_stmts;
    int cap_stmts;
    WasmCfgTermKind term;
    int target;
    int false_target;
    Node *cond_or_ret;
    WasmCfgSwitchCase *cases;
    int num_cases;
    int default_target;
} WasmCfgBlock;

typedef struct {
    char name[MAX_IDENT];
    int block_id;
} WasmCfgLabel;

typedef struct {
    WasmCfgBlock *blocks;
    int num_blocks;
    int cap_blocks;
    WasmCfgLabel *labels;
    int num_labels;
    int cap_labels;
} WasmCfg;

static int wasm_ast_has_goto(Node *node) {
    if (!node) return 0;
    if (node->kind == ND_GOTO) return 1;
    if (node->lhs && wasm_ast_has_goto(node->lhs)) return 1;
    if (node->rhs && wasm_ast_has_goto(node->rhs)) return 1;
    if (node->cond && wasm_ast_has_goto(node->cond)) return 1;
    if (node->then_body && wasm_ast_has_goto(node->then_body)) return 1;
    if (node->else_body && wasm_ast_has_goto(node->else_body)) return 1;
    if (node->body && wasm_ast_has_goto(node->body)) return 1;
    if (node->init && wasm_ast_has_goto(node->init)) return 1;
    if (node->inc && wasm_ast_has_goto(node->inc)) return 1;
    if (node->stmts) {
        int s;
        for (s = 0; s < node->num_stmts; s++) {
            if (wasm_ast_has_goto(node->stmts[s])) return 1;
        }
    }
    return 0;
}

static int wasm_cfg_new_block(WasmCfg *cfg) {
    if (cfg->num_blocks >= cfg->cap_blocks) {
        int nc = cfg->cap_blocks ? cfg->cap_blocks * 2 : 32;
        WasmCfgBlock *nb = (WasmCfgBlock *)realloc(cfg->blocks, (size_t)nc * sizeof(WasmCfgBlock));
        if (!nb) return -1;
        memset(nb + cfg->cap_blocks, 0, (size_t)(nc - cfg->cap_blocks) * sizeof(WasmCfgBlock));
        cfg->blocks = nb;
        cfg->cap_blocks = nc;
    }
    int idx = cfg->num_blocks++;
    memset(&cfg->blocks[idx], 0, sizeof(WasmCfgBlock));
    return idx;
}

static void wasm_cfg_add_stmt(WasmCfg *cfg, int bb, Node *stmt) {
    if (bb < 0 || bb >= cfg->num_blocks || !stmt) return;
    WasmCfgBlock *b = &cfg->blocks[bb];
    if (b->num_stmts >= b->cap_stmts) {
        int nc = b->cap_stmts ? b->cap_stmts * 2 : 16;
        Node **ns = (Node **)realloc(b->stmts, (size_t)nc * sizeof(Node *));
        if (!ns) return;
        b->stmts = ns;
        b->cap_stmts = nc;
    }
    b->stmts[b->num_stmts++] = stmt;
}

static int wasm_cfg_get_label_block(WasmCfg *cfg, const char *name) {
    if (!name || !name[0]) return -1;
    int i;
    for (i = 0; i < cfg->num_labels; i++) {
        if (strcmp(cfg->labels[i].name, name) == 0) {
            return cfg->labels[i].block_id;
        }
    }
    if (cfg->num_labels >= cfg->cap_labels) {
        int nc = cfg->cap_labels ? cfg->cap_labels * 2 : 32;
        WasmCfgLabel *nl = (WasmCfgLabel *)realloc(cfg->labels, (size_t)nc * sizeof(WasmCfgLabel));
        if (!nl) return -1;
        cfg->labels = nl;
        cfg->cap_labels = nc;
    }
    int bb = wasm_cfg_new_block(cfg);
    if (bb < 0) return -1;
    int lidx = cfg->num_labels++;
    strncpy(cfg->labels[lidx].name, name, MAX_IDENT - 1);
    cfg->labels[lidx].name[MAX_IDENT - 1] = '\0';
    cfg->labels[lidx].block_id = bb;
    return bb;
}

static void wasm_cfg_free(WasmCfg *cfg) {
    if (!cfg) return;
    int i;
    for (i = 0; i < cfg->num_blocks; i++) {
        free(cfg->blocks[i].stmts);
        free(cfg->blocks[i].cases);
    }
    free(cfg->blocks);
    free(cfg->labels);
    memset(cfg, 0, sizeof(WasmCfg));
}

static int wasm_cfg_build_stmt(WasmCfg *cfg, Node *stmt, int cur_bb, int break_tgt, int cont_tgt) {
    if (!stmt) return cur_bb;

    switch (stmt->kind) {
        case ND_NOP:
        case ND_VAR:
            return cur_bb;

        case ND_LABEL: {
            int lbl_bb = wasm_cfg_get_label_block(cfg, stmt->label_name);
            if (lbl_bb < 0) return cur_bb;
            if (cur_bb >= 0) {
                cfg->blocks[cur_bb].term = WASM_CFG_JMP;
                cfg->blocks[cur_bb].target = lbl_bb;
            }
            if (stmt->lhs) {
                return wasm_cfg_build_stmt(cfg, stmt->lhs, lbl_bb, break_tgt, cont_tgt);
            }
            return lbl_bb;
        }

        case ND_GOTO: {
            int tgt = wasm_cfg_get_label_block(cfg, stmt->label_name);
            if (cur_bb >= 0 && tgt >= 0) {
                cfg->blocks[cur_bb].term = WASM_CFG_JMP;
                cfg->blocks[cur_bb].target = tgt;
            }
            return -1;
        }

        case ND_BREAK: {
            if (cur_bb >= 0 && break_tgt >= 0) {
                cfg->blocks[cur_bb].term = WASM_CFG_JMP;
                cfg->blocks[cur_bb].target = break_tgt;
            }
            return -1;
        }

        case ND_CONTINUE: {
            if (cur_bb >= 0 && cont_tgt >= 0) {
                cfg->blocks[cur_bb].term = WASM_CFG_JMP;
                cfg->blocks[cur_bb].target = cont_tgt;
            }
            return -1;
        }

        case ND_RETURN: {
            if (cur_bb >= 0) {
                cfg->blocks[cur_bb].term = WASM_CFG_RETURN;
                cfg->blocks[cur_bb].cond_or_ret = stmt;
            }
            return -1;
        }

        case ND_BLOCK: {
            int s;
            for (s = 0; s < stmt->num_stmts; s++) {
                cur_bb = wasm_cfg_build_stmt(cfg, stmt->stmts[s], cur_bb, break_tgt, cont_tgt);
            }
            return cur_bb;
        }

        case ND_IF: {
            Node *then_node = stmt->then_body ? stmt->then_body : stmt->body;
            Node *else_node = stmt->else_body;
            if (cur_bb < 0) {
                int te = then_node ? wasm_cfg_build_stmt(cfg, then_node, -1, break_tgt, cont_tgt) : -1;
                int ee = else_node ? wasm_cfg_build_stmt(cfg, else_node, -1, break_tgt, cont_tgt) : -1;
                if (te < 0 && ee < 0) return -1;
                int after_bb = wasm_cfg_new_block(cfg);
                if (te >= 0) { cfg->blocks[te].term = WASM_CFG_JMP; cfg->blocks[te].target = after_bb; }
                if (ee >= 0) { cfg->blocks[ee].term = WASM_CFG_JMP; cfg->blocks[ee].target = after_bb; }
                return after_bb;
            }
            int then_bb = wasm_cfg_new_block(cfg);
            int after_bb = wasm_cfg_new_block(cfg);
            int else_bb = else_node ? wasm_cfg_new_block(cfg) : after_bb;
            cfg->blocks[cur_bb].term = WASM_CFG_BRANCH;
            cfg->blocks[cur_bb].cond_or_ret = stmt->cond;
            cfg->blocks[cur_bb].target = then_bb;
            cfg->blocks[cur_bb].false_target = else_bb;
            int te = then_node ? wasm_cfg_build_stmt(cfg, then_node, then_bb, break_tgt, cont_tgt) : then_bb;
            if (te >= 0) { cfg->blocks[te].term = WASM_CFG_JMP; cfg->blocks[te].target = after_bb; }
            if (else_node) {
                int ee = wasm_cfg_build_stmt(cfg, else_node, else_bb, break_tgt, cont_tgt);
                if (ee >= 0) { cfg->blocks[ee].term = WASM_CFG_JMP; cfg->blocks[ee].target = after_bb; }
            }
            return after_bb;
        }

        case ND_WHILE: {
            Node *loop_body = stmt->body ? stmt->body : stmt->then_body;
            int hdr_bb = wasm_cfg_new_block(cfg);
            int body_bb = wasm_cfg_new_block(cfg);
            int after_bb = wasm_cfg_new_block(cfg);
            if (cur_bb >= 0) {
                cfg->blocks[cur_bb].term = WASM_CFG_JMP;
                cfg->blocks[cur_bb].target = hdr_bb;
            }
            if (stmt->cond) {
                cfg->blocks[hdr_bb].term = WASM_CFG_BRANCH;
                cfg->blocks[hdr_bb].cond_or_ret = stmt->cond;
                cfg->blocks[hdr_bb].target = body_bb;
                cfg->blocks[hdr_bb].false_target = after_bb;
            } else {
                cfg->blocks[hdr_bb].term = WASM_CFG_JMP;
                cfg->blocks[hdr_bb].target = body_bb;
            }
            int be = loop_body ? wasm_cfg_build_stmt(cfg, loop_body, (cur_bb >= 0 || wasm_ast_has_goto(loop_body)) ? body_bb : -1, after_bb, hdr_bb) : body_bb;
            if (be >= 0) { cfg->blocks[be].term = WASM_CFG_JMP; cfg->blocks[be].target = hdr_bb; }
            return after_bb;
        }

        case ND_DO_WHILE: {
            Node *loop_body = stmt->body ? stmt->body : stmt->then_body;
            int body_bb = wasm_cfg_new_block(cfg);
            int cond_bb = wasm_cfg_new_block(cfg);
            int after_bb = wasm_cfg_new_block(cfg);
            if (cur_bb >= 0) {
                cfg->blocks[cur_bb].term = WASM_CFG_JMP;
                cfg->blocks[cur_bb].target = body_bb;
            }
            int be = loop_body ? wasm_cfg_build_stmt(cfg, loop_body, body_bb, after_bb, cond_bb) : body_bb;
            if (be >= 0) { cfg->blocks[be].term = WASM_CFG_JMP; cfg->blocks[be].target = cond_bb; }
            if (stmt->cond) {
                cfg->blocks[cond_bb].term = WASM_CFG_BRANCH;
                cfg->blocks[cond_bb].cond_or_ret = stmt->cond;
                cfg->blocks[cond_bb].target = body_bb;
                cfg->blocks[cond_bb].false_target = after_bb;
            } else {
                cfg->blocks[cond_bb].term = WASM_CFG_JMP;
                cfg->blocks[cond_bb].target = body_bb;
            }
            return after_bb;
        }

        case ND_FOR: {
            Node *loop_body = stmt->body ? stmt->body : stmt->then_body;
            if (cur_bb >= 0 && stmt->init) {
                wasm_cfg_add_stmt(cfg, cur_bb, stmt->init);
            }
            int hdr_bb = wasm_cfg_new_block(cfg);
            int body_bb = wasm_cfg_new_block(cfg);
            int inc_bb = wasm_cfg_new_block(cfg);
            int after_bb = wasm_cfg_new_block(cfg);
            if (cur_bb >= 0) {
                cfg->blocks[cur_bb].term = WASM_CFG_JMP;
                cfg->blocks[cur_bb].target = hdr_bb;
            }
            if (stmt->cond) {
                cfg->blocks[hdr_bb].term = WASM_CFG_BRANCH;
                cfg->blocks[hdr_bb].cond_or_ret = stmt->cond;
                cfg->blocks[hdr_bb].target = body_bb;
                cfg->blocks[hdr_bb].false_target = after_bb;
            } else {
                cfg->blocks[hdr_bb].term = WASM_CFG_JMP;
                cfg->blocks[hdr_bb].target = body_bb;
            }
            int be = loop_body ? wasm_cfg_build_stmt(cfg, loop_body, body_bb, after_bb, inc_bb) : body_bb;
            if (be >= 0) { cfg->blocks[be].term = WASM_CFG_JMP; cfg->blocks[be].target = inc_bb; }
            if (stmt->inc) wasm_cfg_add_stmt(cfg, inc_bb, stmt->inc);
            cfg->blocks[inc_bb].term = WASM_CFG_JMP;
            cfg->blocks[inc_bb].target = hdr_bb;
            return after_bb;
        }

        case ND_SWITCH: {
            int after_sw = wasm_cfg_new_block(cfg);
            if (cur_bb >= 0) {
                /* Resolve label blocks BEFORE writing cfg->blocks[cur_bb] since wasm_cfg_new_block may realloc cfg->blocks */
                int def_tgt = (stmt->default_case && stmt->default_case->case_body)
                    ? wasm_cfg_get_label_block(cfg, stmt->default_case->case_body->label_name)
                    : after_sw;
                if (def_tgt < 0) def_tgt = after_sw;
                WasmCfgSwitchCase *cases = NULL;
                int ncases = 0;
                if (stmt->num_cases > 0) {
                    cases = (WasmCfgSwitchCase *)malloc((size_t)stmt->num_cases * sizeof(WasmCfgSwitchCase));
                    if (cases) {
                        uint8_t cond_vt = wasm_node_valtype(stmt->cond);
                        int ci;
                        for (ci = 0; ci < stmt->num_cases; ci++) {
                            Node *cn = stmt->cases[ci];
                            if (!cn || !cn->case_body) continue;
                            int tgt = wasm_cfg_get_label_block(cfg, cn->case_body->label_name);
                            if (tgt < 0) continue;
                            int64_t cv = (int64_t)cn->case_val;
                            if (cond_vt == WASM_TYPE_I32) {
                                if (stmt->cond && wasm_type_is_unsigned_or_ptr(stmt->cond->type)) {
                                    cv = (int64_t)(uint64_t)(uint32_t)cv;
                                } else {
                                    cv = (int64_t)(int32_t)cv;
                                }
                            }
                            cases[ncases].val = cv;
                            cases[ncases].target = tgt;
                            ncases++;
                        }
                    }
                }
                WasmCfgBlock *sb = &cfg->blocks[cur_bb];
                sb->term = WASM_CFG_SWITCH;
                sb->cond_or_ret = stmt->cond;
                sb->default_target = def_tgt;
                sb->cases = cases;
                sb->num_cases = ncases;
            }
            int sw_end = wasm_cfg_build_stmt(cfg, stmt->body, -1, after_sw, cont_tgt);
            if (sw_end >= 0) {
                cfg->blocks[sw_end].term = WASM_CFG_JMP;
                cfg->blocks[sw_end].target = after_sw;
            }
            return after_sw;
        }

        default:
            if (cur_bb >= 0) wasm_cfg_add_stmt(cfg, cur_bb, stmt);
            return cur_bb;
    }
}

static void wasm_lower_stmt_cfg(Compiler *cc, const WasmModule *mod, WasmFunction *fn, WasmLocalMap *lmap, WasmGlobalMap *gmap, Node *root, Type *ret_type) {
    WasmCfg cfg;
    memset(&cfg, 0, sizeof(cfg));
    int entry_bb = wasm_cfg_new_block(&cfg);
    if (entry_bb < 0) {
        wasm_lower_stmt(cc, mod, fn, lmap, gmap, root, ret_type);
        return;
    }
    int end_bb = wasm_cfg_build_stmt(&cfg, root, entry_bb, -1, -1);
    if (end_bb >= 0 && cfg.blocks[end_bb].term == WASM_CFG_NONE) {
        cfg.blocks[end_bb].term = WASM_CFG_EXIT;
    }
    int N = cfg.num_blocks;
    int k;
    for (k = 0; k < N; k++) {
        if (cfg.blocks[k].term == WASM_CFG_NONE) cfg.blocks[k].term = WASM_CFG_EXIT;
    }

    int pc_loc = wasm_alloc_temp_local(fn, WASM_TYPE_I32);
    if (pc_loc < 0) {
        wasm_cfg_free(&cfg);
        return;
    }
    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
    wasm_buf_append_sleb128(&fn->body, 0);
    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
    wasm_buf_append_uleb128(&fn->body, (uint32_t)pc_loc);

    /* block $exit { loop $dispatch { block $b_N { ... block $b_0 { br_table } ... } br $exit } } */
    wasm_buf_append_u8(&fn->body, WASM_OP_BLOCK);
    wasm_buf_append_u8(&fn->body, WASM_TYPE_VOID);
    wasm_buf_append_u8(&fn->body, WASM_OP_LOOP);
    wasm_buf_append_u8(&fn->body, WASM_TYPE_VOID);
    for (k = 0; k <= N; k++) {
        wasm_buf_append_u8(&fn->body, WASM_OP_BLOCK);
        wasm_buf_append_u8(&fn->body, WASM_TYPE_VOID);
    }

    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
    wasm_buf_append_uleb128(&fn->body, (uint32_t)pc_loc);
    wasm_buf_append_u8(&fn->body, WASM_OP_BR_TABLE);
    wasm_buf_append_uleb128(&fn->body, (uint32_t)N);
    for (k = 0; k <= N; k++) {
        wasm_buf_append_uleb128(&fn->body, (uint32_t)k);
    }

    for (k = 0; k < N; k++) {
        WasmCfgBlock *b = &cfg.blocks[k];
        wasm_buf_append_u8(&fn->body, WASM_OP_END); /* close $b_k -> enter body k */
        int si;
        for (si = 0; si < b->num_stmts; si++) {
            wasm_lower_stmt(cc, mod, fn, lmap, gmap, b->stmts[si], ret_type);
        }
        uint32_t depth_dispatch = (uint32_t)(N - k);
        uint32_t depth_exit = (uint32_t)(N - k + 1);
        switch (b->term) {
            case WASM_CFG_JMP:
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                wasm_buf_append_sleb128(&fn->body, b->target);
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)pc_loc);
                wasm_buf_append_u8(&fn->body, WASM_OP_BR);
                wasm_buf_append_uleb128(&fn->body, depth_dispatch);
                break;

            case WASM_CFG_BRANCH: {
                uint8_t cond_vt = wasm_node_valtype(b->cond_or_ret);
                wasm_lower_expr(cc, mod, fn, lmap, gmap, b->cond_or_ret);
                if (cond_vt != WASM_TYPE_I32) {
                    wasm_emit_to_bool_i32(&fn->body, cond_vt);
                }
                wasm_buf_append_u8(&fn->body, WASM_OP_IF);
                wasm_buf_append_u8(&fn->body, WASM_TYPE_VOID);
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                wasm_buf_append_sleb128(&fn->body, b->target);
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)pc_loc);
                wasm_buf_append_u8(&fn->body, WASM_OP_ELSE);
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                wasm_buf_append_sleb128(&fn->body, b->false_target);
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)pc_loc);
                wasm_buf_append_u8(&fn->body, WASM_OP_END);
                wasm_buf_append_u8(&fn->body, WASM_OP_BR);
                wasm_buf_append_uleb128(&fn->body, depth_dispatch);
                break;
            }

            case WASM_CFG_SWITCH: {
                uint8_t cond_vt = wasm_node_valtype(b->cond_or_ret);
                wasm_lower_expr(cc, mod, fn, lmap, gmap, b->cond_or_ret);
                wasm_emit_type_conversion(&fn->body, cond_vt, b->cond_or_ret ? b->cond_or_ret->type : NULL,
                                          WASM_TYPE_I64, b->cond_or_ret ? b->cond_or_ret->type : NULL);
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                int ci;
                for (ci = 0; ci < b->num_cases; ci++) {
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap->tmp_i64_idx);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);
                    wasm_buf_append_sleb128_i64(&fn->body, b->cases[ci].val);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_EQ);
                    wasm_buf_append_u8(&fn->body, WASM_OP_IF);
                    wasm_buf_append_u8(&fn->body, WASM_TYPE_VOID);
                    wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                    wasm_buf_append_sleb128(&fn->body, b->cases[ci].target);
                    wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                    wasm_buf_append_uleb128(&fn->body, (uint32_t)pc_loc);
                    wasm_buf_append_u8(&fn->body, WASM_OP_BR);
                    wasm_buf_append_uleb128(&fn->body, depth_dispatch + 1U);
                    wasm_buf_append_u8(&fn->body, WASM_OP_END);
                }
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                wasm_buf_append_sleb128(&fn->body, b->default_target);
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)pc_loc);
                wasm_buf_append_u8(&fn->body, WASM_OP_BR);
                wasm_buf_append_uleb128(&fn->body, depth_dispatch);
                break;
            }

            case WASM_CFG_RETURN:
                if (b->cond_or_ret) {
                    wasm_lower_stmt(cc, mod, fn, lmap, gmap, b->cond_or_ret, ret_type);
                }
                wasm_buf_append_u8(&fn->body, WASM_OP_BR);
                wasm_buf_append_uleb128(&fn->body, depth_exit);
                break;

            default:
                wasm_buf_append_u8(&fn->body, WASM_OP_BR);
                wasm_buf_append_uleb128(&fn->body, depth_exit);
                break;
        }
    }

    wasm_buf_append_u8(&fn->body, WASM_OP_END); /* close $b_N */
    wasm_buf_append_u8(&fn->body, WASM_OP_BR);
    wasm_buf_append_uleb128(&fn->body, 1); /* break $exit */
    wasm_buf_append_u8(&fn->body, WASM_OP_END); /* end loop $dispatch */
    wasm_buf_append_u8(&fn->body, WASM_OP_END); /* end block $exit */
    wasm_cfg_free(&cfg);
}

/* Pre-scan AST to discover local variables, arrays, structs, and address-taken entities */
static void wasm_scan_ast_locals(const WasmModule *mod, Node *node, WasmLocalMap *lmap, WasmGlobalMap *gmap) {
    if (!node || !lmap) return;

    if (node->kind == ND_VAR) {
        const char *vname = wasm_node_var_name(node);
        int is_global_sym = (node->sym && node->sym->is_global) ? 1 : 0;
        int is_local_sym = (node->sym && (node->sym->is_local || node->sym->is_param)) ? 1 : 0;
        int is_ext = is_global_sym || (!is_local_sym && (wasm_global_find(gmap, vname) != NULL || wasm_find_func_idx(mod, vname) >= 0));
        if (vname && vname[0] && !is_ext) {
            WasmLocalEntry *entry = wasm_local_find_node(lmap, node);
            if (!entry) {
                int e = wasm_local_alloc_entry(lmap);
                if (e < 0) return;
                entry = &lmap->entries[e];
                strncpy(entry->name, vname, sizeof(entry->name) - 1);
                entry->type = node->type;
                entry->val_type = wasm_valtype_from_ctype(node->type);
                entry->sym = (void *)node->sym;
            } else if (!entry->sym && node->sym) {
                entry->sym = (void *)node->sym;
            }
            if (entry && node->type && (node->type->kind == TY_ARRAY || node->type->kind == TY_STRUCT || node->type->kind == TY_UNION)) {
                entry->is_memory = 1;
                entry->size = type_size(node->type);
                if (entry->size <= 0) entry->size = 4;
            }
        }
    } else if (node->kind == ND_ADDR) {
        if (node->lhs && node->lhs->kind == ND_VAR) {
            const char *vname = wasm_node_var_name(node->lhs);
            int is_global_sym = (node->lhs->sym && node->lhs->sym->is_global) ? 1 : 0;
            int is_local_sym = (node->lhs->sym && node->lhs->sym->is_local) ? 1 : 0;
            int is_ext = is_global_sym || (!is_local_sym && (wasm_global_find(gmap, vname) != NULL || wasm_find_func_idx(mod, vname) >= 0));
            if (vname && vname[0] && !is_ext) {
                WasmLocalEntry *entry = wasm_local_find_node(lmap, node->lhs);
                if (!entry) {
                    int e = wasm_local_alloc_entry(lmap);
                    if (e < 0) return;
                    entry = &lmap->entries[e];
                    strncpy(entry->name, vname, sizeof(entry->name) - 1);
                    entry->type = node->lhs->type;
                    entry->val_type = wasm_valtype_from_ctype(node->lhs->type);
                    entry->sym = (void *)node->lhs->sym;
                } else if (!entry->sym && node->lhs->sym) {
                    entry->sym = (void *)node->lhs->sym;
                }
                if (entry) {
                    entry->is_memory = 1;
                    entry->size = entry->type ? type_size((Type *)entry->type) : 4;
                    if (entry->size <= 0) entry->size = 4;
                }
            }
        }
    } else if (node->kind == ND_CALL) {
        Type *cret = wasm_call_ret_type(NULL, node);
        if (wasm_is_aggregate_type(cret)) {
            int found = 0;
            int si;
            for (si = 0; si < lmap->count; si++) {
                if (lmap->entries[si].sym == (void *)node) { found = 1; break; }
            }
            if (!found) {
                int e = wasm_local_alloc_entry(lmap);
                if (e >= 0) {
                    int ssz = type_size(cret);
                    lmap->entries[e].sym = (void *)node;
                    snprintf(lmap->entries[e].name, sizeof(lmap->entries[e].name), "__sret_%d", e);
                    lmap->entries[e].type = cret;
                    lmap->entries[e].val_type = WASM_TYPE_I32;
                    lmap->entries[e].is_memory = 1;
                    lmap->entries[e].size = (ssz > 0) ? ssz : 8;
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

static int wasm_eval_ginit_ptr_const(const WasmModule *mod, WasmGlobalMap *gmap, Node *n, int64_t *out_val) {
    while (n && n->kind == ND_CAST) n = n->lhs;
    if (!n) return 0;
    if (n->kind == ND_STR) {
        int s_idx = n->str_id;
        *out_val = (s_idx >= 0 && s_idx < MAX_STRINGS && wasm_string_offsets[s_idx] > 0)
                   ? (int64_t)(uint64_t)wasm_string_offsets[s_idx] : 1024LL;
        return 1;
    }
    if (n->kind == ND_ADDR && n->lhs) {
        if (n->lhs->kind == ND_DEREF && n->lhs->lhs) {
            return wasm_eval_ginit_ptr_const(mod, gmap, n->lhs->lhs, out_val);
        }
        if (n->lhs->kind == ND_MEMBER) {
            Node *mbase = n->lhs->lhs;
            if (mbase && mbase->kind == ND_DEREF && mbase->lhs) mbase = mbase->lhs;
            if (wasm_eval_ginit_ptr_const(mod, gmap, mbase, out_val)) {
                *out_val += (int64_t)n->lhs->member_offset;
                return 1;
            }
        }
        return wasm_eval_ginit_ptr_const(mod, gmap, n->lhs, out_val);
    }
    if (n->kind == ND_VAR) {
        const char *nm = wasm_node_var_name(n);
        if (nm && nm[0]) {
            WasmGlobalVar *gv = wasm_global_find(gmap, nm);
            if (gv) { *out_val = (int64_t)(uint64_t)gv->offset; return 1; }
            int fidx = wasm_find_func_idx(mod, nm);
            if (fidx >= 0) { *out_val = (int64_t)fidx; return 1; }
        }
        return 0;
    }
    if ((n->kind == ND_ADD || n->kind == ND_SUB) && n->lhs && n->rhs) {
        int64_t base = 0;
        int ok = 1;
        if (wasm_eval_ginit_ptr_const(mod, gmap, n->lhs, &base)) {
            int64_t idx = (int64_t)eval_const_expr_p4(n->rhs, &ok);
            if (ok) {
                int esz = (n->lhs->type && (is_pointer(n->lhs->type) || n->lhs->type->kind == TY_ARRAY))
                          ? wasm_ptr_elem_size(n->lhs->type) : 1;
                *out_val = base + ((n->kind == ND_SUB) ? -idx : idx) * (int64_t)esz;
                return 1;
            }
        }
    }
    return 0;
}

static void wasm_fill_ginit(Compiler *cc, const WasmModule *mod, WasmGlobalMap *gmap, Node *init, Type *t, uint8_t *dst, int max_sz) {
    if (!init || max_sz <= 0) return;
    if (init->kind == ND_INIT_LIST) {
        if (t && (t->kind == TY_STRUCT || t->kind == TY_UNION)) {
            StructField *f = t->fields;
            for (int i = 0; i < init->num_args && f; i++, f = f->next) {
                if (f->type && f->offset + type_size(f->type) <= max_sz) {
                    wasm_fill_ginit(cc, mod, gmap, init->args[i], f->type, dst + f->offset, type_size(f->type));
                }
                if (t->kind == TY_UNION) break;
            }
        } else {
            Type *et = (t && t->base) ? t->base : t; int esz = (et && type_size(et) > 0) ? type_size(et) : 4;
            for (int i = 0; i < init->num_args && (i + 1) * esz <= max_sz; i++) wasm_fill_ginit(cc, mod, gmap, init->args[i], et, dst + i * esz, esz);
        }
        return;
    }
    while (init && init->kind == ND_CAST) init = init->lhs;
    if (!init) return;
    if (init->kind == ND_STR) {
        int s_idx = init->str_id;
        if (t && t->kind == TY_ARRAY && cc && s_idx >= 0 && s_idx < cc->num_strings && cc->strings[s_idx].data) {
            int slen = cc->strings[s_idx].len;
            if (slen < 0) slen = (int)strlen(cc->strings[s_idx].data);
            int cpy = (slen + 1 <= max_sz) ? (slen + 1) : max_sz;
            if (cpy > 0) memcpy(dst, cc->strings[s_idx].data, (size_t)cpy);
            return;
        }
        uint32_t soff = (s_idx >= 0 && s_idx < MAX_STRINGS && wasm_string_offsets[s_idx] > 0)
                        ? wasm_string_offsets[s_idx] : 1024U;
        uint64_t pval = (uint64_t)soff;
        memcpy(dst, &pval, max_sz <= 8 ? (size_t)max_sz : 8);
        return;
    }
    if (init->kind == ND_FLIT ||
        (init->kind == ND_NEG && init->lhs && init->lhs->kind == ND_FLIT) ||
        (t && (t->kind == TY_FLOAT || t->kind == TY_DOUBLE))) {
        double dv = (init->kind == ND_NEG && init->lhs) ? -init->lhs->f_val : (init->kind == ND_FLIT ? init->f_val : (double)init->int_val);
        if (t && t->kind == TY_FLOAT) { float fv = (float)dv; if (max_sz >= 4) memcpy(dst, &fv, 4); }
        else if (max_sz >= 8) memcpy(dst, &dv, 8);
        return;
    }
    int64_t ptr_val = 0;
    if (wasm_eval_ginit_ptr_const(mod, gmap, init, &ptr_val)) {
        memcpy(dst, &ptr_val, max_sz <= 8 ? (size_t)max_sz : 8);
        return;
    }
    while (init && (init->kind == ND_CAST || init->kind == ND_ADDR)) init = init->lhs;
    if (!init) return;
    const char *nm = wasm_node_var_name(init);
    int fidx = (nm && nm[0]) ? wasm_find_func_idx(mod, nm) : -1, ok = 1;
    int64_t val = (fidx >= 0) ? (int64_t)fidx : (int64_t)eval_const_expr_p4(init, &ok);
    if (fidx < 0 && !ok) val = (int64_t)init->int_val;
    memcpy(dst, &val, max_sz <= 8 ? (size_t)max_sz : 8);
}

static int wasm_name_in_csv(const char *csv, const char *name) {
    if (!csv || !name || !name[0]) return 0;
    size_t nlen = strlen(name);
    const char *p = csv;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        if (!*p) break;
        const char *start = p;
        while (*p && *p != ',') p++;
        const char *end = p;
        while (end > start && (end[-1] == ' ' || end[-1] == '\t')) end--;
        if ((size_t)(end - start) == nlen && memcmp(start, name, nlen) == 0) return 1;
        if (*p == ',') p++;
    }
    return 0;
}

static int wasm_is_runtime_reserved_name(const char *name) {
    if (!name || !name[0]) return 1;
    if (strncmp(name, "__builtin_", 10) == 0 || strncmp(name, "__zcc_wasm_", 11) == 0) return 1;
    return (strcmp(name, "puts") == 0 || strcmp(name, "putchar") == 0 ||
            strcmp(name, "write") == 0 || strcmp(name, "read") == 0 ||
            strcmp(name, "exit") == 0 || strcmp(name, "_start") == 0 ||
            strcmp(name, "malloc") == 0 || strcmp(name, "free") == 0 ||
            strcmp(name, "calloc") == 0 || strcmp(name, "realloc") == 0 ||
            strcmp(name, "zcc_heap_used") == 0 || strcmp(name, "zcc_heap_capacity") == 0 ||
            strcmp(name, "fabsf") == 0 || strcmp(name, "sqrtf") == 0);
}

static Symbol *wasm_find_scope_symbol(Compiler *cc, const char *name) {
    Scope *sc = cc ? cc->current_scope : NULL;
    while (sc) {
        Symbol *s = sc->symbols;
        while (s) {
            if (!s->is_typedef && s->name[0] && strcmp(s->name, name) == 0) return s;
            s = s->next;
        }
        sc = sc->parent;
    }
    return NULL;
}

static int wasm_ast_has_func_def(Node *prog, const char *name) {
    Node *c = prog;
    while (c) {
        if (c->kind == ND_FUNC_DEF && strcmp(c->func_def_name, name) == 0) return 1;
        c = c->next;
    }
    return 0;
}

static int wasm_fn_param_matches(Node *fn_def, const char *name) {
    if (!fn_def || !fn_def->func_params || !name) return 0;
    int p;
    for (p = 0; p < fn_def->num_params && p < MAX_PARAMS; p++) {
        if (fn_def->func_params->names[p][0] && strcmp(fn_def->func_params->names[p], name) == 0) return 1;
    }
    return 0;
}

static int wasm_collect_extern_imports_walk(Compiler *cc, Node *prog, Node *fn_def, Node *node,
                                            WasmModule *mod, WasmGlobalMap *gmap) {
    if (!node) return 0;
    if (node->kind == ND_CALL && !node->lhs && node->func_name[0]) {
        const char *fname = node->func_name;
        if (!wasm_is_runtime_reserved_name(fname) &&
            !wasm_ast_has_func_def(prog, fname) &&
            !wasm_global_find(gmap, fname) &&
            !wasm_fn_param_matches(fn_def, fname) &&
            wasm_module_find_func_idx(mod, fname) < 0) {
            Symbol *sym = wasm_find_scope_symbol(cc, fname);
            if (sym && sym->type && sym->type->kind == TY_FUNC) {
                uint8_t param_types[MAX_PARAMS];
                int num_params = sym->type->num_params;
                int p;
                if (num_params == 0 && !sym->type->params && node->num_args > 0) {
                    num_params = node->num_args;
                    if (num_params > MAX_PARAMS) return -1;
                    for (p = 0; p < num_params; p++) param_types[p] = wasm_node_valtype(node->args[p]);
                } else {
                    if (num_params < 0 || num_params > MAX_PARAMS) {
                        fprintf(stderr, "zcc: wasm extern import '%s' parameter count %d exceeds MAX_PARAMS (%d)\n",
                                fname, num_params, MAX_PARAMS);
                        return -1;
                    }
                    for (p = 0; p < num_params; p++) {
                        Type *ptype = sym->type->params ? sym->type->params[p] : NULL;
                        param_types[p] = wasm_valtype_from_ctype(ptype);
                    }
                }
                uint8_t res_types[1];
                int num_res = 0;
                Type *ret_type = sym->type->ret;
                if (!ret_type || ret_type->kind != TY_VOID) {
                    res_types[0] = wasm_valtype_from_ctype(ret_type);
                    num_res = 1;
                }
                if (sym->type->is_variadic && num_params < MAX_PARAMS) {
                    param_types[num_params++] = WASM_TYPE_I32;
                }
                if (wasm_is_aggregate_type(ret_type) && num_params < MAX_PARAMS) {
                    param_types[num_params++] = WASM_TYPE_I32;
                }
                int imp_tidx = wasm_module_add_type(mod, param_types, num_params, res_types, num_res);
                if (imp_tidx < 0) return -1;
                const char *imp_mod = "env";
                const char *imp_field = fname;
                if (strncmp(fname, "__wasi_", 7) == 0 && fname[7] != '\0') {
                    imp_mod = "wasi_snapshot_preview1";
                    imp_field = fname + 7;
                }
                if (wasm_module_add_import_func(mod, imp_mod, imp_field, imp_tidx) < 0) return -1;
            }
        }
    }
    if (node->lhs && wasm_collect_extern_imports_walk(cc, prog, fn_def, node->lhs, mod, gmap) < 0) return -1;
    if (node->rhs && wasm_collect_extern_imports_walk(cc, prog, fn_def, node->rhs, mod, gmap) < 0) return -1;
    if (node->cond && wasm_collect_extern_imports_walk(cc, prog, fn_def, node->cond, mod, gmap) < 0) return -1;
    if (node->then_body && wasm_collect_extern_imports_walk(cc, prog, fn_def, node->then_body, mod, gmap) < 0) return -1;
    if (node->else_body && wasm_collect_extern_imports_walk(cc, prog, fn_def, node->else_body, mod, gmap) < 0) return -1;
    if (node->body && wasm_collect_extern_imports_walk(cc, prog, fn_def, node->body, mod, gmap) < 0) return -1;
    if (node->init && wasm_collect_extern_imports_walk(cc, prog, fn_def, node->init, mod, gmap) < 0) return -1;
    if (node->inc && wasm_collect_extern_imports_walk(cc, prog, fn_def, node->inc, mod, gmap) < 0) return -1;
    if (node->stmts) {
        int s;
        for (s = 0; s < node->num_stmts; s++) {
            if (wasm_collect_extern_imports_walk(cc, prog, fn_def, node->stmts[s], mod, gmap) < 0) return -1;
        }
    }
    if (node->args) {
        int a;
        for (a = 0; a < node->num_args; a++) {
            if (wasm_collect_extern_imports_walk(cc, prog, fn_def, node->args[a], mod, gmap) < 0) return -1;
        }
    }
    return 0;
}

int wasm_lower_program(Compiler *cc, Node *prog, const char *output_file) {
    if (!prog || !output_file) return -1;

    extern int zcc_wasm_export_minimal;
    extern int zcc_wasm_export_runtime;
    extern uint32_t zcc_wasm_max_pages;
    extern const char *zcc_wasm_export_list;

    WasmModule mod;
    wasm_module_init(&mod);

    WasmGlobalMap gmap;
    memset(&gmap, 0, sizeof(WasmGlobalMap));
    WasmLocalMap *active_lmap = NULL;
    memset(wasm_string_offsets, 0, sizeof(wasm_string_offsets));
    wasm_cur_prog = prog;

    /* 0. Place String Literals and Global Variables into Data Segments */
    uint32_t global_static_offset = 1024;

    /* Emit String Literals as a single coalesced contiguous blob at 1024 */
    {
        WASMBuffer str_blob;
        wasm_buf_init(&str_blob);
        int s;
        for (s = 0; s < cc->num_strings && s < MAX_STRINGS; s++) {
            if (cc->strings[s].data) {
                int slen = cc->strings[s].len;
                if (slen < 0) slen = (int)strlen(cc->strings[s].data);
                while (str_blob.size & 3U) wasm_buf_append_u8(&str_blob, 0);
                wasm_string_offsets[s] = 1024U + (uint32_t)str_blob.size;
                if (slen > 0) wasm_buf_append_bytes(&str_blob, (const uint8_t *)cc->strings[s].data, (size_t)slen);
                wasm_buf_append_u8(&str_blob, 0);
            }
        }
        if (str_blob.size > 0) {
            int seg_rc = wasm_module_add_data_segment(&mod, 1024U, str_blob.data, str_blob.size);
            global_static_offset = 1024U + (uint32_t)str_blob.size;
            if (seg_rc < 0) {
                wasm_buf_free(&str_blob);
                goto fail_cleanup;
            }
        }
        wasm_buf_free(&str_blob);
    }

    /* Scan cc->globals array and linked prog nodes via single deduplicated helper */
    int g;
    for (g = 0; g < cc->num_globals; g++) {
        Node *gvar = cc->globals[g];
        if (!gvar || gvar->kind != ND_GLOBAL_VAR) continue;
        if (wasm_register_global(&gmap, &global_static_offset, gvar) < 0) goto fail_cleanup;
    }

    Node *curr = prog;
    while (curr) {
        if (curr->kind == ND_GLOBAL_VAR || (curr->kind == ND_VAR && curr->sym && curr->sym->is_global)) {
            if (wasm_register_global(&gmap, &global_static_offset, curr) < 0) goto fail_cleanup;
        }
        curr = curr->next;
    }

    /* Configure stack-first linear memory layout: [0..1024) guard, static, >=64KB stack, then heap */
    if (wasm_module_configure_layout(&mod, global_static_offset, zcc_wasm_max_pages) < 0) goto fail_cleanup;

    /* 1. Detect built-in and WASI dependencies */
    int has_user_puts = 0, has_user_putchar = 0, has_user_write = 0, has_user_read = 0, has_user_exit = 0, has_user_start = 0;
    int needs_puts = 0, needs_putchar = 0, needs_write = 0, needs_read = 0, needs_exit = 0;
    int has_malloc = 0, has_free = 0, has_calloc = 0, has_realloc = 0, has_heap_used = 0, has_heap_cap = 0;
    int needs_malloc = 0, needs_free = 0, needs_calloc = 0, needs_realloc = 0, needs_heap_used = 0, needs_heap_cap = 0;
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
            if (strcmp(curr->func_def_name, "calloc") == 0) has_calloc = 1;
            if (strcmp(curr->func_def_name, "realloc") == 0) has_realloc = 1;
            if (strcmp(curr->func_def_name, "zcc_heap_used") == 0) has_heap_used = 1;
            if (strcmp(curr->func_def_name, "zcc_heap_capacity") == 0) has_heap_cap = 1;
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
            if (!has_calloc && wasm_ast_has_call_name(curr->body, "calloc")) needs_calloc = 1;
            if (!has_realloc && wasm_ast_has_call_name(curr->body, "realloc")) needs_realloc = 1;
            if (!has_heap_used && wasm_ast_has_call_name(curr->body, "zcc_heap_used")) needs_heap_used = 1;
            if (!has_heap_cap && wasm_ast_has_call_name(curr->body, "zcc_heap_capacity")) needs_heap_cap = 1;
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
        if (fd_write_imp_idx < 0) goto fail_cleanup;
    }

    if (needs_read) {
        uint8_t wasi_io_params[4] = { WASM_TYPE_I32, WASM_TYPE_I32, WASM_TYPE_I32, WASM_TYPE_I32 };
        uint8_t wasi_io_results[1] = { WASM_TYPE_I32 };
        int wasi_io_tidx = wasm_module_add_type(&mod, wasi_io_params, 4, wasi_io_results, 1);
        fd_read_imp_idx = wasm_module_add_import_func(&mod, "wasi_snapshot_preview1", "fd_read", wasi_io_tidx);
        if (fd_read_imp_idx < 0) goto fail_cleanup;
    }

    if (needs_exit || synthesize_start) {
        uint8_t exit_params[1] = { WASM_TYPE_I32 };
        int exit_tidx = wasm_module_add_type(&mod, exit_params, 1, NULL, 0);
        proc_exit_imp_idx = wasm_module_add_import_func(&mod, "wasi_snapshot_preview1", "proc_exit", exit_tidx);
        if (proc_exit_imp_idx < 0) goto fail_cleanup;
    }

    /* Auto-import called extern/unresolved C prototypes under WASM module "env" */
    curr = prog;
    while (curr) {
        if (curr->kind == ND_FUNC_DEF && curr->body) {
            if (wasm_collect_extern_imports_walk(cc, prog, curr, curr->body, &mod, &gmap) < 0) goto fail_cleanup;
        }
        curr = curr->next;
    }

    for (g = 0; g < mod.num_imports; g++) {
        if (wasm_module_add_table_element(&mod, (uint32_t)g) < 0) goto fail_cleanup;
    }

    /* 2. Pre-declare all user function signatures and add to Table 0 and Exports */
    curr = prog;
    while (curr) {
        if (curr->kind == ND_FUNC_DEF) {
            uint8_t param_types[MAX_PARAMS];
            int num_params = curr->num_params;
            if (num_params < 0 || num_params > MAX_PARAMS) {
                fprintf(stderr, "zcc: wasm parameter count %d exceeds MAX_PARAMS (%d) in '%s'\n",
                        num_params, MAX_PARAMS, curr->func_def_name);
                goto fail_cleanup;
            }
            int p;
            for (p = 0; p < num_params; p++) {
                Type *ptype = (curr->func_params && curr->func_params->types) ? curr->func_params->types[p] : NULL;
                param_types[p] = wasm_valtype_from_ctype(ptype);
            }
            if (curr->func_type && curr->func_type->is_variadic && num_params < MAX_PARAMS) {
                param_types[num_params++] = WASM_TYPE_I32; /* hidden $va_ptr */
            }
            uint8_t res_types[1];
            int num_res = 0;
            Type *ret_type = curr->func_type ? curr->func_type->ret : NULL;
            if (!ret_type || ret_type->kind != TY_VOID) {
                res_types[0] = wasm_valtype_from_ctype(ret_type);
                num_res = 1;
            }
            if (wasm_is_aggregate_type(ret_type) && num_params < MAX_PARAMS) {
                param_types[num_params++] = WASM_TYPE_I32; /* hidden $sret_ptr */
            }
            int t_idx = wasm_module_add_type(&mod, param_types, num_params, res_types, num_res);
            int f_idx = wasm_module_add_function(&mod, curr->func_def_name, t_idx);
            if (t_idx < 0 || f_idx < 0) goto fail_cleanup;
            uint32_t abs_f_idx = (uint32_t)(mod.num_imports + f_idx);
            if (wasm_module_add_table_element(&mod, abs_f_idx) < 0) goto fail_cleanup;
            if (!curr->is_static) {
                int should_export = 1;
                if (zcc_wasm_export_list != NULL) {
                    should_export = wasm_name_in_csv(zcc_wasm_export_list, curr->func_def_name);
                } else if (zcc_wasm_export_minimal) {
                    should_export = (strcmp(curr->func_def_name, "main") == 0 ||
                                     strcmp(curr->func_def_name, "_start") == 0);
                }
                if (should_export) {
                    wasm_module_add_export(&mod, curr->func_def_name, WASM_EXPORT_FUNC, abs_f_idx);
                }
            }
        }
        curr = curr->next;
    }
    for (g = 0; g < gmap.count; g++) if (gmap.vars[g].init && gmap.vars[g].size > 0) {
        uint8_t *gbuf = (uint8_t *)calloc(1, (size_t)gmap.vars[g].size);
        if (!gbuf) goto fail_cleanup;
        wasm_fill_ginit(cc, &mod, &gmap, gmap.vars[g].init, gmap.vars[g].type, gbuf, gmap.vars[g].size);
        int seg_rc = wasm_module_add_data_segment(&mod, gmap.vars[g].offset, gbuf, (size_t)gmap.vars[g].size);
        free(gbuf);
        if (seg_rc < 0) goto fail_cleanup;
    }

    /* 3. Synthesize freestanding runtime functions in src/wasm_emit.c */
    if (wasm_emit_synth_allocator_ex(&mod, !has_malloc && needs_malloc, !has_free && needs_free,
                                     !has_calloc && needs_calloc, !has_realloc && needs_realloc,
                                     !has_heap_used && needs_heap_used, !has_heap_cap && needs_heap_cap,
                                     zcc_wasm_export_runtime) < 0) {
        goto fail_cleanup;
    }
    if (wasm_emit_synth_wasi(&mod, fd_write_imp_idx, fd_read_imp_idx, proc_exit_imp_idx,
                             !has_user_puts && needs_puts,
                             !has_user_putchar && needs_putchar,
                             !has_user_write && needs_write,
                             !has_user_read && needs_read,
                             !has_user_exit && needs_exit,
                             synthesize_start, zcc_wasm_export_runtime) < 0) {
        goto fail_cleanup;
    }
    if (zcc_wasm_export_list != NULL) {
        int fi, kept = 0;
        for (fi = 0; fi < mod.num_funcs; fi++) {
            if (mod.funcs[fi].name[0] && wasm_name_in_csv(zcc_wasm_export_list, mod.funcs[fi].name)) {
                wasm_module_add_export(&mod, mod.funcs[fi].name, WASM_EXPORT_FUNC, (uint32_t)(mod.num_imports + fi));
            }
        }
        for (fi = 0; fi < mod.num_exports; fi++) {
            if (wasm_name_in_csv(zcc_wasm_export_list, mod.exports[fi].name)) {
                if (kept != fi) mod.exports[kept] = mod.exports[fi];
                kept++;
            }
        }
        mod.num_exports = kept;
    }

    /* 4. Second pass: Lower user function bodies with linear stack frame support */
    curr = prog;
    int f_cursor = 0;
    while (curr) {
        if (curr->kind == ND_FUNC_DEF && f_cursor < mod.num_funcs) {
            WasmFunction *fn = &mod.funcs[f_cursor++];
            WasmLocalMap lmap;
            memset(&lmap, 0, sizeof(WasmLocalMap));
            active_lmap = &lmap;
            lmap.fp_local_idx = -1;
            lmap.va_ptr_local_idx = -1;
            lmap.sret_local_idx = -1;
            lmap.tmp_addr_idx = -1;
            lmap.tmp_val_idx = -1;
            lmap.tmp_i64_idx = -1;
            lmap.tmp_f32_idx = -1;
            lmap.tmp_f64_idx = -1;
            lmap.tmp_v128_idx = -1;

            Type *ret_type = curr->func_type ? curr->func_type->ret : NULL;
            int is_var = (curr->func_type && curr->func_type->is_variadic && curr->num_params < MAX_PARAMS) ? 1 : 0;

            /* Map parameters to initial local indices */
            if (curr->func_params) {
                int p;
                for (p = 0; p < curr->num_params && p < MAX_PARAMS; p++) {
                    const char *pname = curr->func_params->names[p];
                    Type *ptype = curr->func_params->types ? curr->func_params->types[p] : NULL;
                    if (pname && pname[0]) {
                        int e = wasm_local_alloc_entry(&lmap);
                        if (e < 0) goto fail_cleanup;
                        strncpy(lmap.entries[e].name, pname, sizeof(lmap.entries[e].name) - 1);
                        lmap.entries[e].local_idx = (uint32_t)p;
                        lmap.entries[e].type = ptype;
                        lmap.entries[e].val_type = wasm_valtype_from_ctype(ptype);
                        lmap.entries[e].is_memory = 0;
                        lmap.entries[e].sym = (curr->func_params->syms && curr->func_params->syms[p]) ? (void *)curr->func_params->syms[p] : NULL;
                    }
                }
            }
            int num_param_entries = lmap.count;
            if (is_var) {
                lmap.va_ptr_local_idx = curr->num_params;
            }
            int is_sret = (wasm_is_aggregate_type(ret_type) && (curr->num_params + is_var) < MAX_PARAMS) ? 1 : 0;
            if (is_sret) {
                lmap.sret_local_idx = curr->num_params + is_var;
            }
            fn->num_params = (uint32_t)(curr->num_params + is_var + is_sret);

            /* Scan AST for local variables & memory allocations */
            if (curr->body) {
                wasm_scan_ast_locals(&mod, curr->body, &lmap, &gmap);
            }

            /* Ensure enough capacity for all non-memory locals + fp + 5 tmp slots */
            if (wasm_func_reserve_locals(fn, (uint32_t)(lmap.count + 8)) < 0) goto fail_cleanup;

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
                } else if (i >= num_param_entries) {
                    if (wasm_func_reserve_locals(fn, fn->num_locals + 1U) < 0) goto fail_cleanup;
                    lmap.entries[i].local_idx = fn->num_params + fn->num_locals++;
                    fn->local_types[fn->num_locals - 1] = lmap.entries[i].val_type ? lmap.entries[i].val_type : WASM_TYPE_I32;
                }
            }

            /* Allocate frame pointer and temporary locals */
            if (wasm_func_reserve_locals(fn, fn->num_locals + 6U) < 0) goto fail_cleanup;
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

            /* Prologue: allocate linear stack frame & spill address-taken parameters */
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
                for (i = 0; i < num_param_entries; i++) {
                    if (lmap.entries[i].is_memory) {
                        Type *pt = (Type *)lmap.entries[i].type;
                        if (wasm_is_aggregate_type(pt) && lmap.tmp_addr_idx >= 0) {
                            wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                            wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap.fp_local_idx);
                            if (lmap.entries[i].stack_offset > 0) {
                                wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                                wasm_buf_append_sleb128(&fn->body, lmap.entries[i].stack_offset);
                                wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD);
                            }
                            wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_SET);
                            wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap.tmp_addr_idx);
                            int psz = lmap.entries[i].size > 0 ? lmap.entries[i].size : type_size(pt);
                            wasm_emit_memcpy_locals(&fn->body, fn, (uint32_t)lmap.tmp_addr_idx,
                                                    lmap.entries[i].local_idx, psz);
                        } else {
                            wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                            wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap.fp_local_idx);
                            if (lmap.entries[i].stack_offset > 0) {
                                wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                                wasm_buf_append_sleb128(&fn->body, lmap.entries[i].stack_offset);
                                wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD);
                            }
                            wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                            wasm_buf_append_uleb128(&fn->body, lmap.entries[i].local_idx);
                            wasm_emit_typed_store(&fn->body, pt, 0);
                        }
                    }
                }
            }

            /* Lower the function body statement(s) */
            wasm_stmt_block_depth = 0;
            wasm_cur_break_depth = -1;
            wasm_cur_cont_depth = -1;
            if (curr->body) {
                if (wasm_ast_has_goto(curr->body)) {
                    wasm_lower_stmt_cfg(cc, &mod, fn, &lmap, &gmap, curr->body, ret_type);
                } else {
                    wasm_lower_stmt(cc, &mod, fn, &lmap, &gmap, curr->body, ret_type);
                }
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
            } else if (lmap.sret_local_idx >= 0) {
                wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
                wasm_buf_append_uleb128(&fn->body, (uint32_t)lmap.sret_local_idx);
                wasm_buf_append_u8(&fn->body, WASM_OP_RETURN);
            } else {
                wasm_buf_append_u8(&fn->body, WASM_OP_I32_CONST);
                wasm_buf_append_sleb128(&fn->body, 0);
                wasm_buf_append_u8(&fn->body, WASM_OP_RETURN);
            }
            wasm_buf_append_u8(&fn->body, WASM_OP_END);
            wasm_local_map_free(&lmap);
            active_lmap = NULL;
        }
        curr = curr->next;
    }

    /* 5. Synchronize Table 0 elements to cover all imported and defined functions */
    mod.num_table_elements = 0;
    int total_funcs = mod.num_imports + mod.num_funcs;
    int tf;
    for (tf = 0; tf < total_funcs; tf++) {
        if (wasm_module_add_table_element(&mod, (uint32_t)tf) < 0) goto fail_cleanup;
    }

    /* 6. Write final binary .wasm module to file */
    {
        int ret = wasm_module_write_file(&mod, output_file);
        wasm_cur_prog = NULL;
        wasm_global_map_free(&gmap);
        wasm_module_free(&mod);
        return ret;
    }

fail_cleanup:
    wasm_cur_prog = NULL;
    wasm_local_map_free(active_lmap);
    wasm_global_map_free(&gmap);
    wasm_module_free(&mod);
    return -1;
}
