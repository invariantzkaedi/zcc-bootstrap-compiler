#include <stdio.h>
int main() {
    char *p = NULL;
    if (p && p[0] == 'a') {
        printf("Yes\n");
    } else {
        printf("No\n");
    }
    return 0;
}
