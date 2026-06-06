/*
 * ctrl_flow.c — D-30: Control Flow Workload
 *
 * Exercises switch statements, nested loops, and branch logic.
 */
#include <stdio.h>
#include <stdlib.h>

int run_control_flow(int val) {
    int state = val % 5;
    int result = 0;
    
    for (int i = 0; i < 1000; i++) {
        switch (state) {
            case 0:
                result += i * 2;
                state = 1;
                break;
            case 1:
                if (i % 3 == 0) {
                    result -= i;
                    state = 2;
                } else {
                    result += 5;
                    state = 3;
                }
                break;
            case 2:
                result ^= i;
                state = 3;
                break;
            case 3:
                for (int j = 0; j < 5; j++) {
                    result += j;
                }
                state = 4;
                break;
            case 4:
                result += 7;
                state = 0;
                break;
            default:
                state = 0;
                break;
        }
    }
    return result;
}

int main(int argc, char **argv) {
    int val = 17;
    if (argc > 1) {
        val = atoi(argv[1]);
    }
    int result = run_control_flow(val);
    printf("result=%d\n", result);
    return 0;
}
