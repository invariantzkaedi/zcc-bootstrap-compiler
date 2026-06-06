/*
 * call_graph.c — D-30: Call Graph Workload
 *
 * Exercises deep recursion vs shallow wide calls.
 */
#include <stdio.h>
#include <stdlib.h>

/* Forward declarations */
int fib_recursive(int n);
int wide_leaf_1(int x);
int wide_leaf_2(int x);
int wide_leaf_3(int x);
int wide_leaf_4(int x);
int wide_caller(int x);

int fib_recursive(int n) {
    if (n <= 1) return n;
    return fib_recursive(n - 1) + fib_recursive(n - 2);
}

int wide_leaf_1(int x) { return x + 1; }
int wide_leaf_2(int x) { return x * 2; }
int wide_leaf_3(int x) { return x - 3; }
int wide_leaf_4(int x) { return x ^ 4; }

int wide_caller(int x) {
    int r = wide_leaf_1(x);
    r += wide_leaf_2(r);
    r += wide_leaf_3(r);
    r += wide_leaf_4(r);
    return r;
}

int main(int argc, char **argv) {
    int mode = 0;
    if (argc > 1) {
        mode = atoi(argv[1]);
    }
    
    int result = 0;
    if (mode == 0) {
        /* Deep recursion mode */
        result = fib_recursive(12);
    } else {
        /* Wide leaf mode */
        for (int i = 0; i < 50; i++) {
            result += wide_caller(i);
        }
    }
    printf("result=%d\n", result);
    return 0;
}
