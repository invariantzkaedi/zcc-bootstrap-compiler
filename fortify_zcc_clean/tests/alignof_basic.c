_Static_assert(_Alignof(char) == 1, "char alignment");
_Static_assert(_Alignof(short) <= sizeof(short), "short alignment sane");
_Static_assert(_Alignof(int) <= sizeof(int), "int alignment sane");
_Static_assert(_Alignof(long) <= sizeof(long), "long alignment sane");

struct S {
    char c;
    int i;
};

_Static_assert(_Alignof(struct S) >= _Alignof(int), "struct alignment follows max field alignment");

int main(void) {
    return 0;
}