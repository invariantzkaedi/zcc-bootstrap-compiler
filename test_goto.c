#include <stdio.h>
int next_token(int i) {
again:
    if (i < 5) {
        i++;
        goto again;
    }
    return i;
}
int main() {
    printf("res = %d\n", next_token(0));
    return 0;
}
