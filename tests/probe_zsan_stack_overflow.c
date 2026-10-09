#include <stdio.h>

void vulnerable_function(void) {
    volatile char buf[8];
    for (int i = 0; i < 24; i++) {
        buf[i] = 'A';
    }
}

int main(void) {
    vulnerable_function();
    printf("UNSAFE: executed past buffer overflow\n");
    return 0;
}
