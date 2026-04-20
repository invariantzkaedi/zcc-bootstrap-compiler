#include <stdlib.h>
#include <stdio.h>
int main() {
    char *p = getenv("ZCC_IR_BACKEND");
    if (!p) {
        printf("getenv returned NULL!\n");
    } else {
        printf("getenv returned: %s\n", p);
    }
    return 0;
}
