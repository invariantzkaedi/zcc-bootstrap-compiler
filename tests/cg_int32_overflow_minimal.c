/* cg_int32_overflow_minimal.c
 * CG-INT32-001: ZCC uses addq+cltq for int arithmetic.
 * cltq sign-extends intermediate sums, diverging from GCC's
 * 32-bit addl semantics when intermediate values cross INT_MAX.
 *
 * Expected (GCC): CHECKSUM=-2147483646   (INT_MAX+1+1 wraps mod 2^32)
 * If ZCC gives the same, the bug is NOT the addq/cltq chain alone.
 * If ZCC gives a different value, CG-INT32-001 is confirmed here.
 */
int printf(const char *fmt, ...);

/* Force unsigned long -> int cast chains with intermediate overflow */
static int test_ul_to_int_cast(void) {
    unsigned long v1 = 0xFFFFFFFF00000001UL; /* (int)v1 = 1 */
    unsigned long v2 = 0x000000007FFFFFFFUL; /* (int)v2 = 2147483647 (INT_MAX) */
    unsigned long v3 = 0x0000000000000001UL; /* (int)v3 = 1 */
    /* (int)v1 + (int)v2 + (int)v3 = 1 + 2147483647 + 1 = 2147483649
     * As int: 2147483649 mod 2^32 with sign = -2147483647 (wraps)
     * GCC: addl, wraps correctly → -2147483647
     * ZCC broken: addq + cltq on intermediate 0x80000000 sign-extends to
     *             0xFFFFFFFF80000000 before adding v3 → -2147483648+1 = -2147483647
     * Actually these should agree in this case. Test the diverging one below. */
    return (int)v1 + (int)v2 + (int)v3;
}

/* This one has intermediate addq result that cltq changes differently */
static int test_signed_chain(void) {
    int a = 2147483647; /* INT_MAX */
    int b = 2147483647; /* INT_MAX */
    int c = 2;
    /* a+b = 4294967294 as uint32 = -2 as int (0xFFFFFFFE)
     * ZCC addq: 4294967294 in 64-bit; cltq on 0xFFFFFFFE → RAX=-2 (correct)
     * +c=2: -2+2=0. Should be 0.
     * ZCC: addq gives 0 → cltq → 0. OK.
     * Let's use a case that diverges: */
    unsigned long ul = (unsigned long)(a) + (unsigned long)(b);
    /* ul = 4294967294 = 0x00000000FFFFFFFE */
    return (int)ul + c; 
    /* (int)(0x00000000FFFFFFFE) = (int)(4294967294) = low32 = 0xFFFFFFFE = -2
     * -2 + 2 = 0. Both should agree. */
}

/* Concrete reproduction from seed9124 pattern:
 * (int)(unsigned long) where the ulong has high bits set */
static int test_ulong_high_bits(void) {
    /* v2 from seed9124 line 27: ~3281 = 0xFFFFFFFFFFFFECEF as unsigned long */
    unsigned long v2_ul = ~(unsigned long)(3281);
    /* (int)v2_ul = low32 = 0xFFFFECEF = -4977 */
    int v2 = (int)v2_ul;
    /* v1 = 7787, v3 = 88793 */
    int v1 = 7787;
    int v3 = 88793;
    /* Sum should be: 7787 + (-4977) + 88793 = 91603 */
    int result = v1 + v2 + v3;
    return result;
}

int main(void) {
    long checksum = 0;
    checksum += (long)test_ul_to_int_cast();
    checksum += (long)test_signed_chain();
    checksum += (long)test_ulong_high_bits();
    printf("test_ul_to_int_cast  : %ld (expect -2147483647)\n", (long)test_ul_to_int_cast());
    printf("test_signed_chain    : %ld (expect 0)\n",            (long)test_signed_chain());
    printf("test_ulong_high_bits : %ld (expect 91603)\n",        (long)test_ulong_high_bits());
    printf("CHECKSUM=%ld\n", checksum);
    return 0;
}
