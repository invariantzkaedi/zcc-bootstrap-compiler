// src/evm/memory_v2.c
#include "../../evm_lifter.h"
#include "../../ir.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

typedef struct MemCell {
    evm_u256_t value;
    char* symbolic_expr;      // NULL = concrete
    uint64_t last_write_pc;
} MemCell;

typedef struct {
    MemCell* linear;          // dense fast path (64KB default)
    size_t linear_size;
    void* storage_map;        // hashmap<256-bit slot, MemCell>
    uint8_t* calldata;
    size_t calldata_len;
} MemoryModelV2;

MemoryModelV2* memory_v2_new(void) {
    MemoryModelV2* m = calloc(1, sizeof(MemoryModelV2));
    m->linear_size = 65536;
    m->linear = calloc(m->linear_size, sizeof(MemCell));
    return m;
}

void mstore_v2(MemoryModelV2* mem, uint64_t offset, evm_u256_t val, const char* expr) {
    if (!mem) return;
    if (offset < mem->linear_size) {
        mem->linear[offset].value = val;
        mem->linear[offset].symbolic_expr = expr ? strdup(expr) : NULL;
        mem->linear[offset].last_write_pc = 0; // update with current PC
    }
}

evm_u256_t mload_v2(MemoryModelV2* mem, uint64_t offset, char** out_expr) {
    evm_u256_t zero = {0};
    if (!mem) return zero;
    if (offset < mem->linear_size) {
        if (out_expr) *out_expr = mem->linear[offset].symbolic_expr;
        return mem->linear[offset].value;
    }
    return zero;
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
