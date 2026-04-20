#include <stdio.h>
int is_negative(double x) {
    unsigned char* p = (unsigned char*)&x;
    return (p[7] & 0x80) != 0;
}
int main() {
    printf("is_negative(0.45)=%d\n", is_negative(0.45));
    printf("is_negative(-0.45)=%d\n", is_negative(-0.45));
    return 0;
}
