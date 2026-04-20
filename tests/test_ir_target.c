/*
 * test_ir_target.c — ZCC IR Bridge Vulnerability Test Target
 * ===========================================================
 * Contains intentional vulnerabilities for swarm detection validation:
 *
 *   vuln_oob_write()     CWE-119: unguarded GEP beyond buffer end
 *   vuln_escape_stack()  CWE-416: returns pointer to stack-allocated array
 *   vuln_alias_write()   CWE-476: two GEPs from same base, overlapping stores
 *   safe_bounded_loop()  Safe reference — must NOT trigger findings
 *
 * Compile with ZCC IR bridge:
 *   ZCC_IR_BRIDGE=1 ./zcc test_ir_target.c -o test_ir_target.s
 *   python3 ir_energy_bridge.py zcc_ir_dump.json --energy
 *
 * Expected findings:
 *   CRITICAL  CWE-119  vuln_oob_write       (unguarded GEP index 1024)
 *   HIGH      CWE-416  vuln_escape_stack    (stack alloca escapes via return)
 *   MEDIUM    CWE-476  vuln_alias_write     (alias pair from same base)
 *   (none)              safe_bounded_loop   (guarded loop — clean)
 */

/* ── CWE-119: Buffer overflow via unguarded GEP ─────────────────────── */
void vuln_oob_write(int *buf, int val) {
    /* buf is allocated for 8 elements by caller.
     * Writing at index 1024 is always OOB.
     * No bounds check precedes the GEP.
     * IR: gep %p0 buf 1024 → bounds_unsafe annotation expected */
    buf[1024] = val;
}

/* ── CWE-416: Stack alloca escapes via return ────────────────────────── */
int *vuln_escape_stack(int n) {
    /* stack-allocated buffer returned to caller.
     * After this function returns, the frame is gone.
     * IR: alloca %loc 32 → escape annotation via OP_RETURN */
    int local[32];
    local[0] = n;
    return local;   /* ← escape: stack pointer returned */
}

/* ── CWE-476: Alias confusion — two GEPs from same base ─────────────── */
void vuln_alias_write(int *buf) {
    /* Two pointers derived from the same base at overlapping offsets.
     * If buf is 4 elements, p0 and p1 may alias element 3.
     * IR: gep %p0 buf 3 / gep %p1 buf 3 → alias_pairs annotation */
    int *p0 = &buf[3];
    int *p1 = &buf[3];   /* same GEP base + index → alias */
    *p0 = 1;
    *p1 = 2;             /* write through aliased pointer */
}

/* ── Safe: bounded loop — should produce no CRITICAL/HIGH findings ───── */
void safe_bounded_loop(int *buf, int len, int val) {
    /* Bounds check i < len precedes every GEP.
     * IR: lt %cmp i len → gep %p buf i (guarded — bounds_checked=1) */
    int i = 0;
    while (i < len) {
        buf[i] = val;
        i = i + 1;
    }
}

int main(void) {
    int arr[8];
    /* Call safe function — should complete without swarm alert */
    safe_bounded_loop(arr, 8, 42);
    /* Do NOT call the vuln functions in main — we're testing IR analysis,
     * not runtime behaviour. The IR bridge catches them statically. */
    return 0;
}
