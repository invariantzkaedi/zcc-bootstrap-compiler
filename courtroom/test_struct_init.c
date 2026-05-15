#include <stdio.h>
int main() {
    struct S { short f0; long long f1; long f2; short f3; };
    struct S s = {50, 81, 49, 84};
    printf("f0=%d f1=%lld f2=%ld f3=%d ret=%d\n", s.f0, s.f1, s.f2, s.f3, (int)s.f2);
    return (int)(s.f2);
}
