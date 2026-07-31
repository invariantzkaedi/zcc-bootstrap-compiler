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
    WASM_OP_DROP         = 0x1A,
    WASM_OP_SELECT       = 0x1B,
    WASM_OP_LOCAL_GET    = 0x20,
    WASM_OP_LOCAL_SET    = 0x21,
    WASM_OP_LOCAL_TEE    = 0x22,
    WASM_OP_GLOBAL_GET   = 0x23,
    WASM_OP_GLOBAL_SET   = 0x24,
    WASM_OP_I32_LOAD     = 0x28,
    WASM_OP_I32_STORE    = 0x36,
    WASM_OP_I32_CONST    = 0x41,
    WASM_OP_I64_CONST    = 0x42,
    WASM_OP_I32_EQZ      = 0x45,
    WASM_OP_I32_EQ       = 0x46,
    WASM_OP_I32_NE       = 0x47,
    WASM_OP_I32_LT_S     = 0x48,
    WASM_OP_I32_GT_S     = 0x4A,
    WASM_OP_I32_LE_S     = 0x4C,
    WASM_OP_I32_GE_S     = 0x4E,
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
    WASM_OP_I32_SHR_U    = 0x76
} WASMOpcode;

/* Dynamic Byte Buffer for Section Construction */
typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
} WASMBuffer;

/* Function Declarations */
void wasm_buf_init(WASMBuffer *buf);
void wasm_buf_free(WASMBuffer *buf);
void wasm_buf_append_u8(WASMBuffer *buf, uint8_t byte);
void wasm_buf_append_bytes(WASMBuffer *buf, const uint8_t *bytes, size_t len);
size_t wasm_encode_uleb128(uint8_t *out, uint32_t val);
size_t wasm_encode_sleb128(uint8_t *out, int32_t val);
void wasm_buf_append_uleb128(WASMBuffer *buf, uint32_t val);
void wasm_buf_append_sleb128(WASMBuffer *buf, int32_t val);

void wasm_emit_section(WASMBuffer *module_buf, WASMSectionID sec_id, const WASMBuffer *sec_data);
int zcc_emit_wasm_module_to_file(const char *filename, const WASMBuffer *custom_code_body);

#endif /* ZCC_WASM_EMIT_H */
