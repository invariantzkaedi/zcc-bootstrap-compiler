#include <stdio.h>

/* CG-IR-011 Aggressive Reproducer
 * Uses multiple recursive functions with high register pressure
 * to force IR linear-scan to consume rbx, r12-r15
 */

int g1 = 0, g2 = 0, g3 = 0, g4 = 0, g5 = 0;

int complex_calc(int a, int b, int c, int d, int e) {
    if (a <= 0) return b + c + d + e;
    
    /* High register pressure: force IR to use callee-saved regs */
    int t1 = a * 2;
    int t2 = b * 3;
    int t3 = c * 5;
    int t4 = d * 7;
    int t5 = e * 11;
    
    g1 += t1; g2 += t2; g3 += t3; g4 += t4; g5 += t5;
    
    return complex_calc(a-1, t1, t2, t3, t4) + 
           complex_calc(a-1, t2, t3, t4, t5);
}

int main(void) {
    int result = complex_calc(5, 1, 2, 3, 4);
    printf("result = %d\n", result);
    printf("globals: %d %d %d %d %d\n", g1, g2, g3, g4, g5);
    
    /* Just verify it doesn't crash */
    printf("✅ Completed without segfault\n");
    return 0;
}
