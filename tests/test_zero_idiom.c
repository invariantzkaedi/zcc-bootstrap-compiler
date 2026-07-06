/* test_zero_idiom.c — targeted tests for Step 1b (movq $0 -> xorl zero idiom)
 *
 * Exercises literal zero in every context where the emission change could
 * interact with EFLAGS or value semantics:
 *   - zero compared against (==0, !=0, <0 relational chains)
 *   - zero assigned between a comparison and its use (the hazard window)
 *   - zero as function argument adjacent to conditionals
 *   - zero in logical &&/|| short-circuit evaluation
 *   - zero via cast, unsigned, long long (full-width zeroing check)
 *   - zero returned through setcc-style boolean results
 *   - null pointer literal comparisons
 *
 * Self-contained: no includes. Exit code == number of failed checks.
 * Run under both zcc and gcc; both must exit 0.
 */

static int take_two(int a, int b) { return a * 100 + b; }

static int is_positive(int v) { return v > 0; }

static long long zero_ll(void) { return 0; }

int g_zero = 0;
int *g_null = 0;

int main(void) {
    int fails = 0;

    /* --- 1. basic zero compares: setcc result paths --- */
    {
        int x = 0;
        if (!(x == 0)) fails++;
        if (x != 0) fails++;
        if (x < 0) fails++;
        if (x > 0) fails++;
        int r = (x == 0);       /* boolean materialization via setcc */
        if (r != 1) fails++;
    }

    /* --- 2. zero assignment BETWEEN comparison and branch-dependent use.
     *        If codegen ever separates cmp from jcc/setcc and a zeroing
     *        lands in the window, xor would corrupt the flags. --- */
    {
        int a = 5, b = 7, r1, r2;
        r1 = (a < b);           /* cmp + setcc */
        int z = 0;              /* zero init immediately after */
        r2 = (a > b);
        if (r1 != 1) fails++;
        if (r2 != 0) fails++;
        if (z != 0) fails++;
    }

    /* --- 3. zero as argument adjacent to conditionals --- */
    {
        int c = (take_two(0, 0) == 0) ? 1 : 0;
        if (c != 1) fails++;
        int d = is_positive(0);
        if (d != 0) fails++;
        /* zero arg computed while a comparison result is pending in flow */
        int e = (is_positive(3) && take_two(0, 1) == 1) ? 1 : 0;
        if (e != 1) fails++;
    }

    /* --- 4. short-circuit chains with zero literals --- */
    {
        int p = 0, q = 9;
        if (p && q) fails++;             /* 0 && x */
        if (!(p || q)) fails++;          /* 0 || x */
        if (!(q && !p)) fails++;
        int r = (0 || (q > 5)) ? 1 : 0;
        if (r != 1) fails++;
    }

    /* --- 5. full-width zeroing: upper 32 bits must be clear --- */
    {
        long long big = 0x7FFFFFFFFFFFLL;
        big = 0;                          /* if only eXX cleared w/o zext, garbage */
        if (big != 0) fails++;
        if (zero_ll() != 0) fails++;
        unsigned long long u = 0;
        if (u != 0ULL) fails++;
        long long neg = -1;
        neg = 0;
        if (neg + 1 != 1) fails++;        /* arithmetic on the zeroed full width */
    }

    /* --- 6. null pointer literal --- */
    {
        int v = 42;
        int *p = &v;
        int *n = 0;
        if (n != 0) fails++;
        if (p == 0) fails++;
        if (g_null != 0) fails++;
        p = 0;
        if (p) fails++;
    }

    /* --- 7. zero in loop conditions and counters --- */
    {
        int i = 0, sum = 0;
        for (i = 0; i < 5; i++) sum += i;
        if (sum != 10) fails++;
        int k = 5;
        while (k != 0) k--;
        if (k != 0) fails++;
    }

    /* --- 8. zero through globals and chained boolean results --- */
    {
        if (g_zero != 0) fails++;
        int chain = (g_zero == 0) + (g_zero < 1) + (g_zero > -1);
        if (chain != 3) fails++;
    }

    return fails;
}
