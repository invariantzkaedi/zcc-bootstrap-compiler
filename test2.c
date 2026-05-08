#include <string.h>
#include <stdio.h>
int main() {
    const char *mode = NULL;
    printf("strchr: %p\n", strchr(mode, 't'));
    return 0;
}
