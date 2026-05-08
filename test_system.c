#include <stdlib.h>
#include <stdio.h>
int main() {
    int stat = system("exit 1");
    printf("system('exit 1') = %d\n", stat);
    return 0;
}
