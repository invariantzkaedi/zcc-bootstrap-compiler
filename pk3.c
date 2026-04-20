#include <stdio.h>
struct __attribute__((packed)) P { char a; int b; };
int main(void) {
    int sz = (int)sizeof(struct P);
    printf("%d\n", sz);
    return sz == 5 ? 0 : 1;
}
