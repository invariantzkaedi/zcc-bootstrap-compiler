/* vcall_lower_harness.c — proves the IR LOWERING of a virtual call.
 * ─────────────────────────────────────────────────────────────────────
 * The vtable_harness proved LAYOUT (which slot, what's in the vtable).
 * This proves the next thing: that `p->area()` lowers to a correct IR
 * sequence that ZCC can ALREADY emit, because a virtual call is just:
 *
 *     vptr  = LOAD [p + 0]          ; read the vtable pointer (offset 0)
 *     fnp   = LOAD [vptr + slot*8]  ; index the slot
 *     ret   = CALL_INDIRECT fnp(p)  ; the EXISTING `call *%r10` path
 *
 * Key insight verified by reading part4.c:3255 — ZCC already lowers
 * indirect calls (func pointer in a reg -> `call *%r10`). So OP_VCALL needs
 * NO new codegen; it lowers to LOAD, LOAD, <existing indirect call>.
 *
 * This harness emits that exact IR sequence for a call site, then
 * "executes" it against a modeled object+vtable in memory to prove the
 * call lands on the right function. The model mirrors the Itanium layout:
 * object word 0 = vptr; vtable[slot] = fn pointer. Pure C, standalone. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define PTR 8                    /* pointer width on the x86-64 target */

/* ── A tiny IR the way ZCC models it: op + operands + a result reg ──── */
typedef enum { IR_LOAD, IR_VCALL_LOWERED, IR_CALL_INDIRECT } IrOp;

typedef struct {
    IrOp op;
    int  dst;          /* result vreg */
    int  base;         /* base vreg for LOAD */
    int  offset;       /* byte offset for LOAD */
    int  callee_reg;   /* vreg holding fn pointer for indirect call */
    int  this_reg;     /* implicit this argument */
} Ir;

/* The lowering function under test: given a call site, produce the IR.
 * Returns the number of IR instructions emitted into `out`. */
static int lower_virtual_call(Ir *out, int this_reg, int slot,
                              int *result_reg, int *next_vreg) {
    int n = 0;
    int vptr = (*next_vreg)++;
    int fnp  = (*next_vreg)++;
    int ret  = (*next_vreg)++;

    /* vptr = LOAD [this + 0]  — vptr is at object offset 0 */
    out[n].op = IR_LOAD; out[n].dst = vptr; out[n].base = this_reg; out[n].offset = 0; n++;
    /* fnp = LOAD [vptr + slot*PTR] — index the vtable */
    out[n].op = IR_LOAD; out[n].dst = fnp; out[n].base = vptr; out[n].offset = slot * PTR; n++;
    /* ret = CALL_INDIRECT fnp(this) — reuses ZCC's `call *%reg` path */
    out[n].op = IR_CALL_INDIRECT; out[n].dst = ret; out[n].callee_reg = fnp;
    out[n].this_reg = this_reg; n++;

    *result_reg = ret;
    return n;
}

/* ── A modeled runtime: memory + vregs, to EXECUTE the lowered IR ───── */
/* We lay out an object exactly as Itanium would: word 0 is the vptr (an
 * address into a vtable array), and the vtable holds function "addresses"
 * which we model as small integer ids resolving to names. */
#define MEM_WORDS 256
static uint64_t MEM[MEM_WORDS];   /* byte-addressed via /PTR below       */
static const char *FN_NAMES[64];  /* fn id -> name                       */
static int n_fns;

static int intern_fn(const char *name) {
    FN_NAMES[n_fns] = name; return n_fns++;   /* returns the fn id */
}

/* execute the lowered IR; return the name of the function actually called */
static const char *run_ir(Ir *ir, int n, int result_reg, uint64_t *vregs,
                          int this_addr) {
    int i;
    vregs[/*this*/ ir[0].base] = this_addr;   /* seed: this lives in its vreg */
    const char *called = "(none)";
    for (i = 0; i < n; i++) {
        Ir *x = &ir[i];
        if (x->op == IR_LOAD) {
            uint64_t addr = vregs[x->base] + x->offset;
            vregs[x->dst] = MEM[addr / PTR];
        } else if (x->op == IR_CALL_INDIRECT) {
            int fn_id = (int)vregs[x->callee_reg];
            called = FN_NAMES[fn_id];
            /* this is passed in this_reg — verify it's the object we expect */
            if (vregs[x->this_reg] != (uint64_t)this_addr) called = "(this corrupted!)";
        }
    }
    (void)result_reg;
    return called;
}

static int total, fails;
static void ck(const char *got, const char *want, const char *d) {
    int ok = strcmp(got, want) == 0; total++; if (!ok) fails++;
    printf("  %-46s got=%-16s want=%-16s %s\n", d, got, want, ok ? "OK" : "** FAIL **");
}
static void ck_int(int got, int want, const char *d) {
    int ok = got == want; total++; if (!ok) fails++;
    printf("  %-46s got=%-3d            want=%-3d            %s\n", d, got, want, ok ? "OK" : "** FAIL **");
}

int main(void) {
    printf("Virtual-call IR lowering verification\n");
    printf("------------------------------------------------\n");

    /* Build two vtables in MEM, Itanium-style.
     * Shape vtable @ word 10: [Shape::area, Shape::name]
     * Circle vtable @ word 20: [Circle::area, Shape::name, Circle::roll] */
    int shape_area  = intern_fn("Shape::area");
    int shape_name  = intern_fn("Shape::name");
    int circle_area = intern_fn("Circle::area");
    int circle_roll = intern_fn("Circle::roll");

    int shape_vt  = 10;  /* word index of Shape's vtable  */
    int circle_vt = 20;  /* word index of Circle's vtable */
    MEM[shape_vt + 0] = shape_area;
    MEM[shape_vt + 1] = shape_name;
    MEM[circle_vt + 0] = circle_area;   /* override */
    MEM[circle_vt + 1] = shape_name;    /* inherited */
    MEM[circle_vt + 2] = circle_roll;   /* new virtual */

    /* Two objects. word0 = vptr (byte address of the vtable). */
    int shape_obj  = 100;  /* object at word 100 */
    int circle_obj = 120;
    MEM[shape_obj]  = (uint64_t)shape_vt  * PTR;   /* vptr -> Shape vtable  */
    MEM[circle_obj] = (uint64_t)circle_vt * PTR;   /* vptr -> Circle vtable */

    Ir ir[8];
    uint64_t vregs[64];

    /* ── lowering shape check: exactly 3 IR ops, right offsets ── */
    printf("[lowering structure]\n");
    {
        int next = 1, result; /* vreg 0 = this */
        int n = lower_virtual_call(ir, 0, /*slot*/0, &result, &next);
        ck_int(n, 3, "p->area() lowers to 3 IR ops");
        ck_int(ir[0].offset, 0, "op0 LOAD vptr at offset 0");
        ck_int(ir[1].offset, 0 * PTR, "op1 LOAD slot 0 at offset 0");
        ck_int(ir[2].op == IR_CALL_INDIRECT, 1, "op2 is indirect call (reuses call *%reg)");
    }
    {
        int next = 1, result;
        lower_virtual_call(ir, 0, /*slot*/2, &result, &next);
        ck_int(ir[1].offset, 2 * PTR, "p->roll() slot 2 -> offset 16");
    }

    /* ── dispatch via lowered IR: Shape* p = &circle; p->area() ── */
    printf("[lowered dispatch — the real test]\n");
    {
        /* slot for area() is 0 (from static type Shape) */
        int next = 1, result;
        int n = lower_virtual_call(ir, 0, 0, &result, &next);
        memset(vregs, 0, sizeof vregs);
        const char *called = run_ir(ir, n, result, vregs, circle_obj * PTR);
        ck(called, "Circle::area", "Shape* -> Circle: area() dispatches to override");
    }
    {
        /* name() slot 1 — inherited, must hit Shape::name even on Circle */
        int next = 1, result;
        int n = lower_virtual_call(ir, 0, 1, &result, &next);
        memset(vregs, 0, sizeof vregs);
        const char *called = run_ir(ir, n, result, vregs, circle_obj * PTR);
        ck(called, "Shape::name", "Shape* -> Circle: name() dispatches to inherited");
    }
    {
        /* same call site, Shape object -> Shape::area */
        int next = 1, result;
        int n = lower_virtual_call(ir, 0, 0, &result, &next);
        memset(vregs, 0, sizeof vregs);
        const char *called = run_ir(ir, n, result, vregs, shape_obj * PTR);
        ck(called, "Shape::area", "Shape* -> Shape: area() dispatches to base");
    }
    {
        /* Circle::roll via slot 2 on a circle */
        int next = 1, result;
        int n = lower_virtual_call(ir, 0, 2, &result, &next);
        memset(vregs, 0, sizeof vregs);
        const char *called = run_ir(ir, n, result, vregs, circle_obj * PTR);
        ck(called, "Circle::roll", "Circle: roll() new virtual dispatches");
    }

    printf("------------------------------------------------\n");
    printf("=== %d/%d passed, %d failure(s) ===\n", total - fails, total, fails);
    return fails;
}
