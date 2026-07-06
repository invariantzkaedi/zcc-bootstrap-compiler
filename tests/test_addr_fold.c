/* test_addr_fold.c — targeted tests for codegen_addr_offset (SIB fold refactor)
 *
 * Exercises every branch of the fold recursion:
 *   ND_MEMBER chains (dot), DEREF boundaries (arrow), array-of-struct
 *   indexing (DEREF(ADD)), address-of folded lvalues, globals via
 *   name+offset(%rip), unions, large member offsets (>255, stresses
 *   the ARM immediate path and displacement math), and deep negative
 *   frame offsets.
 *
 * Self-contained: no includes. Exit code == number of failed checks.
 * Run under both zcc and gcc; both must exit 0, and behavior must match.
 */

struct Inner { int x; int y; };
struct Mid   { int pad; struct Inner in; };
struct Outer { char head[3]; struct Mid mid; struct Inner *ip; };

/* Large prefix forces member offsets well past 255 — stresses ARM
 * 'adds #imm' encoding limits and x86 displacement folding alike. */
struct Big { char buf[1000]; int tail; struct Inner in2; };

union U { int i; char c[4]; };

struct Outer g_outer;           /* global struct: leaq name+off(%rip) path */
struct Big   g_big;
int          g_arr_probe;

static int check(int cond, int id) {
    if (!cond) return id;       /* nonzero = failing test id */
    return 0;
}

int main(void) {
    int fails = 0;

    /* --- 1. nested dot chain: single folded displacement --- */
    struct Outer o;
    o.mid.in.x = 41;
    o.mid.in.y = 42;
    fails += check(o.mid.in.x == 41, 1) ? 1 : 0;
    fails += check(o.mid.in.y == 42, 2) ? 1 : 0;

    /* --- 2. address-of folded lvalue: &s.a.b must equal base+offsets --- */
    {
        char *base = (char *)&o;
        char *member = (char *)&o.mid.in.y;
        long diff = member - base;
        /* offset must be positive and consistent with a re-read */
        fails += check(diff > 0, 3) ? 1 : 0;
        *(int *)member = 77;
        fails += check(o.mid.in.y == 77, 4) ? 1 : 0;
    }

    /* --- 3. arrow then dot: fold must stop at DEREF, resume after --- */
    {
        struct Inner heap_in;
        heap_in.x = 5; heap_in.y = 6;
        o.ip = &heap_in;
        fails += check(o.ip->x == 5, 5) ? 1 : 0;
        o.ip->y = 60;
        fails += check(heap_in.y == 60, 6) ? 1 : 0;
    }

    /* --- 4. dot then arrow: accumulation restarts at zero past deref --- */
    {
        struct Inner tgt;
        tgt.x = 9;
        g_outer.ip = &tgt;
        fails += check(g_outer.ip->x == 9, 7) ? 1 : 0;
    }

    /* --- 5. array of structs, runtime index (DEREF(ADD) path) --- */
    {
        struct Mid arr[4];
        int i;
        for (i = 0; i < 4; i++) {
            arr[i].in.x = i * 10;
            arr[i].in.y = i * 10 + 1;
        }
        for (i = 0; i < 4; i++) {
            fails += check(arr[i].in.x == i * 10, 8) ? 1 : 0;
            fails += check(arr[i].in.y == i * 10 + 1, 9) ? 1 : 0;
        }
        /* constant index too — some backends const-fold this branch */
        fails += check(arr[2].in.y == 21, 10) ? 1 : 0;
    }

    /* --- 6. global struct member: leaq name+offset(%rip) --- */
    g_outer.mid.in.x = 1234;
    fails += check(g_outer.mid.in.x == 1234, 11) ? 1 : 0;

    /* --- 7. large member offset (>255): ARM imm limit / displacement --- */
    {
        struct Big local_big;
        local_big.tail = 314;
        local_big.in2.y = 159;
        fails += check(local_big.tail == 314, 12) ? 1 : 0;
        fails += check(local_big.in2.y == 159, 13) ? 1 : 0;

        g_big.tail = 271;
        g_big.in2.x = 828;
        fails += check(g_big.tail == 271, 14) ? 1 : 0;
        fails += check(g_big.in2.x == 828, 15) ? 1 : 0;

        /* large offset through a pointer: DEREF path with big constant */
        struct Big *pb = &local_big;
        pb->tail = 999;
        fails += check(local_big.tail == 999, 16) ? 1 : 0;
        fails += check(pb->in2.y == 159, 17) ? 1 : 0;
    }

    /* --- 8. union member: offset 0 fold, aliasing sanity --- */
    {
        union U u;
        u.i = 0x01020304;
        /* both members share offset 0 */
        fails += check((char *)&u.i == (char *)&u.c[0], 18) ? 1 : 0;
        u.c[0] = 0;
        fails += check(u.i != 0x01020304, 19) ? 1 : 0;
    }

    /* --- 9. deep frame: big local before struct forces large negative
     *        rbp displacement combined with positive member offset --- */
    {
        char spacer[2048];
        struct Mid deep;
        spacer[0] = 1; spacer[2047] = 2;  /* keep spacer live */
        deep.in.x = 4242;
        fails += check(deep.in.x == 4242, 20) ? 1 : 0;
        fails += check(spacer[0] + spacer[2047] == 3, 21) ? 1 : 0;
    }

    /* --- 10. member address used as pointer, written through later --- */
    {
        int *py = &g_outer.mid.in.y;
        *py = 555;
        fails += check(g_outer.mid.in.y == 555, 22) ? 1 : 0;
    }

    return fails;
}
