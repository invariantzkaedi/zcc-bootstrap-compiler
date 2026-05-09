// src/evm/jit.c — ZKAEDI PRIME JIT Memory Engine
// Strike 1: Hardened bounds checking on all emit paths.
// Strike 2: Phase 4 — %r13 base pointer pinning for concrete EVM memory.
// Strike 3: Phase 5 — IR_SHR/IR_EQ/IR_BR_IF/IR_LABEL/IR_SUB/IR_MUL + backpatch.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "../../evm_lifter.h"
#include "../../ir.h"
#include <sys/mman.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    uint8_t* code;
    size_t size;
    size_t capacity;
} JITBuffer;

/*
 * HARDENED jit_emit — Strike 2 (Phase 6)
 * Dynamic buffer expansion via mremap(2).
 * Allows unbounded compilation size while maintaining W^X and bounds safety.
 */
void jit_emit(JITBuffer* buf, const void* data, size_t len) {
    if (!buf || !data || len == 0) return;

    if (buf->size + len > buf->capacity) {
        size_t new_cap = buf->capacity * 2;
        while (buf->size + len > new_cap) new_cap *= 2;

        /* MREMAP_MAYMOVE allows the kernel to relocate the page if it can't
         * expand it in-place. W^X is strictly maintained because this is
         * still the PROT_WRITE phase.
         */
        void* new_code = mremap(buf->code, buf->capacity, new_cap, MREMAP_MAYMOVE);
        if (new_code == MAP_FAILED) {
            fprintf(stderr, "[WARDEN FATAL] mremap failed to expand JIT buffer to %zu bytes.\n", new_cap);
            exit(EXIT_FAILURE);
        }
        
        buf->code = (uint8_t*)new_code;
        buf->capacity = new_cap;
    }

    memcpy(buf->code + buf->size, data, len);
    buf->size += len;
}

void jit_emit_call(JITBuffer* buf, const char* target) {
    // mock emit call
    (void)buf; (void)target;
}

/* ── Backpatch table for forward branch resolution ───────────────────
 *
 * Two-phase protocol:
 *   Phase A (emit): IR_BR_IF emits jne with rel32=0, records patch site.
 *   Phase B (fix):  After all nodes, walk patches, fill rel32 from label table.
 *
 * rel32 = (int32_t)(label_offset - (patch_pos + 4))
 *   patch_pos: byte offset of the rel32 field (after opcode bytes 0f 85)
 *   +4: CPU reads RIP from end of the 6-byte instruction
 * ─────────────────────────────────────────────────────────────────── */
#define JIT_MAX_PATCHES 64
#define JIT_MAX_LABELS  64

typedef struct {
    size_t patch_pos;           /* offset of rel32 field in buf->code  */
    char   label[IR_LABEL_MAX]; /* target label name                   */
} JITPatch;

typedef struct {
    char   label[IR_LABEL_MAX]; /* label name                          */
    size_t offset;              /* byte offset in buf->code            */
} JITLabelSite;

void* evm_jit_compile(ir_func_t* func, void* mem_v2) {
    if (!func) return NULL;

    JITBuffer buf = { .capacity = 64*1024, .size = 0 };
    /* W^X compliant: allocate WRITE-only, flip to EXEC after emission via mprotect. */
    buf.code = mmap(NULL, buf.capacity, PROT_READ|PROT_WRITE,
                    MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (buf.code == MAP_FAILED) {
        perror("[WARDEN FATAL] mmap for JIT buffer failed");
        return NULL;
    }

    /*
     * Prologue — x86-64 System V ABI compliant.
     * push rbp          : 0x55
     * mov  rbp, rsp     : 0x48 0x89 0xe5
     * push r13          : 0x41 0x55  (callee-saved; will hold mem->bytes base)
     */
    uint8_t prologue[] = {
        0x55,                   /* push rbp       */
        0x48, 0x89, 0xe5,       /* mov  rbp, rsp  */
        0x41, 0x55              /* push r13       */
    };
    jit_emit(&buf, prologue, sizeof(prologue));

    /*
     * Pin mem->bytes to %r13 for zero-latency base addressing.
     * Pin mem->calldata to %r14 for CALLDATALOAD intrinsic access.
     *
     * If mem_v2 is non-NULL:
     *   r13 = mem->bytes    (flat EVM memory, field[0])
     *   r14 = mem->calldata (calldata buffer, field[3])
     * Otherwise both = 0 (safe: memory ops emit NOP via jit_emit_memory_op).
     */
    {
        typedef struct {
            uint8_t* bytes;
            size_t   size;
            size_t   capacity;
            void*    storage_map;
            uint8_t* calldata;
        } MV2Full;

        uint64_t bytes_addr    = 0;
        uint64_t calldata_addr = 0;
        if (mem_v2 != NULL) {
            MV2Full *mv = (MV2Full*)mem_v2;
            bytes_addr    = (uint64_t)mv->bytes;
            calldata_addr = (uint64_t)mv->calldata;
        }

        /* movabs r13, <bytes_ptr>    : 49 bd [8 bytes] */
        uint8_t pin_r13[10] = { 0x49, 0xbd };
        memcpy(pin_r13 + 2, &bytes_addr, 8);
        jit_emit(&buf, pin_r13, 10);

        /* movabs r14, <calldata_ptr> : 49 be [8 bytes]
         * push r14 first (callee-saved ABI) */
        uint8_t push_r14[] = { 0x41, 0x56 }; /* push r14 */
        jit_emit(&buf, push_r14, 2);

        uint8_t pin_r14[10] = { 0x49, 0xbe };
        memcpy(pin_r14 + 2, &calldata_addr, 8);
        jit_emit(&buf, pin_r14, 10);
    }

    /* ── Backpatch tables (Phase A — filled during emit) ── */
    JITPatch     patches[JIT_MAX_PATCHES];
    JITLabelSite label_sites[JIT_MAX_LABELS];
    int n_patches = 0;
    int n_labels  = 0;

    for (ir_node_t* n = func->head; n; n = n->next) {
        if (n->op == IR_CONST) {
            uint64_t lo = (uint64_t)n->imm;
            uint8_t mov[] = {0x48, 0xb8, 0,0,0,0,0,0,0,0}; /* movabsq $imm, rax */
            memcpy(mov+2, &lo, 8);
            jit_emit(&buf, mov, 10);
        }
        else if (n->op == IR_ADD) {
            /* add rax, rbx : 48 01 d8 */
            uint8_t add[] = {0x48, 0x01, 0xd8};
            jit_emit(&buf, add, 3);
        }
        else if (n->op == IR_SUB) {
            /* sub rax, rcx : load src2 into rcx, then sub
             * 48 29 c8  = subq %rcx, %rax */
            uint8_t sub[] = {0x48, 0x29, 0xc8};
            jit_emit(&buf, sub, 3);
        }
        else if (n->op == IR_MUL) {
            /* imulq %rcx, %rax : 48 0f af c1 */
            uint8_t mul[] = {0x48, 0x0f, 0xaf, 0xc1};
            jit_emit(&buf, mul, 4);
        }
        else if (n->op == IR_SHL) {
            /* shlq %cl, %rax : 48 d3 e0 */
            uint8_t shl[] = {0x48, 0xd3, 0xe0};
            jit_emit(&buf, shl, 3);
        }
        else if (n->op == IR_SHR) {
            /*
             * EVM SHR is ALWAYS logical (unsigned) — never arithmetic.
             * If shift amount is a known constant in node->imm, use imm8 form.
             * Otherwise load count into %rcx and use register form.
             *
             * shrq $imm8, %rax  : 48 c1 e8 <imm8>  (4 bytes, preferred)
             * shrq %cl,   %rax  : 48 d3 e8          (3 bytes, runtime count)
             *
             * NOTE: AOT (ir_to_x86.c) emits sarq (signed) — D7 defect.
             * JIT always emits shrq (unsigned) — correct EVM semantics.
             */
            if (n->imm > 0 && n->imm < 64) {
                uint8_t shr[] = {0x48, 0xc1, 0xe8, (uint8_t)n->imm};
                jit_emit(&buf, shr, 4);
            } else {
                /* shift count in rax → move to rcx first */
                uint8_t mov_rcx[] = {0x48, 0x89, 0xc1}; /* movq rax, rcx */
                jit_emit(&buf, mov_rcx, 3);
                uint8_t shr[] = {0x48, 0xd3, 0xe8};     /* shrq %cl, rax */
                jit_emit(&buf, shr, 3);
            }
        }
        else if (n->op == IR_EQ) {
            /*
             * IR_EQ: rax = (src1 == src2) ? 1 : 0
             * Assumes src1 in %rax, src2 loaded into %rcx.
             *
             * cmpq %rcx, %rax  : 48 39 c8   (sets ZF if equal)
             * sete %al         : 0f 94 c0
             * movzbq %al, %rax : 48 0f b6 c0 (zero-extend to 64-bit result)
             */
            uint8_t cmp[]    = {0x48, 0x39, 0xc8};
            uint8_t sete[]   = {0x0f, 0x94, 0xc0};
            uint8_t movzb[]  = {0x48, 0x0f, 0xb6, 0xc0};
            jit_emit(&buf, cmp,   3);
            jit_emit(&buf, sete,  3);
            jit_emit(&buf, movzb, 4);
        }
        else if (n->op == IR_BR_IF) {
            /*
             * IR_BR_IF: if (%rax != 0) jump to n->label
             *
             * test %rax, %rax  : 48 85 c0        (ZF=1 if rax==0)
             * jne  rel32       : 0f 85 XX XX XX XX  (jump if NOT zero)
             *
             * Phase A: emit with rel32=0 placeholder, record patch site.
             * Phase B: backpatch resolver fills rel32 after all nodes.
             */
            uint8_t test[] = {0x48, 0x85, 0xc0};
            jit_emit(&buf, test, 3);

            if (n_patches < JIT_MAX_PATCHES) {
                patches[n_patches].patch_pos = buf.size + 2; /* rel32 starts after 0f 85 */
                strncpy(patches[n_patches].label, n->label, IR_LABEL_MAX - 1);
                patches[n_patches].label[IR_LABEL_MAX - 1] = '\0';
                n_patches++;
            } else {
                fprintf(stderr, "[JIT WARN] patch table full — branch to %s lost\n", n->label);
            }

            uint8_t jne[] = {0x0f, 0x85, 0x00, 0x00, 0x00, 0x00}; /* jne rel32=0 */
            jit_emit(&buf, jne, 6);
        }
        else if (n->op == IR_LABEL) {
            /*
             * IR_LABEL: record current buffer position for Phase B backpatching.
             * No bytes emitted — label is a pure address marker.
             */
            if (n_labels < JIT_MAX_LABELS) {
                strncpy(label_sites[n_labels].label, n->label, IR_LABEL_MAX - 1);
                label_sites[n_labels].label[IR_LABEL_MAX - 1] = '\0';
                label_sites[n_labels].offset = buf.size;
                n_labels++;
            }
        }
        else if (n->op == IR_LOAD || n->op == IR_STORE) {
            if (n->op == IR_LOAD && n->src1[0] != '\0' && strcmp(n->src1, "t0") == 0) {
                /*
                 * WARDEN INTERCEPT: CALLDATALOAD
                 * %r14 is pinned to mem_v2->calldata.
                 * Assume offset is pre-calculated in %rcx (standard ZCC binop dest)
                 * or immediate in node->imm. (ZCC uses imm if folded, otherwise 0 for stubbed t0 load).
                 */
                if (n->imm >= 0) {
                    // mov rax, [r14 + disp32] -> 0x4d 0x8b 0x86 [disp32]
                    uint8_t load_imm[] = {0x4d, 0x8b, 0x86, 0x00, 0x00, 0x00, 0x00};
                    uint32_t off32 = (uint32_t)n->imm;
                    memcpy(load_imm + 3, &off32, 4);
                    jit_emit(&buf, load_imm, 7);
                } else {
                    // offset in %rcx: mov rax, [r14 + rcx] -> 0x4b 0x8b 0x04 0x0e
                    uint8_t load_reg[] = {0x4b, 0x8b, 0x04, 0x0e};
                    jit_emit(&buf, load_reg, 4);
                }
                
                // EVM is Big-Endian: immediately bswapq to host Little-Endian
                uint8_t bswap[] = {0x48, 0x0f, 0xc8}; // bswapq %rax
                jit_emit(&buf, bswap, 3);
            } else {
                /* Phase 4: r13 is pinned. jit_emit_memory_op owns null/bounds guards. */
                extern void jit_emit_memory_op(JITBuffer* buf, ir_node_t* node, void* mem);
                jit_emit_memory_op(&buf, n, mem_v2);
            }
        }
        else if (n->op == IR_RET) {
            /* Epilogue — restore callee-saved registers in reverse push order:
             *   pop r14          : 0x41 0x5e
             *   pop r13          : 0x41 0x5d
             *   mov rsp, rbp    : 0x48 0x89 0xec
             *   pop rbp         : 0x5d
             *   ret             : 0xc3
             */
            uint8_t epilogue[] = {
                0x41, 0x5e,         /* pop r14      */
                0x41, 0x5d,         /* pop r13      */
                0x48, 0x89, 0xec,   /* mov rsp, rbp */
                0x5d,               /* pop rbp      */
                0xc3                /* ret          */
            };
            jit_emit(&buf, epilogue, sizeof(epilogue));
        }
        else {
            /* Unhandled: emit NOP for safe forward progress */
            uint8_t nop = 0x90;
            jit_emit(&buf, &nop, 1);
        }
    }

    /* ── Phase B: Backpatch forward branch rel32 fields ── */
    for (int pi = 0; pi < n_patches; pi++) {
        size_t patch_pos = patches[pi].patch_pos;
        /* Find matching label */
        size_t target = 0;
        int found = 0;
        for (int li = 0; li < n_labels; li++) {
            if (strcmp(label_sites[li].label, patches[pi].label) == 0) {
                target = label_sites[li].offset;
                found = 1;
                break;
            }
        }
        if (!found) {
            fprintf(stderr, "[JIT WARN] backpatch: label '%s' not found — branch left as 0\n",
                    patches[pi].label);
            continue;
        }
        /* rel32 = target - (patch_pos + 4)
         * patch_pos points to byte 0 of rel32 field.
         * +4 = width of rel32: CPU computes RIP from end of instruction. */
        int32_t rel32 = (int32_t)((int64_t)target - (int64_t)(patch_pos + 4));
        memcpy(buf.code + patch_pos, &rel32, 4);
        fprintf(stderr, "[JIT PATCH] '%s' → offset %zu  rel32=%d\n",
                patches[pi].label, target, rel32);
    }

    /* Fallback epilogue — fires if no explicit IR_RET in stream. */
    {
        uint8_t fallback_epilogue[] = {
            0x41, 0x5e,         /* pop r14      */
            0x41, 0x5d,         /* pop r13      */
            0x48, 0x89, 0xec,   /* mov rsp, rbp */
            0x5d,               /* pop rbp      */
            0xc3                /* ret          */
        };
        jit_emit(&buf, fallback_epilogue, sizeof(fallback_epilogue));
    }

    /* Flip the page: WRITE → READ|EXEC. No W^X aliasing. */
    if (mprotect(buf.code, buf.capacity, PROT_READ|PROT_EXEC) != 0) {
        perror("[WARDEN FATAL] mprotect PROT_EXEC failed");
        munmap(buf.code, buf.capacity);
        return NULL;
    }
    return buf.code;
}

/*
 * jit_exec — invoke a compiled EVM function page.
 * Casts the returned exec_page to a no-arg void function pointer and calls it.
 * Caller is responsible for ensuring exec_page came from evm_jit_compile().
 */
void jit_exec(void* exec_page) {
    if (!exec_page) {
        fprintf(stderr, "[WARDEN FATAL] jit_exec: NULL exec page.\n");
        return;
    }
    typedef void (*jit_fn_t)(void);
    jit_fn_t fn = (jit_fn_t)exec_page;
    fn();
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
            fwrite(jitted_code, 1, 64*1024, out); // Simplification: dump max cap
            fclose(out);
        }
    }
    
    evm_lifter_destroy(&ls);
}
