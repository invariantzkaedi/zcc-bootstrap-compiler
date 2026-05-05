// src/evm/jit_memory.c — Sparse + Concrete direct x86 emission
#include "../../evm_lifter.h"
#include "../../ir.h"
#include <string.h>
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

typedef struct {
    uint8_t* code;
    size_t size;
    size_t capacity;
} JITBuffer;

extern void jit_emit(JITBuffer* buf, const void* data, size_t len);
extern void jit_emit_call(JITBuffer* buf, const char* target);
#include <string.h>

void jit_emit_memory_op(JITBuffer* buf, ir_node_t* node, MemoryModelV2* mem) {
    if (node->op == IR_LOAD) {
        // Wait, OP_MLOAD isn't the IR op. We use IR_LOAD for memory.
        uint64_t offset = node->imm; // Assuming known offset is stored in imm or similar

        if (offset < mem->linear_size) {
            MemCell* cell = &mem->linear[offset];
            if (cell->symbolic_expr == NULL) {
                // Concrete known value → immediate
                uint8_t mov[] = {0x48, 0xb8, 0,0,0,0,0,0,0,0};
                memcpy(mov+2, &cell->value, 8); // Simplification, using 8 bytes
                jit_emit(buf, mov, 10);
                return;
            }
        }
        // Direct addressed load
        uint8_t load[] = {0x48, 0x8b, 0x80}; // mov rax, [rax + offset]
        uint32_t off32 = (uint32_t)offset;
        memcpy(load+3, &off32, 4);
        jit_emit(buf, load, 7);
    }
    else if (node->op == IR_STORE) {
        // Symmetric direct store path
        uint8_t store[] = {0x48, 0x89, 0x80}; // mov [rax + offset], rax
        uint32_t off32 = (uint32_t)node->imm;
        memcpy(store+3, &off32, 4);
        jit_emit(buf, store, 7);
    }
    else {
        // fallback to runtime MemoryModelV2 call
        jit_emit_call(buf, "mload_v2");
    }
}
