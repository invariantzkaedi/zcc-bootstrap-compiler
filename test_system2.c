#include <stdlib.h>
#include <stdio.h>
int main() {
    int stat = system("../lua prog >out 2>&1");
    printf("system('../lua prog >out 2>&1') = %d\n", stat);
    return 0;
}
