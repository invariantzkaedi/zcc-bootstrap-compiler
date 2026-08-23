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

void wasm_module_init(WasmModule *mod) {
    if (!mod) return;
    memset(mod, 0, sizeof(WasmModule));
    mod->has_memory = 1;
    mod->mem_min_pages = 2; /* 128KB default: 64KB static/heap + 64KB stack */
    mod->mem_max_pages = 0; /* Unbounded default */
    mod->has_table = 1;
    mod->table_min_elems = 1;
    mod->table_max_elems = 0;
    /* Add default mutable __stack_pointer global at 128KB (131072) */
    wasm_module_add_global(mod, WASM_TYPE_I32, 1, 131072);
}

void wasm_module_free(WasmModule *mod) {
    if (!mod) return;
    int i;
    for (i = 0; i < mod->num_funcs; i++) {
        wasm_buf_free(&mod->funcs[i].body);
    }
    for (i = 0; i < mod->num_data_segments; i++) {
        if (mod->data_segments[i].data) {
            free(mod->data_segments[i].data);
            mod->data_segments[i].data = NULL;
        }
    }
    memset(mod, 0, sizeof(WasmModule));
}

int wasm_module_add_type(WasmModule *mod, const uint8_t *params, int num_params, const uint8_t *results, int num_results) {
    if (!mod) return -1;
    int i;
    /* Check for existing duplicate type signature */
    for (i = 0; i < mod->num_types; i++) {
        if (mod->types[i].num_params == num_params && mod->types[i].num_results == num_results) {
            int match = 1;
            if (num_params > 0 && memcmp(mod->types[i].params, params, num_params) != 0) match = 0;
            if (num_results > 0 && memcmp(mod->types[i].results, results, num_results) != 0) match = 0;
            if (match) return i;
        }
    }
    if (mod->num_types >= 64) return -1;
    int idx = mod->num_types++;
    WasmFuncType *t = &mod->types[idx];
    t->num_params = num_params;
    if (num_params > 0) memcpy(t->params, params, num_params);
    t->num_results = num_results;
    if (num_results > 0) memcpy(t->results, results, num_results);
    return idx;
}

int wasm_module_add_import_func(WasmModule *mod, const char *mod_name, const char *field_name, int type_idx) {
    if (!mod || !mod_name || !field_name || mod->num_imports >= 32) return -1;
    int i;
    for (i = 0; i < mod->num_imports; i++) {
        if (strcmp(mod->imports[i].mod_name, mod_name) == 0 &&
            strcmp(mod->imports[i].field_name, field_name) == 0) {
            return i;
        }
    }
    int idx = mod->num_imports++;
    WasmImport *imp = &mod->imports[idx];
    strncpy(imp->mod_name, mod_name, sizeof(imp->mod_name) - 1);
    strncpy(imp->field_name, field_name, sizeof(imp->field_name) - 1);
    imp->kind = WASM_IMPORT_FUNC;
    imp->type_idx = (uint32_t)type_idx;
    return idx;
}

int wasm_module_add_function(WasmModule *mod, const char *name, int type_idx) {
    if (!mod || mod->num_funcs >= 128) return -1;
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
        if (strcmp(mod->imports[i].field_name, name) == 0) {
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
    if (!mod || !name || mod->num_exports >= 128) return;
    int idx = mod->num_exports++;
    WasmExport *exp = &mod->exports[idx];
    strncpy(exp->name, name, sizeof(exp->name) - 1);
    exp->kind = kind;
    exp->index = index;
}

int wasm_module_add_global(WasmModule *mod, uint8_t val_type, uint8_t is_mutable, int32_t init_val) {
    if (!mod || mod->num_globals >= 16) return -1;
    int idx = mod->num_globals++;
    WasmGlobal *g = &mod->globals[idx];
    g->val_type = val_type;
    g->is_mutable = is_mutable;
    g->init_val = init_val;
    return idx;
}

int wasm_module_add_data_segment(WasmModule *mod, uint32_t offset, const uint8_t *data, size_t size) {
    if (!mod || !data || size == 0 || mod->num_data_segments >= 32) return -1;
    int idx = mod->num_data_segments++;
    WasmDataSegment *seg = &mod->data_segments[idx];
    seg->offset = offset;
    seg->size = size;
    seg->data = (uint8_t *)malloc(size);
    memcpy(seg->data, data, size);
    return idx;
}

int wasm_module_add_table_element(WasmModule *mod, uint32_t func_idx) {
    if (!mod || mod->num_table_elements >= 256) return -1;
    int idx = mod->num_table_elements++;
    mod->table_func_indices[idx] = func_idx;
    if ((uint32_t)mod->num_table_elements > mod->table_min_elems) {
        mod->table_min_elems = (uint32_t)mod->num_table_elements;
    }
    return idx;
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
