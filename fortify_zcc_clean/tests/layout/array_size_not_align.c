typedef struct {
    char bytes[3];
} Three;

Three items[5];

_Static_assert(sizeof(Three) == 3, "struct with char[3] size");
_Static_assert(_Alignof(Three) == 1, "struct with char[3] align");
_Static_assert(sizeof(items) == 15, "array size must be element size times count");

int main(void) {
    return 0;
}