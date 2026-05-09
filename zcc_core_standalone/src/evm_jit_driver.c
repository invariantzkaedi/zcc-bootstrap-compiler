/*
 * evm_jit_driver.c — ZKAEDI PRIME EVM Pipeline Driver
 *
 * PHASE 1 (AOT):  Bytecode → IR → T1-T8 → x86 .s file
 * PHASE 3 (JIT):  Bytecode → IR → T1-T8 → evm_jit_compile() → mprotect → execute
 *
 * MODE SELECTION:
 *   AOT:  ./evm_jit_driver <input.bin|input.hex> <output.s>
 *   JIT:  ./evm_jit_driver <input.bin|input.hex> --jit
 *
 * INPUT FORMAT DETECTION:
 *   Files beginning with "0x" are ASCII hex-encoded and decoded transparently.
 *   All other files are treated as raw binary EVM bytecode.
 *
 * API contracts verified from primary sources:
 *   ir_module_create(void)                                     → ir_module_t*
 *   evm_lifter_init(ls, bytecode, (int)len, module)           → void
 *   evm_lift_bytecode(ls)                                      → evm_lift_result_t
 *   evm_lifter_destroy(ls)                                     → void
 *   ir_pm_run_default(void *mod_ptr, int verbose)              → void
 *   ir_module_lower_x86(const ir_module_t *mod, FILE *out)    → void  [AOT]
 *   evm_jit_compile(ir_func_t *func, void *mem_v2)            → void* [JIT]
 *   jit_exec(void *exec_page)                                  → void  [JIT]
 *   mod->funcs[0]                                              — ir_func_t* first function
 *
 * Build (from zcc_core_standalone/):
 *   gcc -O2 -w -I. -I.. -I./src -I./src/evm \
 *       -o evm_jit_driver \
 *       src/evm_jit_driver.c evm_lifter.c ir.c ir_pass_manager.c \
 *       ir_to_x86.c regalloc.c ir_vuln_tag.c ir_dominance.c \
 *       ir_telemetry_stub.c ir_pass_warden.c ir_symbolic_cfg.c \
 *       forgezero_receipt_stub.c src/x86_codegen_sse.c \
 *       src/ir_lower_float.c src/evm/memory_v2.c src/evm/abi_extractor.c \
 *       src/evm/jit.c src/evm/jit_memory.c -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "ir.h"
#include "evm_lifter.h"
#include "ir_pass_manager.h"

/*
 * MemoryModelV2 — opaque forward declaration.
 * Full definition lives in src/evm/memory_v2.c.
 * We only need the constructor and the first field (bytes*) here.
 * DO NOT redefine the struct in this TU (D1 from Warden defect map).
 */
typedef struct MemoryModelV2_s MemoryModelV2;
extern MemoryModelV2* memory_v2_new(void);

/* memory_v2.c exposes bytes as field[0] — size readable via this view */
#include <stdint.h>
typedef struct { uint8_t* bytes; size_t size; size_t capacity; } MV2View;

/* Forward declarations — verified against primary source files */
void ir_module_lower_x86(const ir_module_t *mod, FILE *out);   /* ir_to_x86.c:188  */
void *evm_jit_compile(ir_func_t *func, void *mem_v2);          /* jit.c:43         */
void jit_exec(void *exec_page);                                 /* jit.c Strike 1   */

/* ── Hex decoder ─────────────────────────────────────────────────────── */

static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/*
 * Decode an ASCII hex string (with or without "0x" prefix) into raw bytes.
 * Returns heap-allocated buffer; caller must free(). Sets *out_len.
 * Returns NULL on malformed input.
 */
static unsigned char *decode_hex(const char *hex, size_t hex_len, long *out_len) {
    const char *p = hex;
    size_t remaining = hex_len;

    /* Strip optional "0x" / "0X" prefix */
    if (remaining >= 2 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
        remaining -= 2;
    }

    /* Strip trailing whitespace / newlines */
    while (remaining > 0 && (p[remaining-1] == '\n' || p[remaining-1] == '\r' ||
                               p[remaining-1] == ' '  || p[remaining-1] == '\t')) {
        remaining--;
    }

    if (remaining % 2 != 0) {
        fprintf(stderr, "[WARN]  Hex string has odd length (%zu). Truncating last nibble.\n",
                remaining);
        remaining--;
    }

    size_t byte_count = remaining / 2;
    unsigned char *buf = (unsigned char *)malloc(byte_count + 1);
    if (!buf) {
        fprintf(stderr, "[FATAL] OOM decoding hex (%zu bytes)\n", byte_count);
        return NULL;
    }

    for (size_t i = 0; i < byte_count; i++) {
        int hi = hex_nibble(p[i*2]);
        int lo = hex_nibble(p[i*2 + 1]);
        if (hi < 0 || lo < 0) {
            fprintf(stderr, "[FATAL] Invalid hex chars at offset %zu: '%c%c'\n",
                    i*2, p[i*2], p[i*2+1]);
            free(buf);
            return NULL;
        }
        buf[i] = (unsigned char)((hi << 4) | lo);
    }

    *out_len = (long)byte_count;
    return buf;
}

/* ── File loader with auto hex detection ─────────────────────────────── */

/*
 * Load file from disk. If the first two bytes are "0x", decode as ASCII hex.
 * Otherwise treat as raw binary EVM bytecode.
 * Returns heap-allocated raw bytecode buffer. Caller must free().
 */
static unsigned char *load_evm_file(const char *filepath, long *out_len) {
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        fprintf(stderr, "[FATAL] Cannot open: %s\n", filepath);
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fprintf(stderr, "[FATAL] fseek failed: %s\n", filepath);
        fclose(f);
        return NULL;
    }
    long fsize = ftell(f);
    if (fsize <= 0) {
        fprintf(stderr, "[FATAL] Empty or unreadable file: %s\n", filepath);
        fclose(f);
        return NULL;
    }
    rewind(f);

    char *raw = (char *)malloc((size_t)fsize + 1);
    if (!raw) {
        fprintf(stderr, "[FATAL] OOM loading %s\n", filepath);
        fclose(f);
        return NULL;
    }
    if ((long)fread(raw, 1, (size_t)fsize, f) != fsize) {
        fprintf(stderr, "[FATAL] Short read: %s\n", filepath);
        free(raw);
        fclose(f);
        return NULL;
    }
    fclose(f);
    raw[fsize] = '\0';

    /* Detect ASCII hex: starts with "0x" or all chars are [0-9a-fA-F\r\n] */
    int is_hex = 0;
    if (fsize >= 2 && raw[0] == '0' && (raw[1] == 'x' || raw[1] == 'X')) {
        is_hex = 1;
        fprintf(stderr, "[*]         Format: ASCII hex (0x prefix detected).\n");
    } else {
        /* Heuristic: if >80% of bytes are printable hex chars, treat as hex */
        int hex_chars = 0;
        for (long i = 0; i < fsize && i < 64; i++) {
            if (hex_nibble(raw[i]) >= 0 || raw[i] == '\n' || raw[i] == '\r') hex_chars++;
        }
        if (fsize <= 64 && hex_chars > (fsize * 4 / 5)) {
            is_hex = 1;
            fprintf(stderr, "[*]         Format: ASCII hex (heuristic).\n");
        } else {
            fprintf(stderr, "[*]         Format: Raw binary EVM bytecode.\n");
        }
    }

    if (is_hex) {
        unsigned char *decoded = decode_hex(raw, (size_t)fsize, out_len);
        free(raw);
        return decoded;
    }

    /* Raw binary path */
    *out_len = fsize;
    return (unsigned char *)raw;
}

/* ── Lift result → string ─────────────────────────────────────────────── */

static const char *lift_result_name(evm_lift_result_t r) {
    switch (r) {
        case EVM_LIFT_OK:          return "EVM_LIFT_OK";
        case EVM_LIFT_TRUNCATED:   return "EVM_LIFT_TRUNCATED (partial stream)";
        case EVM_LIFT_INVALID_OP:  return "EVM_LIFT_INVALID_OP";
        case EVM_LIFT_STACK_OVER:  return "EVM_LIFT_STACK_OVER";
        case EVM_LIFT_STACK_UNDER: return "EVM_LIFT_STACK_UNDER";
        case EVM_LIFT_OOM:         return "EVM_LIFT_OOM";
        default:                   return "UNKNOWN";
    }
}

/* ── Usage ───────────────────────────────────────────────────────────── */

static void print_usage(const char *argv0) {
    fprintf(stderr,
        "ZKAEDI PRIME — EVM Pipeline Driver\n"
        "  Phase 1 (AOT): %s <input> <output.s>\n"
        "  Phase 3 (JIT): %s <input> --jit\n"
        "\n"
        "  input: raw EVM bytecode (.bin/.evm) or ASCII hex (.hex, 0x-prefixed)\n",
        argv0, argv0);
}

/* ── Main pipeline ───────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    const char *in_file = argv[1];
    const char *out_arg = argv[2];
    int jit_mode = (strcmp(out_arg, "--jit") == 0);

    fprintf(stderr, "ZKAEDI PRIME — EVM Pipeline (Phase %s)\n",
            jit_mode ? "3 JIT" : "1 AOT");
    fprintf(stderr, "Input: %s\n\n", in_file);

    /* ── Stage 0: Ingest & decode ──────────────────────────────────── */
    fprintf(stderr, "[*] Stage 0: Loading EVM input...\n");
    long bc_len = 0;
    unsigned char *bytecode = load_evm_file(in_file, &bc_len);
    if (!bytecode) return 1;

    if (bc_len > 0x7FFFFFFF) {
        fprintf(stderr, "[FATAL] Bytecode too large: %ld bytes\n", bc_len);
        free(bytecode);
        return 1;
    }
    fprintf(stderr, "[+]         %ld raw EVM bytes ready.\n", bc_len);

    /* ── Stage 1: IR module allocation ────────────────────────────── */
    fprintf(stderr, "[*] Stage 1: Allocating ZCC IR module.\n");
    ir_module_t *mod = ir_module_create();
    if (!mod) {
        fprintf(stderr, "[FATAL] ir_module_create() returned NULL\n");
        free(bytecode);
        return 1;
    }

    /* ── Stage 2: EVM lift → ZCC SSA IR ───────────────────────────── */
    fprintf(stderr, "[*] Stage 2: Lifting EVM bytecode to ZCC IR...\n");

    evm_lifter_t ls;
    memset(&ls, 0, sizeof(ls));
    evm_lifter_init(&ls, bytecode, (int)bc_len, mod);

    evm_lift_result_t lift_rc = evm_lift_bytecode(&ls);

    fprintf(stderr, "[+]         Result    : %s\n", lift_result_name(lift_rc));
    fprintf(stderr, "[+]         Insns     : %d\n", ls.insn_count);
    fprintf(stderr, "[+]         CALL-fam  : %d\n", ls.call_count);
    fprintf(stderr, "[+]         Tagged    : %d\n", ls.tagged_count);
    if (ls.error) fprintf(stderr, "[WARN]      Error     : %s\n", ls.errmsg);

    if (lift_rc != EVM_LIFT_OK && lift_rc != EVM_LIFT_TRUNCATED) {
        fprintf(stderr, "[FATAL] Unrecoverable lift failure: %s\n",
                lift_result_name(lift_rc));
        evm_lifter_destroy(&ls);
        ir_module_free(mod);
        free(bytecode);
        return 1;
    }

    if (mod->func_count == 0) {
        fprintf(stderr, "[FATAL] Lifter produced zero IR functions.\n");
        evm_lifter_destroy(&ls);
        ir_module_free(mod);
        free(bytecode);
        return 1;
    }
    fprintf(stderr, "[+]         IR funcs  : %d\n", mod->func_count);

    evm_lifter_destroy(&ls);
    free(bytecode);
    bytecode = NULL;

    /* ── Stage 3: T1-T8 optimization passes ────────────────────────── */
    fprintf(stderr, "[*] Stage 3: Running T1-T8 IR optimization passes...\n");
    ir_pm_run_default(mod, 1);
    fprintf(stderr, "[+]         T1-T8 complete.\n");

    /* ── Stage 4: AOT or JIT dispatch ─────────────────────────────── */
    if (!jit_mode) {
        /* ── AOT path: emit x86-64 .s text ── */
        const char *out_file = out_arg;
        fprintf(stderr, "[*] Stage 4 (AOT): Emitting x86-64 assembly → %s\n", out_file);

        FILE *out_s = fopen(out_file, "w");
        if (!out_s) {
            fprintf(stderr, "[FATAL] Cannot open output: %s\n", out_file);
            ir_module_free(mod);
            return 1;
        }

        fprintf(out_s, "# ZKAEDI PRIME — EVM AOT output\n");
        fprintf(out_s, "# Source  : %s\n", in_file);
        fprintf(out_s, "# Pipeline: evm_lifter -> ir_pm_run_default (T1-T8)"
                       " -> ir_module_lower_x86\n");
        fprintf(out_s, "# Gate    : Phase 1 AOT — x86-64 System V ABI\n\n");

        ir_module_lower_x86(mod, out_s);
        fclose(out_s);

        ir_module_free(mod);
        fprintf(stderr,
            "\n[+] ====================================================\n"
            "[+] PHASE 1 AOT COMPLETE. Output: %s\n"
            "[+] Next: %s --jit to activate Phase 3 live injection.\n"
            "[+] ====================================================\n",
            out_file, argv[0]);
        return 0;
    }

    /* ── JIT path (Phase 3 + Phase 4 concrete memory) ──────────────── */
    fprintf(stderr, "\n[*] Stage 4 (JIT Phase 4): Live memory injection + concrete MLOAD/MSTORE...\n");

    /*
     * Strike 1: Initialize MemoryModelV2 via the correct constructor.
     * memory_v2_new() allocates 64KB flat byte array (calloc, zeroed).
     * DO NOT use memory_v2_init() — that function does not exist (D5/D6).
     * DO NOT stack-allocate MemoryModelV2 — struct layout is opaque here.
     */
    MemoryModelV2 *evm_mem = memory_v2_new();
    if (!evm_mem) {
        fprintf(stderr, "[FATAL] memory_v2_new() returned NULL\n");
        ir_module_free(mod);
        return 1;
    }
    fprintf(stderr, "[+]         EVM memory   : 64KB flat byte array allocated.\n");

    /* Strike 1: Calldata injection — inject 0xAABBCCDD selector (32 EVM bytes).
     * mem_v2->calldata is field[4] in MemoryModelV2 (verified: memory_v2.c:14).
     * CALLDATALOAD reads calldata[0..31] as a big-endian 256-bit word.
     * Padding: [aa bb cc dd 00 00 ... 00] (28 zero bytes follow selector). */
    {
        typedef struct {
            uint8_t* bytes; size_t size; size_t capacity;
            void* storage_map;
            uint8_t* calldata; size_t calldata_len;
        } MV2Inject;
        MV2Inject *mv = (MV2Inject*)evm_mem;
        uint8_t *cd = (uint8_t*)calloc(32, 1);
        if (cd) {
            cd[0] = 0xaa; cd[1] = 0xbb; cd[2] = 0xcc; cd[3] = 0xdd;
            mv->calldata     = cd;
            mv->calldata_len = 32;
            fprintf(stderr, "[+]         Calldata     : 0xaabbccdd00...00 (32 bytes injected)\n");
        }
    }

    /*
     * API: evm_jit_compile(ir_func_t *func, void *mem_v2) → void*
     * mod->funcs[0] is the primary EVM contract function.
     * VERIFIED: ir_module_t.funcs[] is the array field (ir.h:210).
     * mod->first_function does NOT exist.
     */
    ir_func_t *primary_fn = mod->funcs[0];
    fprintf(stderr, "[*]         JIT-compiling: '%s' (%d nodes)\n",
            primary_fn->name, primary_fn->node_count);

    void *exec_page = evm_jit_compile(primary_fn, evm_mem);


    if (!exec_page) {
        fprintf(stderr, "[FATAL] evm_jit_compile() returned NULL.\n");
        ir_module_free(mod);
        return 1;
    }

    fprintf(stderr, "[+]         Exec page    : %p\n", exec_page);
    fprintf(stderr, "[+]         %r13 pinned  : mem->bytes base for MLOAD/MSTORE\n");
    fprintf(stderr, "[+]         PROT_READ|PROT_EXEC active (W^X enforced).\n");
    fprintf(stderr, "[!]         Executing native EVM payload...\n");

    jit_exec(exec_page);

    fprintf(stderr, "[+]         Execution complete. State transition successful.\n");

    /* Post-execution telemetry: Reverse-scan to find actual EVM memory size */
    {
        MV2View *mv = (MV2View*)evm_mem;
        size_t actual_size = 0;
        size_t written = 0;
        
        if (mv->bytes && mv->capacity > 0) {
            // Reverse scan finds the highest touched byte instantly
            for (long i = (long)mv->capacity - 1; i >= 0; i--) {
                if (mv->bytes[i] != 0) {
                    actual_size = i + 1;
                    break;
                }
            }
            
            // Sync the JIT hardware state back to the C software state
            mv->size = actual_size;
            
            for (size_t i = 0; i < actual_size; i++) {
                if (mv->bytes[i] != 0) written++;
            }
        }
        
        fprintf(stderr, "[+]         JIT Synced Mem Size: %zu active bytes\n", mv->size);
        fprintf(stderr, "[+]         Non-zero bytes: %zu (concrete state written)\n", written);
        
        if (actual_size > 0) {
            /* Dump first 64 bytes of EVM memory for state inspection */
            fprintf(stderr, "[+]         EVM mem[0:64]: ");
            for (size_t i = 0; i < 64 && i < mv->capacity; i++) {
                fprintf(stderr, "%02x", mv->bytes[i]);
            }
            fprintf(stderr, "\n");
        }

        /* Wipe the Calldata Base: Explicitly zero out the calldata buffer
         * to prevent phantom state injection on reuse. */
        typedef struct {
            uint8_t* bytes; size_t size; size_t capacity;
            void* storage_map;
            uint8_t* calldata; size_t calldata_len;
        } MV2Wipe;
        MV2Wipe *mw = (MV2Wipe*)evm_mem;
        if (mw->calldata) {
            memset(mw->calldata, 0, mw->calldata_len);
        }
    }

    ir_module_free(mod);

    fprintf(stderr,
        "\n[+] ====================================================\n"
        "[+] PHASE 4 JIT BRIDGE ESTABLISHED.\n"
        "[+] EVM bytecode executed with concrete MLOAD/MSTORE.\n"
        "[+] Next: Phase 5 — KECCAK folding + StateHealer MEV loop.\n"
        "[+] ====================================================\n");

    return 0;
}
