#include <stdio.h>
#include <stdlib.h>

void test_vla(int n) {
    int vla[n];
    for (int i = 0; i < n; ++i) {
        vla[i] = i * 2;
    }
    
    int sum = 0;
    for (int i = 0; i < n; ++i) {
        sum += vla[i];
    }
    if (sum != 12) { // 0+2+4+6 = 12 (for n=4)
        printf("FAIL: Expected 12, got %d\n", sum);
        exit(1);
    }
}

int main() {
    test_vla(4);
    printf("PASS\n");
    return 0;
}
