/* tests/regressions/test_broken_packed_abi.c — Deliberately broken packed struct for ABI verifier fail-closed validation */
#include <stdio.h>

struct idt_ptr_64_broken {
    unsigned short limit;
    unsigned long base;
    unsigned short extra;
} __attribute__((packed));

int main(void) {
    return 0;
}
