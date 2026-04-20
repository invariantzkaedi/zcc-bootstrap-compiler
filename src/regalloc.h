/*
 * regalloc.h — Linear Scan Register Allocator for ZCC IR Backend
 *
 * Fits ZCC's actual IR model: temporaries are string-named (%t0, %t1, …).
 * Only pure temporaries (prefix %t) are candidates; %stack_* vars stay on
 * the stack (they are addressable locals, not ssa temps).
 *
 * Allocatable caller-saved registers (System V x86-64):
 *   We deliberately EXCLUDE:
 *     %rax  — accumulator/return, clobbered on every emit step
 *     %rcx  — shift-count register (shlq/shrq use %cl implicitly)
 *     %rdx  — clobbered by idivq/divq
 *     %rdi,%rsi,%rdx,%rcx,%r8,%r9 — arg-passing; overwritten by IR_ARG
 *   We use only caller-saved regs that are free between instructions:
 *     %rbx, %r12, %r13, %r14, %r15  — callee-saved (need push/pop in prologue)
 *     %r10, %r11                     — caller-saved scratch (safe within fn)
 *
 * Strategy: pure linear scan over the singly-linked node list.
 *   Pass 0: number every node (position index).
 *   Pass 1: compute live intervals [first_def, last_use] per temp name.
 *   Pass 2: sort by start; linear-scan; assign phys reg or keep on stack.
 *
 * Temps that don't get a register keep their existing stack slot (no change
 * to offset logic — the allocator never removes stack slots, only supplements
 * with register assignments where beneficial).
 */

#ifndef ZCC_REGALLOC_H
#define ZCC_REGALLOC_H

#include "ir.h"

/* ── Physical registers ───────────────────────────────────────────────── */
typedef enum {
    PREG_NONE = -1,
    /* callee-saved — we push/pop these in the function prologue/epilogue */
    PREG_RBX  = 0,
    PREG_R12  = 1,
    PREG_R13  = 2,
    PREG_R14  = 3,
    PREG_R15  = 4,
    /* caller-saved scratch — no push/pop needed */
    PREG_R10  = 5,
    PREG_R11  = 6,
    PREG_COUNT = 7
} PhysReg;

/* Human-readable name for a PhysReg, e.g. "rbx" */
const char *preg_name(PhysReg r);

/* Non-zero if this PhysReg is callee-saved (requires push/pop) */
int preg_callee_saved(PhysReg r);

/* ── Live interval ────────────────────────────────────────────────────── */
#define RA_NAME_MAX 64

typedef struct LiveInterval {
    char      name[RA_NAME_MAX]; /* %tN name of the temp              */
    int       start;             /* node index of first definition    */
    int       end;               /* node index of last use            */
    PhysReg   assigned;          /* physical reg, or PREG_NONE        */
} LiveInterval;

/* ── Allocator state ──────────────────────────────────────────────────── */
typedef struct RegAllocator {
    LiveInterval *intervals;     /* sorted array of live intervals    */
    int           num_intervals;
    int           cap_intervals;

    /* Which phys regs are used (for push/pop generation) */
    int           used[PREG_COUNT];
} RegAllocator;

/* ── API ──────────────────────────────────────────────────────────────── */

/* Create an empty allocator. */
RegAllocator *ra_create(void);

/* Release all memory. */
void ra_free(RegAllocator *ra);

/*
 * Run the full linear-scan pipeline over one IR function.
 * After this call, ra->intervals is populated and sorted by start.
 */
void ra_run(RegAllocator *ra, const ir_func_t *fn);

/*
 * Query: what physical register holds temp `name` at node position `pos`?
 * Returns PREG_NONE if the temp is spilled or unknown.
 * `pos` is the index of the *current* IR node in the linear scan.
 */
PhysReg ra_get(const RegAllocator *ra, const char *name);

/*
 * Non-zero if temp `name` needed at least one callee-saved register,
 * so the caller must emit pushq/popq for preg_callee_saved regs.
 */
int ra_any_callee_saved_used(const RegAllocator *ra);

#endif /* ZCC_REGALLOC_H */
