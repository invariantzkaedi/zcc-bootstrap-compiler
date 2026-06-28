/* regression: CG-ABI-STRUCT-002
 * Memory-class struct parameter offset misclassification.
 *
 * Trigger: function that both returns AND receives memory-class structs
 * (SysV AMD64: size > 16 bytes). The second struct parameter was loaded
 * from -64(%rbp) instead of -56(%rbp) due to adjust_sym using
 * -parser_param_space instead of -actual_param_space as its boundary.
 *
 * Expected: ZCC output matches GCC exactly on all struct sizes A-D.
 * Run:
 *   ./zcc -I./zcc_sys_includes tests/regressions/repro_struct_progression.c -o /tmp/t.s
 *   gcc /tmp/t.s -o /tmp/t && /tmp/t
 */
#include <stdio.h>
#include <stddef.h>

/* ── struct A: 8 bytes ───────────────────────────────────────────── */
typedef struct { double d; } A;
A a_op(A x, A y) { A r; r.d = x.d + y.d; return r; }
A a_chain(A x, A y) { return a_op(a_op(x, y), y); }

/* ── struct B: 16 bytes, int+double ─────────────────────────────── */
typedef struct { int i; double d; } B;
B b_op(B x, B y) { B r; r.i = x.i + y.i; r.d = x.d + y.d; return r; }
B b_chain(B x, B y) { return b_op(b_op(x, y), y); }

/* ── struct C: 16 bytes, double+int+int ──────────────────────────── */
typedef struct { double d; int i; int j; } C;
C c_op(C x, C y) { C r; r.d = x.d + y.d; r.i = x.i + y.i; r.j = x.j ^ y.j; return r; }
C c_chain(C x, C y) { return c_op(c_op(x, y), y); }

/* ── struct D: 24 bytes — the boundary case (memory-class ABI) ───── */
typedef struct { double d; unsigned u; int i; float f; int j; } D;
D d_op(D x, D y) {
    D r;
    r.d = x.d * y.d;
    r.u = x.u + y.u;
    r.i = x.i - y.i;
    r.f = x.f + y.f;
    r.j = x.j ^ y.j;
    return r;
}
D d_chain(D x, D y) { return d_op(d_op(x, y), y); }

int main(void) {
    int failed = 0;

    /* struct A — register-class (8B), baseline */
    A a1; a1.d = 4.46;
    A a2; a2.d = 22.29;
    A ar  = a_op(a1, a2);
    A ar2 = a_chain(a1, a2);
    if (ar.d != 26.75)  { printf("FAIL A.single: %.2f\n",  ar.d);  failed++; }
    if (ar2.d != 49.04) { printf("FAIL A.chain:  %.2f\n",  ar2.d); failed++; }

    /* struct B — register-class (16B) */
    B b1; b1.i = 10; b1.d = 4.46;
    B b2; b2.i = 20; b2.d = 22.29;
    B br  = b_op(b1, b2);
    B br2 = b_chain(b1, b2);
    if (br.i != 30 || br.d != 26.75)   { printf("FAIL B.single\n"); failed++; }
    if (br2.i != 50 || br2.d != 49.04) { printf("FAIL B.chain\n");  failed++; }

    /* struct C — register-class (16B) */
    C c1; c1.d = 4.46;  c1.i = 10; c1.j = 0xAA;
    C c2; c2.d = 22.29; c2.i = 20; c2.j = 0x55;
    C cr  = c_op(c1, c2);
    C cr2 = c_chain(c1, c2);
    if (cr.d != 26.75  || cr.i != 30  || cr.j != 255) { printf("FAIL C.single\n"); failed++; }
    if (cr2.d != 49.04 || cr2.i != 50 || cr2.j != 170){ printf("FAIL C.chain\n");  failed++; }

    /* struct D — memory-class (24B): THIS is the regression boundary */
    printf("D size=%zu offsets: d=%zu u=%zu i=%zu f=%zu j=%zu\n",
           sizeof(D),
           offsetof(D,d), offsetof(D,u), offsetof(D,i),
           offsetof(D,f), offsetof(D,j));

    D d1; d1.d = 4.46;  d1.u = 135; d1.i = 75;  d1.f = 32.90f; d1.j = 89;
    D d2; d2.d = 22.29; d2.u = 75;  d2.i = 90;  d2.f = 39.97f; d2.j = 13;
    D dr  = d_op(d1, d2);
    D dr2 = d_chain(d1, d2);

    /* d_op expected: d=99.41 u=210 i=-15 f=72.87 j=84 */
    if (dr.d < 99.40 || dr.d > 99.42) { printf("FAIL D.single d=%.2f\n", dr.d); failed++; }
    if (dr.u != 210)  { printf("FAIL D.single u=%u\n",  dr.u);  failed++; }
    if (dr.i != -15)  { printf("FAIL D.single i=%d\n",  dr.i);  failed++; }
    if (dr.j != 84)   { printf("FAIL D.single j=%d\n",  dr.j);  failed++; }

    /* d_chain expected: d=2215.92 u=285 i=-105 f=112.84 j=89 */
    if (dr2.d < 2215.91 || dr2.d > 2215.93) { printf("FAIL D.chain d=%.2f\n", dr2.d); failed++; }
    if (dr2.u != 285)  { printf("FAIL D.chain u=%u\n",  dr2.u);  failed++; }
    if (dr2.i != -105) { printf("FAIL D.chain i=%d\n",  dr2.i);  failed++; }
    if (dr2.j != 89)   { printf("FAIL D.chain j=%d\n",  dr2.j);  failed++; }

    if (failed == 0) {
        printf("PASS CG-ABI-STRUCT-002 regression\n");
        return 0;
    }
    return 1;
}
