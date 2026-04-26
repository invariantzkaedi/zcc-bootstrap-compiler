union U {
    char c;
    int i;
    double d;
};

_Static_assert(sizeof(union U) == 8, "union size follows max member size");
_Static_assert(_Alignof(union U) == 8, "union align follows max member align");