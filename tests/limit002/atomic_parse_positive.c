/* Positive test: C11 _Atomic(type-name) and _Atomic specifier syntax */
#include <stdio.h>

_Atomic(int) g_a1 = 10;
_Atomic int g_a2 = 20;
_Atomic(double) g_a3 = 3.14;
_Atomic(void *) g_a4 = NULL;

struct Node {
    int val;
};
_Atomic(struct Node) g_node;

_Atomic(int) *get_ptr(void) {
    return &g_a1;
}

int main(void) {
    _Atomic(int) local1 = 100;
    _Atomic int local2 = 200;
    _Atomic(long long) local3 = 300LL;

    _Atomic(int) *ptr = &local1;
    *ptr = 500;

    if (local1 != 500) return 1;
    if (g_a1 != 10) return 2;
    if (g_a2 != 20) return 3;

    return 0;
}
