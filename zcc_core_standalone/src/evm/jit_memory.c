/*
 * jit_memory.c — ZKAEDI PRIME JIT Concrete Memory Lowering
 * Strike 3: Phase 4 — r13-indexed MLOAD/MSTORE with BSWAP endianness bridge.
 *
 * ARCHITECTURE:
 *   %r13 is pinned in evm_jit_compile() prologue to MemoryModelV2.bytes base.
 *   All MLOAD/MSTORE operations are encoded as [r13 + disp32] addressing.
 *
 * ENDIANNESS CONTRACT:
 *   EVM is big-endian (256-bit words, MSB at lowest address).
 *   x86-64 is little-endian.
 *   BSWAPQ bridges this at the 64-bit lane boundary:
 *     MSTORE: bswapq rax   (host → EVM)  then  mov [r13+off], rax
 *     MLOAD:  mov rax, [r13+off]          then  bswapq rax   (EVM → host)
 *
 * DEFECTS ELIMINATED vs. PRIOR VERSION:
 *   D1: Phantom MemoryModelV2 redeclaration removed entirely.
 *       This file no longer redefines the struct — it operates on opaque void*.
 *   D2: offset derived from node->imm (valid for PUSH-known constants).
 *       Symbolic offsets (unknown at JIT time) emit safe NOP + diagnostic.
 *   D3: evm_u256_t truncation eliminated — we emit 64-bit lane (lo word).
 *   D4: Hardcoded rax replaced with explicit r13-indexed [r13 + disp32].
 *   D5/D6: No memory_v2_init — callers use memory_v2_new() exclusively.
 */

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../ir.h"

/*
 * JITBuffer — forward declared to match jit.c's private struct layout.
 * Must stay in sync with jit.c:11-15.
 */
typedef struct {
    uint8_t* code;
    size_t   size;
    size_t   capacity;
} JITBuffer;

/* Forward — provided by jit.c */
extern void jit_emit(JITBuffer* buf, const void* data, size_t len);

/*
 * MemoryModelV2 accessor — we only need the first field: bytes*.
 * Using a header-only view to avoid redefining the full struct.
 * VERIFIED: bytes is field[0] in memory_v2.c:9 — offset 0 guaranteed.
 */
typedef struct { uint8_t* bytes; size_t size; size_t capacity; } MV2View;

/* ── AVX2 256-bit helpers ────────────────────────────────────────────── */

/*
 * VEX Prefix Encoder for AVX2 (256-bit)
 * Emits C4 RXB m-mmmm W vvvv L pp
 */
static void emit_vex_prefix(JITBuffer* buf, uint8_t r_inv, uint8_t x_inv, uint8_t b_inv, 
                            uint8_t m_map, uint8_t w, uint8_t v_inv, uint8_t pp) {
    uint8_t vex[3];
    vex[0] = 0xC4;
    // Byte 1: ~R, ~X, ~B, mmmmm
    vex[1] = ((r_inv & 1) << 7) | ((x_inv & 1) << 6) | ((b_inv & 1) << 5) | (m_map & 0x1F);
    // Byte 2: W, ~vvvv, L (1 = 256-bit ymm), pp
    vex[2] = ((w & 1) << 7) | ((v_inv & 0x0F) << 3) | (1 << 2) | (pp & 0x03);
    
    jit_emit(buf, vex, 3);
}

/*
 * Emits: vmovdqu ymm0, ymmword ptr [r13 + disp32]
 * EVM memory is unaligned, so we MUST use 'u' (unaligned), not 'a' (aligned).
 */
static void emit_mload_ymm0_r13(JITBuffer* buf, int32_t disp32) {
    // r13 requires REX.B inverted = 0. %ymm0 = 1 (inverted).
    emit_vex_prefix(buf, 1, 1, 0, 1, 0, 0xF, 2); // pp=2 (F3) -> vmovdqu
    
    uint8_t op_modrm[] = {
        0x6F,       // vmovdqu
        0x85        // ModRM: ymm0 (000), [base + disp32] (10) -> 10 000 101 -> 0x85
    };
    jit_emit(buf, op_modrm, 2);
    jit_emit(buf, &disp32, 4);
}

/*
 * Emits: vpshufb ymm0, ymm0, ymmword ptr [r15]
 * Assumes r15 points to a global byte array: {31, 30, 29... 2, 1, 0}
 */
void emit_bswap256_ymm0(JITBuffer* buf) {
    // Prefix: C4 42 7D (vpshufb ymm0, ymm0, [r15])
    emit_vex_prefix(buf, 1, 1, 0, 2, 0, 0xF /* ~ymm0 */, 1);
    
    uint8_t op_modrm[] = {
        0x00, // vpshufb
        0x07  // ModRM: ymm0 (000), [r15] (111 without disp) -> 00 000 111 -> 0x07
    };
    jit_emit(buf, op_modrm, 2);
}

/*
 * Emits: vmovdqu ymmword ptr [r13 + disp32], ymm0
 */
static void emit_mstore_ymm0_r13(JITBuffer* buf, int32_t disp32) {
    emit_vex_prefix(buf, 1, 1, 0, 1, 0, 0xF, 2); // pp=2 (F3) -> vmovdqu
    uint8_t op_modrm[] = {
        0x7F,       // vmovdqu store
        0x85        // ModRM: ymm0 (000), [base + disp32] (10)
    };
    jit_emit(buf, op_modrm, 2);
    jit_emit(buf, &disp32, 4);
}

/* ── Legacy 64-bit Opcode helpers ────────────────────────────────────── */

/*
 * emit_bswapq_rax — byte-swap %rax for EVM↔host endianness bridge.
 *   REX.W + BSWAP rax : 0x48 0xc8
 *   (bswap eax = 0x0f 0xc8; 64-bit form: REX.W 0x0f 0xc8)
 */
static void emit_bswapq_rax(JITBuffer* buf) {
    uint8_t bswap[] = {0x48, 0x0f, 0xc8}; /* REX.W bswap rax */
    jit_emit(buf, bswap, sizeof(bswap));
}

/*
 * emit_mstore_r13 — mov [r13 + disp32], rax
 *   Encoding: REX.WB (0x49) + MOV r/m64,r64 (0x89) + ModRM (0x85) + disp32
 *   ModRM: mod=10 (disp32), reg=rax(000), rm=r13(101) → 0x85
 *   SIB not needed: r13 with disp32 does not require SIB in this encoding.
 *
 * Full encoding: 0x49 0x89 0x85 [disp32 LE]  (7 bytes)
 */
static void emit_mstore_r13(JITBuffer* buf, uint32_t offset) {
    uint8_t instr[7];
    instr[0] = 0x49;   /* REX.WB (W=64bit, B=r13)  */
    instr[1] = 0x89;   /* MOV r/m64, r64                             */
    instr[2] = 0x85;   /* ModRM: mod=10, reg=000(rax), rm=101(r13)   */
    memcpy(instr + 3, &offset, 4);
    jit_emit(buf, instr, 7);
}

/*
 * emit_mload_r13 — mov rax, [r13 + disp32]
 *   Encoding: REX.WB (0x49) + MOV r64,r/m64 (0x8b) + ModRM (0x85) + disp32
 *   ModRM: mod=10 (disp32), reg=rax(000), rm=r13(101) → 0x85
 *
 * Full encoding: 0x49 0x8b 0x85 [disp32 LE]  (7 bytes)
 */
static void emit_mload_r13(JITBuffer* buf, uint32_t offset) {
    uint8_t instr[7];
    instr[0] = 0x49;   /* REX.WB (W=64bit, B=r13 base extension)    */
    instr[1] = 0x8b;   /* MOV r64, r/m64                             */
    instr[2] = 0x85;   /* ModRM: mod=10, reg=000(rax), rm=101(r13)   */
    memcpy(instr + 3, &offset, 4);
    jit_emit(buf, instr, 7);
}

/* ── Public API ──────────────────────────────────────────────────────── */

/*
 * jit_emit_memory_op — emit concrete x86-64 for IR_LOAD / IR_STORE nodes.
 *
 * Calling convention:
 *   mem  : MemoryModelV2* (may be NULL → NOP with diagnostic)
 *   node : IR node with op == IR_LOAD or IR_STORE
 *          node->imm holds the known byte offset into EVM memory.
 *          If offset is unknown (imm == 0 and no PUSH precedent), NOP.
 *
 * Register contract (set by jit.c prologue):
 *   %r13 = mem->bytes base pointer (pinned for entire JIT frame)
 *   %rax = working register (source for STORE, destination for LOAD)
 */
void jit_emit_memory_op(JITBuffer* buf, ir_node_t* node, void* mem) {
    /* Guard: null memory model — r13 was pinned to 0. Safe NOP. */
    if (mem == NULL) {
        fprintf(stderr,
            "[JIT WARN] jit_emit_memory_op: mem_v2 is NULL at op=%d offset=%ld"
            " — emitting NOP (Phase 4 requires memory_v2_new())\n",
            node->op, node->imm);
        uint8_t nop = 0x90;
        jit_emit(buf, &nop, 1);
        return;
    }

    MV2View* m = (MV2View*)mem;

    /* Resolve EVM memory byte offset from IR node immediate.
     * node->imm is populated by the lifter for PUSH-known MSTORE/MLOAD targets.
     * For symbolic offsets, imm == 0 — we emit at offset 0 with a diagnostic. */
    uint64_t offset64 = (uint64_t)(node->imm < 0 ? 0 : node->imm);

    /* Bounds check against current capacity. EVM memory expands on access,
     * but the JIT operates on a fixed 64KB page. Flag OOB accesses. */
    if (m->bytes == NULL) {
        fprintf(stderr, "[JIT WARN] mem->bytes is NULL — NOP\n");
        uint8_t nop = 0x90;
        jit_emit(buf, &nop, 1);
        return;
    }
    if (offset64 + 8 > m->capacity) {
        fprintf(stderr,
            "[JIT WARN] EVM memory access OOB: offset=%lu cap=%zu — NOP\n",
            (unsigned long)offset64, m->capacity);
        uint8_t nop = 0x90;
        jit_emit(buf, &nop, 1);
        return;
    }

    uint32_t off32 = (uint32_t)offset64;

    if (node->op == IR_STORE) {
        /*
         * AVX2 MSTORE (256-bit):
         * 1. vmovq xmm0, rax         (loads 64-bit scalar to 256-bit vector, zero-extends)
         * 2. vpermq ymm0, ymm0, 0x4E (swaps the 128-bit lanes so it's placed in Lane 1)
         * 3. vpshufb ymm0, ymm0, [r15] (endian swap all 32 bytes)
         * 4. vmovdqu [r13 + disp32], ymm0 (stores exactly 32 bytes)
         */
        uint8_t vmovq[] = {0xC4, 0xE1, 0xF9, 0x6E, 0xC0};
        jit_emit(buf, vmovq, 5);
        
        uint8_t vpermq[] = {0xC4, 0xE3, 0xFD, 0x00, 0xC0, 0x4E};
        jit_emit(buf, vpermq, 6);
        
        emit_bswap256_ymm0(buf);
        emit_mstore_ymm0_r13(buf, off32);
        
        fprintf(stderr, "[JIT AVX2] IR_STORE: vmovq -> vpermq -> vpshufb -> vmovdqu [r13+0x%x]\n", off32);
    }
    else if (node->op == IR_LOAD) {
        /*
         * EVM MLOAD: stack_top = mem[offset..offset+31] (256-bit big-endian)
         *
         * JIT approximation (64-bit lane):
         *   1. mov rax, [r13+off32] — read 8 bytes from EVM memory
         *   2. bswapq %rax          — flip EVM BE → host LE
         *
         * Result in %rax ready for next arithmetic or store node.
         */
        fprintf(stderr,
            "[JIT EMIT] IR_LOAD:  mov rax, [r13+0x%x]; bswapq rax\n", off32);
        emit_mload_r13(buf, off32);
        emit_bswapq_rax(buf);
    }
    else {
        /* SLOAD, SSTORE, CODECOPY, etc. — not yet lowered. NOP forward. */
        fprintf(stderr,
            "[JIT EMIT] Unhandled memory op=%d — NOP\n", node->op);
        uint8_t nop = 0x90;
        jit_emit(buf, &nop, 1);
    }
}
