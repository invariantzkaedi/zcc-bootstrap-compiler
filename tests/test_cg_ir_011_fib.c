#include <stdio.h>

/* CG-IR-011 Reproducer: Recursive Fibonacci
 * Triggers callee-saved register corruption when IR backend
 * uses rbx/r12/r13/r14/r15 without AST prologue saving them.
 */
int fib(int n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}

int main(void) {
    int result = fib(10);
    printf("fib(10) = %d\n", result);
    
    if (result == 55) {
        printf("✅ PASS\n");
        return 0;
    } else {
        printf("❌ FAIL (expected 55, got %d)\n", result);
        return 1;
    }
}
