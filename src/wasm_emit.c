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

void wasm_emit_section(WASMBuffer *module_buf, WASMSectionID sec_id, const WASMBuffer *sec_data) {
    if (!module_buf || !sec_data || sec_data->size == 0) return;
    wasm_buf_append_u8(module_buf, (uint8_t)sec_id);
    wasm_buf_append_uleb128(module_buf, (uint32_t)sec_data->size);
    wasm_buf_append_bytes(module_buf, sec_data->data, sec_data->size);
}

int zcc_emit_wasm_module_to_file(const char *filename, const WASMBuffer *custom_code_body) {
    if (!filename) return -1;

    WASMBuffer module_buf;
    wasm_buf_init(&module_buf);

    /* 1. Header: Magic \0asm (0x6d736100) + Version 1 (0x00000001) */
    uint8_t magic_header[8] = { 0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00 };
    wasm_buf_append_bytes(&module_buf, magic_header, 8);

    /* 2. Type Section (1): 1 function signature: (i32, i32) -> i32 */
    WASMBuffer type_sec;
    wasm_buf_init(&type_sec);
    wasm_buf_append_uleb128(&type_sec, 1); /* 1 type definition */
    wasm_buf_append_u8(&type_sec, WASM_TYPE_FUNC);
    wasm_buf_append_uleb128(&type_sec, 2); /* 2 params */
    wasm_buf_append_u8(&type_sec, WASM_TYPE_I32);
    wasm_buf_append_u8(&type_sec, WASM_TYPE_I32);
    wasm_buf_append_uleb128(&type_sec, 1); /* 1 result */
    wasm_buf_append_u8(&type_sec, WASM_TYPE_I32);
    wasm_emit_section(&module_buf, WASM_SEC_TYPE, &type_sec);
    wasm_buf_free(&type_sec);

    /* 3. Function Section (3): 1 function signature index (0) */
    WASMBuffer func_sec;
    wasm_buf_init(&func_sec);
    wasm_buf_append_uleb128(&func_sec, 1); /* 1 function */
    wasm_buf_append_uleb128(&func_sec, 0); /* Type index 0 */
    wasm_emit_section(&module_buf, WASM_SEC_FUNCTION, &func_sec);
    wasm_buf_free(&func_sec);

    /* 4. Memory Section (5): 1 memory page (64KB), initial 1 page */
    WASMBuffer mem_sec;
    wasm_buf_init(&mem_sec);
    wasm_buf_append_uleb128(&mem_sec, 1); /* 1 memory count */
    wasm_buf_append_u8(&mem_sec, 0x00);   /* limits: min page only */
    wasm_buf_append_uleb128(&mem_sec, 1); /* min 1 page */
    wasm_emit_section(&module_buf, WASM_SEC_MEMORY, &mem_sec);
    wasm_buf_free(&mem_sec);

    /* 5. Export Section (7): Export "main" (func 0) and "memory" (mem 0) */
    WASMBuffer export_sec;
    wasm_buf_init(&export_sec);
    wasm_buf_append_uleb128(&export_sec, 2); /* 2 exports */
    
    /* Export "main" */
    const char *main_name = "main";
    wasm_buf_append_uleb128(&export_sec, (uint32_t)strlen(main_name));
    wasm_buf_append_bytes(&export_sec, (const uint8_t *)main_name, strlen(main_name));
    wasm_buf_append_u8(&export_sec, WASM_EXPORT_FUNC);
    wasm_buf_append_uleb128(&export_sec, 0); /* func index 0 */

    /* Export "memory" */
    const char *mem_name = "memory";
    wasm_buf_append_uleb128(&export_sec, (uint32_t)strlen(mem_name));
    wasm_buf_append_bytes(&export_sec, (const uint8_t *)mem_name, strlen(mem_name));
    wasm_buf_append_u8(&export_sec, WASM_EXPORT_MEM);
    wasm_buf_append_uleb128(&export_sec, 0); /* memory index 0 */

    wasm_emit_section(&module_buf, WASM_SEC_EXPORT, &export_sec);
    wasm_buf_free(&export_sec);

    /* 6. Code Section (10): Function body */
    WASMBuffer code_sec;
    wasm_buf_init(&code_sec);
    wasm_buf_append_uleb128(&code_sec, 1); /* 1 function body */

    WASMBuffer body_buf;
    wasm_buf_init(&body_buf);
    wasm_buf_append_uleb128(&body_buf, 0); /* 0 local variable declarations */

    if (custom_code_body && custom_code_body->size > 0) {
        wasm_buf_append_bytes(&body_buf, custom_code_body->data, custom_code_body->size);
    } else {
        /* Default body: return arg0 + arg1 */
        wasm_buf_append_u8(&body_buf, WASM_OP_LOCAL_GET);
        wasm_buf_append_uleb128(&body_buf, 0);
        wasm_buf_append_u8(&body_buf, WASM_OP_LOCAL_GET);
        wasm_buf_append_uleb128(&body_buf, 1);
        wasm_buf_append_u8(&body_buf, WASM_OP_I32_ADD);
        wasm_buf_append_u8(&body_buf, WASM_OP_END);
    }

    wasm_buf_append_uleb128(&code_sec, (uint32_t)body_buf.size);
    wasm_buf_append_bytes(&code_sec, body_buf.data, body_buf.size);
    wasm_buf_free(&body_buf);

    wasm_emit_section(&module_buf, WASM_SEC_CODE, &code_sec);
    wasm_buf_free(&code_sec);

    /* Write to file */
    FILE *f = fopen(filename, "wb");
    if (!f) {
        wasm_buf_free(&module_buf);
        return -1;
    }
    fwrite(module_buf.data, 1, module_buf.size, f);
    fclose(f);

    wasm_buf_free(&module_buf);
    return 0;
}
