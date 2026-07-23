/*
 * test_evm_weaver_diff.c — Stateful EVM Differential Execution Gate (Gate 4-EVM)
 * =================================================================================
 * Honest stateful differential execution harness for EVM Yul weaver optimization.
 * Simulates EVM stack & storage operations across unoptimized (opt_level=0) and
 * optimized (opt_level=1) passes to verify Storage_opt == Storage_unopt identity.
 *
 * VERDICT PRECEDENCE CONTRACT:
 * 1. Storage key-value divergence  -> EXIT 1 (RED)   [Evaluated FIRST, unconditionally]
 * 2. rewrites_applied == 0          -> EXIT 2 (ORACLE-SUSPECT) [Evaluated SECOND]
 * 3. Exact Storage Identity & rewrites_applied > 0 -> EXIT 0 (GREEN) [Evaluated THIRD]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <assert.h>
#include "../ir.h"

extern int evm_yul_weaver_opt(ir_func_t *fn, FILE *out, int opt_level);

typedef struct {
    uint64_t stack[1024];
    int sp;
    uint64_t storage_key[64];
    uint64_t storage_val[64];
    int storage_count;
} evm_sim_t;

static void simulate_yul(const char *yul_file, evm_sim_t *sim) {
    memset(sim, 0, sizeof(*sim));
    FILE *f = fopen(yul_file, "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p && isspace((unsigned char)*p)) p++;

        uint64_t val = 0;
        int swap_n = 0;

        if (sscanf(p, "push32 0x%llx", (unsigned long long *)&val) == 1 ||
            sscanf(p, "push4 0x%llx", (unsigned long long *)&val) == 1 ||
            sscanf(p, "push1 0x%llx", (unsigned long long *)&val) == 1) {
            if (sim->sp < 1024) sim->stack[sim->sp++] = val;
        } else if (sscanf(p, "swap%d", &swap_n) == 1) {
            if (swap_n > 0 && sim->sp > swap_n) {
                int top = sim->sp - 1;
                int target = sim->sp - 1 - swap_n;
                uint64_t tmp = sim->stack[top];
                sim->stack[top] = sim->stack[target];
                sim->stack[target] = tmp;
            }
        } else if (strncmp(p, "pop", 3) == 0 && (isspace((unsigned char)p[3]) || p[3] == '\0')) {
            if (sim->sp > 0) sim->sp--;
        } else if (strncmp(p, "add", 3) == 0 && (isspace((unsigned char)p[3]) || p[3] == '\0')) {
            if (sim->sp >= 2) {
                uint64_t b = sim->stack[--sim->sp];
                uint64_t a = sim->stack[--sim->sp];
                sim->stack[sim->sp++] = a + b;
            }
        } else if (strncmp(p, "sub", 3) == 0 && (isspace((unsigned char)p[3]) || p[3] == '\0')) {
            if (sim->sp >= 2) {
                uint64_t b = sim->stack[--sim->sp];
                uint64_t a = sim->stack[--sim->sp];
                sim->stack[sim->sp++] = a - b;
            }
        } else if (strncmp(p, "mul", 3) == 0 && (isspace((unsigned char)p[3]) || p[3] == '\0')) {
            if (sim->sp >= 2) {
                uint64_t b = sim->stack[--sim->sp];
                uint64_t a = sim->stack[--sim->sp];
                sim->stack[sim->sp++] = a * b;
            }
        } else if (strncmp(p, "sstore", 6) == 0 && (isspace((unsigned char)p[6]) || p[6] == '\0')) {
            if (sim->sp >= 2 && sim->storage_count < 64) {
                uint64_t key = sim->stack[--sim->sp];
                uint64_t value = sim->stack[--sim->sp];
                sim->storage_key[sim->storage_count] = key;
                sim->storage_val[sim->storage_count] = value;
                sim->storage_count++;
            }
        }
    }
    fclose(f);
}

int main(void) {
    printf("=== GATE 4-EVM: INITIALIZING EVM STATEFUL DIFFERENTIAL HARNESS ===\n");

    /* Construct synthetic IR function with commutative ADD and 2 stack items at STORE */
    ir_func_t fn;
    memset(&fn, 0, sizeof(fn));
    
    ir_node_t n1 = {.op = IR_CONST, .dst = "v1", .imm = 0x100};
    ir_node_t n2 = {.op = IR_CONST, .dst = "v2", .imm = 0x42};
    ir_node_t n3 = {.op = IR_CONST, .dst = "v3", .imm = 0x10};
    ir_node_t n4 = {.op = IR_ADD, .src1 = "v2", .src2 = "v3", .dst = "v4"};
    ir_node_t n5 = {.op = IR_STORE, .src1 = "v4", .dst = "v1"}; /* sstore key v1 (0x100), val v4 (0x52) */
    
    fn.head = &n1;
    n1.next = &n2;
    n2.next = &n3;
    n3.next = &n4;
    n4.next = &n5;
    n5.next = NULL;

    /* 1. Emit unoptimized Yul (opt_level = 0) */
    const char *unopt_path = "/tmp/yul_unopt_output.yul";
    FILE *f_unopt = fopen(unopt_path, "w");
    if (!f_unopt) return 1;
    (void)evm_yul_weaver_opt(&fn, f_unopt, 0);
    fclose(f_unopt);

    /* 2. Emit optimized Yul (opt_level = 1) */
    const char *opt_path = "/tmp/yul_opt_output.yul";
    FILE *f_opt = fopen(opt_path, "w");
    if (!f_opt) return 1;
#ifdef TEST_BASELINE_BUILD
    int rewrites_applied = evm_yul_weaver_opt(&fn, f_opt, 0); /* Unmodified baseline pass-through -> rewrites=0 -> Exit 2 */
#else
    int rewrites_applied = evm_yul_weaver_opt(&fn, f_opt, 1);
#endif
    fclose(f_opt);

    /* 3. Simulate stack and storage state for both builds */
    evm_sim_t sim_unopt, sim_opt;
    simulate_yul(unopt_path, &sim_unopt);
    simulate_yul(opt_path, &sim_opt);

    /* --- PRECEDENCE CHECK 1: Storage Key-Value Divergence (EXIT 1 - RED) --- */
    if (sim_unopt.storage_count != sim_opt.storage_count) {
        fprintf(stderr, "Gate 4-EVM FAULT DETECTED: Storage op count mismatch (%d vs %d).\n",
                sim_unopt.storage_count, sim_opt.storage_count);
        printf("VERDICT: GATE 4-EVM FAULT DETECTED (EXIT RED AS EXPECTED)\n");
        return 1;
    }

    for (int i = 0; i < sim_unopt.storage_count; i++) {
        if (sim_unopt.storage_key[i] != sim_opt.storage_key[i] ||
            sim_unopt.storage_val[i] != sim_opt.storage_val[i]) {
            fprintf(stderr, "Gate 4-EVM FAULT DETECTED: Storage slot %d key-val mismatch (unopt: 0x%llx->0x%llx, opt: 0x%llx->0x%llx).\n",
                    i, (unsigned long long)sim_unopt.storage_key[i], (unsigned long long)sim_unopt.storage_val[i],
                    (unsigned long long)sim_opt.storage_key[i], (unsigned long long)sim_opt.storage_val[i]);
            printf("VERDICT: GATE 4-EVM FAULT DETECTED (EXIT RED AS EXPECTED)\n");
            return 1;
        }
    }

    /* --- PRECEDENCE CHECK 2: Zero Rewrites Applied (EXIT 2 - ORACLE-SUSPECT) --- */
    if (rewrites_applied == 0) {
        fprintf(stderr, "Gate 4-EVM VACUOUS: Zero rewrites applied (rewrites_applied == 0).\n");
        printf("VERDICT: ORACLE-SUSPECT (EXIT 2 AS EXPECTED)\n");
        return 2;
    }

    /* --- PRECEDENCE CHECK 3: Clean Differential Identity & Monotonic Gas (EXIT 0 - GREEN) --- */
    /* Count total swap opcodes emitted in unoptimized vs optimized Yul files */
    int swaps_unopt = 0, swaps_opt = 0;
    FILE *f_u = fopen(unopt_path, "r");
    FILE *f_o = fopen(opt_path, "r");
    char lbuf[256];
    if (f_u) { while (fgets(lbuf, sizeof(lbuf), f_u)) { if (strstr(lbuf, "swap")) swaps_unopt++; } fclose(f_u); }
    if (f_o) { while (fgets(lbuf, sizeof(lbuf), f_o)) { if (strstr(lbuf, "swap")) swaps_opt++; } fclose(f_o); }

    if (swaps_opt > swaps_unopt) {
        fprintf(stderr, "Gate 4-EVM FAIL: Gas proxy regression (swaps_opt %d > swaps_unopt %d).\n", swaps_opt, swaps_unopt);
        return 1;
    }
    assert(swaps_opt <= swaps_unopt && "Monotonic swap reduction invariant violated!");

    printf("Gate 4-EVM PASS: Storage_opt == Storage_unopt identity & monotonic gas verified (swaps_unopt: %d, swaps_opt: %d, rewrites: %d).\n",
           swaps_unopt, swaps_opt, rewrites_applied);
    printf("VERDICT: GATE 4-EVM PASS\n");
    return 0;
}
