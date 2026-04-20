#include <stdio.h>
int main(void) {
    double a = 2.5;
    double b = 3.5;
    double sum = a + b;
    double prod = a * b;
    printf("a=%f b=%f sum=%f prod=%f\n", a, b, sum, prod);
    printf("Expected: sum=6.0 prod=8.75\n");
    return 0;
}
