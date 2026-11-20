/*
 * ZCC WebAssembly MVP Binary Emitter
 * Implementation File: src/wasm_emit.c
 * Target: WebAssembly MVP (Binary Format v1)
 */

#include "wasm_emit.h"

void wasm_buf_init(WASMBuffer *buf) {
    if (!buf) return;
    buf->capacity = 128;
    buf->size = 0;
    buf->data = (uint8_t *)malloc(buf->capacity);
}

void wasm_buf_free(WASMBuffer *buf) {
    if (!buf) return;
    if (buf->data) {
        free(buf->data);
        buf->data = NULL;
    }
    buf->size = 0;
    buf->capacity = 0;
}

void wasm_buf_append_u8(WASMBuffer *buf, uint8_t byte) {
    if (!buf) return;
    if (buf->size >= buf->capacity) {
        buf->capacity *= 2;
        buf->data = (uint8_t *)realloc(buf->data, buf->capacity);
    }
    buf->data[buf->size++] = byte;
}

void wasm_buf_append_bytes(WASMBuffer *buf, const uint8_t *bytes, size_t len) {
    if (!buf || !bytes || len == 0) return;
    while (buf->size + len > buf->capacity) {
        buf->capacity *= 2;
        buf->data = (uint8_t *)realloc(buf->data, buf->capacity);
    }
    memcpy(buf->data + buf->size, bytes, len);
    buf->size += len;
}

size_t wasm_encode_uleb128(uint8_t *out, uint32_t val) {
    size_t count = 0;
    do {
        uint8_t byte = val & 0x7F;
        val >>= 7;
        if (val != 0) {
            byte |= 0x80;
        }
        if (out) out[count] = byte;
        count++;
    } while (val != 0);
    return count;
}

size_t wasm_encode_sleb128(uint8_t *out, int32_t val) {
    size_t count = 0;
    int more = 1;
    while (more) {
        uint8_t byte = val & 0x7F;
        val >>= 7;
        if ((val == 0 && (byte & 0x40) == 0) || (val == -1 && (byte & 0x40) != 0)) {
            more = 0;
        } else {
            byte |= 0x80;
        }
        if (out) out[count] = byte;
        count++;
    }
    return count;
}

size_t wasm_encode_sleb128_i64(uint8_t *out, int64_t val) {
    size_t count = 0;
    int more = 1;
    uint64_t u = (uint64_t)val;
    while (more) {
        uint8_t byte = (uint8_t)(u & 0x7FULL);
        int sign_bit = (byte & 0x40) != 0;
        /* Portable 64-bit arithmetic right shift by 7 bits */
        if (u & 0x8000000000000000ULL) {
            u = (u >> 7) | 0xFE00000000000000ULL;
        } else {
            u = (u >> 7);
        }
        if ((u == 0ULL && !sign_bit) || (u == 0xFFFFFFFFFFFFFFFFULL && sign_bit)) {
            more = 0;
        } else {
            byte |= 0x80;
        }
        if (out) out[count] = byte;
        count++;
    }
    return count;
}

void wasm_buf_append_uleb128(WASMBuffer *buf, uint32_t val) {
    uint8_t tmp[16];
    size_t len = wasm_encode_uleb128(tmp, val);
    wasm_buf_append_bytes(buf, tmp, len);
}

void wasm_buf_append_sleb128(WASMBuffer *buf, int32_t val) {
    uint8_t tmp[16];
    size_t len = wasm_encode_sleb128(tmp, val);
    wasm_buf_append_bytes(buf, tmp, len);
}

void wasm_buf_append_sleb128_i64(WASMBuffer *buf, int64_t val) {
    uint8_t tmp[16];
    size_t len = wasm_encode_sleb128_i64(tmp, val);
    wasm_buf_append_bytes(buf, tmp, len);
}

void wasm_buf_append_f32(WASMBuffer *buf, float val) {
    uint8_t tmp[4];
    memcpy(tmp, &val, 4);
    wasm_buf_append_bytes(buf, tmp, 4);
}

void wasm_buf_append_f64(WASMBuffer *buf, double val) {
    uint8_t tmp[8];
    memcpy(tmp, &val, 8);
    wasm_buf_append_bytes(buf, tmp, 8);
}

void wasm_buf_append_memarg(WASMBuffer *buf, uint32_t align, uint32_t offset) {
    wasm_buf_append_uleb128(buf, align);
    wasm_buf_append_uleb128(buf, offset);
}

void wasm_emit_section(WASMBuffer *module_buf, WASMSectionID sec_id, const WASMBuffer *sec_data) {
    if (!module_buf || !sec_data || sec_data->size == 0) return;
    wasm_buf_append_u8(module_buf, (uint8_t)sec_id);
    wasm_buf_append_uleb128(module_buf, (uint32_t)sec_data->size);
    wasm_buf_append_bytes(module_buf, sec_data->data, sec_data->size);
}

static int wasm_grow_array(void **ptr, int *cap, int count, size_t elem_size, int init_cap) {
    if (count < *cap) return 0;
    int new_cap = (*cap > 0) ? (*cap * 2) : init_cap;
    if (new_cap <= count) new_cap = count + init_cap;
    if (new_cap <= 0 || (size_t)new_cap > ((size_t)-1) / elem_size) return -1;
    void *nb = realloc(*ptr, (size_t)new_cap * elem_size);
    if (!nb) return -1;
    memset((uint8_t *)nb + (size_t)(*cap) * elem_size, 0,
           (size_t)(new_cap - *cap) * elem_size);
    *ptr = nb;
    *cap = new_cap;
    return 0;
}

static void emit_op(WASMBuffer *b, uint8_t op) { wasm_buf_append_u8(b, op); }
static void emit_u32(WASMBuffer *b, uint8_t op, uint32_t v) { wasm_buf_append_u8(b, op); wasm_buf_append_uleb128(b, v); }
static void emit_i32(WASMBuffer *b, int32_t v) { wasm_buf_append_u8(b, WASM_OP_I32_CONST); wasm_buf_append_sleb128(b, v); }
static void emit_i64(WASMBuffer *b, int64_t v) { wasm_buf_append_u8(b, WASM_OP_I64_CONST); wasm_buf_append_sleb128_i64(b, v); }
static void emit_mem(WASMBuffer *b, uint8_t op, uint32_t align, uint32_t offset) {
    wasm_buf_append_u8(b, op);
    wasm_buf_append_uleb128(b, align);
    wasm_buf_append_uleb128(b, offset);
}

void wasm_module_init(WasmModule *mod) {
    if (!mod) return;
    memset(mod, 0, sizeof(WasmModule));
    mod->has_memory = 1;
    mod->mem_min_pages = 2; /* 128KB default: static/stack below heap */
    mod->mem_max_pages = 0; /* Unbounded default */
    mod->has_table = 1;
    mod->table_min_elems = 1;
    mod->table_max_elems = 0;
    mod->heap_base_init = 131072U;
    mod->free_head_global_idx = -1;
    mod->has_longjmp_tag = 0;
    mod->longjmp_tag_type_idx = -1;
    /* Global 0: mutable __stack_pointer initialized at 128KB */
    wasm_module_add_global(mod, WASM_TYPE_I32, 1, 131072);
}

void wasm_module_free(WasmModule *mod) {
    if (!mod) return;
    int i;
    for (i = 0; i < mod->num_types; i++) {
        free(mod->types[i].params);
        mod->types[i].params = NULL;
    }
    free(mod->types);
    free(mod->imports);
    for (i = 0; i < mod->num_funcs; i++) {
        free(mod->funcs[i].local_types);
        mod->funcs[i].local_types = NULL;
        wasm_buf_free(&mod->funcs[i].body);
    }
    free(mod->funcs);
    free(mod->exports);
    free(mod->globals);
    for (i = 0; i < mod->num_data_segments; i++) {
        if (mod->data_segments[i].data) {
            free(mod->data_segments[i].data);
            mod->data_segments[i].data = NULL;
        }
    }
    free(mod->data_segments);
    free(mod->table_func_indices);
    memset(mod, 0, sizeof(WasmModule));
    mod->free_head_global_idx = -1;
    mod->longjmp_tag_type_idx = -1;
}

int wasm_local_alloc_entry(WasmLocalMap *map) {
    if (!map) return -1;
    if (wasm_grow_array((void **)&map->entries, &map->cap, map->count, sizeof(WasmLocalEntry), 64) < 0)
        return -1;
    int idx = map->count++;
    memset(&map->entries[idx], 0, sizeof(WasmLocalEntry));
    return idx;
}

void wasm_local_map_free(WasmLocalMap *map) {
    if (!map) return;
    free(map->entries);
    map->entries = NULL;
    map->count = 0;
    map->cap = 0;
}

int wasm_func_reserve_locals(WasmFunction *fn, uint32_t needed) {
    if (!fn) return -1;
    if (needed <= fn->cap_locals) return 0;
    uint32_t new_cap = fn->cap_locals ? fn->cap_locals : 32U;
    while (new_cap < needed) {
        if (new_cap > 0x40000000U) { new_cap = needed; break; }
        new_cap *= 2U;
    }
    uint8_t *nb = (uint8_t *)realloc(fn->local_types, (size_t)new_cap);
    if (!nb) return -1;
    if (new_cap > fn->cap_locals) {
        memset(nb + fn->cap_locals, 0, (size_t)(new_cap - fn->cap_locals));
    }
    fn->local_types = nb;
    fn->cap_locals = new_cap;
    return 0;
}

int wasm_module_add_type(WasmModule *mod, const uint8_t *params, int num_params, const uint8_t *results, int num_results) {
    if (!mod || num_params < 0 || num_results < 0 || num_results > 8) return -1;
    if (num_params > 0 && !params) return -1;
    if (num_results > 0 && !results) return -1;
    int i;
    /* Check for existing duplicate type signature */
    for (i = 0; i < mod->num_types; i++) {
        if (mod->types[i].num_params == num_params && mod->types[i].num_results == num_results) {
            int match = 1;
            if (num_params > 0 && memcmp(mod->types[i].params, params, (size_t)num_params) != 0) match = 0;
            if (num_results > 0 && memcmp(mod->types[i].results, results, (size_t)num_results) != 0) match = 0;
            if (match) return i;
        }
    }
    uint8_t *pbuf = NULL;
    if (num_params > 0) {
        pbuf = (uint8_t *)malloc((size_t)num_params);
        if (!pbuf) return -1;
        memcpy(pbuf, params, (size_t)num_params);
    }
    if (wasm_grow_array((void **)&mod->types, &mod->cap_types, mod->num_types, sizeof(WasmFuncType), 32) < 0) {
        free(pbuf);
        return -1;
    }
    int idx = mod->num_types++;
    WasmFuncType *t = &mod->types[idx];
    memset(t, 0, sizeof(WasmFuncType));
    t->params = pbuf;
    t->cap_params = num_params;
    t->num_params = num_params;
    t->num_results = num_results;
    if (num_results > 0) memcpy(t->results, results, (size_t)num_results);
    return idx;
}

int wasm_module_add_import_func(WasmModule *mod, const char *mod_name, const char *field_name, int type_idx) {
    if (!mod || !mod_name || !field_name || type_idx < 0) return -1;
    int i;
    for (i = 0; i < mod->num_imports; i++) {
        if (strcmp(mod->imports[i].mod_name, mod_name) == 0 &&
            strcmp(mod->imports[i].field_name, field_name) == 0) {
            return i;
        }
    }
    if (wasm_grow_array((void **)&mod->imports, &mod->cap_imports, mod->num_imports, sizeof(WasmImport), 16) < 0)
        return -1;
    int idx = mod->num_imports++;
    WasmImport *imp = &mod->imports[idx];
    memset(imp, 0, sizeof(WasmImport));
    strncpy(imp->mod_name, mod_name, sizeof(imp->mod_name) - 1);
    strncpy(imp->field_name, field_name, sizeof(imp->field_name) - 1);
    imp->kind = WASM_IMPORT_FUNC;
    imp->type_idx = (uint32_t)type_idx;
    return idx;
}

int wasm_module_add_function(WasmModule *mod, const char *name, int type_idx) {
    if (!mod || type_idx < 0) return -1;
    if (wasm_grow_array((void **)&mod->funcs, &mod->cap_funcs, mod->num_funcs, sizeof(WasmFunction), 64) < 0)
        return -1;
    int idx = mod->num_funcs++;
    WasmFunction *fn = &mod->funcs[idx];
    memset(fn, 0, sizeof(WasmFunction));
    if (name) strncpy(fn->name, name, sizeof(fn->name) - 1);
    fn->type_idx = type_idx;
    wasm_buf_init(&fn->body);
    return idx;
}

int wasm_module_find_func_idx(const WasmModule *mod, const char *name) {
    if (!mod || !name) return -1;
    int i;
    /* Check imports first (indices 0 .. num_imports - 1) */
    for (i = 0; i < mod->num_imports; i++) {
        if (strcmp(mod->imports[i].field_name, name) == 0 ||
            (strncmp(name, "__wasi_", 7) == 0 &&
             strcmp(mod->imports[i].mod_name, "wasi_snapshot_preview1") == 0 &&
             strcmp(mod->imports[i].field_name, name + 7) == 0)) {
            return i;
        }
    }
    /* Check defined functions (indices num_imports .. num_imports + num_funcs - 1) */
    for (i = 0; i < mod->num_funcs; i++) {
        if (strcmp(mod->funcs[i].name, name) == 0) {
            return mod->num_imports + i;
        }
    }
    return -1;
}

void wasm_module_add_export(WasmModule *mod, const char *name, WASMExportKind kind, uint32_t index) {
    if (!mod || !name || !name[0]) return;
    int i;
    for (i = 0; i < mod->num_exports; i++) {
        if (strcmp(mod->exports[i].name, name) == 0) {
            mod->exports[i].kind = kind;
            mod->exports[i].index = index;
            return;
        }
    }
    if (wasm_grow_array((void **)&mod->exports, &mod->cap_exports, mod->num_exports, sizeof(WasmExport), 64) < 0)
        return;
    int idx = mod->num_exports++;
    WasmExport *exp = &mod->exports[idx];
    memset(exp, 0, sizeof(WasmExport));
    strncpy(exp->name, name, sizeof(exp->name) - 1);
    exp->kind = kind;
    exp->index = index;
}

int wasm_module_add_global(WasmModule *mod, uint8_t val_type, uint8_t is_mutable, int32_t init_val) {
    if (!mod) return -1;
    if (wasm_grow_array((void **)&mod->globals, &mod->cap_globals, mod->num_globals, sizeof(WasmGlobal), 16) < 0)
        return -1;
    int idx = mod->num_globals++;
    WasmGlobal *g = &mod->globals[idx];
    g->val_type = val_type;
    g->is_mutable = is_mutable;
    g->init_val = init_val;
    return idx;
}

int wasm_module_add_data_segment(WasmModule *mod, uint32_t offset, const uint8_t *data, size_t size) {
    if (!mod || !data || size == 0) return -1;
    uint8_t *dbuf = (uint8_t *)malloc(size);
    if (!dbuf) return -1;
    if (wasm_grow_array((void **)&mod->data_segments, &mod->cap_data_segments, mod->num_data_segments,
                        sizeof(WasmDataSegment), 32) < 0) {
        free(dbuf);
        return -1;
    }
    memcpy(dbuf, data, size);
    int idx = mod->num_data_segments++;
    WasmDataSegment *seg = &mod->data_segments[idx];
    seg->offset = offset;
    seg->size = size;
    seg->data = dbuf;
    return idx;
}

int wasm_module_add_table_element(WasmModule *mod, uint32_t func_idx) {
    if (!mod) return -1;
    if (wasm_grow_array((void **)&mod->table_func_indices, &mod->cap_table_elements, mod->num_table_elements,
                        sizeof(uint32_t), 128) < 0)
        return -1;
    int idx = mod->num_table_elements++;
    mod->table_func_indices[idx] = func_idx;
    if ((uint32_t)mod->num_table_elements > mod->table_min_elems) {
        mod->table_min_elems = (uint32_t)mod->num_table_elements;
    }
    return idx;
}

int wasm_module_configure_layout(WasmModule *mod, uint32_t static_data_end, uint32_t max_pages_override) {
    if (!mod) return -1;
    if (static_data_end < 1024U) static_data_end = 1024U;
    if (static_data_end > 0x7FFEFFF0U) {
        fprintf(stderr, "zcc: wasm static data %u bytes exceeds 0x7FFEFFF0 budget\n", static_data_end);
        return -1;
    }
    uint32_t static_aligned = (static_data_end + 15U) & ~15U;
    uint64_t stack_bytes = 65536ULL;
    if (static_aligned >= 16384U && (max_pages_override == 0 || max_pages_override >= 64U)) {
        stack_bytes = 2097152ULL; /* 2MB stack for large standalone WASI programs */
    }
    uint64_t min_top = (uint64_t)static_aligned + stack_bytes;
    uint32_t min_pages = (uint32_t)((min_top + 65535ULL) / 65536ULL);
    if (min_pages < 2U) min_pages = 2U;
    if (max_pages_override > 65536U) {
        fprintf(stderr, "zcc: --wasm-max-pages=%u exceeds 65536 page limit\n", max_pages_override);
        return -1;
    }
    if (max_pages_override > 0 && min_pages > max_pages_override) {
        fprintf(stderr, "zcc: wasm static+stack requires %u pages, exceeding --wasm-max-pages=%u\n",
                min_pages, max_pages_override);
        return -1;
    }
    if (min_pages > 32767U) {
        fprintf(stderr, "zcc: wasm min_pages %u exceeds 32767 page safety limit\n", min_pages);
        return -1;
    }
    uint32_t stack_top = min_pages * 65536U;
    mod->mem_min_pages = min_pages;
    mod->mem_max_pages = max_pages_override;
    if (mod->num_globals > 0) {
        mod->globals[0].init_val = (int32_t)stack_top;
    } else if (wasm_module_add_global(mod, WASM_TYPE_I32, 1, (int32_t)stack_top) < 0) {
        return -1;
    }
    if (mod->num_globals > 1) {
        mod->globals[1].init_val = (int32_t)stack_top;
    } else if (wasm_module_add_global(mod, WASM_TYPE_I32, 1, (int32_t)stack_top) < 0) {
        return -1;
    }
    mod->heap_base_init = stack_top;
    return 0;
}

#define WASM_ALLOC_MAGIC 0x5A434D41

int wasm_emit_synth_allocator_ex(WasmModule *mod, int synth_malloc, int synth_free,
                                 int synth_calloc, int synth_realloc,
                                 int synth_heap_used, int synth_heap_cap, int export_runtime) {
    if (!mod) return -1;
    if (!synth_malloc && !synth_free && !synth_calloc && !synth_realloc &&
        !synth_heap_used && !synth_heap_cap) return 0;
    int need_malloc = (synth_malloc || synth_calloc || synth_realloc) &&
                      (wasm_module_find_func_idx(mod, "malloc") < 0);
    int need_free = (synth_free || synth_realloc) &&
                    (wasm_module_find_func_idx(mod, "free") < 0);
    if ((need_malloc || need_free) && mod->free_head_global_idx < 0) {
        mod->free_head_global_idx = wasm_module_add_global(mod, WASM_TYPE_I32, 1, 0);
        if (mod->free_head_global_idx < 0) return -1;
    }
    uint32_t fg = (mod->free_head_global_idx >= 0) ? (uint32_t)mod->free_head_global_idx : 0U;

    if (need_malloc) {
        uint8_t m_params[1] = { WASM_TYPE_I32 };
        uint8_t m_res[1] = { WASM_TYPE_I32 };
        int m_tidx = wasm_module_add_type(mod, m_params, 1, m_res, 1);
        int m_fidx = wasm_module_add_function(mod, "malloc", m_tidx);
        if (m_tidx < 0 || m_fidx < 0) return -1;
        uint32_t abs_m_idx = (uint32_t)(mod->num_imports + m_fidx);
        if (wasm_module_add_table_element(mod, abs_m_idx) < 0) return -1;
        if (export_runtime) wasm_module_add_export(mod, "malloc", WASM_EXPORT_FUNC, abs_m_idx);

        WasmFunction *mfn = &mod->funcs[m_fidx];
        mfn->num_params = 1;
        if (wasm_func_reserve_locals(mfn, 10) < 0) return -1;
        for (int li = 0; li < 7; li++) mfn->local_types[li] = WASM_TYPE_I32; /* locals 1..7 */
        mfn->local_types[7] = WASM_TYPE_I64; /* local 8: cur_bytes64 */
        mfn->local_types[8] = WASM_TYPE_I64; /* local 9: new_top64 */
        mfn->local_types[9] = WASM_TYPE_I32; /* local 10: need_pages */
        mfn->num_locals = 10;
        WASMBuffer *b = &mfn->body;

        /* Reject requests > 0x7FFF0000 to avoid signed/32-bit size wraparound */
        emit_u32(b, WASM_OP_LOCAL_GET, 0);
        emit_i32(b, 0x7FFF0000);
        emit_op(b, WASM_OP_I32_GT_U);
        emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
            emit_i32(b, 0);
            emit_op(b, WASM_OP_RETURN);
        emit_op(b, WASM_OP_END);

        /* req = max(16, (size + 15) & -16) + 16 */
        emit_u32(b, WASM_OP_LOCAL_GET, 0);
        emit_i32(b, 15);
        emit_op(b, WASM_OP_I32_ADD);
        emit_i32(b, -16);
        emit_op(b, WASM_OP_I32_AND);
        emit_u32(b, WASM_OP_LOCAL_TEE, 1);
        emit_i32(b, 16);
        emit_op(b, WASM_OP_I32_LT_U);
        emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
            emit_i32(b, 16);
            emit_u32(b, WASM_OP_LOCAL_SET, 1);
        emit_op(b, WASM_OP_END);
        emit_u32(b, WASM_OP_LOCAL_GET, 1);
        emit_i32(b, 16);
        emit_op(b, WASM_OP_I32_ADD);
        emit_u32(b, WASM_OP_LOCAL_SET, 1);

        /* Search address-ordered free list */
        emit_i32(b, 0); emit_u32(b, WASM_OP_LOCAL_SET, 3);
        emit_u32(b, WASM_OP_GLOBAL_GET, fg); emit_u32(b, WASM_OP_LOCAL_SET, 2);
        emit_op(b, WASM_OP_BLOCK); emit_op(b, WASM_TYPE_VOID);
        emit_op(b, WASM_OP_LOOP); emit_op(b, WASM_TYPE_VOID);
            emit_u32(b, WASM_OP_LOCAL_GET, 2);
            emit_op(b, WASM_OP_I32_EQZ);
            emit_u32(b, WASM_OP_BR_IF, 1);

            emit_u32(b, WASM_OP_LOCAL_GET, 2);
            emit_mem(b, WASM_OP_I32_LOAD, 2, 0);
            emit_u32(b, WASM_OP_LOCAL_TEE, 4); /* blk_sz */
            emit_u32(b, WASM_OP_LOCAL_GET, 1); /* req */
            emit_op(b, WASM_OP_I32_GE_U);
            emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
                emit_u32(b, WASM_OP_LOCAL_GET, 4);
                emit_u32(b, WASM_OP_LOCAL_GET, 1);
                emit_op(b, WASM_OP_I32_SUB);
                emit_u32(b, WASM_OP_LOCAL_TEE, 5); /* rem */
                emit_i32(b, 32);
                emit_op(b, WASM_OP_I32_GE_U);
                emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
                    emit_u32(b, WASM_OP_LOCAL_GET, 2);
                    emit_u32(b, WASM_OP_LOCAL_GET, 1);
                    emit_op(b, WASM_OP_I32_ADD);
                    emit_u32(b, WASM_OP_LOCAL_TEE, 6); /* next_blk */
                    emit_u32(b, WASM_OP_LOCAL_GET, 5);
                    emit_mem(b, WASM_OP_I32_STORE, 2, 0);
                    emit_u32(b, WASM_OP_LOCAL_GET, 6);
                    emit_i32(b, 0);
                    emit_mem(b, WASM_OP_I32_STORE, 2, 4);
                    emit_u32(b, WASM_OP_LOCAL_GET, 6);
                    emit_u32(b, WASM_OP_LOCAL_GET, 2);
                    emit_mem(b, WASM_OP_I32_LOAD, 2, 8);
                    emit_mem(b, WASM_OP_I32_STORE, 2, 8);
                    emit_u32(b, WASM_OP_LOCAL_GET, 6);
                    emit_i32(b, WASM_ALLOC_MAGIC);
                    emit_mem(b, WASM_OP_I32_STORE, 2, 12);
                    emit_u32(b, WASM_OP_LOCAL_GET, 2);
                    emit_u32(b, WASM_OP_LOCAL_GET, 1);
                    emit_mem(b, WASM_OP_I32_STORE, 2, 0);
                emit_op(b, WASM_OP_ELSE);
                    emit_u32(b, WASM_OP_LOCAL_GET, 2);
                    emit_mem(b, WASM_OP_I32_LOAD, 2, 8);
                    emit_u32(b, WASM_OP_LOCAL_SET, 6);
                emit_op(b, WASM_OP_END);

                emit_u32(b, WASM_OP_LOCAL_GET, 3);
                emit_op(b, WASM_OP_I32_EQZ);
                emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
                    emit_u32(b, WASM_OP_LOCAL_GET, 6);
                    emit_u32(b, WASM_OP_GLOBAL_SET, fg);
                emit_op(b, WASM_OP_ELSE);
                    emit_u32(b, WASM_OP_LOCAL_GET, 3);
                    emit_u32(b, WASM_OP_LOCAL_GET, 6);
                    emit_mem(b, WASM_OP_I32_STORE, 2, 8);
                emit_op(b, WASM_OP_END);

                emit_u32(b, WASM_OP_LOCAL_GET, 2); emit_i32(b, 1); emit_mem(b, WASM_OP_I32_STORE, 2, 4);
                emit_u32(b, WASM_OP_LOCAL_GET, 2); emit_i32(b, 0); emit_mem(b, WASM_OP_I32_STORE, 2, 8);
                emit_u32(b, WASM_OP_LOCAL_GET, 2); emit_i32(b, WASM_ALLOC_MAGIC); emit_mem(b, WASM_OP_I32_STORE, 2, 12);
                emit_u32(b, WASM_OP_LOCAL_GET, 2); emit_i32(b, 16); emit_op(b, WASM_OP_I32_ADD);
                emit_op(b, WASM_OP_RETURN);
            emit_op(b, WASM_OP_END);

            emit_u32(b, WASM_OP_LOCAL_GET, 2);
            emit_u32(b, WASM_OP_LOCAL_SET, 3);
            emit_u32(b, WASM_OP_LOCAL_GET, 2);
            emit_mem(b, WASM_OP_I32_LOAD, 2, 8);
            emit_u32(b, WASM_OP_LOCAL_SET, 2);
            emit_u32(b, WASM_OP_BR, 0);
        emit_op(b, WASM_OP_END);
        emit_op(b, WASM_OP_END);

        /* Bump from __heap_base (global 1) with overflow + memory.grow check */
        emit_u32(b, WASM_OP_GLOBAL_GET, 1);
        emit_u32(b, WASM_OP_LOCAL_TEE, 7);
        emit_op(b, WASM_OP_I64_EXTEND_I32_U);
        emit_u32(b, WASM_OP_LOCAL_GET, 1);
        emit_op(b, WASM_OP_I64_EXTEND_I32_U);
        emit_op(b, WASM_OP_I64_ADD);
        emit_u32(b, WASM_OP_LOCAL_TEE, 9);
        emit_i64(b, (int64_t)0xFFFFFFFFLL);
        emit_op(b, WASM_OP_I64_GT_U);
        emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
            emit_i32(b, 0);
            emit_op(b, WASM_OP_RETURN);
        emit_op(b, WASM_OP_END);

        emit_op(b, WASM_OP_MEMORY_SIZE); emit_op(b, 0x00);
        emit_op(b, WASM_OP_I64_EXTEND_I32_U);
        emit_i64(b, 16);
        emit_op(b, WASM_OP_I64_SHL);
        emit_u32(b, WASM_OP_LOCAL_TEE, 8);
        emit_u32(b, WASM_OP_LOCAL_GET, 9);
        emit_op(b, WASM_OP_I64_LT_U);
        emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
            emit_u32(b, WASM_OP_LOCAL_GET, 9);
            emit_u32(b, WASM_OP_LOCAL_GET, 8);
            emit_op(b, WASM_OP_I64_SUB);
            emit_i64(b, 65535);
            emit_op(b, WASM_OP_I64_ADD);
            emit_i64(b, 16);
            emit_op(b, WASM_OP_I64_SHR_U);
            emit_op(b, WASM_OP_I32_WRAP_I64);
            emit_u32(b, WASM_OP_LOCAL_SET, 10);
            emit_u32(b, WASM_OP_LOCAL_GET, 10);
            emit_op(b, WASM_OP_MEMORY_GROW); emit_op(b, 0x00);
            emit_i32(b, -1);
            emit_op(b, WASM_OP_I32_EQ);
            emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
                emit_i32(b, 0);
                emit_op(b, WASM_OP_RETURN);
            emit_op(b, WASM_OP_END);
        emit_op(b, WASM_OP_END);

        emit_u32(b, WASM_OP_LOCAL_GET, 9);
        emit_op(b, WASM_OP_I32_WRAP_I64);
        emit_u32(b, WASM_OP_GLOBAL_SET, 1);

        emit_u32(b, WASM_OP_LOCAL_GET, 7); emit_u32(b, WASM_OP_LOCAL_GET, 1); emit_mem(b, WASM_OP_I32_STORE, 2, 0);
        emit_u32(b, WASM_OP_LOCAL_GET, 7); emit_i32(b, 1); emit_mem(b, WASM_OP_I32_STORE, 2, 4);
        emit_u32(b, WASM_OP_LOCAL_GET, 7); emit_i32(b, 0); emit_mem(b, WASM_OP_I32_STORE, 2, 8);
        emit_u32(b, WASM_OP_LOCAL_GET, 7); emit_i32(b, WASM_ALLOC_MAGIC); emit_mem(b, WASM_OP_I32_STORE, 2, 12);
        emit_u32(b, WASM_OP_LOCAL_GET, 7); emit_i32(b, 16); emit_op(b, WASM_OP_I32_ADD);
        emit_op(b, WASM_OP_RETURN);
        emit_op(b, WASM_OP_END);
    }

    if (need_free) {
        uint8_t f_params[1] = { WASM_TYPE_I32 };
        int f_tidx = wasm_module_add_type(mod, f_params, 1, NULL, 0);
        int f_fidx = wasm_module_add_function(mod, "free", f_tidx);
        if (f_tidx < 0 || f_fidx < 0) return -1;
        uint32_t abs_f_idx = (uint32_t)(mod->num_imports + f_fidx);
        if (wasm_module_add_table_element(mod, abs_f_idx) < 0) return -1;
        if (export_runtime) wasm_module_add_export(mod, "free", WASM_EXPORT_FUNC, abs_f_idx);

        WasmFunction *ffn = &mod->funcs[f_fidx];
        ffn->num_params = 1;
        if (wasm_func_reserve_locals(ffn, 8) < 0) return -1;
        for (int li = 0; li < 8; li++) ffn->local_types[li] = WASM_TYPE_I32; /* locals 1..8 */
        ffn->num_locals = 8;
        WASMBuffer *b = &ffn->body;
        uint32_t min_valid_ptr = mod->heap_base_init + 16U;

        /* Guard NULL, below-heap pointers, alignment, past-heap, magic, double-free */
        emit_u32(b, WASM_OP_LOCAL_GET, 0);
        emit_op(b, WASM_OP_I32_EQZ);
        emit_u32(b, WASM_OP_LOCAL_GET, 0);
        emit_i32(b, (int32_t)min_valid_ptr);
        emit_op(b, WASM_OP_I32_LT_U);
        emit_op(b, WASM_OP_I32_OR);
        emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
            emit_op(b, WASM_OP_RETURN);
        emit_op(b, WASM_OP_END);

        emit_u32(b, WASM_OP_LOCAL_GET, 0);
        emit_i32(b, 16);
        emit_op(b, WASM_OP_I32_SUB);
        emit_u32(b, WASM_OP_LOCAL_TEE, 1); /* blk */
        emit_i32(b, 15);
        emit_op(b, WASM_OP_I32_AND);
        emit_u32(b, WASM_OP_LOCAL_GET, 1);
        emit_u32(b, WASM_OP_GLOBAL_GET, 1);
        emit_op(b, WASM_OP_I32_GE_U);
        emit_op(b, WASM_OP_I32_OR);
        emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
            emit_op(b, WASM_OP_RETURN);
        emit_op(b, WASM_OP_END);

        emit_u32(b, WASM_OP_LOCAL_GET, 1);
        emit_mem(b, WASM_OP_I32_LOAD, 2, 12);
        emit_i32(b, WASM_ALLOC_MAGIC);
        emit_op(b, WASM_OP_I32_NE);
        emit_u32(b, WASM_OP_LOCAL_GET, 1);
        emit_mem(b, WASM_OP_I32_LOAD, 2, 4);
        emit_op(b, WASM_OP_I32_EQZ);
        emit_op(b, WASM_OP_I32_OR);
        emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
            emit_op(b, WASM_OP_RETURN);
        emit_op(b, WASM_OP_END);

        emit_u32(b, WASM_OP_LOCAL_GET, 1);
        emit_mem(b, WASM_OP_I32_LOAD, 2, 0);
        emit_u32(b, WASM_OP_LOCAL_TEE, 2); /* blk_sz */
        emit_i32(b, 32);
        emit_op(b, WASM_OP_I32_LT_U);
        emit_u32(b, WASM_OP_LOCAL_GET, 2);
        emit_i32(b, 15);
        emit_op(b, WASM_OP_I32_AND);
        emit_op(b, WASM_OP_I32_OR);
        emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
            emit_op(b, WASM_OP_RETURN);
        emit_op(b, WASM_OP_END);

        /* blk[4] = 0 (free) */
        emit_u32(b, WASM_OP_LOCAL_GET, 1);
        emit_i32(b, 0);
        emit_mem(b, WASM_OP_I32_STORE, 2, 4);

        /* Find insertion point in address-ordered free list */
        emit_i32(b, 0); emit_u32(b, WASM_OP_LOCAL_SET, 6); /* prev_prev = 0 */
        emit_i32(b, 0); emit_u32(b, WASM_OP_LOCAL_SET, 3); /* prev = 0 */
        emit_u32(b, WASM_OP_GLOBAL_GET, fg); emit_u32(b, WASM_OP_LOCAL_SET, 4); /* curr */
        emit_op(b, WASM_OP_BLOCK); emit_op(b, WASM_TYPE_VOID);
        emit_op(b, WASM_OP_LOOP); emit_op(b, WASM_TYPE_VOID);
            emit_u32(b, WASM_OP_LOCAL_GET, 4);
            emit_op(b, WASM_OP_I32_EQZ);
            emit_u32(b, WASM_OP_LOCAL_GET, 4);
            emit_u32(b, WASM_OP_LOCAL_GET, 1);
            emit_op(b, WASM_OP_I32_GE_U);
            emit_op(b, WASM_OP_I32_OR);
            emit_u32(b, WASM_OP_BR_IF, 1);
            emit_u32(b, WASM_OP_LOCAL_GET, 3); emit_u32(b, WASM_OP_LOCAL_SET, 6);
            emit_u32(b, WASM_OP_LOCAL_GET, 4); emit_u32(b, WASM_OP_LOCAL_SET, 3);
            emit_u32(b, WASM_OP_LOCAL_GET, 4); emit_mem(b, WASM_OP_I32_LOAD, 2, 8);
            emit_u32(b, WASM_OP_LOCAL_SET, 4);
            emit_u32(b, WASM_OP_BR, 0);
        emit_op(b, WASM_OP_END);
        emit_op(b, WASM_OP_END);

        /* Link blk between prev and curr */
        emit_u32(b, WASM_OP_LOCAL_GET, 1);
        emit_u32(b, WASM_OP_LOCAL_GET, 4);
        emit_mem(b, WASM_OP_I32_STORE, 2, 8);
        emit_u32(b, WASM_OP_LOCAL_GET, 3);
        emit_op(b, WASM_OP_I32_EQZ);
        emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
            emit_u32(b, WASM_OP_LOCAL_GET, 1);
            emit_u32(b, WASM_OP_GLOBAL_SET, fg);
        emit_op(b, WASM_OP_ELSE);
            emit_u32(b, WASM_OP_LOCAL_GET, 3);
            emit_u32(b, WASM_OP_LOCAL_GET, 1);
            emit_mem(b, WASM_OP_I32_STORE, 2, 8);
        emit_op(b, WASM_OP_END);

        /* Forward coalesce with curr */
        emit_u32(b, WASM_OP_LOCAL_GET, 4);
        emit_op(b, WASM_OP_I32_EQZ);
        emit_op(b, WASM_OP_I32_EQZ); /* curr != 0 */
        emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
            emit_u32(b, WASM_OP_LOCAL_GET, 1);
            emit_u32(b, WASM_OP_LOCAL_GET, 2);
            emit_op(b, WASM_OP_I32_ADD);
            emit_u32(b, WASM_OP_LOCAL_GET, 4);
            emit_op(b, WASM_OP_I32_EQ);
            emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
                emit_u32(b, WASM_OP_LOCAL_GET, 2);
                emit_u32(b, WASM_OP_LOCAL_GET, 4);
                emit_mem(b, WASM_OP_I32_LOAD, 2, 0);
                emit_op(b, WASM_OP_I32_ADD);
                emit_u32(b, WASM_OP_LOCAL_SET, 2);
                emit_u32(b, WASM_OP_LOCAL_GET, 1);
                emit_u32(b, WASM_OP_LOCAL_GET, 2);
                emit_mem(b, WASM_OP_I32_STORE, 2, 0);
                emit_u32(b, WASM_OP_LOCAL_GET, 1);
                emit_u32(b, WASM_OP_LOCAL_GET, 4);
                emit_mem(b, WASM_OP_I32_LOAD, 2, 8);
                emit_mem(b, WASM_OP_I32_STORE, 2, 8);
                emit_u32(b, WASM_OP_LOCAL_GET, 4);
                emit_i32(b, 0);
                emit_mem(b, WASM_OP_I32_STORE, 2, 12);
            emit_op(b, WASM_OP_END);
        emit_op(b, WASM_OP_END);

        /* Backward coalesce with prev, track tail/tail_prev */
        emit_u32(b, WASM_OP_LOCAL_GET, 1); emit_u32(b, WASM_OP_LOCAL_SET, 7); /* tail = blk */
        emit_u32(b, WASM_OP_LOCAL_GET, 3); emit_u32(b, WASM_OP_LOCAL_SET, 8); /* tail_prev = prev */
        emit_u32(b, WASM_OP_LOCAL_GET, 3);
        emit_op(b, WASM_OP_I32_EQZ);
        emit_op(b, WASM_OP_I32_EQZ);
        emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
            emit_u32(b, WASM_OP_LOCAL_GET, 3);
            emit_mem(b, WASM_OP_I32_LOAD, 2, 0);
            emit_u32(b, WASM_OP_LOCAL_TEE, 5); /* prev_sz */
            emit_u32(b, WASM_OP_LOCAL_GET, 3);
            emit_op(b, WASM_OP_I32_ADD);
            emit_u32(b, WASM_OP_LOCAL_GET, 1);
            emit_op(b, WASM_OP_I32_EQ);
            emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
                emit_u32(b, WASM_OP_LOCAL_GET, 3);
                emit_u32(b, WASM_OP_LOCAL_GET, 5);
                emit_u32(b, WASM_OP_LOCAL_GET, 2);
                emit_op(b, WASM_OP_I32_ADD);
                emit_mem(b, WASM_OP_I32_STORE, 2, 0);
                emit_u32(b, WASM_OP_LOCAL_GET, 3);
                emit_u32(b, WASM_OP_LOCAL_GET, 1);
                emit_mem(b, WASM_OP_I32_LOAD, 2, 8);
                emit_mem(b, WASM_OP_I32_STORE, 2, 8);
                emit_u32(b, WASM_OP_LOCAL_GET, 1);
                emit_i32(b, 0);
                emit_mem(b, WASM_OP_I32_STORE, 2, 12);
                emit_u32(b, WASM_OP_LOCAL_GET, 3); emit_u32(b, WASM_OP_LOCAL_SET, 7);
                emit_u32(b, WASM_OP_LOCAL_GET, 6); emit_u32(b, WASM_OP_LOCAL_SET, 8);
            emit_op(b, WASM_OP_END);
        emit_op(b, WASM_OP_END);

        /* Reclaim tail block at __heap_base frontier */
        emit_u32(b, WASM_OP_LOCAL_GET, 7);
        emit_mem(b, WASM_OP_I32_LOAD, 2, 8);
        emit_op(b, WASM_OP_I32_EQZ);
        emit_u32(b, WASM_OP_LOCAL_GET, 7);
        emit_u32(b, WASM_OP_LOCAL_GET, 7);
        emit_mem(b, WASM_OP_I32_LOAD, 2, 0);
        emit_op(b, WASM_OP_I32_ADD);
        emit_u32(b, WASM_OP_GLOBAL_GET, 1);
        emit_op(b, WASM_OP_I32_EQ);
        emit_op(b, WASM_OP_I32_AND);
        emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
            emit_u32(b, WASM_OP_LOCAL_GET, 8);
            emit_op(b, WASM_OP_I32_EQZ);
            emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
                emit_i32(b, 0);
                emit_u32(b, WASM_OP_GLOBAL_SET, fg);
            emit_op(b, WASM_OP_ELSE);
                emit_u32(b, WASM_OP_LOCAL_GET, 8);
                emit_i32(b, 0);
                emit_mem(b, WASM_OP_I32_STORE, 2, 8);
            emit_op(b, WASM_OP_END);
            emit_u32(b, WASM_OP_LOCAL_GET, 7);
            emit_i32(b, 0);
            emit_mem(b, WASM_OP_I32_STORE, 2, 12);
            emit_u32(b, WASM_OP_LOCAL_GET, 7);
            emit_u32(b, WASM_OP_GLOBAL_SET, 1);
        emit_op(b, WASM_OP_END);

        emit_op(b, WASM_OP_RETURN);
        emit_op(b, WASM_OP_END);
    }

    if (synth_calloc && wasm_module_find_func_idx(mod, "calloc") < 0) {
        int malloc_idx = wasm_module_find_func_idx(mod, "malloc");
        if (malloc_idx < 0) return -1;
        uint8_t c_params[2] = { WASM_TYPE_I32, WASM_TYPE_I32 };
        uint8_t c_res[1] = { WASM_TYPE_I32 };
        int c_tidx = wasm_module_add_type(mod, c_params, 2, c_res, 1);
        int c_fidx = wasm_module_add_function(mod, "calloc", c_tidx);
        if (c_tidx < 0 || c_fidx < 0) return -1;
        uint32_t abs_c_idx = (uint32_t)(mod->num_imports + c_fidx);
        if (wasm_module_add_table_element(mod, abs_c_idx) < 0) return -1;
        if (export_runtime) wasm_module_add_export(mod, "calloc", WASM_EXPORT_FUNC, abs_c_idx);

        WasmFunction *cfn = &mod->funcs[c_fidx];
        cfn->num_params = 2;
        if (wasm_func_reserve_locals(cfn, 4) < 0) return -1;
        cfn->local_types[0] = WASM_TYPE_I32; /* local 2: total */
        cfn->local_types[1] = WASM_TYPE_I32; /* local 3: ptr */
        cfn->local_types[2] = WASM_TYPE_I32; /* local 4: i */
        cfn->local_types[3] = WASM_TYPE_I64; /* local 5: total64 */
        cfn->num_locals = 4;
        WASMBuffer *b = &cfn->body;

        /* Overflow check: total64 = (u64)nmemb * (u64)size <= 0x7FFF0000 */
        emit_u32(b, WASM_OP_LOCAL_GET, 0);
        emit_op(b, WASM_OP_I64_EXTEND_I32_U);
        emit_u32(b, WASM_OP_LOCAL_GET, 1);
        emit_op(b, WASM_OP_I64_EXTEND_I32_U);
        emit_op(b, WASM_OP_I64_MUL);
        emit_u32(b, WASM_OP_LOCAL_TEE, 5);
        emit_i64(b, (int64_t)0x7FFF0000LL);
        emit_op(b, WASM_OP_I64_GT_U);
        emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
            emit_i32(b, 0);
            emit_op(b, WASM_OP_RETURN);
        emit_op(b, WASM_OP_END);

        emit_u32(b, WASM_OP_LOCAL_GET, 5);
        emit_op(b, WASM_OP_I32_WRAP_I64);
        emit_u32(b, WASM_OP_LOCAL_TEE, 2);
        emit_u32(b, WASM_OP_CALL, (uint32_t)malloc_idx);
        emit_u32(b, WASM_OP_LOCAL_TEE, 3);
        emit_op(b, WASM_OP_I32_EQZ);
        emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
            emit_i32(b, 0);
            emit_op(b, WASM_OP_RETURN);
        emit_op(b, WASM_OP_END);

        /* Zero-fill 4 bytes at a time, then trailing bytes (malloc blocks may come from free list) */
        emit_i32(b, 0); emit_u32(b, WASM_OP_LOCAL_SET, 4);
        emit_op(b, WASM_OP_BLOCK); emit_op(b, WASM_TYPE_VOID);
        emit_op(b, WASM_OP_LOOP); emit_op(b, WASM_TYPE_VOID);
            emit_u32(b, WASM_OP_LOCAL_GET, 2);
            emit_u32(b, WASM_OP_LOCAL_GET, 4);
            emit_op(b, WASM_OP_I32_SUB);
            emit_i32(b, 4);
            emit_op(b, WASM_OP_I32_LT_U);
            emit_u32(b, WASM_OP_BR_IF, 1);
            emit_u32(b, WASM_OP_LOCAL_GET, 3);
            emit_u32(b, WASM_OP_LOCAL_GET, 4);
            emit_op(b, WASM_OP_I32_ADD);
            emit_i32(b, 0);
            emit_mem(b, WASM_OP_I32_STORE, 2, 0);
            emit_u32(b, WASM_OP_LOCAL_GET, 4);
            emit_i32(b, 4);
            emit_op(b, WASM_OP_I32_ADD);
            emit_u32(b, WASM_OP_LOCAL_SET, 4);
            emit_u32(b, WASM_OP_BR, 0);
        emit_op(b, WASM_OP_END);
        emit_op(b, WASM_OP_END);

        emit_op(b, WASM_OP_BLOCK); emit_op(b, WASM_TYPE_VOID);
        emit_op(b, WASM_OP_LOOP); emit_op(b, WASM_TYPE_VOID);
            emit_u32(b, WASM_OP_LOCAL_GET, 4);
            emit_u32(b, WASM_OP_LOCAL_GET, 2);
            emit_op(b, WASM_OP_I32_GE_U);
            emit_u32(b, WASM_OP_BR_IF, 1);
            emit_u32(b, WASM_OP_LOCAL_GET, 3);
            emit_u32(b, WASM_OP_LOCAL_GET, 4);
            emit_op(b, WASM_OP_I32_ADD);
            emit_i32(b, 0);
            emit_mem(b, WASM_OP_I32_STORE8, 0, 0);
            emit_u32(b, WASM_OP_LOCAL_GET, 4);
            emit_i32(b, 1);
            emit_op(b, WASM_OP_I32_ADD);
            emit_u32(b, WASM_OP_LOCAL_SET, 4);
            emit_u32(b, WASM_OP_BR, 0);
        emit_op(b, WASM_OP_END);
        emit_op(b, WASM_OP_END);

        emit_u32(b, WASM_OP_LOCAL_GET, 3);
        emit_op(b, WASM_OP_RETURN);
        emit_op(b, WASM_OP_END);
    }

    if (synth_realloc && wasm_module_find_func_idx(mod, "realloc") < 0) {
        int malloc_idx = wasm_module_find_func_idx(mod, "malloc");
        int free_idx = wasm_module_find_func_idx(mod, "free");
        if (malloc_idx < 0 || free_idx < 0) return -1;
        uint8_t r_params[2] = { WASM_TYPE_I32, WASM_TYPE_I32 };
        uint8_t r_res[1] = { WASM_TYPE_I32 };
        int r_tidx = wasm_module_add_type(mod, r_params, 2, r_res, 1);
        int r_fidx = wasm_module_add_function(mod, "realloc", r_tidx);
        if (r_tidx < 0 || r_fidx < 0) return -1;
        uint32_t abs_r_idx = (uint32_t)(mod->num_imports + r_fidx);
        if (wasm_module_add_table_element(mod, abs_r_idx) < 0) return -1;
        if (export_runtime) wasm_module_add_export(mod, "realloc", WASM_EXPORT_FUNC, abs_r_idx);

        WasmFunction *rfn = &mod->funcs[r_fidx];
        rfn->num_params = 2;
        if (wasm_func_reserve_locals(rfn, 6) < 0) return -1;
        for (int li = 0; li < 6; li++) rfn->local_types[li] = WASM_TYPE_I32; /* locals 2..7 */
        rfn->num_locals = 6;
        WASMBuffer *b = &rfn->body;
        uint32_t min_valid_ptr = mod->heap_base_init + 16U;

        /* realloc(NULL, n) == malloc(n) */
        emit_u32(b, WASM_OP_LOCAL_GET, 0);
        emit_op(b, WASM_OP_I32_EQZ);
        emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
            emit_u32(b, WASM_OP_LOCAL_GET, 1);
            emit_u32(b, WASM_OP_CALL, (uint32_t)malloc_idx);
            emit_op(b, WASM_OP_RETURN);
        emit_op(b, WASM_OP_END);

        /* realloc(p, 0) == free(p), return NULL */
        emit_u32(b, WASM_OP_LOCAL_GET, 1);
        emit_op(b, WASM_OP_I32_EQZ);
        emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
            emit_u32(b, WASM_OP_LOCAL_GET, 0);
            emit_u32(b, WASM_OP_CALL, (uint32_t)free_idx);
            emit_i32(b, 0);
            emit_op(b, WASM_OP_RETURN);
        emit_op(b, WASM_OP_END);

        /* Bounds & header integrity validation matching free() */
        emit_u32(b, WASM_OP_LOCAL_GET, 1);
        emit_i32(b, 0x7FFF0000);
        emit_op(b, WASM_OP_I32_GT_U);
        emit_u32(b, WASM_OP_LOCAL_GET, 0);
        emit_i32(b, (int32_t)min_valid_ptr);
        emit_op(b, WASM_OP_I32_LT_U);
        emit_op(b, WASM_OP_I32_OR);
        emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
            emit_i32(b, 0);
            emit_op(b, WASM_OP_RETURN);
        emit_op(b, WASM_OP_END);

        emit_u32(b, WASM_OP_LOCAL_GET, 0);
        emit_i32(b, 16);
        emit_op(b, WASM_OP_I32_SUB);
        emit_u32(b, WASM_OP_LOCAL_TEE, 2); /* blk */
        emit_i32(b, 15);
        emit_op(b, WASM_OP_I32_AND);
        emit_u32(b, WASM_OP_LOCAL_GET, 2);
        emit_u32(b, WASM_OP_GLOBAL_GET, 1);
        emit_op(b, WASM_OP_I32_GE_U);
        emit_op(b, WASM_OP_I32_OR);
        emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
            emit_i32(b, 0);
            emit_op(b, WASM_OP_RETURN);
        emit_op(b, WASM_OP_END);

        emit_u32(b, WASM_OP_LOCAL_GET, 2);
        emit_mem(b, WASM_OP_I32_LOAD, 2, 12);
        emit_i32(b, WASM_ALLOC_MAGIC);
        emit_op(b, WASM_OP_I32_NE);
        emit_u32(b, WASM_OP_LOCAL_GET, 2);
        emit_mem(b, WASM_OP_I32_LOAD, 2, 4);
        emit_op(b, WASM_OP_I32_EQZ);
        emit_op(b, WASM_OP_I32_OR);
        emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
            emit_i32(b, 0);
            emit_op(b, WASM_OP_RETURN);
        emit_op(b, WASM_OP_END);

        emit_u32(b, WASM_OP_LOCAL_GET, 2);
        emit_mem(b, WASM_OP_I32_LOAD, 2, 0);
        emit_u32(b, WASM_OP_LOCAL_TEE, 7); /* blk_sz */
        emit_i32(b, 32);
        emit_op(b, WASM_OP_I32_LT_U);
        emit_u32(b, WASM_OP_LOCAL_GET, 7);
        emit_i32(b, 15);
        emit_op(b, WASM_OP_I32_AND);
        emit_op(b, WASM_OP_I32_OR);
        emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
            emit_i32(b, 0);
            emit_op(b, WASM_OP_RETURN);
        emit_op(b, WASM_OP_END);

        /* old_sz = blk_sz - 16; fast path if new_size <= old_sz */
        emit_u32(b, WASM_OP_LOCAL_GET, 7);
        emit_i32(b, 16);
        emit_op(b, WASM_OP_I32_SUB);
        emit_u32(b, WASM_OP_LOCAL_TEE, 3);
        emit_u32(b, WASM_OP_LOCAL_GET, 1);
        emit_op(b, WASM_OP_I32_GE_U);
        emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
            emit_u32(b, WASM_OP_LOCAL_GET, 0);
            emit_op(b, WASM_OP_RETURN);
        emit_op(b, WASM_OP_END);

        /* Allocate replacement; preserve ptr untouched if OOM */
        emit_u32(b, WASM_OP_LOCAL_GET, 1);
        emit_u32(b, WASM_OP_CALL, (uint32_t)malloc_idx);
        emit_u32(b, WASM_OP_LOCAL_TEE, 4);
        emit_op(b, WASM_OP_I32_EQZ);
        emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
            emit_i32(b, 0);
            emit_op(b, WASM_OP_RETURN);
        emit_op(b, WASM_OP_END);

        /* Copy old_sz bytes (always 16B aligned multiple >= 16) in 4B words */
        emit_i32(b, 0); emit_u32(b, WASM_OP_LOCAL_SET, 6);
        emit_op(b, WASM_OP_BLOCK); emit_op(b, WASM_TYPE_VOID);
        emit_op(b, WASM_OP_LOOP); emit_op(b, WASM_TYPE_VOID);
            emit_u32(b, WASM_OP_LOCAL_GET, 6);
            emit_u32(b, WASM_OP_LOCAL_GET, 3);
            emit_op(b, WASM_OP_I32_GE_U);
            emit_u32(b, WASM_OP_BR_IF, 1);
            emit_u32(b, WASM_OP_LOCAL_GET, 4);
            emit_u32(b, WASM_OP_LOCAL_GET, 6);
            emit_op(b, WASM_OP_I32_ADD);
            emit_u32(b, WASM_OP_LOCAL_GET, 0);
            emit_u32(b, WASM_OP_LOCAL_GET, 6);
            emit_op(b, WASM_OP_I32_ADD);
            emit_mem(b, WASM_OP_I32_LOAD, 2, 0);
            emit_mem(b, WASM_OP_I32_STORE, 2, 0);
            emit_u32(b, WASM_OP_LOCAL_GET, 6);
            emit_i32(b, 4);
            emit_op(b, WASM_OP_I32_ADD);
            emit_u32(b, WASM_OP_LOCAL_SET, 6);
            emit_u32(b, WASM_OP_BR, 0);
        emit_op(b, WASM_OP_END);
        emit_op(b, WASM_OP_END);

        emit_u32(b, WASM_OP_LOCAL_GET, 0);
        emit_u32(b, WASM_OP_CALL, (uint32_t)free_idx);
        emit_u32(b, WASM_OP_LOCAL_GET, 4);
        emit_op(b, WASM_OP_RETURN);
        emit_op(b, WASM_OP_END);
    }

    if (synth_heap_used && wasm_module_find_func_idx(mod, "zcc_heap_used") < 0) {
        uint8_t u_res[1] = { WASM_TYPE_I32 };
        int u_tidx = wasm_module_add_type(mod, NULL, 0, u_res, 1);
        int u_fidx = wasm_module_add_function(mod, "zcc_heap_used", u_tidx);
        if (u_tidx < 0 || u_fidx < 0) return -1;
        uint32_t abs_u_idx = (uint32_t)(mod->num_imports + u_fidx);
        if (wasm_module_add_table_element(mod, abs_u_idx) < 0) return -1;
        if (export_runtime) wasm_module_add_export(mod, "zcc_heap_used", WASM_EXPORT_FUNC, abs_u_idx);

        WasmFunction *ufn = &mod->funcs[u_fidx];
        ufn->num_params = 0;
        WASMBuffer *b = &ufn->body;
        if (mod->free_head_global_idx < 0) {
            emit_u32(b, WASM_OP_GLOBAL_GET, 1);
            emit_i32(b, (int32_t)mod->heap_base_init);
            emit_op(b, WASM_OP_I32_SUB);
            emit_op(b, WASM_OP_RETURN);
            emit_op(b, WASM_OP_END);
        } else {
            if (wasm_func_reserve_locals(ufn, 3) < 0) return -1;
            ufn->local_types[0] = WASM_TYPE_I32; /* local 0: free_sum */
            ufn->local_types[1] = WASM_TYPE_I32; /* local 1: curr */
            ufn->local_types[2] = WASM_TYPE_I32; /* local 2: next */
            ufn->num_locals = 3;
            emit_i32(b, 0); emit_u32(b, WASM_OP_LOCAL_SET, 0);
            emit_u32(b, WASM_OP_GLOBAL_GET, fg); emit_u32(b, WASM_OP_LOCAL_SET, 1);
            emit_op(b, WASM_OP_BLOCK); emit_op(b, WASM_TYPE_VOID);
            emit_op(b, WASM_OP_LOOP); emit_op(b, WASM_TYPE_VOID);
                emit_u32(b, WASM_OP_LOCAL_GET, 1);
                emit_op(b, WASM_OP_I32_EQZ);
                emit_u32(b, WASM_OP_LOCAL_GET, 1);
                emit_i32(b, (int32_t)mod->heap_base_init);
                emit_op(b, WASM_OP_I32_LT_U);
                emit_op(b, WASM_OP_I32_OR);
                emit_u32(b, WASM_OP_LOCAL_GET, 1);
                emit_u32(b, WASM_OP_GLOBAL_GET, 1);
                emit_op(b, WASM_OP_I32_GE_U);
                emit_op(b, WASM_OP_I32_OR);
                emit_u32(b, WASM_OP_BR_IF, 1);

                emit_u32(b, WASM_OP_LOCAL_GET, 1);
                emit_mem(b, WASM_OP_I32_LOAD, 2, 12);
                emit_i32(b, WASM_ALLOC_MAGIC);
                emit_op(b, WASM_OP_I32_NE);
                emit_u32(b, WASM_OP_LOCAL_GET, 1);
                emit_mem(b, WASM_OP_I32_LOAD, 2, 4);
                emit_op(b, WASM_OP_I32_EQZ);
                emit_op(b, WASM_OP_I32_EQZ); /* blk[4] != 0 means in-use -> abort */
                emit_op(b, WASM_OP_I32_OR);
                emit_u32(b, WASM_OP_BR_IF, 1);

                emit_u32(b, WASM_OP_LOCAL_GET, 0);
                emit_u32(b, WASM_OP_LOCAL_GET, 1);
                emit_mem(b, WASM_OP_I32_LOAD, 2, 0);
                emit_op(b, WASM_OP_I32_ADD);
                emit_u32(b, WASM_OP_LOCAL_SET, 0);

                emit_u32(b, WASM_OP_LOCAL_GET, 1);
                emit_mem(b, WASM_OP_I32_LOAD, 2, 8);
                emit_u32(b, WASM_OP_LOCAL_TEE, 2);
                emit_op(b, WASM_OP_I32_EQZ);
                emit_op(b, WASM_OP_I32_EQZ);
                emit_u32(b, WASM_OP_LOCAL_GET, 2);
                emit_u32(b, WASM_OP_LOCAL_GET, 1);
                emit_op(b, WASM_OP_I32_LE_U);
                emit_op(b, WASM_OP_I32_AND);
                emit_u32(b, WASM_OP_BR_IF, 1);

                emit_u32(b, WASM_OP_LOCAL_GET, 2);
                emit_u32(b, WASM_OP_LOCAL_SET, 1);
                emit_u32(b, WASM_OP_BR, 0);
            emit_op(b, WASM_OP_END);
            emit_op(b, WASM_OP_END);

            emit_u32(b, WASM_OP_GLOBAL_GET, 1);
            emit_i32(b, (int32_t)mod->heap_base_init);
            emit_op(b, WASM_OP_I32_SUB);
            emit_u32(b, WASM_OP_LOCAL_GET, 0);
            emit_op(b, WASM_OP_I32_SUB);
            emit_op(b, WASM_OP_RETURN);
            emit_op(b, WASM_OP_END);
        }
    }

    if (synth_heap_cap && wasm_module_find_func_idx(mod, "zcc_heap_capacity") < 0) {
        uint8_t k_res[1] = { WASM_TYPE_I32 };
        int k_tidx = wasm_module_add_type(mod, NULL, 0, k_res, 1);
        int k_fidx = wasm_module_add_function(mod, "zcc_heap_capacity", k_tidx);
        if (k_tidx < 0 || k_fidx < 0) return -1;
        uint32_t abs_k_idx = (uint32_t)(mod->num_imports + k_fidx);
        if (wasm_module_add_table_element(mod, abs_k_idx) < 0) return -1;
        if (export_runtime) wasm_module_add_export(mod, "zcc_heap_capacity", WASM_EXPORT_FUNC, abs_k_idx);

        WasmFunction *kfn = &mod->funcs[k_fidx];
        kfn->num_params = 0;
        WASMBuffer *b = &kfn->body;
        emit_op(b, WASM_OP_MEMORY_SIZE); emit_op(b, 0x00);
        emit_op(b, WASM_OP_I64_EXTEND_I32_U);
        emit_i64(b, 16);
        emit_op(b, WASM_OP_I64_SHL);
        emit_i32(b, (int32_t)mod->heap_base_init);
        emit_op(b, WASM_OP_I64_EXTEND_I32_U);
        emit_op(b, WASM_OP_I64_SUB);
        emit_op(b, WASM_OP_I32_WRAP_I64);
        emit_op(b, WASM_OP_RETURN);
        emit_op(b, WASM_OP_END);
    }

    return 0;
}

int wasm_emit_synth_allocator(WasmModule *mod, int synth_malloc, int synth_free, int export_runtime) {
    return wasm_emit_synth_allocator_ex(mod, synth_malloc, synth_free, 0, 0, 0, 0, export_runtime);
}

static int emit_wasi_rw(WasmModule *mod, const char *name, int imp_idx, int export_runtime) {
    uint8_t rw_params[3] = { WASM_TYPE_I32, WASM_TYPE_I32, WASM_TYPE_I32 };
    uint8_t rw_res[1] = { WASM_TYPE_I32 };
    int tidx = wasm_module_add_type(mod, rw_params, 3, rw_res, 1);
    int fidx = wasm_module_add_function(mod, name, tidx);
    if (tidx < 0 || fidx < 0) return -1;
    uint32_t abs_idx = (uint32_t)(mod->num_imports + fidx);
    if (wasm_module_add_table_element(mod, abs_idx) < 0) return -1;
    if (export_runtime) wasm_module_add_export(mod, name, WASM_EXPORT_FUNC, abs_idx);

    WasmFunction *fn = &mod->funcs[fidx];
    fn->num_params = 3;
    if (wasm_func_reserve_locals(fn, 3) < 0) return -1;
    fn->local_types[0] = WASM_TYPE_I32; /* local 3: frame */
    fn->local_types[1] = WASM_TYPE_I32; /* local 4: err */
    fn->local_types[2] = WASM_TYPE_I32; /* local 5: nbytes */
    fn->num_locals = 3;
    WASMBuffer *b = &fn->body;

    /* Guard negative count or NULL buf with count > 0 */
    emit_u32(b, WASM_OP_LOCAL_GET, 2);
    emit_i32(b, 0);
    emit_op(b, WASM_OP_I32_LT_S);
    emit_u32(b, WASM_OP_LOCAL_GET, 1);
    emit_op(b, WASM_OP_I32_EQZ);
    emit_u32(b, WASM_OP_LOCAL_GET, 2);
    emit_i32(b, 0);
    emit_op(b, WASM_OP_I32_GT_S);
    emit_op(b, WASM_OP_I32_AND);
    emit_op(b, WASM_OP_I32_OR);
    emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
        emit_i32(b, -1);
        emit_op(b, WASM_OP_RETURN);
    emit_op(b, WASM_OP_END);

    /* Stack-allocate 32-byte scratch frame */
    emit_u32(b, WASM_OP_GLOBAL_GET, 0);
    emit_i32(b, 32);
    emit_op(b, WASM_OP_I32_SUB);
    emit_u32(b, WASM_OP_LOCAL_TEE, 3);
    emit_u32(b, WASM_OP_GLOBAL_SET, 0);

    emit_u32(b, WASM_OP_LOCAL_GET, 3);
    emit_u32(b, WASM_OP_LOCAL_GET, 1);
    emit_mem(b, WASM_OP_I32_STORE, 2, 0);

    emit_u32(b, WASM_OP_LOCAL_GET, 3);
    emit_u32(b, WASM_OP_LOCAL_GET, 2);
    emit_mem(b, WASM_OP_I32_STORE, 2, 4);

    emit_u32(b, WASM_OP_LOCAL_GET, 0);
    emit_u32(b, WASM_OP_LOCAL_GET, 3);
    emit_i32(b, 1);
    emit_u32(b, WASM_OP_LOCAL_GET, 3);
    emit_i32(b, 16);
    emit_op(b, WASM_OP_I32_ADD);
    emit_u32(b, WASM_OP_CALL, (uint32_t)imp_idx);
    emit_u32(b, WASM_OP_LOCAL_SET, 4);

    emit_u32(b, WASM_OP_LOCAL_GET, 3);
    emit_mem(b, WASM_OP_I32_LOAD, 2, 16);
    emit_u32(b, WASM_OP_LOCAL_SET, 5);

    /* Restore stack pointer */
    emit_u32(b, WASM_OP_LOCAL_GET, 3);
    emit_i32(b, 32);
    emit_op(b, WASM_OP_I32_ADD);
    emit_u32(b, WASM_OP_GLOBAL_SET, 0);

    emit_u32(b, WASM_OP_LOCAL_GET, 4);
    emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
        emit_i32(b, -1);
        emit_op(b, WASM_OP_RETURN);
    emit_op(b, WASM_OP_END);

    emit_u32(b, WASM_OP_LOCAL_GET, 5);
    emit_op(b, WASM_OP_RETURN);
    emit_op(b, WASM_OP_END);
    return 0;
}

int wasm_emit_synth_wasi(WasmModule *mod, int fd_write_idx, int fd_read_idx, int proc_exit_idx,
                         int need_puts, int need_putchar, int need_write, int need_read,
                         int need_exit, int need_start, int export_runtime) {
    if (!mod) return -1;

    if (need_puts && fd_write_idx >= 0) {
        uint8_t p_params[1] = { WASM_TYPE_I32 };
        uint8_t p_res[1] = { WASM_TYPE_I32 };
        int p_tidx = wasm_module_add_type(mod, p_params, 1, p_res, 1);
        int p_fidx = wasm_module_add_function(mod, "puts", p_tidx);
        if (p_tidx < 0 || p_fidx < 0) return -1;
        uint32_t abs_p_idx = (uint32_t)(mod->num_imports + p_fidx);
        if (wasm_module_add_table_element(mod, abs_p_idx) < 0) return -1;
        if (export_runtime) wasm_module_add_export(mod, "puts", WASM_EXPORT_FUNC, abs_p_idx);

        WasmFunction *pfn = &mod->funcs[p_fidx];
        pfn->num_params = 1;
        if (wasm_func_reserve_locals(pfn, 3) < 0) return -1;
        pfn->local_types[0] = WASM_TYPE_I32; /* local 1: len */
        pfn->local_types[1] = WASM_TYPE_I32; /* local 2: frame */
        pfn->local_types[2] = WASM_TYPE_I32; /* local 3: err */
        pfn->num_locals = 3;
        WASMBuffer *b = &pfn->body;

        emit_u32(b, WASM_OP_LOCAL_GET, 0);
        emit_op(b, WASM_OP_I32_EQZ);
        emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
            emit_i32(b, -1);
            emit_op(b, WASM_OP_RETURN);
        emit_op(b, WASM_OP_END);

        emit_i32(b, 0);
        emit_u32(b, WASM_OP_LOCAL_SET, 1);
        emit_op(b, WASM_OP_BLOCK); emit_op(b, WASM_TYPE_VOID);
        emit_op(b, WASM_OP_LOOP); emit_op(b, WASM_TYPE_VOID);
            emit_u32(b, WASM_OP_LOCAL_GET, 0);
            emit_u32(b, WASM_OP_LOCAL_GET, 1);
            emit_op(b, WASM_OP_I32_ADD);
            emit_mem(b, WASM_OP_I32_LOAD8_U, 0, 0);
            emit_op(b, WASM_OP_I32_EQZ);
            emit_u32(b, WASM_OP_BR_IF, 1);
            emit_u32(b, WASM_OP_LOCAL_GET, 1);
            emit_i32(b, 1);
            emit_op(b, WASM_OP_I32_ADD);
            emit_u32(b, WASM_OP_LOCAL_SET, 1);
            emit_u32(b, WASM_OP_BR, 0);
        emit_op(b, WASM_OP_END);
        emit_op(b, WASM_OP_END);

        /* Allocate 32-byte reentrant stack scratch frame */
        emit_u32(b, WASM_OP_GLOBAL_GET, 0);
        emit_i32(b, 32);
        emit_op(b, WASM_OP_I32_SUB);
        emit_u32(b, WASM_OP_LOCAL_TEE, 2);
        emit_u32(b, WASM_OP_GLOBAL_SET, 0);

        emit_u32(b, WASM_OP_LOCAL_GET, 2); emit_u32(b, WASM_OP_LOCAL_GET, 0); emit_mem(b, WASM_OP_I32_STORE, 2, 0);
        emit_u32(b, WASM_OP_LOCAL_GET, 2); emit_u32(b, WASM_OP_LOCAL_GET, 1); emit_mem(b, WASM_OP_I32_STORE, 2, 4);
        emit_u32(b, WASM_OP_LOCAL_GET, 2); emit_i32(b, 10); emit_mem(b, WASM_OP_I32_STORE8, 0, 20);
        emit_u32(b, WASM_OP_LOCAL_GET, 2);
        emit_u32(b, WASM_OP_LOCAL_GET, 2); emit_i32(b, 20); emit_op(b, WASM_OP_I32_ADD);
        emit_mem(b, WASM_OP_I32_STORE, 2, 8);
        emit_u32(b, WASM_OP_LOCAL_GET, 2); emit_i32(b, 1); emit_mem(b, WASM_OP_I32_STORE, 2, 12);

        emit_i32(b, 1);
        emit_u32(b, WASM_OP_LOCAL_GET, 2);
        emit_i32(b, 2);
        emit_u32(b, WASM_OP_LOCAL_GET, 2); emit_i32(b, 16); emit_op(b, WASM_OP_I32_ADD);
        emit_u32(b, WASM_OP_CALL, (uint32_t)fd_write_idx);
        emit_u32(b, WASM_OP_LOCAL_SET, 3);

        emit_u32(b, WASM_OP_LOCAL_GET, 2); emit_i32(b, 32); emit_op(b, WASM_OP_I32_ADD);
        emit_u32(b, WASM_OP_GLOBAL_SET, 0);

        emit_u32(b, WASM_OP_LOCAL_GET, 3);
        emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
            emit_i32(b, -1);
            emit_op(b, WASM_OP_RETURN);
        emit_op(b, WASM_OP_END);
        emit_i32(b, 0);
        emit_op(b, WASM_OP_RETURN);
        emit_op(b, WASM_OP_END);
    }

    if (need_putchar && fd_write_idx >= 0) {
        uint8_t pc_params[1] = { WASM_TYPE_I32 };
        uint8_t pc_res[1] = { WASM_TYPE_I32 };
        int pc_tidx = wasm_module_add_type(mod, pc_params, 1, pc_res, 1);
        int pc_fidx = wasm_module_add_function(mod, "putchar", pc_tidx);
        if (pc_tidx < 0 || pc_fidx < 0) return -1;
        uint32_t abs_pc_idx = (uint32_t)(mod->num_imports + pc_fidx);
        if (wasm_module_add_table_element(mod, abs_pc_idx) < 0) return -1;
        if (export_runtime) wasm_module_add_export(mod, "putchar", WASM_EXPORT_FUNC, abs_pc_idx);

        WasmFunction *pcfn = &mod->funcs[pc_fidx];
        pcfn->num_params = 1;
        if (wasm_func_reserve_locals(pcfn, 2) < 0) return -1;
        pcfn->local_types[0] = WASM_TYPE_I32; /* local 1: frame */
        pcfn->local_types[1] = WASM_TYPE_I32; /* local 2: err */
        pcfn->num_locals = 2;
        WASMBuffer *b = &pcfn->body;

        emit_u32(b, WASM_OP_GLOBAL_GET, 0);
        emit_i32(b, 32);
        emit_op(b, WASM_OP_I32_SUB);
        emit_u32(b, WASM_OP_LOCAL_TEE, 1);
        emit_u32(b, WASM_OP_GLOBAL_SET, 0);

        emit_u32(b, WASM_OP_LOCAL_GET, 1);
        emit_u32(b, WASM_OP_LOCAL_GET, 0);
        emit_mem(b, WASM_OP_I32_STORE8, 0, 20);
        emit_u32(b, WASM_OP_LOCAL_GET, 1);
        emit_u32(b, WASM_OP_LOCAL_GET, 1); emit_i32(b, 20); emit_op(b, WASM_OP_I32_ADD);
        emit_mem(b, WASM_OP_I32_STORE, 2, 0);
        emit_u32(b, WASM_OP_LOCAL_GET, 1); emit_i32(b, 1); emit_mem(b, WASM_OP_I32_STORE, 2, 4);

        emit_i32(b, 1);
        emit_u32(b, WASM_OP_LOCAL_GET, 1);
        emit_i32(b, 1);
        emit_u32(b, WASM_OP_LOCAL_GET, 1); emit_i32(b, 16); emit_op(b, WASM_OP_I32_ADD);
        emit_u32(b, WASM_OP_CALL, (uint32_t)fd_write_idx);
        emit_u32(b, WASM_OP_LOCAL_SET, 2);

        emit_u32(b, WASM_OP_LOCAL_GET, 1); emit_i32(b, 32); emit_op(b, WASM_OP_I32_ADD);
        emit_u32(b, WASM_OP_GLOBAL_SET, 0);

        emit_u32(b, WASM_OP_LOCAL_GET, 2);
        emit_op(b, WASM_OP_IF); emit_op(b, WASM_TYPE_VOID);
            emit_i32(b, -1);
            emit_op(b, WASM_OP_RETURN);
        emit_op(b, WASM_OP_END);
        emit_u32(b, WASM_OP_LOCAL_GET, 0);
        emit_i32(b, 255);
        emit_op(b, WASM_OP_I32_AND);
        emit_op(b, WASM_OP_RETURN);
        emit_op(b, WASM_OP_END);
    }

    if (need_write && fd_write_idx >= 0) {
        if (emit_wasi_rw(mod, "write", fd_write_idx, export_runtime) < 0) return -1;
    }
    if (need_read && fd_read_idx >= 0) {
        if (emit_wasi_rw(mod, "read", fd_read_idx, export_runtime) < 0) return -1;
    }

    if (need_exit && proc_exit_idx >= 0) {
        uint8_t ex_params[1] = { WASM_TYPE_I32 };
        int ex_tidx = wasm_module_add_type(mod, ex_params, 1, NULL, 0);
        int ex_fidx = wasm_module_add_function(mod, "exit", ex_tidx);
        if (ex_tidx < 0 || ex_fidx < 0) return -1;
        uint32_t abs_ex_idx = (uint32_t)(mod->num_imports + ex_fidx);
        if (wasm_module_add_table_element(mod, abs_ex_idx) < 0) return -1;
        if (export_runtime) wasm_module_add_export(mod, "exit", WASM_EXPORT_FUNC, abs_ex_idx);

        WasmFunction *exfn = &mod->funcs[ex_fidx];
        exfn->num_params = 1;
        emit_u32(&exfn->body, WASM_OP_LOCAL_GET, 0);
        emit_u32(&exfn->body, WASM_OP_CALL, (uint32_t)proc_exit_idx);
        emit_op(&exfn->body, WASM_OP_UNREACHABLE);
        emit_op(&exfn->body, WASM_OP_END);
    }

    if (need_start) {
        int st_tidx = wasm_module_add_type(mod, NULL, 0, NULL, 0);
        int st_fidx = wasm_module_add_function(mod, "_start", st_tidx);
        if (st_tidx < 0 || st_fidx < 0) return -1;
        uint32_t abs_st_idx = (uint32_t)(mod->num_imports + st_fidx);
        if (wasm_module_add_table_element(mod, abs_st_idx) < 0) return -1;
        wasm_module_add_export(mod, "_start", WASM_EXPORT_FUNC, abs_st_idx);

        WasmFunction *stfn = &mod->funcs[st_fidx];
        stfn->num_params = 0;

        int main_idx = wasm_module_find_func_idx(mod, "main");
        if (main_idx >= 0) {
            int main_tidx = -1;
            if (main_idx < mod->num_imports) main_tidx = (int)mod->imports[main_idx].type_idx;
            else if (main_idx - mod->num_imports < mod->num_funcs) main_tidx = mod->funcs[main_idx - mod->num_imports].type_idx;

            if (main_tidx >= 0 && main_tidx < mod->num_types) {
                int p;
                for (p = 0; p < mod->types[main_tidx].num_params; p++) {
                    emit_i32(&stfn->body, 0);
                }
                emit_u32(&stfn->body, WASM_OP_CALL, (uint32_t)main_idx);

                if (mod->types[main_tidx].num_results > 0) {
                    if (proc_exit_idx >= 0) {
                        emit_u32(&stfn->body, WASM_OP_CALL, (uint32_t)proc_exit_idx);
                        emit_op(&stfn->body, WASM_OP_UNREACHABLE);
                    } else {
                        emit_op(&stfn->body, WASM_OP_DROP);
                        emit_op(&stfn->body, WASM_OP_RETURN);
                    }
                } else {
                    if (proc_exit_idx >= 0) {
                        emit_i32(&stfn->body, 0);
                        emit_u32(&stfn->body, WASM_OP_CALL, (uint32_t)proc_exit_idx);
                        emit_op(&stfn->body, WASM_OP_UNREACHABLE);
                    } else {
                        emit_op(&stfn->body, WASM_OP_RETURN);
                    }
                }
            }
        }
        emit_op(&stfn->body, WASM_OP_END);
    }

    return 0;
}

int wasm_module_emit_binary(const WasmModule *mod, WASMBuffer *out) {
    if (!mod || !out) return -1;
    wasm_buf_init(out);

    /* 1. Header: Magic \0asm + Version 1 */
    uint8_t magic_header[8] = { 0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00 };
    wasm_buf_append_bytes(out, magic_header, 8);

    /* 2. Type Section (1) */
    if (mod->num_types > 0) {
        WASMBuffer type_sec;
        wasm_buf_init(&type_sec);
        wasm_buf_append_uleb128(&type_sec, (uint32_t)mod->num_types);
        int i, j;
        for (i = 0; i < mod->num_types; i++) {
            const WasmFuncType *t = &mod->types[i];
            wasm_buf_append_u8(&type_sec, WASM_TYPE_FUNC);
            wasm_buf_append_uleb128(&type_sec, (uint32_t)t->num_params);
            for (j = 0; j < t->num_params; j++) wasm_buf_append_u8(&type_sec, t->params[j]);
            wasm_buf_append_uleb128(&type_sec, (uint32_t)t->num_results);
            for (j = 0; j < t->num_results; j++) wasm_buf_append_u8(&type_sec, t->results[j]);
        }
        wasm_emit_section(out, WASM_SEC_TYPE, &type_sec);
        wasm_buf_free(&type_sec);
    }

    /* 3. Import Section (2) */
    if (mod->num_imports > 0) {
        WASMBuffer import_sec;
        wasm_buf_init(&import_sec);
        wasm_buf_append_uleb128(&import_sec, (uint32_t)mod->num_imports);
        int i;
        for (i = 0; i < mod->num_imports; i++) {
            const WasmImport *imp = &mod->imports[i];
            size_t mlen = strlen(imp->mod_name);
            wasm_buf_append_uleb128(&import_sec, (uint32_t)mlen);
            wasm_buf_append_bytes(&import_sec, (const uint8_t *)imp->mod_name, mlen);

            size_t flen = strlen(imp->field_name);
            wasm_buf_append_uleb128(&import_sec, (uint32_t)flen);
            wasm_buf_append_bytes(&import_sec, (const uint8_t *)imp->field_name, flen);

            wasm_buf_append_u8(&import_sec, (uint8_t)imp->kind);
            if (imp->kind == WASM_IMPORT_FUNC) {
                wasm_buf_append_uleb128(&import_sec, imp->type_idx);
            }
        }
        wasm_emit_section(out, WASM_SEC_IMPORT, &import_sec);
        wasm_buf_free(&import_sec);
    }

    /* 4. Function Section (3) */
    if (mod->num_funcs > 0) {
        WASMBuffer func_sec;
        wasm_buf_init(&func_sec);
        wasm_buf_append_uleb128(&func_sec, (uint32_t)mod->num_funcs);
        int i;
        for (i = 0; i < mod->num_funcs; i++) {
            wasm_buf_append_uleb128(&func_sec, (uint32_t)mod->funcs[i].type_idx);
        }
        wasm_emit_section(out, WASM_SEC_FUNCTION, &func_sec);
        wasm_buf_free(&func_sec);
    }

    /* 4. Table Section (4) */
    if (mod->has_table && mod->num_table_elements > 0) {
        WASMBuffer table_sec;
        wasm_buf_init(&table_sec);
        wasm_buf_append_uleb128(&table_sec, 1); /* 1 table */
        wasm_buf_append_u8(&table_sec, 0x70); /* anyfunc / funcref element type */
        if (mod->table_max_elems > 0) {
            wasm_buf_append_u8(&table_sec, 0x01); /* min & max */
            wasm_buf_append_uleb128(&table_sec, mod->table_min_elems);
            wasm_buf_append_uleb128(&table_sec, mod->table_max_elems);
        } else {
            wasm_buf_append_u8(&table_sec, 0x00); /* min only */
            uint32_t min_elems = mod->num_table_elements > 0 ? (uint32_t)mod->num_table_elements : 1;
            wasm_buf_append_uleb128(&table_sec, min_elems);
        }
        wasm_emit_section(out, WASM_SEC_TABLE, &table_sec);
        wasm_buf_free(&table_sec);
    }

    /* 5. Memory Section (5) */
    if (mod->has_memory) {
        WASMBuffer mem_sec;
        wasm_buf_init(&mem_sec);
        wasm_buf_append_uleb128(&mem_sec, 1);
        if (mod->mem_max_pages > 0) {
            wasm_buf_append_u8(&mem_sec, 0x01); /* min and max */
            wasm_buf_append_uleb128(&mem_sec, mod->mem_min_pages);
            wasm_buf_append_uleb128(&mem_sec, mod->mem_max_pages);
        } else {
            wasm_buf_append_u8(&mem_sec, 0x00); /* min only */
            wasm_buf_append_uleb128(&mem_sec, mod->mem_min_pages ? mod->mem_min_pages : 2);
        }
        wasm_emit_section(out, WASM_SEC_MEMORY, &mem_sec);
        wasm_buf_free(&mem_sec);
    }

    /* 5b. Tag Section (13) — Standard WebAssembly Exception Handling (only when setjmp/longjmp used) */
    if (mod->has_longjmp_tag && mod->longjmp_tag_type_idx >= 0) {
        WASMBuffer tag_sec;
        wasm_buf_init(&tag_sec);
        wasm_buf_append_uleb128(&tag_sec, 1); /* tag 0: (i32) -> () */
        wasm_buf_append_u8(&tag_sec, 0x00);   /* attribute = exception */
        wasm_buf_append_uleb128(&tag_sec, (uint32_t)mod->longjmp_tag_type_idx);
        wasm_emit_section(out, WASM_SEC_TAG, &tag_sec);
        wasm_buf_free(&tag_sec);
    }

    /* 6. Global Section (6) */
    if (mod->num_globals > 0) {
        WASMBuffer glob_sec;
        wasm_buf_init(&glob_sec);
        wasm_buf_append_uleb128(&glob_sec, (uint32_t)mod->num_globals);
        int i;
        for (i = 0; i < mod->num_globals; i++) {
            const WasmGlobal *g = &mod->globals[i];
            wasm_buf_append_u8(&glob_sec, g->val_type ? g->val_type : WASM_TYPE_I32);
            wasm_buf_append_u8(&glob_sec, g->is_mutable ? 0x01 : 0x00);
            wasm_buf_append_u8(&glob_sec, WASM_OP_I32_CONST);
            wasm_buf_append_sleb128(&glob_sec, g->init_val);
            wasm_buf_append_u8(&glob_sec, WASM_OP_END);
        }
        wasm_emit_section(out, WASM_SEC_GLOBAL, &glob_sec);
        wasm_buf_free(&glob_sec);
    }

    /* 7. Export Section (7) */
    if (mod->num_exports > 0 || mod->has_memory) {
        WASMBuffer export_sec;
        wasm_buf_init(&export_sec);
        uint32_t total_exports = (uint32_t)mod->num_exports + (mod->has_memory ? 1 : 0);
        wasm_buf_append_uleb128(&export_sec, total_exports);
        int i;
        for (i = 0; i < mod->num_exports; i++) {
            const WasmExport *exp = &mod->exports[i];
            size_t nlen = strlen(exp->name);
            wasm_buf_append_uleb128(&export_sec, (uint32_t)nlen);
            wasm_buf_append_bytes(&export_sec, (const uint8_t *)exp->name, nlen);
            wasm_buf_append_u8(&export_sec, (uint8_t)exp->kind);
            wasm_buf_append_uleb128(&export_sec, exp->index);
        }
        if (mod->has_memory) {
            const char *mname = "memory";
            wasm_buf_append_uleb128(&export_sec, (uint32_t)strlen(mname));
            wasm_buf_append_bytes(&export_sec, (const uint8_t *)mname, strlen(mname));
            wasm_buf_append_u8(&export_sec, WASM_EXPORT_MEM);
            wasm_buf_append_uleb128(&export_sec, 0);
        }
        wasm_emit_section(out, WASM_SEC_EXPORT, &export_sec);
        wasm_buf_free(&export_sec);
    }

    /* 8. Element Section (9) */
    if (mod->has_table && mod->num_table_elements > 0) {
        WASMBuffer elem_sec;
        wasm_buf_init(&elem_sec);
        wasm_buf_append_uleb128(&elem_sec, 1); /* 1 element segment */
        wasm_buf_append_uleb128(&elem_sec, 0); /* table 0 */
        wasm_buf_append_u8(&elem_sec, WASM_OP_I32_CONST);
        wasm_buf_append_sleb128(&elem_sec, 0); /* offset 0 */
        wasm_buf_append_u8(&elem_sec, WASM_OP_END);
        wasm_buf_append_uleb128(&elem_sec, (uint32_t)mod->num_table_elements);
        int i;
        for (i = 0; i < mod->num_table_elements; i++) {
            wasm_buf_append_uleb128(&elem_sec, mod->table_func_indices[i]);
        }
        wasm_emit_section(out, WASM_SEC_ELEMENT, &elem_sec);
        wasm_buf_free(&elem_sec);
    }

    /* 9. Code Section (10) */
    if (mod->num_funcs > 0) {
        WASMBuffer code_sec;
        wasm_buf_init(&code_sec);
        wasm_buf_append_uleb128(&code_sec, (uint32_t)mod->num_funcs);
        int i;
        for (i = 0; i < mod->num_funcs; i++) {
            const WasmFunction *fn = &mod->funcs[i];
            WASMBuffer fn_body;
            wasm_buf_init(&fn_body);

            /* Local declarations: group consecutive locals of same type */
            if (fn->num_locals > 0) {
                wasm_buf_append_uleb128(&fn_body, fn->num_locals);
                uint32_t l;
                for (l = 0; l < fn->num_locals; l++) {
                    wasm_buf_append_uleb128(&fn_body, 1); /* count 1 */
                    wasm_buf_append_u8(&fn_body, fn->local_types[l] ? fn->local_types[l] : WASM_TYPE_I32);
                }
            } else {
                wasm_buf_append_uleb128(&fn_body, 0); /* 0 local decl groups */
            }

            /* Function instructions */
            if (fn->body.size > 0) {
                wasm_buf_append_bytes(&fn_body, fn->body.data, fn->body.size);
            } else {
                wasm_buf_append_u8(&fn_body, WASM_OP_END);
            }

            wasm_buf_append_uleb128(&code_sec, (uint32_t)fn_body.size);
            wasm_buf_append_bytes(&code_sec, fn_body.data, fn_body.size);
            wasm_buf_free(&fn_body);
        }
        wasm_emit_section(out, WASM_SEC_CODE, &code_sec);
        wasm_buf_free(&code_sec);
    }

    /* 10. Data Section (11) */
    if (mod->num_data_segments > 0) {
        WASMBuffer data_sec;
        wasm_buf_init(&data_sec);
        wasm_buf_append_uleb128(&data_sec, (uint32_t)mod->num_data_segments);
        int i;
        for (i = 0; i < mod->num_data_segments; i++) {
            const WasmDataSegment *seg = &mod->data_segments[i];
            wasm_buf_append_u8(&data_sec, 0x00); /* active segment in memory 0 */
            wasm_buf_append_u8(&data_sec, WASM_OP_I32_CONST);
            wasm_buf_append_sleb128(&data_sec, (int32_t)seg->offset);
            wasm_buf_append_u8(&data_sec, WASM_OP_END);
            wasm_buf_append_uleb128(&data_sec, (uint32_t)seg->size);
            wasm_buf_append_bytes(&data_sec, seg->data, seg->size);
        }
        wasm_emit_section(out, WASM_SEC_DATA, &data_sec);
        wasm_buf_free(&data_sec);
    }

    /* 11. Standard W3C "name" Custom Section (0) for readable DevTools/Wasmtime traces */
    if (mod->num_imports + mod->num_funcs > 0) {
        WASMBuffer name_sec;
        WASMBuffer fn_sub;
        const char *sec_title = "name";
        uint32_t named_count = 0;
        int i;
        wasm_buf_init(&name_sec);
        wasm_buf_init(&fn_sub);
        for (i = 0; i < mod->num_imports; i++) {
            if (mod->imports[i].field_name[0]) named_count++;
        }
        for (i = 0; i < mod->num_funcs; i++) {
            if (mod->funcs[i].name[0]) named_count++;
        }
        if (named_count > 0) {
            wasm_buf_append_uleb128(&fn_sub, named_count);
            for (i = 0; i < mod->num_imports; i++) {
                if (!mod->imports[i].field_name[0]) continue;
                size_t flen = strlen(mod->imports[i].field_name);
                wasm_buf_append_uleb128(&fn_sub, (uint32_t)i);
                wasm_buf_append_uleb128(&fn_sub, (uint32_t)flen);
                wasm_buf_append_bytes(&fn_sub, (const uint8_t *)mod->imports[i].field_name, flen);
            }
            for (i = 0; i < mod->num_funcs; i++) {
                if (!mod->funcs[i].name[0]) continue;
                size_t flen = strlen(mod->funcs[i].name);
                wasm_buf_append_uleb128(&fn_sub, (uint32_t)(mod->num_imports + i));
                wasm_buf_append_uleb128(&fn_sub, (uint32_t)flen);
                wasm_buf_append_bytes(&fn_sub, (const uint8_t *)mod->funcs[i].name, flen);
            }
            wasm_buf_append_uleb128(&name_sec, (uint32_t)strlen(sec_title));
            wasm_buf_append_bytes(&name_sec, (const uint8_t *)sec_title, strlen(sec_title));
            wasm_buf_append_u8(&name_sec, 0x01); /* subsection 1: function names */
            wasm_buf_append_uleb128(&name_sec, (uint32_t)fn_sub.size);
            wasm_buf_append_bytes(&name_sec, fn_sub.data, fn_sub.size);
            wasm_emit_section(out, WASM_SEC_CUSTOM, &name_sec);
        }
        wasm_buf_free(&fn_sub);
        wasm_buf_free(&name_sec);
    }

    /* 12. Proof-Carrying Custom Section (0): .zcc.prove */
    {
        WASMBuffer prove_sec;
        const char *sec_name = ".zcc.prove";
        const char *payload = "{\"compiler\":\"zcc-omega\",\"prove\":\"ZCC-PROVE-11..30\",\"simd128\":true,\"quad_isa\":true}";
        size_t nlen = strlen(sec_name);
        size_t plen = strlen(payload);
        wasm_buf_init(&prove_sec);
        wasm_buf_append_uleb128(&prove_sec, (uint32_t)nlen);
        wasm_buf_append_bytes(&prove_sec, (const uint8_t *)sec_name, nlen);
        wasm_buf_append_bytes(&prove_sec, (const uint8_t *)payload, plen);
        wasm_emit_section(out, WASM_SEC_CUSTOM, &prove_sec);
        wasm_buf_free(&prove_sec);
    }

    return 0;
}

int wasm_module_write_file(const WasmModule *mod, const char *filename) {
    if (!mod || !filename) return -1;
    WASMBuffer out;
    if (wasm_module_emit_binary(mod, &out) != 0) return -1;
    FILE *f = fopen(filename, "wb");
    if (!f) {
        wasm_buf_free(&out);
        return -1;
    }
    fwrite(out.data, 1, out.size, f);
    fclose(f);
    wasm_buf_free(&out);
    return 0;
}

int zcc_emit_wasm_module_to_file(const char *filename, const WASMBuffer *custom_code_body) {
    WasmModule mod;
    wasm_module_init(&mod);
    uint8_t params[2] = { WASM_TYPE_I32, WASM_TYPE_I32 };
    uint8_t results[1] = { WASM_TYPE_I32 };
    int t_idx = wasm_module_add_type(&mod, params, 2, results, 1);
    int fn_idx = wasm_module_add_function(&mod, "main", t_idx);
    if (t_idx < 0 || fn_idx < 0) {
        wasm_module_free(&mod);
        return -1;
    }
    wasm_module_add_export(&mod, "main", WASM_EXPORT_FUNC, (uint32_t)fn_idx);

    WasmFunction *fn = &mod.funcs[fn_idx];
    if (custom_code_body && custom_code_body->size > 0) {
        wasm_buf_append_bytes(&fn->body, custom_code_body->data, custom_code_body->size);
    } else {
        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
        wasm_buf_append_uleb128(&fn->body, 0);
        wasm_buf_append_u8(&fn->body, WASM_OP_LOCAL_GET);
        wasm_buf_append_uleb128(&fn->body, 1);
        wasm_buf_append_u8(&fn->body, WASM_OP_I32_ADD);
        wasm_buf_append_u8(&fn->body, WASM_OP_END);
    }

    int res = wasm_module_write_file(&mod, filename);
    wasm_module_free(&mod);
    return res;
}
