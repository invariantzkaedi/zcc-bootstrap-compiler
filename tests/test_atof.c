#include <stdio.h>
extern int sqlite3AtoF(const char*, double*, int, unsigned char);
int main() {
    double r = -1.0;
    int rc = sqlite3AtoF("3.14", &r, 4, 1);
    printf("rc=%d r=%.6f\n", rc, r);
    return 0;
}
