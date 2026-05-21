#include "zcc.c"
#include <stddef.h>
#include <stdio.h>
#undef main
int main() {
    printf("%zu\n", offsetof(Compiler, arena));
    return 0;
}
