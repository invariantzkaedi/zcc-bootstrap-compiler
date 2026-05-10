// src/evm/memory_v2.c — Hardened EVM Memory Arena
// M1: offset overflow guards on all entry points
// M2: realloc NULL check with old pointer preservation
// M3: doubling loop with SIZE_MAX/2 saturation + hard ceiling
#include "../../evm_lifter.h"
#include "../../ir.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define MEM_V2_MAX_CAP ((size_t)1 << 30)  /* 1 GB hard ceiling */

typedef struct {
    uint8_t* bytes;           // Flat byte array for true EVM overlapping memory
    size_t size;              // Active size (high-water mark)
    size_t capacity;          // Allocated capacity
    void* storage_map;        // hashmap<256-bit slot, MemCell>
    uint8_t* calldata;
    size_t calldata_len;
} MemoryModelV2;

MemoryModelV2* memory_v2_new(void) {
    MemoryModelV2* m = calloc(1, sizeof(MemoryModelV2));
    if (!m) return NULL;
    m->capacity = 65536; // 64KB default
    m->bytes = calloc(m->capacity, sizeof(uint8_t));
    if (!m->bytes) {
        free(m);
        return NULL;
    }
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

/*
 * M1+M2+M3: Hardened capacity growth.
 * Returns 0 on success, -1 on overflow/OOM (caller must abort the operation).
 */
static int ensure_capacity(MemoryModelV2* mem, size_t needed) {
    if (needed <= mem->capacity) goto update_size;

    /* M3: doubling with saturation — can't wrap size_t to 0 */
    {
        size_t new_cap = mem->capacity;
        while (new_cap < needed) {
            if (new_cap > SIZE_MAX / 2 || new_cap > MEM_V2_MAX_CAP / 2) {
                new_cap = (needed < MEM_V2_MAX_CAP) ? needed : MEM_V2_MAX_CAP;
                break;
            }
            new_cap *= 2;
        }
        if (new_cap < needed) {
            fprintf(stderr, "[memory_v2] FATAL: capacity %zu exceeds ceiling\n", needed);
            return -1;
        }

        /* M2: realloc with old pointer preservation */
        uint8_t *tmp = realloc(mem->bytes, new_cap);
        if (!tmp) {
            fprintf(stderr, "[memory_v2] FATAL: realloc failed at %zu\n", new_cap);
            return -1;  /* old mem->bytes still valid */
        }

        /* Zero only the newly grown region */
        memset(tmp + mem->capacity, 0, new_cap - mem->capacity);

        mem->bytes = tmp;
        mem->capacity = new_cap;
    }

update_size:
    if (needed > mem->size) {
        mem->size = needed;
    }
    return 0;
}

void mstore_v2(MemoryModelV2* mem, uint64_t offset, evm_u256_t val, const char* expr) {
    if (!mem) return;
    (void)expr;

    /* M1: overflow guard — offset + 32 must not wrap */
    if (offset > SIZE_MAX - 32) return;
    if (ensure_capacity(mem, (size_t)(offset + 32)) != 0) return;

    for (int i = 0; i < 32; i++) {
        mem->bytes[offset + i] = val.bytes[i];
    }
}

void mstore8_v2(MemoryModelV2* mem, uint64_t offset, uint8_t val) {
    if (!mem) return;

    /* M1: overflow guard */
    if (offset > SIZE_MAX - 1) return;
    if (ensure_capacity(mem, (size_t)(offset + 1)) != 0) return;

    mem->bytes[offset] = val;
}

const uint8_t* memory_v2_get_ptr(MemoryModelV2* mem, uint64_t offset, size_t length) {
    if (!mem) return NULL;

    /* M1: overflow guard */
    if (length > SIZE_MAX - offset) return NULL;
    if (ensure_capacity(mem, (size_t)(offset + length)) != 0) return NULL;

    return &mem->bytes[offset];
}

evm_u256_t mload_v2(MemoryModelV2* mem, uint64_t offset, char** out_expr) {
    evm_u256_t result;
    memset(result.bytes, 0, 32);
    if (!mem) return result;

    if (out_expr) *out_expr = NULL;

    /* M1: overflow guard */
    if (offset > SIZE_MAX - 32) return result;
    if (ensure_capacity(mem, (size_t)(offset + 32)) != 0) return result;

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
