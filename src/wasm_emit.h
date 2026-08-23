/*
 * ZCC WebAssembly MVP Binary Emitter
 * Header File: src/wasm_emit.h
 * Target: WebAssembly MVP (Binary Format v1)
 */

#ifndef ZCC_WASM_EMIT_H
#define ZCC_WASM_EMIT_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* WebAssembly Binary Magic Header and Version */
#define WASM_MAGIC_HEADER   0x6d736100  /* \0asm */
#define WASM_MAGIC_VERSION  0x00000001  /* v1 */

/* WebAssembly Section IDs */
typedef enum {
    WASM_SEC_CUSTOM   = 0,
    WASM_SEC_TYPE     = 1,
    WASM_SEC_IMPORT   = 2,
    WASM_SEC_FUNCTION = 3,
    WASM_SEC_TABLE    = 4,
    WASM_SEC_MEMORY   = 5,
    WASM_SEC_GLOBAL   = 6,
    WASM_SEC_EXPORT   = 7,
    WASM_SEC_START    = 8,
    WASM_SEC_ELEMENT  = 9,
    WASM_SEC_CODE     = 10,
    WASM_SEC_DATA     = 11
} WASMSectionID;

/* WebAssembly Value Types */
typedef enum {
    WASM_TYPE_I32  = 0x7F,
    WASM_TYPE_I64  = 0x7E,
    WASM_TYPE_F32  = 0x7D,
    WASM_TYPE_F64  = 0x7C,
    WASM_TYPE_FUNC = 0x60,
    WASM_TYPE_VOID = 0x40
} WASMValueType;

/* WebAssembly Export Kinds */
typedef enum {
    WASM_EXPORT_FUNC   = 0x00,
    WASM_EXPORT_TABLE  = 0x01,
    WASM_EXPORT_MEM    = 0x02,
    WASM_EXPORT_GLOBAL = 0x03
} WASMExportKind;

/* WebAssembly Opcodes (MVP Core Set) */
typedef enum {
    WASM_OP_UNREACHABLE  = 0x00,
    WASM_OP_NOP          = 0x01,
    WASM_OP_BLOCK        = 0x02,
    WASM_OP_LOOP         = 0x03,
    WASM_OP_IF           = 0x04,
    WASM_OP_ELSE         = 0x05,
    WASM_OP_END          = 0x0B,
    WASM_OP_BR           = 0x0C,
    WASM_OP_BR_IF        = 0x0D,
    WASM_OP_RETURN       = 0x0F,
    WASM_OP_CALL         = 0x10,
    WASM_OP_CALL_INDIRECT= 0x11,
    WASM_OP_DROP         = 0x1A,
    WASM_OP_SELECT       = 0x1B,
    WASM_OP_LOCAL_GET    = 0x20,
    WASM_OP_LOCAL_SET    = 0x21,
    WASM_OP_LOCAL_TEE    = 0x22,
    WASM_OP_GLOBAL_GET   = 0x23,
    WASM_OP_GLOBAL_SET   = 0x24,
    WASM_OP_I32_LOAD     = 0x28,
    WASM_OP_I64_LOAD     = 0x29,
    WASM_OP_F32_LOAD     = 0x2A,
    WASM_OP_F64_LOAD     = 0x2B,
    WASM_OP_I32_LOAD8_S  = 0x2C,
    WASM_OP_I32_LOAD8_U  = 0x2D,
    WASM_OP_I32_LOAD16_S = 0x2E,
    WASM_OP_I32_LOAD16_U = 0x2F,
    WASM_OP_I32_STORE    = 0x36,
    WASM_OP_I64_STORE    = 0x37,
    WASM_OP_F32_STORE    = 0x38,
    WASM_OP_F64_STORE    = 0x39,
    WASM_OP_I32_STORE8   = 0x3A,
    WASM_OP_I32_STORE16  = 0x3B,
    WASM_OP_I32_CONST    = 0x41,
    WASM_OP_I64_CONST    = 0x42,
    WASM_OP_F32_CONST    = 0x43,
    WASM_OP_F64_CONST    = 0x44,
    WASM_OP_I32_EQZ      = 0x45,
    WASM_OP_I32_EQ       = 0x46,
    WASM_OP_I32_NE       = 0x47,
    WASM_OP_I32_LT_S     = 0x48,
    WASM_OP_I32_GT_S     = 0x4A,
    WASM_OP_I32_LE_S     = 0x4C,
    WASM_OP_I32_GE_S     = 0x4E,
    WASM_OP_F32_EQ       = 0x5B,
    WASM_OP_F32_NE       = 0x5C,
    WASM_OP_F32_LT       = 0x5D,
    WASM_OP_F32_GT       = 0x5E,
    WASM_OP_F32_LE       = 0x5F,
    WASM_OP_F32_GE       = 0x60,
    WASM_OP_F64_EQ       = 0x61,
    WASM_OP_F64_NE       = 0x62,
    WASM_OP_F64_LT       = 0x63,
    WASM_OP_F64_GT       = 0x64,
    WASM_OP_F64_LE       = 0x65,
    WASM_OP_F64_GE       = 0x66,
    WASM_OP_I32_ADD      = 0x6A,
    WASM_OP_I32_SUB      = 0x6B,
    WASM_OP_I32_MUL      = 0x6C,
    WASM_OP_I32_DIV_S    = 0x6D,
    WASM_OP_I32_REM_S    = 0x6F,
    WASM_OP_I32_AND      = 0x71,
    WASM_OP_I32_OR       = 0x72,
    WASM_OP_I32_XOR      = 0x73,
    WASM_OP_I32_SHL      = 0x74,
    WASM_OP_I32_SHR_S    = 0x75,
    WASM_OP_I32_SHR_U    = 0x76,
    WASM_OP_F32_ABS      = 0x8B,
    WASM_OP_F32_NEG      = 0x8C,
    WASM_OP_F32_CEIL     = 0x8D,
    WASM_OP_F32_FLOOR    = 0x8E,
    WASM_OP_F32_TRUNC    = 0x8F,
    WASM_OP_F32_NEAREST  = 0x90,
    WASM_OP_F32_SQRT     = 0x91,
    WASM_OP_F32_ADD      = 0x92,
    WASM_OP_F32_SUB      = 0x93,
    WASM_OP_F32_MUL      = 0x94,
    WASM_OP_F32_DIV      = 0x95,
    WASM_OP_F32_MIN      = 0x96,
    WASM_OP_F32_MAX      = 0x97,
    WASM_OP_F32_COPYSIGN = 0x98,
    WASM_OP_F64_ABS      = 0x99,
    WASM_OP_F64_NEG      = 0x9A,
    WASM_OP_F64_CEIL     = 0x9B,
    WASM_OP_F64_FLOOR    = 0x9C,
    WASM_OP_F64_TRUNC    = 0x9D,
    WASM_OP_F64_NEAREST  = 0x9E,
    WASM_OP_F64_SQRT     = 0x9F,
    WASM_OP_F64_ADD      = 0xA0,
    WASM_OP_F64_SUB      = 0xA1,
    WASM_OP_F64_MUL      = 0xA2,
    WASM_OP_F64_DIV      = 0xA3,
    WASM_OP_F64_MIN      = 0xA4,
    WASM_OP_F64_MAX      = 0xA5,
    WASM_OP_F64_COPYSIGN = 0xA6,
    WASM_OP_I32_TRUNC_F32_S = 0xA8,
    WASM_OP_I32_TRUNC_F32_U = 0xA9,
    WASM_OP_I32_TRUNC_F64_S = 0xAA,
    WASM_OP_I32_TRUNC_F64_U = 0xAB,
    WASM_OP_I64_TRUNC_F32_S = 0xAE,
    WASM_OP_I64_TRUNC_F32_U = 0xAF,
    WASM_OP_I64_TRUNC_F64_S = 0xB0,
    WASM_OP_I64_TRUNC_F64_U = 0xB1,
    WASM_OP_F32_CONVERT_I32_S = 0xB2,
    WASM_OP_F32_CONVERT_I32_U = 0xB3,
    WASM_OP_F32_CONVERT_I64_S = 0xB4,
    WASM_OP_F32_CONVERT_I64_U = 0xB5,
    WASM_OP_F32_DEMOTE_F64  = 0xB6,
    WASM_OP_F64_CONVERT_I32_S = 0xB7,
    WASM_OP_F64_CONVERT_I32_U = 0xB8,
    WASM_OP_F64_CONVERT_I64_S = 0xB9,
    WASM_OP_F64_CONVERT_I64_U = 0xBA,
    WASM_OP_F64_PROMOTE_F32 = 0xBB
} WASMOpcode;

/* Dynamic Byte Buffer for Section Construction */
typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
} WASMBuffer;

/* Function Signature Type */
typedef struct {
    uint8_t params[16];
    int num_params;
    uint8_t results[8];
    int num_results;
} WasmFuncType;

/* Global Variable Definition */
typedef struct {
    uint8_t val_type;
    uint8_t is_mutable;
    int32_t init_val;
} WasmGlobal;

/* Data Segment */
typedef struct {
    uint32_t offset;
    uint8_t *data;
    size_t size;
} WasmDataSegment;

/* Local Variable Mapping */
typedef struct {
    char name[64];
    uint32_t local_idx;
    uint8_t val_type;
    int is_memory;       /* 1 if stored in linear memory */
    int stack_offset;    /* byte offset relative to function frame pointer */
    int size;            /* byte size in memory */
    void *type;          /* Type* */
} WasmLocalEntry;

typedef struct {
    WasmLocalEntry entries[128];
    int count;
    int frame_size;      /* Total local memory frame size in bytes */
    int fp_local_idx;    /* WASM local index for frame pointer ($fp), -1 if none */
    int tmp_addr_idx;    /* Scratch local for address evaluation */
    int tmp_val_idx;     /* Scratch local for value evaluation */
    int tmp_f32_idx;     /* Scratch local for f32 evaluation */
    int tmp_f64_idx;     /* Scratch local for f64 evaluation */
} WasmLocalMap;

/* Function Definition */
typedef struct {
    char name[64];
    int type_idx;
    uint32_t num_params;
    uint8_t local_types[32];
    uint32_t num_locals;
    WASMBuffer body;
} WasmFunction;

/* Export Entry */
typedef struct {
    char name[64];
    WASMExportKind kind;
    uint32_t index;
} WasmExport;

/* Import Entry */
typedef enum {
    WASM_IMPORT_FUNC   = 0x00,
    WASM_IMPORT_TABLE  = 0x01,
    WASM_IMPORT_MEM    = 0x02,
    WASM_IMPORT_GLOBAL = 0x03
} WASMImportKind;

typedef struct {
    char mod_name[64];
    char field_name[64];
    WASMImportKind kind;
    uint32_t type_idx;
} WasmImport;

/* Complete WebAssembly Module Representation */
typedef struct {
    WasmFuncType types[64];
    int num_types;
    WasmImport imports[32];
    int num_imports;
    WasmFunction funcs[128];
    int num_funcs;
    WasmExport exports[128];
    int num_exports;
    WasmGlobal globals[16];
    int num_globals;
    WasmDataSegment data_segments[32];
    int num_data_segments;
    int has_memory;
    uint32_t mem_min_pages;
    uint32_t mem_max_pages;
    int has_table;
    uint32_t table_min_elems;
    uint32_t table_max_elems;
    uint32_t table_func_indices[256];
    int num_table_elements;
} WasmModule;

/* Buffer & Encoding Functions */
void wasm_buf_init(WASMBuffer *buf);
void wasm_buf_free(WASMBuffer *buf);
void wasm_buf_append_u8(WASMBuffer *buf, uint8_t byte);
void wasm_buf_append_bytes(WASMBuffer *buf, const uint8_t *bytes, size_t len);
size_t wasm_encode_uleb128(uint8_t *out, uint32_t val);
size_t wasm_encode_sleb128(uint8_t *out, int32_t val);
void wasm_buf_append_uleb128(WASMBuffer *buf, uint32_t val);
void wasm_buf_append_sleb128(WASMBuffer *buf, int32_t val);
void wasm_buf_append_f32(WASMBuffer *buf, float val);
void wasm_buf_append_f64(WASMBuffer *buf, double val);
void wasm_buf_append_memarg(WASMBuffer *buf, uint32_t align, uint32_t offset);

void wasm_emit_section(WASMBuffer *module_buf, WASMSectionID sec_id, const WASMBuffer *sec_data);

/* Module Building API */
void wasm_module_init(WasmModule *mod);
void wasm_module_free(WasmModule *mod);
int wasm_module_add_type(WasmModule *mod, const uint8_t *params, int num_params, const uint8_t *results, int num_results);
int wasm_module_add_import_func(WasmModule *mod, const char *mod_name, const char *field_name, int type_idx);
int wasm_module_add_function(WasmModule *mod, const char *name, int type_idx);
int wasm_module_find_func_idx(const WasmModule *mod, const char *name);
void wasm_module_add_export(WasmModule *mod, const char *name, WASMExportKind kind, uint32_t index);
int wasm_module_add_global(WasmModule *mod, uint8_t val_type, uint8_t is_mutable, int32_t init_val);
int wasm_module_add_data_segment(WasmModule *mod, uint32_t offset, const uint8_t *data, size_t size);
int wasm_module_add_table_element(WasmModule *mod, uint32_t func_idx);
int wasm_module_emit_binary(const WasmModule *mod, WASMBuffer *out);
int wasm_module_write_file(const WasmModule *mod, const char *filename);

int zcc_emit_wasm_module_to_file(const char *filename, const WASMBuffer *custom_code_body);

#endif /* ZCC_WASM_EMIT_H */
