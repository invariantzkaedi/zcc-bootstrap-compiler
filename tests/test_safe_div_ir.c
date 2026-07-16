#include <stdio.h>
#include <stdlib.h>

int run_div(int x, int y) {
    return x / y;
}

int run_mod(int x, int y) {
    return x % y;
}

int main(int argc, char **argv) {
    volatile int zero = argc - 1; 
    int num = 42;
    printf("Result: %d\n", run_div(num, zero));
    return 0;
}
