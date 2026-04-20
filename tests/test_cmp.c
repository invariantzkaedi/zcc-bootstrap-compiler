#include <stdio.h>
int main() {
    double x = 1e30;
    double y = 1e29;
    if (x < y) printf("BUG\n"); else printf("OK\n");
    return 0;
}
