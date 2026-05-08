#include <stdio.h>
#include <string.h>
int main() {
    char *res = strchr("bt", 'B');
    if (res == NULL) {
        printf("res is NULL\n");
    } else {
        printf("res is %p\n", res);
    }
    return 0;
}
