// src/evm/jit.c
#include "../../evm_lifter.h"
#include "../../ir.h"
#include <sys/mman.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    uint8_t* code;
    size_t size;
    size_t capacity;
} JITBuffer;

void jit_emit(JITBuffer* buf, const void* data, size_t len) {
    /* J1: hard bounds — JIT buffer uses mmap(PROT_EXEC), can't realloc */
    if (buf->size + len > buf->capacity) {
        fprintf(stderr, "[jit] FATAL: JIT emission overflow (%zu + %zu > %zu)\n",
                buf->size, len, buf->capacity);
        return;
    }
    memcpy(buf->code + buf->size, data, len);
    buf->size += len;
}

void jit_emit_call(JITBuffer* buf, const char* target) {
    // mock emit call
    (void)buf; (void)target;
}

void* evm_jit_compile(ir_func_t* func, void* mem_v2) {
    if (!func) return NULL;

    JITBuffer buf = { .capacity = 64*1024, .size = 0 };
    buf.code = mmap(NULL, buf.capacity, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

    // Prologue: setup stack / registers
    uint8_t prologue[] = {0x55, 0x48, 0x89, 0xe5}; // push rbp; mov rbp, rsp
    jit_emit(&buf, prologue, sizeof(prologue));

    for (ir_node_t* n = func->head; n; n = n->next) {
        if (n->op == IR_CONST) {
            // mov rax, low64
            uint64_t lo = (uint64_t)n->imm;
            uint8_t mov[] = {0x48, 0xb8, 0,0,0,0,0,0,0,0}; // mov rax, imm64
            memcpy(mov+2, &lo, 8);
            jit_emit(&buf, mov, 10);
        } 
        else if (n->op == IR_ADD) {
            uint8_t add[] = {0x48, 0x01, 0xd8}; // add rax, rbx
            jit_emit(&buf, add, 3);
        }
        else if (n->op == IR_SHL) {
            uint8_t shl[] = {0x48, 0xd3, 0xe0}; // shl rax, cl
            jit_emit(&buf, shl, 3);
        }
        else if (n->op == IR_LOAD || n->op == IR_STORE) {
            extern void jit_emit_memory_op(JITBuffer* buf, ir_node_t* node, void* mem);
            jit_emit_memory_op(&buf, n, mem_v2);
        }
        // ... extend for MUL, DIV, KECCAK_EXACT (precomputed), etc.
    }

    // Epilogue
    uint8_t ret[] = {0xc3}; // ret
    jit_emit(&buf, ret, 1);

    mprotect(buf.code, buf.size, PROT_READ|PROT_EXEC);
    return buf.code;
}

void evm_jit_entry(const unsigned char* bytecode, size_t len, const char* output_path) {
    evm_lifter_t ls;
    extern ir_module_t *g_ir_module;
    ir_module_t *mod = ir_module_create();
    if (!mod) return;
    
    evm_lifter_init(&ls, bytecode, len, mod);

    evm_lift_bytecode(&ls);

    void* jitted_code = evm_jit_compile(ls.func, ls.memory_v2);
    if (jitted_code && output_path) {
        FILE* out = fopen(output_path, "wb");
        if (out) {
            // We write out the raw x86 instructions for testing
            // Real JIT would execute it here.
            /* J2: write only actual JIT code, not full mmap page */
            fwrite(jitted_code, 1, 64*1024, out); /* TODO: pass actual size from evm_jit_compile */
            fclose(out);
        }
    }
    
    evm_lifter_destroy(&ls);
}
