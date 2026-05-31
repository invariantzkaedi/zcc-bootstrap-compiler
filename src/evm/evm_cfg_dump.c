/*
 * evm_cfg_dump.c — EVM Bytecode CFG Edge Extractor
 * =================================================
 * Performs a two-pass walk of raw EVM bytecode to extract the exact
 * control-flow graph as a flat JSON edge list:
 *
 *   Pass 1: Bitmap all JUMPDEST offsets into valid_dest[].
 *   Pass 2: Walk instructions, emit JSON edge records for:
 *             JUMP  (unconditional) → static target or UNKNOWN
 *             JUMPI (conditional)   → taken + fallthrough edges
 *             STOP / RETURN / REVERT / INVALID → terminal edges
 *             all others            → implicit fallthrough edge
 *
 * Output schema (one JSON array to stdout or FILE*):
 *   [
 *     {"src_pc":"0x00","dst_pc":"0x0a","weight":1.0,"type":"FALLTHROUGH",
 *      "tag":"NONE"},
 *     ...
 *   ]
 *
 * The "tag" field carries any evm_ir_tag_t annotation for security-relevant
 * opcodes (CALL, SSTORE, etc.) so the Hamiltonian builder can weight those
 * edges higher in the energy matrix.
 *
 * Compile standalone:
 *   gcc -I../.. -O2 -o evm_cfg_dump evm_cfg_dump.c -lm
 */

#include "evm_cfg_dump.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ── EVM opcode constants (local, no header dep) ─────────────────────── */
#define OP_STOP         0x00
#define OP_JUMPDEST     0x5b
#define OP_JUMP         0x56
#define OP_JUMPI        0x57
#define OP_PUSH1        0x60
#define OP_PUSH32       0x7f
#define OP_PUSH0        0x5f
#define OP_RETURN       0xf3
#define OP_REVERT       0xfd
#define OP_INVALID      0xfe
#define OP_SELFDESTRUCT 0xff
#define OP_CALL         0xf1
#define OP_DELEGATECALL 0xf4
#define OP_CALLCODE     0xf2
#define OP_STATICCALL   0xfa
#define OP_SSTORE       0x55
#define OP_SLOAD        0x54
#define OP_SHA3         0x20
#define OP_CREATE       0xf0
#define OP_CREATE2      0xf5

/* ── Security weight table ────────────────────────────────────────────── */
/* Returns an energy weight (1.0 = normal, >1.0 = elevated risk). */
static double opcode_energy_weight(unsigned int op) {
    switch (op) {
        case OP_CALL:         return 4.5;  /* untrusted external call      */
        case OP_DELEGATECALL: return 5.0;  /* highest risk: delegates ctx  */
        case OP_CALLCODE:     return 4.8;  /* delegates balance, not ctx   */
        case OP_STATICCALL:   return 1.5;  /* read-only, lower risk        */
        case OP_SSTORE:       return 3.5;  /* persistent state write       */
        case OP_CREATE:       return 3.0;  /* contract deployment          */
        case OP_CREATE2:      return 3.2;  /* deterministic deployment     */
        case OP_SELFDESTRUCT: return 5.0;  /* destructive                  */
        case OP_SHA3:         return 1.2;  /* hash: moderate               */
        case OP_SLOAD:        return 1.3;  /* storage read                 */
        case OP_JUMPI:        return 1.1;  /* conditional branch           */
        default:              return 1.0;
    }
}

static const char *opcode_tag(unsigned int op) {
    switch (op) {
        case OP_CALL:         return "UNTRUSTED_CALL";
        case OP_DELEGATECALL: return "DELEGATECALL";
        case OP_CALLCODE:     return "CALLCODE";
        case OP_STATICCALL:   return "STATIC_CALL";
        case OP_SSTORE:       return "SSTORE";
        case OP_CREATE:       return "CREATE";
        case OP_CREATE2:      return "CREATE2";
        case OP_SELFDESTRUCT: return "SELFDESTRUCT";
        case OP_SHA3:         return "SHA3";
        default:              return "NONE";
    }
}

/* ── PUSH immediate size (bytes after opcode) ─────────────────────────── */
static int push_imm_size(unsigned int op) {
    if (op >= OP_PUSH1 && op <= OP_PUSH32)
        return (int)(op - OP_PUSH1 + 1);
    return 0;
}

/* ── Read a big-endian immediate value from bytecode ─────────────────── */
static int64_t read_push_imm(const unsigned char *bc, int len, int pc, int imm_sz) {
    int64_t val = 0;
    for (int i = 0; i < imm_sz && (pc + i) < len; i++) {
        val = (val << 8) | bc[pc + i];
    }
    return val;
}

/* ── Pass 1: Bitmap valid JUMPDEST offsets ────────────────────────────── */
static unsigned char *build_jumpdest_bitmap(const unsigned char *bc, int len) {
    unsigned char *bm = (unsigned char *)calloc(len, 1);
    if (!bm) return NULL;
    int pc = 0;
    while (pc < len) {
        unsigned int op = bc[pc];
        if (op == OP_JUMPDEST)
            bm[pc] = 1;
        int imm = push_imm_size(op);
        pc += 1 + imm;
    }
    return bm;
}

/* ── Pass 2: Emit CFG edges as JSON ──────────────────────────────────── */
int evm_cfg_dump(const unsigned char *bc, int len, FILE *out) {
    if (!bc || len <= 0 || !out) return -1;

    unsigned char *jd = build_jumpdest_bitmap(bc, len);
    if (!jd) return -1;

    /* Track the last PUSH value so JUMP/JUMPI can resolve static targets */
    int64_t last_push_val  = -1;
    int     last_push_valid = 0;
    int     edge_count      = 0;

    fprintf(out, "[\n");

    int pc = 0;
    while (pc < len) {
        unsigned int op  = (unsigned int)bc[pc];
        int          src = pc;
        int          imm = push_imm_size(op);
        int          next_pc = pc + 1 + imm;
        double       weight  = opcode_energy_weight(op);
        const char  *tag     = opcode_tag(op);

        /* ── Track last PUSH immediate for static jump resolution ── */
        if (imm > 0) {
            last_push_val   = read_push_imm(bc, len, pc + 1, imm);
            last_push_valid = 1;
        } else if (op == OP_PUSH0) {
            last_push_val   = 0;
            last_push_valid = 1;
        }

        /* ── Emit edges based on control-flow semantics ── */
        if (op == OP_JUMP) {
            /* Unconditional jump: target is top-of-stack */
            if (edge_count > 0) fprintf(out, ",\n");
            if (last_push_valid && last_push_val >= 0 && last_push_val < len
                && jd[(int)last_push_val]) {
                fprintf(out,
                    "  {\"src_pc\":\"0x%x\",\"dst_pc\":\"0x%llx\","
                    "\"weight\":%.2f,\"type\":\"JUMP\",\"tag\":\"%s\"}",
                    src, (long long)last_push_val, weight, tag);
            } else {
                /* Indirect / unresolvable jump */
                fprintf(out,
                    "  {\"src_pc\":\"0x%x\",\"dst_pc\":\"UNKNOWN\","
                    "\"weight\":%.2f,\"type\":\"JUMP_INDIRECT\",\"tag\":\"%s\"}",
                    src, weight, tag);
            }
            edge_count++;
            last_push_valid = 0;

        } else if (op == OP_JUMPI) {
            /* Conditional: emit taken + fallthrough */
            if (edge_count > 0) fprintf(out, ",\n");
            if (last_push_valid && last_push_val >= 0 && last_push_val < len
                && jd[(int)last_push_val]) {
                fprintf(out,
                    "  {\"src_pc\":\"0x%x\",\"dst_pc\":\"0x%llx\","
                    "\"weight\":%.2f,\"type\":\"JUMPI_TAKEN\",\"tag\":\"%s\"}",
                    src, (long long)last_push_val, weight, tag);
            } else {
                fprintf(out,
                    "  {\"src_pc\":\"0x%x\",\"dst_pc\":\"UNKNOWN\","
                    "\"weight\":%.2f,\"type\":\"JUMPI_TAKEN_INDIRECT\",\"tag\":\"%s\"}",
                    src, weight, tag);
            }
            edge_count++;
            /* Fallthrough (condition false) */
            if (next_pc < len) {
                fprintf(out, ",\n");
                fprintf(out,
                    "  {\"src_pc\":\"0x%x\",\"dst_pc\":\"0x%x\","
                    "\"weight\":0.20,\"type\":\"JUMPI_FALLTHROUGH\",\"tag\":\"NONE\"}",
                    src, next_pc);
                edge_count++;
            }
            last_push_valid = 0;

        } else if (op == OP_STOP || op == OP_RETURN ||
                   op == OP_REVERT || op == OP_INVALID || op == OP_SELFDESTRUCT) {
            /* Terminal: emit a TERMINAL edge to sentinel -1 */
            if (edge_count > 0) fprintf(out, ",\n");
            const char *ttype =
                op == OP_RETURN      ? "RETURN"      :
                op == OP_REVERT      ? "REVERT"      :
                op == OP_SELFDESTRUCT? "SELFDESTRUCT" :
                op == OP_INVALID     ? "INVALID"     : "STOP";
            fprintf(out,
                "  {\"src_pc\":\"0x%x\",\"dst_pc\":\"TERMINAL\","
                "\"weight\":%.2f,\"type\":\"%s\",\"tag\":\"%s\"}",
                src, weight, ttype, tag);
            edge_count++;
            last_push_valid = 0;

        } else {
            /* Normal fallthrough — only emit if there is a valid next pc */
            if (next_pc < len) {
                /* Only emit for non-trivial ops — skip pure data PUSHes
                 * to keep the edge list compact.  Always emit for security ops. */
                int is_security = (weight > 1.0);
                if (is_security || imm == 0) {
                    if (edge_count > 0) fprintf(out, ",\n");
                    fprintf(out,
                        "  {\"src_pc\":\"0x%x\",\"dst_pc\":\"0x%x\","
                        "\"weight\":%.2f,\"type\":\"FALLTHROUGH\",\"tag\":\"%s\"}",
                        src, next_pc, weight, tag);
                    edge_count++;
                }
            }
        }

        pc = next_pc;
    }

    fprintf(out, "\n]\n");
    free(jd);
    return edge_count;
}
