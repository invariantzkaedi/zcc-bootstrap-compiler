#include <stdio.h>
int main(void) {
    int complexity = 130;
    int parallelism = 70;
    int result = 649 + 4 * complexity + 3 * parallelism;
    printf("TQPU result: %d (self-modded, quantum-collapsed, surface-protected)\n", result);
    return 0;
}
