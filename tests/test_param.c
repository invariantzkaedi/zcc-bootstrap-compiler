#include <stdio.h>
void func(int a, double b) {
    printf("a = %d, b = %f\n", a, b);
}
int main() {
    func(42, 3.14);
    return 0;
}
