#include <stdio.h>

int main(void) {
    /* 1. Basic register input and output */
    int dst = 0, src = 42;
    __asm__("mov %1, %0" : "=r"(dst) : "r"(src));
    if (dst != 42) {
        printf("PROBE_ASM FAIL: test 1 dst=%d (expected 42)\n", dst);
        return 1;
    }

    /* 2. Read-write operand (+) */
    int val = 10;
    __asm__("add $15, %0" : "+r"(val));
    if (val != 25) {
        printf("PROBE_ASM FAIL: test 2 val=%d (expected 25)\n", val);
        return 2;
    }

    /* 3. Fixed register constraints (a, d) with %% escaping */
    unsigned int lo = 0, hi = 0;
    __asm__("mov $0x1234, %%eax\n\tmov $0x5678, %%edx" : "=a"(lo), "=d"(hi));
    if (lo != 0x1234 || hi != 0x5678) {
        printf("PROBE_ASM FAIL: test 3 lo=0x%x, hi=0x%x\n", lo, hi);
        return 3;
    }

    /* 4. Matching constraint ("0") */
    long sum = 0, a = 100, b = 200;
    __asm__("addq %2, %0" : "=r"(sum) : "0"(a), "r"(b));
    if (sum != 300) {
        printf("PROBE_ASM FAIL: test 4 sum=%ld (expected 300)\n", sum);
        return 4;
    }

    /* 5. Modifier %k (32-bit register) and memory clobber */
    unsigned long qword = 0x1122334455667788ULL;
    unsigned int low32 = 0;
    __asm__("movl %k1, %0" : "=r"(low32) : "r"(qword) : "memory");
    if (low32 != 0x55667788) {
        printf("PROBE_ASM FAIL: test 5 low32=0x%x (expected 0x55667788)\n", low32);
        return 5;
    }

    /* 6. Named operands (%[name]) */
    long total = 100, diff = 50;
    __asm__("addq %[delta], %[sum]" : [sum] "+r"(total) : [delta] "r"(diff));
    if (total != 150) {
        printf("PROBE_ASM FAIL: test 6 total=%ld (expected 150)\n", total);
        return 6;
    }

    /* 7. Named operand with modifier (%k[name]) */
    unsigned long full_val = 0xdeadbeef12345678ULL;
    unsigned int named_low32 = 0;
    __asm__("movl %k[src], %[dst]" : [dst] "=r"(named_low32) : [src] "r"(full_val));
    if (named_low32 != 0x12345678) {
        printf("PROBE_ASM FAIL: test 7 named_low32=0x%x (expected 0x12345678)\n", named_low32);
        return 7;
    }

    /* 8. Immediate integer operand ("i") */
    int product = 5;
    __asm__("imull %1, %0" : "+r"(product) : "i"(10));
    if (product != 50) {
        printf("PROBE_ASM FAIL: test 8 product=%d (expected 50)\n", product);
        return 8;
    }

    /* 9. Memory operand ("m") */
    int mem_var = 777;
    int read_val = 0;
    __asm__("movl %1, %0" : "=r"(read_val) : "m"(mem_var));
    if (read_val != 777) {
        printf("PROBE_ASM FAIL: test 9 read_val=%d (expected 777)\n", read_val);
        return 9;
    }

    /* 10. Callee-saved clobbers ("rbx", "r12", "r13", "r14", "r15") */
    __asm__ volatile(
        "mov $0x111, %%rbx\n\t"
        "mov $0x222, %%r12\n\t"
        "mov $0x333, %%r13\n\t"
        "mov $0x444, %%r14\n\t"
        "mov $0x555, %%r15"
        : : : "rbx", "r12", "r13", "r14", "r15"
    );

    /* 11. Trailing semicolons and whitespace stripping */
    int semi_val = 100;
    __asm__("add $25, %0;\n\t" : "+r"(semi_val));
    if (semi_val != 125) {
        printf("PROBE_ASM FAIL: test 11 semi_val=%d (expected 125)\n", semi_val);
        return 11;
    }

    /* 12. Hardware primitive: rdtsc */
    unsigned int tsc_lo = 0, tsc_hi = 0;
    __asm__ volatile("rdtsc" : "=a"(tsc_lo), "=d"(tsc_hi));
    if (tsc_lo == 0 && tsc_hi == 0) {
        printf("PROBE_ASM FAIL: test 12 rdtsc returned all zeros\n");
        return 12;
    }

    /* 13. Hardware primitive: 4-register cpuid */
    unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));
    if (ebx == 0) {
        printf("PROBE_ASM FAIL: test 13 cpuid ebx=0\n");
        return 13;
    }

    printf("PROBE_ASM PASS: all 13 extended inline asm tests succeeded!\n");
    return 0;
}
