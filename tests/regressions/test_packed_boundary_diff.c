/*
 * test_packed_boundary_diff.c — Hardware descriptor packed boundary test
 * Verifies attribute((packed)) behavior for member alignment across 8-byte boundaries.
 */

#include <stdio.h>

struct __attribute__((packed)) idt_ptr_64 {
    unsigned short limit;
    unsigned long base;
};

struct __attribute__((packed)) mixed_packed {
    char c;
    unsigned long val;
    unsigned short s;
};

int main(void) {
    if (sizeof(struct idt_ptr_64) != 10) {
        printf("PACKED BOUNDARY FAIL: sizeof(idt_ptr_64)=%zu (expected 10)\n", sizeof(struct idt_ptr_64));
        return 1;
    }
    if (sizeof(struct mixed_packed) != 11) {
        printf("PACKED BOUNDARY FAIL: sizeof(mixed_packed)=%zu (expected 11)\n", sizeof(struct mixed_packed));
        return 1;
    }
    printf("PACKED BOUNDARY PASS\n");
    return 0;
}
