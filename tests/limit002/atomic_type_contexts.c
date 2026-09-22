/* Type-name contexts for _Atomic: sizeof, _Alignof, constant expressions, casts */
#include <stdio.h>

typedef _Atomic(int) my_atomic_int;

enum {
    ENUM_SZ_INT = sizeof(_Atomic(int)),
    ENUM_AL_INT = _Alignof(_Atomic(int)),
    ENUM_SZ_LL  = sizeof(_Atomic(long long)),
    ENUM_AL_LL  = _Alignof(_Atomic(long long))
};

int main(void) {
    /* 1. Compile-time constant expressions */
    if (ENUM_SZ_INT != 4) return 1;
    if (ENUM_AL_INT != 4) return 2;
    if (ENUM_SZ_LL != 8) return 3;
    if (ENUM_AL_LL != 8) return 4;

    /* 2. Runtime expression sizeof & _Alignof */
    int sz1 = sizeof(_Atomic(int));
    int al1 = _Alignof(_Atomic(int));
    int sz2 = sizeof(_Atomic(double));
    int al2 = _Alignof(_Atomic(double));

    if (sz1 != 4 || al1 != 4) return 5;
    if (sz2 != 8 || al2 != 8) return 6;

    /* 3. Cast expressions */
    int val = (_Atomic(int))77;
    if (val != 77) return 7;

    int val2 = (_Atomic int)88;
    if (val2 != 88) return 8;

    /* 4. Typedef usage */
    my_atomic_int a = 99;
    if (a != 99) return 9;

    return 0;
}
