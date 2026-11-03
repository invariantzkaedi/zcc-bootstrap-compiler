/* tests/x86_mv02_imm64.c */
int classify(long long x) {
    switch (x) {
        case 4294967296LL: return 7;
        case -4294967296LL: return 9;
        default: return 3;
    }
}

/* Adversarial live-%r11 test across 64-bit switch immediate comparison
 * Also initializes r11_after to constant literal 0 to test ND_ASM ICP guard. */
int classify_live_r11(long long x) {
    long long r11_after = 0;
    asm("movq $13374242, %%r11" : : : "r11");
    switch (x) {
        case 4294967296LL:
            asm("movq %%r11, %0" : "=r"(r11_after));
            return (r11_after == 13374242LL) ? 7 : -1;
        case -4294967296LL:
            asm("movq %%r11, %0" : "=r"(r11_after));
            return (r11_after == 13374242LL) ? 9 : -1;
        default:
            asm("movq %%r11, %0" : "=r"(r11_after));
            return (r11_after == 13374242LL) ? 3 : -1;
    }
}

/* Register aliasing: cmpq $<imm64>, %r11 while %r10 and %r11 are both live */
int classify_alias_r11(void) {
    long long eq = 0, r10_after = 0, r11_after = 0;
#ifdef __ZCC__
    asm("movq $999888777, %r10");
    asm("movabsq $4294967296, %r11");
    asm("cmpq $4294967296, %r11");
    asm("sete %%al\n    movzbq %%al, %0" : "=r"(eq));
    asm("movq %%r10, %0" : "=r"(r10_after));
    asm("movq %%r11, %0" : "=r"(r11_after));
#else
    eq = 1;
    r10_after = 999888777LL;
    r11_after = 4294967296LL;
#endif
    if (eq != 1) return -10;
    if (r10_after != 999888777LL) return -11;
    if (r11_after != 4294967296LL) return -12;
    return 11;
}

/* %rsp-relative operand: cmpq $<imm64>, 8(%rsp) while %r11 is live */
int classify_rsp_relative(void) {
    long long eq = 0, r11_after = 0;
#ifdef __ZCC__
    asm("subq $16, %rsp");
    asm("movq $111222333, (%rsp)");
    asm("movabsq $4294967296, %rax");
    asm("movq %rax, 8(%rsp)");
    asm("movq $555666777, %r11");
    asm("cmpq $4294967296, 8(%rsp)");
    asm("sete %al");
    asm("addq $16, %rsp");
    asm("movzbq %%al, %0" : "=r"(eq));
    asm("movq %%r11, %0" : "=r"(r11_after));
#else
    eq = 1;
    r11_after = 555666777LL;
#endif
    if (eq != 1) return -20;
    if (r11_after != 555666777LL) return -21;
    return 22;
}

/* MV-04: Base-register memory operand 8(%r11) while %r10 and %r11 are live */
int classify_mem_r11_base(void) {
    long long arr[2];
    long long eq = 0, r10_after = 0, r11_after = 0;
    arr[0] = 111LL;
    arr[1] = 4294967296LL;
#ifdef __ZCC__
    long long *p = arr;
    long long expected_p = (long long)p;
    asm("movq %0, %%r11" : : "r"(p));
    asm("movq $777888999, %r10");
    asm("cmpq $4294967296, 8(%r11)");
    asm("sete %%al\n    movzbq %%al, %0" : "=r"(eq));
    asm("movq %%r10, %0" : "=r"(r10_after));
    asm("movq %%r11, %0" : "=r"(r11_after));
    if (r11_after != expected_p) return -32;
#else
    eq = 1;
    r10_after = 777888999LL;
#endif
    if (eq != 1) return -30;
    if (r10_after != 777888999LL) return -31;
    return 33;
}

/* MV-04: SIB memory operand 8(%r11, %r10, 8) using both %r11 and %r10 while %r9 is live */
int classify_mem_r11_r10_sib(void) {
    long long arr[3];
    long long eq = 0, r9_after = 0, r10_after = 0, r11_after = 0;
    arr[0] = 10LL;
    arr[1] = 20LL;
    arr[2] = 4294967296LL;
#ifdef __ZCC__
    long long *p = arr;
    long long expected_p = (long long)p;
    asm("movq %0, %%r11" : : "r"(p));
    asm("movq $1, %r10");
    asm("movq $888999111, %r9");
    asm("cmpq $4294967296, 8(%r11, %r10, 8)");
    asm("sete %%al\n    movzbq %%al, %0" : "=r"(eq));
    asm("movq %%r9, %0" : "=r"(r9_after));
    asm("movq %%r10, %0" : "=r"(r10_after));
    asm("movq %%r11, %0" : "=r"(r11_after));
    if (r11_after != expected_p) return -43;
#else
    eq = 1;
    r9_after = 888999111LL;
    r10_after = 1LL;
#endif
    if (eq != 1) return -40;
    if (r9_after != 888999111LL) return -41;
    if (r10_after != 1LL) return -42;
    return 44;
}

/* MV-04: Indexed %rsp-relative SIB operand 8(%rsp, %rcx, 8) while %r11 is live */
int classify_rsp_indexed(void) {
    long long eq = 0, r11_after = 0;
#ifdef __ZCC__
    asm("subq $24, %rsp");
    asm("movq $0, (%rsp)");
    asm("movq $0, 8(%rsp)");
    asm("movabsq $4294967296, %rax");
    asm("movq %rax, 16(%rsp)");
    asm("movq $1, %rcx");
    asm("movq $333444555, %r11");
    asm("cmpq $4294967296, 8(%rsp, %rcx, 8)");
    asm("sete %al");
    asm("addq $24, %rsp");
    asm("movzbq %%al, %0" : "=r"(eq));
    asm("movq %%r11, %0" : "=r"(r11_after));
#else
    eq = 1;
    r11_after = 333444555LL;
#endif
    if (eq != 1) return -50;
    if (r11_after != 333444555LL) return -51;
    return 55;
}

/* MV-05 / MV-06: Simultaneous live red-zone memory operands -8(%rsp), -120(%rsp), and -128(%rsp) while %r11 is live */
int classify_redzone_rsp(void) {
    long long eq8 = 0, eq128 = 0, rz8_after = 0, rz120_after = 0, rz128_after = 0, r11_after = 0;
#ifdef __ZCC__
    asm("movabsq $4294967296, %rax");
    asm("movq %rax, -8(%rsp)");
    asm("movq %rax, -128(%rsp)");
    asm("movabsq $123456789012345, %rax");
    asm("movq %rax, -120(%rsp)");
    asm("movq $777888999, %r11");
    asm("cmpq $4294967296, -8(%rsp)");
    asm("sete %al");
    asm("movzbq %%al, %0" : "=r"(eq8));
    asm("cmpq $4294967296, -128(%rsp)");
    asm("sete %al");
    asm("movzbq %%al, %0" : "=r"(eq128));
    asm("movq -8(%%rsp), %0" : "=r"(rz8_after));
    asm("movq -120(%%rsp), %0" : "=r"(rz120_after));
    asm("movq -128(%%rsp), %0" : "=r"(rz128_after));
    asm("movq %%r11, %0" : "=r"(r11_after));
#else
    eq8 = 1;
    eq128 = 1;
    rz8_after = 4294967296LL;
    rz120_after = 123456789012345LL;
    rz128_after = 4294967296LL;
    r11_after = 777888999LL;
#endif
    if (eq8 != 1) return -60;
    if (eq128 != 1) return -61;
    if (rz8_after != 4294967296LL) return -62;
    if (rz120_after != 123456789012345LL) return -65;
    if (rz128_after != 4294967296LL) return -63;
    if (r11_after != 777888999LL) return -64;
    return 66;
}

/* MV-05: Direct %rsp operand cmpq $4294967296, %rsp while %r11 is live */
int classify_direct_rsp(void) {
    long long map_res = 0, eq = 0, r11_after = 0, saved_rsp = 0;
#ifdef __ZCC__
    /* sys_mmap(0xfffff000, 8192, PROT_READ|PROT_WRITE(3), MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED(0x32), -1, 0) */
    asm("movq $9, %%rax\n"
        "movabsq $4294963200, %%rdi\n"
        "movq $8192, %%rsi\n"
        "movq $3, %%rdx\n"
        "movq $0x32, %%r10\n"
        "movq $-1, %%r8\n"
        "xorq %%r9, %%r9\n"
        "syscall\n"
        "movq %%rax, %0" : "=r"(map_res) : : "rax", "rdi", "rsi", "rdx", "r10", "r8", "r9", "rcx", "r11");
    if (map_res != 4294963200LL) return -70;

    asm("movq %rsp, %r10");
    asm("movq $555666777, %r11");
    asm("movabsq $4294967296, %rsp");
    asm("cmpq $4294967296, %rsp");
    asm("sete %al");
    asm("movq %r10, %rsp");
    asm("movzbq %%al, %0" : "=r"(eq));
    asm("movq %%r11, %0" : "=r"(r11_after));

    /* sys_munmap(0xfffff000, 8192) */
    asm("movq $11, %%rax\n"
        "movabsq $4294963200, %%rdi\n"
        "movq $8192, %%rsi\n"
        "syscall" : : : "rax", "rdi", "rsi", "rcx", "r11");
#else
    eq = 1;
    r11_after = 555666777LL;
#endif
    if (eq != 1) return -71;
    if (r11_after != 555666777LL) return -72;
    return 77;
}

/* MV-08: Zero-Memory-Touch XMM15 Spill & Indexed Effective-Address Aliasing Test:
 * Simultaneously populates live values at BOTH 32(%rsp) (-336(%rbp)) AND 24(%rsp) (-344(%rbp)),
 * then executes:
 *   1) cmpq $4294967296, 32(%rsp)
 *   2) cmpq $4294967296, -336(%rbp)
 *   3) cmpq $4294967296, 24(%rsp)
 *   4) cmpq $4294967296, 24(%rsp, %rcx, 8) with %rcx = 1 (EA == RSP + 32)
 *   5) cmpq $4294967296, 16(%rsp, %rcx, 8) with %rcx = 1 (EA == RSP + 24)
 * while verifying that BOTH 32(%rsp) and 24(%rsp) retain their live values across every comparison!
 */
int classify_frame_scratch_alias(void) {
    long long eq_rsp32 = 0, eq_rbp = 0, eq_rsp24 = 0, eq_idx32 = 0, eq_idx24 = 0;
    long long val_rsp32 = 0, val_rbp = 0, val_rsp24 = 0, r11_after = 0;
#ifdef __ZCC__
    asm("movq $777888999, %r11");
    asm("movabsq $4294967296, %rax");
    asm("movq %rax, 32(%rsp)");
    asm("movq %rax, 24(%rsp)");

    asm("cmpq $4294967296, 32(%rsp)");
    asm("sete %al");
    asm("movzbq %%al, %0" : "=r"(eq_rsp32));

    asm("cmpq $4294967296, -352(%rbp)");
    asm("sete %al");
    asm("movzbq %%al, %0" : "=r"(eq_rbp));

    asm("cmpq $4294967296, 24(%rsp)");
    asm("sete %al");
    asm("movzbq %%al, %0" : "=r"(eq_rsp24));

    /* MV08-01 + Adjudicated CEX 1:
     *   24(%rsp, %rcx, 8) with %rcx = 1 -> EA == RSP + 32
     *    8(%rsp, %rcx, 8) with %rcx = 3 -> EA == RSP + 32 */
    asm("movq $1, %rcx");
    asm("cmpq $4294967296, 24(%rsp, %rcx, 8)");
    asm("sete %dl");
    asm("movq $3, %rcx");
    asm("cmpq $4294967296, 8(%rsp, %rcx, 8)");
    asm("sete %al");
    asm("andb %dl, %al");
    asm("movzbq %%al, %0" : "=r"(eq_idx32));

    /* MV08-02 + Adjudicated CEX 2:
     *   16(%rsp, %rcx, 8) with %rcx = 1  -> EA == RSP + 24
     *   32(%rsp, %rcx, 8) with %rcx = -1 -> EA == RSP + 24 */
    asm("movq $1, %rcx");
    asm("cmpq $4294967296, 16(%rsp, %rcx, 8)");
    asm("sete %dl");
    asm("movq $-1, %rcx");
    asm("cmpq $4294967296, 32(%rsp, %rcx, 8)");
    asm("sete %al");
    asm("andb %dl, %al");
    asm("movzbq %%al, %0" : "=r"(eq_idx24));

    asm("movq 32(%%rsp), %0" : "=r"(val_rsp32));
    asm("movq -352(%%rbp), %0" : "=r"(val_rbp));
    asm("movq 24(%%rsp), %0" : "=r"(val_rsp24));
    asm("movq %%r11, %0" : "=r"(r11_after));
#else
    eq_rsp32 = 1;
    eq_rbp = 1;
    eq_rsp24 = 1;
    eq_idx32 = 1;
    eq_idx24 = 1;
    val_rsp32 = 4294967296LL;
    val_rbp = 4294967296LL;
    val_rsp24 = 4294967296LL;
    r11_after = 777888999LL;
#endif
    if (eq_rsp32 != 1) return -80;
    if (eq_rbp != 1) return -81;
    if (eq_rsp24 != 1) return -82;
    if (eq_idx32 != 1) return -87;
    if (eq_idx24 != 1) return -88;
    if (val_rsp32 != 4294967296LL) return -83;
    if (val_rbp != 4294967296LL) return -84;
    if (val_rsp24 != 4294967296LL) return -85;
    if (r11_after != 777888999LL) return -86;
    return 88;
}

/* MV-07: Variadic function frame scratch verification */
int classify_variadic_scratch(int count, ...) {
    long long v = 4294967296LL;
    long long eq = 0, r11_after = 0;
#ifdef __ZCC__
    asm("movq %0, %%rax" : : "m"(v));
    asm("movq $333444555, %r11");
    asm("cmpq $4294967296, %rax");
    asm("sete %al");
    asm("movzbq %%al, %0" : "=r"(eq));
    asm("movq %%r11, %0" : "=r"(r11_after));
#else
    eq = (v == 4294967296LL) ? 1 : 0;
    r11_after = 333444555LL;
#endif
    if (eq != 1) return -90;
    if (r11_after != 333444555LL) return -91;
    return 90 + count;
}

/* MV-09: Full 128-bit XMM15 & XMM14 Liveness & Scratch Ownership Verification:
 * Initializes %xmm15 to 128-bit pattern [0x66554433221100ff : 0x1122334455667788]
 * and %xmm14 to 128-bit pattern [0x7766554433221100 : 0x2233445566778899],
 * executes two 64-bit immediate comparisons while %r11 is live:
 *   1) With both %xmm15 and %xmm14 live after comparison -> selects %xmm13
 *   2) After %xmm14 is consumed, with %xmm15 still live -> selects %xmm14
 * and verifies that all 128 bits ([127:64] and [63:0]) of both %xmm15 and %xmm14
 * as well as %r11 and memory[RSP+32]/memory[RSP+24] are preserved bit-for-bit!
 */
int classify_xmm15_128bit_liveness(void) {
    long long eq1 = 0, eq2 = 0, r11_after = 0;
    long long x15_lo = 0, x15_hi = 0, x14_lo = 0, x14_hi = 0;
#ifdef __ZCC__
    asm("movabsq $0x1122334455667788, %rax");
    asm("movq %rax, -96(%rbp)");
    asm("movabsq $0x66554433221100ff, %rax");
    asm("movq %rax, -88(%rbp)");
    asm("movaps -96(%rbp), %xmm15");

    asm("movabsq $0x2233445566778899, %rax");
    asm("movq %rax, -112(%rbp)");
    asm("movabsq $0x7766554433221100, %rax");
    asm("movq %rax, -104(%rbp)");
    asm("movaps -112(%rbp), %xmm14");

    asm("movq $777888999, %r11");
    asm("movabsq $4294967296, %rax");
    asm("movq %rax, 32(%rsp)");
    asm("movq %rax, 24(%rsp)");

    /* Site 1: Both %xmm15 and %xmm14 are read below -> part5.c selects %xmm13 */
    asm("movq $-1, %rcx");
    asm("cmpq $4294967296, 32(%rsp, %rcx, 8)");
    asm("sete %al");
    asm("movzbq %%al, %0" : "=r"(eq1));

    /* Consume %xmm14 (128-bit store) */
    asm("movaps %xmm14, -128(%rbp)");
    asm("movq -128(%rbp), %rax");
    asm("movq %%rax, %0" : "=r"(x14_lo));
    asm("movq -120(%rbp), %rax");
    asm("movq %%rax, %0" : "=r"(x14_hi));

    /* Site 2: %xmm15 is still live below, %xmm14 is now dead -> part5.c selects %xmm14 */
    asm("movq $3, %rcx");
    asm("cmpq $4294967296, 8(%rsp, %rcx, 8)");
    asm("sete %al");
    asm("movzbq %%al, %0" : "=r"(eq2));

    /* Consume %xmm15 (128-bit store) */
    asm("movaps %xmm15, -144(%rbp)");
    asm("movq -144(%rbp), %rax");
    asm("movq %%rax, %0" : "=r"(x15_lo));
    asm("movq -136(%rbp), %rax");
    asm("movq %%rax, %0" : "=r"(x15_hi));
    asm("movq %%r11, %0" : "=r"(r11_after));
#else
    eq1 = 1;
    eq2 = 1;
    r11_after = 777888999LL;
    x15_lo = 0x1122334455667788LL;
    x15_hi = 0x66554433221100ffLL;
    x14_lo = 0x2233445566778899LL;
    x14_hi = 0x7766554433221100LL;
#endif
    if (eq1 != 1) return -100;
    if (eq2 != 1) return -101;
    if (r11_after != 777888999LL) return -102;
    if (x15_lo != 0x1122334455667788LL) return -103;
    if (x15_hi != 0x66554433221100ffLL) return -104;
    if (x14_lo != 0x2233445566778899LL) return -105;
    if (x14_hi != 0x7766554433221100LL) return -106;
    return 108;
}

/* MV-10A: Full-Function Backward-Loop 128-bit XMM15 Liveness Verification:
 * %xmm15 is read ONLY ABOVE `cmpq $4294967296, %rdx` inside `.Lmv10_loop`.
 * A forward-only scan from `i + 1` misses the `%xmm15` read above `i` and clobbers
 * bits [127:64] of `%xmm15` on iteration 0 so iteration 1 observes 0 in bits [127:64].
 * Full-function scan from entry `pushq %rbp` sees `%xmm15` above `i` and selects `%xmm14`.
 */
int classify_mv10_backward_loop_xmm15(void) {
    long long x15_lo = 0, x15_hi = 0, r11_after = 0;
#ifdef __ZCC__
    asm("movabsq $0x1122334455667788, %rax");
    asm("movq %rax, -64(%rbp)");
    asm("movabsq $0x66554433221100ff, %rax");
    asm("movq %rax, -56(%rbp)");
    asm("movaps -64(%rbp), %xmm15");
    asm("movq $0, %rcx");
    asm("movq $777888999, %r11");
    asm(".Lmv10_loop:");
    asm("movaps %xmm15, -80(%rbp)");
    asm("movabsq $4294967296, %rdx");
    asm("cmpq $4294967296, %rdx");
    asm("addq $1, %rcx");
    asm("cmpq $2, %rcx");
    asm("jl .Lmv10_loop");
    asm("movq -80(%rbp), %rax");
    asm("movq %%rax, %0" : "=r"(x15_lo));
    asm("movq -72(%rbp), %rax");
    asm("movq %%rax, %0" : "=r"(x15_hi));
    asm("movq %%r11, %0" : "=r"(r11_after));
#else
    x15_lo = 0x1122334455667788LL;
    x15_hi = 0x66554433221100ffLL;
    r11_after = 777888999LL;
#endif
    if (x15_lo != 0x1122334455667788LL) return -110;
    if (x15_hi != 0x66554433221100ffLL) return -111;
    if (r11_after != 777888999LL) return -112;
    return 118;
}

/* MV-10B + ZCC-PROVE-02: 4-XMM Pool (%xmm15/%xmm14/%xmm13 live -> %xmm12)
 * AND `movq $0, %r` -> `xorq %r, %r` EFLAGS Liveness Guard Verification:
 *   1) Simultaneously keeps 128-bit %xmm15, %xmm14, and %xmm13 live across
 *      `cmpq $4294967296, 8(%rsp, %rcx, 8)` -> part5.c selects `%xmm12`.
 *   2) Executes `cmpq $99, %rcx` (%rcx = 77, so ZF = 0) followed immediately by
 *      `movq $0, %rax` and `sete %al`, verifying `movq $0, %rax` is NOT rewritten
 *      to `xorq %rax, %rax` when `sete` consumes `ZF`.
 */
int classify_mv10_xmm12_and_eflags(void) {
    long long eq1 = 0, x13_lo = 0, x13_hi = 0, flag_res = 99;
#ifdef __ZCC__
    asm("movabsq $0x33445566778899aa, %rax");
    asm("movq %rax, -96(%rbp)");
    asm("movabsq $0x554433221100ffee, %rax");
    asm("movq %rax, -88(%rbp)");
    asm("movaps -96(%rbp), %xmm15");
    asm("movaps -96(%rbp), %xmm14");
    asm("movaps -96(%rbp), %xmm13");

    asm("movabsq $4294967296, %rax");
    asm("movq %rax, 32(%rsp)");
    asm("movq $3, %rcx");
    asm("cmpq $4294967296, 8(%rsp, %rcx, 8)");
    asm("sete %al");
    asm("movzbq %%al, %0" : "=r"(eq1));

    asm("movaps %xmm15, -112(%rbp)");
    asm("movaps %xmm14, -128(%rbp)");
    asm("movaps %xmm13, -144(%rbp)");
    asm("movq -144(%rbp), %rax");
    asm("movq %%rax, %0" : "=r"(x13_lo));
    asm("movq -136(%rbp), %rax");
    asm("movq %%rax, %0" : "=r"(x13_hi));

    /* EFLAGS guard test: %rcx = 77 != 99 -> ZF = 0 before movq $0, %rax; sete %al */
    asm("movq $77, %rcx");
    asm("cmpq $99, %rcx");
    asm("movq $0, %rax");
    asm("sete %al");
    asm("movq %%rax, %0" : "=r"(flag_res));
#else
    eq1 = 1;
    x13_lo = 0x33445566778899aaLL;
    x13_hi = 0x554433221100ffeeLL;
    flag_res = 0;
#endif
    if (eq1 != 1) return -120;
    if (x13_lo != 0x33445566778899aaLL) return -121;
    if (x13_hi != 0x554433221100ffeeLL) return -122;
    if (flag_res != 0) return -123;
    return 128;
}

/* MV-11 + ZCC-PROVE-03:
 *   1) Verifies `movq %r11, %rax` is NOT elided before `addq -64(%rbp), %rax`
 *      and `addq -64(%rbp), %rax` is encoded as memory-to-register `0x48 0x03 0x45 0xc0`
 *      rather than `addq %r15, %rax`.
 *   2) Verifies `movq %r11, %rax` is NOT elided before `xorq %rcx, %rax`
 *      (only `xorq %rax, %rax` / `xorl %eax, %eax` overwrites `%rax` without reading it).
 *   3) Verifies `movabsq $0x8877665544332211, %rax` parses unsigned 64-bit hex
 *      constants >= 2^63 via `strtoull` without clamping to `LLONG_MAX`.
 */
int classify_mv11_rmw_and_u64_imm(void) {
    long long res_add = 0, res_xor = 0, res_u64 = 0;
#ifdef __ZCC__
    asm("movq $50, -64(%rbp)");
    asm("movq $70, %r11");
    asm("movq $999, %rax");
    asm("movq %r11, %rax");
    asm("addq -64(%rbp), %rax");
    asm("movq %%rax, %0" : "=r"(res_add));

    asm("movq $0x55, %r11");
    asm("movq $0x0f, %rcx");
    asm("movq $0x1234, %rax");
    asm("movq %r11, %rax");
    asm("xorq %rcx, %rax");
    asm("movq %%rax, %0" : "=r"(res_xor));

    asm("movabsq $0x8877665544332211, %rax");
    asm("movq %%rax, %0" : "=r"(res_u64));
#else
    res_add = 120LL;
    res_xor = 0x5aLL;
    res_u64 = (long long)0x8877665544332211ULL;
#endif
    if (res_add != 120LL) return -130;
    if (res_xor != 0x5aLL) return -131;
    if (res_u64 != (long long)0x8877665544332211ULL) return -132;
    return 138;
}

/* MV-12 + ZCC-PROVE-04:
 *   1) Tail-Call Optimization (TCO) full-function local-stack escape scan:
 *      `a = 42` has its address taken (`p = &a`) BEFORE a conditional branch label (`.L...`),
 *      followed by a tail call `return mv12_consume_escaped_ptr(p, 10, 20);`.
 *      If TCO stops scanning at `.L` labels or 32 lines, it tears down caller's frame
 *      before jumping to `mv12_consume_escaped_ptr`, which overwrites the stack frame
 *      and reads corrupted `*p != 42`.
 *   2) `zcc_x86_peephole_boost` Pass 7 (`ZCC_OPT=1`):
 *      - `movq %rdx, %rax` followed by `addq -64(%rbp), %rax` must NOT delete `movq %rdx, %rax`.
 *      - `movq %r11, %rax` followed by `pxor %xmm0, %xmm0` must NOT delete `movq %r11, %rax`.
 */
long long mv12_consume_escaped_ptr(long long *p, long long a, long long b) {
    long long overwrite[16];
    int i;
    for (i = 0; i < 16; i++) overwrite[i] = 999999LL + i + a + b;
    if (overwrite[0] != 1000029LL) return -1;
    return *p;
}

long long mv12_tco_caller(int cond) {
    long long a = 42;
    long long *p = &a;
    if (!cond) {
        return 0;
    }
    return mv12_consume_escaped_ptr(p, 10, 20);
}

int classify_mv12_boost_and_tco(void) {
    long long tco_val = mv12_tco_caller(1);
    long long res_rdx_add = 0, res_r11_pxor = 0;
#ifdef __ZCC__
    asm("movq $50, -64(%rbp)");
    asm("movq $70, %rdx");
    asm("movq $999, %rax");
    asm("movq %rdx, %rax");
    asm("addq -64(%rbp), %rax");
    asm("movq %%rax, %0" : "=r"(res_rdx_add));

    asm("movq $88, %r11");
    asm("movq $999, %rax");
    asm("movq %r11, %rax");
    asm("pxor %xmm0, %xmm0");
    asm("movq %%rax, %0" : "=r"(res_r11_pxor));
#else
    res_rdx_add = 120LL;
    res_r11_pxor = 88LL;
#endif
    if (tco_val != 42LL) return -140;
    if (res_rdx_add != 120LL) return -141;
    if (res_r11_pxor != 88LL) return -142;
    return 148;
}

/* MV-13 + ZCC-PROVE-05:
 *   1) `zcc_x86_peephole_boost` Pass 8 (`ZCC_OPT=1`, `nlines >= 600`):
 *      Verifies `rsp_depth` is invalidated (`rsp_depth = -1`) at control-flow labels (`.L...`)
 *      and jumps (`jmp`/`jne`), preventing linear `pushq` accumulation across unreachable/branched
 *      blocks from injecting an unaligned `subq $8, %rsp` before `call mv13_check_rsp_alignment`.
 *   2) `zcc_x86_peephole_boost` Pass 5c (`ZCC_OPT=1`):
 *      Verifies `movq %xmm0, %rax` before `addsd %xmm0, %xmm0` is NOT elided when `%rax` is
 *      read afterward without an intervening `%rax` overwrite.
 */
int mv13_check_rsp_alignment(void) {
    long long rsp_val = 0;
#ifdef __ZCC__
    asm("movq %%rbp, %0" : "=r"(rsp_val));
#endif
    return (rsp_val & 15LL) == 0 ? 0 : -150;
}

int mv13_pass8_branch_caller(int cond) {
    int res = 0;
#ifdef __ZCC__
    asm("movl -8(%rbp), %eax");
    asm("testl %eax, %eax");
    asm("jne .Lmv13_pass8_join");
    asm("pushq %rax");
    asm("jmp .Lmv13_pass8_done");
    asm(".Lmv13_pass8_join:");
    res = mv13_check_rsp_alignment();
    asm(".Lmv13_pass8_done:");
#else
    (void)cond;
    res = 0;
#endif
    return res;
}

int classify_mv13_pass8_and_pass5c(void) {
    int r_align = mv13_pass8_branch_caller(1);
    long long rax_bits = 0;
#ifdef __ZCC__
    asm("movq $0x42424242, %rax\n    movq %rax, %xmm0\n    xorl %eax, %eax\n    movq %xmm0, %rax\n    addsd %xmm0, %xmm0");
    asm("movq %%rax, %0" : "=r"(rax_bits));
#else
    rax_bits = 0x42424242LL;
#endif
    if (r_align != 0) return -151;
    if (rax_bits != 0x42424242LL) return -152;
    return 158;
}

int main(void) {
    if (classify(4294967296LL) != 7) return 1;
    if (classify(-4294967296LL) != 9) return 2;
    if (classify(0) != 3) return 3;
    if (classify_live_r11(4294967296LL) != 7) return 4;
    if (classify_live_r11(-4294967296LL) != 9) return 5;
    if (classify_live_r11(0) != 3) return 6;
    if (classify_alias_r11() != 11) return 7;
    if (classify_rsp_relative() != 22) return 8;
    if (classify_mem_r11_base() != 33) return 9;
    if (classify_mem_r11_r10_sib() != 44) return 10;
    if (classify_rsp_indexed() != 55) return 11;
    if (classify_redzone_rsp() != 66) return 12;
    if (classify_direct_rsp() != 77) return 13;
    if (classify_frame_scratch_alias() != 88) return 14;
    if (classify_variadic_scratch(9, 100LL, 200LL) != 99) return 15;
    if (classify_xmm15_128bit_liveness() != 108) return 16;
    if (classify_mv10_backward_loop_xmm15() != 118) return 17;
    if (classify_mv10_xmm12_and_eflags() != 128) return 18;
    if (classify_mv11_rmw_and_u64_imm() != 138) return 19;
    if (classify_mv12_boost_and_tco() != 148) return 20;
    if (classify_mv13_pass8_and_pass5c() != 158) return 21;
    return 0;
}
