// src/evm/memory_v2.c
#include "../../evm_lifter.h"
#include "../../ir.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

typedef struct {
    uint8_t* bytes;           // Flat byte array for true EVM overlapping memory
    size_t size;              // Active size
    size_t capacity;          // Allocated capacity
    void* storage_map;        // hashmap<256-bit slot, MemCell>
    uint8_t* calldata;
    size_t calldata_len;
} MemoryModelV2;

MemoryModelV2* memory_v2_new(void) {
    MemoryModelV2* m = calloc(1, sizeof(MemoryModelV2));
    m->capacity = 65536; // 64KB default
    m->bytes = calloc(m->capacity, sizeof(uint8_t));
    m->size = 0;
    return m;
}

void memory_v2_free(void *ptr) {
    MemoryModelV2 *m = (MemoryModelV2 *)ptr;
    if (!m) return;
    free(m->bytes);
    free(m->calldata);
    free(m);
}

static void ensure_capacity(MemoryModelV2* mem, size_t needed) {
    if (needed > mem->capacity) {
        size_t new_cap = mem->capacity * 2;
        while (new_cap < needed) {
            new_cap *= 2;
        }
        mem->bytes = realloc(mem->bytes, new_cap);
        memset(mem->bytes + mem->capacity, 0, new_cap - mem->capacity);
        mem->capacity = new_cap;
    }
    if (needed > mem->size) {
        mem->size = needed;
    }
}

void mstore_v2(MemoryModelV2* mem, uint64_t offset, evm_u256_t val, const char* expr) {
    if (!mem) return;
    (void)expr; // Symbolic expression tracking omitted for flat byte simplicity
    
    ensure_capacity(mem, offset + 32);
    
    // Write 32 bytes with correct Big-Endian EVM semantics
    for (int i = 0; i < 32; i++) {
        mem->bytes[offset + i] = val.bytes[i];
    }
}

void mstore8_v2(MemoryModelV2* mem, uint64_t offset, uint8_t val) {
    if (!mem) return;
    ensure_capacity(mem, offset + 1);
    mem->bytes[offset] = val;
}

const uint8_t* memory_v2_get_ptr(MemoryModelV2* mem, uint64_t offset, size_t length) {
    if (!mem) return NULL;
    ensure_capacity(mem, offset + length);
    return &mem->bytes[offset];
}

evm_u256_t mload_v2(MemoryModelV2* mem, uint64_t offset, char** out_expr) {
    evm_u256_t result;
    memset(result.bytes, 0, 32);
    if (!mem) return result;
    
    if (out_expr) *out_expr = NULL; // Flat byte model omits symbolic tracking
    
    // EVM implicitly expands memory on load too
    ensure_capacity(mem, offset + 32);
    
    for (int i = 0; i < 32; i++) {
        result.bytes[i] = mem->bytes[offset + i];
    }
    return result;
}

void sstore_v2(MemoryModelV2* mem, evm_u256_t slot, evm_u256_t val) {
    // hashmap insert placeholder
    (void)mem; (void)slot; (void)val;
}

void infer_abi_dispatch(MemoryModelV2* mem, const uint8_t* cd, size_t len) {
    (void)mem;
    if (len >= 4) {
        uint32_t selector = (cd[0]<<24) | (cd[1]<<16) | (cd[2]<<8) | cd[3];
        printf("Detected selector: 0x%08x\n", selector);
    }
}
