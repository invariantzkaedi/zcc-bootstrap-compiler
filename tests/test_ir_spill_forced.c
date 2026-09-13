#include <stdio.h>
#include <assert.h>

/* Forced-spill unit test: 12 simultaneously live variables (pressure > 7) */
long forced_spill_calc(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9, long a10, long a11) {
    long v0 = a0 * 3 + 1;
    long v1 = a1 * 5 + 2;
    long v2 = a2 * 7 + 3;
    long v3 = a3 * 11 + 4;
    long v4 = a4 * 13 + 5;
    long v5 = a5 * 17 + 6;
    long v6 = a6 * 19 + 7;
    long v7 = a7 * 23 + 8;
    long v8 = a8 * 29 + 9;
    long v9 = a9 * 31 + 10;
    long v10 = a10 * 37 + 11;
    long v11 = a11 * 41 + 12;

    /* All 12 variables are live simultaneously across this reduction */
    long sum = v0 + v1 + v2 + v3 + v4 + v5 + v6 + v7 + v8 + v9 + v10 + v11;
    return sum;
}

int main(void) {
    long res = forced_spill_calc(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
    /* 
     * v0 = 1*3+1 = 4
     * v1 = 2*5+2 = 12
     * v2 = 3*7+3 = 24
     * v3 = 4*11+4 = 48
     * v4 = 5*13+5 = 70
     * v5 = 6*17+6 = 108
     * v6 = 7*19+7 = 140
     * v7 = 8*23+8 = 192
     * v8 = 9*29+9 = 270
     * v9 = 10*31+10 = 320
     * v10 = 11*37+11 = 418
     * v11 = 12*41+12 = 504
     * Sum = 4+12+24+48+70+108+140+192+270+320+418+504 = 2110
     */
    printf("forced_spill_calc result: %ld (expected 2110)\n", res);
    assert(res == 2110);
    return 0;
}
