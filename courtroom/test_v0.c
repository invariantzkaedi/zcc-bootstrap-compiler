#include <stdio.h>
int main() {
    long long v0 = (((short)-473) / (((signed char)82) == 0 ? 1 : ((signed char)82)));
    unsigned int v1 = (((signed char)-597) | ((unsigned long)434));
    char v2 = (((unsigned short)162) - ((signed char)-94));
    signed char v3 = ((unsigned short)353);
    int ret_val = (v0 >> (v1 & 31));
    printf("v0=%lld, v1=%u, v1&31=%u, ret_val=%d, ans=%d\n", v0, v1, v1&31, ret_val, (int)(ret_val & 255));
    return 0;
}
