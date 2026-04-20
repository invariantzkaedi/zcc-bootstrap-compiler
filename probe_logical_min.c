#include <stdio.h>
static int g_pass = 0;
static int g_fail = 0;
int main(void) {
    float f;
    int r;
    f = -0.5f;
    r = f && 1;
    if (r == 1) { printf("PASS logical-and\n"); g_pass++; }
    else { printf("FAIL logical-and\n"); g_fail++; }
    f = 0.0f;
    r = !f;
    if (r == 1) { printf("PASS not-zero\n"); g_pass++; }
    else { printf("FAIL not-zero\n"); g_fail++; }
    printf("DONE: %d pass %d fail\n", g_pass, g_fail);
    return g_fail;
}
