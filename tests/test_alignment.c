/* Test for CG-IR-015: 16-byte stack alignment */
#include <stdio.h>
void test_aligned_call(void) {
    /* SSE/AVX instructions require 16-byte alignment */
    printf("Stack alignment test\n");
}
int main() { test_aligned_call(); return 0; }
