struct Basic {
    char c;
    int i;
    double d;
};

_Static_assert(sizeof(char) == 1, "char size");
_Static_assert(_Alignof(char) == 1, "char align");

_Static_assert(sizeof(int) == 4, "int size");
_Static_assert(_Alignof(int) == 4, "int align");

_Static_assert(sizeof(void *) == 8, "pointer size on 64-bit target");
_Static_assert(_Alignof(void *) == 8, "pointer align on 64-bit target");

_Static_assert(_Alignof(struct Basic) >= _Alignof(double), "struct align follows max member");

int main(void) {
    return 0;
}