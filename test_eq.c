#include <stdio.h>
int main() {
    int c = 10;
    if (c == "\x1bLua"[0]) printf("10 == 27 is TRUE!\n");
    else printf("10 == 27 is FALSE!\n");
    return 0;
}
