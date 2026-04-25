#include <stddef.h>

struct S {
    char c;
    int i;
    char tail;
};

_Static_assert(offsetof(struct S, c) == 0, "offset c");
_Static_assert(offsetof(struct S, i) == 4, "offset i");
_Static_assert(offsetof(struct S, tail) == 8, "offset tail");
_Static_assert(sizeof(struct S) == 12, "struct S size");
_Static_assert(_Alignof(struct S) == 4, "struct S align");