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
    return 0;
}
