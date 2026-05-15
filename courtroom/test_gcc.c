#include <stdio.h>
int main() {
    long v0 = ((short)-149);
    unsigned int v1 = ((((unsigned short)497) + ((char)-472)) - (((short)-681) >> (((unsigned long)41) & 31)));
    unsigned int ret_val = ((v0 == 0 ? 1 : v0) / (v1 == 0 ? 1 : v1));
    printf("v0=%ld, v1=%u, ret_val=%u, ans=%d\n", v0, v1, ret_val, (int)(ret_val & 255));
    return 0;
}
