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
    WASM_SEC_DATA     = 11,
    WASM_SEC_TAG      = 13
} WASMSectionID;

/* WebAssembly Value Types */
typedef enum {
    WASM_TYPE_I32  = 0x7F,
    WASM_TYPE_I64  = 0x7E,
    WASM_TYPE_F32  = 0x7D,
    WASM_TYPE_F64  = 0x7C,
    WASM_TYPE_V128 = 0x7B,
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
    WASM_OP_THROW        = 0x08,
    WASM_OP_END          = 0x0B,
    WASM_OP_BR           = 0x0C,
    WASM_OP_BR_IF        = 0x0D,
    WASM_OP_BR_TABLE     = 0x0E,
    WASM_OP_RETURN       = 0x0F,
    WASM_OP_CALL         = 0x10,
    WASM_OP_CALL_INDIRECT= 0x11,
    WASM_OP_DROP         = 0x1A,
    WASM_OP_SELECT       = 0x1B,
    WASM_OP_TRY_TABLE    = 0x1F,
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
    WASM_OP_I64_LOAD8_S  = 0x30,
    WASM_OP_I64_LOAD8_U  = 0x31,
    WASM_OP_I64_LOAD16_S = 0x32,
    WASM_OP_I64_LOAD16_U = 0x33,
    WASM_OP_I64_LOAD32_S = 0x34,
    WASM_OP_I64_LOAD32_U = 0x35,
    WASM_OP_I32_STORE    = 0x36,
    WASM_OP_I64_STORE    = 0x37,
    WASM_OP_F32_STORE    = 0x38,
    WASM_OP_F64_STORE    = 0x39,
    WASM_OP_I32_STORE8   = 0x3A,
    WASM_OP_I32_STORE16  = 0x3B,
    WASM_OP_I64_STORE8   = 0x3C,
    WASM_OP_I64_STORE16  = 0x3D,
    WASM_OP_I64_STORE32  = 0x3E,
    WASM_OP_MEMORY_SIZE  = 0x3F,
    WASM_OP_MEMORY_GROW  = 0x40,
    WASM_OP_I32_CONST    = 0x41,
    WASM_OP_I64_CONST    = 0x42,
    WASM_OP_F32_CONST    = 0x43,
    WASM_OP_F64_CONST    = 0x44,
    WASM_OP_I32_EQZ      = 0x45,
    WASM_OP_I32_EQ       = 0x46,
    WASM_OP_I32_NE       = 0x47,
    WASM_OP_I32_LT_S     = 0x48,
    WASM_OP_I32_LT_U     = 0x49,
    WASM_OP_I32_GT_S     = 0x4A,
    WASM_OP_I32_GT_U     = 0x4B,
    WASM_OP_I32_LE_S     = 0x4C,
    WASM_OP_I32_LE_U     = 0x4D,
    WASM_OP_I32_GE_S     = 0x4E,
    WASM_OP_I32_GE_U     = 0x4F,
    WASM_OP_I64_EQZ      = 0x50,
    WASM_OP_I64_EQ       = 0x51,
    WASM_OP_I64_NE       = 0x52,
    WASM_OP_I64_LT_S     = 0x53,
    WASM_OP_I64_LT_U     = 0x54,
    WASM_OP_I64_GT_S     = 0x55,
    WASM_OP_I64_GT_U     = 0x56,
    WASM_OP_I64_LE_S     = 0x57,
    WASM_OP_I64_LE_U     = 0x58,
    WASM_OP_I64_GE_S     = 0x59,
    WASM_OP_I64_GE_U     = 0x5A,
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
    WASM_OP_I32_CLZ      = 0x67,
    WASM_OP_I32_CTZ      = 0x68,
    WASM_OP_I32_POPCNT   = 0x69,
    WASM_OP_I32_ADD      = 0x6A,
    WASM_OP_I32_SUB      = 0x6B,
    WASM_OP_I32_MUL      = 0x6C,
    WASM_OP_I32_DIV_S    = 0x6D,
    WASM_OP_I32_DIV_U    = 0x6E,
    WASM_OP_I32_REM_S    = 0x6F,
    WASM_OP_I32_REM_U    = 0x70,
    WASM_OP_I32_AND      = 0x71,
    WASM_OP_I32_OR       = 0x72,
    WASM_OP_I32_XOR      = 0x73,
    WASM_OP_I32_SHL      = 0x74,
    WASM_OP_I32_SHR_S    = 0x75,
    WASM_OP_I32_SHR_U    = 0x76,
    WASM_OP_I32_ROTL     = 0x77,
    WASM_OP_I32_ROTR     = 0x78,
    WASM_OP_I64_CLZ      = 0x79,
    WASM_OP_I64_CTZ      = 0x7A,
    WASM_OP_I64_POPCNT   = 0x7B,
    WASM_OP_I64_ADD      = 0x7C,
    WASM_OP_I64_SUB      = 0x7D,
    WASM_OP_I64_MUL      = 0x7E,
    WASM_OP_I64_DIV_S    = 0x7F,
    WASM_OP_I64_DIV_U    = 0x80,
    WASM_OP_I64_REM_S    = 0x81,
    WASM_OP_I64_REM_U    = 0x82,
    WASM_OP_I64_AND      = 0x83,
    WASM_OP_I64_OR       = 0x84,
    WASM_OP_I64_XOR      = 0x85,
    WASM_OP_I64_SHL      = 0x86,
    WASM_OP_I64_SHR_S    = 0x87,
    WASM_OP_I64_SHR_U    = 0x88,
    WASM_OP_I64_ROTL     = 0x89,
    WASM_OP_I64_ROTR     = 0x8A,
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
    WASM_OP_I32_WRAP_I64    = 0xA7,
    WASM_OP_I32_TRUNC_F32_S = 0xA8,
    WASM_OP_I32_TRUNC_F32_U = 0xA9,
    WASM_OP_I32_TRUNC_F64_S = 0xAA,
    WASM_OP_I32_TRUNC_F64_U = 0xAB,
    WASM_OP_I64_EXTEND_I32_S = 0xAC,
    WASM_OP_I64_EXTEND_I32_U = 0xAD,
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
    WASM_OP_F64_PROMOTE_F32 = 0xBB,
    WASM_OP_SIMD_PREFIX  = 0xFD
} WASMOpcode;

/* WebAssembly 128-Bit SIMD (0xFD Prefix) Sub-Opcodes */
typedef enum {
    WASM_SIMD_V128_LOAD         = 0x00,
    WASM_SIMD_V128_STORE        = 0x0B,
    WASM_SIMD_V128_CONST        = 0x0C,
    WASM_SIMD_I8X16_SHUFFLE     = 0x0D,
    WASM_SIMD_I32X4_SPLAT       = 0x11,
    WASM_SIMD_I64X2_SPLAT       = 0x12,
    WASM_SIMD_F32X4_SPLAT       = 0x13,
    WASM_SIMD_F64X2_SPLAT       = 0x14,
    WASM_SIMD_I32X4_EXTRACT_LANE = 0x1B,
    WASM_SIMD_I32X4_REPLACE_LANE = 0x1C,
    WASM_SIMD_V128_NOT          = 0x4D,
    WASM_SIMD_V128_AND          = 0x4E,
    WASM_SIMD_V128_OR           = 0x50,
    WASM_SIMD_V128_XOR          = 0x51,
    WASM_SIMD_I8X16_AVGR_U      = 0x7B,
    WASM_SIMD_I16X8_AVGR_U      = 0x9B,
    WASM_SIMD_I32X4_ABS         = 0xA0,
    WASM_SIMD_I32X4_NEG         = 0xA1,
    WASM_SIMD_I32X4_SHL         = 0xAB,
    WASM_SIMD_I32X4_SHR_S       = 0xAC,
    WASM_SIMD_I32X4_SHR_U       = 0xAD,
    WASM_SIMD_I32X4_ADD         = 0xAE,
    WASM_SIMD_I32X4_SUB         = 0xB1,
    WASM_SIMD_I32X4_MUL         = 0xB5,
    WASM_SIMD_I32X4_MIN_S       = 0xB6,
    WASM_SIMD_I32X4_MIN_U       = 0xB7,
    WASM_SIMD_I32X4_MAX_S       = 0xB8,
    WASM_SIMD_I32X4_MAX_U       = 0xB9,
    WASM_SIMD_I32X4_DOT_I16X8_S = 0xBA,
    WASM_SIMD_F32X4_ABS         = 0xE0,
    WASM_SIMD_F32X4_NEG         = 0xE1,
    WASM_SIMD_F32X4_SQRT        = 0xE3,
    WASM_SIMD_F32X4_ADD         = 0xE4,
    WASM_SIMD_F32X4_SUB         = 0xE5,
    WASM_SIMD_F32X4_MUL         = 0xE6,
    WASM_SIMD_F32X4_DIV         = 0xE7,
    WASM_SIMD_F32X4_MIN         = 0xE8,
    WASM_SIMD_F32X4_MAX         = 0xE9
} WASMSimdOpcode;

/* Dynamic Byte Buffer for Section Construction */
typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
} WASMBuffer;

/* Function Signature Type */
typedef struct {
    uint8_t *params;
    int num_params;
    int cap_params;
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
    char name[128];
    uint32_t local_idx;
    uint8_t val_type;
    int is_memory;       /* 1 if stored in linear memory */
    int stack_offset;    /* byte offset relative to function frame pointer */
    int size;            /* byte size in memory */
    void *type;          /* Type* */
    void *sym;           /* Symbol* for lexical block disambiguation */
} WasmLocalEntry;

typedef struct {
    WasmLocalEntry *entries;
    int count;
    int cap;
    int frame_size;      /* Total local memory frame size in bytes */
    int fp_local_idx;    /* WASM local index for frame pointer ($fp), -1 if none */
    int va_ptr_local_idx;/* Hidden trailing i32 param for variadic funcs, -1 if none */
    int sret_local_idx;  /* Hidden trailing i32 param for struct/union return, -1 if none */
    int tmp_addr_idx;    /* Scratch local for address evaluation */
    int tmp_val_idx;     /* Scratch local for i32 value evaluation */
    int tmp_i64_idx;     /* Scratch local for i64 value evaluation */
    int tmp_f32_idx;     /* Scratch local for f32 evaluation */
    int tmp_f64_idx;     /* Scratch local for f64 evaluation */
    int tmp_v128_idx;    /* Scratch local for v128 vector reduction */
    int sj_val_local_idx;/* Returned value local for setjmp EH dispatch, -1 if none */
} WasmLocalMap;

/* Function Definition */
typedef struct {
    char name[128];
    int type_idx;
    uint32_t num_params;
    uint8_t *local_types;
    uint32_t num_locals;
    uint32_t cap_locals;
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
    WasmFuncType *types;
    int num_types;
    int cap_types;
    WasmImport *imports;
    int num_imports;
    int cap_imports;
    WasmFunction *funcs;
    int num_funcs;
    int cap_funcs;
    WasmExport *exports;
    int num_exports;
    int cap_exports;
    WasmGlobal *globals;
    int num_globals;
    int cap_globals;
    WasmDataSegment *data_segments;
    int num_data_segments;
    int cap_data_segments;
    int has_memory;
    uint32_t mem_min_pages;
    uint32_t mem_max_pages;
    int has_table;
    uint32_t table_min_elems;
    uint32_t table_max_elems;
    uint32_t *table_func_indices;
    int num_table_elements;
    int cap_table_elements;
    uint32_t heap_base_init;
    int free_head_global_idx;
    int has_longjmp_tag;
    int longjmp_tag_type_idx;
} WasmModule;

#define WASM_CATCH_TAG 0x00

/* Buffer & Encoding Functions */
void wasm_buf_init(WASMBuffer *buf);
void wasm_buf_free(WASMBuffer *buf);
void wasm_buf_append_u8(WASMBuffer *buf, uint8_t byte);
void wasm_buf_append_bytes(WASMBuffer *buf, const uint8_t *bytes, size_t len);
size_t wasm_encode_uleb128(uint8_t *out, uint32_t val);
size_t wasm_encode_sleb128(uint8_t *out, int32_t val);
size_t wasm_encode_sleb128_i64(uint8_t *out, int64_t val);
void wasm_buf_append_uleb128(WASMBuffer *buf, uint32_t val);
void wasm_buf_append_sleb128(WASMBuffer *buf, int32_t val);
void wasm_buf_append_sleb128_i64(WASMBuffer *buf, int64_t val);
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
int wasm_local_alloc_entry(WasmLocalMap *map);
void wasm_local_map_free(WasmLocalMap *map);
int wasm_func_reserve_locals(WasmFunction *fn, uint32_t needed);
int wasm_module_configure_layout(WasmModule *mod, uint32_t static_data_end, uint32_t max_pages_override);
int wasm_emit_synth_allocator(WasmModule *mod, int synth_malloc, int synth_free, int export_runtime);
int wasm_emit_synth_allocator_ex(WasmModule *mod, int synth_malloc, int synth_free,
                                 int synth_calloc, int synth_realloc,
                                 int synth_heap_used, int synth_heap_cap, int export_runtime);
int wasm_emit_synth_wasi(WasmModule *mod, int fd_write_idx, int fd_read_idx, int proc_exit_idx,
                         int need_puts, int need_putchar, int need_write, int need_read,
                         int need_exit, int need_start, int export_runtime);
int wasm_module_emit_binary(const WasmModule *mod, WASMBuffer *out);
int wasm_module_write_file(const WasmModule *mod, const char *filename);

int zcc_emit_wasm_module_to_file(const char *filename, const WASMBuffer *custom_code_body);

#endif /* ZCC_WASM_EMIT_H */
